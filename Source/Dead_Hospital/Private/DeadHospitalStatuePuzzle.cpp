#include "DeadHospitalStatuePuzzle.h"

#include "InventoryComponent.h"
#include "DeadHospitalDoor.h"

ADeadHospitalStatuePuzzle::ADeadHospitalStatuePuzzle()
{
	PrimaryActorTick.bCanEverTick = false;

	// PZ04 전용 퍼즐
	PuzzleId = TEXT("PZ04");

	InteractionText = FText::FromString(TEXT("E to interact"));
}

void ADeadHospitalStatuePuzzle::BeginPlay()
{
	Super::BeginPlay();

	if (AngelHeadItemId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s: AngelHeadItemId is None."),
			*GetName());
	}

	if (DemonHeadItemId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s: DemonHeadItemId is None."),
			*GetName());
	}

	if (!IsValid(ConnectedDoor))
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s: ConnectedDoor is not assigned."),
			*GetName());
	}
}

// ---------------------------------------------------------
// 천사 머리 배치
// ---------------------------------------------------------
bool ADeadHospitalStatuePuzzle::PlaceAngelHead(AActor* Interactor)
{
	return PlaceHead(
		Interactor,
		AngelHeadItemId,
		bAngelHeadPlaced,
		true
	);
}

// ---------------------------------------------------------
// 악마 머리 배치
// ---------------------------------------------------------
bool ADeadHospitalStatuePuzzle::PlaceDemonHead(AActor* Interactor)
{
	return PlaceHead(
		Interactor,
		DemonHeadItemId,
		bDemonHeadPlaced,
		false
	);
}

// ---------------------------------------------------------
// 실제 머리 배치 처리
// ---------------------------------------------------------
bool ADeadHospitalStatuePuzzle::PlaceHead(
	AActor* Interactor,
	FName RequiredItemId,
	bool& bPlacedState,
	bool bIsAngel)
{
	// 이미 머리를 배치했다면 다시 사용할 수 없음
	if (bPlacedState)
	{
		return false;
	}

	// 상호작용 대상 확인
	if (!IsValid(Interactor))
	{
		return false;
	}

	// 플레이어의 InventoryComponent 찾기
	UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s: InventoryComponent not found."),
			*GetName());

		return false;
	}

	// 필요한 머리를 가지고 있는지 확인
	if (Inventory->GetItemQuantity(RequiredItemId) < 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Player does not have %s."),
			*GetName(),
			*RequiredItemId.ToString());

		return false;
	}

	// 머리 아이템 1개 소비
	if (!Inventory->ConsumeKeyItem(RequiredItemId, 1))
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s: Failed to consume %s."),
			*GetName(),
			*RequiredItemId.ToString());

		return false;
	}

	// 해당 석상의 머리가 배치됐다고 저장
	bPlacedState = true;

	// BP에서 실제 머리 Mesh를 보이게 할 이벤트
	if (bIsAngel)
	{
		OnAngelHeadPlaced();
	}
	else
	{
		OnDemonHeadPlaced();
	}

	// 두 석상이 모두 완성됐는지 확인
	CheckPuzzleCompletion(Interactor);

	return true;
}

// ---------------------------------------------------------
// 천사 + 악마가 모두 완성됐는지 확인
// ---------------------------------------------------------
void ADeadHospitalStatuePuzzle::CheckPuzzleCompletion(AActor* Interactor){

	if (!IsValid(OtherStatue)){

		UE_LOG(LogTemp, Error, TEXT("%s: OtherStatue is not assigned."), *GetName());

		return;
	}

	// 나 + 상대 석상의 상태를 같이 확인
	const bool bAngelComplete =
		bAngelHeadPlaced || OtherStatue->IsAngelHeadPlaced();

	const bool bDemonComplete =
		bDemonHeadPlaced || OtherStatue->IsDemonHeadPlaced();

	// 둘 다 완성되지 않았으면 아직 퍼즐 완료 X
	if (!bAngelComplete || !bDemonComplete)
	{
		return;
	}

	// 둘 다 완성 → PZ04 완료 → 문 잠금 해제
	if (TryCompletePuzzle(Interactor))
	{
		OnBothStatuesCompleted();

		UE_LOG(LogTemp, Warning,
			TEXT("%s: PZ04 Statue Puzzle Completed."),
			*GetName());
	}
}

bool ADeadHospitalStatuePuzzle::IsAngelHeadPlaced() const
{
	return bAngelHeadPlaced;
}

bool ADeadHospitalStatuePuzzle::IsDemonHeadPlaced() const
{
	return bDemonHeadPlaced;
}