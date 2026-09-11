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

	// 아이템 추가
	bool AddItem(const FItemData& NewItem);

	// 아이템 제거
	bool RemoveItem(FName ItemID, int32 RemoveQuantity = 1);

	int32 GetItemQuantity(FName ItemID) const;

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