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

    FinalPoint = CreateDefaultSubobject<USceneComponent>(TEXT("FinalPoint"));
    FinalPoint->SetupAttachment(RootComponent);

    TensionAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("TensionAudioComp"));
    TensionAudioComp->SetupAttachment(RootComponent);
    TensionAudioComp->bAutoActivate = false; // 트리거 밟기 전에는 소리 끄기
}

void ABlackoutScareTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr && GhostClass != nullptr)
    {

        // 중복 실행 방지를 위해 콜리전 끄기
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // 플레이어의 이동 입력을 막음 (시야는 회전 가능)
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            PC->SetIgnoreMoveInput(true);
        }

        // 귀신을 미리 한 번만 소환해 두고 투명하게 숨김
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnedGhost = GetWorld()->SpawnActor<AZombieCharacter>(GhostClass, FarPoint->GetComponentLocation(), FarPoint->GetComponentRotation(), SpawnParams);

        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorHiddenInGame(true); // 안 보이게 숨김
            //SpawnedGhost->SetActorEnableCollision(false); // 충돌체 끄기
        }

        if (TensionAudioComp && !TensionAudioComp->IsPlaying())
        {
            TensionAudioComp->Play();
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
        // [불 켜짐] 상황 종료, 트리거 자체 파괴
        OnToggleLights(true);

        // 플레이어 이동 다시 활성화
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            PC->SetIgnoreMoveInput(false);
        }

        // [최종 등장] 원하는 위치(FinalPoint)에 귀신 배치 및 애니메이션 재생
        if (SpawnedGhost)
        {
            SpawnedGhost->SetActorLocationAndRotation(FinalPoint->GetComponentLocation(), FinalPoint->GetComponentRotation());
            SpawnedGhost->SetActorHiddenInGame(false);

            if (PointingAnimMontage)
            {
                SpawnedGhost->PlayAnimMontage(PointingAnimMontage);
            }
        }

        // 연출을 담당하던 트리거 액터만 파괴 (귀신은 맵에 계속 남음)
        Destroy();
        return; // 타이머 종료
    }

    CurrentStage++;

    // 계산된 지연 시간 후에 다음 단계를 자동으로 실행
    GetWorld()->GetTimerManager().SetTimer(SequenceTimerHandle, this, &ABlackoutScareTrigger::AdvanceScareSequence, NextDelay, false);
}