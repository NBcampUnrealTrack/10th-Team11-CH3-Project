#include "ZombieAIController.h"

AZombieAIController::AZombieAIController()
{

}

void AZombieAIController::BeginPlay()
{
	Super::BeginPlay();
	
	GetWorldTimerManager().SetTimer(
		RandomMoveTImer,
		this,
		&AZombieAIController::MoveToRandomLocation,
		3.0f,
		true,
		1.0f
	);
}

void AZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AZombieAIController::MoveToRandomLocation()
{

}
