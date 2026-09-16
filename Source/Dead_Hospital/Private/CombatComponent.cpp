#include "CombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "InventoryComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "Perception/AISense_Hearing.h"


UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    bCanPrimaryAttack = true;
    bIsReloading = false;
}


// Getter 함수 구현

int32 UCombatComponent::GetCurrentAmmo() const
{
    // 인벤토리에서 장착된 무기가 있는지 확인
    UInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (Inventory != nullptr && Inventory->HasEquippedWeapon())
    {
        // 인벤토리에서 현재 무기 ID를 가져와서 맵에서 검색
        if (const FWeaponData* Data = WeaponDataMap.Find(Inventory->GetEquippedWeaponID()))
        {
            return Data->CurrentAmmo;
        }
    }
    return 0;
}

float UCombatComponent::GetWeaponDamage() const
{
    UInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (Inventory != nullptr && Inventory->HasEquippedWeapon())
    {
        if (const FWeaponData* Data = WeaponDataMap.Find(Inventory->GetEquippedWeaponID()))
        {
            return Data->BaseDamage;
        }
    }
    return 0.0f;
}

void UCombatComponent::PrimaryAttack()
{
    // 플레이어 캐릭터로 캐스팅
    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetOwner());

    //// 플레이어가 없거나, 죽었거나, 숨어있으면 즉시 차단
    //if (PlayerCharacter == nullptr || PlayerCharacter->IsDead() || PlayerCharacter->IsHiding())
    //{
    //    return;
    //}

    // 전투 컴포넌트 내부 상태 검사 (쿨타임, 장전 중)
    if (bCanPrimaryAttack == false || bIsReloading == true)
    {
        return;
    }

    // 인벤토리에서 장착 상태 실시간 확인
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr || !InventoryComponent->HasEquippedWeapon())
    {
        return; // 장착된 무기가 없으면 사격 불가
    }

    // 인벤토리에서 현재 무기 ID를 가져옴
    FName CurrentWeaponID = InventoryComponent->GetEquippedWeaponID();

    // 유효한 무기인지 검사
    if (CurrentWeaponID != FName("HandGun") && CurrentWeaponID != FName("Magnum"))
    {
        return;
    }

    // 가져온 무기 ID로 맵에서 데이터를 찾음
    FWeaponData* CurrentWeaponData = WeaponDataMap.Find(CurrentWeaponID);
    if (CurrentWeaponData == nullptr) return;

    /*if (CurrentWeaponData->CurrentAmmo <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("(탄약이 없습니다)"));
        if (EmptySound) UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetOwner()->GetActorLocation());
        OnAmmoEmptyWarning();

        if (PlayerCharacter->IsHiding()) return;

        ReloadWeapon();
        return;
    }*/

    // 조건 충족 시: 상태 변경 및 탄약 차감
    bCanPrimaryAttack = false;
    CurrentWeaponData->CurrentAmmo -= 1; // 탄약 차감

    UpdateAmmoUI();

    // 쿨타임 타이머 시작 (공격 속도 시간만큼 대기 후 ResetPrimaryAttack 실행)
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_PrimaryCooldown, 
        this, 
        &UCombatComponent::ResetPrimaryAttack, 
        CurrentWeaponData->AttackSpeed, 
        false
    );

    AActor* Owner = GetOwner();
    if (Owner == nullptr) return;

    // 플레이어의 카메라 위치와 회전값 가져오기
    FVector EyeLocation;
    FRotator EyeRotation;
    Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    // 총기 사운드 및 애니메이션 재생
    if (FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, EyeLocation);
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
    if (OwnerCharacter && FireAnimation)
    {
        OwnerCharacter->PlayAnimMontage(FireAnimation);
    }

    // 무한한 사거리 (시선 방향 벡터 * 999999.0f)
    FVector ShotDirection = EyeRotation.Vector();
    FVector TraceEnd = EyeLocation + (ShotDirection * 999999.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner); // 내가 쏜 총에 내가 맞지 않도록 예외 처리

    // 레이캐스트(보이지 않는 선) 발사
    bool bSuccess = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        EyeLocation,
        TraceEnd,
        ECC_Visibility, // 눈에 보이는 물체들과 충돌
        QueryParams
    );

    // 테스트용 빨간색 궤적 그리기 (에디터에서 눈으로 확인하기 위함, 2초간 유지)
    DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::Red, false, 2.0f, 0, 2.0f);

    if (bSuccess)
    {
        // 맞은 지점에 초록색 점 표시
        DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 20.0f, FColor::Green, false, 2.0f);

        // 어떤 액터를 맞췄는지, 데미지는 얼마를 줘야 하는지 계산해서 ProcessHit로 넘김
        AActor* HitActor = HitResult.GetActor();
        float DamageToApply = GetWeaponDamage(); 

        FName HitBoneName = HitResult.BoneName;

        // 뼈 이름이 "head", "Head", "neck" 등일 경우 데미지 2배 (권총/매그넘 공통)
        if (HitBoneName == FName("head") || HitBoneName == FName("Head") || HitBoneName == FName("neck"))
        {
            DamageToApply *= HeadshotMultiplier; // 2.0f 대신 헤더에서 선언한 변수 사용
            UE_LOG(LogTemp, Warning, TEXT("헤드샷. 배수(%f) 적용됨 (적중 부위: %s)"), HeadshotMultiplier, *HitBoneName.ToString());
        }

        // 계산된 최종 데미지를 전달
        ProcessHit(HitActor, DamageToApply);

        // 타격 위치에 피 튀김/스파크 이펙트 생성
        if (HitEffect)
        {
            UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitEffect, HitResult.ImpactPoint);
        }
    }
}

