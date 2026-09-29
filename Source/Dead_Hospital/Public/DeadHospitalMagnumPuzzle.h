// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalMagnumPuzzle.generated.h"

class ADeadHospitalProgressionItem;

/**
 * PZ02 "매그넘 금고 — 사라의 환자 번호"를 담당하는 퍼즐 Actor입니다.
 * 진료실 문서의 사라 밀러 환자 번호와 금고 주변의 '사라' 표시를 연결하면 정답 3178을 알 수 있습니다.
 * 키패드 UI가 SubmitCode를 호출하면 C++이 정답을 검사하고, 성공할 때만 PZ02를 저장하고 금고를 엽니다.
 *
 * 금고가 열리면 MagnumPickup Actor가 보이게 되며 Player가 다시 E로 주워야 인벤토리에 들어갑니다.
 * 이렇게 나누면 인벤토리가 가득 찬 경우에도 매그넘이 사라지지 않고 금고 안에 남아 있게 됩니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalMagnumPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalMagnumPuzzle();

	/**
	 * 금고 키패드 UI의 확인 버튼에서 호출합니다.
	 * EnteredCode가 최신 GDD의 정답 "3178"과 같을 때만 퍼즐 완료를 시도합니다.
	 * 틀리면 GameMode의 퍼즐 목록과 금고 문은 아무것도 바뀌지 않으므로 다시 입력할 수 있습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Magnum")
	bool SubmitCode(AActor* Interactor, const FString& EnteredCode);

	/**
	 * 부모의 공통 퍼즐 완료 처리가 GameMode 기록과 금고 문 잠금 해제를 수행합니다.
	 * 완료 Broadcast를 받은 MagnumPickup이 공개되고, 실제 AddItem은 Player가 보상을 E로 주울 때 실행됩니다.
	 */
	virtual bool TryCompletePuzzle(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual bool CanCompletePuzzle(AActor* Interactor) const override;

	/**
	 * 맵에 따로 배치한 매그넘 아이템 Actor를 Details에서 지정합니다.
	 * 포인터(*)는 "그 Actor를 가리키는 참조"이고 nullptr는 아직 지정되지 않았다는 뜻입니다.
	 * 연결하지 않으면 퍼즐을 풀어도 보상이 없으므로 완료 자체를 막습니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	ADeadHospitalProgressionItem* MagnumPickup = nullptr;

	/**
	 * 매그넘을 인벤토리에서 구분하는 최종 ID입니다.
	 * DT_ItemData와 팀원 Inventory에서 사용하는 "Magnum"과 정확히 같아야
	 * 퍼즐 보상으로 얻은 무기를 장착/전투 코드에서도 같은 아이템으로 찾을 수 있습니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	FName MagnumItemId = TEXT("Magnum");

	/** 한 번 집은 매그넘이 다시 나타나지 않도록 저장할 Event 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	FName MagnumPickupEventId = TEXT("PZ02_MagnumCollected");

	/** 금고의 확정 비밀번호입니다. FString으로 보관하므로 앞의 0도 하나의 글자로 구분할 수 있습니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	FString CorrectCode = TEXT("3178");

	/** C++ 정답 판정과 진행 저장이 모두 성공한 뒤 UI에 성공 표시/소리를 내보낼 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Magnum")
	void OnCodeAccepted();

	/** 오답을 입력했을 때 UI에 빨간 표시/오답 소리를 내보낼 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Magnum")
	void OnCodeRejected(const FString& EnteredCode);
};
