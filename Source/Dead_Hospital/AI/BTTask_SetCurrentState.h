#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "ZombieCharacter.h"
#include "BTTask_SetCurrentState.generated.h"

UCLASS()
class DEAD_HOSPITAL_API UBTTask_SetCurrentState : public UBTTaskNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "State")
	EZombieState NewState;
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
