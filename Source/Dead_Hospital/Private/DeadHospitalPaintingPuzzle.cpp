// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalPaintingPuzzle.h"

#include "DeadHospitalGameMode.h"
#include "DeadHospitalProgressionItem.h"
#include "InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADeadHospitalPaintingPuzzle::ADeadHospitalPaintingPuzzle()
{
	// Actor 생성 시 기본 그림 아이템 데이터와 E 안내 글자를 만듭니다.
	// 그림을 KeyItem으로 지정했지만 숨겨진 Key의 ItemID와는 전혀 다른 항목입니다.
	PrimaryActorTick.bCanEverTick = false;

	PaintingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaintingMesh"));
	SetRootComponent(PaintingMesh);

	PaintingItemData.ItemID = TEXT("PZ02_MiddleAgedManPainting");
	PaintingItemData.ItemName = FText::FromString(TEXT("열쇠를 든 중년 남자 그림"));
	PaintingItemData.ItemType = EItemType::KeyItem;
	PaintingItemData.Quantity = 1;
	PaintingItemData.MaxStack = 1;
	InteractionText = FText::FromString(TEXT("E 키로 그림 조사"));
}

void ADeadHospitalPaintingPuzzle::BeginPlay()
{
	// 게임이 시작되면 그림과 숨겨진 열쇠의 연결을 확인하고, GameMode 저장 상태를
	// 읽어 이미 그림을 뗀 체크포인트에서 다시 보이지 않도록 맞춥니다.
	Super::BeginPlay();

	if (!IsValid(HiddenKeyPickup))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: HiddenKeyPickup is not assigned. PZ-02 cannot be completed."), *GetName());
	}
	else
	{
		// 그림과 Key Actor의 BeginPlay 순서가 어느 쪽이 먼저든 같은 설정이 적용됩니다.
		// 그림 제거 전에는 Key가 숨겨지고, Key 획득 순간에만 PZ-02 완료가 기록됩니다.
		HiddenKeyPickup->ConfigureAsHiddenPuzzleKey(
			HiddenKeyItemId,
			PaintingRemovedEventId,
			PuzzleCompletionId
		);
	}

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		PaintingRemoved = GameMode->IsOneTimeEventCompleted(PaintingRemovedEventId);
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalPaintingPuzzle::HandleCheckpointRestored);
	}

	ApplyRemovedState();
	OnPaintingStateRestored(PaintingRemoved);
}

void ADeadHospitalPaintingPuzzle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalPaintingPuzzle::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalPaintingPuzzle::Interact_Implementation(AActor* Interactor)
{
	// 실행 순서: 게임 중 Player인지 -> 뒤쪽 열쇠 Actor가 연결됐는지 ->
	// 인벤토리를 찾는지 -> Event 예약 -> 그림 아이템 추가 -> Event 저장 -> 그림 숨김.
	// 어떤 단계라도 실패하면 다음 E 입력에서 다시 시도할 수 있도록 앞선 예약을 취소합니다.
	if (!CanInteract_Implementation(Interactor))
	{
		OnPaintingRemovalFailed();
		return;
	}

	// Key Actor가 없는데 그림부터 없애면 플레이어가 필수 Key를 영원히 얻지 못합니다.
	// 따라서 아이템 지급과 상태 저장보다 먼저 필수 연결을 검사합니다.
	if (!IsValid(HiddenKeyPickup))
	{
		OnPaintingRemovalFailed();
		return;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	// FindComponentByClass는 "이 Player Actor에 인벤토리 부품이 붙어 있나"를 찾습니다.
	// Player 팀 코드가 아직 인벤토리를 연결하지 않았다면 안전하게 실패합니다.
	if (!IsValid(GameMode) || !IsValid(Inventory))
	{
		OnPaintingRemovalFailed();
		return;
	}

	// 먼저 EventId를 예약하면 같은 프레임에 E 입력이 여러 번 들어와도 그림 아이템을 한 번만 지급합니다.
	if (!GameMode->TryStartOneTimeEvent(PaintingRemovedEventId))
	{
		OnPaintingRemovalFailed();
		return;
	}

	if (!Inventory->AddItem(PaintingItemData))
	{
		GameMode->CancelOneTimeEvent(PaintingRemovedEventId);
		OnPaintingRemovalFailed();
		return;
	}

	// Event 완료가 실패했는데 그림만 사라지면 숨겨진 Key를 다시 찾을 수 없습니다.
	// 따라서 실제 기록에 성공한 뒤에만 그림을 제거하고, 실패하면 지급한 그림도 취소합니다.
	if (!GameMode->CompleteOneTimeEvent(PaintingRemovedEventId))
	{
		Inventory->RemoveItem(PaintingItemData.ItemID, PaintingItemData.Quantity);
		GameMode->CancelOneTimeEvent(PaintingRemovedEventId);
		OnPaintingRemovalFailed();
		return;
	}

	PaintingRemoved = true;
	ApplyRemovedState();
	OnPaintingRemoved();
}

bool ADeadHospitalPaintingPuzzle::CanInteract_Implementation(AActor* Interactor) const
{
	// 이미 떼어낸 그림, 빈 Event ID, 잘못 전달된 대상이면 다시 조사할 수 없습니다.
	if (PaintingRemoved || PaintingRemovedEventId.IsNone() || !IsValid(Interactor))
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	return IsValid(PlayerPawn)
		&& PlayerPawn->IsPlayerControlled()
		&& IsValid(GameMode)
		&& GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing;
}

FText ADeadHospitalPaintingPuzzle::GetInteractionText_Implementation() const
{
	return InteractionText;
}

void ADeadHospitalPaintingPuzzle::ApplyRemovedState()
{
	// true면 그림은 보이지 않고 충돌도 없어져 더는 E 입력 대상이 아닙니다.
	// 열쇠 Actor는 그림 제거 Event가 실제 완료됐을 때만 공개되도록 그 Actor가 재검사합니다.
	SetActorHiddenInGame(PaintingRemoved);
	SetActorEnableCollision(!PaintingRemoved);

	if (IsValid(HiddenKeyPickup))
	{
		HiddenKeyPickup->SetPickupEnabled(PaintingRemoved);
	}
}

void ADeadHospitalPaintingPuzzle::HandleCheckpointRestored(FName CheckpointId)
{
	// 체크포인트 재시작 시 그림 제거 전으로 돌아왔다면 그림도 다시 나타나야 합니다.
	// 화면 상태를 자체 기억값이 아닌 GameMode가 복구한 Event 목록으로 다시 계산합니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return;
	}

	PaintingRemoved = GameMode->IsOneTimeEventCompleted(PaintingRemovedEventId);
	ApplyRemovedState();
	OnPaintingStateRestored(PaintingRemoved);
}
