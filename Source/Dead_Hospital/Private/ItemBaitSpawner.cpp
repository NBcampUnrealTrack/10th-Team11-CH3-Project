#include "ItemBaitSpawner.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "Engine/World.h"

AItemBaitSpawner::AItemBaitSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AItemBaitSpawner::TriggerScare(APlayerCharacter* TargetPlayer)
{
    // 플레이어 정보가 없거나, 에디터에서 스폰할 좀비 클래스를 안 넣었으면 무시
    if (TargetPlayer == nullptr || ZombieClassToSpawn == nullptr) return;

    // 플레이어의 등 뒤 위치 계산
    FVector PlayerLocation = TargetPlayer->GetActorLocation();
    FVector PlayerForward = TargetPlayer->GetActorForwardVector();

    // 등 뒤 좌표 = 플레이어 위치 - (플레이어 앞방향 벡터 * 거리)
    FVector SpawnLocation = PlayerLocation - (PlayerForward * SpawnDistance);

    // 높이는 플레이어와 동일하게 맞춰서 공중에 뜨거나 땅에 박히지 않게 함
    SpawnLocation.Z = PlayerLocation.Z;

    // 좀비가 스폰되자마자 플레이어를 바라보도록 회전값 계산
    FRotator SpawnRotation = (PlayerLocation - SpawnLocation).Rotation();

    // 월드에 좀비 스폰 설정 (벽에 끼면 살짝 밀어내서라도 무조건 소환)
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 좀비 소환
    AZombieCharacter* SpawnedZombie = GetWorld()->SpawnActor<AZombieCharacter>(
        ZombieClassToSpawn,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (SpawnedZombie)
    {
        UE_LOG(LogTemp, Warning, TEXT("등 뒤에 좀비가 소환되었습니다."));
    }
}