#include "DeadHospitalDoor.h"

#include "DeadHospitalGameMode.h"
#include "InventoryComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADeadHospitalDoor::ADeadHospitalDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root Component
	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);

	// 실제 문 Mesh
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot);

	LockedInteractionText = FText::FromString(TEXT("잠겨 있다"));
	OpenInteractionText = FText::FromString(TEXT("E 키로 문 열기"));
	CloseInteractionText = FText::FromString(TEXT("E 키로 문 닫기"));
}

void ADeadHospitalDoor::BeginPlay()
{
	Super::BeginPlay();

	if (DoorId.IsNone())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: DoorId is None. Unlock state cannot be restored from a checkpoint."),
			*GetName());
	}

	if (!RequiredPuzzleId.IsNone() && !RequiredKeyItemId.IsNone())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Set either RequiredPuzzleId or RequiredKeyItemId, not both."),
			*GetName());
	}

	IsLocked = StartsLocked;
	IsOpen = false;

	if (ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		if (GameMode->IsOneTimeEventCompleted(MakeUnlockedEventId()))
		{
			IsLocked = false;
		}

		GameMode->OnCheckpointRestored.AddDynamic(
			this,
			&ADeadHospitalDoor::HandleCheckpointRestored);
	}

	OnDoorStateRestored(IsLocked, IsOpen);
}

void ADeadHospitalDoor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(
			this,
			&ADeadHospitalDoor::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalDoor::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	// 문이 잠겨있으면 문 자체에서는 열 수 없음
	// 퍼즐이나 카드리더를 통해 먼저 잠금을 해제해야 함
	if (IsLocked)
	{
		OnDoorAccessDenied();
		return;
	}

	// 잠금이 풀린 문만 직접 열고 닫을 수 있음
	IsOpen = !IsOpen;

	OnDoorMovementRequested(IsOpen);
}

bool ADeadHospitalDoor::CanInteract_Implementation(
	AActor* Interactor) const
{
	if (!IsValid(Interactor) || !CanUseDoorInCurrentPhase())
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);

	return IsValid(PlayerPawn)
		&& PlayerPawn->IsPlayerControlled();
}

FText ADeadHospitalDoor::GetInteractionText_Implementation() const
{
	if (IsLocked)
	{
		return LockedInteractionText;
	}

	return IsOpen
		? CloseInteractionText
		: OpenInteractionText;
}

// ============================================================
// 퍼즐에서 문 잠금 해제
// ============================================================

bool ADeadHospitalDoor::UnlockFromPuzzle(
	FName SolvedPuzzleId)
{
	if (!CanUnlockFromPuzzle(SolvedPuzzleId))
	{
		return false;
	}

	if (!IsLocked)
	{
		return true;
	}

	const ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();

	if (!IsValid(GameMode)
		|| !GameMode->IsPuzzleCompleted(SolvedPuzzleId))
	{
		return false;
	}

	return UnlockDoor();
}

bool ADeadHospitalDoor::CanUnlockFromPuzzle(
	FName PuzzleIdToCheck) const
{
	if (PuzzleIdToCheck.IsNone()
		|| RequiredPuzzleId.IsNone()
		|| !RequiredKeyItemId.IsNone()
		|| PuzzleIdToCheck != RequiredPuzzleId)
	{
		return false;
	}

	return true;
}

// ============================================================
// 카드리더에서 호출
// ============================================================

bool ADeadHospitalDoor::UnlockAndOpenFromKeyReader(
	FName KeyItemId)
{
	// 카드 ID가 비어 있으면 실패
	if (KeyItemId.IsNone())
	{
		return false;
	}

	// 이 문이 카드키 방식인지 확인
	if (RequiredKeyItemId.IsNone())
	{
		return false;
	}

	// 퍼즐 직접 해제 방식의 문에는 사용하지 않음
	if (!RequiredPuzzleId.IsNone())
	{
		return false;
	}

	// 카드리더가 검사한 카드와
	// 문이 요구하는 카드가 같은지 확인
	if (KeyItemId != RequiredKeyItemId)
	{
		return false;
	}

	// 잠겨 있다면 잠금 해제
	if (IsLocked)
	{
		if (!UnlockDoor())
		{
			return false;
		}
	}

	// 이미 열려 있지 않을 때만 열기
	if (!IsOpen)
	{
		IsOpen = true;

		// BP에서 만든 DoorTimeline 실행
		OnDoorMovementRequested(true);
	}

	return true;
}

