#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "DrawDebugHelpers.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Kismet/KismetSystemLibrary.h"

AZombieAIController::AZombieAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	//좀비의 감지 기능을 담당할 AIPerceptionn 컴포넌트를 생성
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
	
	//방금 설정한 SightConfig를 AIPerception 컴포넌트에 등록
	AIPerception->ConfigureSense(*SightConfig);
	//여러 감각을 등록했을 때 대표(기준)로 삼을 감각을 시야로 지정
	//지금은 시야뿐이라 당장 큰 차이는 없지만, 나중에 청각 등을 추가할 때를 대비한 명시적 설정
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
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

	if (Stimulus.WasSuccessfullySensed())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Chase"));
		//새로 감지됐으므로, 이 대상을 쫓아가야 한다고 ChaseTarget을 지정해준다.
		BlackboardComp->SetValueAsObject(TEXT("ChaseTarget"), Actor);
		BlackboardComp->SetValueAsBool(TEXT("bCanSeeTarget"), true);
	}
	else
	{
		if (CheckSearchTurnVisibility(Actor))
		{
			BlackboardComp->SetValueAsObject(TEXT("ChaseTarget"), Actor);
			BlackboardComp->SetValueAsBool(TEXT("bCanSeeTarget"), true);
		}
		else
		{
			//Sitmulus는 왓지만 실패(WasSuccessfullySensed == false)로 온 경우
			//즉 "방금 시야에서 놓쳤다"는 뜻. 아직 ChaseTarget 자체를 지우지는 않고
			//(LastKnownLocation으로 계속 추적할 수 있도록) bCanSeeTarget만 false로 내린다.
			//ChaseTarget을 완전히 비우는 처리는 OnPerceptionForgotten에서 한다.
			BlackboardComp->SetValueAsBool(TEXT("bCanSeeTarget"), false);
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
	if (BlackboardComp->GetValueAsObject(TEXT("ChaseTarget")) == Actor)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Forgotten"));
		//대상을 잃어버렸으므로, ChaseTarget을 비워서 다시 순찰 상태로 돌아가게 한다.
		BlackboardComp->ClearValue(TEXT("ChaseTarget"));
	}
}


//매 프레임마다 호출된다. 현재 타겟이 시야에 보이는 동안(bCanSeeTarget == true)
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
	UObject* ChaseTargetObj = BlackboardComp->GetValueAsObject(TEXT("ChaseTarget"));
	AActor* ChaseTarget = Cast<AActor>(ChaseTargetObj);

	if (!ChaseTarget)
	{
		return;
	}

	bool bCanSeeTarget = BlackboardComp->GetValueAsBool(TEXT("bCanSeeTarget"));

	if (bCanSeeTarget)
	{
		BlackboardComp->SetValueAsVector(TEXT("LastKnownLocation"), ChaseTarget->GetActorLocation());
	}

	if (ChaseTarget && !BlackboardComp->GetValueAsBool(TEXT("bCanSeeTarget")))
	{
		if (CheckSearchTurnVisibility(ChaseTarget))
		{
			BlackboardComp->SetValueAsBool(TEXT("bCanSeeTarget"), true);
		}
	}

	//APlayerCharacter* Player = Cast<APlayerCharacter>(ChaseTarget);
	//if (Player)
	//{
	//	bool bIsHidingNow = Player->IsHiding();

	//	if (bIsHidingNow && !bWasPlayerHidingLastFrame)
	//	{
	//		if (bCanSeeTarget)
	//		{
	//			if (AActor* HideSpot = Player->GetCurrentHideSpot())
	//			{
	//				BlackboardComp->SetValueAsVector(TEXT("KnownHideSpotLocation"), HideSpot->GetActorLocation());
	//				BlackboardComp->SetValueAsBool(TEXT("bInvestigateHideSpot"), true);
	//			}
	//		}
	//	}
	//	bWasPlayerHidingLastFrame = bIsHidingNow;
	//}
}

