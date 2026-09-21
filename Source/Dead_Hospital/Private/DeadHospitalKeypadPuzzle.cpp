// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalKeypadPuzzle.h"

#include "DeadHospitalDoor.h"

ADeadHospitalKeypadPuzzle::ADeadHospitalKeypadPuzzle()
{
	// 생성자는 Actor가 처음 만들어질 때 기본값을 넣는 함수입니다.
	// PuzzleId는 GameMode가 체크포인트에 저장할 이름이고, 표시 글자와는 다릅니다.
	PuzzleId = TEXT("PZ01");
	InteractionText = FText::FromString(TEXT("E 키로 키패드 조사"));
}

bool ADeadHospitalKeypadPuzzle::SubmitCode(AActor* Interactor, const FString& EnteredCode)
{
	// 먼저 부모의 공통 검사로 상호작용 Actor가 유효하고 탐색 단계인지 확인합니다.
	// 이 부모 검사는 Player 소유 여부까지 확인하지 않으므로 호출하는 Player 쪽 연결도 중요합니다.
	// false라면 아래 정답 검사까지 가지 않으므로 게임 종료 후 입력도 무시됩니다.
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	// TrimStartAndEnd는 글자 맨 앞/맨 뒤 공백만 지웁니다. 내부의 숫자는 건드리지 않습니다.
	// 앞뒤의 실수로 들어간 공백만 제거하고 숫자의 순서와 개수는 그대로 비교합니다.
	// 따라서 01796, 17960, 1795 같은 값은 정답으로 처리되지 않습니다.
	const FString TrimmedCode = EnteredCode.TrimStartAndEnd();
	if (TrimmedCode != CorrectCode)
	{
		OnCodeRejected(TrimmedCode);
		return false;
	}

	if (!TryCompletePuzzle(Interactor))
	{
		// 정답이어도 연결 문이 없거나 진행 저장이 실패했다면 성공 화면을 띄우지 않습니다.
		return false;
	}

	// TryCompletePuzzle이 성공한 뒤에만 UI 성공 연출을 실행합니다.
	// 이렇게 해야 문 해제나 완료 저장이 실패했는데 화면만 성공으로 보이는 문제를 피할 수 있습니다.
	OnCodeAccepted();
	return true;
}

bool ADeadHospitalKeypadPuzzle::CanCompletePuzzle(AActor* Interactor) const
{
	// 정답이 1796이어도 연결된 문이 없다면 퍼즐 완료 기록을 남기지 않습니다.
	// 그렇지 않으면 체크포인트에는 성공으로 저장되지만 Player는 다음 구역으로 갈 수 없게 됩니다.
	return IsValid(ConnectedDoor) && Super::CanCompletePuzzle(Interactor);
}