// ============================================================
// 실제 잠금 해제
// ============================================================

bool ADeadHospitalDoor::UnlockDoor()
{
	if (!IsLocked)
	{
		return false;
	}

	if (DoorId.IsNone())
	{
		return false;
	}

	if (ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		const FName UnlockedEventId = MakeUnlockedEventId();

		if (!UnlockedEventId.IsNone())
		{
			if (GameMode->IsOneTimeEventCompleted(UnlockedEventId))
			{
				IsLocked = false;
				return true;
			}

			if (!GameMode->TryStartOneTimeEvent(UnlockedEventId))
			{
				return false;
			}

			if (!GameMode->CompleteOneTimeEvent(UnlockedEventId))
			{
				GameMode->CancelOneTimeEvent(UnlockedEventId);
				return false;
			}
		}
	}

	IsLocked = false;

	OnDoorUnlocked();

	return true;
}

// ============================================================
// 기존 문 직접 Key 검사
// ============================================================

bool ADeadHospitalDoor::TryUnlockWithKey(
	AActor* Interactor)
{
	if (RequiredKeyItemId.IsNone()
		|| !RequiredPuzzleId.IsNone()
		|| !IsValid(Interactor))
	{
		return false;
	}

	UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory)
		|| !Inventory->HasItem(RequiredKeyItemId))
	{
		return false;
	}

	const bool ShouldConsumeKey =
		ConsumeKeyWhenUnlocked;

	FItemData ConsumedKeyItemData;

	if (ShouldConsumeKey)
	{
		bool FoundKeyItemData = false;

		for (const FInventorySlot& Slot :
			Inventory->GetInventorySlots())
		{
			if (Slot.bIsEmpty
				|| Slot.ItemData.ItemID != RequiredKeyItemId)
			{
				continue;
			}

			ConsumedKeyItemData = Slot.ItemData;
			ConsumedKeyItemData.Quantity = 1;

			FoundKeyItemData = true;

			break;
		}

		if (!FoundKeyItemData
			|| !Inventory->ConsumeKeyItem(
				RequiredKeyItemId,
				1))
		{
			return false;
		}
	}

	if (UnlockDoor())
	{
		return true;
	}

	// 실패하면 소비한 Key 복구
	if (ShouldConsumeKey
		&& !Inventory->AddItem(ConsumedKeyItemData))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Door %s failed to return consumed KeyItem %s after unlock failure."),
			*DoorId.ToString(),
			*RequiredKeyItemId.ToString());
	}

	return false;
}

// ============================================================
// 현재 GamePhase에서 문 사용 가능한지 검사
// ============================================================

bool ADeadHospitalDoor::CanUseDoorInCurrentPhase() const
{
	const ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();

	if (!IsValid(GameMode))
	{
		return false;
	}

	const EDeadHospitalGamePhase Phase =
		GameMode->GetCurrentGamePhase();

	return
		Phase == EDeadHospitalGamePhase::Playing
		|| Phase == EDeadHospitalGamePhase::FinalObjective
		|| Phase == EDeadHospitalGamePhase::ReturningToHospital
		|| Phase == EDeadHospitalGamePhase::Escape;
}

// ============================================================
// 체크포인트 Event ID
// ============================================================

FName ADeadHospitalDoor::MakeUnlockedEventId() const
{
	if (DoorId.IsNone())
	{
		return NAME_None;
	}

	return FName(
		*(DoorId.ToString() + TEXT("_Unlocked")));
}

// ============================================================
// 체크포인트 복구
// ============================================================

void ADeadHospitalDoor::HandleCheckpointRestored(
	FName CheckpointId)
{
	const ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();

	if (!IsValid(GameMode))
	{
		return;
	}

	IsLocked =
		StartsLocked
		&& !GameMode->IsOneTimeEventCompleted(
			MakeUnlockedEventId());

	IsOpen = false;

	OnDoorStateRestored(
		IsLocked,
		IsOpen);
}