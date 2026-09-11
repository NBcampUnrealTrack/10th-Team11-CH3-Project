#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
//class UInventoryComponent;
struct FInputActionValue;

UCLASS()
class DEAD_HOSPITAL_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	APlayerCharacter();

	UFUNCTION(CPF_BlueprintCallable, Category = "Health")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(CPF_BlueprintCallable, Category = "Health")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(CPF_BlueprintCallable, Category = "Stamina")
	float GetCurrentStamina() const { return GetCurrentStamina; }

	UFUNCTION(CPF_BlueprintCallable, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(CPF_BlueprintCallable, Category = "Health")
	bool IsDead() const { return bIsDead; }

	// 힐 아이템 호출하는 함수
	UFUNCTION(CPF_BlueprintCallable, Category = "Health")
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
	//UPROPERTY(VisibleAnywhere, BlueprintReadonly, category = "Inventory")
	//UInventoryComponent* InventoryComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EyeHeight = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float NormalSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeedMultiplier; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed;

	// 인벤토리 탐색 거리/ 반경
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	float InteractDistance = 200.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	float InteractRadius = 40.0f;
	
	// HP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHP = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsDead = false;

	// 스테미너
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float CurrentStamina;

	// 초당 소모/회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaDrainRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StamimaRegenRate = 10.0f;

	// 스테미너가 이 값 이상이어야 뛰기 시작 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MinStaminaToSprint = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	bool bIsSprinting = false;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;


	UFUNCTION()
	void Move(const FInputActionValue& value);
	UFUNCTION()
	void StartSit(const FInputActionValue& value);
	UFUNCTION()
	void StopSit(const FInputActionValue& value);
	UFUNCTION()
	void Look(const FInputActionValue& value);
	UFUNCTION()
	void StartSprint(const FInputActionValue& value);
	UFUNCTION()
	void StopSprint(const FInputActionValue& value);

	// E키 입력 핸들러
	UFUNCTION()
	void OnInteractPressed(const FInputActionValue& value);


	void Sit();
	void StopSitting();

	// 근처 아이템 줍기 탐색 + Interact 호출
	void TryInteract();

	//  매 프레임 스테미너 소모/ 회복 처리
	void UpdateStamina(float DeltaTime);

	// 사망 처리 (HP 0 이하일 때 1회 호출)
	void Die();

	// 블루프린트에서 사망 연출(애니메이션, UI 등) 붙일 수 있게 이벤트로 노출
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDeath();

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHP, float InMaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stamina")
	void OnStaminaChanged(float NewStamina, float InMaxStamina);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsSitting = false;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SitSpeed = 300.0f;

	float DefaultMaxSpeed;
};
