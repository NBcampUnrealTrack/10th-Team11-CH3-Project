#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemBaitSpawner.generated.h"

class AZombieCharacter;
class APlayerCharacter;

UCLASS()
class DEAD_HOSPITAL_API AItemBaitSpawner : public AActor
{
	GENERATED_BODY()
	
public:
    AItemBaitSpawner();

    // 좀비의 종류
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
    TSubclassOf<AZombieCharacter> ZombieClassToSpawn;

    // 플레이어 등 뒤 얼만큼 떨어져서 스폰될지 (기본값 3미터)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare")
    float SpawnDistance = 300.0f;

    // 아이템 담당자가 아이템 습득 시 호출할 스위치 함수
    UFUNCTION(BlueprintCallable, Category = "JumpScare")
    void TriggerScare(APlayerCharacter* TargetPlayer);
};
