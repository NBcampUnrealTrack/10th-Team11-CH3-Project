#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "PlayerCharacter.h"

#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

#include "Navigation/PathFollowingComponent.h"

//Blackboard 키 이름들을 static const FName으로 한 곳에 모아둠.
//문자열 리터럴을 여기저기 흩어놓으면 오타 나기 쉬워서(실제로 과거 오타 버그 있었음)
//상수로 한 번만 정의하고 코드는 항상 이걸 참조하게 함. 키 이름 변경도 여기 한 곳만 고치면 됨
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
	
	//청각 설정
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	//감지 가능한 최대 거리. 실제 소리 이벤트 발생은 소리 낸 쪽에서
	//UAISense_Hearing::ReportNoiseEvent()를 호출해줘야 함
	HearingConfig->HearingRange = 1000.0f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	//두 감각을 실제로 AIPerception에 등록해야 작동을 시작함
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	//여러 감각을 등록했을 때 대표(기준)로 삼을 감각을 시야로 지정
	//지금은 시야뿐이라 당장 큰 차이는 없지만, 나중에 청각 등을 추가할 때를 대비한 명시적 설정
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	//직전 프레임 플레이어 은신 여부 - "숨는 순간"을 잡기 위한상태값
	bWasPlayerHidingLastFrame = false;

	//공격 후 다음 공격까지 대기 시간(초)
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

//Pawn에서 떨어져 나갈 때 호출 - 공격 쿨다운 쿨이머 정리(죽은 좀비 참조 방지)
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

	//감지 대상이 플레이어 조종 Pawn인지 확인, 아니면 무시
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

	//청각 자극 처리
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		//성공적으로 감지된 게 아니면 무시(청각은 실패 시 별도 처리 불필요)
		if (!Stimulus.WasSuccessfullySensed())
		{
			return;
		}

		EZombieState CurrentZombieState = EZombieState::Patrol;
		if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
		{
			CurrentZombieState = Zombie->GetCurrentState();
		}

		//이미 Investigating/Chase 중이면 소리 하나 더 들었다고 상태를 낮출 필요 없음
		if (CurrentZombieState == EZombieState::Investigating || CurrentZombieState == EZombieState::Chase)
		{
			return;
		}

		//소리는 간접 정보이므로 Chase가 아니라 한 단계 낮은 Invesigating으로 전환
		BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);
		BlackboardComp->SetValueAsVector(BBKey_LastKnownLocation, Stimulus.StimulusLocation);
		SetZombieState(EZombieState::Investigating);

		if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
		{
			Zombie->RefreshAttackRange();
		}
		//청각 처리는 여기서 종료, 아래 시야 로직으로 넘어가지 않음
		return;
	}

	//시야 자극 처리
	if (Stimulus.WasSuccessfullySensed())
	{
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Chase"));
#endif
		//새로 감지됐으므로, 이 대상을 쫓아가야 한다고 ChaseTarget을 지정해준다.
		BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);

		const FVector LastSeenLocation = Actor->GetActorLocation();

		BlackboardComp->SetValueAsVector(
			BBKey_LastKnownLocation,
			Actor->GetActorLocation()
		);
#if WITH_EDITOR
		/*DrawDebugSphere(
			GetWorld(),
			LastSeenLocation,
			40.0f,
			12,
			FColor::Yellow,
			false,
			5.0f
		);*/
