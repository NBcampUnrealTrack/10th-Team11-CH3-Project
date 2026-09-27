#include "BlackoutScareTrigger.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ABlackoutScareTrigger::ABlackoutScareTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABlackoutScareTrigger::OnOverlapBegin);

    FarPoint = CreateDefaultSubobject<USceneComponent>(TEXT("FarPoint"));
    FarPoint->SetupAttachment(RootComponent);

    MidPoint = CreateDefaultSubobject<USceneComponent>(TEXT("MidPoint"));
    MidPoint->SetupAttachment(RootComponent);

    ClosePoint = CreateDefaultSubobject<USceneComponent>(TEXT("ClosePoint"));
    ClosePoint->SetupAttachment(RootComponent);

    TensionAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("TensionAudioComp"));
    TensionAudioComp->SetupAttachment(RootComponent);
    TensionAudioComp->bAutoActivate = false; // 트리거 밟기 전에는 소리 끄기
}

void ABlackoutScareTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr)
    {
        // 기존 겹침 이벤트도 StartScare 함수를 재사용합니다.
        StartScare(Player);
    }
}

void ABlackoutScareTrigger::StartScare(ACharacter* Player)
{
    if (Player == nullptr || GhostClass == nullptr) return;

    // 중복 실행 방지
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetPlayer = Player;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (PC)
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnedGhost = GetWorld()->SpawnActor<AZombieCharacter>(GhostClass, FarPoint->GetComponentLocation(), FarPoint->GetComponentRotation(), SpawnParams);

    if (SpawnedGhost)
    {
        SpawnedGhost->SetActorHiddenInGame(true);
    }

    if (TensionAudioComp && !TensionAudioComp->IsPlaying())
    {
        TensionAudioComp->Play();
    }

    CurrentStage = 0;
    AdvanceScareSequence();
}

void ABlackoutScareTrigger::AdvanceScareSequence()
{
    float NextDelay = 0.1f;

    switch (CurrentStage)
    {
    case 0:
        // [불 꺼짐] 첫 정전
        OnToggleLights(false);
        NextDelay = LightOffDuration;
        break;

    case 1:
        // [불 켜짐] 멀리서 귀신 등장
        OnToggleLights(true);
        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorLocationAndRotation(FarPoint->GetComponentLocation(), FarPoint->GetComponentRotation());
            SpawnedGhost->SetActorHiddenInGame(false);
        }
        NextDelay = LightOnDuration;
        break;

    case 2:
        // [불 꺼짐]
        OnToggleLights(false);
        if (SpawnedGhost) SpawnedGhost->SetActorHiddenInGame(true);
        NextDelay = LightOffDuration;
        break;

    case 3:
        // [불 켜짐] 중간 거리 귀신 등장
        OnToggleLights(true);
        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorLocationAndRotation(MidPoint->GetComponentLocation(), MidPoint->GetComponentRotation());
            SpawnedGhost->SetActorHiddenInGame(false);
        }
        NextDelay = LightOnDuration;
        break;

    case 4:
        // [불 꺼짐]
        OnToggleLights(false);
        if (SpawnedGhost) SpawnedGhost->SetActorHiddenInGame(true);
        NextDelay = LightOffDuration;
        break;

    case 5:
        // [불 켜짐] 코앞 귀신 등장
        OnToggleLights(true);
        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorLocationAndRotation(ClosePoint->GetComponentLocation(), ClosePoint->GetComponentRotation());
            SpawnedGhost->SetActorHiddenInGame(false);

            if (JumpScareSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, JumpScareSound, ClosePoint->GetComponentLocation());
            }
        }
        NextDelay = LightOnDuration;
        break;

    case 6:
        // [불 꺼짐] 3번째 귀신 모습 감추기
        OnToggleLights(false);
        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorHiddenInGame(true); // Destroy() 대신 숨기기만 함
        }
        NextDelay = LightOffDuration;
        break;

    case 7:
        // [옵션 처리] JS06에서는 불을 켜지 않음
        if (!bKeepLightsOffAtEnd)
        {
            OnToggleLights(true);
        }

        // [옵션 처리] JS06에서는 텔레포트가 끝날 때까지 입력을 복구하지 않음
        if (bRestoreInputWhenScareEnds)
        {
            if (APlayerController* PC = Cast<APlayerController>(TargetPlayer->GetController()))
            {
                PC->SetIgnoreMoveInput(false);
                PC->SetIgnoreLookInput(false);
            }
        }

        // Actor를 파괴하기 전에 텔레포트를 실행하도록 블루프린트에 신호 발송
        OnScareFinished.Broadcast(TargetPlayer);

        return;
    }

    CurrentStage++;

    // 계산된 지연 시간 후에 다음 단계를 자동으로 실행
    GetWorld()->GetTimerManager().SetTimer(SequenceTimerHandle, this, &ABlackoutScareTrigger::AdvanceScareSequence, NextDelay, false);
}

void ABlackoutScareTrigger::ResetLightsAndDestroy()
{
    // 블루프린트에 만들어둔 조명 켜기 이벤트를 다시 호출
    OnToggleLights(true);

    // 조명을 켠 후 안전하게 스스로 파괴
    Destroy();
}