#include "VendingMachine.h"
#include "InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"

AVendingMachine::AVendingMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	// 자판기 외형을 담당할 Static Mesh Component 생성
	VendingMachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VendingMachineMesh"));

	// 자판기 Mesh를 이 Actor의 Root Component로 사용
	RootComponent = VendingMachineMesh;
}

void AVendingMachine::BeginPlay()
{
	Super::BeginPlay();

	// DataTable이 설정되어 있다면 자판기 상품 데이터를 불러옴
	if (ItemDataTable){
		LoadShopItemsFromTable();
	}
}

// ShopItems의 Row Name을 이용해 DT_ItemData에서 상품 데이터를 불러옴
bool AVendingMachine::LoadShopItemsFromTable(){

	// DataTable이 설정되지 않았다면 실패
	if (!ItemDataTable){

		UE_LOG(LogTemp, Warning, TEXT("VendingMachine: ItemDataTable is not set"));

		return false;
	}

	// 등록된 상품들을 하나씩 확인
	for (FShopItem& ShopItem : ShopItems){

		// Row Name이 설정되지 않은 상품은 실패
		if (ShopItem.ItemRowName.IsNone()){

			UE_LOG(LogTemp, Warning, TEXT("VendingMachine: ItemRowName is not set"));

			return false;
		}

		// DT_ItemData에서 해당 Row 검색
		const FItemData* FoundItemData = ItemDataTable->FindRow<FItemData>(ShopItem.ItemRowName, TEXT("VendingMachine LoadShopItems"));

		// 해당 Row를 찾지 못했다면 실패
		if (!FoundItemData){

			UE_LOG(LogTemp, Warning, TEXT("VendingMachine: Failed to find Row: %s"), *ShopItem.ItemRowName.ToString());

			return false;
		}

		// DataTable에서 찾은 아이템 정보를 상품 데이터에 복사
		ShopItem.ItemData = *FoundItemData;

		UE_LOG(LogTemp, Warning, TEXT("VendingMachine loaded Item: %s / Stock: %d"), *ShopItem.ItemData.ItemID.ToString(), ShopItem.Stock);
	}

	return true;
}

// 자판기에서 판매 중인 전체 상품 목록 반환
const TArray<FShopItem>& AVendingMachine::GetShopItems() const
{
	return ShopItems;
}

// 해당 상품을 현재 구매할 수 있는지 확인
bool AVendingMachine::CanPurchaseItem(int32 ShopItemIndex, UInventoryComponent* Inventory) const
{
	// 구매 결과가 Success라면 구매 가능
	return GetPurchaseResult(ShopItemIndex, Inventory) == EPurchaseResult::Success;
}

// 선택한 상품 구매
EPurchaseResult AVendingMachine::PurchaseItem(int32 ShopItemIndex, UInventoryComponent* Inventory){

	// 이미 구매 처리 중이면 중복 구매 요청으로 판단
	if (bIsPurchasing){
		return EPurchaseResult::InvalidItem;
	}

	// 현재 구매 가능 여부와 실패 이유 확인
	EPurchaseResult PurchaseResult = GetPurchaseResult(ShopItemIndex, Inventory);

	// 구매할 수 없는 상태라면 해당 실패 이유 반환
	if (PurchaseResult != EPurchaseResult::Success){
		return PurchaseResult;
	}

	// 구매 처리 시작
	bIsPurchasing = true;

	// 선택한 상품 가져오기
	FShopItem& ShopItem = ShopItems[ShopItemIndex];

	// 플레이어 인벤토리에 상품 지급
	if (!Inventory->AddItem(ShopItem.ItemData)){
		bIsPurchasing = false;

		return EPurchaseResult::InventoryFull;
	}

	// 상품 가격만큼 Coin 차감
	if (!Inventory->SpendCoin(ShopItem.ItemData.Price)){

		// Coin 차감 실패 시 먼저 지급한 상품 제거
		Inventory->RemoveItem(ShopItem.ItemData.ItemID, ShopItem.ItemData.Quantity
		);

		bIsPurchasing = false;

		UE_LOG(LogTemp, Error, TEXT("Purchase failed while spending Coin. Item rollback: %s"), *ShopItem.ItemData.ItemID.ToString());

		return EPurchaseResult::NotEnoughCoin;
	}

	// 구매 성공 → 재고 1 감소
	ShopItem.Stock--;

	// 구매 처리 종료
	bIsPurchasing = false;

	UE_LOG(LogTemp, Warning,
		TEXT("Purchase Success / Item: %s / Price: %d / Stock: %d"),
		*ShopItem.ItemData.ItemID.ToString(),
		ShopItem.ItemData.Price,
		ShopItem.Stock);

	return EPurchaseResult::Success;
}

// 해당 상품의 구매 가능 여부와 실패 이유 확인
EPurchaseResult AVendingMachine::GetPurchaseResult(int32 ShopItemIndex, UInventoryComponent* Inventory) const
{
	// Inventory가 없으면 정상적인 구매가 불가능
	if (!Inventory){
		return EPurchaseResult::InvalidItem;
	}

	// 존재하지 않는 상품 번호
	if (!ShopItems.IsValidIndex(ShopItemIndex)){
		return EPurchaseResult::InvalidItem;
	}

	// 선택한 상품 가져오기
	const FShopItem& ShopItem = ShopItems[ShopItemIndex];

	// 재고 없음
	if (ShopItem.Stock <= 0){
		return EPurchaseResult::OutOfStock;
	}

	// 상품 데이터가 잘못된 경우
	if (ShopItem.ItemData.ItemID.IsNone() || ShopItem.ItemData.Quantity <= 0 || ShopItem.ItemData.MaxStack <= 0 || ShopItem.ItemData.Price < 0){
		return EPurchaseResult::InvalidItem;
	}

	// Coin 부족
	if (Inventory->GetCoinQuantity() < ShopItem.ItemData.Price){
		return EPurchaseResult::NotEnoughCoin;
	}

	// 인벤토리 공간 부족
	if (!Inventory->CanAddItem(ShopItem.ItemData)){
		return EPurchaseResult::InventoryFull;
	}

	// 위 조건에 아무것도 걸리지 않았다면 구매 가능
	return EPurchaseResult::Success;
}