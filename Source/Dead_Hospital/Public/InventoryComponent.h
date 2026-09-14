#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "InventoryComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEAD_HOSPITAL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// 인벤토리에 들어있는 아이템 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FItemData> Items;

	// Grid Inventory용 슬롯 배열
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> InventorySlots;

	// 인벤토리 최대 슬롯 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 MaxInventorySlots = 20;

	// 아이템 추가
	bool AddItem(const FItemData& NewItem);

	// 아이템 제거
	bool RemoveItem(FName ItemID, int32 RemoveQuantity = 1);

	// 전투 파트가 Ammo 수량을 확인할수 있는 함수
	int32 GetItemQuantity(FName ItemID) const;

	// 해당 아이템을 1개 이상 가지고 있는지 확인
	bool HasItem(FName ItemID) const;

	// 버리거나 일반 제거하면 안 되는 진행 아이템인지 확인
	bool IsProtectedItem(FName ItemID) const;

	// 소비 아이템 사용
	bool UseItem(FName ItemID);

	// 퍼즐에서 KeyItem을 정상 사용했을 때 제거
	bool ConsumeKeyItem(FName ItemID, int32 Quantity = 1);

	// 무기 장착
	bool EquipWeapon(FName ItemID);

	// 무기 해제
	void UnequipWeapon();

	// 현재 장착 중인 무기 ID
	FName GetEquippedWeaponID() const;

	// 무기 장착 여부 확인
	bool HasEquippedWeapon() const;

protected:
	virtual void BeginPlay() override;

private:

	//현재 장착 중인 무기 ID
	FName EquippedWeaponID;
};