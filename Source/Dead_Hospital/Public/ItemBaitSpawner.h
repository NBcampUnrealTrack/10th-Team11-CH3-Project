#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraShakeBase.h"
#include "ItemBaitSpawner.generated.h"

class AZombieCharacter;
class APlayerCharacter;

UCLASS()
class DEAD_HOSPITAL_API AItemBaitSpawner : public AActor
{
    GENERATED_BODY()

public:
    AItemBaitSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
    TSubclassOf<AZombieCharacter> ZombieClassToSpawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
    float SpawnDistance = 300.0f;

    // 점프스케어 연출용 카메라 셰이크
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
    TSubclassOf<UCameraShakeBase> ScareCameraShake;

    // 카메라 셰이크 강도 배율 (기본 1.0 = 원본 그대로)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare", meta = (ClampMin = "0.0"))
    float CameraShakeScale = 1.0f;

    // 스폰 효과음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Audio")
    class USoundBase* SpawnSound;

    UPROPERTY()
    bool bHasTriggered = false;

    UFUNCTION(BlueprintCallable, Category = "JumpScare")
    void TriggerScare(APlayerCharacter* TargetPlayer);
};