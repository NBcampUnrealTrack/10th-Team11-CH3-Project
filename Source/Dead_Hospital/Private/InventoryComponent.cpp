#include "InventoryComponent.h"

// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 아이템 추가
bool UInventoryComponent::AddItem(const FItemData& NewItem)
{
	// 이미 같은 아이템이 있는지 확인
	for (FItemData& Item : Items)
	{
		if (Item.ItemID == NewItem.ItemID)
		{
			// 현재 수량 + 새로 얻은 수량
			int32 NewQuantity = Item.Quantity + NewItem.Quantity;

			// 최대 수량을 넘지 않도록 제한
			Item.Quantity = FMath::Min(NewQuantity, Item.MaxStack);

			return true;
		}
	}

	// 같은 아이템이 없으면 새로 추가
	FItemData ItemToAdd = NewItem;

	// 처음 추가할 때도 최대 수량 제한
	ItemToAdd.Quantity =
		FMath::Min(ItemToAdd.Quantity, ItemToAdd.MaxStack);

	Items.Add(ItemToAdd);

	return true;
}

// 아이템 제거
bool UInventoryComponent::RemoveItem(FName ItemID, int32 RemoveQuantity)
{
	for (int32 i = 0; i < Items.Num(); i++)
	{
		if (Items[i].ItemID == ItemID)
		{
			// 현재 수량이 제거할 수량보다 많으면 수량만 감소
			if (Items[i].Quantity > RemoveQuantity)
			{
				Items[i].Quantity -= RemoveQuantity;
				return true;
			}

			// 수량이 같거나 더 적으면 아이템 자체 삭제
			Items.RemoveAt(i);
			return true;
		}
	}

	// 해당 아이템을 찾지 못함
	return false;
}