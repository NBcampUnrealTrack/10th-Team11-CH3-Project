// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ItemData.h"
#include "DeadHospitalPaintingPuzzle.generated.h"

class ADeadHospitalProgressionItem;
class UStaticMeshComponent;

/**
 * PZ-02는 "그림 제거"와 "그림 뒤 열쇠 획득"이라는 두 단계입니다.
 * 이 클래스는 첫 단계만 담당합니다. Player가 E로 그림을 조사하면 그림 아이템을
 * 인벤토리에 넣고, 제거 사실을 GameMode의 Event로 기록하고, 벽 뒤 열쇠를 공개합니다.
 * 두 번째 단계는 별도로 배치한 DeadHospitalProgressionItem이 담당합니다.
 * 열쇠를 실제로 얻은 순간에만 전체 PZ-02를 완료로 저장합니다.
 * L_MainLevel에 그림과 열쇠 Actor를 따로 놓고 HiddenKeyPickup을 연결해야 합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalPaintingPuzzle : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalPaintingPuzzle();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Painting Puzzle")
	UStaticMeshComponent* PaintingMesh;

	/**
	 * FItemData는 팀원의 InventoryComponent에 넘길 이름/종류/수량입니다.
	 * 여기에는 그림만 넣습니다. 뒤쪽 Key는 HiddenKeyPickup의 별도 ItemData입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Painting Puzzle")
	FItemData PaintingItemData;

	/**
	 * 실제 맵에 놓인 숨겨진 열쇠 Actor를 참조합니다. nullptr는 아직 연결하지 않았다는 뜻.
	 * BeginPlay에서 열쇠의 공개 조건을 "그림 제거 완료"로 자동 설정합니다.
	 * 빠뜨리면 그림만 사라지고 열쇠를 못 얻으므로 그림 제거 자체를 거부합니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Painting Puzzle")
	ADeadHospitalProgressionItem* HiddenKeyPickup = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Painting Puzzle")
	FName PaintingRemovedEventId = TEXT("PZ02_PaintingRemoved");

	// EventId는 "그림을 이미 떼어냈다"를 체크포인트에서 기억할 이름입니다.

	/** 그림 뒤에서 얻는 필수 Key의 확정 Item ID입니다. GameMode 보호 목록과 같은 값을 사용합니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Painting Puzzle")
	FName HiddenKeyItemId = TEXT("PZ02_HiddenKey");

	/** Key를 실제로 획득했을 때 완료되는 PZ-02의 고유 Puzzle ID입니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Painting Puzzle")
	FName PuzzleCompletionId = TEXT("PZ_02_PaintingKey");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/**
	 * C++은 그림 Actor를 숨깁니다. 그림 뒤 벽의 구멍 메시나 소리는 별도이므로
	 * Blueprint 자식에서 이 이벤트에 "구멍 공개" 연출을 연결해야 합니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Painting Puzzle")
	void OnPaintingRemoved();

	UFUNCTION(BlueprintImplementableEvent, Category = "Painting Puzzle")
	void OnPaintingRemovalFailed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Painting Puzzle")
	void OnPaintingStateRestored(bool WasRemoved);

private:
	void ApplyRemovedState();

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Painting Puzzle", meta = (AllowPrivateAccess = "true"))
	bool PaintingRemoved = false;
};
