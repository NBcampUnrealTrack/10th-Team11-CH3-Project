#include "BTTask_SearchTurn.h"
#include "ZombieAIController.h"

EBTNodeResult::Type UBTTask_SearchTurn::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AZombieAIController* AIController = Cast<AZombieAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->StartSearchTurn();
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
