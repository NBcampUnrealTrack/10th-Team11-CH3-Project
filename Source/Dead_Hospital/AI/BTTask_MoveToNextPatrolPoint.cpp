#include "BTTask_MoveToNextPatrolPoint.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "ZombieCharacter.h"

UBTTask_MoveToNextPatrolPoint::UBTTask_MoveToNextPatrolPoint()
{
	NodeName = TEXT("Move To Next Patrol Point");

	LocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToNextPatrolPoint, LocationKey));
}

EBTNodeResult::Type UBTTask_MoveToNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(AIController->GetPawn());
	if (!Zombie) return EBTNodeResult::Failed;

	FVector NextLocation;
	if (!Zombie->GetNextPatrolLocation(NextLocation))
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	BlackboardComp->SetValueAsVector(LocationKey.SelectedKeyName, NextLocation);
	return EBTNodeResult::Succeeded;
}
