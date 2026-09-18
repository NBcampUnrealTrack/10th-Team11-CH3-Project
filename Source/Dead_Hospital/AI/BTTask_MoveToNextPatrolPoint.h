#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBkey.h"
#include "BTTask_MoveToNextPatrolPoint.generated.h"

UCLASS()
class DEAD_HOSPITAL_API UBTTask_MoveToNextPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_MoveToNextPatrolPoint();

	virtual  EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LocationKey;
};
