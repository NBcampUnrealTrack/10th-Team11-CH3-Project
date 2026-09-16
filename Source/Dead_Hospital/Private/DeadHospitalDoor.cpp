// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalDoor.h"

#include "DeadHospitalGameMode.h"
#include "InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADeadHospitalDoor::ADeadHospitalDoor()
{
	// 생성자: 맵에 놓을 때 Actor의 외형 기본 부품과 E 안내 글자를 준비합니다.
	PrimaryActorTick.bCanEverTick = false;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);

	LockedInteractionText = FText::FromString(TEXT("잠겨 있다"));
	OpenInteractionText = FText::FromString(TEXT("E 키로 문 열기"));
	CloseInteractionText = FText::FromString(TEXT("E 키로 문 닫기"));
}

void ADeadHospitalDoor::BeginPlay()
{
	// 게임 시작에 Details의 시작 잠금을 적용하고, 저장된 잠금 해제 Event가
	// 있으면 그 결과를 우선합니다. 이후 체크포인트 복구 알림도 연결합니다.
	Super::BeginPlay();

	if (DoorId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DoorId is None. Unlock state cannot be restored from a checkpoint."), *GetName());
	}

	if (!RequiredPuzzleId.IsNone() && !RequiredKeyItemId.IsNone())
	{
		// 현재 기획의 문은 "퍼즐로 해제" 또는 "Key로 해제" 중 한 방식만 사용합니다.
		// 두 조건을 동시에 넣으면 어느 조건을 우선해야 하는지 모호해지므로 설정 오류를 명확히 알립니다.
		UE_LOG(LogTemp, Error, TEXT("%s: Set either RequiredPuzzleId or RequiredKeyItemId, not both."), *GetName());
	}

	IsLocked = StartsLocked;
	IsOpen = false;

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		// 잠금 해제 EventId가 체크포인트에 있으면 StartsLocked 값보다 저장 상태를 우선합니다.
		if (GameMode->IsOneTimeEventCompleted(MakeUnlockedEventId()))
		{
			IsLocked = false;
		}

		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalDoor::HandleCheckpointRestored);
	}

	OnDoorStateRestored(IsLocked, IsOpen);
}

void ADeadHospitalDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalDoor::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalDoor::Interact_Implementation(AActor* Interactor)
{
	// 플레이어의 E 입력: 사용 가능 검사 -> (잠겼다면 Key 검사) -> 현재 열림 상태 반전.
	// 이 함수는 문짝을 직접 회전시키지 않고 Blueprint 애니메이션에 목표 상태를 알립니다.
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	if (IsLocked)
	{
		if (!TryUnlockWithKey(Interactor))
		{
			OnDoorAccessDenied();
			return;
		}
	}

	// 잠금이 풀린 문만 열고 닫을 수 있습니다.
	// IsOpen 값을 먼저 바꾼 뒤 Blueprint에 전달하면 애니메이션 중 E 연타가 와도 목표 상태가 명확합니다.
	// !는 bool을 반대로 바꿉니다. 열려 있었다면 false(닫기), 닫혀 있었다면 true(열기).
	IsOpen = !IsOpen;
	OnDoorMovementRequested(IsOpen);
}

bool ADeadHospitalDoor::CanInteract_Implementation(AActor* Interactor) const
{
	// Cast<APawn>은 전달된 Actor가 Pawn인지 확인합니다. 일반 Actor나
	// AI Pawn이 문을 억지로 열지 못하도록 Player가 조종하는 Pawn만 허용합니다.
	if (!IsValid(Interactor) || !CanUseDoorInCurrentPhase())
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	return IsValid(PlayerPawn) && PlayerPawn->IsPlayerControlled();
}

FText ADeadHospitalDoor::GetInteractionText_Implementation() const
{
	if (IsLocked)
	{
		return LockedInteractionText;
	}

	return IsOpen ? CloseInteractionText : OpenInteractionText;
}

