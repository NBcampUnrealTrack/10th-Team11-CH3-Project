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

void ADeadHospitalCardReader::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		UE_LOG(LogTemp, Warning, TEXT("PZ07 DEBUG: CanInteract FAILED"));
		return;
	}

	UInventoryComponent* Inventory =
		Interactor->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		UE_LOG(LogTemp, Warning, TEXT("PZ07 DEBUG: Inventory FAILED"));
		OnCardRejected();
		return;
	}

	if (!Inventory->HasItem(RequiredKeyItemId))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PZ07 DEBUG: Key FAILED - RequiredKeyItemId = %s"),
			*RequiredKeyItemId.ToString());

		OnCardRejected();
		return;
	}

	if (!IsValid(ConnectedDoor))
	{
		UE_LOG(LogTemp, Warning, TEXT("PZ07 DEBUG: ConnectedDoor FAILED"));
		OnCardRejected();
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("PZ07 DEBUG: Calling Door - Key = %s, Door = %s"),
		*RequiredKeyItemId.ToString(),
		*ConnectedDoor->GetName());

	if (!ConnectedDoor->UnlockAndOpenFromKeyReader(RequiredKeyItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("PZ07 DEBUG: Door REJECTED"));
		OnCardRejected();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PZ07 DEBUG: SUCCESS"));
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