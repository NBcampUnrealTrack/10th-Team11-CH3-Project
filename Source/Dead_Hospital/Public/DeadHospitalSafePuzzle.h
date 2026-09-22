#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalSafePuzzle.generated.h"

/**
 * PZ02 매그넘 금고 퍼즐
 * 비밀번호 3178을 입력하면 PZ02를 완료합니다.
 * 실제 금고가 열리는 연출은 Blueprint의 OnPuzzleSolved에서 처리합니다.
 */

UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalSafePuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalSafePuzzle();

	// 금고 UI에서 확인 버튼을 눌렀을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Safe")
	bool SubmitCode(AActor* Interactor, const FString& EnteredCode);

protected:

	// PZ02 금고 비밀번호
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Safe")
	FString CorrectCode = TEXT("3178");

	// 정답 입력 시 Blueprint에서 사용할 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Safe")
	void OnCodeAccepted();

	// 오답 입력 시 Blueprint에서 사용할 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Safe")
	void OnCodeRejected(const FString& EnteredCode);
};