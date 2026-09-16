#include "DocumentPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "DocumentComponent.h"
#include "Engine/DataTable.h"

ADocumentPickup::ADocumentPickup(){
	// 매 프레임 Tick이 필요하지 않으므로 비활성화
	PrimaryActorTick.bCanEverTick = false;

	// 플레이어가 문서 근처에 왔는지 확인할 충돌 영역 생성
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));

	// 충돌 영역을 RootComponent로 설정
	RootComponent = InteractionSphere;

	// 상호작용 범위 설정
	InteractionSphere->SetSphereRadius(150.0f);

	// 월드에서 보일 문서 Mesh 생성
	DocumentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DocumentMesh"));

	// Mesh를 충돌 영역에 부착
	DocumentMesh->SetupAttachment(RootComponent);

	// 문서 Mesh 자체의 충돌은 사용하지 않음
	DocumentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 처음에는 상호작용 중인 플레이어가 없음
	InteractingActor = nullptr;
}

void ADocumentPickup::BeginPlay()
{
	Super::BeginPlay();

	// DataTable과 Row Name이 설정되어 있다면 문서 데이터를 불러옴
	if (DocumentDataTable && !DocumentRowName.IsNone()){
		LoadDocumentDataFromTable();
	}

	// 플레이어가 상호작용 범위에 들어왔을 때 이벤트 연결
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADocumentPickup::OnSphereBeginOverlap);

	// 플레이어가 상호작용 범위에서 나갔을 때 이벤트 연결
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ADocumentPickup::OnSphereEndOverlap);
}

// 플레이어가 문서의 상호작용 범위 안에 들어왔을 때
void ADocumentPickup::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 들어온 Actor가 플레이어 Character인지 확인
	ACharacter* Player = Cast<ACharacter>(OtherActor);

	if (Player && Player->IsPlayerControlled()){

		// 현재 상호작용 가능한 플레이어 저장
		InteractingActor = Player;

		UE_LOG(LogTemp, Warning, TEXT("Player entered DocumentPickup range"));
	}
}


// 플레이어가 문서의 상호작용 범위에서 나갔을 때
void ADocumentPickup::OnSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// 범위를 나간 Actor가 현재 저장된 플레이어라면 초기화
	if (OtherActor == InteractingActor){

		InteractingActor = nullptr;

		UE_LOG(LogTemp, Warning, TEXT("Player left DocumentPickup range"));
	}
}

// 플레이어가 문서와 상호작용
void ADocumentPickup::Interact(AActor* PlayerActor){

	// 전달받은 Actor가 없다면 실패
	if (!PlayerActor){
		return;
	}

	// 현재 상호작용 범위 안에 있는 플레이어인지 확인
	if (PlayerActor != InteractingActor){

		UE_LOG(LogTemp, Warning, TEXT("DocumentPickup: Player is not in interaction range"));

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
	if (DocumentComponent->AddDocument(DocumentData)){

		UE_LOG(LogTemp, Warning, TEXT("Document acquired: %s"), *DocumentData.DocumentID.ToString());

		// 획득 성공 후 월드의 문서 제거
		Destroy();
	}
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