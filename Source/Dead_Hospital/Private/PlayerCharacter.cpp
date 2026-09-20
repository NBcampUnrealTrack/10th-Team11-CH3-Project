#include "PlayerCharacter.h"
#include "PlayerCharacterController.h"
#include "InventoryComponent.h"
#include "DocumentComponent.h"
#include "CombatComponent.h"
#include "PlayerInterface.h"
#include "EnhancedInputComponent.h"
#include "Interactable.h"
#include "DeadHospitalGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"


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

	// 1인칭 손(팔) 메시: 카메라에 부착, 본인에게만 보임, 그림자/충돌 없음
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(CameraComp);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->bCastDynamicShadow = false;
	ArmsMesh->CastShadow = false;
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 인벤토리 컴포넌트 생성
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// 문서 컴포넌트 생성 (문서 전용 별도 칸 - 인벤토리 자리 부족으로 분리)
	DocumentComp = CreateDefaultSubobject<UDocumentComponent>(TEXT("DocumentComponent"));

	// 전투 컴포넌트 생성 (사격/재장전/피격 처리 - 전투 파트)
	CombatComp = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));

	//NormalSpeed = 600.0f;
	//SprintSpeedMultiplier = 1.5f; //3줄다 주석 지우지 마세요!!
	//SprintSpeed = NormalSpeed * SprintSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;

	// 내장 Crouch(앉기) 시스템 사용 설정. 이동 중 캡슐 리사이즈/천장 체크를
	// 엔진이 안전하게 처리해줘서 수동 SetCapsuleHalfHeight 방식보다 안정적이다.
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = SitSpeed;
	GetCharacterMovement()->CrouchedHalfHeight = CrouchedHalfHeight;

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
		UpdateMovementNoise(DeltaTime);
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
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnSitPressed
				);
			}

			/*if (CharacterController->SitAction)
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
			}*/

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

			// I키 (InventoryAction) 바인딩
			if (CharacterController->InventoryAction)
			{
				EnhancedInput->BindAction(
					CharacterController->InventoryAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnInventoryPressed
				);
			}

			// J키 (DocumentAction) 바인딩
			if (CharacterController->DocumentAction)
			{
				EnhancedInput->BindAction(
					CharacterController->DocumentAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnDocumentPressed
				);
			}

			// O키 -> 조합창
			if (CharacterController->CraftAction)
			{
				EnhancedInput->BindAction(
					CharacterController->CraftAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnCraftPressed
				);
			}

			// ESC키 -> 현재 메뉴 UI 닫기
			if (CharacterController->CloseUIAction)
			{
				EnhancedInput->BindAction(
					CharacterController->CloseUIAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnCloseUIPressed
				);
			}

			// P키 - 게임 일시정지 / 해제
			if (CharacterController->PauseAction)
			{
				EnhancedInput->BindAction(
					CharacterController->PauseAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnPausePressed
				);
			}

			// 1/2/3키 (QuickSlot1/2/3Action) 바인딩
			if (CharacterController->QuickSlot1Action)
			{
				EnhancedInput->BindAction(
					CharacterController->QuickSlot1Action,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnQuickSlot1Pressed
				);
			}

			if (CharacterController->QuickSlot2Action)
			{
				EnhancedInput->BindAction(
					CharacterController->QuickSlot2Action,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnQuickSlot2Pressed
				);
			}

			if (CharacterController->QuickSlot3Action)
			{
				EnhancedInput->BindAction(
					CharacterController->QuickSlot3Action,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnQuickSlot3Pressed
				);
			}

			// R키 (ReloadAction) 바인딩
			if (CharacterController->ReloadAction)
			{
				EnhancedInput->BindAction(
					CharacterController->ReloadAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnReloadPressed
				);
			}

			// 마우스 좌클릭 (FireAction) 바인딩
			if (CharacterController->FireAction)
			{
				EnhancedInput->BindAction(
					CharacterController->FireAction,
					ETriggerEvent::Started,
					this,
					&APlayerCharacter::OnFirePressed
				);

				EnhancedInput->BindAction(
					CharacterController->FireAction,
					ETriggerEvent::Completed,
					this,
					&APlayerCharacter::OnFireReleased
				);
			}
		}
	}
}


