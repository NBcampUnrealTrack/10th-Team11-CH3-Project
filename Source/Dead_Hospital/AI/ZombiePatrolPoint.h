#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombiePatrolPoint.generated.h"

UCLASS()
class DEAD_HOSPITAL_API AZombiePatrolPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	AZombiePatrolPoint();

private:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere)
	class UBillboardComponent* EditorBillboard;
#endif
};
