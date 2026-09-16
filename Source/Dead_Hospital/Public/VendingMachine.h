#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemData.h"
#include "VendingMachine.generated.h"

class UInventoryComponent;
class UStaticMeshComponent;
class UDataTable;

UCLASS()
class DEAD_HOSPITAL_API AVendingMachine : public AActor
{
	GENERATED_BODY()

public:
	AVendingMachine();

	// 자판기에서 판매하는 상품 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FShopItem> ShopItems;

	// 자판기에서 판매 중인 전체 상품 목록 반환
	UFUNCTION(BlueprintPure, Category = "Shop")
	const TArray<FShopItem>& GetShopItems() const;

	// 해당 상품을 현재 구매할 수 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool CanPurchaseItem(int32 ShopItemIndex, UInventoryComponent* Inventory) const;

	// 해당 상품의 구매 가능 여부와 실패 이유 확인
	UFUNCTION(BlueprintPure, Category = "Shop")
	EPurchaseResult GetPurchaseResult(int32 ShopItemIndex, UInventoryComponent* Inventory) const;

	// 선택한 상품 구매
	UFUNCTION(BlueprintCallable, Category = "Shop")
	EPurchaseResult PurchaseItem(int32 ShopItemIndex, UInventoryComponent* Inventory);

	// 현재 구매 처리 중인지 확인
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bIsPurchasing = false;

protected:
	virtual void BeginPlay() override;

	// 자판기 상품 정보를 가져올 DataTable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Data")
	UDataTable* ItemDataTable;

	// ShopItems의 Row Name을 이용해 DT_ItemData에서 상품 데이터를 불러옴
	bool LoadShopItemsFromTable();

	// 자판기의 외형을 담당하는 Static Mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VendingMachine")
	UStaticMeshComponent* VendingMachineMesh;
};