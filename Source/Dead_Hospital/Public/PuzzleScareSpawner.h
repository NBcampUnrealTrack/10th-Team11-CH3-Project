#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleScareSpawner.generated.h"

class AZombieCharacter;
class APlayerCharacter;
class USoundBase;

UCLASS()
class DEAD_HOSPITAL_API APuzzleScareSpawner : public AActor
{
	GENERATED_BODY()
	
public:
    APuzzleScareSpawner();

    // 소환할 좀비 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Puzzle")
    TSubclassOf<AZombieCharacter> ZombieClassToSpawn;

    // 등 뒤에서 들릴 사운드 (발소리, 괴성 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Puzzle")
    USoundBase* ScareSound;

    // 퍼즐 완료 후 적이 등장하기까지의 안심 시간 (기본값 2.5초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Puzzle")
    float DelayBeforeScare = 2.5f;

    // 등 뒤 얼만큼 떨어져서 스폰될지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Puzzle")
    float SpawnDistance = 400.0f;

    // 퍼즐 성공 시 호출해 줄 함수
    UFUNCTION(BlueprintCallable, Category = "JumpScare|Puzzle")
    void OnPuzzleSolved(APlayerCharacter* Player);

private:
    // 안심 시간이 끝난 뒤 실제로 적을 소환하는 내부 함수
    void ExecuteScare();

    // 타이머가 돌 동안 플레이어 정보를 들고 있을 변수
    APlayerCharacter* TargetPlayer;

    // 시간차 공격을 위한 타이머 핸들
    FTimerHandle ScareTimerHandle;
};