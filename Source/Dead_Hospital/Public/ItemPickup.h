#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemData.h"
#include "PlayerInterface.h"
#include "ItemPickup.generated.h"

class UStaticMeshComponent;
class UDataTable;

UCLASS()
class DEAD_HOSPITAL_API AItemPickup : public AActor, public IPlayerInterface
{
	GENERATED_BODY()
	
public:		
	AItemPickup();

	// 플레이어가 아이템과 상호작용
	virtual void Interact_Implementation(AActor* Interactor) override;

	// 현재 아이템과 상호작용할 수 있는지 확인
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	// 은신 중에는 아이템 획득 불가
	virtual bool IsAllowedWhileHiding_Implementation() const override;

	// 화면에 표시할 상호작용 문구
	virtual FText GetInteractionText_Implementation(AActor* Interactor) const override;

	// 강제로 상호작용을 해제할 때 호출
	virtual void ForceRelease_Implementation(AActor* Interactor) override;

	// 외부에서 Pickup의 아이템 데이터를 설정
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItemData(const FItemData& NewItemData);

protected:
	virtual void BeginPlay() override;
	
	// 월드에서 보이는 아이템 모델
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* ItemMesh;

	// 아이템 정보를 가져올 DataTable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Data")
	UDataTable* ItemDataTable;

	// DataTable에서 가져올 아이템의 Row Name
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Data")
	FName ItemRowName;

	// 이 pickup이 어떤 아이템인지 저장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FItemData ItemData;

	// DataTable의 Row Name을 이용해 ItemData를 설정
	bool LoadItemDataFromTable();
};
