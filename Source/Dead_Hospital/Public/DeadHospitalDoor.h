#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalDoor();

	// 퍼즐 완료로 문 잠금 해제
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool UnlockFromPuzzle(FName SolvedPuzzleId);

	// 퍼즐 ID가 이 문과 맞는지 검사
	UFUNCTION(BlueprintPure, Category = "Door")
	bool CanUnlockFromPuzzle(FName PuzzleIdToCheck) const;

	// 카드리더 전용
	// 카드리더가 올바른 카드키를 확인한 뒤 호출
	// 성공하면 잠금을 해제하고 문을 바로 연다.
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool UnlockAndOpenFromKeyReader(FName KeyItemId);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool GetIsLocked() const { return IsLocked; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool GetIsOpen() const { return IsOpen; }

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 문 전체의 Root
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	USceneComponent* DoorRoot;

	// 실제 문 Mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName DoorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool StartsLocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Puzzle")
	FName RequiredPuzzleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Key")
	FName RequiredKeyItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Key")
	bool ConsumeKeyWhenUnlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText LockedInteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText OpenInteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText CloseInteractionText;

	// true = 열기 / false = 닫기
	// Blueprint Timeline이 이 이벤트를 받아 실제 문을 움직임
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorMovementRequested(bool ShouldOpen);

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorUnlocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorAccessDenied();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorStateRestored(bool RestoredLocked, bool RestoredOpen);

private:
	bool UnlockDoor();
	bool TryUnlockWithKey(AActor* Interactor);
	bool CanUseDoorInCurrentPhase() const;
	FName MakeUnlockedEventId() const;

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Door",
		meta = (AllowPrivateAccess = "true"))
	bool IsLocked = true;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Door",
		meta = (AllowPrivateAccess = "true"))
	bool IsOpen = false;
};