void UCombatComponent::ProcessHit(AActor* HitTarget, float AppliedDamage, bool bIsHeadshot)
{
    // 타격 대상이 유효한지 검사
    if (HitTarget == nullptr) return;

    // 시체 타격 방지 (중복 킬 방지)
    // 맞은 대상이 좀비 캐릭터인지 확인
    AZombieCharacter* HitZombie = Cast<AZombieCharacter>(HitTarget);

    if (HitZombie)
    {
        // 체력이 0 이하라면(죽었다면) 타격 무시
        if (HitZombie->GetHealth() <= 0.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("이미 죽은 적입니다. 타격 무시."));
            return;
        }

        // 데미지 전달 (벽에는 적용되지 않음)
        float ActualDamage = UGameplayStatics::ApplyDamage(
            HitTarget,
            AppliedDamage,
            GetOwner()->GetInstigatorController(),
            GetOwner(),
            UDamageType::StaticClass()
        );

        // 명중 신호 및 실제 적용된 데미지 UI로 발송 (벽을 맞추면 신호 안 감)
        OnEnemyHit.Broadcast(ActualDamage, bIsHeadshot);

        UE_LOG(LogTemp, Warning, TEXT("타격 성공. 맞은 대상: %s, 최종 데미지: %f"), *HitTarget->GetName(), ActualDamage);
    }
}

void UCombatComponent::ResetPrimaryAttack()
{
    // 타이머 시간이 다 되면 다시 사격할 수 있도록 상태 복구
    bCanPrimaryAttack = true;
}

void UCombatComponent::ReloadWeapon()
{
    // 상태 및 장전 중복 검사
    // 플레이어 캐릭터로 캐스팅
    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetOwner());

    //// 플레이어가 없거나, 죽었거나, 숨어있으면 즉시 차단
    //if (PlayerCharacter == nullptr || PlayerCharacter->IsDead() || PlayerCharacter->IsHiding())
    //{
    //    return;
    //}

    // 전투 컴포넌트 내부 상태 검사 (쿨타임, 장전 중)
    if (bCanPrimaryAttack == false || bIsReloading == true)
    {
        return;
    }

    // 인벤토리에서 장착 상태 실시간 확인
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr || !InventoryComponent->HasEquippedWeapon())
    {
        return;
    }

    // 인벤토리에서 현재 무기 ID를 가져옴
    FName CurrentWeaponID = InventoryComponent->GetEquippedWeaponID();

    if (CurrentWeaponID != FName("HandGun") && CurrentWeaponID != FName("Magnum")) return;

    // 가져온 무기 ID로 맵에서 데이터를 찾음
    FWeaponData* CurrentWeaponData = WeaponDataMap.Find(CurrentWeaponID);
    if (CurrentWeaponData == nullptr) return;

    // 이미 탄창이 꽉 찼으면 무시
    if (CurrentWeaponData->CurrentAmmo >= CurrentWeaponData->MagazineCapacity) return;

    // CurrentWeaponID 사용
    FName AmmoItemName = (CurrentWeaponID == FName("HandGun")) ? FName("HandGunAmmo") : FName("MagnumAmmo");

    // 가방에 예비 총알이 있는지 검사
    int32 AmmoCount = InventoryComponent->GetItemQuantity(AmmoItemName);
    if (AmmoCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("장전 실패: 가방에 %s가 없습니다"), *AmmoItemName.ToString());

        // 장전 실패 시에도 찰칵 소리 재생
        if (EmptySound) UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetOwner()->GetActorLocation());
        return;
    }

    // 장전 상태 진입 (사격 불가)
    bIsReloading = true;
    UE_LOG(LogTemp, Warning, TEXT("재장전 중..."));

    // 재장전 사운드 및 애니메이션 재생
    if (ReloadSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetOwner()->GetActorLocation());
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter && ReloadAnimation)
    {
        OwnerCharacter->PlayAnimMontage(ReloadAnimation);
    }

    // 장전 대기 시간(2초) 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_Reload,
        this,
        &UCombatComponent::FinishReload,
        2.0f,
        false
    );
}

