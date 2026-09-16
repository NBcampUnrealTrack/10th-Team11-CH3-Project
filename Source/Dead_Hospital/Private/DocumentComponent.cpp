#include "DocumentComponent.h"


UDocumentComponent::UDocumentComponent(){
	
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDocumentComponent::BeginPlay(){

	Super::BeginPlay();

	// ...
	
}

// 해당 문서를 이미 획득했는지 확인
bool UDocumentComponent::HasDocument(FName DocumentID) const{

	// 획득한 문서들을 하나씩 확인
	for (const FDocumentData& Document : Documents){

		// 같은 DocumentID를 가진 문서가 있으면 이미 획득한 상태
		if (Document.DocumentID == DocumentID){
			return true;
		}
	}

	// 끝까지 찾지 못했다면 아직 획득하지 않은 문서
	return false;
}

// 문서를 획득 목록에 추가
bool UDocumentComponent::AddDocument(const FDocumentData& NewDocument){

	// 잘못된 DocumentID는 추가하지 않음
	if (NewDocument.DocumentID.IsNone()){

		return false;
	}

	// 이미 획득한 문서라면 중복 추가하지 않음
	if (HasDocument(NewDocument.DocumentID)){

		return false;
	}

	// 획득한 문서 목록에 추가
	Documents.Add(NewDocument);

	// 문서 획득 사실을 UI 등에 전달
	OnDocumentAcquired.Broadcast(NewDocument.DocumentID, NewDocument.DocumentTitle);

	UE_LOG(LogTemp, Warning, TEXT("Document Added: %s"), *NewDocument.DocumentID.ToString());

	return true;
}


// 현재 획득한 모든 문서 목록 반환
const TArray<FDocumentData>& UDocumentComponent::GetDocuments() const{
	return Documents;
}

// DocumentID에 해당하는 문서 정보를 찾아 반환
bool UDocumentComponent::GetDocument(FName DocumentID, FDocumentData& OutDocument) const
{
	// 획득한 문서 목록을 하나씩 확인
	for (const FDocumentData& Document : Documents){

		// 찾으려는 DocumentID와 같다면
		if (Document.DocumentID == DocumentID){

			// 찾은 문서 정보를 OutDocument에 전달
			OutDocument = Document;

			return true;
		}
	}

	// 끝까지 찾지 못했다면 실패
	return false;
}
