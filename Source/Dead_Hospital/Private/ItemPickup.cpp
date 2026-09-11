#include "ItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "InventoryComponent.h"


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

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();

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
	if (OtherActor && OtherActor != this) {
		InteractingActor = OtherActor;

		UE_LOG(LogTemp, Warning, TEXT("Entered item interaction range"));
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
	if (OtherActor == InteractingActor) {
		InteractingActor = nullptr;

		UE_LOG(LogTemp, Warning, TEXT("Exited item interaction range"));
	}
}

void AItemPickup::Interact(AActor* PlayerActor) {
	
	//전달받은 플레이어가 없으면 종료
	if (!PlayerActor) {
		return;
	}

	// 플레이어에서 InventoryComponent가 있는지 찾기
	UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();

	// 인벤토리가 있으면
	if (Inventory) {

		// 인벤토리에 추가
		if (Inventory->AddItem(ItemData)) {
			UE_LOG(LogTemp, Warning, TEXT("Item picked up: %s"), *ItemData.ItemID.ToString());

			//획득한 아이템을 월드에서 제거
			Destroy();
		}
	}
}