#endif
		
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

		if (!Zombie || Zombie->GetCurrentState() != EZombieState::Attacking)
		{
			SetZombieState(EZombieState::Chase);
		}

		if (Zombie)
		{
			Zombie->RefreshAttackRange();
		}
	}
	else
	{
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

		//이미 Chase 중이면 시야를 잠깐 놓쳐도 상태를 바꾸지 않고 그대로 추격 유지.
		//진짜로 놓쳤는지 판정은 여기가 아니라 OnPerceptionForgotten(MaxAge 5초)에서 처리함
		if (Zombie && (Zombie->GetCurrentState() == EZombieState::Chase || Zombie->GetCurrentState() == EZombieState::Attacking))
		{
			const float LoseRadius = SightConfig ? SightConfig->LoseSightRadius : 2000.0f;
			const float DistanceToPlayer = FVector::Dist(Zombie->GetActorLocation(), Actor->GetActorLocation());

			if (DistanceToPlayer <= LoseRadius)
			{
				return;
			}

			BlackboardComp->SetValueAsVector(
				BBKey_LastKnownLocation,
				Actor->GetActorLocation()
			);

			SetZombieState(EZombieState::MoveToLastKnown);
			return;
		}

		if (Zombie && Zombie->GetCurrentState() == EZombieState::Chase)
		{
			const float DistToStimulus = FVector::Dist(Zombie->GetActorLocation(), Stimulus.StimulusLocation);
			const float LoseRadius = SightConfig ? SightConfig->LoseSightRadius : 2000.0f;

			if (DistToStimulus <= LoseRadius)
			{
				return;
			}
		}

		//시야 감지 실패로 전환된 경우. Search 몽타주 재생 중이면 캡슐이 이미
		//플레이어 쪽을 보고 있을 수 있으므로 CheckSearchTurnVisibility로 재확인
		//(Search 중 정면에 있어도 Chase 재진입 안 되던 버그 대응)
		if (CheckSearchTurnVisibility(Actor))
		{
			BlackboardComp->SetValueAsObject(BBKey_ChaseTarget, Actor);
			SetZombieState(EZombieState::Chase);

			if (Zombie)
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

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

	if (Zombie && Zombie->GetCurrentState() == EZombieState::MoveToLastKnown)
	{
		BlackboardComp->ClearValue(BBKey_ChaseTarget);
		return;
	}

	//지금 Blackboard에 저장된 ChaseTarget이 "잊혀진 그 Actor"가 맞는지 확인한다.
	//(다른 대상으로 이미 바뀌어 있는데 예전 이벤트 때문에 잘못 지우는 것을 방지)
	if (BlackboardComp->GetValueAsObject(BBKey_ChaseTarget) == Actor)
	{
#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Forgotten"));
#endif
		//완전히 놓친 순간 - 바로 Patrol이 아니라 먼저 Search로 보내서
		//마지막으로 쫓던 위치를 확인하게 함. LastKnownLocation은 Chase 중
		//Tick()이 매 프레임 실시간으로 갱신해온 값이라 이 시점 기준 최신 위치임
		SetZombieState(EZombieState::Search);
		BlackboardComp->ClearValue(BBKey_ChaseTarget);
	}
}

//Search 상태에서 지금 실제로 타겟이 보이는지 확인해서 보이면 Chase로 전환.
//애니메이션 Notify 등에서 주기적으로 호출되는 것으로 보임
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

//좀비 State 변경의 통합 진입점
//Blackboard의 State 키와 캐릭터의 CurrentState를 동시에 갱신해서 값이 어긋나지 않게 함
//(캐릭터 SetCurrent를 직접 호출하면 Blackboard가 안 바뀌어 BT가 엉뚱한 값을 보게 됨)
void AZombieAIController::SetZombieState(EZombieState NewState)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());

	if (!BlackboardComp || !Zombie)
	{
		return;
	}

	//이미 죽은 좀비의 상태는 절대 덮어쓰지 않음(사망과 겹치는 타이밍 문제 방지)
	if (Zombie->GetCurrentState() == EZombieState::Dead)
	{
		return;
	}

	BlackboardComp->SetValueAsEnum(BBKey_State, static_cast<uint8>(NewState));
	Zombie->SetCurrentState(NewState);
}


