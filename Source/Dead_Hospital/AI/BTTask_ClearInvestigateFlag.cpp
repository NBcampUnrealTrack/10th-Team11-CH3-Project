#include "BTTask_ClearInvestigateFlag.h"
#include "BehaviorTree/BlackboardComponent.h"

EBTNodeResult::Type UBTTask_ClearInvestigateFlag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsBool(TEXT("bInvestigateHideSpot"), false);
	}

	return EBTNodeResult::Succeeded;
}
