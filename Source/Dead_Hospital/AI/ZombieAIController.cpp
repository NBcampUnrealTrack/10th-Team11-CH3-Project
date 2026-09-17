#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "PlayerCharacter.h"

#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

#include "Navigation/PathFollowingComponent.h"

const FName AZombieAIController::BBKey_bInvestigatingHideSpot(TEXT("bInvestigatingHideSpot"));
const FName AZombieAIController::BBKey_ChaseTarget(TEXT("ChaseTarget"));
const FName AZombieAIController::BBKey_InAttackRange(TEXT("InAttackRange"));
const FName AZombieAIController::BBKey_KnownHideSpotLocation(TEXT("KnownHideSpotLocation"));
const FName AZombieAIController::BBKey_LastKnownLocation(TEXT("LastKnownLocation"));
const FName AZombieAIController::BBKey_State(TEXT("State"));

AZombieAIController::AZombieAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	//좀비의 감지 기능을 담당할 AIPerception 컴포넌트를 생성
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	//방금 만든 컴포넌트를, 이 AI 컨트롤러의 공식 Perception 컴포넌트로 등록한다.
	SetPerceptionComponent(*AIPerception);

	//시야(Sight) 감각에 대한 세부 설정을 생성
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	//이 거리 안에 들어오면 감지를 시작함
	SightConfig->SightRadius = 1500.0f;
	//이미 감지한 타겟은 이 거리를 넘어야 완전히 놓친 것으로 처리 - SightRadius보다 일부러 더 크게 잡아서,
	//경계선 거리에서 감지가 켜졌다 꺼졌다 하는 떨림 현상(hysteresis)를 방지한다.
	SightConfig->LoseSightRadius = 2000.0f;
	//좀비가 정면 기준으로 좌우 몇 도까지 볼 수 있는지(시야각)
	//예를 들어 90도로 지정해주면 총 정면에서부터 좌우 즉, 180도를 보는거다...
	SightConfig->PeripheralVisionAngleDegrees = 30.0f;

	//감지된 정보가 5초 동안은 유효하게 취급된다 - 타겟이 순간적으로 시야에서 가려져도
	//바로 놓친 것으로 처리하지 않아 부자연스러운 끊김을 방지해준다
	SightConfig->SetMaxAge(5.0f);

	//아직 팀(적/아군) 시스템이 없어서 '적'만 감지하게 하면 아무것도 안 걸릴 수 있음
	//안전하게 적/중립/아군 전부 감지 대상으로 켜둠
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1500.0f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	//방금 설정한 SightConfig를 AIPerception 컴포넌트에 등록
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	//여러 감각을 등록했을 때 대표(기준)로 삼을 감각을 시야로 지정
	//지금은 시야뿐이라 당장 큰 차이는 없지만, 나중에 청각 등을 추가할 때를 대비한 명시적 설정
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	bWasPlayerHidingLastFrame = false;
	AttackCooldown = 1.2f;
}

void AZombieAIController::BeginPlay()
{
	Super::BeginPlay();

	//AIPerception이 유효한지 확인하는 안전장치
	if (AIPerception)
	{
		//AIPerception이 뭔가를 감지/갱신할 때마다(OnTargetPerceptionUpdated 이벤트가 발생할 때마다),
		//이 객체(this)의 OnPerceptionUpdated 함수가 자동으로 호출되도록 미리 등록해둠
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(
			//지금 이 코드가 실행되고 있는 객체 자신을 가리키는 포인터
			this,
			&AZombieAIController::OnPerceptionUpdated
		);
		//OnPerceptionUpdated에서 감지 실패(WasSuccessfullySensed = false)가 온 뒤에도, AIPerception이 MaxAge 시간 동안은 그 대상을 기억하고 있다가,
		//그 시간이 다 지나서 완전히 잊혀지는 순간 호출되는 지연성 이벤트. 이 시점에 진짜로 ChaseTarget을 Clear한다.
		AIPerception->OnTargetPerceptionForgotten.AddDynamic(
			this,
			&AZombieAIController::OnPerceptionForgotten
		);
	}
	
}

//AI 컨트롤러가 특정 Pawn(좀비 캐릭터)에 빙의되는 바로 그 순간에 자동으로 호출되는 함수
void AZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	//BehaviorTreeAsset이 유효한지 확인하는 안전장치
	if (BehaviorTreeAsset)
	{
		//실제 BehaviorTree를 돌리기 시작하는 실행 명령
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

void AZombieAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);

	Super::OnUnPossess();
}

