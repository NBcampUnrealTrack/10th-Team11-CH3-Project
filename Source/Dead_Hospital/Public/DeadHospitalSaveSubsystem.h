#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DeadHospitalSaveSubsystem.generated.h"

UCLASS()
class DEAD_HOSPITAL_API UDeadHospitalSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 체크포인트 때마다 01 → 02 → 03 → 01 순서로 저장합니다.
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	bool SaveNextAutoSlot();

	// 0, 1, 2 = UI의 슬롯 01, 02, 03
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	bool SaveSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Save Game")
	bool LoadSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Save Game")
	bool GetSlotInfo(int32 SlotIndex, bool& bOutHasSave, FName& OutAreaId, FString& OutSavedAt, int32& OutPlayTimeSeconds) const;

	UFUNCTION(BlueprintCallable)
	bool RequestLoadSlotFromMainMenu(int32 SlotIndex);

	UFUNCTION(BlueprintCallable)
	bool LoadPendingSlot();

	UFUNCTION(BlueprintPure)
	bool HasPendingLoad() const;

private:
	int32 NextAutoSaveSlotIndex = 0;

	int32 PendingLoadSlotIndex = INDEX_NONE;
};
