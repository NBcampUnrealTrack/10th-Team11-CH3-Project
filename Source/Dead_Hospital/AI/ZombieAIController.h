#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UCLASS()
class DEAD_HOSPITAL_API AZombieAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AZombieAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	//이 AI 컨트롤러가 실제로 실행할 Behavior Tree 에셋 (에디터/블루프린트에서 BT_Zombie를 지정)
	UPROPERTY(EditAnywhere, Category = "AI")
	class UBehaviorTree* BehaviorTreeAsset;
	//좀비의 시야/청각 등 감지 기능을 담당하는 컴포넌트 (생성자에서 직접 생성 - 존재는 보이되 교체는 불가)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionComponent* AIPerception;
	//AIPerception에 등록할 시야 감지 설정 (시야 거리, 시야각 등을 결정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Sight* SightConfig;

	//AIPerception이 뭔가를 감지/놓쳤을 때 호출되는 콜백 함수
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);
	virtual void Tick(float DeltaTime) override;
	
public:
	void StartAttack();
	void FinishAttack();
};
