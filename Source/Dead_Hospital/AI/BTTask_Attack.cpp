#include "BTTask_Attack.h"
#include "ZombieAIController.h"

EBTNodeResult::Type UBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AZombieAIController* AIController = Cast<AZombieAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->StartAttack();
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