void APlayerCharacter::Move(const FInputActionValue& value)
{
	// 은신 중이거나 인벤토리 등 UI가 열려 있으면 일반 이동 불가 (은신 장소에서 나오는 것만 E로 허용)
	if (!Controller || bIsHiding || bIsUIOpen) return;

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

// 앉기 키 입력 -> 앉기/일어서기 토글
void APlayerCharacter::OnSitPressed(const FInputActionValue& value)
{
	ToggleSit();
}

void APlayerCharacter::ToggleSit()
{
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked || bIsUIOpen)
	{
		return;
	}

	if (bIsSitting)
	{
		// UnCrouch()는 엔진이 자체적으로 천장/장애물 체크를 해서, 공간이 없으면
		// 알아서 크라우치 상태를 유지한다 (수동 스윕 체크보다 안정적).
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

/*void APlayerCharacter::StartSit(const FInputActionValue& value)
{
	if (value.Get<bool>())
	{
		Sit();
	}
}

void APlayerCharacter::StopSit(const FInputActionValue& value)
{
	StopSitting();
}*/


// 내장 Crouch 시작 완료 시점. bIsSitting 동기화 + 블루프린트 연출 지점 제공용
void APlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	bIsSitting = true;
}

// 내장 Crouch 종료(기상) 완료 시점. bIsSitting 동기화 + 블루프린트 연출 지점 제공용
void APlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	bIsSitting = false;
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	// 인벤토리 등 UI가 열려 있으면 카메라 조작 불가
	if (bIsUIOpen) return;
	
	FVector2D LookInput = value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void APlayerCharacter::StartSprint(const FInputActionValue& value)
{
	// 은신중이거나 사망 상태, 스테미너 최소치보다 적으면 달리기 자체를 못하게 막음
	if (bIsDead || bIsHiding || bIsUIOpen || (bUseStaminaSystem && (bStaminaDepleted || CurrentStamina < MinStaminaToSprint)))
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
			bStaminaDepleted = true;
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

	if (bStaminaDepleted && CurrentStamina >= MaxStamina)
	{
		bStaminaDepleted = false;
	}

	OnStaminaChanged(CurrentStamina, MaxStamina);
}

// 이동 상태(앉기/걷기/뛰기)에 따라 다른 크기/빈도로 소음을 발생시킨다 (AI 청각 감지용).
// 앉아서 이동할 때는 MakeNoise 자체를 호출하지 않아 사실상 소리/범위가 없다.
void APlayerCharacter::UpdateMovementNoise(float DeltaTime)
{
	if (bIsDead || bIsHiding || bIsSitting)
	{
		NoiseTimer = 0.0f;
		return;
	}

	const float Speed = GetVelocity().Size2D();
	if (Speed < 10.0f)
	{
		NoiseTimer = 0.0f;
		return;
	}

	const float Loudness = bIsSprinting ? SprintNoiseLoudness : WalkNoiseLoudness;
	const float Interval = bIsSprinting ? SprintNoiseInterval : WalkNoiseInterval;

	NoiseTimer += DeltaTime;
	if (NoiseTimer >= Interval)
	{
		NoiseTimer = 0.0f;

		USoundBase* FootstepSound = bIsSprinting ? SprintFootstepSound : WalkFootstepSound;
		if (FootstepSound && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
		{
			UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, GetActorLocation(), FootstepVolume);
		}

		// Loudness가 클수록 AI Hearing Sense가 감지하는 범위도 넓어짐 (AISenseConfig_Hearing 설정 기준)
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			GetActorLocation(),
			Loudness,
			this,
			0.0f,      // MaxRange (0이면 AI의 Hearing Sense Config에 설정된 HearingRange를 그대로 사용)
			NAME_None
		);
	}
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
	// 사망 / 은신 중에는 회복 불가
	if (bIsDead || bIsHiding || HealAmount <= 0.0f) return;

	CurrentHP = FMath::Clamp(CurrentHP + HealAmount, 0.0f, MaxHP);
	OnHealthChanged(CurrentHP, MaxHP);
}

void APlayerCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	UE_LOG(LogTemp, Warning, TEXT("PlayerCharacter::Die() called"));

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

	// 은신 중 사망 -> 은신 상태 강제 해제 (연출없이 즉시 정리)
	if (bIsHiding)
	{
		ResetHidingState();
	}

	// 앉기 상태 정리 (내장 Crouch 강제 해제. OnEndCrouch에서 bIsSitting도 같이 정리됨)
	if (bIsCrouched)
	{
		UnCrouch();
	}

	// 연사 중단
	StopAutoFire();

	// 스프린트 상태 정리
	bIsSprinting = false;
	bStaminaDepleted = false;

	// 손전등 강제 소등
	if (bFlashlightOn)
	{
		bFlashlightOn = false;
		OnFlashlightStateChanged(false);
	}

	// 사망 시 열려 있는 인벤토리 / 문서 / 조합창 모두 닫기
	CloseAllMenuUI();

	// 상호작용 연타 방지용 값 정리
	LastInteractActor.Reset();
	LastInteractTime = -1.0f;

	// GameOver 화면에서 마우스로 버튼을 누를 수 있도록 커서 표시 + UI 입력 모드로 전환
	// (CloseAllMenuUI()가 커서를 숨긴 뒤에 켜야 하므로 반드시 그 아래에 둔다)
	if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetController()))
	{
		CharacterController->SetUIInputMode(true);
	}

	// GameMode에 사망 확정 알림 (GameOverReason = PlayerDied로 기록됨)
	if (UWorld* World = GetWorld())
	{
		if (ADeadHospitalGameMode* GameMode = World->GetAuthGameMode<ADeadHospitalGameMode>())
		{
			UE_LOG(LogTemp, Warning, TEXT("Die(): HandlePlayerDeath() 호출"));
			GameMode->HandlePlayerDeath();
		}
		else
		{
			AGameModeBase* CurrentMode = World->GetAuthGameMode();
			UE_LOG(LogTemp, Error, TEXT("Die(): GameMode가 ADeadHospitalGameMode가 아닙니다! (현재: %s)"),
				CurrentMode ? *CurrentMode->GetClass()->GetName() : TEXT("nullptr"));
		}
	}

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
		AActor* Spot = CurrentHidingSpot.Get();

		// HidingSpot 쪽 State도 같이 Idle로 되돌려서, 다음에 다시 정상 사용 가능하게 함
		if (Spot->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
		{
			IPlayerInterface::Execute_ForceRelease(Spot, this);
		}
		Spot->OnDestroyed.RemoveDynamic(this, &APlayerCharacter::HandleHidingSpotDestroyed);
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

// I키 입력 -> 인벤토리 토글
void APlayerCharacter::OnInventoryPressed(const FInputActionValue& value)
{
	ToggleInventory();
}

// J키 입력 -> 문서창 토글
void APlayerCharacter::OnDocumentPressed(const FInputActionValue& value)
{
	ToggleDocument();
}

// O키 입력 -> 조합창 열기 / 닫기
void APlayerCharacter::OnCraftPressed(const FInputActionValue& value)
{
	// 사망, 은신 전환, 강제 입력 잠금 상태에서는 사용하지 않음
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked)
	{
		return;
	}

	ToggleCraft();
}

// ESC키 - 현재 열려 있는 메뉴 UI 닫기
void APlayerCharacter::OnCloseUIPressed(const FInputActionValue& Value){

	// Pause 메뉴가 열려 있다면
	// 게임 일시정지를 해제하고 Pause 메뉴를 닫음
	if (bIsPauseOpen){

		TogglePause();
		return;
	}

	// 일반 메뉴가 아무것도 열려 있지 않으면
	// ESC키로 Pause 메뉴 열기
	if (!bIsInventoryOpen && !bIsDocumentOpen && !bIsCraftOpen){

		TogglePause();
		return;
	}

	// 인벤토리 / 문서 / 조합창이 열려 있다면
	// 현재 메뉴만 닫기
	CloseAllMenuUI();
}

// P키 - 게임 일시정지 / 해제
void APlayerCharacter::OnPausePressed(const FInputActionValue& Value)
{
	// 사망 상태에서는 Pause 메뉴를 열지 않음
	if (bIsDead)
	{
		return;
	}

	TogglePause();
}

// 1/2/3 입력 -> 해당 인덱스의 퀵슬롯 아이템 사용
void APlayerCharacter::OnQuickSlot1Pressed(const FInputActionValue& value)
{
	UseQuickSlot(0);
}

