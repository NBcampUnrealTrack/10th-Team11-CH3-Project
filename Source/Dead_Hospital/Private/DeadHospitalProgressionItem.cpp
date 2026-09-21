// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalProgressionItem.h"

#include "DeadHospitalGameMode.h"
#include "InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADeadHospitalProgressionItem::ADeadHospitalProgressionItem()
{
	// 생성자는 Actor가 처음 만들어질 때 기본 외형과 안내 글자를 설정합니다.
	// 아이템 ID/획득 Event ID는 연결 퍼즐 또는 L_MainLevel Details에서 따로 지정합니다.
	PrimaryActorTick.bCanEverTick = false;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);

	InteractionText = FText::FromString(TEXT("E 키로 아이템 획득"));
}

void ADeadHospitalProgressionItem::ConfigureAsHiddenPuzzleKey(
	FName HiddenKeyItemId,
	FName RevealEventId,
	FName CompletedPuzzleId)
{
	// PZ03의 카드키 A는 "그림이 제거되어야 보이고, 카드키를 집어야 퍼즐 완료"입니다.
	// 세 값을 따로 수동 설정하면 엇갈리기 쉬워 PaintingPuzzle이 이 함수를 호출합니다.
	// 주의: PickupEventId는 이 함수에서 설정하지 않으므로 열쇠 Actor에 따로 입력합니다.
	// 그림과 연결된 Key는 일반 파밍 아이템이 아니라 진행 필수 아이템입니다.
	// 잘못된 ItemType, 수량, Stack 설정 때문에 획득이나 보호 로직이 실패하지 않도록 고정합니다.
	ItemData.ItemID = HiddenKeyItemId;
	ItemData.ItemType = EItemType::KeyItem;
	ItemData.Quantity = 1;
	ItemData.MaxStack = 1;
	RequiredPuzzleId = NAME_None;
	RequiredCompletedEventId = RevealEventId;
	PuzzleIdCompletedByPickup = CompletedPuzzleId;
	SubObjectiveIdToClearOnPickup = TEXT("S04");
	StartsEnabled = false;

	// BeginPlay 뒤에 Painting Actor가 설정한 경우에도 화면 상태를 즉시 다시 계산합니다.
	// Actor 생성 순서에 따라 그림이 열쇠보다 늦게 BeginPlay할 수 있습니다.
	// 이미 시작했다면 변경된 조건을 기다리지 말고 화면에 즉시 반영합니다.
	if (HasActorBegunPlay())
	{
		RefreshStateFromGameMode();
		ApplyVisibleState();
	}
}

void ADeadHospitalProgressionItem::ConfigureAsPuzzleReward(
	FName RequiredCompletedPuzzleId,
	FName RewardItemId,
	EItemType RewardItemType,
	FName RewardPickupEventId,
	FName CompletedSubObjectiveId)
{
	// 매그넘처럼 "퍼즐을 먼저 풀고 나서 보상이 나타나는" 경우입니다.
	// 획득을 퍼즐 완료로 다시 처리하지 않으므로 PuzzleIdCompletedByPickup은 None입니다.
	ItemData.ItemID = RewardItemId;
	ItemData.ItemType = RewardItemType;
	ItemData.Quantity = 1;
	ItemData.MaxStack = 1;
	RequiredPuzzleId = RequiredCompletedPuzzleId;
	RequiredCompletedEventId = NAME_None;
	PuzzleIdCompletedByPickup = NAME_None;
	PickupEventId = RewardPickupEventId;
	SubObjectiveIdToClearOnPickup = CompletedSubObjectiveId;
	StartsEnabled = false;

	if (HasActorBegunPlay())
	{
		RefreshStateFromGameMode();
		ApplyVisibleState();
	}
}

void ADeadHospitalProgressionItem::BeginPlay()
{
	// 게임 시작 시 GameMode의 퍼즐 완료/그림 제거/체크포인트 복구 알림을 듣습니다.
	// 아이템은 자신의 상태만 기억하지 않고 GameMode 저장 목록으로 상태를 맞춥니다.
	Super::BeginPlay();

	PickupEnabled = StartsEnabled;

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalProgressionItem::HandleCheckpointRestored);
		GameMode->OnPuzzleCompleted.AddDynamic(this, &ADeadHospitalProgressionItem::HandlePuzzleCompleted);
		GameMode->OnOneTimeEventCompleted.AddDynamic(this, &ADeadHospitalProgressionItem::HandleOneTimeEventCompleted);
	}

	if (PickupEventId.IsNone() || ItemData.ItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PickupEventId and ItemData.ItemID must be set."), *GetName());
	}

	RefreshStateFromGameMode();
	ApplyVisibleState();
	OnItemStateRestored(ItemCollected);
}

void ADeadHospitalProgressionItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 게임이 끝날 때 등록한 알림을 해제합니다. AddDynamic의 반대가 RemoveDynamic입니다.
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalProgressionItem::HandleCheckpointRestored);
		GameMode->OnPuzzleCompleted.RemoveDynamic(this, &ADeadHospitalProgressionItem::HandlePuzzleCompleted);
		GameMode->OnOneTimeEventCompleted.RemoveDynamic(this, &ADeadHospitalProgressionItem::HandleOneTimeEventCompleted);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalProgressionItem::Interact_Implementation(AActor* Interactor)
{
	// E 획득 순서: Player/퍼즐 조건 확인 -> Player 인벤토리 찾기 -> Event 예약 ->
	// AddItem -> 저장 목록 완료 기록 -> 맵 Actor 숨김 -> Blueprint 획득 연출.
	// 실패하면 아이템이나 예약을 되돌려 "아이템만 얻었는데 완료 기록이 없는" 상황을 줄입니다.
	if (!CanInteract_Implementation(Interactor))
	{
		OnItemCollectionFailed();
		return;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!IsValid(GameMode) || !IsValid(Inventory))
	{
		// Player C++에 InventoryComponent가 아직 연결되지 않은 동안에는 여기서 안전하게 실패합니다.
		// 팀원의 Player 파일을 수정하지 않고, 해당 연결이 완료된 뒤 자동으로 정상 동작합니다.
		OnItemCollectionFailed();
		return;
	}

	if (!GameMode->TryStartOneTimeEvent(PickupEventId))
	{
		// 이미 집었거나 같은 ID의 획득이 진행 중이면 중복 지급을 막습니다.
		OnItemCollectionFailed();
		return;
	}

	// ItemID를 이용해 DT_ItemData에서 최신 아이템 정보를 가져와 인벤토리에 추가
	if (!Inventory->AddItemByID(ItemData.ItemID, ItemData.Quantity))
	{
		// 인벤토리에 넣지 못했다면 Event 예약을 취소해야 다음 E 입력에서 다시 시도할 수 있습니다.
		GameMode->CancelOneTimeEvent(PickupEventId);
		OnItemCollectionFailed();
		return;
	}

	// CardKeyA 획득과 PZ03 완료를 함께 저장합니다. 매그넘처럼 퍼즐을 먼저 풀고
	// 보상만 획득하는 아이템은 Event 하나만 저장합니다.
	// ? : 는 조건이 true일 때 앞 함수, false일 때 뒤 함수를 선택합니다.
	// PZ03 CardKeyA는 아이템 Event와 퍼즐 완료를 함께 저장하고,
	// 일반 보상은 아이템 Event만 저장합니다.
	const bool WasProgressRecorded = PuzzleIdCompletedByPickup.IsNone()
		? GameMode->CompleteOneTimeEvent(PickupEventId)
		: GameMode->CompletePickupEventAndPuzzle(PickupEventId, PuzzleIdCompletedByPickup);
	if (!WasProgressRecorded)
	{
		// 기록에 실패하면 인벤토리에 방금 넣은 물건도 취소하고 다시 E를 누를 수 있게 합니다.
		// Key와 Painting은 일반 삭제가 금지되어 RemoveItem()이 거절합니다. 이 경우에만
		// ConsumeKeyItem()으로 실패한 지급을 되돌리고, Magnum 같은 일반 보상은 RemoveItem()을 사용합니다.
		if (Inventory->IsProtectedItem(ItemData.ItemID))
		{
			Inventory->ConsumeKeyItem(ItemData.ItemID, ItemData.Quantity);
		}
		else
		{
			Inventory->RemoveItem(ItemData.ItemID, ItemData.Quantity);
		}
		GameMode->CancelOneTimeEvent(PickupEventId);
		OnItemCollectionFailed();
		return;
	}

	// 아이템 획득과 Event 저장이 모두 성공한 뒤에만 선택적인 서브 목표 숫자를 올립니다.
	// 앞에서 올리면 인벤토리가 가득 차 획득에 실패해도 HUD만 1/3으로 올라가는 오류가 생길 수 있습니다.
	if (!SubObjectiveId.IsNone())
	{
		const bool ObjectiveUpdated = GameMode->AdvanceSubObjectiveProgress(
			SubObjectiveId,
			SubObjectiveText,
			FMath::Max(SubObjectiveProgressToAdd, 1),
			FMath::Max(SubObjectiveTargetProgress, 1));

		if (!ObjectiveUpdated)
		{
			// 아이템은 이미 정상 획득되었으므로 돌려놓지 않습니다.
			// 다른 ID의 서브 목표가 활성화된 연결 오류일 수 있으므로 Output Log에 알려 맵 설정을 찾을 수 있게 합니다.
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s: Item was collected, but SubObjective %s could not be updated."),
				*GetName(),
				*SubObjectiveId.ToString());
		}
	}

	// 획득으로 완료되는 단기 서브 목표가 설정돼 있다면 해당 ID 하나만 제거합니다.
	// S03처럼 동시에 유지 중인 다른 목표는 ClearSubObjectiveById가 건드리지 않습니다.
	if (!SubObjectiveIdToClearOnPickup.IsNone())
	{
		GameMode->ClearSubObjectiveById(SubObjectiveIdToClearOnPickup);
	}

	ItemCollected = true;
	PickupEnabled = false;

	ApplyVisibleState();
	OnItemCollected(ItemData);
}

