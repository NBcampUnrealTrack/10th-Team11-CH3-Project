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
	Attack,
	Dead
};

class USphereComponent;

UCLASS()
class DEAD_HOSPITAL_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
	USphereComponent* AttackRangeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float Health;//현재체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float MaxHealth;//최대체력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	FString ZombieName;//이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float Defense;//방어력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float Power;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float AttackRange;//공격 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float PatrolSpeed;//순찰(걷는) 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float ChaseSpeed;//추적 속도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ACharacter* ChaseTarget;//추적타겟
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	EZombieState CurrentState;//좀비 현재 상태(Idle, Patrol, Chase, Attack,	Dead)

public:
	AZombieCharacter();

	float GetHealth() { return Health; }
	float GetMaxHealth() { return MaxHealth; }
	FString GetZombieName() { return ZombieName; }
	float GetDefense() { return Defense; }
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
