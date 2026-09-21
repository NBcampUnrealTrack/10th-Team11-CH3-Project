// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "ItemData.h"
#include "DeadHospitalItemPlacementPuzzle.generated.h"

class UInventoryComponent;

/**
 * 여러 중요 아이템을 올바른 자리에 놓아 해결하는 퍼즐의 공통 Actor입니다.
 * 최신 GDD의 다음 퍼즐들이 같은 원리로 동작하므로 코드를 세 번 복사하지 않고 이 클래스 하나를 재사용합니다.
 *
 * - PZ04: 천사 머리와 악마 머리를 각 받침대에 배치하고, 성공하면 두 머리를 소비합니다.
 * - PZ06: 수집한 그림 세 점을 지정된 세 위치에 배치하고, 성공하면 그림들을 소비합니다.
 * - PZ07: 조합한 MasterCardKey를 확인하고 특수중환자격리실을 엽니다. 이 경우에는 소비 여부를 끌 수 있습니다.
 *
 * 이 클래스는 아이템 팀의 InventoryComponent 내부 배열을 직접 수정하지 않습니다.
 * 공개 함수 GetItemQuantity(), GetInventorySlots(), ConsumeKeyItem(), AddItem()만 호출합니다.
 * 따라서 팀원이 인벤토리 내부 구현을 바꾸더라도 공개 함수 약속이 유지되면 퍼즐 코드는 함께 사용할 수 있습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalItemPlacementPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalItemPlacementPuzzle();

	/**
	 * UI의 각 슬롯에 놓인 ItemID 배열을 한 번에 전달합니다.
	 * RequireExactOrder가 true이면 배열의 0번, 1번, 2번 위치까지 RequiredItemIds와 정확히 같아야 합니다.
	 * 예를 들어 RequiredItemIds가 [천사머리, 악마머리]인데 제출 배열이 [악마머리, 천사머리]이면 오답입니다.
	 * false이면 위치 순서는 무시하지만 필요한 아이템 종류와 개수는 정확히 같아야 합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Item Placement")
	bool SubmitItemLayout(AActor* Interactor, const TArray<FName>& SubmittedItemIds);

	/**
	 * 슬롯 순서가 없는 퍼즐에서 현재 인벤토리 보유 여부만 확인해 해결합니다.
	 * PZ07처럼 MasterCardKey 한 개만 확인하는 경우 이 함수를 Blueprint 확인 버튼에 연결할 수 있습니다.
	 */
	virtual bool TryCompletePuzzle(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual bool CanCompletePuzzle(AActor* Interactor) const override;

	/**
	 * 퍼즐 해결에 필요한 ItemID 목록입니다. 같은 ID가 두 번 들어가면 그 아이템 두 개가 필요합니다.
	 * 배열의 순서는 SubmitItemLayout에서 사용하는 슬롯 순서이기도 합니다.
	 * 현재 아이템 팀 DataTable에 머리 두 개와 나머지 그림 두 개의 최종 Row 이름이 없으므로
	 * 여기서 이름을 임의로 만들지 않았습니다. 팀이 정한 최종 ID를 Details에 정확히 입력해야 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Item Placement")
	TArray<FName> RequiredItemIds;

	/** true면 제출 배열의 위치 순서까지 검사하고, false면 아이템 종류와 개수만 검사합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Item Placement")
	bool RequireExactOrder = true;

	/**
	 * true면 퍼즐 성공 순간 필요한 KeyItem을 인벤토리에서 제거합니다.
	 * PZ04의 머리와 PZ06의 배치 그림에는 true를 사용합니다.
	 * PZ07의 MasterCardKey를 계속 보유시키려면 해당 Actor에서 false로 설정합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Item Placement")
	bool ConsumeItemsOnSuccess = true;

	/** true면 ConnectedDoor를 반드시 연결해야 퍼즐 완료를 허용합니다. 세 퍼즐 모두 문을 여는 용도라 기본값은 true입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Item Placement")
	bool RequireConnectedDoor = true;

	/**
	 * 성공할 때 완료 처리하여 HUD에서 제거할 서브 목표 ID 목록입니다.
	 * PZ04에는 S05와 S06, PZ06에는 S03과 S08처럼 해당 퍼즐이 끝내는 목표를 Details에서 넣습니다.
	 * 목록이 비어 있으면 목표를 지우지 않으며, 다른 활성 서브 목표는 그대로 유지됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Item Placement|Objective")
	TArray<FName> SubObjectiveIdsToClearOnSuccess;

	/** 정답 배열과 아이템 소비, GameMode 완료 저장, 문 해제가 모두 성공한 뒤 연출을 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Item Placement")
	void OnItemLayoutAccepted();

	/** 배열 순서가 틀리거나 아이템이 부족하거나 문 연결이 잘못된 경우 실패 UI/효과음을 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Item Placement")
	void OnItemLayoutRejected();

private:
	/** 제출된 배열이 필요한 배열과 같은 종류·수량인지 검사합니다. */
	bool DoesSubmittedLayoutMatch(const TArray<FName>& SubmittedItemIds) const;

	/** 필요한 아이템을 모두 KeyItem 방식으로 소비하고, 중간 실패 시 앞에서 소비한 아이템을 되돌립니다. */
	bool ConsumeRequiredItems(UInventoryComponent* Inventory);
	/** PendingConsumedItems에 기억한 아이템을 Inventory에 다시 넣고 임시 목록을 비웁니다. */
	void RestorePendingConsumedItems(UInventoryComponent* Inventory);

	/**
	 * 아이템을 먼저 소비한 뒤 부모 완료 함수가 다시 CanCompletePuzzle을 호출할 때만 true가 됩니다.
	 * 이 표시가 없으면 이미 소비한 아이템이 없다고 판단해 자기 자신이 완료를 거절하게 됩니다.
	 */
	bool CompletingAfterItemConsumption = false;

	/** 소비 중이거나 부모 완료 기록 전인 아이템을 잠시 보관하는 복구용 목록입니다. */
	TArray<FItemData> PendingConsumedItems;
};
