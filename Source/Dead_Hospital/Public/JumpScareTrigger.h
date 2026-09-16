#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumpScareTrigger.generated.h"

class UBoxComponent;
class AZombieCharacter;

UCLASS()
class DEAD_HOSPITAL_API AJumpScareTrigger : public AActor
{
	GENERATED_BODY()
	
public:	
	AJumpScareTrigger();

protected:
    virtual void BeginPlay() override;

    // 플레이어가 밟을 투명한 충돌 박스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
    UBoxComponent* TriggerBox;

    // 이 트리거를 밟았을 때 깨울 대상 좀비
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    AZombieCharacter* TargetZombie;

    // 플레이어가 박스에 닿았을 때 실행될 이벤트
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};