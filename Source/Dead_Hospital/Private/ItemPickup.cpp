#include "ItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "InventoryComponent.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"


AItemPickup::AItemPickup(){

	PrimaryActorTick.bCanEverTick = false;

	// 충돌 영역 생성
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));

	RootComponent = InteractionSphere;

	// 상호작용 범위
	InteractionSphere->SetSphereRadius(150.0f);

	// 아이템 메시 생성
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));

	ItemMesh->SetupAttachment(RootComponent);

	// 아이템 메시 자체는 충돌하지 않게 설정
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 현재 상호작용 중인 액터는 처음에는 없음
	InteractingActor = nullptr;
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

	// 플레이어가 Sphere 안으로 들어왔을 때
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AItemPickup::OnSphereBeginOverlap);

	// 플레이어가 Sphere 밖으로 나갔을 때
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AItemPickup::OnSphereEndOverlap);

}

// Sphere 안으로 들어왔을 때 실행
void AItemPickup::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (ACharacter* Player = Cast<ACharacter>(OtherActor)){
		// 플레이어가 조종하는 캐릭터일 때만 상호작용 대상으로 저장
		if (Player->IsPlayerControlled())
		{
			InteractingActor = Player;

			UE_LOG(LogTemp, Warning, TEXT("Player entered item interaction range"));
		}
	}
}

// Sphere 밖으로 나갔을 때 실행
void AItemPickup::OnSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (OtherActor == InteractingActor)
	{
		InteractingActor = nullptr;

		UE_LOG(LogTemp, Warning,TEXT("Player exited item interaction range"));
	}
}

void AItemPickup::Interact(AActor* PlayerActor) {
	
	//전달받은 플레이어가 없으면 종료
	if (!PlayerActor) {
		return;
	}

	// 현재 InteractionSphere 안에 들어와 있는 플레이어가 아니라면
	// 아이템을 획득하지 못하도록 막는다.
	if (PlayerActor != InteractingActor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Player is not in interaction range"));

		return;
	}

	// 플레이어에서 InventoryComponent가 있는지 찾기
	UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();

	// InventoryComponent가 없다면 아이템을 추가할 수 없으므로 종료한다.
	if (!Inventory){
		UE_LOG(LogTemp, Warning, TEXT("InventoryComponent not found"));

		return;
	}

	// 아이템 데이터가 정상적으로 설정되어 있는지 확인
	if (ItemData.ItemID.IsNone() || ItemData.Quantity <= 0 || ItemData.MaxStack <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemPickup has invalid ItemData"));

		return;
	}

	// 인벤토리에 아이템 추가 시도
	if (Inventory->AddItem(ItemData)){

		// 아이템 추가에 성공했을 때만 획득 이벤트 발생
		Inventory->OnItemAcquired.Broadcast(ItemData.ItemID, ItemData.Quantity);

		UE_LOG(LogTemp, Warning, TEXT("Item Acquired / Item: %s / Quantity: %d"), *ItemData.ItemID.ToString(), ItemData.Quantity);

		// 획득이 완료되었으므로 월드의 아이템 제거
		Destroy();
	}
}