void APlayerCharacter::OnQuickSlot2Pressed(const FInputActionValue& value)
{
	UseQuickSlot(1);
}

void APlayerCharacter::OnQuickSlot3Pressed(const FInputActionValue& value)
{
	UseQuickSlot(2);
}

// R키 -> 전투 파트 CombatComponent의 ReloadWeapon 호출
void APlayerCharacter::OnReloadPressed(const FInputActionValue& value)
{
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked || bIsUIOpen)
	{
		return;
	}

	if (CombatComp)
	{
		CombatComp->ReloadWeapon();
	}
}


// 마우스 좌클릭 -> 전투 파트 CombatComponent의 PrimaryAttack 호출
void APlayerCharacter::OnFirePressed(const FInputActionValue& value)
{
	// 사망 / 은신(연출 포함) / 입력 잠금 / 인벤토리 등 UI Open 상태면 무시
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked || bIsUIOpen)
	{
		return;
	}

	if (CombatComp)
	{
		CombatComp->PrimaryAttack();

		if (bAutoFire)
		{
			StartAutoFire();
		}
	}
}

void APlayerCharacter::OnFireReleased(const FInputActionValue& value)
{
	StopAutoFire();
}

void APlayerCharacter::StartAutoFire()
{
	GetWorldTimerManager().SetTimer(
		AutoFireTimerHandle,
		this,
		&APlayerCharacter::HandleAutoFire,
		AutoFireInterval,
		true
	);
}

// 타이머가 간격마다 호출. 발사 불가 상태(사망/은신/UI 등)가 되면 스스로 중단한다.
void APlayerCharacter::HandleAutoFire()
{
	if (!CombatComp || !CanPerformAction())
	{
		StopAutoFire();
		return;
	}

	CombatComp->PrimaryAttack();
}

void APlayerCharacter::StopAutoFire()
{
	GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
}

void APlayerCharacter::AcquireFlashlight()
{
	bHasFlashlight = true;
}

// UI 열림/닫힘 상태 갱신 + 마우스 커서/Input Mode 전환
void APlayerCharacter::SetUIOpen(bool bNewUIOpen)
{
	bIsUIOpen = bNewUIOpen;

	// UI가 열리면 연사 중단
	if (bNewUIOpen)
	{
		StopAutoFire();
	}

	if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetController()))
	{
		CharacterController->SetUIInputMode(bNewUIOpen);
	}
}

void APlayerCharacter::ToggleFlashlight()
{
	// 손전등 미보유 -> 아무 동작 안 함
	if (!bHasFlashlight)
	{
		return;
	}

	// 사망 / 은신 중 / 입력 잠금(엔딩·컷씬 등) -> F 무시
	if (bIsDead || bIsHiding || bIsInputLocked || bIsUIOpen)
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

// I 입력->인벤토리 토글.실제 인벤토리 위젯 표시 / 숨김은 인벤토리 파트에서
// OnInventoryToggled 이벤트를 받아 처리한다. (Player 쪽은 상태 관리 + 입력 차단만 담당)
void APlayerCharacter::ToggleInventory()
{
	// 사망 / 은신(연출 포함) / 입력 잠금 상태면 무시
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked)
	{
		return;
	}

	// 이미 인벤토리가 열려 있다면 닫기
	if (bIsInventoryOpen)
	{
		CloseInventory();
		return;
	}

	// 문서창이 열려 있다면 닫기
	if (bIsDocumentOpen)
	{
		CloseDocument();
	}

	// 조합창이 열려 있다면 닫기
	if (bIsCraftOpen)
	{
		CloseCraft();
	}

	// 인벤토리 열기
	bIsInventoryOpen = true;

	// 인벤토리 / 문서 / 조합창 중 하나라도 열려 있으면
	// Player는 UI 사용 중 상태를 유지
	SetUIOpen(
		bIsInventoryOpen ||
		bIsDocumentOpen ||
		bIsCraftOpen
	);

	// 실제 인벤토리 위젯 표시
	OnInventoryToggled.Broadcast(true);
}

