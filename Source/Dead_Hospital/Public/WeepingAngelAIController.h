#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WeepingAngelAIController.generated.h"

UCLASS()
class DEAD_HOSPITAL_API AWeepingAngelAIController : public AAIController
{
	GENERATED_BODY()

public:
	AWeepingAngelAIController();

	// 매 프레임 실행 - 플레이어 위치로 지속적으로 MoveTo 갱신
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// 빙의 시 초기 추적 대상(플레이어) 캐싱
	virtual void OnPossess(APawn* InPawn) override;

private:
	// 매 프레임 MoveToActor를 새로 호출하면 내부적으로 경로 재계산 비용이 들기 때문에,
	// 일정 주기로만 목적지를 갱신한다 (우는 천사는 어차피 순간이동하듯 빠르게 다가오는 컨셉이라
	// 약간의 갱신 지연이 있어도 체감상 문제 없음)
	UPROPERTY(EditAnywhere, Category = "WeepingAngel")
	float MoveToUpdateInterval = 0.2f;

	// 목적지 갱신 주기를 재는 누적 시간
	float TimeSinceLastMoveToUpdate = 0.0f;

	// 항상 쫓아갈 대상(플레이어) - BeginPlay/OnPossess 시점에 미리 찾아둠
	UPROPERTY()
	class APawn* CachedPlayerPawn;

	// 플레이어 Pawn을 찾아 CachedPlayerPawn에 캐싱 (아직 못 찾았으면 재시도)
	void TryCachePlayerPawn();
};