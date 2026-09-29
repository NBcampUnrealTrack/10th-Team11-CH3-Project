#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ZombieCharacter.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UCLASS()
class DEAD_HOSPITAL_API AZombieAIController : public AAIController
{
	GENERATED_BODY()

protected:
	//이 AI 컨트롤러가 실제로 실행할 Behavior Tree 에셋 (에디터/블루프린트에서 BT_Zombie를 지정)
	UPROPERTY(EditAnywhere, Category = "AI")
	class UBehaviorTree* BehaviorTreeAsset;
	//좀비의 시야/청각 등 감지 기능을 담당하는 컴포넌트 (생성자에서 직접 생성 - 존재는 보이되 교체는 불가)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionComponent* AIPerception;
	//AIPerception에 등록할 시야 감지 설정 (시야 거리, 시야각 등을 결정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Sight* SightConfig;
	//AIPerception에 등록할 청각 감지 설정 (청각 반경 등을 결정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Hearing* HearingConfig;

	//직전 프레임에 플레이어가 은신 중이었는지 여부.
	bool bWasPlayerHidingLastFrame;
	//공격 한 번 하고 다음 공격까지 대기 쿨타임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float AttackCooldown;

	//Search 상태에서 시야 재확인을 주기적으로 예약하는 데 쓰이는 타이머
	FTimerHandle SearchTurnSightCheckTimerHandle;
	//공격 쿨다운 대기 타이머
	FTimerHandle AttackCooldownTimerHandle;
public:
	//하드코딩 방지를 위한 Blackboard Key 상의 정의
	static const FName BBKey_bInvestigatingHideSpot;
	static const FName BBKey_ChaseTarget;
	static const FName BBKey_InAttackRange;
	static const FName BBKey_KnownHideSpotLocation;
	static const FName BBKey_LastKnownLocation;
	static const FName BBKey_State;

public:
	AZombieAIController();
	//델리게이트 선언 추가
	DECLARE_MULTICAST_DELEGATE(FOnAttackSequenceFinished);
	FOnAttackSequenceFinished OnAttackSequenceFinished; //공격+쿨다운까지 완전 끝났을 때 터짐

	//매 프레임 실행 - Pitch/Roll 보정, LastKnownLocation 갱신, HideSpot 감지 등을 처리
	virtual void Tick(float DeltaTime) override;

	//공격 시작/종료 처리
	void StartAttack();
	void FinishAttack();
	
	// Search(두리번거림) 상태 시작 처리
	void StartSearchTurn();

	//Search 상태에서 "지금 실제로 캡슐이 향한 방향' 기준으로 타겟이 시야각 안에
	//들어와 잇는지 직접 계산하는 보조 판정 함수
	bool CheckSearchTurnVisibility(AActor* Target) const;
	//공격 쿨다운이 끝났을 때 호출되는 콜백
	void OnAttackCooldownFinished();

	//Search 상태에서 실제로 타겟이 보이는지 확인해서, 보이면 즉시 Chase로 전환하는 함수
	UFUNCTION(BlueprintCallable)
	void CheckSearchTurnSight();
	//좀비의 State를 변경하는 통합 진입점
	void SetZombieState(EZombieState NewState);

protected:
	virtual void BeginPlay() override;

	//AI 컨트롤러가 특정 Pawn(좀비)에 실제로 빙의하는 순간 호출 - Behavior Tree 실행 시작
	virtual void OnPossess(APawn* InPawn) override;
	//컨트롤러가 Pawn에서 떨어져 나갈 때 호출 - 남은 타이머 정리
	virtual void OnUnPossess() override;
	//AIPerception이 뭔가를 감지/놓쳤을 때 호출되는 콜백 함수
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	//AIPerception이 MaxAge 시간이 지나 대상을 완전히 "잊었을" 때 호출되는 콜백
	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);
	
};
