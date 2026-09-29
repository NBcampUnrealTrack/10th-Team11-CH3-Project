#include "ZombiePatrolPoint.h"
#include "Components/BillboardComponent.h"

AZombiePatrolPoint::AZombiePatrolPoint()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
	EditorBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	EditorBillboard->SetupAttachment(RootComponent);
#endif
}

