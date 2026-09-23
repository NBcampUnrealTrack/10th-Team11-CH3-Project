#pragma once

#include "ItemData.h"
#include "ZombieDropTable.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "ZombieCharacter.generated.h"

//좀비의 행동 상태
//예전엔 여러 bool 조합으로 상태를 표현하다 꼬였던 문제를 막기 위해
// "한 순간엔 하나의 값만" 갖도록 enum으로 통합함
UENUM(BlueprintType)
enum class EZombieState : uint8
{
	Patrol UMETA(DisplayName = "Patrol"),
	Investigating UMETA(DisplayName = "Investigating"),
	Chase UMETA(DisplayName = "Chase"),
	Search UMETA(DisplayName = "Search"),
	Attacking UMETA(DisplayName = "Attacking"),
	Hitstun UMETA(DisplayName = "Hitstun"),
	Dead UMETA(DisplayName = "Dead"),
	MoveToLastKnown UMETA(DisplayName = "Move To Last Known")
};

class USphereComponent;

UCLASS()
class DEAD_HOSPITAL_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	//Search 두리번거림 반전 여부
	//(Root Motion 몽타주는 Mirror 노드로 반전이 안 되는 한계로 인해
	//현재 값만 토글되고, 각도 보정 계산에만 쓰임)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Search")
	bool bSearchTurnMirrored = false;
	// 이 좀비가 죽은 척 대기 중인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|JumpScare")
	bool bIsFakeDead = false;
	//스폰 즉시 어그로 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	bool bAggroOnSpawn;

	int32 SearchTurnCount = 0;
	UPROPERTY(EditAnywhere, Category = "Zombie|Search")
	int32 MaxSearchTurnCount = 3;


protected:
	// 컴포넌트 및 타이머

	//공격 판정용 구 콜리전. 생성자에서 고정 생성되므로 컴포넌트 자체 교체는 불가
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Component")
	USphereComponent* AttackRangeComp;
	//디버그용 화살표
	/*UPROPERTY(VisibleAnywhere)
	class UArrowComponent* DebugArrow;*/

	//Search 두리번거림 반복 예약용 타이머
	FTimerHandle SearchTimerHandle;
	//사망 후 제거 예약용 타이머
	FTimerHandle DisappearTimerHandle;
	//경직 지속시간 예약용 타이머
	FTimerHandle HitstunTimerHandle;

	//메시 기본 상대 회전/위치 (BeginPlay 시점 캐싱, 흐트러졌을 때 복원 기준값)
	FRotator DefaultMeshRelativeRotation;
	FVector DefaultMeshRelativeLocation;

	//P2P 왕복 순찰 지점들. EditInstanceOnly라서 같은 Bp를 여러 곳에 배치해도
	//인스턴스마다 다른 순찰 경로를 지정할 수 있음
	UPROPERTY(EditInstanceOnly, Category = "Zombie|LocationPoint")
	TArray<AActor*> PatrolPoints;

	//캐릭터 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Stats")
	float Health;//현재체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
	float MaxHealth;//최대체력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Stats")
	FString ZombieName;//이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
	float Power;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
	float AttackRange;//공격 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
	float PatrolSpeed;//순찰(걷는) 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
	float ChaseSpeed;//추적 속도

	//거리별 속도 변화를 위한 변수들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Chase")
	float MinChaseDistance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Chase")
	float MaxChaseDistance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Chase")
	float MinChaseSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Chase")
	float MaxChaseSpeed;

	//P2P 왕복 순찰에서 현재 목표 지점 인덱스
	int32 CurrentPatrolIndex;
	//피격 경직 지속시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat")
	float HitstunDuration;
	//Search 진입 시점 기준 Yaw
	UPROPERTY()
	float SearchBaseYaw;
	//경직 걸리기 직전 상태 - 경직 종료 후 이 상태로 복귀
	EZombieState PreHitstunState;

	//P2P 왕복 순찰 진행 방향
	bool bPatrolForward = true;

	//상태 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|State")
	ACharacter* ChaseTarget;//추적타겟
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|State")
	EZombieState CurrentState;//좀비 현재 상태(Idle, Patrol, Chase, Attack,	Dead)

	//애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat")
	UAnimMontage* AttackMontage;//애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Search")
	UAnimMontage* SearchTurnMontage;
	// 일어날 때 재생할 애니메이션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|JumpScare")
	UAnimMontage* GetUpMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat")
	UAnimMontage* HitReactMontage;//피격 리액션 애니메이션. 비어있어도 상태 전환/이동 정지는 되지만 애니메이션만 안 나옴

	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Drop")
	UDataTable* ZombieDropTable;//드랍 아이템/확률(가중치)/수량 정의 테이블
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Drop")
	UDataTable* ItemDataTable;//아이템 상세 정보 테이블. ZombieDropTable의 ItemID로 여기서 조회

