#include "BlackoutScareTrigger.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "../AI/ZombieCharacter.h"
#include "PlayerCharacter.h"

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
}

void ABlackoutScareTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr && GhostClass != nullptr)
    {
        // 중복 실행 방지를 위해 콜리전 끄기
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // 귀신을 미리 한 번만 소환해 두고 투명하게 숨김
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnedGhost = GetWorld()->SpawnActor<AZombieCharacter>(GhostClass, FarPoint->GetComponentLocation(), FarPoint->GetComponentRotation(), SpawnParams);

        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorHiddenInGame(true); // 안 보이게 숨김
            SpawnedGhost->SetActorEnableCollision(false); // 충돌체 끄기
        }

        // 연출 상태 머신 시작
        CurrentStage = 0;
        AdvanceScareSequence();
    }
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

            // 사운드 부착 가능 (마지막 귀신 나타날 때)
        }
        NextDelay = LightOnDuration;
        break;

    case 6:
        // [불 꺼짐] 귀신 소멸
        OnToggleLights(false);
        if (SpawnedGhost) SpawnedGhost->Destroy(); // 연출이 끝났으므로 완전 삭제
        NextDelay = LightOffDuration;
        break;

    case 7:
        // [불 켜짐] 상황 종료, 트리거 자체 파괴
        OnToggleLights(true);
        Destroy();
        return; // 타이머 종료
    }

    CurrentStage++;

    // 계산된 지연 시간 후에 다음 단계를 자동으로 실행
    GetWorld()->GetTimerManager().SetTimer(SequenceTimerHandle, this, &ABlackoutScareTrigger::AdvanceScareSequence, NextDelay, false);
}