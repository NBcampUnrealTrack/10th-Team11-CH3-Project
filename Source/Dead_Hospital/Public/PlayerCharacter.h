#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInventoryComponent;
class UDocumentComponent;
class UCombatComponent;
struct FInputActionValue;

UCLASS()
class DEAD_HOSPITAL_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	APlayerCharacter();

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Hiding") 
	bool IsHiding() const { return bIsHiding; }

	// 진입/퇴장 연출(문 열기, 이동, 카메라 전환 등) 도중인지 이 동안은 E 연타, 다른 상호작용,
	// 공격/아이템 등 모든 행동이 막혀야 한다 HidingSpotActor가 연출 시작/종료 시점에 호출해준다.
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	bool IsHideTransitioning() const { return bIsHideTransitioning; }

	UFUNCTION(BlueprintCallable, Category = "Hiding")
	void SetHideTransitioning(bool bNewTransitioning) { bIsHideTransitioning = bNewTransitioning; }

	// HidingSpot 인자를 넘기면 지금 어떤 은신처에 들어갔는지 CurrentHidingSpot에 기록된다.
	// 나올 때는 HidingSpot 없이 false만 넘기면 자동으로 비워진다.
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	void SetHiding(bool bNewHiding, AActor * HidingSpot = nullptr);

	// 지금 어떤 HidingSpot(Cabinet/Desk 등)에 들어가 있는지 반환. 숨어 있지 않으면 nullptr.
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	AActor* GetCurrentHidingSpot() const { return CurrentHidingSpot.Get(); }

	// 손전등 보유 여부 확인 (블루프린트/아이템 시스템에서 조회용)
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	bool HasFlashlight() const { return bHasFlashlight; }

	// 손전등 켜져 있는지 확인
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	// 손전등 ItemPickup에서 E로 획득 시 호출. 왼손 장착은 블루프린트/애님에서 처리(메시 생략).
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void AcquireFlashlight();

	// 인벤토리 창이 열려 있는지 확인 (UI/다른 시스템에서 조회용)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsInventoryOpen() const { return bIsInventoryOpen; }

	// 인벤토리 UI 쪽(ESC, X버튼, 아이템 사용 후 자동 닫힘 등)에서 호출.
	// 이미 닫혀 있으면 아무 동작 안 함. 상태를 false로 바꾸고 OnInventoryToggled(false)를 발생시킨다.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	// 문서창이 열려 있는지 확인 (UI/다른 시스템에서 조회용)
	UFUNCTION(BlueprintCallable, Category = "Document")
	bool IsDocumentOpen() const { return bIsDocumentOpen; }

	// 문서 UI 쪽(ESC, X버튼 등)에서 호출. 이미 닫혀 있으면 아무 동작 안 함.
	// 상태를 false로 바꾸고 OnDocumentToggled(false)를 발생시킨다.
	UFUNCTION(BlueprintCallable, Category = "Document")
	void CloseDocument();

	// 조합창이 열려 있는지 확인
	UFUNCTION(BlueprintCallable, Category = "Craft")
	bool IsCraftOpen() const { return bIsCraftOpen; }

	// 조합 UI 쪽에서 ESC, X버튼 등으로 닫을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Craft")
	void CloseCraft();

	// 인벤토리 / 문서 / 조합창을 모두 닫습니다.
	// ESC 입력에서 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseAllMenuUI();

	// Ending, 컷씬, 강제 연출 등 F 입력 자체를 막아야 할 때 외부에서 호출
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetInputLocked(bool bNewLocked) { bIsInputLocked = bNewLocked; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInputLocked() const { return bIsInputLocked; }

	// 문서/키패드/Pause 등 다른 IU가 열려 있는 동안 일반 상호작용 프롬프트를 숨기기 위한 스위치
	// 해당 UI 위젯을 열고 닫을 떄 블루프린트에서 호출해준다.
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void SetUIOpen(bool bNewUIOpen) { bIsUIOpen = bNewUIOpen; }

	// 은신 중이거나 사망 상태면 false. 공격/아이템 사용 등 다른 액션 시스템에서
	// 공격/아이템 사용 등 다른 핵션 시스템에서
	// 함수 맨 앞에 "if (!CanPerformAction()) return;" 형태로 가져다 쓰면 된다
	UFUNCTION(BlueprintCallable, Category = "Action")
	bool CanPerformAction() const
	{
		return !bIsDead
			&& !bIsHiding
			&& !bIsHideTransitioning
			&& !bIsInputLocked
			&& !bIsUIOpen;
	}

	UFUNCTION(BlueprintCallable, Category = "Camera")
	float GetEyeHeight() const { return EyeHeight; }

	// Desk 은신 시 카메라를 낮추거나, 나올 때 원래 높이로 복구하는 용도
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetEyeHeight(float NewEyeHeight);

	// GameOver 등 외부 시스템에서 은신 상태를 강제로 초기화할 때 호출.
	// 현재 HidingSpot에도 ForceRelease를 걸어서, 그쪽 State도 같이 Idle로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	void ResetHidingState();

	// 힐 아이템 호출하는 함수
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float HealAmount);

	// 언리얼 기본 데미지 파이프라인 오버라이드
	virtual float TakeDamage(
		float DamageAmount, 
		struct FDamageEvent const& DamageEvent, 
		class AController* EventInstigator, 
		AActor* DamageCauser
	) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "camera")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "camera")
	UCameraComponent* CameraComp;

	// Inventory
	UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "Inventory")
	UInventoryComponent* InventoryComp;

	// Document ( 문서 전용 별도 칸 - 인벤토리 자리 부족으로 분리)
	UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "Document")
	UDocumentComponent* DocumentComp;

	// Combat 사격/재장전/피격 처리 등은 이 컴포넌트 내부에서 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "Combat")
	UCombatComponent* CombatComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EyeHeight = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float NormalSpeed = 240.0f; //걷기
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	//float SprintSpeedMultiplier; //주석뺴지 마세요!!
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 400.0f; //뛰기

	// 연타 방지용
	TWeakObjectPtr<AActor> LastInteractActor;
	float LastInteractTime = -1.0f;

	UPROPERTY(EditAnyWhere, BlueprintReadOnly, Category = "Interact")
	float InteractCooldown = 0.3f;

	// 인벤토리 탐색 거리/ 반경
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	float InteractDistance = 200.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	float InteractRadius = 40.0f;

	/*지금 프롬프트로 표시 중인 대상.E를 눌렀을 떄도 이 값을 그대로 사용해서
	  "프롬프트에 뜬 대상 == 실제 상호작용 대상"이 항상 보장되게 한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	TWeakObjectPtr<AActor> CurrentInteractableActor;

	// 문서/키패드/Pause UI가 열려 있는 동안 true. 이 동안은 일반 상호작용 프롬프트를 숨긴다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	bool bIsUIOpen = false;

	// HP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHP = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsDead = false;

	// 은신
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding")
	bool bIsHiding = false;

	// 손전등 온/오프, 은신 중에는 켤 수 없고, 은신 진입 시 자동으로 꺼진다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	bool bFlashlightOn = false;

	// 손전등을 획득했는지 여부. false면 F를 눌러도 아무 반응 없음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	bool bHasFlashlight = false;

	// F 연타로 ON/OFF가 순간적으로 꼬이는 것을 막기 위한 쿨다운
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flashlight")
	float FlashlightToggleCooldown = 0.2f;

	float LastFlashlightToggleTime = -1.0f;

	// I키로 인벤토리 창이 열려 있는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	bool bIsInventoryOpen = false;

	// J키로 문서창이 연리 있는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	bool bIsDocumentOpen = false;

	// O키로 조합창이 열려 있는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Craft")
	bool bIsCraftOpen = false;

	// P키 일시정지 메뉴가 열려 있는지 확인
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsPauseOpen = false;

	// Ending, 컷씬 등 외부 연출이 강제로 모든 입력을 막을 때 true
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	bool bIsInputLocked = false;

	// 은신 진입/퇴장 연출 도중인지( E 연타, 다른 상호작용, 공격/ 아이템 차단용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding")
	bool bIsHideTransitioning = false;

	// 지금 들어가 있는 HidingSpot(Cabinet, Desk 등).숨어 있지 않을 때는 비어 있음.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding")
	TWeakObjectPtr<AActor> CurrentHidingSpot;

	// 스테미너
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float CurrentStamina;

	// 스테미너 - 팀 확정 전까지 꺼두는 스위치 (코드는 삭제하지 않고 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
	bool bUseStaminaSystem = false;

	// 초당 소모/회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaDrainRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaRegenRate = 10.0f;

	// 스테미너가 이 값 이상이어야 뛰기 시작 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MinStaminaToSprint = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	bool bStaminaDepleted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	bool bIsSprinting = false;

	// 소음(AI 청각 감지용) - 앉기는 소리 / 범위 없음, 걷기 / 뛰기는 크기와 간격을 다르게 적용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float WalkNoiseLoudness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float SprintNoiseLoudness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float WalkNoiseInterval = 0.5f; // 발소리 간격(초)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float SprintNoiseInterval = 0.3f;

	float NoiseTimer = 0.0f;

	// Jump 기능 미사용 (이번 게임에서 사용 안 함)
	virtual bool CanJumpInternal_Implementation() const override { return false;  }
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 내장 Crouch 시스템 훅. 앉기/서기 완료 시점에 상태 동기화(bIsSitting)와
	// 블루프린트 연출 붙일 지점을 제공한다. 이동 중 캡슐 리사이즈/천장 체크를
	// 엔진이 안전하게 처리해주므로 수동 SetCapsuleHalfHeight 방식보다 안정적이다.
	// ACharacter의 평범한 virtual 함수라 _Implementation 없이 그대로 오버라이드한다.
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;


	UFUNCTION()
	void Move(const FInputActionValue& value);
	// 앉기 키 입력 핸들러 (누를 때마다 앉기/일어서기 토글)
	UFUNCTION()
	void OnSitPressed(const FInputActionValue& value);
	/*UFUNCTION()
	void StartSit(const FInputActionValue& value);
	UFUNCTION()
	void StopSit(const FInputActionValue& value);*/
	UFUNCTION()
	void Look(const FInputActionValue& value);
	UFUNCTION()
	void StartSprint(const FInputActionValue& value);
	UFUNCTION()
	void StopSprint(const FInputActionValue& value);

	// E키 입력 핸들러
	UFUNCTION()
	void OnInteractPressed(const FInputActionValue& value);

	// F키 입력 핸들러
	UFUNCTION()
	void OnFlashlightPressed(const FInputActionValue& value);

	// I키 입력 핸들러
	UFUNCTION()
	void OnInventoryPressed(const FInputActionValue& value);

	// J키 입력 핸들러 (문서창 토글)
	UFUNCTION()
	void OnDocumentPressed(const FInputActionValue& value);

	// O키 입력 핸들러 (조합창 토글)
	UFUNCTION()
	void OnCraftPressed(const FInputActionValue& value);

	// ESC키 입력 핸들러 (현재 열려 있는 메뉴 UI 닫기)
	UFUNCTION()
	void OnCloseUIPressed(const FInputActionValue& value);

	// P키 - 게임 일시정지 / 해제
	void OnPausePressed(const FInputActionValue& Value);

	// 1/2/3 퀵슬롯 입력 핸들러 
	UFUNCTION()
	void OnQuickSlot1Pressed(const FInputActionValue& value);
	UFUNCTION()
	void OnQuickSlot2Pressed(const FInputActionValue& value);
	UFUNCTION()
	void OnQuickSlot3Pressed(const FInputActionValue& value);

	// R키 입력 핸들러 -> 전투 파트 CombatComponent의 ReloadWeapon 호출
	UFUNCTION()
	void OnReloadPressed(const FInputActionValue& value);

	// 마우스 우클릭 입력 핸들러 -> 전투 파트 CombatComponent의 PrimaryAttack 호출
	UFUNCTION()
	void OnFirePressed(const FInputActionValue& value);

	// Sit 키 입력 -> 앉기/일어서기 토글. 사망/은신/입력잠금 상태면 무시된다
	void ToggleSit();

	//void Sit();
	//void StopSitting();

	// 근처 아이템 줍기 탐색 + Interact 호출
	void TryInteract();

	// F 입력 -> 손전등 토글 은신 중/사망 상태면 무시된다
	void ToggleFlashlight();

	// I 입력 -> 인벤토리 토글. 실제 UI 표시는 인벤토리 파트에서 OnInventoryToggled를 받아 처리한다
	void ToggleInventory();

	// J 입력 -> 문서창 토글. 실제 UI 표시는 문서 파트에서 OnDocumentToggled를 받아 처리한다
	void ToggleDocument();

	// O 입력 -> 조합창 토글
	void ToggleCraft();

	// 게임 일시정지 / 해제
	void TogglePause();

	// 1/2/3 입력 -> 해당 인덱스의 퀵슬롯 아이템 사용
	void UseQuickSlot(int32 SlotIndex);

	// 지금 은신 중인 HidingSpot Actor가 Destroy될 때 호출됨 (SetHiding에서 구독)
	UFUNCTION()
	void HandleHidingSpotDestroyed(AActor* DestroyedActor);

	// 카메라 전방 스피어 트레이스로 상호작용 가능한 대상을 찾는다.
	// TryInteract()와 UpdateInteractionPrompt()가 공유하는 단일 진입점.
	AActor* FindInteractableTarget() const;

	// 매 프레임 바라보는 대상을 갱신하고, 변화가 있으면 UI 이벤트를 쏴준다.
	void UpdateInteractionPrompt();

	//  매 프레임 스테미너 소모/ 회복 처리
	void UpdateStamina(float DeltaTime);

	// 매 프레임 이동 상태(앉기/걷기/뛰기)에 따른 소음 처리 (AI 청각 감지용)
	void UpdateMovementNoise(float DeltaTime);

	// 사망 처리 (HP 0 이하일 때 1회 호출)
	void Die();

	// 블루프린트에서 사망 연출(애니메이션, UI 등) 붙일 수 있게 이벤트로 노출
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDeath();

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHP, float InMaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stamina")
	void OnStaminaChanged(float NewStamina, float InMaxStamina);

	// 상호작용 프롬프트 위젯 표시/숨김 (블루프린트에서 UI에 연결)
	UFUNCTION(BlueprintImplementableEvent, Category = "Interact")
	void ShowInteractionPrompt(const FText& InteractionText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interact")
	void HideInteractionPrompt();

	UFUNCTION(BlueprintImplementableEvent, Category = "Flashlight")
	void OnFlashlightStateChanged(bool bNewOn);

	// 인벤토리 열림/닫힘 상태가 바뀔 때 호출됨. 인벤토리/UI 파트에서 이 이벤트를 받아
	// 실제 인벤토리 위젯을 열고 닫으면 된다. (Player 쪽은 상태 관리와 입력 차단만 담당)
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnInventoryToggled(bool bNewOpen);

	// 문서창 열림/닫힘 상태가 바뀔 떄 호출됨. 문서 파트에서 이 이벤트를 받아
	// 실제 문서 위젯을 열고 닫으면 된다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Document")
	void OnDocumentToggled(bool bNewOpen);

	// 조합창 열림/닫힘 상태가 바뀔 때 호출됩니다.
	// UI Blueprint에서 이 이벤트를 받아 Craft 위젯을 열고 닫습니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Craft")
	void OnCraftToggled(bool bNewOpen);

	// Pause UI 열기 / 닫기를 Blueprint에 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnPauseToggled(bool bIsOpen);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsSitting = false;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SitSpeed = 150.0f;

	// 앉기 시 Capsule 반높이
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchedHalfHeight = 30.0f;

	// BeginPlay에서 캐싱되는 기본(서 있을 때) Capsule 반높이
	//float DefaultCapsuleHalfHeight = 0.0f;

	//float DefaultMaxSpeed;
};
