#include "PuzzleScareSpawner.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "Components/AudioComponent.h"

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
        // 생성된 사운드를 PlayingAudioComp 변수에 저장합니다.
        PlayingAudioComp = UGameplayStatics::SpawnSoundAtLocation(this, ScareSound, SpawnLocation);
        if (PlayingAudioComp)
        {
            PlayingAudioComp->FadeIn(DelayBeforeScare, 1.0f);
        }
    }

    // 좀비 소환 타이머
    GetWorld()->GetTimerManager().SetTimer(
        ScareTimerHandle,
        this,
        &APuzzleScareSpawner::ExecuteScare,
        DelayBeforeScare,
        false
    );

    // 사운드 강제 종료 타이머 추가 (SoundDuration인 7초 뒤에 StopScareSound 실행)
    GetWorld()->GetTimerManager().SetTimer(
        AudioStopTimerHandle,
        this,
        &APuzzleScareSpawner::StopScareSound,
        SoundDuration,
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


void APuzzleScareSpawner::StopScareSound()
{
    // 오디오 컴포넌트가 존재하고, 아직 재생 중인지 안전하게 검사
    if (IsValid(PlayingAudioComp) && PlayingAudioComp->IsPlaying())
    {
        // 1초에 걸쳐 스르륵 자연스럽게 소리 끄기 (뚝 끊기면 어색하므로 FadeOut 사용)
        PlayingAudioComp->FadeOut(1.0f, 0.0f);
    }
}