#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DocumentData.h"
#include "DocumentPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UDataTable;

UCLASS()
class DEAD_HOSPITAL_API ADocumentPickup : public AActor
{
	GENERATED_BODY()

public:
	ADocumentPickup();

	// 플레이어가 문서와 상호작용
	UFUNCTION(BlueprintCallable, Category = "Document")
	void Interact(AActor* PlayerActor);

protected:
	virtual void BeginPlay() override;

	// 월드에서 보이는 문서 모델
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Document")
	UStaticMeshComponent* DocumentMesh;

	// 플레이어가 문서 근처에 있는지 확인하는 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Document")
	USphereComponent* InteractionSphere;

	// 문서 정보를 가져올 DataTable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document|Data")
	UDataTable* DocumentDataTable;

	// DataTable에서 가져올 문서의 Row Name
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document|Data")
	FName DocumentRowName;

	// 실제 문서 정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Document")
	FDocumentData DocumentData;

	// DataTable에서 문서 정보 불러오기
	bool LoadDocumentDataFromTable();

	// 현재 상호작용 범위 안에 있는 플레이어
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