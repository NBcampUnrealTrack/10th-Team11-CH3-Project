#include "PlayerCharacter.h"
#include "PlayerCharacterController.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


APlayerCharacter::APlayerCharacter()
{
 
	PrimaryActorTick.bCanEverTick = false;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));

	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->TargetArmLength = 0.0f;
	SpringArmComp->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	SpringArmComp->bUsePawnControlRotation = true;

	SpringArmComp->bDoCollisionTest = false;

	SpringArmComp->bEnableCameraLag = false;
	SpringArmComp->bEnableCameraRotationLag = false;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;

	NormalSpeed = 600.0f;
	SprintSpeedMultiplier = 1.5f;
	SprintSpeed = NormalSpeed * SprintSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;
}


void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->SetOwnerNoSee(true);
}



void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetController()))
		{
			if (CharacterController->MoveAction)
			{
				EnhancedInput->BindAction(
					CharacterController->MoveAction,
					ETriggerEvent::Triggered,
					this,
					&APlayerCharacter::Move
				);
			}

			if (CharacterController->SitAction)
			{
				EnhancedInput->BindAction(
					CharacterController->SitAction,
					ETriggerEvent::Triggered,
					this,
					&APlayerCharacter::StartSit
				);

				EnhancedInput->BindAction(
					CharacterController->SitAction,
					ETriggerEvent::Completed,
					this,
					&APlayerCharacter::StopSit
				);
			}

			if (CharacterController->LookAction)
			{
				EnhancedInput->BindAction(
					CharacterController->LookAction,
					ETriggerEvent::Triggered,
					this,
					&APlayerCharacter::Look
				);
			}

			if (CharacterController->SprintAction)
			{
				EnhancedInput->BindAction(
					CharacterController->SprintAction,
					ETriggerEvent::Triggered,
					this,
					&APlayerCharacter::StartSprint
				);

				EnhancedInput->BindAction(
					CharacterController->SprintAction,
					ETriggerEvent::Completed,
					this,
					&APlayerCharacter::StopSprint
				);
			}
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& value)
{
	if (!Controller) return;

	const FVector2D MoveInput = value.Get<FVector2D>();

	if (!FMath::IsNearlyZero(MoveInput.X))
	{
		AddMovementInput(GetActorForwardVector(), MoveInput.X);
	}

	if (!FMath::IsNearlyZero(MoveInput.Y))
	{
		AddMovementInput(GetActorRightVector(), MoveInput.Y);
	}
}

void APlayerCharacter::StartSit(const FInputActionValue& value)
{
	if (value.Get<bool>())
	{
		Sit();
	}
}

void APlayerCharacter::StopSit(const FInputActionValue& value)
{
	StopSitting();
}

void APlayerCharacter::Sit()
{
	if (bIsSitting) return;
	bIsSitting = true;

	DefaultMaxSpeed = GetCharacterMovement()->MaxWalkSpeed;
	GetCharacterMovement()->MaxWalkSpeed = SitSpeed;
}

void APlayerCharacter::StopSitting()
{
	if (!bIsSitting) return;
	bIsSitting = false;

	GetCharacterMovement()->MaxWalkSpeed = DefaultMaxSpeed;
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	FVector2D LookInput = value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void APlayerCharacter::StartSprint(const FInputActionValue& value)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void APlayerCharacter::StopSprint(const FInputActionValue& value)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;
	}
}