bool ADeadHospitalDoor::UnlockFromPuzzle(FName SolvedPuzzleId)
{
	// "이 문을 열 자격이 있는 퍼즐인가"와 "GameMode에 실제로 완료됐나"를
	// 따로 확인합니다. UI가 문을 직접 열었다고 주장하는 것만으로는 열리지 않습니다.
	if (!CanUnlockFromPuzzle(SolvedPuzzleId))
	{
		return false;
	}

	// 이미 잠금이 풀린 문은 원하는 상태이므로 추가 Event를 실행하지 않고 성공으로 봅니다.
	if (!IsLocked)
	{
		return true;
	}

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode) || !GameMode->IsPuzzleCompleted(SolvedPuzzleId))
	{
		return false;
	}

	return UnlockDoor();
}

bool ADeadHospitalDoor::CanUnlockFromPuzzle(FName PuzzleIdToCheck) const
{
	if (PuzzleIdToCheck.IsNone()
		|| RequiredPuzzleId.IsNone()
		|| !RequiredKeyItemId.IsNone()
		|| PuzzleIdToCheck != RequiredPuzzleId)
	{
		return false;
	}

	// 문이 이미 열렸다면 추가 잠금 해제는 필요 없지만 퍼즐 완료 자체는 막지 않습니다.
	return true;
}

bool ADeadHospitalDoor::UnlockDoor()
{
	// 실제 잠금을 풀고 체크포인트용 일회성 Event를 기록하는 내부 함수입니다.
	// GameMode가 연결된 정상 환경에서는 Event 기록을 성공한 뒤 잠금을 해제합니다.
	// GameMode가 없는 경우 현재 코드가 저장 없이 잠금을 해제할 수 있으므로
	// 실제 맵에서는 GameMode Override가 정확히 연결되어 있는지 반드시 확인합니다.
	if (!IsLocked)
	{
		return false;
	}

	// 잠금이 풀린 사실을 체크포인트에 기록하려면 L_MainLevel의 모든 잠긴 문에
	// 고유 DoorId가 있어야 합니다. 비어 있다면 '화면에서는 열렸지만 저장에는 잠김'이 됩니다.
	if (DoorId.IsNone())
	{
		return false;
	}

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		const FName UnlockedEventId = MakeUnlockedEventId();
		if (!UnlockedEventId.IsNone())
		{
			// 체크포인트에 이미 완료로 저장된 문이라면 별도 연출 없이 잠금 상태만 맞춥니다.
			if (GameMode->IsOneTimeEventCompleted(UnlockedEventId))
			{
				IsLocked = false;
				return true;
			}

			// 순간적으로 끝나는 상태 변경도 Start와 Complete를 한 쌍으로 사용합니다.
			// 예약에 실패했는데 문부터 열어 버리면 저장 상태와 실제 문 상태가 달라질 수 있습니다.
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

bool ADeadHospitalDoor::TryUnlockWithKey(AActor* Interactor)
{
	// E 입력으로 잠긴 문에 다가왔을 때 필요한 Key를 Player 인벤토리에서 찾습니다.
	// 이 파일은 팀원 InventoryComponent의 공개 함수만 사용하고 그 파일은 변경하지 않습니다.
	if (RequiredKeyItemId.IsNone() || !RequiredPuzzleId.IsNone() || !IsValid(Interactor))
	{
		return false;
	}

	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!IsValid(Inventory) || Inventory->GetItemQuantity(RequiredKeyItemId) <= 0)
	{
		return false;
	}

	// 기본값은 false이므로 PZ-02 같은 필수 Key는 인벤토리에 남습니다.
	// 별도의 일회용 Key 문에서만 true로 설정하고, 실제 제거가 성공해야 잠금도 해제합니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	const bool IsProtectedKey = IsValid(GameMode) && GameMode->IsProtectedKeyItem(RequiredKeyItemId);

	// 에디터에서 ConsumeKeyWhenUnlocked를 실수로 켜도 보호 목록의 필수 Key는 절대 제거하지 않습니다.
	const bool ShouldConsumeKey = ConsumeKeyWhenUnlocked && !IsProtectedKey;

	// 일회용 Key라면 먼저 인벤토리 슬롯에서 같은 ItemID의 원본 자료를 한 개 찾아 둡니다.
	// 문 잠금 해제 기록에 실패하면 아래에서 AddItem()으로 Key 한 개를 돌려줘야 하기 때문입니다.
	// 여기서는 팀원 InventoryComponent 내부 배열을 직접 수정하지 않고,
	// GetInventorySlots()로 읽고 ConsumeKeyItem()/AddItem() 공개 함수로만 변경합니다.
	FItemData ConsumedKeyItemData;
	if (ShouldConsumeKey)
	{
		bool FoundKeyItemData = false;
		for (const FInventorySlot& Slot : Inventory->GetInventorySlots())
		{
			// 빈 슬롯이거나 이 문에 필요한 Key가 아닌 슬롯은 다음 슬롯을 확인합니다.
			if (Slot.bIsEmpty || Slot.ItemData.ItemID != RequiredKeyItemId)
			{
				continue;
			}

			// 슬롯에는 여러 개가 겹쳐 있을 수 있지만 문 하나가 소비하는 수량은 한 개입니다.
			// 따라서 이름·종류·최대 중첩 수 같은 자료는 복사하고 Quantity만 1로 바꿉니다.
			ConsumedKeyItemData = Slot.ItemData;
			ConsumedKeyItemData.Quantity = 1;
			FoundKeyItemData = true;
			break;
		}

		// 수량 검사는 위에서 통과했더라도 정상적인 KeyItem 자료를 찾지 못했다면
		// 잘못된 인벤토리 자료일 수 있으므로 문을 열지 않고 안전하게 끝냅니다.
		if (!FoundKeyItemData || !Inventory->ConsumeKeyItem(RequiredKeyItemId, 1))
		{
			return false;
		}
	}

	if (UnlockDoor())
	{
		return true;
	}

	// 문 Event 예약/기록에 실패했다면, 잠금이 여전히 유지됩니다.
	// 그때 Key만 사라지면 진행이 막히므로 조금 전에 소비한 Key 한 개를 공개 함수로 돌려줍니다.
	if (ShouldConsumeKey && !Inventory->AddItem(ConsumedKeyItemData))
	{
		// 한 개를 소비했으므로 원래 있던 자리가 반드시 생기는 것이 정상입니다.
		// 그래도 복구가 실패했다면 자료 설정에 문제가 있다는 뜻이므로 로그를 남겨 찾기 쉽게 합니다.
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Door %s failed to return consumed KeyItem %s after unlock failure."),
			*DoorId.ToString(),
			*RequiredKeyItemId.ToString());
	}
	return false;
}

