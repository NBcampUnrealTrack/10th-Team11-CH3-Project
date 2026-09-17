#pragma once

#include "CoreMinimal.h"
#include "ZombieCharacter.h"
#include "WeepingAngelZombie.generated.h"

UCLASS()
class DEAD_HOSPITAL_API AWeepingAngelZombie : public AZombieCharacter
{
    GENERATED_BODY()

public:
    AWeepingAngelZombie();

    // 매 프레임 시야를 검사하기 위해 Tick 함수 오버라이드
    virtual void Tick(float DeltaTime) override;

    // 공격 불가 처리 -> 데미지 파이프라인 오버라이드
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // 우는 천사에게 닿았을 때 즉사 처리를 위한 오버랩 함수
    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

    // 즉사 데미지가 들어가기 직전에 효과음(뼈 부러지는 소리 등)
    UPROPERTY(EditAnywhere)
    USoundBase* CatchSound;

private:
    // 플레이어가 나를 보고 있는지 계산하는 함수
    bool CheckIfSeenByPlayer();

    // 상태 변화를 감지하기 위한 이전 프레임 상태 저장용 변수
    bool bWasSeenLastFrame = false;

protected:
    // 이동 시 재생할 반복 사운드 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Audio")
    class UAudioComponent* MovementAudioComp;
};