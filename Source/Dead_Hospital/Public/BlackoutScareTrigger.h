#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackoutScareTrigger.generated.h"

class UBoxComponent;
class AZombieCharacter;

UCLASS()
class DEAD_HOSPITAL_API ABlackoutScareTrigger : public AActor
{
	GENERATED_BODY()
	
public:
    ABlackoutScareTrigger();

protected:
    // 플레이어가 밟을 트리거
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    UBoxComponent* TriggerBox;

    // 귀신이 나타날 3가지 위치 (에디터에서 화살표로 직관적 배치 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* FarPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* MidPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* ClosePoint;

    // 소환할 귀신(좀비) 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    TSubclassOf<AZombieCharacter> GhostClass;

    // 불이 켜져 있는 시간 (기본 0.4초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    float LightOnDuration = 0.4f;

    // 불이 꺼져 있는 시간 (기본 0.2초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    float LightOffDuration = 0.2f;

    // 블루프린트에서 조명 끄고 켜기를 구현할 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "JumpScare|Event")
    void OnToggleLights(bool bTurnOn);

private:
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 연출 단계를 진행하는 핵심 함수
    void AdvanceScareSequence();

    FTimerHandle SequenceTimerHandle;
    int32 CurrentStage = 0;
    AZombieCharacter* SpawnedGhost = nullptr;
};
