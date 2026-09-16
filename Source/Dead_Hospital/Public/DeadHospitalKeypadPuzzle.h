// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalKeypadPuzzle.generated.h"

/**
 * PZ-01 "피로 적힌 비밀번호 426"의 정답을 판정하는 퍼즐 Actor입니다.
 * Player가 벽의 숫자를 보고 키패드 UI에 입력하면 UI가 SubmitCode를 호출합니다.
 * 이 Actor가 426인지 비교하고, 성공 시 GameMode에 PZ-01 완료를 저장하고 문을 풉니다.
 * UI는 숫자 버튼과 소리/색 표시를 맡고 정답 판정은 여기에서 하도록 분리했습니다.
 * 이렇게 나누면 UI 디자인을 변경해도 426 판정과 저장 규칙이 달라지지 않습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalKeypadPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalKeypadPuzzle();

	/**
	 * UI의 확인 버튼에서 호출합니다. Interactor는 입력한 Player,
	 * EnteredCode는 버튼으로 모은 글자(예: "426")입니다. 성공일 때만 true를 돌려줍니다.
	 * FString은 글자를 보관하는 Unreal 자료형이고 숫자 426과는 타입이 다릅니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Keypad")
	bool SubmitCode(AActor* Interactor, const FString& EnteredCode);

protected:
	/** 부모 퍼즐의 검사에 "연결된 문이 있어야 한다"는 조건을 추가합니다. */
	virtual bool CanCompletePuzzle(AActor* Interactor) const override;

	/**
	 * 정답 문자열은 "426"입니다. 숫자 하나씩이 아니라 전체 글자를 비교하므로
	 * "0426"이나 "4260"은 틀립니다. VisibleDefaultsOnly는 에디터에서 볼 수 있지만
	 * 배치할 때 실수로 다른 정답을 입력하지 못하게 하는 표시 방식입니다.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Keypad")
	FString CorrectCode = TEXT("426");

	/** C++가 성공 판정한 뒤 Blueprint에서 녹색 표시/성공음을 넣을 수 있는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Keypad")
	void OnCodeAccepted();

	/** 오답 연출 자리입니다. 실패 횟수를 저장하거나 잠그지 않으므로 다시 입력할 수 있습니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Keypad")
	void OnCodeRejected(const FString& EnteredCode);
};
