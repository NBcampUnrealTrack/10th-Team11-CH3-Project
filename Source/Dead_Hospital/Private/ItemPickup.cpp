#include "ItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "InventoryComponent.h"
#include "Engine/DataTable.h"
#include "PlayerCharacter.h"

AItemPickup::AItemPickup(){

	PrimaryActorTick.bCanEverTick = false;

	// 아이템 메시 생성
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));

	// ItemMesh를 RootComponent로 사용
	RootComponent = ItemMesh;

	// 플레이어의 상호작용 Sweep이 아이템을 감지할 수 있도록 Query 충돌 활성화
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Visibility 채널의 상호작용 검사에 반응하도록 설정
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ItemMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

// Pickup이 가지고 있을 아이템 데이터 설정
void AItemPickup::SetItemData(const FItemData& NewItemData)
{
	ItemData = NewItemData;

	UE_LOG(LogTemp, Warning, TEXT("ItemPickup data set: %s / Quantity: %d"), *ItemData.ItemID.ToString(), ItemData.Quantity);
}

// DataTable의 Row Name을 이용해 ItemData를 설정
bool AItemPickup::LoadItemDataFromTable(){
	// DataTable이 설정되지 않았다면 실패
	if (!ItemDataTable){

		UE_LOG(LogTemp, Warning, TEXT("ItemPickup: ItemDataTable is not set"));
		return false;
	}

	// Row Name이 설정되지 않았다면 실패
	if (ItemRowName.IsNone()){

		UE_LOG(LogTemp, Warning, TEXT("ItemPickup: ItemRowName is not set"));
		return false;
	}

	// DataTable에서 ItemRowName에 해당하는 FItemData 검색
	const FItemData* FoundItemData = ItemDataTable->FindRow<FItemData>(ItemRowName, TEXT("ItemPickup LoadItemData"));

	// 해당 Row를 찾지 못했다면 실패
	if (!FoundItemData){

		UE_LOG(LogTemp, Warning,TEXT("ItemPickup: Failed to find Row: %s"), *ItemRowName.ToString());

		return false;
	}

	// 찾은 데이터를 현재 Pickup의 ItemData에 복사
	ItemData = *FoundItemData;

	UE_LOG(LogTemp, Warning, TEXT("ItemPickup loaded from DataTable: %s / Quantity: %d"), *ItemData.ItemID.ToString(), ItemData.Quantity);

	return true;
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();

	// DataTable과 Row Name이 설정되어 있다면 ItemData를 불러옴
	if (ItemDataTable && !ItemRowName.IsNone())
	{
		LoadItemDataFromTable();
	}
}

void AItemPickup::Interact_Implementation(AActor* PlayerActor){

	// 전달받은 플레이어가 없으면 종료
	if (!PlayerActor){
		return;
	}

	// 아이템 데이터가 정상적으로 설정되어 있는지 확인
	if (ItemData.ItemID.IsNone() || ItemData.Quantity <= 0 || ItemData.MaxStack <= 0){

		UE_LOG(LogTemp, Warning, TEXT("ItemPickup has invalid ItemData"));

		return;
	}

	// Flashlight는 일반 인벤토리에 추가하지 않고
	// PlayerCharacter의 손전등 기능을 직접 활성화
	if (ItemData.ItemID == FName(TEXT("Flashlight"))){

		APlayerCharacter* Player = Cast<APlayerCharacter>(PlayerActor);

		if (!Player){

			UE_LOG(LogTemp, Warning, TEXT("ItemPickup: PlayerCharacter not found"));

			return;
		}

		// 이미 손전등을 가지고 있다면 중복 획득 방지
		if (Player->HasFlashlight()){

			UE_LOG(LogTemp, Warning, TEXT("ItemPickup: Player already has Flashlight"));

			return;
		}

		// 플레이어에게 손전등 기능 지급
		Player->AcquireFlashlight();

		UE_LOG(LogTemp, Warning, TEXT("Flashlight Acquired"));

		// Destroy 전에 프롬프트 정리
		Player->ClearInteractionTarget();

		// 획득 완료 후 월드의 손전등 제거
		Destroy();

		return;
	}

	// 일반 아이템은 플레이어의 InventoryComponent를 찾음
	UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();

	if (!Inventory){

		UE_LOG(LogTemp, Warning, TEXT("InventoryComponent not found"));

		return;
	}

	// 인벤토리에 아이템 추가 시도
	if (Inventory->AddItem(ItemData)){

		// 아이템 추가에 성공했을 때만 획득 이벤트 발생
		Inventory->OnItemAcquired.Broadcast(ItemData.ItemID, ItemData.Quantity);

		UE_LOG(LogTemp, Warning, TEXT("Item Acquired / Item: %s / Quantity: %d"), *ItemData.ItemID.ToString(), ItemData.Quantity);

		// 아이템이 사라지기 전에 상호작용 대상과 HUD 프롬프트 정리
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(PlayerActor))
		{
			Player->ClearInteractionTarget();
		}

		// 획득이 완료되었으므로 월드의 아이템 제거
		Destroy();
	}
}

// 현재 아이템과 상호작용할 수 있는지 확인
bool AItemPickup::CanInteract_Implementation(AActor* Interactor) const
{
	// 상호작용 대상이 없으면 불가능
	if (!Interactor){

		return false;
	}

	// 아이템 데이터가 잘못되어 있으면 상호작용 불가능
	if (ItemData.ItemID.IsNone() || ItemData.Quantity <= 0 || ItemData.MaxStack <= 0){
		return false;
	}

	// 손전등일 경우
	if (ItemData.ItemID == FName(TEXT("Flashlight"))){

		const APlayerCharacter* Player = Cast<APlayerCharacter>(Interactor);

		// 플레이어가 아니면 획득 불가능
		if (!Player){
			return false;
		}

		// 이미 손전등을 가지고 있으면 다시 획득 불가능
		if (Player->HasFlashlight()){
			return false;
		}
	}

	return true;
}


// 은신 중에는 아이템 획득을 허용하지 않음
bool AItemPickup::IsAllowedWhileHiding_Implementation() const{

	return false;
}


// 플레이어 화면에 표시할 상호작용 문구
FText AItemPickup::GetInteractionText_Implementation(AActor* Interactor) const
{
	if (!ItemData.ItemName.IsEmpty())
	{
		return FText::Format(
			NSLOCTEXT("ItemPickup", "PickupItem", "Pick up {0}"),
			ItemData.ItemName
		);
	}

	return NSLOCTEXT(
		"ItemPickup",
		"Pickup",
		"Pick up item"
	);
}


// 강제로 상호작용을 해제할 때 호출
void AItemPickup::ForceRelease_Implementation(AActor* Interactor){

	// ItemPickup은 지속적으로 잡고 있는 상호작용이 아니므로
	// 별도로 해제할 상태가 없음
}