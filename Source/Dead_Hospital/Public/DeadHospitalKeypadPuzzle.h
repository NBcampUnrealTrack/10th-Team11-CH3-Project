// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalKeypadPuzzle.generated.h"

/**
 * PZ01 "지하 2층 도어락"의 비밀번호 1796을 판정하는 퍼즐 Actor입니다.
 * Player가 벽에 피로 적힌 숫자를 확인하고 키패드 UI에 입력하면 UI가 SubmitCode를 호출합니다.
 * 이 C++ Actor가 입력값 전체를 1796과 비교하고, 정답일 때만 GameMode에 PZ01 완료를 저장한 뒤 문을 엽니다.
 *
 * 숫자 버튼, 입력창, 성공·실패 색과 소리는 Blueprint UI가 담당합니다.
 * 실제 정답과 체크포인트 기록은 C++이 담당하므로 UI 모양을 바꾸어도 진행 규칙은 달라지지 않습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalKeypadPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalKeypadPuzzle();

	/**
	 * 키패드 UI의 확인 버튼에서 호출합니다.
	 * Interactor에는 E로 키패드를 사용한 Player Actor를, EnteredCode에는 버튼으로 모은 글자 "1796"을 전달합니다.
	 * FString은 여러 글자를 보관하는 Unreal 자료형이므로 정수 1796과는 다른 타입입니다.
	 * return true는 정답 판정, PZ01 저장, 연결 문의 잠금 해제가 모두 승인되었다는 뜻입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Keypad")
	bool SubmitCode(AActor* Interactor, const FString& EnteredCode);

protected:
	/** 부모의 공통 조건에 "ConnectedDoor가 실제로 연결되어 있어야 한다"는 조건을 추가합니다. */
	virtual bool CanCompletePuzzle(AActor* Interactor) const override;

	/**
	 * 최신 GDD에서 확정한 정답 문자열입니다.
	 * 전체 문자열을 정확히 비교하므로 "01796", "17960", "1795"는 모두 오답입니다.
	 * VisibleDefaultsOnly는 에디터에서 값을 확인할 수 있지만 배치 Actor마다 실수로 바꾸지는 못하게 합니다.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Keypad")
	FString CorrectCode = TEXT("1796");

	/** C++의 정답·진행 저장이 성공한 뒤 Blueprint에서 녹색 표시와 성공음을 실행하는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Keypad")
	void OnCodeAccepted();

	/** 오답일 때 빨간 표시와 실패음을 실행하는 연결 지점입니다. 횟수 제한이 없으므로 다시 입력할 수 있습니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Keypad")
	void OnCodeRejected(const FString& EnteredCode);
};
