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
    return CurrentWeapon.CurrentAmmo;
}

float UCombatComponent::GetWeaponDamage() const
{
    return CurrentWeapon.BaseDamage;
}

void UCombatComponent::PrimaryAttack()
{
    // 발사 조건 검사 (쿨타임 대기 중이거나, 장전 중이거나, 총알이 0개면 발사 불가)
    if (bCanPrimaryAttack == false || bIsReloading == true)
    {
        return;
    }

    if (CurrentWeapon.CurrentAmmo <= 0)
    {
        // 총알이 없을 때의 처리 (빈 총 소리 등)
        UE_LOG(LogTemp, Warning, TEXT("(탄약이 없습니다)"));
        
        // 빈 총 사운드 재생
        if (EmptySound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetOwner()->GetActorLocation());
        }

        // 탄약이 0일 때 자동 재장전 실행
        ReloadWeapon();
        return;
    }

    // 조건 충족 시: 상태 변경 및 탄약 차감
    bCanPrimaryAttack = false;
    CurrentWeapon.CurrentAmmo -= 1;

    // 쿨타임 타이머 시작 (공격 속도 시간만큼 대기 후 ResetPrimaryAttack 실행)
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_PrimaryCooldown,
        this,
        &UCombatComponent::ResetPrimaryAttack,
        CurrentWeapon.AttackSpeed,
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
        ProcessHit(HitActor, DamageToApply);

        // 타격 위치에 피 튀김/스파크 이펙트 생성
        if (HitEffect)
        {
            UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitEffect, HitResult.ImpactPoint);
        }
    }
}

void UCombatComponent::ProcessHit(AActor* HitTarget, float AppliedDamage)
{
    if (HitTarget)
    {
        // 데미지 전달 함수 호출
        UGameplayStatics::ApplyDamage(
            HitTarget,               // 맞는 대상
            AppliedDamage,           // 데미지 수치
            GetOwner()->GetInstigatorController(), // 공격한 사람의 컨트롤러
            GetOwner(),              // 공격한 사람(무기 또는 플레이어)
            UDamageType::StaticClass()
        );

        // 출력 로그 창에 맞은 대상의 이름과 들어간 데미지를 글자로 띄움 (확인용)
        UE_LOG(LogTemp, Warning, TEXT("타격 성공! 맞은 대상: %s, 데미지: %f"), *HitTarget->GetName(), AppliedDamage);
    }
}

void UCombatComponent::ResetPrimaryAttack()
{
    // 타이머 시간이 다 되면 다시 사격할 수 있도록 상태 복구
    bCanPrimaryAttack = true;
}

void UCombatComponent::ReloadWeapon()
{
    // 이미 탄창이 꽉 찼거나 장전 중이면 무시
    if (CurrentWeapon.CurrentAmmo >= CurrentWeapon.MagazineCapacity || bIsReloading == true)
    {
        return;
    }

    // 플레이어 액터에서 인벤토리 컴포넌트 찾아오기
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr) return;

    // 가방에 예비 총알이 있는지 검사
    int32 AmmoCount = InventoryComponent->GetItemQuantity("Ammo");
    if (AmmoCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("장전 실패: 가방에 총알이 없습니다"));

        // 장전 실패 시에도 찰칵 소리 재생
        if (EmptySound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetOwner()->GetActorLocation());
        }
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
    UInventoryComponent* InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (InventoryComponent == nullptr)
    {
        bIsReloading = false;
        return;
    }

    // 탄창에 채워야 할 빈 공간 계산
    int32 NeededAmmo = CurrentWeapon.MagazineCapacity - CurrentWeapon.CurrentAmmo;

    // 가방에 남아있는 총알 갯수 확인
    int32 AmmoCount = InventoryComponent->GetItemQuantity("Ammo");

    // 실제 장전할 수량(ReloadAmount) 결정
    // 빈 공간(NeededAmmo)과 가방 속 총알(AmmoCount) 중 더 작은 값을 선택
    int32 ReloadAmount = FMath::Min(NeededAmmo, AmmoCount);

    // 탄창에 총알 추가
    CurrentWeapon.CurrentAmmo += ReloadAmount;

    // 가방에서 실제로 소비한 총알 차감
    InventoryComponent->RemoveItem("Ammo", ReloadAmount);

    // 장전 상태 해제
    bIsReloading = false;
    UE_LOG(LogTemp, Warning, TEXT("장전 완료. 현재 탄창: %d발"), CurrentWeapon.CurrentAmmo);
}