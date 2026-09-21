// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ItemData.h"
#include "DeadHospitalProgressionItem.generated.h"

class UStaticMeshComponent;

/**
 * 일반 파밍 아이템이 아니라 "획득 여부가 스토리 진행에 영향을 주는 아이템"입니다.
 * 예: PZ03 그림을 떼어낸 뒤 나타나는 CardKeyA, PZ02 해결 뒤 받는 매그넘, PZ05의 CardKeyB.
 * 게임 중 보이는 Actor와 Player 인벤토리의 ItemData는 다릅니다. E를 눌러
 * InventoryComponent::AddItem이 성공하고 GameMode에 획득 Event가 저장되면
 * 맵 Actor를 숨깁니다. 체크포인트를 불러오면 저장 시점에 집었는지 다시 맞춥니다.
 * ItemID와 PickupEventId가 비어 있으면 정상 획득/중복 복구를 할 수 없습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalProgressionItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalProgressionItem();

	/**
	 * Blueprint가 아이템을 공개/숨기려고 호출합니다. true라고 해도 PZ03 그림 제거
	 * Event나 PZ02 매그넘 금고처럼 지정된 요구 조건이 완료되지 않았다면 공개되지 않습니다.
	 * 즉 화면 연출이 진행 조건을 잘못 앞질러도 필수 아이템을 미리 얻을 수 없습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Progression Item")
	void SetPickupEnabled(bool ShouldEnable);

	UFUNCTION(BlueprintPure, Category = "Progression Item")
	bool HasBeenCollected() const { return ItemCollected; }

	/**
	 * PZ03 그림 Actor가 연결된 CardKeyA를 안전하게 설정할 때 사용하는 C++ 전용 함수입니다.
	 * Item ID, 그림 제거 Event, Key 획득 완료 Puzzle ID를 맞춥니다.
	 * 단, 이 아이템 Actor 자신의 PickupEventId는 L_MainLevel Details에서
	 * 다른 아이템과 겹치지 않는 이름(예: PZ02_KeyCollected)으로 지정해야 합니다.
	 */
	void ConfigureAsHiddenPuzzleKey(
		FName HiddenKeyItemId,
		FName RevealEventId,
		FName CompletedPuzzleId
	);

	/**
	 * PZ02 매그넘이나 PZ05 CardKeyB처럼 퍼즐 완료 뒤 나타나는 고유 보상을 설정하는 C++ 전용 함수입니다.
	 * 퍼즐 정답 방식과 보상 지급을 분리하므로 다른 보상 퍼즐에서도 이 Actor를 재사용할 수 있습니다.
	 */
	void ConfigureAsPuzzleReward(
		FName RequiredCompletedPuzzleId,
		FName RewardItemId,
		EItemType RewardItemType,
		FName RewardPickupEventId,
		FName CompletedSubObjectiveId = NAME_None
	);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression Item")
	UStaticMeshComponent* ItemMesh;

	/**
	 * Player 인벤토리에 넣을 실제 데이터. ItemID가 비어 있거나 수량/스택이 0이면
	 * E로 획득할 수 없습니다. 그림 Key/매그넘은 연결 퍼즐이 대부분 값을 설정합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item")
	FItemData ItemData;

	/** 이 퍼즐이 완료되기 전에는 아이템을 얻을 수 없습니다. 필요 없으면 NAME_None으로 둡니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Requirement")
	FName RequiredPuzzleId = NAME_None;

	/** 그림 제거처럼 이 일회성 이벤트가 끝나기 전에는 아이템을 얻을 수 없습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Requirement")
	FName RequiredCompletedEventId = NAME_None;

	/** PZ03 CardKeyA처럼 "획득하는 순간 해결"할 때만 퍼즐 ID를 넣습니다. 매그넘과 CardKeyB는 None입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Result")
	FName PuzzleIdCompletedByPickup = NAME_None;

	/**
	 * 아이템 Actor마다 다른 ID입니다. 획득 기록을 체크포인트에 저장해 같은 물건을
	 * 반복 지급하지 않게 합니다. PZ03 CardKeyA Actor도 다른 아이템과 겹치지 않는 ID를 지정해야 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Result")
	FName PickupEventId = NAME_None;

	/**
	 * 이 아이템 획득이 S01~S09 서브 목표의 숫자를 올려야 할 때만 ID를 설정합니다.
	 * 예를 들어 나머지 수집용 그림 Actor에 S03을 넣으면 획득 성공 후 그림 수집 진행도가 올라갑니다.
	 * None으로 두면 인벤토리와 Event만 처리하고 HUD 목표는 건드리지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Objective")
	FName SubObjectiveId = NAME_None;

	/** SubObjectiveId가 있을 때 HUD에 표시할 설명입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Objective")
	FText SubObjectiveText;

	/** 이 아이템 하나를 주웠을 때 더할 진행도입니다. 보통은 1을 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Objective", meta = (ClampMin = "1"))
	int32 SubObjectiveProgressToAdd = 1;

	/** 목표의 총 개수입니다. 그림 3점 수집이면 3을 입력합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Objective", meta = (ClampMin = "1"))
	int32 SubObjectiveTargetProgress = 1;

	/**
	 * 이 아이템을 실제로 얻는 순간 완료되어 HUD에서 제거할 서브 목표 ID입니다.
	 * 예: PZ03 CardKeyA는 S04, PZ05 CardKeyB는 S07, PZ02 Magnum은 단서 목표 S02를 끝낼 수 있습니다.
	 * None이면 어떤 서브 목표도 제거하지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item|Objective")
	FName SubObjectiveIdToClearOnPickup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression Item")
	bool StartsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	UFUNCTION(BlueprintImplementableEvent, Category = "Progression Item")
	void OnItemCollected(const FItemData& CollectedItemData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Progression Item")
	void OnItemCollectionFailed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Progression Item")
	void OnItemStateRestored(bool WasCollected);

private:
	bool AreRequirementsSatisfied() const;
	void RefreshStateFromGameMode();
	void ApplyVisibleState();

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	UFUNCTION()
	void HandlePuzzleCompleted(FName CompletedPuzzleId);

	UFUNCTION()
	void HandleOneTimeEventCompleted(FName CompletedEventId);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression Item", meta = (AllowPrivateAccess = "true"))
	bool PickupEnabled = true;

	/** GameMode 완료 목록과 대조한 현재 획득 여부입니다. true면 다시 E로 얻을 수 없습니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression Item", meta = (AllowPrivateAccess = "true"))
	bool ItemCollected = false;
};
