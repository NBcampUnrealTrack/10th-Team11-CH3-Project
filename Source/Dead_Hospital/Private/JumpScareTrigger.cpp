#include "JumpScareTrigger.h"
#include "Components/BoxComponent.h"
#include "PlayerCharacter.h"
#include "../AI/ZombieCharacter.h"

AJumpScareTrigger::AJumpScareTrigger()
{
    // 박스 컴포넌트 생성 및 설정
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    // 플레이어와만 충돌하도록 설정
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

    // 오버랩 이벤트 바인딩
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpScareTrigger::OnOverlapBegin);
}

// 게임 시작 시 좀비 눕히기
void AJumpScareTrigger::BeginPlay()
{
    Super::BeginPlay();

    // 에디터에서 연결해 둔 타겟 좀비가 있다면
    if (TargetZombie != nullptr)
    {
        // 시체화 + BT 정지 함수 실행
        TargetZombie->EnterFakeDead();
    }
}

void AJumpScareTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 밟은 대상이 플레이어인지 확인
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
    if (Player != nullptr)
    {
        // 연결된 좀비가 있고, 그 좀비가 죽은 척 중이라면
        if (TargetZombie && TargetZombie->bIsFakeDead)
        {
            TargetZombie->WakeUp();

            // 한 번만 발생해야 하므로 트리거 스스로 파괴
            Destroy();
        }
    }
}