//매 프레임 실행되는 감시/보정 로직 모음
//1) 디버그 Yaw 출력 2) Search 중 Pitch/Roll 보정 3) 추적 대상 사망 확인
//4) Chase 중 LastKnownLocation 갱신 5) "목격된 채로 숨는 순간" 포착
void AZombieAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	//[디버그] 캡슐 Yaw와 ControlRotation Yaw를 비교 출력(시야 튐 버그 확인용, 회귀 확인용으로 유지)
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

	//Pitch/Roll 보정 Search 중엔 SearchTurn 몽타주의 Root Motion이 캡슐을 회전시키는데,
	//이때 미세한 Pitch/Roll까지 섞여 캡슐이 기울어지는 부작용이 있어 Yaw만 남기고 매 프레임 리셋.
	//아래 "ChaseTarget 없으면 return"보다 반드시 위에 있어야 함 - Search 진입 직후엔
	//보통 ChaseTarget이 비어있어서, 이 코드가 return 밑에 있으면 보정 자체가 안 먹음
	if (Zombie && Zombie->GetCurrentState() == EZombieState::Search)
	{
		FRotator CurrentRot = Zombie->GetActorRotation();

		if (!FMath::IsNearlyZero(CurrentRot.Pitch) || !FMath::IsNearlyZero(CurrentRot.Roll))
		{
			Zombie->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}

	//여기서부터는 ChaseTarget이 있어야 의미 있는 로직이므로 없으면 종료
	AActor* ChaseTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(BBKey_ChaseTarget));

	if (!IsValid(ChaseTarget))
	{
		return;
	}

	//추적 대상 사망 확인 쫓던 플레이어가 죽었으면 추적/조사 상태 정리하고 Patrol 복귀
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

	bool bIsCurrentlySeen = false;
	if (AIPerception)
	{
		TArray<AActor*> PerceivedActors;
		AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
		bIsCurrentlySeen = PerceivedActors.Contains(ChaseTarget);
	}

	//[LastKnownLocation 갱신] Chase 중이면 매 프레임 대상 위치를 계속 기록해서,
	//나중에 시야를 놓쳤을 때 수색 기준점으로 사용
	bool bIsChasing = (Zombie && Zombie->GetCurrentState() == EZombieState::Chase);	
	bool bIsTrackingTarget = (Zombie && Zombie->GetCurrentState() == EZombieState::Chase || Zombie->GetCurrentState() == EZombieState::Attacking);

	if (bIsTrackingTarget && bIsCurrentlySeen)
	{
		BlackboardComp->SetValueAsVector(BBKey_LastKnownLocation, ChaseTarget->GetActorLocation());
	}

	if (bIsChasing)
	{

		//거리 기반 Chase 속도 조절 - 멀수록 빠르게, 가까울수록 느리게
		const float DistanceToTarget = FVector::Dist(Zombie->GetActorLocation(), ChaseTarget->GetActorLocation());

		const float  NewChaseSpeed = FMath::GetMappedRangeValueClamped(
			FVector2D(Zombie->GetMinChaseDistance(), Zombie->GetMaxChaseDistance()),
			FVector2D(Zombie->GetMinChaseSpeed(), Zombie->GetMaxChaseSpeed()),
			DistanceToTarget
		);

		Zombie->GetCharacterMovement()->MaxWalkSpeed = NewChaseSpeed;

#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(102, 0.0f, FColor::Magenta,
			FString::Printf(TEXT("Dist: %.1f / Speed: %.1f"), DistanceToTarget, NewChaseSpeed));
#endif
	
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
		//다음 프레임 비교를 위해 이번 프레임 은신 여부 저장
		bWasPlayerHidingLastFrame = bIsHidingNow;
	}
}

//Search 상태에서 "지금 실제로 캡슐이 향한 방향" 기준으로 타겟이 시야각 안에
//있는지 직접 계산. AIPerception 기본 판정은 GetActorEyesViewPoint 기준인데,
//Search 중엔 몽타주가 캡슐을 계속 돌리고 있어 판정 주기와 타이밍이 어긋날 수 있어서
//이 순간 기준으로 각도/라인트레이스를 직접 재계산하는 보조 판정
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

	//좀비/타겟이 없거나 SearchTurn 재생 중이 아니면 이 판정 자체가 불필요
	if (!Zombie || !IsValid(Target) || !Zombie->IsPlayingSearchTurn())
	{
		return false;
	}

	//바라보는 방향 계산(Z축 제거하여 2D Yaw 회전만 깔끔하게 계산)
	const float SearchTurnOffsetDeg = 90.0f;//실제 애니메이션이 도는 각도에 맞춰 조정 가능
	const float SignedOffset = Zombie->bSearchTurnMirrored ? -SearchTurnOffsetDeg : SearchTurnOffsetDeg;

	FRotator LookRotation = Zombie->GetActorRotation();
	LookRotation.Yaw = FRotator::NormalizeAxis(LookRotation.Yaw + SignedOffset);
	//수평 각도만 비교하기 위해 Pitch/Roll 고정
	LookRotation.Pitch = 0.0f;
	LookRotation.Roll = 0.0f;
	FVector LookDirection = LookRotation.Vector();

	// 시점 위치(Eye Location)와 타겟의 중심 위치(Target Location) 계산
	FVector StartLoc = Zombie->GetPawnViewLocation();
	FVector TargetLoc = Target->GetTargetLocation();

	//시야 최대 거리 - SightConfig가 있으면 LoseSightRadius 재사용
	//"시야가 완전히 끊기는 거리" 기준을 일관되게 유지하기 위함
	const float MaxSearchTurnSightDistance = SightConfig
		? SightConfig->LoseSightRadius : 2000.0f;

	//sqrt 연산을 피하기 위해 제곱 거리로 비교(2D 기준, 높이차 무시)
	if (FVector::DistSquared2D(StartLoc, TargetLoc) > FMath::Square(MaxSearchTurnSightDistance))
	{
		return false;
	}

