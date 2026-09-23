#include "DeadHospitalCardReader.h"

#include "DeadHospitalDoor.h"
#include "InventoryComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

ADeadHospitalCardReader::ADeadHospitalCardReader()
{
	PrimaryActorTick.bCanEverTick = false;

	Root =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("Root"));

	SetRootComponent(Root);

	ReaderMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("ReaderMesh"));

	ReaderMesh->SetupAttachment(Root);

	InteractionText =
		FText::FromString(TEXT("E 키로 카드키 사용"));
}

void ADeadHospitalCardReader::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수 확인
	if (!IsValid(ConnectedDoor))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: ConnectedDoor is not set."),
			*GetName());
	}

	if (RequiredKeyItemId.IsNone())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: RequiredKeyItemId is None."),
			*GetName());
	}
}

void ADeadHospitalCardReader::Interact_Implementation(
	AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	// Player의 Inventory 찾기
	UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		OnCardRejected();
		return;
	}

	// 필요한 카드가 있는지 검사
	if (!Inventory->HasItem(RequiredKeyItemId))
	{
		OnCardRejected();
		return;
	}

	// 연결된 문이 없으면 실패
	if (!IsValid(ConnectedDoor))
	{
		OnCardRejected();
		return;
	}

	// 카드리더가 확인한 Key ID를 Door에 전달
	if (!ConnectedDoor->UnlockAndOpenFromKeyReader(
		RequiredKeyItemId))
	{
		OnCardRejected();
		return;
	}

	// 성공
	OnCardAccepted();
}

bool ADeadHospitalCardReader::CanInteract_Implementation(
	AActor* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return false;
	}

	const APawn* PlayerPawn =
		Cast<APawn>(Interactor);

	return
		IsValid(PlayerPawn)
		&& PlayerPawn->IsPlayerControlled();
}

FText ADeadHospitalCardReader::GetInteractionText_Implementation() const
{
	return InteractionText;
}