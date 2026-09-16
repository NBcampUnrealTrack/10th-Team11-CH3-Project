#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UCLASS()
class DEAD_HOSPITAL_API AZombieAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AZombieAIController();

	virtual void Tick(float DeltaTime) override;

	void StartAttack();
	void FinishAttack();
	void StartSearchTurn();
	bool CheckSearchTurnVisibility(AActor* Target) const;
	void OnAttackCooldownFinished();
	UFUNCTION(BlueprintCallable)
	void CheckSearchTurnSight();

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Hearing* HearingConfig;

	bool bWasPlayerHidingLastFrame;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float AttackCooldown;

	FTimerHandle SearchTurnSightCheckTimerHandle;
	FTimerHandle AttackCooldownTimerHandle;
public:
	//하드코딩 방지를 위한 Blackboard Key 상의 정의
	static const FName BBKey_ChaseTarget;
	static const FName BBKey_bCanSeeTarget;
	static const FName BBKey_LastKnownLocation;
	static const FName BBKey_IsAttacking;
	static const FName BBKey_bInvestigatingNoise;
	static const FName BBKey_InAttackRange;
	static const FName BBKey_KnownHideSpotLocation;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//AIPerception이 뭔가를 감지/놓쳤을 때 호출되는 콜백 함수
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);
	
};
