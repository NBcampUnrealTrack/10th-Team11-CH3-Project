#include "ItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "InventoryComponent.h"
#include "GameFramework/Character.h"


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

	// 인벤토리에 아이템 추가를 시도한다.
	if (Inventory->AddItem(ItemData)){
		UE_LOG(LogTemp, Warning,TEXT("Item picked up: %s"), *ItemData.ItemID.ToString());

		// 획득에 성공했으므로 월드에 있는 Pickup 제거
		Destroy();
	}
}