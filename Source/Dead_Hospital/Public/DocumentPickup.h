#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DocumentData.h"
#include "PlayerInterface.h"
#include "DocumentPickup.generated.h"

class UStaticMeshComponent;
class UDataTable;

UCLASS()
class DEAD_HOSPITAL_API ADocumentPickup : public AActor, public IPlayerInterface
{
	GENERATED_BODY()

public:
	ADocumentPickup();

	// PlayerInterface 상호작용
	virtual void Interact_Implementation(AActor* Interactor) override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual bool IsAllowedWhileHiding_Implementation() const override;

	virtual FText GetInteractionText_Implementation(AActor* Interactor) const override;

	virtual void ForceRelease_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	// 월드에서 보이는 문서 모델
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Document")
	UStaticMeshComponent* DocumentMesh;

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
};