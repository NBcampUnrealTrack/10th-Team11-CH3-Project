#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

// 무기 기본 데이터 구조체
USTRUCT(BlueprintType)
struct FWeaponData
{
    GENERATED_BODY()

public:
    // --- 탄약 스탯 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
    int32 CurrentAmmo;  // 현재 탄창의 총알 수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
    int32 MagazineCapacity; // 탄창 최대 용량

    // --- 공격 스탯 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
    float BaseDamage;   // 기본 공격력

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
    float AttackSpeed;  // 공격 속도

    FWeaponData()
    {
        CurrentAmmo = 30;
        MagazineCapacity = 30;
        BaseDamage = 25.0f;
        AttackSpeed = 0.2f; // 0.2초마다 1발 발사
    }
};

// 탄약 갱신 신호 (현재 탄창, 가방에 남은 예비 총알)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, MaxAmmo);

// 적중 신호 (실제 들어간 데미지, 헤드샷 여부)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyHitSignature, float, DamageApplied, bool, bIsHeadshot);

// 적 처치 신호 (해골 문양 표시용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyKilledSignature);

// 무기 교체 신호 (UI 연동용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponChangedSignature, FName, WeaponName, float, Damage, int32, CurrentAmmo, int32, MaxAmmo);

// 무기 발사 성공 신호 (크로스헤어 반동 연출용 등)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponFired, FName, WeaponID);

// 전투 총괄 관리 컴포넌트
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEAD_HOSPITAL_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

protected:
    virtual void BeginPlay() override;

public:
    // 블루프린트에서 바인딩할 이벤트 변수
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnWeaponChangedSignature OnWeaponChanged;

    // 블루프린트에서 바인딩할 실제 발사 신호
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnWeaponFired OnWeaponFired;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnAmmoChangedSignature OnAmmoChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnEnemyHitSignature OnEnemyHit;

    // 블루프린트에서 바인딩할 적 처치 이벤트 변수
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnEnemyKilledSignature OnEnemyKilled;

    // 탄약 UI 갱신을 한 번에 처리할 헬퍼 함수
    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    void UpdateAmmoUI();

    // 무기 데이터 및 상태 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data")
    TMap<FName, FWeaponData> WeaponDataMap;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bCanPrimaryAttack; // 기본 공격 가능 여부 - 연사 쿨타임 체크

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsReloading;  // 재장전 중인지 여부 - 장전 중 공격 불가능

    UFUNCTION(BlueprintPure, Category = "Combat|Weapon Info")
    float GetEquippedWeaponDamage() const;

    // 외부 시스템 연동용 Getter 함수 (UI, 인벤토리 등)
    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    int32 GetCurrentAmmo() const;   // 총알 갯수

    // 지정한 무기의 현재 탄창 수를 반환합니다.
    // 체크포인트에서 HandGun / Magnum 탄창 상태를 저장할 때 사용합니다.
    UFUNCTION(BlueprintPure, Category = "Combat|Ammo")
    int32 GetWeaponCurrentAmmo(FName WeaponID) const;

    // 지정한 무기의 현재 탄창 수를 설정합니다.
    // 체크포인트에서 저장된 탄창 상태를 복원할 때 사용합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Ammo")
    bool SetWeaponCurrentAmmo(FName WeaponID, int32 NewAmmo);

    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    int32 GetMagazineCapacity() const;  // 탄창 최대 용량 반환

    UFUNCTION(BlueprintCallable)
    void NotifyWeaponChanged();

    // UI 경고 메시지 띄우기 (탄약이 없습니다)
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|UI")
    void OnAmmoEmptyWarning();

    // 기본 공격
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void PrimaryAttack();

    // 타격 처리 및 데미지 전달
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void ProcessHit(AActor* HitTarget, float AppliedDamage, bool bIsHeadshot = false);

    // 쿨타임 복구 함수
    UFUNCTION()
    void ResetPrimaryAttack();

    // 장전 시작
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void ReloadWeapon();

    // 시각/청각 에셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Effects")
    class USoundBase* FireSound; // 사격 소리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Effects")
    class USoundBase* EmptySound; // 빈 총 찰칵 소리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Effects")
    class USoundBase* ReloadSound; // 재장전 소리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Effects")
    class UParticleSystem* HitEffect; // 피격 이펙트 (피 튀김, 스파크)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    class UAnimMontage* FireAnimation; // 사격 애니메이션

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    class UAnimMontage* ReloadAnimation; // 재장전 애니메이션

    // 헤드샷 배수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stats")
    float HeadshotMultiplier = 2.0f;

    // 플레이어 사망 시 호출되어 타이머와 상태를 강제 초기화
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void HandlePlayerDeath();

    // UI 비활성화 신호 (사망 시 호출되어 전투 UI를 끄도록 유도)
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|UI")
    void OnCombatStateCleared();

    // 무기 교체, 구르기 등 특정 액션 시 장전 취소
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void CancelReload();

private:
    // 장전 완료 콜백
    UFUNCTION()
    void FinishReload();

    // 타이머 제어용 핸들 (재장전 소요 시간(딜레이))
    FTimerHandle TimerHandle_Reload;

    // 타이머 제어용 핸들 (총의 연사 속도(발사 딜레이))
    FTimerHandle TimerHandle_PrimaryCooldown;

    // 재장전을 시작한 무기의 ID를 저장합니다.
    // 재장전 도중 다른 무기로 변경되어도
    // 처음 재장전을 시작한 무기를 기준으로 장전합니다.
    FName ReloadingWeaponID = NAME_None;
};