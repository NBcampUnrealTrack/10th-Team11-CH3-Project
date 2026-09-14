#include "BTTask_SetCurrentState.h"
#include "AIController.h"
#include "ZombieCharacter.h"

EBTNodeResult::Type UBTTask_SetCurrentState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	APawn* MyPawn = AIController->GetPawn();
	if (!MyPawn) return EBTNodeResult::Failed;

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(MyPawn);
	if (!Zombie) return EBTNodeResult::Failed;

	Zombie->SetCurrentState(NewState);

	return EBTNodeResult::Succeeded;
}
