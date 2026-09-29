#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DocumentData.h"
#include "DocumentComponent.generated.h"

// 문서를 획득했을 때 발생하는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnDocumentAcquired,
	FName, DocumentID,
	FText, DocumentTitle
);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEAD_HOSPITAL_API UDocumentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDocumentComponent();

	// 새로운 문서를 획득했을 때 UI 등에 알려주는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Document")
	FOnDocumentAcquired OnDocumentAcquired;

	// 문서 획득
	UFUNCTION(BlueprintCallable, Category = "Document")
	bool AddDocument(const FDocumentData& NewDocument);

	// 해당 문서를 이미 획득했는지 확인
	UFUNCTION(BlueprintPure, Category = "Document")
	bool HasDocument(FName DocumentID) const;

	// 현재 획득한 모든 문서 반환
	UFUNCTION(BlueprintPure, Category = "Document")
	const TArray<FDocumentData>& GetDocuments() const;

	// DocumentID에 해당하는 문서 정보를 찾아 반환
	UFUNCTION(BlueprintCallable, Category = "Document")
	bool GetDocument(FName DocumentID, FDocumentData& OutDocument) const;

protected:
	virtual void BeginPlay() override;

	// 플레이어가 획득한 문서 목록
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Document")
	TArray<FDocumentData> Documents;
};