#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalStatuePuzzle.generated.h"

class UInventoryComponent;

UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalStatuePuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalStatuePuzzle();

	// 천사 몸통에 상호작용했을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Statue")
	bool PlaceAngelHead(AActor* Interactor);

	// 악마 몸통에 상호작용했을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Statue")
	bool PlaceDemonHead(AActor* Interactor);

protected:
	virtual void BeginPlay() override;

	// 필요한 아이템 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Statue")
	FName AngelHeadItemId = TEXT("AngelHead");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Statue")
	FName DemonHeadItemId = TEXT("DemonHead");

	// 현재 각각의 머리가 배치되었는지
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|Statue")
	bool bAngelHeadPlaced = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|Statue")
	bool bDemonHeadPlaced = false;

	// 같은 PZ04의 반대편 석상
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Statue")
	ADeadHospitalStatuePuzzle* OtherStatue;

	// 천사 머리가 배치되었는지 반환
	bool IsAngelHeadPlaced() const;

	// 악마 머리가 배치되었는지 반환
	bool IsDemonHeadPlaced() const;

	// 천사 머리가 성공적으로 배치됐을 때 BP에서 실행
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Statue")
	void OnAngelHeadPlaced();

	// 악마 머리가 성공적으로 배치됐을 때 BP에서 실행
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Statue")
	void OnDemonHeadPlaced();

	// 두 석상이 모두 완성됐을 때 BP에서 실행
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Statue")
	void OnBothStatuesCompleted();

private:
	// 실제 머리 배치 공통 처리
	bool PlaceHead(
		AActor* Interactor,
		FName RequiredItemId,
		bool& bPlacedState,
		bool bIsAngel);

	// 천사/악마가 모두 완성됐는지 확인
	void CheckPuzzleCompletion(AActor* Interactor);
};