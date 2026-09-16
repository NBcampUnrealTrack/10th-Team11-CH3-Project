#include "PlayerCharacter.h"
#include "PlayerCharacterController.h"
//#include "InventoryComponent.h"
#include "PlayerInterface.h"
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

	//NormalSpeed = 600.0f;
	//SprintSpeedMultiplier = 1.5f;
	//SprintSpeed = NormalSpeed * SprintSpeedMultiplier;

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

	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsDead)
	{
		UpdateStamina(DeltaTime);
	}

	UpdateInteractionPrompt();
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

			// F키 (FlashlightAction)바인딩
			if (CharacterController->FlashlightAction)
			{
				EnhancedInput->BindAction(
						CharacterController->FlashlightAction,
						ETriggerEvent::Started,
						this,
						&APlayerCharacter::OnFlashlightPressed
				);
			}
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& value)
{
	// 은신 중에는 일반 이동 불가 (은신 장소에서 나오는 것만 E로 허용)
	if (!Controller || bIsHiding) return;

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

	GetCapsuleComponent()->SetCapsuleHalfHeight(CrouchedHalfHeight);
}

void APlayerCharacter::StopSitting()
{
	if (!bIsSitting) return;

	// 머리 위가 막혀 있으면 일어서기 불가 -> 앉은 상태 유지
	const float HeightDiff = DefaultCapsuleHalfHeight - CrouchedHalfHeight;
	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(0.f, 0.f, HeightDiff);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bBlocked = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		GetCapsuleComponent()->GetCollisionShape(),
		Params
	);

	if (bBlocked)
	{
		// 아직 일어설 공간이 없음. 앉은 상태 유지 (Ctrl/C를 다시 떼는 시점에 재시도됨)
		return;
	}
	

	bIsSitting = false;
	GetCharacterMovement()->MaxWalkSpeed = DefaultMaxSpeed;
	GetCapsuleComponent()->SetCapsuleHalfHeight(DefaultCapsuleHalfHeight);
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	FVector2D LookInput = value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void APlayerCharacter::StartSprint(const FInputActionValue& value)
{
	// 은신중이거나 사망 상태, 스테미너 최소치보다 적으면 달리기 자체를 못하게 막음
	if (bIsDead || bIsHiding || (bUseStaminaSystem && CurrentStamina < MinStaminaToSprint))
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

// 스테미너 (bUseStaminaSystem이 false면 완전히 비활성화. 코드는 삭제하지 않고 유지)
void APlayerCharacter::UpdateStamina(float DeltaTime)
{
	if (!bUseStaminaSystem)
	{
		return;
	}

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
	// 사망 / 음신 중에는 회복 불가
	if (bIsDead || bIsHiding || HealAmount <= 0.0f) return;

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

	// 사망 시 프롬프트도 확실히 정리
	CurrentInteractableActor.Reset();
	HideInteractionPrompt();
	
	// 은신 중 사망 -> 은신 상태 강제 해제 (연출없이 즉이 정리)
	if (bIsHiding)
	{
		ResetHidingState();
	}

	// 앉기 상태 정리
	bIsSitting = false;

	// 스프린트 상태 정리
	bIsSprinting = false;

	// 손전등 강제 소등
	if (bFlashlightOn)
	{
		bFlashlightOn = false;
		OnFlashlightStateChanged(false);
	}

	// 상호작용 연타 방지용 값 정리
	LastInteractActor.Reset();
	LastInteractTime = -1.0f;

	// 블루프린트에서 사망 애니메이션, 리스폰 UI 등 연출 붙이는 지점
	OnDeath();
}

void APlayerCharacter::SetEyeHeight(float NewEyeHeight)
{
	EyeHeight = NewEyeHeight;

	if (SpringArmComp)
	{
		SpringArmComp->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	}
}

void APlayerCharacter::SetHiding(bool bNewHiding, AActor* HidingSpot)
{
	// 이전에 구독해둔 HidingSpot의 Destroy 이벤트가 있으면 해제 (누수/중복 방지)
	if (CurrentHidingSpot.IsValid())
	{
		CurrentHidingSpot->OnDestroyed.RemoveDynamic(this, &APlayerCharacter::HandleHidingSpotDestroyed);
	}

	bIsHiding = bNewHiding;
	CurrentHidingSpot = bNewHiding ? HidingSpot : nullptr;

	// 은신 진입->손전등 자동 오프
	if (bNewHiding && bFlashlightOn)
	{
		bFlashlightOn = false;
		OnFlashlightStateChanged(false);
	}

	// 은신 장소 Actor가 사라지는 경우를 대비해 Destroy 이벤트를 구독해둔다.
	// (레벨 스트리밍 등으로 EndPlay만 발생하는 경우까지 잡고 싶다면 HidingSpotActor 쪽에서
	// EndPlay를 오버라이드해 별도로 알려주는 방식을 추가해야 하지만, 기본적인 Destroy()는 이걸로 커버된다)
	
	if (bNewHiding && HidingSpot)
	{
		HidingSpot->OnDestroyed.AddDynamic(this, &APlayerCharacter::HandleHidingSpotDestroyed);
	}
}

void APlayerCharacter::HandleHidingSpotDestroyed(AActor* DestroyedActor)
{
	// 은신 장소 Actor가 사라짐->Player가 계속 은신 상태로 남지 않도록 강제 해제
	bIsHiding = false;
	CurrentHidingSpot.Reset();
}

void APlayerCharacter::ResetHidingState()
{
	if (CurrentHidingSpot.IsValid())
	{
		// HidingSpot 쪽 State도 같이 Idle로 되돌려서, 다음에 다른 플레이어/재시작 후에도 정상 사용 가능하게 함
		IPlayerInterface::Execute_ForceRelease(CurrentHidingSpot.Get(), this);
		CurrentHidingSpot->OnDestroyed.RemoveDynamic(this, &APlayerCharacter::HandleHidingSpotDestroyed);
	}

	bIsHiding = false;
	CurrentHidingSpot.Reset();
}

// E키 입력 -> 상호작용 시도
void APlayerCharacter::OnInteractPressed(const FInputActionValue& value)
{
	TryInteract();
}

// F키 입력 ->손전등 토글
void APlayerCharacter::OnFlashlightPressed(const FInputActionValue& value)
{
	ToggleFlashlight();
}

void APlayerCharacter::AcquireFlashlight()
{
	bHasFlashlight = true;
}

void APlayerCharacter::ToggleFlashlight()
{
	// 손전등 미보유 -> 아무 동작 안 함
	if (!bHasFlashlight)
	{
		return;
	}

	// 사망 / 은신 중 / 입력 잠금(엔딩·컷씬 등) -> F 무시
	if (bIsDead || bIsHiding || bIsInputLocked)
	{
		return;
	}

	// F 연타 방지: 쿨다운 안 지났으면 무시
	const float Now = GetWorld()->GetTimeSeconds();
	if ((Now - LastFlashlightToggleTime) < FlashlightToggleCooldown)
	{
		return;
	}
	LastFlashlightToggleTime = Now;

	bFlashlightOn = !bFlashlightOn;
	OnFlashlightStateChanged(bFlashlightOn);
}


// 카메라 전방으로 스피어 트레이스 쏴서 맞은 액터 확인
// TryInteract()와 UpdateInteractionPrompt()가 동일하게 사용하는 단일 트레이스 진입점.
AActor* APlayerCharacter::FindInteractableTarget() const
{
	if (!CameraComp || !GetWorld())
	{
		return nullptr;
	}

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
		return nullptr;
	}

	AActor* HitActor = Hit.GetActor();

	// 상호작용 불가능한 Actor
	if (!IsValid(HitActor) || !HitActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return nullptr;
	}

	// 너무 멀리 있음
	if (Hit.Distance > InteractDistance)
	{
		return nullptr;
	}

	// 은신 중 -> 허용된 상호작용만 실행
	if (bIsHiding && !IPlayerInterface::Execute_IsAllowedWhileHiding(HitActor))
	{
		return nullptr;
	}

	// 이미 사용된 Actor / 지금 상호작용 불가능한 상태
	if (!IPlayerInterface::Execute_CanInteract(HitActor, const_cast<APlayerCharacter*>(this)))
	{
		return nullptr;
	}

	return HitActor;
}