//AIPerception이 뭔가를 감지하거나 놓칠 때마다 호출된다. - 그 결과를 Blackboard의
//ChaseTarget 키에 반영해서, Behavior Tree가 추격할지 순찰할지 판단할 수 있게 해준다.
void AZombieAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	//블랙보드 컴포넌트를 찾지 못하면 아래까지 내려가지 않도록 여기서 끊어버린다.
	if (!BlackboardComp || !Actor) {
		return;
	}

	// 감지된 Actor가 Pawn인지 확인한다.
	APawn* DetectedPawn = Cast<APawn>(Actor);
	// Pawn이 아니거나 플레이어가 조종하는 Pawn이 아니라면
	// 추격 대상으로 사용하지 않고 무시한다.
	if (!DetectedPawn || !DetectedPawn->IsPlayerControlled())
	{
		return;
	}

	//죽은 플레이어가 낸 소리/모습에 반응하지 않도록 설정
	APlayerCharacter* PlayerTarget = Cast<APlayerCharacter>(Actor);
	if (PlayerTarget && PlayerTarget->IsDead())
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		if (!Stimulus.WasSuccessfullySensed())
		{
			return;
		}

		EZombieState CurrentZombieState = EZombieState::Patrol;
		if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
		{
			CurrentZombieState = Zombie->GetCurrentState();
		}

		if (CurrentZombieState == EZombieState::Investigating || CurrentZombieState == EZombieState::Chase)
		{
			return;
		}

		BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);
		BlackboardComp->SetValueAsVector(BBKey_LastKnownLocation, Stimulus.StimulusLocation);
		SetZombieState(EZombieState::Investigating);

		if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
		{
			Zombie->RefreshAttackRange();
		}
		
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Chase"));
#endif
		//새로 감지됐으므로, 이 대상을 쫓아가야 한다고 ChaseTarget을 지정해준다.
		BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);
		SetZombieState(EZombieState::Chase);

		if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
		{
			Zombie->RefreshAttackRange();
		}
	}
	else
	{
		if (CheckSearchTurnVisibility(Actor))
		{
			BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);
			SetZombieState(EZombieState::Chase);

			if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
			{
				Zombie->RefreshAttackRange();
			}
		}
		else
		{
			SetZombieState(EZombieState::Search);
		}
	}
}

//MaxAge(5초) 동안 다시 감지되지 않아 AIPerception이 대상을 완전히 잊었을 때 호출된다.
//이 시점에 ChaseTarget을 실제로 비워서, Behaivor Tree가 추격을 포기하고
//순찰 등 다른 행동으로 전환할 수 있게 해준다.
void AZombieAIController::OnPerceptionForgotten(AActor* Actor)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp || !Actor) {
		return;
	}

	//지금 Blackboard에 저장된 ChaseTarget이 "잊혀진 그 Actor"가 맞는지 확인한다.
	//(다른 대상으로 이미 바뀌어 있는데 예전 이벤트 때문에 잘못 지우는 것을 방지)
	if (BlackboardComp->GetValueAsObject(BBKey_ChaseTarget) == Actor)
	{
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Forgotten"));
#endif
		BlackboardComp->ClearValue(BBKey_ChaseTarget);
		SetZombieState(EZombieState::Patrol);
	}
}

void AZombieAIController::CheckSearchTurnSight()
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	AActor* ChaseTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(BBKey_ChaseTarget));

	if (!IsValid(ChaseTarget))
	{
		return;
	}

	if (CheckSearchTurnVisibility(ChaseTarget))
	{
		SetZombieState(EZombieState::Chase);
		BlackboardComp->SetValueAsVector(BBKey_LastKnownLocation, ChaseTarget->GetActorLocation());
	}
}

void AZombieAIController::SetZombieState(EZombieState NewState)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

	if (!BlackboardComp || !Zombie)
	{
		return;
	}

	if (Zombie->GetCurrentState() == EZombieState::Dead)
	{
		return;
	}

	BlackboardComp->SetValueAsEnum(BBKey_State, static_cast<uint8>(NewState));
	Zombie->SetCurrentState(NewState);
}


//계속 LastKnownLocation을 갱신해서, 시야를 놓쳤을 때 그 마지막 위치로
//이동해 수색하는 등의 행동에 쓸 수 있게 해준다.
void AZombieAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	if (AZombieCharacter* DebugZombie = Cast<AZombieCharacter>(GetPawn()))
	{
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("ActorYaw: %.1f"), DebugZombie->GetActorRotation().Yaw));
		GEngine->AddOnScreenDebugMessage(101, 0.0f, FColor::Green,
			FString::Printf(TEXT("ControlYaw: %.1f"), GetControlRotation().Yaw));
