#include "DeadHospitalPaintingFramePuzzle.h"

#include "InventoryComponent.h"

ADeadHospitalPaintingFramePuzzle::ADeadHospitalPaintingFramePuzzle()
{
	PrimaryActorTick.bCanEverTick = false;

	PuzzleId = TEXT("PZ06");

	InteractionText =
		FText::FromString(TEXT("E - Place Painting"));
}

void ADeadHospitalPaintingFramePuzzle::BeginPlay()
{
	Super::BeginPlay();

	if (RequiredPaintingItemId.IsNone())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: RequiredPaintingItemId is None."),
			*GetName()
		);
	}
}

bool ADeadHospitalPaintingFramePuzzle::PlacePainting(
	AActor* Interactor)
{
	// 이미 그림을 걸었다면 다시 실행하지 않음
	if (bPaintingPlaced)
	{
		return false;
	}

	if (!IsValid(Interactor))
	{
		return false;
	}

	if (RequiredPaintingItemId.IsNone())
	{
		return false;
	}

	// 플레이어의 InventoryComponent 찾기
	UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: InventoryComponent not found."),
			*GetName()
		);

		OnPaintingPlacementFailed();
		return false;
	}

	// 이 액자에 필요한 그림을 가지고 있는지 확인
	if (Inventory->GetItemQuantity(RequiredPaintingItemId) <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: Player does not have %s."),
			*GetName(),
			*RequiredPaintingItemId.ToString()
		);

		OnPaintingPlacementFailed();
		return false;
	}

	// 그림들은 기존 PZ06과 마찬가지로 KeyItem 방식으로 소비
	if (!Inventory->ConsumeKeyItem(
		RequiredPaintingItemId,
		1))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Failed to consume %s."),
			*GetName(),
			*RequiredPaintingItemId.ToString()
		);

		OnPaintingPlacementFailed();
		return false;
	}

	// 이 액자 완료
	bPaintingPlaced = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: Painting placed: %s"),
		*GetName(),
		*RequiredPaintingItemId.ToString()
	);

	// BP에서 숨겨진 그림 Mesh 표시
	OnPaintingPlaced();

	// 3개 액자가 모두 완료됐는지 검사
	CheckAllFramesCompleted(Interactor);

	return true;
}

bool ADeadHospitalPaintingFramePuzzle::IsPaintingPlaced() const
{
	return bPaintingPlaced;
}

void ADeadHospitalPaintingFramePuzzle::CheckAllFramesCompleted(
	AActor* Interactor)
{
	// 자기 자신이 아직 완료되지 않았으면 종료
	if (!bPaintingPlaced)
	{
		return;
	}

	// 연결된 다른 액자 확인
	for (ADeadHospitalPaintingFramePuzzle* Frame : OtherFrames)
	{
		if (!IsValid(Frame))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s: OtherFrames contains invalid frame."),
				*GetName()
			);

			return;
		}

		if (!Frame->IsPaintingPlaced())
		{
			// 아직 그림이 안 걸린 액자가 있음
			return;
		}
	}

	// 자기 자신 + 다른 액자들이 모두 완료됨
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: All PZ06 paintings placed."),
		*GetName()
	);

	// 기존 PuzzleBase의 완료 시스템 사용
	// GameMode에 PZ06 완료 저장 + ConnectedDoor 해제
	if (TryCompletePuzzle(Interactor))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: PZ06 completed."),
			*GetName()
		);
	}
}