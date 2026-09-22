#include "WeepingAngelAIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AWeepingAngelAIController::AWeepingAngelAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	CachedPlayerPawn = nullptr;
}

void AWeepingAngelAIController::BeginPlay()
{
	Super::BeginPlay();

	// OnPossess보다 BeginPlay가 먼저 불리는 순서 보장이 애매할 수 있어
	// 여기서도 한 번 더 시도해둔다 (둘 중 하나만 성공해도 됨)
	TryCachePlayerPawn();
}

void AWeepingAngelAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 우는 천사는 별도 Behavior Tree 없이 이 컨트롤러가 직접 MoveTo를 굴린다.
	// (AZombieAIController처럼 RunBehaviorTree를 호출하지 않음)
	TryCachePlayerPawn();
}

void AWeepingAngelAIController::TryCachePlayerPawn()
{
	// 이미 캐싱되어 있으면 재탐색 불필요
	if (CachedPlayerPawn)
	{
		return;
	}

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void AWeepingAngelAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 혹시 BeginPlay/OnPossess 시점에 플레이어가 아직 스폰 전이었을 경우를 대비해
	// 캐싱 실패 시 매 프레임 재시도 (성공하면 TryCachePlayerPawn 내부에서 즉시 스킵되므로 비용 적음)
	if (!CachedPlayerPawn)
	{
		TryCachePlayerPawn();
		return;
	}

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	// 일정 주기로만 MoveTo 목적지를 갱신
	TimeSinceLastMoveToUpdate += DeltaTime;

	if (TimeSinceLastMoveToUpdate >= MoveToUpdateInterval)
	{
		TimeSinceLastMoveToUpdate = 0.0f;

		// 항상 플레이어를 목적지로 지정 - 시야/청각 감지 없이 무조건 추적
		// (AZombieAIController와 달리 ChaseTarget/Blackboard 개념 자체가 없음)
		MoveToActor(CachedPlayerPawn, 50.0f);
	}
}