#endif
	}

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

	if (Zombie && Zombie->GetCurrentState() == EZombieState::Search)
	{
		FRotator CurrentRot = Zombie->GetActorRotation();

		if (!FMath::IsNearlyZero(CurrentRot.Pitch) || !FMath::IsNearlyZero(CurrentRot.Roll))
		{
			Zombie->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}

	AActor* ChaseTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(BBKey_ChaseTarget));

	if (!IsValid(ChaseTarget))
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ChaseTarget);
	if (PlayerCharacter && PlayerCharacter->IsDead())
	{
		BlackboardComp->ClearValue(BBKey_ChaseTarget);
		SetZombieState(EZombieState::Patrol);
		//Player 사망 시 HideSpot 수색도 같이 종료
		BlackboardComp->SetValueAsBool(BBKey_bInvestigatingHideSpot, false);
		bWasPlayerHidingLastFrame = false;

		return;	
	}

	bool bIsChasing = (Zombie && Zombie->GetCurrentState() == EZombieState::Chase);	

	if (bIsChasing)
	{
		BlackboardComp->SetValueAsVector(BBKey_LastKnownLocation, ChaseTarget->GetActorLocation());
	}

	//HideSpot(은신) 감지 - "목격된 상태로 숨는 순간"을 포착 
	//player연결해야함
	if (PlayerCharacter)
	{
		AActor* HideSpot = PlayerCharacter->GetCurrentHidingSpot();
		bool bIsHidingNow = (HideSpot != nullptr);

		if (bIsHidingNow && !bWasPlayerHidingLastFrame && bIsChasing)
		{
			BlackboardComp->SetValueAsVector(BBKey_KnownHideSpotLocation, HideSpot->GetActorLocation());
			BlackboardComp->SetValueAsBool(BBKey_bInvestigatingHideSpot, true);
		}
		bWasPlayerHidingLastFrame = bIsHidingNow;
	}
}

bool AZombieAIController::CheckSearchTurnVisibility(AActor* Target) const
{
	APawn* MyPawn = GetPawn();
	AZombieCharacter* Zombie = MyPawn ? Cast<AZombieCharacter>(MyPawn) : nullptr;

#if WITH_EDITOR
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Orange,
		FString::Printf(TEXT("CheckSearchTurnVisibility called - Zombie:%s Target:%s IsPlaying:%s"),
			Zombie ? TEXT("O") : TEXT("X"),
			Target ? TEXT("O") : TEXT("X"),
			(Zombie && Zombie->IsPlayingSearchTurn()) ? TEXT("O") : TEXT("X")));
#endif

	if (!Zombie || !IsValid(Target) || !Zombie->IsPlayingSearchTurn())
	{
		return false;
	}

	//바라보는 방향 계산(Z축 제거하여 2D Yaw 회전만 깔끔하게 계산)
	const float SearchTurnOffsetDeg = 90.0f;//실제 애니메이션이 도는 각도에 맞춰 조정 가능
	const float SignedOffset = Zombie->bSearchTurnMirrored ? -SearchTurnOffsetDeg : SearchTurnOffsetDeg;

	FRotator LookRotation = Zombie->GetActorRotation();
	LookRotation.Yaw = FRotator::NormalizeAxis(LookRotation.Yaw + SignedOffset);
	LookRotation.Pitch = 0.0f;
	LookRotation.Roll = 0.0f;
	FVector LookDirection = LookRotation.Vector();

	// 시점 위치(Eye Location)와 타겟의 중심 위치(Target Location) 계산
	FVector StartLoc = Zombie->GetPawnViewLocation();
	FVector TargetLoc = Target->GetTargetLocation();

	const float MaxSearchTurnSightDistance = SightConfig
		? SightConfig->LoseSightRadius : 2000.0f;

	if (FVector::DistSquared2D(StartLoc, TargetLoc) > FMath::Square(MaxSearchTurnSightDistance))
	{
		return false;
	}

#if WITH_EDITOR
	DrawDebugLine(GetWorld(), Zombie->GetActorLocation(), Zombie->GetActorLocation() + Zombie->GetActorForwardVector() * 300.0f, FColor::Red, false, 0.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), Zombie->GetActorLocation(), Zombie->GetActorLocation() + LookDirection * 300.0f, FColor::Green, false, 0.0f, 0, 2.0f);
#endif
	//방향 벡터도 Z축을 제거하여 높이차로 인한 각도 왜곡 방지
	FVector DirToTarget2D = (TargetLoc - StartLoc);
	DirToTarget2D.Z = 0.0f;
	DirToTarget2D.Normalize();

	FVector LookDir2D = LookDirection;
	LookDir2D.Z = 0.0f;
	LookDir2D.Normalize();

	//수평 각도 차이 계산
	float DotResult = FVector::DotProduct(LookDir2D, DirToTarget2D);
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotResult, -1.0f, 1.0f)));