bool ADeadHospitalProgressionItem::CanInteract_Implementation(AActor* Interactor) const
{
	// ||는 어느 조건 하나라도 틀리면 false입니다. 이미 얻은 아이템,
	// 빈 ItemID/EventID, 0 수량은 인벤토리에 넣지 않습니다.
	if (!PickupEnabled
		|| ItemCollected
		|| !IsValid(Interactor)
		|| PickupEventId.IsNone()
		|| ItemData.ItemID.IsNone()
		|| ItemData.Quantity <= 0
		|| ItemData.MaxStack <= 0)
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	// Cast는 Actor를 Pawn으로 다룰 수 있는지 검사합니다. AI나 다른 Actor 대신
	// 실제 Player가 조종하는 Pawn만 아이템을 획득할 수 있습니다.
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return false;
	}

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	return IsValid(GameMode)
		&& GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing
		&& AreRequirementsSatisfied();
}

FText ADeadHospitalProgressionItem::GetInteractionText_Implementation() const
{
	return InteractionText;
}

void ADeadHospitalProgressionItem::SetPickupEnabled(bool ShouldEnable)
{
	// Blueprint가 실수로 먼저 활성화해도 퍼즐/Event 조건을 건너뛰어 보상이 나타나지 않게 합니다.
	PickupEnabled = ShouldEnable && !ItemCollected && AreRequirementsSatisfied();
	ApplyVisibleState();
}

bool ADeadHospitalProgressionItem::AreRequirementsSatisfied() const
{
	// 둘 중 지정된 조건이 있다면 GameMode에 실제 완료 기록이 있는지 물어봅니다.
	// RequiredPuzzleId는 매그넘, RequiredCompletedEventId는 그림 뒤 Key에 쓰입니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return false;
	}

	if (!RequiredPuzzleId.IsNone() && !GameMode->IsPuzzleCompleted(RequiredPuzzleId))
	{
		return false;
	}

	if (!RequiredCompletedEventId.IsNone() && !GameMode->IsOneTimeEventCompleted(RequiredCompletedEventId))
	{
		return false;
	}

	return true;
}

void ADeadHospitalProgressionItem::ApplyVisibleState()
{
	// 숨김과 충돌을 함께 바꿉니다. 안 보이는 Key가 여전히 E 입력을 받지 않게 하기 위해서입니다.
	// !(느낌표)는 true/false를 반대로 바꿉니다.
	const bool ShouldBeVisible = PickupEnabled && !ItemCollected;
	SetActorHiddenInGame(!ShouldBeVisible);
	SetActorEnableCollision(ShouldBeVisible);
}

void ADeadHospitalProgressionItem::HandleCheckpointRestored(FName CheckpointId)
{
	RefreshStateFromGameMode();
	ApplyVisibleState();
	OnItemStateRestored(ItemCollected);
}

void ADeadHospitalProgressionItem::HandlePuzzleCompleted(FName CompletedPuzzleId)
{
	// 다른 퍼즐이 완료됐다는 알림에는 반응하지 않습니다. 내 보상의 필요 퍼즐 ID와
	// 같은 경우에만 상태를 다시 계산해 해당 아이템을 공개합니다.
	if (CompletedPuzzleId != RequiredPuzzleId)
	{
		return;
	}

	RefreshStateFromGameMode();
	ApplyVisibleState();
}

void ADeadHospitalProgressionItem::HandleOneTimeEventCompleted(FName CompletedEventId)
{
	if (CompletedEventId != RequiredCompletedEventId)
	{
		return;
	}

	RefreshStateFromGameMode();
	ApplyVisibleState();
}

void ADeadHospitalProgressionItem::RefreshStateFromGameMode()
{
	// 체크포인트 재시작/퍼즐 해결/그림 제거 때 공통으로 호출하는 계산 함수입니다.
	// 저장된 PickupEventId가 완료됐다면 이미 집은 물건이므로 다시 나타나지 않습니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		PickupEnabled = false;
		return;
	}

	ItemCollected = GameMode->IsOneTimeEventCompleted(PickupEventId);

	const bool HasProgressRequirement = !RequiredPuzzleId.IsNone() || !RequiredCompletedEventId.IsNone();

	// 조건이 지정된 중요 아이템은 StartsEnabled 값보다 조건 충족 여부를 우선합니다.
	// 따라서 Magnum이나 숨겨진 Key를 실수로 StartsEnabled=true로 두어도 퍼즐 전에 나타나지 않습니다.
	PickupEnabled = !ItemCollected
		&& (HasProgressRequirement ? AreRequirementsSatisfied() : StartsEnabled);
}