bool AZombieAIController::CheckSearchTurnVisibility(AActor* Target) const
{
	APawn* MyPawn = GetPawn();
	AZombieCharacter* Zombie = MyPawn ? Cast<AZombieCharacter>(MyPawn) : nullptr;

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Orange,
		FString::Printf(TEXT("CheckSearchTurnVisibility called - Zombie:%s Target:%s IsPlaying:%s"),
			Zombie ? TEXT("O") : TEXT("X"),
			Target ? TEXT("O") : TEXT("X"),
			(Zombie && Zombie->IsPlayingSearchTurn()) ? TEXT("O") : TEXT("X")));

	if (!Zombie || !Target || !Zombie->IsPlayingSearchTurn())
	{
		return false;
	}
	//정면 기준으로, 두리번 방향(좌/우)만큼 오프셋을 준 "바라보는 방향" 근사
	const float SearchTurnOffsetDog = 90.0f;//실제 애니메이션이 도는 각도에 맞춰 조정 가능
	const float SignedOffset = Zombie->bSearchTurnMirrored ? -SearchTurnOffsetDog : SearchTurnOffsetDog;

	FRotator LookRotation = Zombie->GetActorRotation();
	LookRotation.Yaw += SignedOffset;
	FVector LookDirection = LookRotation.Vector();

#if WITH_EDITOR
	DrawDebugLine(GetWorld(), Zombie->GetActorLocation(), Zombie->GetActorLocation() + Zombie->GetActorForwardVector() * 300.0f, FColor::Red, false, 0.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), Zombie->GetActorLocation(), Zombie->GetActorLocation() + LookDirection * 300.0f, FColor::Green, false, 0.0f, 0, 2.0f);
#endif

	FVector ToTarget = (Target->GetActorLocation() - Zombie->GetActorLocation()).GetSafeNormal();
	float DotResult = FVector::DotProduct(LookDirection, ToTarget);
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotResult, -1.0f, 1.0f)));

#if WITH_EDITOR
	DrawDebugString(GetWorld(), Zombie->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Angle: %.1f / Mirrored: %d"), AngleDeg, Zombie->bSearchTurnMirrored), nullptr, FColor::Yellow, 0.0f);
#endif

	const float SearchTurnSightAngle = 45.0f;

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("SearchTurn AngleDeg: %.1f / Threshold: %.1f / Mirrored: %s"),
		AngleDeg, SearchTurnSightAngle, Zombie->bSearchTurnMirrored ? TEXT("true") : TEXT("falst")));

	if (AngleDeg > SearchTurnSightAngle)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Zombie);
	Params.AddIgnoredActor(Target);

	bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Zombie->GetActorLocation() + FVector(0, 0, 50.0f),
		Target->GetActorLocation() + FVector(0, 0, 50.0f),
		ECC_Visibility,
		Params
	);

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, bBlocked ? FColor::Red : FColor::Green,
		FString::Printf(TEXT("LineTrace Blocked: %s"), bBlocked ? TEXT("Yes") : TEXT("No")));

	return !bBlocked;
}

//Behavior Tree Task 등에서 호출 - 공격을 시작할 때 이동을 멈추고
//Blackboard 상태를 갱신한 뒤, 실제로 좀비 캐릭터의 공격 애니메이션을 재생시킨다.
void AZombieAIController::StartAttack()
{
	//현재 실행 중인 Move To 등의 이동을 중단시킨다.
	StopMovement();

	//BT가 "좀비가 공격 중인지" 알 수 있게 Blackboard 값을 true로 바꾼다.
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		//IsAttacking = true "지금 공격 중이다"라는 상태를 Blackboard에 저장한다.
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("True"));
		BlackboardComp->SetValueAsBool(TEXT("IsAttacking"), true);
	}

	//이 AI 컨트롤러가 빙의 중인 Pawn(좀비 캐릭터)를 가져와서
	//실제 공격 몽타주 재생을 위임한다.
	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(MyPawn);
		if (Zombie)
		{
			Zombie->PlayAttackMontage();
		}
	}
}

//공격 애니메이션이 끝난 뒤 실행해서 "이제 공격이 끝났다"고 알려주는 역할이다. BT가 다시 이동/추적 판단을 할 수 있게 해준다.
void AZombieAIController::FinishAttack()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("False"));
		BlackboardComp->SetValueAsBool(TEXT("IsAttacking"), false);
	}
}

void AZombieAIController::StartSearchTurn()
{
	APawn* MyPawn = GetPawn();
	if(MyPawn)
	{ 
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(MyPawn);
		if (Zombie)
		{
			Zombie->PlaySearchTurnMontage();
		}
	}
}

