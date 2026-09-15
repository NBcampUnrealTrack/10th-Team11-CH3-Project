// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "DeadHospitalMagnumPuzzle.generated.h"

class ADeadHospitalProgressionItem;

/**
 * PZ-03 "병동 1층 매그넘 획득 퍼즐"의 변하지 않는 핵심 연결만 담당합니다.
 *
 * 현재 기획에서 아직 정해지지 않은 것:
 * - 어떤 단서를 보고 정답을 알아내는지
 * - 버튼, 다이얼, 그림 맞추기 중 어떤 조작을 사용하는지
 *
 * 이미 정해진 것:
 * - 퍼즐 성공 전에는 매그넘을 얻을 수 없음
 * - 성공하면 연결된 보관함 문이 열림
 * - 매그넘은 Inventory에 정확히 한 번만 들어감
 *
 * 그래서 여기서 임의로 정답(숫자나 버튼 순서)을 발명하지 않았습니다.
 * 나중에 정답 방식이 정해지면 Blueprint 자식이 올바른 입력을 확인한 뒤
 * 부모의 TryCompletePuzzle을 호출합니다. 공통 저장/문/보상 기능은 그대로 재사용합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalMagnumPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalMagnumPuzzle();

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
	 * 매그넘을 인벤토리에서 구분할 임시 ID입니다. 다른 팀원의 무기 ID가 확정되면
	 * 그 ID와 반드시 일치시켜야 실제 매그넘 장착/전투와 연결됩니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	FName MagnumItemId = TEXT("Weapon_Magnum");

	/** 한 번 집은 매그넘이 다시 나타나지 않도록 저장할 Event 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Magnum")
	FName MagnumPickupEventId = TEXT("PZ03_MagnumCollected");
};
