#include "ItemBaitSpawner.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"

AItemBaitSpawner::AItemBaitSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AItemBaitSpawner::TriggerScare(APlayerCharacter* TargetPlayer)
{
    if (bHasTriggered) return;
    if (TargetPlayer == nullptr || ZombieClassToSpawn == nullptr) return;

    bHasTriggered = true;

    FVector PlayerLocation = TargetPlayer->GetActorLocation();
    FVector PlayerForward = TargetPlayer->GetActorForwardVector();

    FVector SpawnLocation = PlayerLocation - (PlayerForward * SpawnDistance);
    SpawnLocation.Z = PlayerLocation.Z;

    // 바닥 높이 보정 (계단/경사 등에서 파묻히거나 공중에 뜨는 것 방지)
    FHitResult HitResult;
    FVector TraceStart = SpawnLocation + FVector(0, 0, 200.f);
    FVector TraceEnd = SpawnLocation - FVector(0, 0, 500.f);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility))
    {
        SpawnLocation.Z = HitResult.Location.Z + GetDefault<AZombieCharacter>(ZombieClassToSpawn)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }

    FRotator SpawnRotation = (PlayerLocation - SpawnLocation).Rotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AZombieCharacter* SpawnedZombie = GetWorld()->SpawnActor<AZombieCharacter>(
        ZombieClassToSpawn,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (SpawnedZombie)
    {
        // AutoPossessAI = PlacedInWorldOrSpawned 이므로 SpawnActor가 끝난 시점엔
        // 이미 AIController가 Possess + BT 시작까지 완료된 상태 → 안전하게 바로 호출 가능
        SpawnedZombie->AggroOnSpawn();

        // 카메라 셰이크는 플레이어를 조종하는 로컬 컨트롤러에서 재생해야 함
        if (ScareCameraShake)
        {
            if (APlayerController* PC = Cast<APlayerController>(TargetPlayer->GetController()))
            {
                PC->ClientStartCameraShake(ScareCameraShake, CameraShakeScale);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("등 뒤에 좀비가 소환되었습니다."));
    }
}