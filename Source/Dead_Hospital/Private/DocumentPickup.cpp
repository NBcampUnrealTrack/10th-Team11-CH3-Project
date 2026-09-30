#include "DocumentPickup.h"
#include "Components/StaticMeshComponent.h"
#include "DocumentComponent.h"
#include "Engine/DataTable.h"
#include "DeadHospitalGameMode.h"

ADocumentPickup::ADocumentPickup(){
	// 매 프레임 Tick이 필요하지 않으므로 비활성화
	PrimaryActorTick.bCanEverTick = false;

	// 월드에서 보일 문서 Mesh 생성
	DocumentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DocumentMesh"));

	// DocumentMesh를 RootComponent로 사용
	RootComponent = DocumentMesh;

	// Player의 상호작용 Sweep이 문서를 감지할 수 있도록 Query 충돌 활성화
	DocumentMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 다른 충돌 채널은 무시
	DocumentMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Player가 사용하는 Visibility 채널에는 반응
	DocumentMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADocumentPickup::BeginPlay()
{
	Super::BeginPlay();

	// DataTable과 Row Name이 설정되어 있다면 문서 데이터를 불러옴
	if (DocumentDataTable && !DocumentRowName.IsNone())
	{
		LoadDocumentDataFromTable();
	}

	// 체크포인트 복구 이벤트 연결
	if (ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.AddDynamic(
			this,
			&ADocumentPickup::HandleCheckpointRestored
		);
	}
}

void ADocumentPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(
			this,
			&ADocumentPickup::HandleCheckpointRestored
		);
	}

	Super::EndPlay(EndPlayReason);
}

void ADocumentPickup::HandleCheckpointRestored(FName CheckpointId)
{
	RefreshDocumentState();
}

void ADocumentPickup::RefreshDocumentState()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	if (!IsValid(PlayerController))
	{
		return;
	}

	APawn* PlayerPawn = PlayerController->GetPawn();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	UDocumentComponent* DocumentComponent =
		PlayerPawn->FindComponentByClass<UDocumentComponent>();

	if (!IsValid(DocumentComponent))
	{
		return;
	}

	// 체크포인트 복구 후 플레이어가 이 문서를 가지고 있으면 숨기고,
	// 가지고 있지 않으면 월드에 다시 표시
	const bool bHasDocument =
		DocumentComponent->HasDocument(DocumentData.DocumentID);

	SetDocumentActive(!bHasDocument);
}

void ADocumentPickup::SetDocumentActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
}

// 플레이어가 문서와 상호작용
void ADocumentPickup::Interact_Implementation(AActor* PlayerActor){

	// 전달받은 Actor가 없다면 실패
	if (!PlayerActor){
		return;
	}

	// 플레이어에게 붙어있는 DocumentComponent 찾기
	UDocumentComponent* DocumentComponent = PlayerActor->FindComponentByClass<UDocumentComponent>();

	if (!DocumentComponent){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: DocumentComponent not found"));

		return;
	}

	// 문서 데이터가 정상인지 확인
	if (DocumentData.DocumentID.IsNone()){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: Invalid DocumentData"));

		return;
	}

	// 문서 획득 시도
	if (DocumentComponent->AddDocument(DocumentData))
	{
		UE_LOG(LogTemp, Warning, TEXT("Document acquired: %s"), *DocumentData.DocumentID.ToString());

		// Actor를 삭제하지 않고 숨김 + 충돌 비활성화
		// 체크포인트 복구 시 다시 나타날 수 있도록 유지
		SetDocumentActive(false);
	}
}

// 현재 문서와 상호작용할 수 있는지 확인
bool ADocumentPickup::CanInteract_Implementation(AActor* Interactor) const{

	// 상호작용하려는 Actor가 없으면 불가능
	if (!Interactor){
		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("DOCUMENT DEBUG - DocumentID: [%s]"),
		*DocumentData.DocumentID.ToString()
	);

	// 문서 데이터가 정상적이지 않으면 상호작용 불가능
	if (DocumentData.DocumentID.IsNone()){
		return false;
	}

	return true;
}


// 은신 중에는 문서 획득을 허용하지 않음
bool ADocumentPickup::IsAllowedWhileHiding_Implementation() const{
	return false;
}


// 플레이어 화면에 표시할 상호작용 문구
FText ADocumentPickup::GetInteractionText_Implementation(AActor* Interactor) const{

	if (!DocumentData.DocumentTitle.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("DocumentPickup", "PickupDocument", "Read {0}"), DocumentData.DocumentTitle);
	}

	return NSLOCTEXT(
		"DocumentPickup",
		"ReadDocument",
		"Read document"
	);
}


// 강제로 상호작용을 해제할 때 호출
void ADocumentPickup::ForceRelease_Implementation(AActor* Interactor){
	// DocumentPickup은 한 번 상호작용하면 획득되는 방식이므로
	// 별도로 해제할 상태가 없음
}

// DataTable의 Row Name을 이용해 DocumentData를 설정
bool ADocumentPickup::LoadDocumentDataFromTable(){

	// Document DataTable이 설정되지 않았다면 실패
	if (!DocumentDataTable){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: DocumentDataTable is not set"));

		return false;
	}

	// Row Name이 설정되지 않았다면 실패
	if (DocumentRowName.IsNone()){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: DocumentRowName is not set"));

		return false;
	}

	// DataTable에서 DocumentRowName에 해당하는 문서 검색
	const FDocumentData* FoundDocumentData = DocumentDataTable->FindRow<FDocumentData>(DocumentRowName, TEXT("DocumentPickup LoadDocumentData"));

	// 해당 Row를 찾지 못했다면 실패
	if (!FoundDocumentData){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: Failed to find Row: %s"), *DocumentRowName.ToString());

		return false;
	}

	// 찾은 문서 데이터를 현재 Pickup에 복사
	DocumentData = *FoundDocumentData;

	UE_LOG(LogTemp, Warning, TEXT("DocumentPickup loaded from DataTable: %s"), *DocumentData.DocumentID.ToString());

	return true;
}