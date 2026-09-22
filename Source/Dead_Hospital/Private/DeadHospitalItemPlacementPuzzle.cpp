// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalItemPlacementPuzzle.h"

#include "DeadHospitalDoor.h"
#include "DeadHospitalGameMode.h"
#include "InventoryComponent.h"
#include "GameFramework/Pawn.h"

ADeadHospitalItemPlacementPuzzle::ADeadHospitalItemPlacementPuzzle()
{
	// 퍼즐마다 필요한 아이템과 PuzzleId가 다르므로 생성자에서 임의의 ID를 만들지 않습니다.
	// L_MainLevel에 만든 Blueprint 자식 또는 배치 Actor의 Details에서 PZ04/PZ06/PZ07 중 맞는 값을 설정합니다.
	PrimaryActorTick.bCanEverTick = false;
	InteractionText = FText::FromString(TEXT("E 키로 아이템 배치 퍼즐 조사"));
}

void ADeadHospitalItemPlacementPuzzle::BeginPlay()
{
	Super::BeginPlay();

	// 필요한 ID가 하나도 없으면 어떤 아이템을 검사해야 하는지 알 수 없어 절대 완료되지 않습니다.
	// 플레이 중 조용히 막히는 것보다 에디터 Output Log에서 설정 누락을 바로 찾을 수 있도록 오류를 남깁니다.
	if (RequiredItemIds.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: RequiredItemIds is empty."), *GetName());
	}

	for (const FName RequiredItemId : RequiredItemIds)
	{
		if (RequiredItemId.IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("%s: RequiredItemIds contains None."), *GetName());
			break;
		}
	}
}

bool ADeadHospitalItemPlacementPuzzle::SubmitItemLayout(
	AActor* Interactor,
	const TArray<FName>& SubmittedItemIds)
{
	// 화면의 슬롯 배치가 정답과 다른 경우 인벤토리, GameMode, Door를 하나도 변경하지 않습니다.
	// 따라서 오답을 내도 중요 아이템을 잃지 않고 UI에서 자리를 바꿔 다시 제출할 수 있습니다.
	if (!DoesSubmittedLayoutMatch(SubmittedItemIds))
	{
		OnItemLayoutRejected();
		return false;
	}

	if (!TryCompletePuzzle(Interactor))
	{
		OnItemLayoutRejected();
		return false;
	}

	OnItemLayoutAccepted();
	return true;
}

bool ADeadHospitalItemPlacementPuzzle::TryCompletePuzzle(AActor* Interactor)
{
	// 소비 전에 모든 조건과 수량을 먼저 확인합니다. 이 검사에서 실패하면 인벤토리는 바뀌지 않습니다.
	if (!CanInteract_Implementation(Interactor) || !CanCompletePuzzle(Interactor))
	{
		return false;
	}

	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!IsValid(Inventory))
	{
		return false;
	}

	if (ConsumeItemsOnSuccess && !ConsumeRequiredItems(Inventory))
	{
		return false;
	}

	// 부모 TryCompletePuzzle은 GameMode에 PuzzleId를 저장하고 ConnectedDoor를 엽니다.
	// 아이템을 이미 소비한 경우 부모 안의 두 번째 조건 검사가 실패하지 않도록 잠시 상태 표시를 켭니다.
	CompletingAfterItemConsumption = ConsumeItemsOnSuccess;
	const bool CompletionRecorded = Super::TryCompletePuzzle(Interactor);
	CompletingAfterItemConsumption = false;

	if (CompletionRecorded)
	{
		PendingConsumedItems.Reset();

		if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
		{
			for (const FName CompletedSubObjectiveId : SubObjectiveIdsToClearOnSuccess)
			{
				if (!CompletedSubObjectiveId.IsNone())
				{
					GameMode->ClearSubObjectiveById(CompletedSubObjectiveId);
				}
			}
		}

		return true;
	}

	// 부모 완료가 예상 밖으로 실패했다면 방금 소비한 아이템을 즉시 복구합니다.
	// 이로써 문은 열리지 않았는데 수집한 머리/그림만 없어지는 상태를 막습니다.
	if (ConsumeItemsOnSuccess)
	{
		RestorePendingConsumedItems(Inventory);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Items were consumed, but puzzle completion failed. Check Blueprint callbacks that change the door or game phase during submission."),
			*GetName());
	}

	return false;
}

