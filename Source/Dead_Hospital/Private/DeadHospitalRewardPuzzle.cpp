// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalRewardPuzzle.h"

#include "DeadHospitalProgressionItem.h"
#include "InventoryComponent.h"
#include "GameFramework/Pawn.h"

ADeadHospitalRewardPuzzle::ADeadHospitalRewardPuzzle()
{
	// 최신 GDD에서 도자기 퍼즐은 PZ05이고 보상은 CardKeyB입니다.
	// Blueprint 자식의 예전 저장값이 있으면 해당 값이 우선할 수 있으므로 에디터 Details도 함께 확인해야 합니다.
	PrimaryActorTick.bCanEverTick = false;
	PuzzleId = TEXT("PZ05");
	InteractionText = FText::FromString(TEXT("E 키로 도자기 퍼즐 조사"));
}

void ADeadHospitalRewardPuzzle::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(RewardPickup))
	{
		// 보상이 없는 상태로 퍼즐 완료를 기록하면 CardKeyB를 영원히 얻지 못할 수 있습니다.
		// CanCompletePuzzle도 완료를 막지만, 설정 원인을 쉽게 찾을 수 있도록 시작 시점에 오류를 남깁니다.
		UE_LOG(LogTemp, Error, TEXT("%s: RewardPickup is not assigned."), *GetName());
		return;
	}

	// ProgressionItem에 네 값을 한 번에 전달해 퍼즐 ID와 보상 Actor의 요구 조건이 서로 어긋나지 않게 합니다.
	// 이 호출 뒤 보상은 PZ05가 완료되기 전까지 숨겨지고, 완료 알림을 받으면 E로 주울 수 있게 나타납니다.
	RewardPickup->ConfigureAsPuzzleReward(
		PuzzleId,
		RewardItemId,
		RewardItemType,
		RewardPickupEventId,
		TEXT("S07"));
}

bool ADeadHospitalRewardPuzzle::TryCompletePuzzle(AActor* Interactor)
{
	if (!Super::TryCompletePuzzle(Interactor))
	{
		OnRewardPuzzleRejected();
		return false;
	}

	// 부모의 GameMode 완료 Broadcast를 RewardPickup이 듣고 있으므로 이 시점에는 보상 Actor가 공개됩니다.
	// 실제 인벤토리 AddItem은 Player가 공개된 CardKeyB Actor에 E를 눌렀을 때 실행됩니다.
	OnRewardPuzzleAccepted();
	return true;
}

bool ADeadHospitalRewardPuzzle::CanCompletePuzzle(AActor* Interactor) const
{
	if (!IsValid(RewardPickup) || !Super::CanCompletePuzzle(Interactor))
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	const UInventoryComponent* Inventory = IsValid(Interactor)
		? Interactor->FindComponentByClass<UInventoryComponent>()
		: nullptr;
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled() || !IsValid(Inventory))
	{
		return false;
	}

	// 같은 단서 ID가 여러 번 들어갈 가능성까지 고려해 필요한 수량을 먼저 합칩니다.
	// 단서 확인만 하며 ConsumeKeyItem/RemoveItem은 호출하지 않으므로 성공 후에도 단서는 그대로 남습니다.
	TMap<FName, int32> RequiredClueQuantities;
	for (const FName ClueItemId : RequiredClueItemIds)
	{
		if (ClueItemId.IsNone())
		{
			return false;
		}

		RequiredClueQuantities.FindOrAdd(ClueItemId) += 1;
	}

	for (const TPair<FName, int32>& RequiredPair : RequiredClueQuantities)
	{
		if (Inventory->GetItemQuantity(RequiredPair.Key) < RequiredPair.Value)
		{
			return false;
		}
	}

	return true;
}
