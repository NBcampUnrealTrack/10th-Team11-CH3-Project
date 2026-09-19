#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Weapon,      // HandGun, Magnum
	Ammo,        // HandGunAmmo, MagnumAmmo
	Consumable,  // Bandage
	Throwable,   // 기존 코드 호환용, 나중에 사용 여부 확인 후 제거 가능
	Material,    // Gunpowder
	Tool,        // Flashlight
	Currency,    // Coin
	KeyItem      // CardKeyA, CardKeyB, MasterCardKey, Painting
};

USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
	GENERATED_BODY()

	// 아이템을 구분하기 위한 고유 ID
	// ex) HandGun, HandGunAmmo, Bandage
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	// UI에 표시할 아이템 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	// 화면에 표시할 아이템 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ItemName;

	// 아이템 종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemType ItemType = EItemType::Material;

	// 현재 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity = 1;

	// 한 슬롯에 들어갈 수 있는 최대 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxStack = 1;

	// 소비 아이템 효과량
	// ex) Bandage HP 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EffectAmount = 0.0f;

	// 상점 구매 가격
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Price = 0;
};

USTRUCT(BlueprintType)
struct FInventorySlot{
	GENERATED_BODY()

	// 인벤토리 슬롯 번호
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SlotIndex = -1;

	// 이 슬롯에 아이템이 들어있는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsEmpty = true;

	// 슬롯에 들어있는 아이템 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FItemData ItemData;
};

// 자판기에서 판매하는 상품 정보
USTRUCT(BlueprintType)
struct FShopItem{

	GENERATED_BODY()

	// DT_ItemData에서 가져올 상품의 Row Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemRowName;

	// 실제 판매할 아이템 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FItemData ItemData;

	// 현재 자판기에 남아있는 판매 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Stock = 0;
};

// 자판기 구매 결과
UENUM(BlueprintType)
enum class EPurchaseResult : uint8
{
	Success,          // 구매 성공
	InvalidItem,      // 잘못된 상품
	OutOfStock,       // 재고 없음
	NotEnoughCoin,    // Coin 부족
	InventoryFull     // 인벤토리 공간 부족
};