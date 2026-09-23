#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalCardReader.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class ADeadHospitalDoor;

UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalCardReader
	: public AActor,
	public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalCardReader();

	virtual void Interact_Implementation(
		AActor* Interactor) override;

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	// Root
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CardReader")
	USceneComponent* Root;

	// 카드리더 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CardReader")
	UStaticMeshComponent* ReaderMesh;

	// 이 카드리더가 열 문
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CardReader")
	ADeadHospitalDoor* ConnectedDoor;

	// 필요한 카드키
	// CardKeyA / CardKeyB / MasterCardKey
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CardReader")
	FName RequiredKeyItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	// 카드 인증 성공 연출
	// 초록불 / 삑 소리 등에 사용
	UFUNCTION(BlueprintImplementableEvent, Category = "CardReader")
	void OnCardAccepted();

	// 카드가 없거나 잘못된 카드일 때
	// 빨간불 / 실패음 등에 사용
	UFUNCTION(BlueprintImplementableEvent, Category = "CardReader")
	void OnCardRejected();
};