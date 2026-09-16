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
bool UInventoryComponent::AddItem(const FItemData& NewItem){

	// 잘못된 아이템 데이터 방지
	if (NewItem.Quantity <= 0 || NewItem.MaxStack <= 0){

		UE_LOG(LogTemp, Warning, TEXT("Invalid item data: %s"), *NewItem.ItemID.ToString());

		return false;
	}

	// 아이템 전체를 넣을 공간이 있는지 먼저 확인
	// 공간이 부족하면 아무것도 추가하지 않음
	if (!CanAddItem(NewItem)){

		UE_LOG(LogTemp, Warning, TEXT("Inventory is full: %s"), *NewItem.ItemID.ToString());

		return false;
	}

	// 아직 추가해야 하는 수량
	int32 RemainingQuantity = NewItem.Quantity;

	// 1. 기존에 있는 같은 아이템 스택부터 채움
	for (FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯이면 기존 스택이 아니므로 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 같은 아이템인지 확인
		if (Slot.ItemData.ItemID == NewItem.ItemID){

			// 이미 최대 수량이면 건너뜀
			if (Slot.ItemData.Quantity >= Slot.ItemData.MaxStack){
				continue;
			}

			// 현재 슬롯에 남아있는 공간
			int32 AvailableSpace = Slot.ItemData.MaxStack - Slot.ItemData.Quantity;

			// 실제로 이번 슬롯에 넣을 수량
			int32 AddQuantity = FMath::Min(AvailableSpace, RemainingQuantity);

			// 슬롯 수량 증가
			Slot.ItemData.Quantity += AddQuantity;

			// 추가한 만큼 남은 수량 감소
			RemainingQuantity -= AddQuantity;

			// 전부 추가했으면 성공
			if (RemainingQuantity <= 0){

				UE_LOG(LogTemp, Warning, TEXT("Item added: %s / Amount: %d"), *NewItem.ItemID.ToString(), NewItem.Quantity);


				// 인벤토리가 변경되었음을 알림
				OnInventoryChanged.Broadcast();

				return true;
			}
		}
	}

	// 2. 기존 스택에 다 못 넣었다면 빈 슬롯 사용
	while (RemainingQuantity > 0){

		// 비어있는 첫 번째 슬롯 찾기
		int32 EmptySlotIndex = FindEmptySlotIndex();

		// CanAddItem에서 이미 확인했기 때문에
		// 정상이라면 여기서 -1이 나오면 안 됨
		if (EmptySlotIndex == -1){

			UE_LOG(LogTemp, Error, TEXT("Failed to find empty inventory slot"));

			return false;
		}

		// 빈 슬롯 가져오기
		FInventorySlot& EmptySlot = InventorySlots[EmptySlotIndex];

		// 새로운 아이템 데이터를 복사
		EmptySlot.ItemData = NewItem;

		// 한 슬롯에는 MaxStack까지만 넣음
		EmptySlot.ItemData.Quantity = FMath::Min(RemainingQuantity, NewItem.MaxStack);

		// 이제 빈 슬롯이 아님
		EmptySlot.bIsEmpty = false;

		// 넣은 수량만큼 감소
		RemainingQuantity -= EmptySlot.ItemData.Quantity;
	}

	UE_LOG(LogTemp, Warning, TEXT("Item added: %s / Amount: %d"), *NewItem.ItemID.ToString(), NewItem.Quantity);

	// 인벤토리가 변경되었음을 알림
	OnInventoryChanged.Broadcast();

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

	// 가지고 있는 전체 수량 확인
	int32 TotalQuantity = GetItemQuantity(ItemID);

	// 가지고 있는 수량보다 많이 제거하려고 하면 실패
	if (TotalQuantity < RemoveQuantity){

		UE_LOG(LogTemp, Warning, TEXT("Not enough item quantity: %s"), *ItemID.ToString());

		return false;
	}

	// 실제로 제거해야 하는 남은 수량
	int32 RemainingQuantity = RemoveQuantity;

	// 뒤쪽 슬롯부터 확인
	for (int32 i = InventorySlots.Num() - 1; i >= 0; i--) {
		FInventorySlot& Slot = InventorySlots[i];

		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 제거하려는 아이템이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// 현재 슬롯의 아이템을 전부 제거해야 하는 경우
		if (Slot.ItemData.Quantity <= RemainingQuantity){

			RemainingQuantity -= Slot.ItemData.Quantity;

			// 슬롯 자체를 삭제하지 않고 빈 슬롯으로 변경
			Slot.ItemData = FItemData();
			Slot.bIsEmpty = true;
		}
		else{
			// 현재 슬롯에서 일부만 제거
			Slot.ItemData.Quantity -= RemainingQuantity;

			RemainingQuantity = 0;
		}

		// 필요한 만큼 전부 제거했다면 종료
		if (RemainingQuantity <= 0){

			// 장착 중인 무기를 전부 제거했다면 장착 상태도 해제
			if (EquippedWeaponID == ItemID && GetItemQuantity(ItemID) <= 0){
				EquippedWeaponID = NAME_None;
			}

			UE_LOG(LogTemp, Warning, TEXT("Item removed: %s / Amount: %d"), *ItemID.ToString(), RemoveQuantity);

			// 인벤토리가 변경되었음을 알림
			OnInventoryChanged.Broadcast();

			return true;
		}
	}

	return false;
}