public:
	AZombieCharacter();

	//엔진 데미지 처리 진입점
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE float GetSearchBaseYaw() const { return SearchBaseYaw; }
	FORCEINLINE float GetPatrolSpeed() const { return PatrolSpeed; }
	FORCEINLINE float GetMinChaseDistance() const { return MinChaseDistance; }
	FORCEINLINE float GetMaxChaseDistance() const { return MaxChaseDistance; }
	FORCEINLINE float GetMinChaseSpeed() const { return MinChaseSpeed; }
	FORCEINLINE float GetMaxChaseSpeed() const { return MaxChaseSpeed; }


	FORCEINLINE FString GetZombieName() const { return ZombieName; }
	FORCEINLINE FRotator GetDefaultMeshRelativeRotation() const { return DefaultMeshRelativeRotation; }
	FORCEINLINE FVector GetDefaultMeshRelativeLocation() const { return DefaultMeshRelativeLocation; }

	//체력 직접 세팅
	void SetHealth(float NewHealth);
	//상태 전환의 유일한 진입점
	void SetCurrentState(EZombieState NewState);
	//Search 진입 시점 기준 Yaw 저장
	FORCEINLINE void SetSearchBaseYaw(float NewYaw);
	FORCEINLINE EZombieState GetCurrentState() const { return CurrentState; }

	//Anim Notify에서 호출 - 공격 판정 타이밍에 데미지 적용
	UFUNCTION(BlueprintCallable, Category = "Zombie|Attack")
	void Attack();

	//사망처리
	void Die();

	//경직 진입/종료
	void EnterHitstun();
	void OnHitstunEnded();

	//공격 애니메이션 종료 시 AI 컨트롤러에 알림
	UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
	void OnAttackAnimationFinished();

	//몽타주 재생 함수들
	void PlayAttackMontage();
	void PlaySearchTurnMontage();
	void ToggleSearchTurnDirection();//Search 반전 방향 토글(현재 실제 반전 재생은 보류)
	void AggroOnSpawn();//스폰 즉시 어그로

	//SearchTurn 몽타주 재생 중여부
	UFUNCTION(BlueprintCallable, Category = "Zombie|Search")
	bool IsPlayingSearchTurn() const;

	// 트리거가 밟히면 호출
	UFUNCTION(BlueprintCallable, Category = "Zombie|JumpScare")
	void WakeUp();
	UFUNCTION(BlueprintCallable, Category = "Zombie|JumpScare")
	void EnterFakeDead();
	//일어나는 몽타주 종료 콜백(BT 재시작)
	void OnGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	//ChaseTarget이 공격 범위 안에 있는지 재계산해서 Blackboard 갱신
	void RefreshAttackRange();
	//Search 시야 판정을 AI 컨트롤러에 위임
	UFUNCTION(BlueprintCallable)
	void CheckSearchTurnSight();
	//bSearchTurnMirrored Getter
	UFUNCTION(BlueprintPure, Category = "Zombie|Search")
	bool bIsSearchTurnMirrored() const;
	//다음 순찰 지점 좌표 계산(왕복 로직 포함)
	bool GetNextPatrolLocation(FVector& OutLocation);

	void RestoreCheckpointTransform(const FTransform& SavedTransform);

protected:
	virtual void BeginPlay() override;
	
	//AI Perception 시야 기준을 캡슐의 실제 회전으로 고정
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	//공격 범위 콜리전 오버랩 콜백
	//UFUNCTION 필요 이유: AddDynamic(동적 델리게이트) 바인딩엔 리플렉션 등록이 필요함
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

	//사망 후 제거 타이머 콜백
	void OnDeathTimerExpired();
	//아이템 드랍 로직
	void HandleItemDrop();

	//델리게이트 바인딩용 함수 - UFUNCTION 필수
	UFUNCTION()
	void OnSearchTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	//BindUObject로 바인딩되므로 (AddDynamic 아님) UFUNCTION 없이도 동작
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
