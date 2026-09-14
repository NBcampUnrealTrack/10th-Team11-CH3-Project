#pragma once

#include "CoreMinimal.h"
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
	USphereComponent* AttackRangeComp;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ACharacter* ChaseTarget;//추적타겟
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	EZombieState CurrentState;//좀비 현재 상태(Idle, Patrol, Chase, Attack,	Dead)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* AttackMontage;//애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	UAnimMontage* SearchTurnMontage;


public:
	AZombieCharacter();

	float GetHealth() { return Health; }
	float GetMaxHealth() { return MaxHealth; }
	FString GetZombieName() { return ZombieName; }
	//float GetDefense() { return Defense; }
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void Attack();
	void Die();
	void SetHealth(float NewHealth);
	void SetCurrentState(EZombieState NewState);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnAttackAnimationFinished();
	void PlayAttackMontage();
	void PlaySearchTurnMontage();
	void OnSearchTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ToggleSearchTurnDirection();
	UFUNCTION(BlueprintCallable, Category = "Search")
	bool IsPlayingSearchTurn() const;
	FTimerHandle DisappearTimerHandle;

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
};
