#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalPaintingFramePuzzle.generated.h"

class UInventoryComponent;
class ADeadHospitalPaintingFramePuzzle;

/**
 * PZ06 그림 배치 퍼즐의 액자 하나를 담당하는 Actor.
 *
 * 각 액자마다 RequiredPaintingItemId를 하나 지정합니다.
 * 플레이어가 액자에 상호작용하면:
 *
 * 1. 해당 그림을 가지고 있는지 확인
 * 2. 그림 아이템 소비
 * 3. 이 액자를 배치 완료 상태로 변경
 * 4. Blueprint에서 그림 Mesh 표시
 * 5. 연결된 액자 3개가 모두 완료되면 PZ06 완료 및 문 잠금 해제
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalPaintingFramePuzzle
	: public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalPaintingFramePuzzle();

	/**
	 * 플레이어가 이 액자에 맞는 그림을 가지고 있으면 배치합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Painting")
	bool PlacePainting(AActor* Interactor);

	/**
	 * 현재 이 액자에 그림이 배치됐는지 확인합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Puzzle|Painting")
	bool IsPaintingPlaced() const;

protected:
	virtual void BeginPlay() override;

	/**
	 * 이 액자에 들어가야 하는 그림 ItemID.
	 *
	 * 예:
	 * Painting
	 * PaintingWoman
	 * PaintingPottery
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Painting")
	FName RequiredPaintingItemId = NAME_None;

	/**
	 * 나머지 PZ06 액자들.
	 *
	 * 각 액자에서 자기 자신을 제외한
	 * 다른 액자 2개를 지정합니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Painting")
	TArray<ADeadHospitalPaintingFramePuzzle*> OtherFrames;

	/**
	 * 이 액자에 그림이 정상적으로 배치됐을 때 호출.
	 *
	 * Blueprint에서 숨겨둔 그림 Mesh를
	 * 표시하는 데 사용합니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Painting")
	void OnPaintingPlaced();

	/**
	 * 필요한 그림을 가지고 있지 않거나
	 * 배치에 실패했을 때 호출.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Painting")
	void OnPaintingPlacementFailed();

private:
	/**
	 * 이 액자에 그림이 이미 배치됐는지.
	 */
	bool bPaintingPlaced = false;

	/**
	 * 자신 + 연결된 액자들이 모두 완료됐는지 검사하고
	 * 전부 완료됐으면 PZ06을 완료합니다.
	 */
	void CheckAllFramesCompleted(AActor* Interactor);
};