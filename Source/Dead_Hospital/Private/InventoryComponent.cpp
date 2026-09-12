#include "InventoryComponent.h"

// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	//처음에는 장착된 무기가 없음
	EquippedWeaponID = NAME_None;
}

// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 아이템 추가
bool UInventoryComponent::AddItem(const FItemData& NewItem)
{
	// 잘못된 아이템 데이터 방지
	if (NewItem.Quantity <= 0 || NewItem.MaxStack <= 0) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid item data: %s"), *NewItem.ItemID.ToString());

		return false;
	}

	// 새로 추가해야 할 남은 수량
	int32 RemainingQuantity = NewItem.Quantity;

	// 기존에 있는 같은 아이템 스택부터 채움
	for (FItemData& Item : Items)
	{
		if (Item.ItemID == NewItem.ItemID)
		{
			//이미 꽉 찬 스택이면 건너뜀
			if (Item.Quantity >= Item.MaxStack) {
				continue;
			}

			// 현재 스택에 얼마나 더 들어갈 수 있는지 계산
			int32 AvailableSpace = Item.MaxStack - Item.Quantity;

			// 실제로 이번 스택에 넣을 수량
			int32 AddQuantity = FMath::Min(AvailableSpace, RemainingQuantity);

			// 수량 추가
			Item.Quantity += AddQuantity;

			// 추가한 만큼 남은 수량 감소
			RemainingQuantity -= AddQuantity;

			// 전부 추가했으면 성공
			if (RemainingQuantity <= 0) {
				return true;
			}
		}
	}

	// 기존 스택에 다 못 넣었다면
	// 남은 수량으로 새 스택 생성
	while (RemainingQuantity > 0) {
		FItemData ItemToAdd = NewItem;

		// 새 스택에 넣을 수량
		ItemToAdd.Quantity = FMath::Min(RemainingQuantity, NewItem.MaxStack);

		Items.Add(ItemToAdd);

		// 새로 넣은 만큼 감소
		RemainingQuantity -= ItemToAdd.Quantity;
	}

	return true;
}

// 아이템 제거
bool UInventoryComponent::RemoveItem(FName ItemID, int32 RemoveQuantity){
	// 잘못된 수량 요청
	if (RemoveQuantity <= 0){
		return false;
	}

	// 먼저 전체 수량 확인
	int32 TotalQuantity = GetItemQuantity(ItemID);

	// 가지고 있는 수량보다 많이 제거하려고 하면 실패
	if (TotalQuantity < RemoveQuantity){
		UE_LOG(LogTemp, Warning, TEXT("Not enough item quantity: %s"),*ItemID.ToString());

		return false;
	}

	// 실제로 빼야 할 남은 수량
	int32 RemainingQuantity = RemoveQuantity;

	// 뒤에서부터 확인하면 RemoveAt() 사용하기 편함
	for (int32 i = Items.Num() - 1; i >= 0; i--){
		if (Items[i].ItemID == ItemID){
			// 현재 스택 수량이 빼야 할 수량보다 작거나 같으면
			// 현재 스택을 통째로 제거
			if (Items[i].Quantity <= RemainingQuantity){

				RemainingQuantity -= Items[i].Quantity;

				Items.RemoveAt(i);
			}
			else{
				// 현재 스택에서 일부만 제거
				Items[i].Quantity -= RemainingQuantity;

				RemainingQuantity = 0;
			}

			// 필요한 만큼 전부 제거했으면 종료
			if (RemainingQuantity <= 0){

				//장착 중인 무기를 전부 제거했다면 장착 해제
				if (EquippedWeaponID == ItemID && GetItemQuantity(ItemID) <= 0) {
					UnequipWeapon();
				}

				UE_LOG(LogTemp, Warning, TEXT("Item removed: %s / Amount: %d"),*ItemID.ToString(),RemoveQuantity);

				return true;
			}
		}
	}

	return false;
}
// 전투 파트가 Ammo 수량을 확인할수 있는 함수
int32 UInventoryComponent::GetItemQuantity(FName ItemID) const{

	//같은 ItemID의 전체 수량을 저장
	int32 TotalQuantity = 0;
	
	//인벤토리 전체 확인
	for (const FItemData& Item : Items){
		if (Item.ItemID == ItemID){
			//같은 아이템이면 수량을 계속 더함
			TotalQuantity += Item.Quantity;
		}
	}

	// 인벤토리에 해당 아이템이 없으면 0
	return TotalQuantity;
}

// 무기 장착 함수
bool UInventoryComponent::EquipWeapon(FName ItemID) {

	//인벤토리에서 해당 아이템 찾기
	for (const FItemData& Item : Items) {
		if (Item.ItemID == ItemID) {
			//무기 타입인지 확인
			if (Item.ItemType != EItemType::Weapon) {
				UE_LOG(LogTemp, Warning, TEXT("Item is not a weapon: %s"), *ItemID.ToString());

				return false;
			}

			// 현재 장착 무기로 설정
			EquippedWeaponID = ItemID;

			UE_LOG(LogTemp, Warning, TEXT("Weapon equipped: %s"), *ItemID.ToString());

			return true;
		}
	}

	// 인벤토리에 해당 무기가 없음
	UE_LOG(LogTemp, Warning, TEXT("Weapon not found in inventory: %s"), *ItemID.ToString());

	return false;
}

// 무기 해제 함수
void UInventoryComponent::UnequipWeapon() {
	if (EquippedWeaponID.IsNone()) {
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Weapon unequipped: %s"), *EquippedWeaponID.ToString());

	EquippedWeaponID = NAME_None;
}

//현재 장착 무기 확인
FName UInventoryComponent::GetEquippedWeaponID() const {
	return EquippedWeaponID;
}

// 장착 여부 확인
bool UInventoryComponent::HasEquippedWeapon() const {
	return !EquippedWeaponID.IsNone();
}