bool ADeadHospitalItemPlacementPuzzle::CanCompletePuzzle(AActor* Interactor) const
{
	// 1. 부모 퍼즐 조건 + RequiredItemIds 확인
	if (!Super::CanCompletePuzzle(Interactor))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - Super::CanCompletePuzzle FAILED"));
		return false;
	}

	if (RequiredItemIds.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - RequiredItemIds is EMPTY"));
		return false;
	}


	// 2. 상호작용한 대상이 플레이어인지 확인
	const APawn* PlayerPawn = Cast<APawn>(Interactor);

	if (!IsValid(PlayerPawn))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - Interactor is NOT Pawn"));
		return false;
	}

	if (!PlayerPawn->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - Pawn is NOT PlayerControlled"));
		return false;
	}


	// 3. 연결된 문 확인
	if (RequireConnectedDoor && !IsValid(ConnectedDoor))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - ConnectedDoor INVALID"));
		return false;
	}

	if (IsValid(ConnectedDoor) &&
		!ConnectedDoor->CanUnlockFromPuzzle(PuzzleId))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - Door rejected PuzzleId: %s"),
			*PuzzleId.ToString());

		return false;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("PZ07 DEBUG - DOOR CHECK SUCCESS"));


	// 4. 아이템 소비 후 다시 검사하는 경우
	if (CompletingAfterItemConsumption)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PZ07 DEBUG - CompletingAfterItemConsumption SUCCESS"));

		return true;
	}


	// 5. 플레이어 인벤토리 확인
	const UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PZ07 DEBUG - Inventory INVALID"));
		return false;
	}


	// 6. 필요한 아이템과 수량 계산
	TMap<FName, int32> RequiredQuantities;

	for (const FName RequiredItemId : RequiredItemIds)
	{
		if (RequiredItemId.IsNone())
		{
			UE_LOG(LogTemp, Error,
				TEXT("PZ07 DEBUG - RequiredItemId is None"));
			return false;
		}

		RequiredQuantities.FindOrAdd(RequiredItemId) += 1;
	}


	// 7. 실제 인벤토리 수량 확인
	for (const TPair<FName, int32>& RequiredPair : RequiredQuantities)
	{
		const int32 HaveQuantity =
			Inventory->GetItemQuantity(RequiredPair.Key);

		UE_LOG(LogTemp, Warning,
			TEXT("PZ07 DEBUG - Required: %s / Need: %d / Have: %d"),
			*RequiredPair.Key.ToString(),
			RequiredPair.Value,
			HaveQuantity);

		if (HaveQuantity < RequiredPair.Value)
		{
			UE_LOG(LogTemp, Error,
				TEXT("PZ07 DEBUG - NOT ENOUGH ITEM"));

			return false;
		}
	}


	UE_LOG(LogTemp, Warning,
		TEXT("PZ07 DEBUG - ALL CHECK SUCCESS"));

	return true;
}

