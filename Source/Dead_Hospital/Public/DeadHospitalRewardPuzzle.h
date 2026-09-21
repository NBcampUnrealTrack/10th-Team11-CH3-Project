// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalPuzzleBase.h"
#include "ItemData.h"
#include "DeadHospitalRewardPuzzle.generated.h"

class ADeadHospitalProgressionItem;

/**
 * 퍼즐을 해결한 뒤 맵에 중요 아이템 보상을 공개하는 공통 Actor입니다.
 * 최신 GDD의 PZ05는 도자기 단서/오브젝트 퍼즐을 해결하면 CardKeyB를 얻는 구조이므로
 * PZ05 Blueprint의 부모를 이 클래스로 만들고 RewardPickup에 CardKeyB Actor를 연결할 수 있습니다.
 *
 * C++은 아직 기획에서 확정되지 않은 도자기 모양이나 정답 순서를 임의로 만들지 않습니다.
 * Blueprint가 실제 정답을 확인한 후 TryCompletePuzzle을 호출하면 이 클래스가
 * 필수 단서 보유 확인, GameMode 완료 저장, 선택적 연결 문 해제, 보상 공개를 담당합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalRewardPuzzle : public ADeadHospitalPuzzleBase
{
	GENERATED_BODY()

public:
	ADeadHospitalRewardPuzzle();

	/**
	 * Blueprint가 도자기 퍼즐의 정답을 확인한 뒤 호출합니다.
	 * return true는 PZ05 완료가 GameMode에 기록되어 CardKeyB 보상을 주울 수 있게 되었다는 뜻입니다.
	 */
	virtual bool TryCompletePuzzle(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual bool CanCompletePuzzle(AActor* Interactor) const override;

	/** 퍼즐 완료 뒤 나타날 중요 아이템 Actor입니다. PZ05에서는 CardKeyB Pickup을 연결합니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Reward")
	ADeadHospitalProgressionItem* RewardPickup = nullptr;

	/** 팀원 DT_ItemData의 Row 이름과 정확히 같아야 합니다. 최신 PZ05 기본 보상은 CardKeyB입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Reward")
	FName RewardItemId = TEXT("CardKeyB");

	/** 보상 아이템 종류입니다. CardKeyB처럼 진행용 물건은 KeyItem으로 둡니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Reward")
	EItemType RewardItemType = EItemType::KeyItem;

	/** 보상을 한 번만 지급하고 체크포인트에서 획득 여부를 복구하기 위한 고유 Event ID입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle|Reward")
	FName RewardPickupEventId = TEXT("PZ05_CardKeyBCollected");

	/**
	 * 정답 제출 전에 반드시 보유해야 하는 단서 ItemID 목록입니다.
	 * 단서는 정답을 알아내는 데 사용하는 물건이므로 퍼즐 성공 시 소비하지 않습니다.
	 * 도자기 퍼즐의 단서가 인벤토리 아이템이 아니라 월드 오브젝트뿐이라면 배열을 비워 둘 수 있습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Reward")
	TArray<FName> RequiredClueItemIds;

	/** 보상 Actor 공개와 완료 저장이 성공한 직후 성공 연출을 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Reward")
	void OnRewardPuzzleAccepted();

	/** 보상 Actor 누락, 단서 부족, 진행 단계 오류 등으로 실패했을 때 UI/효과음을 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Reward")
	void OnRewardPuzzleRejected();
};