#if WITH_EDITOR
	DrawDebugString(GetWorld(), Zombie->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Angle: %.1f / Mirrored: %d"), AngleDeg, Zombie->bSearchTurnMirrored), nullptr, FColor::Yellow, 0.0f);
#endif

	const float SearchTurnSightAngle = 45.0f;

#if WITH_EDITOR
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("SearchTurn AngleDeg: %.1f / Threshold: %.1f / Mirrored: %s"),
		AngleDeg, SearchTurnSightAngle, Zombie->bSearchTurnMirrored ? TEXT("true") : TEXT("false")));
#endif

	if (AngleDeg > SearchTurnSightAngle)
	{
		return false;
	}

	//LineTrace (자기 자신 및 자기 컴포넌트 모두 무시)
	FHitResult Hit;
	FCollisionQueryParams Params;
	//Zombie 자신만 충돌 무시
	Params.AddIgnoredActor(Zombie);
	//Params.AddIgnoredActor(Target);

	//좀비의 모든 자식 컴포넌트(메시, 콜리전 등)도 무시 대상에 추가
	TArray<UPrimitiveComponent*> ZombieComponents;
	Zombie->GetComponents<UPrimitiveComponent>(ZombieComponents);
	for (UPrimitiveComponent* Comp : ZombieComponents)
	{
		Params.AddIgnoredComponent(Comp);
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		StartLoc,
		TargetLoc,
		ECC_Visibility,
		Params
	);

#if WITH_EDITOR
	DrawDebugLine(GetWorld(), StartLoc, TargetLoc, (!bHit || Hit.GetActor() == Target) ? FColor::Green : FColor::Red, false, 0.1f, 0, 1.5f);
#endif

	//시야선 상 장애물에 걸리지 않았거나, 장애물로 판정된 Actor가 Target 본인인 경우 가시 범위 인정
	return !bHit || (Hit.GetActor() == Target) || (Hit.GetActor() && Hit.GetActor()->IsAttachedTo(Target));
}

void AZombieAIController::OnAttackCooldownFinished()
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	AActor* ChaseTarget = BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(BBKey_ChaseTarget)) : nullptr;

	if (IsValid(ChaseTarget))
	{
		SetZombieState(EZombieState::Chase);
	}
	else
	{
		SetZombieState(EZombieState::Patrol);
	}
}

//Behavior Tree Task 등에서 호출 - 공격을 시작할 때 이동을 멈추고
//Blackboard 상태를 갱신한 뒤, 실제로 좀비 캐릭터의 공격 애니메이션을 재생시킨다.
void AZombieAIController::StartAttack()
{
	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());
	if (!Zombie || Zombie->GetCurrentState() == EZombieState::Attacking)
	{
		return;
	}

	//현재 실행 중인 Move To 등의 이동을 중단시킨다.
	StopMovement();

	//BT가 "좀비가 공격 중인지" 알 수 있게 Blackboard 값을 true로 바꾼다.
	//IsAttacking = true "지금 공격 중이다"라는 상태를 Blackboard에 저장한다.
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("True"));
#endif

	SetZombieState(EZombieState::Attacking);
	Zombie->PlayAttackMontage();
}

//공격 애니메이션이 끝난 뒤 실행해서 "이제 공격이 끝났다"고 알려주는 역할이다. BT가 다시 이동/추적 판단을 할 수 있게 해준다.
void AZombieAIController::FinishAttack()
{
	//여기서 바로 IsAttacking을 내리지 않고, Cooldown 시간만큼 기다렸다가 내린다.
	GetWorldTimerManager().SetTimer(
		AttackCooldownTimerHandle,
		this,
		&AZombieAIController::OnAttackCooldownFinished,
		AttackCooldown,
		false
	);
}

void AZombieAIController::StartSearchTurn()
{
	//1. AI의 이동 명령을 즉시 강제 종료(캡슐 회전 고정 해제)
	StopMovement();

	if (UPathFollowingComponent* PFComp = GetPathFollowingComponent())
	{
		PFComp->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}

	//2. 바라보고 있던 타겟 Focus 제거
	ClearFocus(EAIFocusPriority::Gameplay);

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());
	if (Zombie)
	{
		SetZombieState(EZombieState::Search);
		Zombie->SetSearchBaseYaw(Zombie->GetActorRotation().Yaw);//집입 시점 각도를 기준으로 고정
		Zombie->PlaySearchTurnMontage();
	}

}
