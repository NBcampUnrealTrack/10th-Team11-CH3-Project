#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZombieDropTable.generated.h"

USTRUCT(BlueprintType)
struct DEAD_HOSPITAL_API FZombieDropEntry : public FTableRowBase
{
	GENERATED_BODY()
	
	//드랍될 아이템의 ItemID (ItemData DataTable의 Row Name과 매칭). "드랍 없음" 항목은 None으로 비워둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop")
	FName ItemID = NAME_None;
	//이 항목이 뽑힐 가중치. 전체 가중치 합 대비 비율로 확률이 정해진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop")
	float Weight = 1.0f;
	//드랍될 수량(권총 탄약 10발, 붕대 1개, 화약 1개 등 항목마다 다르게 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop")
	int32 DropQuantity = 1;
};