// 퍼즐에서 KeyItem을 정상 사용했을 때 제거
bool UInventoryComponent::ConsumeKeyItem(FName ItemID, int32 Quantity){

	// 잘못된 수량 요청
	if (Quantity <= 0){
		return false;
	}

	// 해당 KeyItem을 필요한 수량만큼 가지고 있는지 확인
	if (GetItemQuantity(ItemID) < Quantity){

		UE_LOG(LogTemp, Warning, TEXT("Not enough KeyItem quantity: %s"), *ItemID.ToString());

		return false;
	}

	// 실제 Grid Inventory에서 해당 아이템 찾기
	bool bIsKeyItem = false;

	for (const FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 해당 아이템 찾기
		if (Slot.ItemData.ItemID == ItemID){

			// KeyItem 타입인지 확인
			if (Slot.ItemData.ItemType != EItemType::KeyItem){

				UE_LOG(LogTemp, Warning, TEXT("Item is not KeyItem: %s"), *ItemID.ToString());

				return false;
			}

			bIsKeyItem = true;
			break;
		}
	}

	// 혹시 KeyItem을 찾지 못한 경우
	if (!bIsKeyItem){
		return false;
	}

	// 실제로 제거해야 하는 남은 수량
	int32 RemainingQuantity = Quantity;

	// 뒤쪽 슬롯부터 확인
	for (int32 i = InventorySlots.Num() - 1; i >= 0; i--){

		FInventorySlot& Slot = InventorySlots[i];

		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 사용할 KeyItem이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// 현재 슬롯을 전부 사용해야 하는 경우
		if (Slot.ItemData.Quantity <= RemainingQuantity){

			RemainingQuantity -= Slot.ItemData.Quantity;

			// 슬롯 자체는 삭제하지 않고 빈 슬롯으로 변경
			Slot.ItemData = FItemData();
			Slot.bIsEmpty = true;
		}
		else{

			// 현재 슬롯에서 필요한 수량만 감소
			Slot.ItemData.Quantity -= RemainingQuantity;

			RemainingQuantity = 0;
		}

		// 필요한 수량을 전부 사용했다면 성공
		if (RemainingQuantity <= 0){

			UE_LOG(LogTemp, Warning, TEXT("KeyItem consumed: %s / Amount: %d"), *ItemID.ToString(), Quantity);

			// 인벤토리가 변경되었음을 알림
			OnInventoryChanged.Broadcast();

			return true;
		}
	}

	return false;
}

// 해당 ItemID의 전체 수량 확인
int32 UInventoryComponent::GetItemQuantity(FName ItemID) const{

	// 같은 ItemID의 전체 수량을 저장
	int32 TotalQuantity = 0;

	// Grid Inventory의 모든 슬롯 확인
	for (const FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯은 확인할 필요가 없음
		if (Slot.bIsEmpty){
			continue;
		}

		// 찾고 있는 아이템인지 확인
		if (Slot.ItemData.ItemID == ItemID){

			// 같은 아이템이면 수량을 계속 더함
			TotalQuantity += Slot.ItemData.Quantity;
		}
	}

	// 해당 아이템이 없으면 0 반환
	return TotalQuantity;
}

// 현재 보유 중인 Coin 수량 확인
int32 UInventoryComponent::GetCoinQuantity() const
{
	// Coin도 일반 아이템처럼 InventorySlots에 저장되므로
	// 기존 GetItemQuantity()를 이용해서 전체 Coin 수량을 반환
	return GetItemQuantity(FName(TEXT("Coin")));
}

