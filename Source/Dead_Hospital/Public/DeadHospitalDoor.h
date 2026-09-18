// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalDoor.generated.h"

class UStaticMeshComponent;

/**
 * 플레이어가 E 키로 여닫는 문입니다. 잠긴 문에는 "필요한 퍼즐을 푼다" 또는
 * "필요한 Key를 가진다" 중 한 가지 조건을 Details에서 설정합니다.
 * C++은 조건 확인/잠금 상태/체크포인트 기록을 담당합니다. 문 회전, 소리,
 * 잠긴 문 피드백은 아래 BlueprintImplementableEvent를 자식 BP에 연결해야 보입니다.
 *
 * 문과 퍼즐은 별개의 Actor입니다. PZ-01의 RequiredPuzzleId와 키패드 PuzzleId를
 * 같은 값으로 맞추고, 키패드 ConnectedDoor도 이 문을 가리키도록 설정해야 합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalDoor();

	/** 풀었다고 전달된 퍼즐 ID가 이 문 조건과 일치하고 GameMode에 완료됐을 때만 잠금 해제. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool UnlockFromPuzzle(FName SolvedPuzzleId);

	/** 퍼즐 완료 기록을 남기기 전에 연결 실수가 없는지 미리 검사할 때 사용합니다. */
	UFUNCTION(BlueprintPure, Category = "Door")
	bool CanUnlockFromPuzzle(FName PuzzleIdToCheck) const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UStaticMeshComponent* DoorMesh;

	/**
	 * 문마다 다른 ID입니다. 잠금이 풀리면 DoorId + "_Unlocked"라는 Event를 저장합니다.
	 * ID가 None이면 문이 열렸는지 저장할 수 없어 잠금 해제가 거부됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName DoorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool StartsLocked = true;

	/** 이 문을 여는 퍼즐의 정확한 ID입니다. 다른 퍼즐 ID가 실수로 전달되면 해제하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Puzzle")
	FName RequiredPuzzleId = NAME_None;

	/** 퍼즐 문이면 None, PZ-02 Key 문이면 DT_ItemData의 최종 ID인 "Key"를 입력합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Key")
	FName RequiredKeyItemId = NAME_None;

	/**
	 * true면 잠금을 해제할 때 Key 한 개를 ConsumeKeyItem()으로 소비합니다.
	 * PZ-02의 최종 ID "Key"는 기존 맵 Actor에 이 값이 false로 저장되어 있어도 반드시 소비합니다.
	 * Key의 일반 삭제 금지는 플레이어가 임의로 버리는 것만 막으며, 정상적인 문 사용은 허용합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Key")
	bool ConsumeKeyWhenUnlocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText LockedInteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText OpenInteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText CloseInteractionText;

	/** true이면 열기, false이면 닫기 애니메이션을 실행하는 Blueprint 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorMovementRequested(bool ShouldOpen);

	/** 자물쇠 해제음과 표시등 변경을 한 번만 실행하는 Blueprint 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorUnlocked();

	/** 필요한 Key가 없거나 아직 퍼즐을 풀지 않은 경우 UI 피드백을 표시하는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorAccessDenied();

	/** 체크포인트 복구 후 문 외형을 잠금 상태와 맞추는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnDoorStateRestored(bool RestoredLocked, bool RestoredOpen);

private:
	/**
	 * 퍼즐 완료 또는 정확한 Key 보유 검사를 통과한 내부 코드만 호출할 수 있는 실제 잠금 해제 함수입니다.
	 * Blueprint가 이 함수를 바로 호출해 필수 조건을 건너뛰지 못하도록 private으로 숨겨 둡니다.
	 */
	bool UnlockDoor();
	bool TryUnlockWithKey(AActor* Interactor);
	bool CanUseDoorInCurrentPhase() const;
	FName MakeUnlockedEventId() const;

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door", meta = (AllowPrivateAccess = "true"))
	/** 실제 게임 중 잠겼는지. StartsLocked는 시작 설정이고 이 값은 현재 상태입니다. */
	bool IsLocked = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door", meta = (AllowPrivateAccess = "true"))
	/** 잠금과 다르게 문짝이 지금 열려 있는지 표시합니다. */
	bool IsOpen = false;
};