// 인벤토리 UI 쪽(ESC, X버튼, 아이템 사용 후 자동 닫힘 등)에서 호출.
// I키 Toggle과 달리 "무조건 닫기"만 하는 함수라서, 이미 닫혀 있으면 아무 일도 하지 않는다.
void APlayerCharacter::CloseInventory()
{
	if (!bIsInventoryOpen)
	{
		return;
	}

	bIsInventoryOpen = false;

	// 인벤토리를 닫아도 문서창 또는 조합창이 열려 있다면
	// UI 사용 중 상태는 계속 유지합니다.
	SetUIOpen(bIsInventoryOpen || bIsDocumentOpen ||bIsCraftOpen);

	OnInventoryToggled.Broadcast(false);
}

// J 입력 -> 문서창 토글. 실제 문서 위젯 표시/숨김은 문서 파트에서
// OnDocumentToggled 이벤트를 받아 처리한다. (Player 쪽은 상태 관리 + 입력 차단만 담당)
void APlayerCharacter::ToggleDocument()
{
	// 사망 / 은신(연출포함) / 입력 잠금 상태면 무시
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked)
	{
		return;
	}

	// 이미 문서창이 열려 있으면 닫기
	if (bIsDocumentOpen)
	{
		CloseDocument();
		return;
	}

	// 인벤토리가 열려 있으면 먼저 닫기
	if (bIsInventoryOpen)
	{
		CloseInventory();
	}

	// 조합창이 열려 있으면 먼저 닫기
	if (bIsCraftOpen)
	{
		CloseCraft();
	}

	// 문서창 열기
	bIsDocumentOpen = true;

	// 인벤토리 / 문서 / 조합창 중 하나라도 열려 있으면
	// Player는 UI 사용 중 상태를 유지
	SetUIOpen(
		bIsInventoryOpen ||
		bIsDocumentOpen ||
		bIsCraftOpen
	);

	// 실제 문서 위젯 표시
	OnDocumentToggled(true);
}

// 문서 UI 쪽(ESC, X버튼 등)에서 호출. 이미 닫혀 있으면 아무 동작 안 함.
void APlayerCharacter::CloseDocument()
{
	if (!bIsDocumentOpen)
	{
		return;
	}

	bIsDocumentOpen = false;
	// 문서창을 닫아도 인벤토리 또는 조합창이 열려 있다면
	// UI 사용 중 상태는 계속 유지합니다.
	SetUIOpen(bIsInventoryOpen || bIsDocumentOpen || bIsCraftOpen);
	OnDocumentToggled(false);
}

// O 입력 -> 조합창 토글
// 실제 조합 위젯 표시 / 숨김은
// OnCraftToggled 이벤트를 받아 처리한다.
// Player 쪽은 상태 관리 + 입력 차단만 담당
void APlayerCharacter::ToggleCraft()
{
	// 사망 / 은신(연출포함) / 입력 잠금 상태면 무시
	if (bIsDead || bIsHiding || bIsHideTransitioning || bIsInputLocked)
	{
		return;
	}

	// 이미 조합창이 열려 있으면 닫기
	if (bIsCraftOpen)
	{
		CloseCraft();
		return;
	}

	// 인벤토리가 열려 있으면 먼저 닫기
	if (bIsInventoryOpen)
	{
		CloseInventory();
	}

	// 문서창이 열려 있으면 먼저 닫기
	if (bIsDocumentOpen)
	{
		CloseDocument();
	}

	// 조합창 열기
	bIsCraftOpen = true;

	// 인벤토리 / 문서 / 조합창 중 하나라도 열려 있으면
	// Player는 UI 사용 중 상태를 유지
	SetUIOpen(
		bIsInventoryOpen ||
		bIsDocumentOpen ||
		bIsCraftOpen
	);

	// 실제 조합 위젯 표시
	OnCraftToggled(true);
}

// 조합 UI 쪽(ESC, X버튼 등)에서 호출.
// 이미 닫혀 있으면 아무 동작 안 함.
void APlayerCharacter::CloseCraft()
{
	// 이미 조합창이 닫혀 있으면 아무것도 하지 않음
	if (!bIsCraftOpen)
	{
		return;
	}

	// 조합창 닫기
	bIsCraftOpen = false;

	// 다른 메뉴가 열려 있다면 UI 사용 중 상태 유지
	SetUIOpen(
		bIsInventoryOpen ||
		bIsDocumentOpen ||
		bIsCraftOpen
	);

	// 실제 조합 위젯을 닫으라고 Blueprint에 전달
	OnCraftToggled(false);
}

