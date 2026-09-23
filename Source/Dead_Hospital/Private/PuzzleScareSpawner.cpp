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

    // 좀비가 소환될 등 뒤 위치를 미리 계산
    FVector PlayerLocation = TargetPlayer->GetActorLocation();
    FVector PlayerForward = TargetPlayer->GetActorForwardVector();
    FVector SpawnLocation = PlayerLocation - (PlayerForward * SpawnDistance);
    SpawnLocation.Z = PlayerLocation.Z;

    // 단발성 재생 대신 오디오 컴포넌트를 생성하여 서서히 볼륨 증가(Fade-In)
    if (ScareSound)
    {
        UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAtLocation(this, ScareSound, SpawnLocation);
        if (AudioComp)
        {
            // DelayBeforeScare(예: 2.5초) 동안 볼륨이 0에서 최종 볼륨(1.0f)까지 커짐
            AudioComp->FadeIn(DelayBeforeScare, 1.0f);
        }
    }

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