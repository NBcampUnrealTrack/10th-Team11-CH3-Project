#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DeadHospitalSaveGame.generated.h"


/**
 * 디스크에 저장되는 슬롯 1개입니다.
 * CheckpointBytes에는 FDeadHospitalCheckpointData 전체를 직렬화해 보관합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API UDeadHospitalSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 SaveVersion = 1;

	UPROPERTY(SaveGame)
	FName SavedAreaId = NAME_None;

	UPROPERTY(SaveGame)
	FString SavedAt;

	UPROPERTY(SaveGame)
	int32 SavedPlayTimeSeconds = 0;

	UPROPERTY(SaveGame)
	TArray<uint8> CheckpointBytes;
};
