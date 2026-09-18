#include "BTTask_ClearInvestigateFlag.h"
#include "ZombieAIController.h"
#include "BehaviorTree/BlackboardComponent.h"

EBTNodeResult::Type UBTTask_ClearInvestigateFlag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_bInvestigatingHideSpot, false);
	}

	return EBTNodeResult::Succeeded;
}
