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

// 전투 총괄 관리 컴포넌트
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEAD_HOSPITAL_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UCombatComponent();

protected:
    virtual void BeginPlay() override;

public:
    // 무기 데이터 및 상태 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FWeaponData CurrentWeapon;  // 현재 장착 중인 무기 데이터 참조

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bCanPrimaryAttack; // 기본 공격 가능 여부 - 연사 쿨타임 체크

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsReloading;  // 재장전 중인지 여부 - 장전 중 공격 불가능

    // 외부 시스템 연동용 Getter 함수 (UI, 인벤토리 등)
    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    int32 GetCurrentAmmo() const;   // 총알 갯수

    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    float GetWeaponDamage() const;  // 총 데미지

    // UI 경고 메시지 띄우기 (탄약이 없습니다)
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|UI")
    void OnAmmoEmptyWarning();

    // 기본 공격
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void PrimaryAttack();

    // 타격 처리 및 데미지 전달
    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void ProcessHit(AActor* HitTarget, float AppliedDamage);

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

private:
    // 장전 완료 콜백
    UFUNCTION()
    void FinishReload();

    // 타이머 제어용 핸들 (재장전 소요 시간(딜레이))
    FTimerHandle TimerHandle_Reload;

    // 타이머 제어용 핸들 (총의 연사 속도(발사 딜레이))
    FTimerHandle TimerHandle_PrimaryCooldown;
};