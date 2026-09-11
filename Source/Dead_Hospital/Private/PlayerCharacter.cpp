#include "PlayerCharacter.h"
#include "PlayerCharacterController.h"
//#include "InventoryComponent.h"
//#include "Interactable.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"


APlayerCharacter::APlayerCharacter()
{
 
	PrimaryActorTick.bCanEverTick = true;

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

	// 인벤토리 컴포넌트 생성
	//InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	NormalSpeed = 600.0f;
	SprintSpeedMultiplier = 1.5f;
	SprintSpeed = NormalSpeed * SprintSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;

	// HP/Stamina 초기값은 Max값으로 시작
	CurrentHP = MaxHP;
	CurrentStamina = MaxStamina;
}


void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->SetOwnerNoSee(true);

	CurrentHP = MaxHP;
	CurrentStamina = MaxStamina;
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsDead)
	{
		UpdateStamina(DeltaTime);
	}
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
			// E키(InteractAction) 바인딩
			if (CharacterController->InteractAction)
			{
				EnhancedInput->BindAction(
					CharacterController->InteractAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnInteractPressed
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
	// 스테미너 최소치보다 적으면 달리기 자체를 못하게 막음
	if (bIsDead || CurrentStamina < MinStaminaToSprint)
	{
		return;
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		bIsSprinting = true;
	}
}

void APlayerCharacter::StopSprint(const FInputActionValue& value)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;
	}
	bIsSprinting = false;
}

// 스테미너
void APlayerCharacter::UpdateStamina(float DeltaTime)
{
	if (bIsSprinting)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina - StaminaDrainRate * DeltaTime, 0.0f, MaxStamina);

		// 스테미너 다 떨어지면 강제로 달리기 중단
		if (CurrentStamina <= 0.0f)
		{
			bIsSprinting = false;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;
			}
		}
	}
	else
	{
		CurrentStamina = FMath::Clamp(CurrentStamina + StaminaRegenRate * DeltaTime, 0.0f, MaxStamina);
	}

	OnStaminaChanged(CurrentStamina, MaxStamina);
}

float APlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (bIsDead || ActualDamage <= 0.0f)
	{
		return ActualDamage;
	}

	CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.0f, MaxHP);
	OnHealthChanged(CurrentHP, MaxHP);

	UE_LOG(LogTemp, Warning, TEXT("PlayerCharacter took %.1f damage, HP: %.1f / %.1f"), ActualDamage, CurrentHP, MaxHP);

	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	return ActualDamage;
}

void APlayerCharacter::Heal(float HealAmount)
{
	if (bIsDead || HealAmount <= 0.0f) return;

	CurrentHP = FMath::Clamp(CurrentHP + HealAmount, 0.0f, MaxHP);
	OnHealthChanged(CurrentHP, MaxHP);
}

void APlayerCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	UE_LOG(LogTemp, Warning, TEXT("PlayerCharacter died"));

	// 이동/입력 정지
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
		GetCharacterMovement()->StopMovementImmediately();
	}

	if (Controller)
	{
		DisableInput(Cast<APlayerController>(Controller));
	}

	// 캡슐 충돌은 꺼서 다른 액터들이 시체를 통과할 수 있게 함
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 블루프린트에서 사망 애니메이션, 리스폰 UI 등 연출 붙이는 지점
	OnDeath();
}

// E키 입력 -> 상호작용 시도
void APlayerCharacter::OnInteractPressed(const FInputActionValue& value)
{
	TryInteract();
}

// 카메라 전방으로 스피어 트레이스 쏴서 맞은 액터 확인
// (Interactable.h가 아직 없어서, 지금은 트레이스 결과만 로그로 확인)
void APlayerCharacter::TryInteract()
{
	if (!CameraComp) return;

	const FVector Start = CameraComp->GetComponentLocation();
	const FVector End = Start + (CameraComp->GetForwardVector() * InteractDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(InteractRadius),
		Params
	);

	if (!bHit || !Hit.GetActor())
	{
		UE_LOG(LogTemp, Warning, TEXT("Interact: nothing hit"));
		return;
	}

	AActor* HitActor = Hit.GetActor();
	UE_LOG(LogTemp, Warning, TEXT("Interact hit: %s"), *HitActor->GetName());

	// 팀원이 Interactable.h 완성하면 아래 주석 풀고 위 include도 살리기
	// if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	// {
	// 	IInteractable::Execute_Interact(HitActor, this);
	// }
}
