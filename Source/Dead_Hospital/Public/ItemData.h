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
	KeyItem      // Key, Painting
};

USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
	GENERATED_BODY()

	// 아이템을 구분하기 위한 고유 ID
	// ex) HandGun, HandGunAmmo, Bandage
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

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
struct FInventorySlot
{
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