// Coin을 필요한 수량만큼 사용
bool UInventoryComponent::SpendCoin(int32 Amount){
	// 0개 또는 음수 Coin 사용 요청은 잘못된 요청
	if (Amount <= 0){

		UE_LOG(LogTemp, Warning, TEXT("Invalid Coin spend amount: %d"), Amount);
		return false;
	}

	// 현재 보유 중인 Coin 수량 확인
	int32 CurrentCoin = GetCoinQuantity();

	// 필요한 Coin보다 적게 가지고 있으면 사용 실패
	if (CurrentCoin < Amount){

		UE_LOG(LogTemp, Warning, TEXT("Not enough Coin / Current: %d / Required: %d"), CurrentCoin, Amount);

		return false;
	}

	// 기존 RemoveItem을 이용해서 Coin 차감
	// RemoveItem 내부에서 InventoryChanged Event도 발생함
	if (!RemoveItem(FName(TEXT("Coin")), Amount)){
		return false;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Coin spent / Amount: %d / Remaining: %d"), Amount, GetCoinQuantity());

	return true;
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

// 해당 아이템을 인벤토리에서 버릴 수 있는지 확인
bool UInventoryComponent::CanDiscardItem(FName ItemID) const{

	// 인벤토리에 없는 아이템은 버릴 수 없음
	if (!HasItem(ItemID)){
		return false;
	}

	// Key, Painting 같은 보호 아이템은 버릴 수 없음
	if (IsProtectedItem(ItemID)){
		return false;
	}

	// 그 외 아이템은 버릴 수 있음
	return true;
}

// 해당 아이템을 현재 일반 사용 가능한지 확인
bool UInventoryComponent::CanUseItem(FName ItemID) const{

	// 인벤토리 슬롯을 하나씩 확인
	for (const FInventorySlot& Slot : InventorySlots){
		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 찾고 있는 아이템이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// 소비 아이템만 일반 사용 가능
		if (Slot.ItemData.ItemType != EItemType::Consumable){
			return false;
		}

		// 현재 실제 사용 기능이 구현된 아이템은 Bandage
		if (Slot.ItemData.ItemID == FName(TEXT("Bandage"))){
			return true;
		}

		return false;
	}

	// 인벤토리에 해당 아이템이 없음
	return false;
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
	// Player 담당자가 IsHiding()을 추가하면 주석 해제
	/*
	if (Player->IsHiding()){

		UE_LOG(LogTemp, Warning, TEXT("Cannot use item while hiding"));

		return false;
	}
	*/

	// Grid Inventory에서 사용할 아이템 찾기
	for (const FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 사용하려는 아이템이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// 소비 아이템만 사용 가능
		if (Slot.ItemData.ItemType != EItemType::Consumable){

			UE_LOG(LogTemp, Warning, TEXT("Item is not consumable: %s"), *ItemID.ToString());

			return false;
		}

		// 현재는 Bandage만 소비 아이템으로 처리
		if (Slot.ItemData.ItemID == FName(TEXT("Bandage"))){

			// HP가 이미 최대라면 사용하지 않음
			if (Player->GetCurrentHP() >= Player->GetMaxHP()){

				UE_LOG(LogTemp, Warning, TEXT("Cannot use Bandage: HP is full"));

				return false;
			}

			// 효과량이 잘못 설정된 경우 사용하지 않음
			if (Slot.ItemData.EffectAmount <= 0.0f){

				UE_LOG(LogTemp, Warning, TEXT("Invalid Bandage EffectAmount"));

				return false;
			}

			// RemoveItem()을 호출하기 전에 회복량 저장
			float HealAmount = Slot.ItemData.EffectAmount;

			// Bandage 1개 제거
			if (!RemoveItem(ItemID, 1)){

				return false;
			}

			// HP 회복
			Player->Heal(HealAmount);

			UE_LOG(LogTemp, Warning,
				TEXT("Bandage used / Heal: %.1f / HP: %.1f / %.1f"),
				HealAmount,
				Player->GetCurrentHP(),
				Player->GetMaxHP());

			return true;
		}
	}

	return false;
}

// 해당 아이템을 장착할 수 있는지 확인
bool UInventoryComponent::CanEquipItem(FName ItemID) const{

	// 인벤토리 슬롯을 하나씩 확인
	for (const FInventorySlot& Slot : InventorySlots){
		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 찾고 있는 아이템이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// Weapon 타입만 장착 가능
		if (Slot.ItemData.ItemType != EItemType::Weapon){
			return false;
		}

		return true;
	}

	// 인벤토리에 해당 아이템이 없음
	return false;
}

// 무기 장착 함수
bool UInventoryComponent::EquipWeapon(FName ItemID){

	// Grid Inventory에서 해당 아이템 찾기
	for (const FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯은 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 장착하려는 아이템이 아니면 건너뜀
		if (Slot.ItemData.ItemID != ItemID){
			continue;
		}

		// 무기 타입인지 확인
		if (Slot.ItemData.ItemType != EItemType::Weapon){

			UE_LOG(LogTemp, Warning, TEXT("Item is not a weapon: %s"), *ItemID.ToString());

			return false;
		}

		// 현재 장착 무기로 설정
		EquippedWeaponID = ItemID;

		UE_LOG(LogTemp, Warning, TEXT("Weapon equipped: %s"), *ItemID.ToString());

		// 장착 무기가 변경되었음을 알림
		OnInventoryChanged.Broadcast();
		return true;
	}

	// 인벤토리에 해당 무기가 없음
	UE_LOG(LogTemp, Warning, TEXT("Weapon not found in inventory: %s"), *ItemID.ToString());

	return false;
}

// 무기 해제 함수
void UInventoryComponent::UnequipWeapon(){
	if (EquippedWeaponID.IsNone()){
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Weapon unequipped: %s"), *EquippedWeaponID.ToString());

	EquippedWeaponID = NAME_None;

	// 장착 무기가 변경되었음을 알림
	OnInventoryChanged.Broadcast();
}

// 해당 아이템이 현재 장착 중인지 확인
bool UInventoryComponent::IsItemEquipped(FName ItemID) const{

	// ItemID가 비어있으면 장착 상태가 아님
	if (ItemID.IsNone()){
		return false;
	}

	// 현재 장착 중인 무기 ID와 같은지 확인
	return EquippedWeaponID == ItemID;
}

//현재 장착 무기 확인
FName UInventoryComponent::GetEquippedWeaponID() const {
	return EquippedWeaponID;
}

// 장착 여부 확인
bool UInventoryComponent::HasEquippedWeapon() const {
	return !EquippedWeaponID.IsNone();
}

// 비어있는 첫 번째 슬롯 번호 찾기
int32 UInventoryComponent::FindEmptySlotIndex() const{
	for (int32 i = 0; i < InventorySlots.Num(); i++){
		// 비어있는 슬롯 발견
		if (InventorySlots[i].bIsEmpty){
			return i;
		}
	}

	// 빈 슬롯이 없으면 -1 반환
	return -1;
}

// 아이템을 전부 추가할 공간이 있는지 확인
bool UInventoryComponent::CanAddItem(const FItemData& NewItem) const{

	// 잘못된 아이템 데이터면 추가 불가
	if (NewItem.Quantity <= 0 || NewItem.MaxStack <= 0){
		return false;
	}

	// 추가해야 하는 남은 수량
	int32 RemainingQuantity = NewItem.Quantity;

	// 1. 기존에 있는 같은 아이템 슬롯의 남은 공간 확인
	for (const FInventorySlot& Slot : InventorySlots){

		// 빈 슬롯은 일단 건너뜀
		if (Slot.bIsEmpty){
			continue;
		}

		// 같은 아이템인지 확인
		if (Slot.ItemData.ItemID == NewItem.ItemID){

			// 이 슬롯에 추가로 들어갈 수 있는 수량
			int32 AvailableSpace =
				Slot.ItemData.MaxStack - Slot.ItemData.Quantity;

			// 실제로 넣을 수 있는 수량만큼 계산
			int32 AddQuantity =
				FMath::Min(AvailableSpace, RemainingQuantity);

			RemainingQuantity -= AddQuantity;

			// 기존 스택들만으로 전부 들어간다면 추가 가능
			if (RemainingQuantity <= 0){
				return true;
			}
		}
	}

	// 2. 빈 슬롯에 들어갈 수 있는 공간 확인

	for (const FInventorySlot& Slot : InventorySlots){
		if (Slot.bIsEmpty){

			// 빈 슬롯 하나에는 최대 MaxStack만큼 들어갈 수 있음
			RemainingQuantity -= NewItem.MaxStack;

			// 필요한 수량을 전부 넣을 수 있음
			if (RemainingQuantity <= 0){
				return true;
			}
		}
	}

	// 기존 스택 + 빈 슬롯을 모두 사용해도 공간 부족
	return false;
}

// Grid Inventory에서 아이템 위치 이동
bool UInventoryComponent::MoveItem(int32 FromIndex, int32 ToIndex){

	// 출발 슬롯 번호가 올바른지 확인
	if (!InventorySlots.IsValidIndex(FromIndex)){

		UE_LOG(LogTemp, Warning, TEXT("Invalid FromIndex: %d"), FromIndex);

		return false;
	}

	// 도착 슬롯 번호가 올바른지 확인
	if (!InventorySlots.IsValidIndex(ToIndex)){

		UE_LOG(LogTemp, Warning, TEXT("Invalid ToIndex: %d"), ToIndex);

		return false;
	}

	// 같은 슬롯으로 이동하려는 경우
	if (FromIndex == ToIndex){
		return false;
	}

	// 출발 슬롯이 비어있으면 이동할 아이템이 없음
	if (InventorySlots[FromIndex].bIsEmpty){
		UE_LOG(LogTemp, Warning, TEXT("From slot is empty: %d"), FromIndex);

		return false;
	}

	// 도착 슬롯에 이미 아이템이 있는 경우
	if (!InventorySlots[ToIndex].bIsEmpty){

		// 두 슬롯의 아이템이 서로 다른 아이템인지 확인
		if (InventorySlots[FromIndex].ItemData.ItemID != InventorySlots[ToIndex].ItemData.ItemID){

			// 출발 슬롯의 아이템을 임시로 저장
			FItemData TempItem = InventorySlots[FromIndex].ItemData;

			// 도착 슬롯 아이템을 출발 슬롯로 이동
			InventorySlots[FromIndex].ItemData = InventorySlots[ToIndex].ItemData;

			// 임시 저장했던 출발 아이템을 도착 슬롯으로 이동
			InventorySlots[ToIndex].ItemData = TempItem;

			UE_LOG(LogTemp, Warning, TEXT("Items swapped: Slot %d <-> Slot %d"), FromIndex, ToIndex);

			// 인벤토리가 변경되었음을 알림
			OnInventoryChanged.Broadcast();

			return true;
		}

		// 같은 아이템이면 Stack 합치기
		int32 AvailableSpace = InventorySlots[ToIndex].ItemData.MaxStack - InventorySlots[ToIndex].ItemData.Quantity;

		// 도착 슬롯이 이미 MaxStack까지 가득 찬 경우
		if (AvailableSpace <= 0){

			UE_LOG(LogTemp, Warning, TEXT("Target stack is already full"));

			return false;
		}

		// 실제로 옮길 수량 계산
		int32 MoveQuantity = FMath::Min(InventorySlots[FromIndex].ItemData.Quantity, AvailableSpace);

		// 도착 슬롯에 수량 추가
		InventorySlots[ToIndex].ItemData.Quantity += MoveQuantity;

		// 출발 슬롯에서 옮긴 만큼 수량 감소
		InventorySlots[FromIndex].ItemData.Quantity -= MoveQuantity;

		// 출발 슬롯의 수량이 0이 되었다면 빈 슬롯으로 변경
		if (InventorySlots[FromIndex].ItemData.Quantity <= 0){
			InventorySlots[FromIndex].ItemData = FItemData();
			InventorySlots[FromIndex].bIsEmpty = true;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("Items stacked: Slot %d -> Slot %d / Amount: %d"),
			FromIndex,
			ToIndex,
			MoveQuantity);

		// 인벤토리 슬롯 위치가 변경되었음을 알림
		OnInventoryChanged.Broadcast();

		return true;
	}

	// 출발 슬롯의 아이템 데이터를 도착 슬롯으로 복사
	InventorySlots[ToIndex].ItemData = InventorySlots[FromIndex].ItemData;

	// 도착 슬롯을 사용 중인 상태로 변경
	InventorySlots[ToIndex].bIsEmpty = false;

	// 출발 슬롯의 아이템 데이터 제거
	InventorySlots[FromIndex].ItemData = FItemData();

	// 출발 슬롯을 빈 슬롯으로 변경
	InventorySlots[FromIndex].bIsEmpty = true;

	UE_LOG(LogTemp, Warning, TEXT("Item moved: Slot %d -> Slot %d"), FromIndex, ToIndex);

	// 인벤토리 슬롯 위치가 변경되었음을 알림
	OnInventoryChanged.Broadcast();

	return true;
}

// 현재 Grid Inventory의 전체 슬롯 데이터 반환
const TArray<FInventorySlot>& UInventoryComponent::GetInventorySlots() const
{
	return InventorySlots;
}