void UCombatComponent::FinishReload()
{
    // 인벤토리 컴포넌트와 무기 장착 여부 확인
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr || !InventoryComponent->HasEquippedWeapon())
    {
        bIsReloading = false;
        return;
    }

    // 인벤토리에서 무기 ID를 가져와서 데이터 조회
    FName CurrentWeaponID = InventoryComponent->GetEquippedWeaponID();
    FWeaponData* CurrentWeaponData = WeaponDataMap.Find(CurrentWeaponID);
    if (CurrentWeaponData == nullptr) return;

    // 소모할 인벤토리 탄약 이름 결정
    FName AmmoItemName = (CurrentWeaponID == FName("HandGun")) ? FName("HandGunAmmo") : FName("MagnumAmmo");

    // 탄창에 채워야 할 빈 공간 계산
    int32 NeededAmmo = CurrentWeaponData->MagazineCapacity - CurrentWeaponData->CurrentAmmo;

    // 가방에 남아있는 총알 갯수 확인
    int32 AmmoCount = InventoryComponent->GetItemQuantity(AmmoItemName);

    // 실제 장전할 수량(ReloadAmount) 결정
    // 빈 공간(NeededAmmo)과 가방 속 총알(AmmoCount) 중 더 작은 값을 선택
    int32 ReloadAmount = FMath::Min(NeededAmmo, AmmoCount);

    if (InventoryComponent->RemoveItem(AmmoItemName, ReloadAmount))
    {
        CurrentWeaponData->CurrentAmmo += ReloadAmount;
    }

    // 장전 상태 해제
    bIsReloading = false;
    UE_LOG(LogTemp, Warning, TEXT("장전 완료. 현재 탄창: %d발"), CurrentWeaponData->CurrentAmmo);

    UpdateAmmoUI();
}

// 사망 시 상태 강제 정리
void UCombatComponent::HandlePlayerDeath()
{
    bIsReloading = false;
    bCanPrimaryAttack = false;

    // 장전 중이거나 쿨타임 대기 중 사망 시, 돌고 있던 언리얼 타이머를 없애서 추가 동작 방지
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Reload);
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PrimaryCooldown);

    // UI 블루프린트에 전투 UI를 비활성화하라는 신호 전달
    OnCombatStateCleared();

    UE_LOG(LogTemp, Warning, TEXT("플레이어 사망: 전투 타이머 없애고 상태 초기화 완료"));
}

void UCombatComponent::UpdateAmmoUI()
{
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr || !InventoryComponent->HasEquippedWeapon()) return;

    FName CurrentWeaponID = InventoryComponent->GetEquippedWeaponID();
    FWeaponData* CurrentWeaponData = WeaponDataMap.Find(CurrentWeaponID);
    if (CurrentWeaponData == nullptr) return;

    // 인벤토리에서 예비 탄약(ReserveAmmo) 수량 확인
    FName AmmoItemName = (CurrentWeaponID == FName("HandGun")) ? FName("HandGunAmmo") : FName("MagnumAmmo");
    int32 ReserveAmmo = InventoryComponent->GetItemQuantity(AmmoItemName);

    // UI 블루프린트로 현재 탄창과 예비 총알 수량 전하기
    OnAmmoChanged.Broadcast(CurrentWeaponData->CurrentAmmo, ReserveAmmo);
}

int32 UCombatComponent::GetMagazineCapacity() const
{
    UInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (Inventory != nullptr && Inventory->HasEquippedWeapon())
    {
        if (const FWeaponData* Data = WeaponDataMap.Find(Inventory->GetEquippedWeaponID()))
        {
            return Data->MagazineCapacity;
        }
    }
    return 0;
}

void UCombatComponent::CancelReload()
{
    // 장전 중이 아니면 무시
    if (!bIsReloading) return;

    // 돌고 있던 장전 타이머를 없애서 FinishReload가 실행되지 않게 막음
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Reload);
    bIsReloading = false;

    UE_LOG(LogTemp, Warning, TEXT("장전이 취소되었습니다."));
}