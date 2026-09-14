#include "InventoryComponent.h"
#include "PlayerCharacter.h"

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

	// Grid Inventory 슬롯 초기화
	InventorySlots.Empty();

	for (int32 i = 0; i < MaxInventorySlots; i++){
		FInventorySlot NewSlot;

		// 슬롯 번호 설정
		NewSlot.SlotIndex = i;

		// 처음에는 모든 슬롯이 비어있음
		NewSlot.bIsEmpty = true;

		// 슬롯 배열에 추가
		InventorySlots.Add(NewSlot);
	}

	UE_LOG(LogTemp, Warning, TEXT("Inventory Slots Initialized: %d"), InventorySlots.Num());
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

	// Key, Painting 같은 진행 아이템은 일반 제거 불가
	if (IsProtectedItem(ItemID)){

		UE_LOG(LogTemp, Warning, TEXT("Protected item cannot be removed: %s"), *ItemID.ToString());

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

// 퍼즐에서 KeyItem을 정상 사용했을 때 제거
bool UInventoryComponent::ConsumeKeyItem(FName ItemID, int32 Quantity){
	if (Quantity <= 0){
		return false;
	}

	// 해당 KeyItem을 필요한 수량만큼 가지고 있는지 확인
	if (GetItemQuantity(ItemID) < Quantity){
		UE_LOG(LogTemp, Warning, TEXT("Not enough KeyItem quantity: %s"), *ItemID.ToString());

		return false;
	}

	// 실제 인벤토리에서 해당 아이템 찾기
	for (const FItemData& Item : Items)
	{
		if (Item.ItemID == ItemID)
		{
			// KeyItem 타입만 퍼즐 전용 소비 허용
			if (Item.ItemType != EItemType::KeyItem){
				UE_LOG(LogTemp, Warning, TEXT("Item is not KeyItem: %s"), *ItemID.ToString());

				return false;
			}

			break;
		}
	}

	// RemoveItem은 보호 아이템을 막기 때문에
	// 여기서는 직접 수량 제거
	int32 RemainingQuantity = Quantity;

	for (int32 i = Items.Num() - 1; i >= 0; i--){
		if (Items[i].ItemID == ItemID){
			if (Items[i].Quantity <= RemainingQuantity){
				RemainingQuantity -= Items[i].Quantity;
				Items.RemoveAt(i);
			}
			else{
				Items[i].Quantity -= RemainingQuantity;
				RemainingQuantity = 0;
			}

			if (RemainingQuantity <= 0){
				UE_LOG(LogTemp, Warning, TEXT("KeyItem consumed: %s / Amount: %d"), *ItemID.ToString(), Quantity);

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

// 해당 아이템을 가지고 있는지 확인
bool UInventoryComponent::HasItem(FName ItemID) const
{
	return GetItemQuantity(ItemID) > 0;
}

// 진행에 필요한 보호 아이템인지 확인
bool UInventoryComponent::IsProtectedItem(FName ItemID) const
{
	return ItemID == FName(TEXT("Key")) || ItemID == FName(TEXT("Painting"));
}

// 소비 아이템 사용
bool UInventoryComponent::UseItem(FName ItemID){
	
	// 해당 아이템을 가지고 있는지 확인
	if (!HasItem(ItemID)){
		UE_LOG(LogTemp, Warning, TEXT("Item not found: %s"), *ItemID.ToString());

		return false;
	}

	// InventoryComponent를 가지고 있는 Player 가져오기
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());

	if (!Player){
		UE_LOG(LogTemp, Warning, TEXT("Inventory owner is not PlayerCharacter"));

		return false;
	}

	// Player가 죽어 있으면 아이템 사용 불가
	if (Player->IsDead()){
		UE_LOG(LogTemp, Warning, TEXT("Cannot use item: Player is dead"));

		return false;
	}

	// Player가 은신 중이면 아이템 사용 불가
	/*
	if (Player->IsHiding()){

		UE_LOG(LogTemp, Warning, TEXT("Cannot use item while hiding"));

		return false;
	}
	*/

	// 인벤토리에서 사용할 아이템 찾기
	for (const FItemData& Item : Items){
		if (Item.ItemID != ItemID){
			continue;
		}

		// 소비 아이템만 사용 가능
		if (Item.ItemType != EItemType::Consumable){
			UE_LOG(LogTemp, Warning, TEXT("Item is not consumable: %s"), *ItemID.ToString());

			return false;
		}

		// 현재는 Bandage만 소비 아이템으로 처리
		if (Item.ItemID == FName(TEXT("Bandage"))){

			// HP가 이미 최대라면 사용하지 않음
			if (Player->GetCurrentHP() >= Player->GetMaxHP()){
				UE_LOG(LogTemp, Warning, TEXT("Cannot use Bandage: HP is full"));

				return false;
			}

			// 효과량이 잘못 설정된 경우 사용하지 않음
			if (Item.EffectAmount <= 0.0f){
				UE_LOG(LogTemp, Warning, TEXT("Invalid Bandage EffectAmount"));

				return false;
			}

			// Bandage 1개 제거
			// 제거에 실패하면 회복도 하지 않음
			if (!RemoveItem(ItemID, 1)){
				return false;
			}

			// HP 회복
			Player->Heal(Item.EffectAmount);

			UE_LOG(LogTemp, Warning, TEXT("Bandage used / Heal: %.1f / HP: %.1f / %.1f"),
				Item.EffectAmount,
				Player->GetCurrentHP(),
				Player->GetMaxHP());

			return true;
		}
	}

	return false;
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