// ESC 입력 -> 현재 열려 있는 메뉴 UI를 모두 닫음
void APlayerCharacter::CloseAllMenuUI()
{
	// 인벤토리 닫기
	CloseInventory();

	// 문서창 닫기
	CloseDocument();

	// 조합창 닫기
	CloseCraft();

	// 모든 메뉴가 닫혔으므로
	// Player의 UI 사용 중 상태도 해제
	SetUIOpen(false);
}

// 게임 일시정지 / 해제
void APlayerCharacter::TogglePause()
{
	// 현재 Pause 메뉴가 닫혀 있다면
	if (!bIsPauseOpen)
	{
		// 인벤토리 / 문서 / 조합창이 열려 있다면 먼저 닫기
		CloseAllMenuUI();

		// Pause 메뉴 열기
		bIsPauseOpen = true;

		// UI 사용 중 상태로 변경
		SetUIOpen(true);

		// Blueprint에 Pause UI를 열라고 알림
		OnPauseToggled(true);

		// 게임 일시정지
		UGameplayStatics::SetGamePaused(GetWorld(), true);

		return;
	}

	// 이미 Pause 상태라면 게임 재개
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	// Pause 메뉴 닫기
	bIsPauseOpen = false;

	// UI 사용 상태 해제
	SetUIOpen(false);

	// Blueprint에 Pause UI를 닫으라고 알림
	OnPauseToggled(false);
}

// 1/2/3 입력 -> 해당 인덱스의 퀵슬롯 아이템 사용
// TODO: InventoryComponent에 실제로 있는 함수 이름/시그니처에 맞게 아래 내부 구현을 교체해야 한다.
void APlayerCharacter::UseQuickSlot(int32 SlotIndex)
{
	if (!CanPerformAction() || bIsUIOpen)
	{
		return;
	}

	if (InventoryComp)
	{
		InventoryComp->UseQuickSlot(SlotIndex);
	}
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
	if (!IsValid(HitActor))
	{
		return nullptr;
	}

	const bool bImplementsPlayerInterface = HitActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass());
	const bool bImplementsInteractable = HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass());

	if (!bImplementsPlayerInterface && !bImplementsInteractable)
	{
		return nullptr;
	}

	if (Hit.Distance > InteractDistance)
	{
		return nullptr;
	}

	if (bIsHiding)
	{
		if (bImplementsPlayerInterface)
		{
			if (!IPlayerInterface::Execute_IsAllowedWhileHiding(HitActor))
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}

	if (bImplementsPlayerInterface)
	{
		if (!IPlayerInterface::Execute_CanInteract(HitActor, const_cast<APlayerCharacter*>(this)))
		{
			return nullptr;
		}
	}
	else if (bImplementsInteractable)
	{
		if (!IInteractable::Execute_CanInteract(HitActor, const_cast<APlayerCharacter*>(this)))
		{
			return nullptr;
		}
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
	FText PromptText;
	if (NewTarget->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		PromptText = IPlayerInterface::Execute_GetInteractionText(NewTarget, const_cast<APlayerCharacter*>(this));
	}
	else if (NewTarget->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		PromptText = IInteractable::Execute_GetInteractionText(NewTarget);
	}
	
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

	const bool bImplementsPlayerInterface = TargetActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass());
	const bool bImplementsInteractable = TargetActor->GetClass()->ImplementsInterface(UInteractable::StaticClass());

	if (bImplementsPlayerInterface)
	{
		if (!IPlayerInterface::Execute_CanInteract(TargetActor, this))
		{
			return;
		}
	}
	else if (bImplementsInteractable)
	{
		if (!IInteractable::Execute_CanInteract(TargetActor, this))
		{
			return;
		}
	}
	else
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (LastInteractActor.Get() == TargetActor && (Now - LastInteractTime) < InteractCooldown)
	{
		return;
	}

	LastInteractActor = TargetActor;
	LastInteractTime = Now;

	if (bImplementsPlayerInterface)
	{
		IPlayerInterface::Execute_Interact(TargetActor, this);
	}
	else
	{
		IInteractable::Execute_Interact(TargetActor, this);
	}
}
