#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "InventoryComponent.generated.h"

// 인벤토리 내용이 변경되었을 때 알려주는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

// 아이템을 획득했을 때 아이템 정보와 획득 수량을 알려주는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnItemAcquired,
	FName, ItemID,
	int32, AcquiredQuantity
);

// 퀵슬롯 내용이 변경되었을 때 UI에 알려주는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnQuickSlotChanged,
	int32, QuickSlotIndex,
	FName, ItemID
);

class UDataTable;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEAD_HOSPITAL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// 인벤토리 내용이 변경되었을 때 발생하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	// 아이템 획득에 성공했을 때 발생하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAcquired OnItemAcquired;

	// 퀵슬롯이 변경되면 UI에서 받아서 갱신
	UPROPERTY(BlueprintAssignable, Category = "QuickSlot")
	FOnQuickSlotChanged OnQuickSlotChanged;

	// Grid Inventory용 슬롯 배열
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> InventorySlots;

	// 인벤토리 최대 슬롯 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 MaxInventorySlots = 20;

	// 아이템 추가
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(const FItemData& NewItem);

	// 아이템 제거
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemID, int32 RemoveQuantity = 1);

	// 해당 아이템의 전체 수량 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemQuantity(FName ItemID) const;

	// 현재 보유 중인 Coin 수량 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCoinQuantity() const;

	// Coin을 필요한 수량만큼 사용
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SpendCoin(int32 Amount);

	// 해당 아이템을 1개 이상 가지고 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(FName ItemID) const;

	//인벤토리 빈 슬롯 있는지 확인
	int32 FindEmptySlotIndex() const;

	// 아이템을 전부 추가할 공간이 있는지 확인
	bool CanAddItem(const FItemData& NewItem) const;

	// 버리거나 일반 제거하면 안 되는 진행 아이템인지 확인
	bool IsProtectedItem(FName ItemID) const;

	// 해당 아이템을 인벤토리에서 버릴 수 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanDiscardItem(FName ItemID) const;

	// 해당 아이템을 현재 일반 사용 가능한지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanUseItem(FName ItemID) const;

	// 소비 아이템 사용
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(FName ItemID);

	// 퍼즐에서 KeyItem을 정상 사용했을 때 제거
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeKeyItem(FName ItemID, int32 Quantity = 1);

	// Grid Inventory에서 아이템 위치 이동
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItem(int32 FromIndex, int32 ToIndex);

	// 현재 Grid Inventory의 전체 슬롯 데이터 반환
	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FInventorySlot>& GetInventorySlots() const;

	// 해당 아이템을 장착할 수 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanEquipItem(FName ItemID) const;

	// 무기 장착
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipWeapon(FName ItemID);

	// 무기 해제
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UnequipWeapon();

	// 해당 아이템이 현재 장착 중인지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsItemEquipped(FName ItemID) const;

	// 현재 장착 중인 무기 ID
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FName GetEquippedWeaponID() const;

	// 무기 장착 여부 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasEquippedWeapon() const;

	// 필요한 재료를 가지고 있는지 확인
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CanCraftItem(FName ResultItemID) const;

	// 재료를 소비하고 아이템을 실제로 제작
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CraftItem(FName ResultItemID);

	// 아이템을 지정한 퀵슬롯에 등록
	// QuickSlotIndex : 0 = 1번키, 1 = 2번키, 2 = 3번키
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool SetQuickSlot(int32 QuickSlotIndex, FName ItemID);

	// 지정한 퀵슬롯에 등록된 아이템 ID 반환
	// QuickSlotIndex : 0 = 1번키, 1 = 2번키, 2 = 3번키
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	FName GetQuickSlotItem(int32 QuickSlotIndex) const;

	// 지정한 퀵슬롯에 등록된 아이템 사용
	// 소비 아이템이면 사용하고, 무기면 장착
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UseQuickSlot(int32 QuickSlotIndex);

	// 지정한 퀵슬롯의 등록 아이템 해제
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool ClearQuickSlot(int32 QuickSlotIndex);

protected:
	virtual void BeginPlay() override;

	// 아이템 정보를 가져올 DataTable
	// 에디터에서 DT_ItemData를 연결해서 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	UDataTable* ItemDataTable;

private:

	// 제작 재료를 소비한 뒤 결과 아이템을 넣을 공간이 생기는지 확인
	bool CanAddCraftResultAfterConsumingMaterials(FName ResultItemID, const FItemData& ResultItemData) const;

	//현재 장착 중인 무기 ID
	FName EquippedWeaponID;

	// 퀵슬롯에 등록된 아이템 ID
	// 0번 = 키보드 1번
	// 1번 = 키보드 2번
	// 2번 = 키보드 3번
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot",
		meta = (AllowPrivateAccess = "true"))
	TArray<FName> QuickSlots;

};