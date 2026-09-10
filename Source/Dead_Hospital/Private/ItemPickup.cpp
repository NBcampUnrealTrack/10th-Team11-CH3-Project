#include "ItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

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
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();
}
