// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemData.h"
#include "ItemPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class DEAD_HOSPITAL_API AItemPickup : public AActor
{
	GENERATED_BODY()
	
public:		
	AItemPickup();

protected:
	virtual void BeginPlay() override;
	
	// 월드에서 보이는 아이템 모델
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* ItemMesh;

	// 플레이어가 아이템 근처에 왔는지 확인할 충돌 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USphereComponent* InteractionSphere;

	// 이 pickup이 어떤 아이템인지 저장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FItemData ItemData;

	// 현재 상호작용 범위 안에 들어온 엑터
	UPROPERTY()
	AActor* InteractingActor;

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);
};
