#include "PlayerCharacterController.h"
#include "EnhancedInputSubsystems.h"

APlayerCharacterController::APlayerCharacterController()
	: InputMappingContext(nullptr),
	MoveAction(nullptr),
	SitAction(nullptr),
	LookAction(nullptr),
	SprintAction(nullptr),
	InteractAction(nullptr),
	FlashlightAction(nullptr),
	InventoryAction(nullptr),
	DocumentAction(nullptr),
	QuickSlot1Action(nullptr),
	QuickSlot2Action(nullptr),
	QuickSlot3Action(nullptr),
	ReloadAction(nullptr),
	FireAction(nullptr)
{
}

void APlayerCharacterController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);
			}
		}
	}
}
