// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalGameMode.h"
#include "DeadHospitalObjectiveTrigger.generated.h"

class UBoxComponent;

/**
 * Player가 특정 구역에 들어왔을 때 최신 GDD의 메인 목표 M00~M09와 서브 목표 S01~S09를 바꾸는 Trigger입니다.
 * 목표 변경을 Level Blueprint 한곳에 길게 이어 붙이지 않고 각 구역 Actor의 Details에서 설정할 수 있습니다.
 *
 * 예시:
 * - 동관 입구에 놓고 MainObjectiveId=M03, Text="동관을 조사하세요"
 * - 서관 입구에 놓고 MainObjectiveId=M04, Text="서관을 조사하세요"
 * - 악마 머리 획득 지점에는 MainObjectiveId=M05, Text="본관 2층으로 가는 방법을 찾으세요"
 * - 그림 수집을 시작하는 지점에 SubObjectiveId=S03, TargetProgress=3
 * - 생명유지장치 구역 진입 전에는 RequiredPuzzleIds에 PZ01~PZ07을 넣어 잘못된 조기 목표 변경을 차단
 *
 * 이 Actor는 목표 UI를 직접 생성하지 않습니다. GameMode 이벤트가 바뀐 목표 자료를 UI에 전달합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalObjectiveTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalObjectiveTrigger();

	/** 컷신이나 문 상호작용에서 Overlap 없이 같은 목표 변경을 실행할 때 호출할 수 있습니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective Trigger")
	bool TryApplyObjectives(AActor* Interactor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Player Pawn만 Overlap으로 감지하는 상자입니다. 크기는 L_MainLevel 배치 Actor의 Details에서 조절합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective Trigger")
	UBoxComponent* TriggerBox;

	/** true면 첫 성공 뒤 다시 진입해도 목표를 반복 설정하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger")
	bool OneUseOnly = true;

	/** 일회용 상태를 체크포인트에서 기억할 고유 이름입니다. 예: OBJ_M05_WestWing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger", meta = (EditCondition = "OneUseOnly"))
	FName TriggerEventId = NAME_None;

	/** true면 RequiredGamePhase와 현재 GameMode 단계가 정확히 같을 때만 작동합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement")
	bool RequireSpecificGamePhase = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement", meta = (EditCondition = "RequireSpecificGamePhase"))
	EDeadHospitalGamePhase RequiredGamePhase = EDeadHospitalGamePhase::Playing;

	/** 여기에 적은 퍼즐이 모두 완료된 뒤에만 목표를 바꿉니다. 비워 두면 퍼즐 조건이 없습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement")
	TArray<FName> RequiredPuzzleIds;

	/** 여기에 적은 일회성 Event가 모두 완료된 뒤에만 목표를 바꿉니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement")
	TArray<FName> RequiredCompletedEventIds;

	/**
	 * Player 인벤토리에 반드시 들어 있어야 하는 ItemID 목록입니다.
	 * 예를 들어 시작 병실 출구에는 Flashlight를 넣어 손전등을 얻지 않고 M01로 넘어가는 것을 막을 수 있습니다.
	 * 목록을 비우면 아이템을 검사하지 않습니다. 팀원의 Inventory 내부 배열은 수정하지 않고 HasItem()만 사용합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement")
	TArray<FName> RequiredItemIds;

	/**
	 * Player의 DocumentComponent에 반드시 기록되어 있어야 하는 DocumentID 목록입니다.
	 * 시작 문서나 병원장실 핵심 문서를 읽은 뒤에만 다음 목표를 시작시키고 싶을 때 사용합니다.
	 * DT_DocumentData의 Row Name이 아니라 실제 FDocumentData.DocumentID와 정확히 같은 값을 입력해야 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Requirement")
	TArray<FName> RequiredDocumentIds;

	/**
	 * 특수중환자격리실 입구 Trigger에서만 true로 사용합니다.
	 * Player를 순간이동시키지 않고 GameMode의 FinalObjective 단계와 M08 목표를 시작합니다.
	 * 이 값이 true일 때는 아래 메인/서브 목표 설정을 둘 다 false로 두고 StartFinalObjective의 기본 M08을 사용합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Final Objective")
	bool StartFinalObjectiveOnTrigger = false;

	/** true면 아래 MainObjectiveId/Text/진행도로 메인 목표를 바꿉니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Main")
	bool SetMainObjectiveOnTrigger = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Main", meta = (EditCondition = "SetMainObjectiveOnTrigger"))
	FName MainObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Main", meta = (EditCondition = "SetMainObjectiveOnTrigger"))
	FText MainObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Main", meta = (EditCondition = "SetMainObjectiveOnTrigger", ClampMin = "0"))
	int32 MainCurrentProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Main", meta = (EditCondition = "SetMainObjectiveOnTrigger", ClampMin = "0"))
	int32 MainTargetProgress = 0;

	/** true면 메인 목표와 별도로 아래 S01~S09 서브 목표를 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub")
	bool SetSubObjectiveOnTrigger = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub", meta = (EditCondition = "SetSubObjectiveOnTrigger"))
	FName SubObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub", meta = (EditCondition = "SetSubObjectiveOnTrigger"))
	FText SubObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub", meta = (EditCondition = "SetSubObjectiveOnTrigger", ClampMin = "0"))
	int32 SubCurrentProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub", meta = (EditCondition = "SetSubObjectiveOnTrigger", ClampMin = "0"))
	int32 SubTargetProgress = 0;

	/**
	 * Trigger 성공 시 완료 처리할 기존 서브 목표 ID 목록입니다.
	 * 예를 들어 병원장실 핵심 문서를 읽은 뒤 통과하는 Trigger에는 S09를 넣을 수 있습니다.
	 * 배열에 없는 다른 서브 목표(S03 등)는 그대로 유지됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective Trigger|Sub")
	TArray<FName> SubObjectiveIdsToClearOnSuccess;

	/** 두 목표 설정과 일회성 기록이 모두 성공한 뒤 Blueprint 연출을 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Objective Trigger")
	void OnObjectivesApplied();

	/** 조건 부족이나 설정 오류 때문에 작동하지 않았을 때 선택적으로 안내를 연결하는 자리입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Objective Trigger")
	void OnObjectiveApplicationRejected();

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool FromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	bool AreRequirementsMet(const ADeadHospitalGameMode* GameMode, const AActor* Interactor) const;
	void RestoreObjectiveState(
		ADeadHospitalGameMode* GameMode,
		const FDeadHospitalObjectiveState& PreviousMain,
		const TArray<FDeadHospitalObjectiveState>& PreviousSubObjectives) const;

	bool HasBeenUsed = false;
};