#if WITH_EDITOR
	//캡슐 정면(빨강)과 오프셋 보정된 방향(초록)을 그려서 비교
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

	//내적으로 두 방향 사이 각도 계산. Clamp는 부동소수점 오차로 Acos가 NaN 나는 것 방지
	float DotResult = FVector::DotProduct(LookDir2D, DirToTarget2D);
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotResult, -1.0f, 1.0f)));

#if WITH_EDITOR
	DrawDebugString(GetWorld(), Zombie->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Angle: %.1f / Mirrored: %d"), AngleDeg, Zombie->bSearchTurnMirrored), nullptr, FColor::Yellow, 0.0f);
#endif

	//이 각도(45도) 이내여야 시야각 안으로 판정
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
	//Params.AddIgnoredActor(Target); //Target까지 무시하면 "Target 뒤의 벽" 확인이 안 되므로 미사용)

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
	
	//다음 중 하나면 "보인다"로 판정:
	//1) 아무것도 안 맞음 2) 타겟 본인에 맞음 3) 타겟에 Attach된 다른 액터(장비 등)에 맞음
	return !bHit || (Hit.GetActor() == Target) || (Hit.GetActor() && Hit.GetActor()->IsAttachedTo(Target));
}

//공격 쿨다운 종료 시 호출 - 여전히 쫓을 대상 있으면 Chase, 없으면 Patrol로 복귀
void AZombieAIController::OnAttackCooldownFinished()
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	AActor* ChaseTarget = BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(BBKey_ChaseTarget)) : nullptr;

	//ChaseTarget이 아예 없다(완전히 잊혀진 상태) -> Patrol
	if (!IsValid(ChaseTarget))
	{
		SetZombieState(EZombieState::Patrol);
	}

	//ChaseTarget은 아직 안 잊혀졌더라도(MaxAge 5초 이내), "지금 이 순간" 진짜로 보이는지
	//AIPerception한테 직접 물어본다. IsValid만 보면 Search 중이어도 무조건 true라서
	//Search를 강제로 덮어쓰고 Chase로 되돌리는 문제가 있었음
	bool bIsCurrentlySeen = false;
	if (AIPerception)
	{
		TArray<AActor*> PerceivedActors;
		AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
		bIsCurrentlySeen = PerceivedActors.Contains(ChaseTarget);
	}

	if (bIsCurrentlySeen)
	{
		SetZombieState(EZombieState::Chase);
	}
	else if (AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn()))
	{
		//지금 안 보인다면, 이미 OnPerceptionUpdated가 Search/Investigating으로
		//제대로 넘겨놨을 테니 그 상태를 덮어쓰지 않는다.
		//단, 혹시 갱신 이벤트를 못 받아서 아직도 Attacking으로 남아잇는 예외 상황이면
		//안전하게 Search로 보내서 최소한 멈춰있지는 않게 한다.
		if (Zombie->GetCurrentState() == EZombieState::Attacking)
		{
			SetZombieState(EZombieState::Chase);
		}
	}

	OnAttackSequenceFinished.Broadcast();
}

//Behavior Tree Task 등에서 호출 - 공격을 시작할 때 이동을 멈추고
//Blackboard 상태를 갱신한 뒤, 실제로 좀비 캐릭터의 공격 애니메이션을 재생시킨다.
void AZombieAIController::StartAttack()
{
	AZombieCharacter* Zombie = Cast<AZombieCharacter>(GetPawn());
	//좀비가 없거나 이미 공격 중이면 중복 시작 방지
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

//Search(두리번거림) 상태 시작 - 이동 정지, Focus 해제, State 전환 후 몽타주 재생
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

		Zombie->GetCharacterMovement()->MaxWalkSpeed = Zombie->GetPatrolSpeed();
		//진짜로 제자리 두리번거림이 시작되는 이 시점에만 이동 방향 회전을 끈다.
		Zombie->GetCharacterMovement()->bOrientRotationToMovement = false;

		Zombie->PlaySearchTurnMontage();
	}

}
