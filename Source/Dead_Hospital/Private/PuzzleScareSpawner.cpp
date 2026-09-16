#include "PuzzleScareSpawner.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"

APuzzleScareSpawner::APuzzleScareSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void APuzzleScareSpawner::OnPuzzleSolved(APlayerCharacter* Player)
{
    if (Player == nullptr || ZombieClassToSpawn == nullptr) return;

    TargetPlayer = Player; // 타이머가 끝난 뒤에 쓰기 위해 플레이어 저장

    // 설정된 시간(DelayBeforeScare)만큼 기다렸다가 ExecuteScare 실행
    GetWorld()->GetTimerManager().SetTimer(
        ScareTimerHandle,
        this,
        &APuzzleScareSpawner::ExecuteScare,
        DelayBeforeScare,
        false
    );

    UE_LOG(LogTemp, Warning, TEXT("퍼즐 완료 신호 수신. %f초 뒤 점프 스케어가 발동됩니다..."), DelayBeforeScare);
}

void APuzzleScareSpawner::ExecuteScare()
{
    if (TargetPlayer == nullptr) return;

    // 플레이어 등 뒤 좌표 계산
    FVector PlayerLocation = TargetPlayer->GetActorLocation();
    FVector PlayerForward = TargetPlayer->GetActorForwardVector();

    FVector SpawnLocation = PlayerLocation - (PlayerForward * SpawnDistance);
    SpawnLocation.Z = PlayerLocation.Z;
    FRotator SpawnRotation = (PlayerLocation - SpawnLocation).Rotation();

    // 등 뒤(스폰될 위치)에서 사운드 재생
    if (ScareSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ScareSound, SpawnLocation);
    }

    // 좀비 소환
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
        UE_LOG(LogTemp, Warning, TEXT("플레이어 등 뒤에 좀비가 나타났습니다."));
    }
}