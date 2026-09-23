#include "DeadHospitalSaveSubsystem.h"
#include "DeadHospitalGameMode.h"
#include "DeadHospitalSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	constexpr int32 ManualSaveSlotCount = 3;
	bool IsManualSlotIndexValid(int32 SlotIndex) { return SlotIndex >= 0 && SlotIndex < ManualSaveSlotCount; }
	FString MakeManualSlotName(int32 SlotIndex) { return FString::Printf(TEXT("DeadHospital_Save_%02d"), SlotIndex + 1); }

	bool SerializeCheckpoint(const FDeadHospitalCheckpointData& Checkpoint, TArray<uint8>& OutBytes)
	{
		OutBytes.Reset();
		FMemoryWriter Writer(OutBytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = false;
		FDeadHospitalCheckpointData::StaticStruct()->SerializeItem(Archive, const_cast<FDeadHospitalCheckpointData*>(&Checkpoint), nullptr);
		return !Archive.IsError();
	}

	bool DeserializeCheckpoint(const TArray<uint8>& Bytes, FDeadHospitalCheckpointData& OutCheckpoint)
	{
		if (Bytes.IsEmpty()) return false;
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Archive.ArIsSaveGame = false;
		FDeadHospitalCheckpointData::StaticStruct()->SerializeItem(Archive, &OutCheckpoint, nullptr);
		return !Archive.IsError() && OutCheckpoint.IsValid;
	}

	ADeadHospitalGameMode* GetDeadHospitalGameMode(const UObject* Context)
	{
		return Context && Context->GetWorld()
			? Cast<ADeadHospitalGameMode>(UGameplayStatics::GetGameMode(Context->GetWorld()))
			: nullptr;
	}

	bool SaveCheckpointToFile(
		ADeadHospitalGameMode* GameMode,
		const FString& SlotName)
	{
		FDeadHospitalCheckpointData Checkpoint;

		if (!IsValid(GameMode))
		{
			UE_LOG(LogTemp, Error, TEXT("SAVE FAILED: GameMode is invalid"));
			return false;
		}

		if (!GameMode->GetCheckpointSnapshot(Checkpoint))
		{
			UE_LOG(LogTemp, Error, TEXT("SAVE FAILED: Checkpoint snapshot is invalid"));
			return false;
		}

		UDeadHospitalSaveGame* SaveGame =
			Cast<UDeadHospitalSaveGame>(
				UGameplayStatics::CreateSaveGameObject(
					UDeadHospitalSaveGame::StaticClass()));

		if (!IsValid(SaveGame))
		{
			UE_LOG(LogTemp, Error, TEXT("SAVE FAILED: SaveGame object creation failed"));
			return false;
		}

		if (!SerializeCheckpoint(Checkpoint, SaveGame->CheckpointBytes))
		{
			UE_LOG(LogTemp, Error, TEXT("SAVE FAILED: Checkpoint serialization failed"));
			return false;
		}

		SaveGame->SavedAreaId = Checkpoint.SavedAreaId;
		SaveGame->SavedPlayTimeSeconds = Checkpoint.SavedPlayTimeSeconds;
		SaveGame->SavedAt = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));

		const bool bSaved =
			UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SAVE FILE %s : %s"),
			*SlotName,
			bSaved ? TEXT("SUCCESS") : TEXT("FAILED")
		);

		return bSaved;
	}

	bool ReadSlotInfo(const FString& SlotName, bool& bOutHasSave, FName& OutAreaId, FString& OutSavedAt, int32& OutPlayTimeSeconds)
	{
		bOutHasSave = false;
		OutAreaId = NAME_None;
		OutSavedAt.Empty();
		OutPlayTimeSeconds = 0;
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
		const UDeadHospitalSaveGame* SaveGame = Cast<UDeadHospitalSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!IsValid(SaveGame) || SaveGame->CheckpointBytes.IsEmpty()) return false;
		bOutHasSave = true;
		OutAreaId = SaveGame->SavedAreaId;
		OutSavedAt = SaveGame->SavedAt;
		OutPlayTimeSeconds = SaveGame->SavedPlayTimeSeconds;
		return true;
	}

	bool LoadCheckpointFromFile(ADeadHospitalGameMode* GameMode, const FString& SlotName)
	{
		if (!IsValid(GameMode) || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
		const UDeadHospitalSaveGame* SaveGame = Cast<UDeadHospitalSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		FDeadHospitalCheckpointData LoadedCheckpoint;
		if (!IsValid(SaveGame) || !DeserializeCheckpoint(SaveGame->CheckpointBytes, LoadedCheckpoint)) return false;
		return GameMode->RestoreCheckpointSnapshot(LoadedCheckpoint);
	}
}

bool UDeadHospitalSaveSubsystem::SaveNextAutoSlot()
{
	const bool bSaved = SaveSlot(NextAutoSaveSlotIndex);
	if (bSaved)
	{
		NextAutoSaveSlotIndex = (NextAutoSaveSlotIndex + 1) % ManualSaveSlotCount;
	}
	return bSaved;
}
bool UDeadHospitalSaveSubsystem::SaveSlot(int32 SlotIndex) { return IsManualSlotIndexValid(SlotIndex) && SaveCheckpointToFile(GetDeadHospitalGameMode(this), MakeManualSlotName(SlotIndex)); }
bool UDeadHospitalSaveSubsystem::LoadSlot(int32 SlotIndex) { return IsManualSlotIndexValid(SlotIndex) && LoadCheckpointFromFile(GetDeadHospitalGameMode(this), MakeManualSlotName(SlotIndex)); }
bool UDeadHospitalSaveSubsystem::GetSlotInfo(int32 SlotIndex, bool& bOutHasSave, FName& OutAreaId, FString& OutSavedAt, int32& OutPlayTimeSeconds) const
{
	if (!IsManualSlotIndexValid(SlotIndex))
	{
		bOutHasSave = false; OutAreaId = NAME_None; OutSavedAt.Empty(); OutPlayTimeSeconds = 0; return false;
	}
	return ReadSlotInfo(MakeManualSlotName(SlotIndex), bOutHasSave, OutAreaId, OutSavedAt, OutPlayTimeSeconds);
}
