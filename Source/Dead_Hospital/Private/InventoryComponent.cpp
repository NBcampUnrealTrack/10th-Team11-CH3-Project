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
	//제거 수량이 0 이하이면 잘못된 요청
	if (RemoveQuantity <= 0) {
		return false;
	}

	//인벤토리 전체를 확인
	for (int32 i = 0; i < Items.Num(); i++)
	{	
		//같은 ItemID를 찾음
		if (Items[i].ItemID == ItemID) {
			// 가지고 있는 수량보다 많이 제거하려고 하면 실패
			if (Items[i].Quantity < RemoveQuantity) {
				UE_LOG(LogTemp, Warning, TEXT("Not enough item quantity: %s"), *ItemID.ToString());

				return false;
			}

			//수량이 정확히 같으면 인벤토리에서 아이템 자체를 제거
			if (Items[i].Quantity == RemoveQuantity) {
				Items.RemoveAt(i);

				UE_LOG(LogTemp, Warning, TEXT("Item removed: %s"), *ItemID.ToString());

				return true;
			}

			//가지고 있는 수량이 더 많으면 수량만 감소
			Items[i].Quantity -= RemoveQuantity;

			UE_LOG(LogTemp, Warning, TEXT("Item quantity decreased: %s / Remaining: %d"), *ItemID.ToString(), Items[i].Quantity);

			return true;
		}
	}

	// 해당 아이템을 찾지 못함
	return false;
}
// 전투 파트가 Ammo 수량을 확인할수 있는 함수
int32 UInventoryComponent::GetItemQuantity(FName ItemID) const{
	for (const FItemData& Item : Items){
		if (Item.ItemID == ItemID){
			return Item.Quantity;
		}
	}

	// 인벤토리에 해당 아이템이 없으면 0
	return 0;
}