bool ADeadHospitalDoor::CanUseDoorInCurrentPhase() const
{
	// GamePhase는 "지금 게임이 어느 단계인가"를 나타냅니다.
	// Waiting/Ending/GameOver/Clear에서는 늦게 들어온 E 입력으로 문이 변하지 않습니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return false;
	}

	const EDeadHospitalGamePhase Phase = GameMode->GetCurrentGamePhase();
	return Phase == EDeadHospitalGamePhase::Playing
		|| Phase == EDeadHospitalGamePhase::FinalObjective
		|| Phase == EDeadHospitalGamePhase::ReturningToHospital
		|| Phase == EDeadHospitalGamePhase::Escape;
}

FName ADeadHospitalDoor::MakeUnlockedEventId() const
{
	// 같은 이름의 DoorId가 없다면 Event를 구별할 수 없습니다.
	// 예: DoorId=PZ01_Door -> 저장 이름은 PZ01_Door_Unlocked.
	if (DoorId.IsNone())
	{
		return NAME_None;
	}

	return FName(*(DoorId.ToString() + TEXT("_Unlocked")));
}

void ADeadHospitalDoor::HandleCheckpointRestored(FName CheckpointId)
{
	// GameMode에 저장된 잠금 해제 목록을 다시 읽고 문을 닫은 상태로 재배치합니다.
	// Blueprint는 OnDoorStateRestored에서 애니메이션 없는 초기 외형을 맞춥니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return;
	}

	IsLocked = StartsLocked && !GameMode->IsOneTimeEventCompleted(MakeUnlockedEventId());
	IsOpen = false;
	OnDoorStateRestored(IsLocked, IsOpen);
}
