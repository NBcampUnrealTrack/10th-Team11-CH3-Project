#include "FakeAudioScareTrigger.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerCharacter.h"

AFakeAudioScareTrigger::AFakeAudioScareTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    // 소리 발생지 세팅
    SoundOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("SoundOrigin"));
    RootComponent = SoundOrigin;

    // 소리 시작 트리거 세팅
    PlayTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PlayTrigger"));
    PlayTrigger->SetupAttachment(RootComponent);
    PlayTrigger->SetCollisionProfileName(TEXT("Trigger"));
    PlayTrigger->OnComponentBeginOverlap.AddDynamic(this, &AFakeAudioScareTrigger::OnPlayTriggerOverlap);

    // 소리 종료 트리거 세팅
    StopTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("StopTrigger"));
    StopTrigger->SetupAttachment(RootComponent);
    StopTrigger->SetCollisionProfileName(TEXT("Trigger"));
    StopTrigger->OnComponentBeginOverlap.AddDynamic(this, &AFakeAudioScareTrigger::OnStopTriggerOverlap);

    // 오디오 컴포넌트 세팅
    AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComp"));
    AudioComp->SetupAttachment(RootComponent);
    AudioComp->bAutoActivate = false;
}

void AFakeAudioScareTrigger::OnPlayTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr && ScareSound != nullptr)
    {
        if (bIsOneShot)
        {
            // 쾅 하는 단발성 소리 (문 닫힘)
            UGameplayStatics::PlaySoundAtLocation(this, ScareSound, SoundOrigin->GetComponentLocation());

            Destroy(); // 한 번 놀래키고 액터 삭제
        }
        else
        {
            // 콧노래처럼 계속 들리는 소리
            if (!AudioComp->IsPlaying())
            {
                AudioComp->SetSound(ScareSound);
                AudioComp->Play();
            }

            // 소리를 켰으니 시작 트리거만 끄고(중복 실행 방지), 끄는 트리거는 남겨둠
            PlayTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
}

void AFakeAudioScareTrigger::OnStopTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr && !bIsOneShot)
    {
        // 빈 방에 도착했을 때: 콧노래 소리를 1초에 걸쳐 점점 줄여서 꺼버림
        if (AudioComp->IsPlaying())
        {
            AudioComp->FadeOut(1.0f, 0.0f);
        }

        StopTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // 페이드아웃 끝날 시간(1.5초) 여유를 주고 액터 파괴
        SetLifeSpan(1.5f);
    }
}