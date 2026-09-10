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

protected:
	virtual void BeginPlay() override;
};