// 매 프레임 호출: 지금 바라보는 대상을 갱신하고 프롬프트 UI에 반영한다.
void APlayerCharacter::UpdateInteractionPrompt()
{
	// Player 사망/ 문서, 키패드, Pause 등 다른 UI 사용 중 -> 일반 프롬프트 숨김
	if (bIsDead || bIsUIOpen)
	{
		if (CurrentInteractableActor.IsValid())
		{
			CurrentInteractableActor.Reset();
			HideInteractionPrompt();
		}
		return;
	}

	AActor* NewTarget = FindInteractableTarget();

	// 아무것도 안 바라봄 / 거리 벗어남 / Actor Destroy -> 프롬프트 숨김
	if (!NewTarget)
	{
		if (CurrentInteractableActor.IsValid())
		{
			CurrentInteractableActor.Reset();
			HideInteractionPrompt();
		}
		return;
	}

	// 다른 Actor를 바라봄, 혹은 같은 Actor라도 상태(잠김->열림 등)가 바뀌었을 수 있으므로
	// 매 프레임 텍스트를 다시 받아와서 UI에 갱신해준다.
	CurrentInteractableActor = NewTarget;
	
	const FText PromptText = IPlayerInterface::Execute_GetInteractionText(NewTarget, const_cast<APlayerCharacter*>(this));
	ShowInteractionPrompt(PromptText);
}

// E키로 실제 상호작용 실행. 새로 트레이스하지 않고 UpdateInteractionPrompt가 채워둔
// CurrentInteractableActor를 그대로 사용해서, 프롬프트에 표시된 대상과 항상 일치시킨다.
void APlayerCharacter::TryInteract()
{
	if (bIsDead || bIsUIOpen || bIsHideTransitioning)
	{
		return;
	}

	AActor* TargetActor = CurrentInteractableActor.Get();
	if (!TargetActor)
	{
		return;
	}

	if (!IPlayerInterface::Execute_CanInteract(TargetActor, this))
	{
		return;
	}

	// E 연타 방지
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastInteractActor.Get() == TargetActor && (Now - LastInteractTime) < InteractCooldown)
	{
		return;
	}

	LastInteractActor = TargetActor;
	LastInteractTime = Now;

	IPlayerInterface::Execute_Interact(TargetActor, this);
}