bool ADeadHospitalItemPlacementPuzzle::DoesSubmittedLayoutMatch(
	const TArray<FName>& SubmittedItemIds) const
{
	if (SubmittedItemIds.Num() != RequiredItemIds.Num() || RequiredItemIds.IsEmpty())
	{
		return false;
	}

	if (RequireExactOrder)
	{
		return SubmittedItemIds == RequiredItemIds;
	}

	TMap<FName, int32> RequiredCounts;
	TMap<FName, int32> SubmittedCounts;

	for (const FName ItemId : RequiredItemIds)
	{
		RequiredCounts.FindOrAdd(ItemId) += 1;
	}

	for (const FName ItemId : SubmittedItemIds)
	{
		SubmittedCounts.FindOrAdd(ItemId) += 1;
	}

	if (RequiredCounts.Num() != SubmittedCounts.Num())
	{
		return false;
	}

	for (const TPair<FName, int32>& RequiredPair : RequiredCounts)
	{
		const int32* SubmittedCount = SubmittedCounts.Find(RequiredPair.Key);

		if (SubmittedCount == nullptr ||
			*SubmittedCount != RequiredPair.Value)
		{
			return false;
		}
	}

	return true;
}

bool ADeadHospitalItemPlacementPuzzle::ConsumeRequiredItems(UInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return false;
	}

	TMap<FName, int32> RequiredQuantities;
	TMap<FName, FItemData> OriginalItemData;
	PendingConsumedItems.Reset();
	for (const FName RequiredItemId : RequiredItemIds)
	{
		RequiredQuantities.FindOrAdd(RequiredItemId) += 1;
	}

	// 실패 복구에 필요한 이름, 타입, MaxStack 등을 소비 전에 슬롯에서 복사합니다.
	// InventorySlots 자체를 수정하지 않고 GetInventorySlots()가 돌려준 읽기 전용 자료만 확인합니다.
	for (const FInventorySlot& Slot : Inventory->GetInventorySlots())
	{
		if (!Slot.bIsEmpty
			&& RequiredQuantities.Contains(Slot.ItemData.ItemID)
			&& !OriginalItemData.Contains(Slot.ItemData.ItemID))
		{
			OriginalItemData.Add(Slot.ItemData.ItemID, Slot.ItemData);
		}
	}

	// ConsumeKeyItem은 KeyItem 타입만 소비할 수 있습니다. 필요한 ID의 원본 자료를 하나라도 찾지 못했거나
	// 타입이 KeyItem이 아니라면 아직 아무것도 소비하지 않은 상태에서 안전하게 실패합니다.
	for (const TPair<FName, int32>& RequiredPair : RequiredQuantities)
	{
		const FItemData* SavedData = OriginalItemData.Find(RequiredPair.Key);
		if (SavedData == nullptr || SavedData->ItemType != EItemType::KeyItem)
		{
			return false;
		}
	}

	for (const TPair<FName, int32>& RequiredPair : RequiredQuantities)
	{
		if (Inventory->ConsumeKeyItem(RequiredPair.Key, RequiredPair.Value))
		{
			if (const FItemData* SavedData = OriginalItemData.Find(RequiredPair.Key))
			{
				FItemData PendingData = *SavedData;
				PendingData.Quantity = RequiredPair.Value;
				PendingConsumedItems.Add(PendingData);
			}
			continue;
		}

		// 중간에 한 종류라도 소비에 실패하면 앞에서 소비한 종류들을 원래 수량만큼 다시 넣습니다.
		// 이 복구 때문에 머리 하나만 사라지고 퍼즐은 실패하는 부분 완료 상태를 만들지 않습니다.
		RestorePendingConsumedItems(Inventory);
		return false;
	}

	return true;
}

void ADeadHospitalItemPlacementPuzzle::RestorePendingConsumedItems(UInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return;
	}

	for (const FItemData& ItemToRestore : PendingConsumedItems)
	{
		if (!Inventory->AddItem(ItemToRestore))
		{
			// 소비하면서 같은 수만큼 빈 공간이 생겼기 때문에 정상 상황에서는 복구가 성공해야 합니다.
			// 실패한다면 외부 Blueprint가 그 사이 인벤토리를 채운 것이므로 잃은 아이템 ID를 로그에 남깁니다.
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s: Failed to restore consumed item %s."),
				*GetName(),
				*ItemToRestore.ItemID.ToString());
		}
	}

	PendingConsumedItems.Reset();
}
