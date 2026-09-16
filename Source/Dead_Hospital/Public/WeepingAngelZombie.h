#pragma once

#include "CoreMinimal.h"
#include "../AI/ZombieCharacter.h"
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

    // 시야에서 벗어났을 때의 추격 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Angel")
    float ChaseSpeed = 800.0f;

private:
    // 플레이어가 나를 보고 있는지 계산하는 함수
    bool CheckIfSeenByPlayer();

    // 상태 변화를 감지하기 위한 이전 프레임 상태 저장용 변수
    bool bWasSeenLastFrame = false;
};