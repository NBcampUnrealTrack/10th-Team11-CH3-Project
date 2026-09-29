#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DocumentData.generated.h"

// 문서 한 개의 정보를 저장하는 구조체
USTRUCT(BlueprintType)
struct FDocumentData : public FTableRowBase
{
	GENERATED_BODY()

	// 문서를 구분하기 위한 고유 ID
	// 예: Document_01, Document_02
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Document")
	FName DocumentID;

	// 문서 목록에 표시할 제목
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Document")
	FText DocumentTitle;

	// 플레이어가 실제로 읽게 될 문서 내용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Document",
		meta = (MultiLine = "true"))
	FText DocumentContent;
};