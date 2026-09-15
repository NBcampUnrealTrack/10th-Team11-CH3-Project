#pragma once

#include "ItemData.h"
#include "ZombieDropTable.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "ZombieCharacter.generated.h"

UENUM(BlueprintType)
enum class EZombieState : uint8
{
	Idle,
	Patrol,
	Chase,
	Search,
	Attack,
	Dead
};

class USphereComponent;

UCLASS()
class DEAD_HOSPITAL_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	bool bSearchTurnMirrored = false;
	// 이 좀비가 죽은 척 대기 중인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
	bool bIsFakeDead = false;

protected:
	// 컴포넌트 및 타이머
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
	USphereComponent* AttackRangeComp;

	FTimerHandle DisappearTimerHandle;

	//캐릭터 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float Health;//현재체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;//최대체력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FString ZombieName;//이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Power;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackRange;//공격 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float PatrolSpeed;//순찰(걷는) 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float ChaseSpeed;//추적 속도

	//상태 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ACharacter* ChaseTarget;//추적타겟
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	EZombieState CurrentState;//좀비 현재 상태(Idle, Patrol, Chase, Attack,	Dead)

	//애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* AttackMontage;//애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	UAnimMontage* SearchTurnMontage;
	// 일어날 때 재생할 애니메이션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
	class UAnimMontage* GetUpMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	UDataTable* ZombieDropTable;
	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	UDataTable* ItemDataTable;
	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	TSubclassOf<class AItemPickup> ItemPickupClass;

public:
	AZombieCharacter();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE FString GetZombieName() const { return ZombieName; }

	void SetHealth(float NewHealth);
	void SetCurrentState(EZombieState NewState);
	FORCEINLINE EZombieState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Attack")
	void Attack();

	void Die();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnAttackAnimationFinished();

	void PlayAttackMontage();
	void PlaySearchTurnMontage();
	void ToggleSearchTurnDirection();

	UFUNCTION(BlueprintCallable, Category = "Search")
	bool IsPlayingSearchTurn() const;

	// 트리거가 밟히면 호출
	UFUNCTION(BlueprintCallable, Category = "JumpScare")
	void WakeUp();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnAttackOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnAttackEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void OnDeathTimerExpired();
	void HandleItemDrop();

	//델리게이트 바인딩용 함수 - UFUNCTION 필수
	UFUNCTION()
	void OnSearchTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
