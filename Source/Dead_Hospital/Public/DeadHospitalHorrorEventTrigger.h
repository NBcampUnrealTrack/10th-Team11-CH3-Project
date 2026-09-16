// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalGameMode.h"
#include "DeadHospitalHorrorEventTrigger.generated.h"

class APawn;
class UBoxComponent;

/**
 * Trigger는 Player가 보이지 않는 Box 범위에 들어올 때 작동하는 Actor입니다.
 * 문이 갑자기 닫히거나 유리창에 실루엣이 보이는 공포 연출은 같은 장소를
 * 여러 번 지나도 처음 한 번만 보여야 합니다. 이 클래스가 GameMode에
 * EventId를 예약/완료하여 중복을 막고, 체크포인트 시점에 이미 봤는지 복구합니다.
 * 실제 Ghost Mesh, 소리, 조명, Level Sequence는 Blueprint에서 연결합니다.
 * C++만 L_MainLevel에 놓으면 연출이 저절로 만들어지지는 않습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalHorrorEventTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalHorrorEventTrigger();

	/**
	 * 실제 Sequence의 Finished 출력에서 호출합니다. true는 완료가 GameMode에 기록됨,
	 * false는 시작되지 않았거나 기록 실패. 알림 뒤 Input 잠금도 풀립니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Horror Event")
	bool CompleteHorrorEvent();

	/** 연출을 못 시작했다면 Event 예약을 취소합니다. 완료 저장 없이 다시 통과해서 재시도할 수 있습니다. */
	UFUNCTION(BlueprintCallable, Category = "Horror Event")
	void CancelHorrorEvent();

	UFUNCTION(BlueprintCallable, Category = "Horror Event")
	void SetHorrorEventEnabled(bool ShouldEnable);

	UFUNCTION(BlueprintPure, Category = "Horror Event")
	bool IsHorrorEventRunning() const { return EventRunning; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Horror Event")
	UBoxComponent* TriggerBox;

	/** FName 고유 이름(예: Horror_DoorClose_01). 비어 있거나 중복이면 저장/재생 상태가 섞입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event")
	FName EventId = NAME_None;

	/** true이면 아래 RequiredGamePhase와 현재 단계가 같을 때만 실행합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event|Condition")
	bool RequireSpecificGamePhase = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event|Condition", meta = (EditCondition = "RequireSpecificGamePhase"))
	EDeadHospitalGamePhase RequiredGamePhase = EDeadHospitalGamePhase::Playing;

	/** TArray는 여러 이름의 목록입니다. 나열한 퍼즐이 전부 완료됐을 때만 시작합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event|Condition")
	TArray<FName> RequiredPuzzleIds;

	/** 이 일회성 Event들이 완료된 뒤에만 연출을 시작합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event|Condition")
	TArray<FName> RequiredCompletedEventIds;

	/**
	 * 동시에 실행되면 안 되는 다른 Event ID 목록입니다.
	 * 같은 귀신을 공유하는 여러 Trigger의 ID를 서로 등록하면 귀신 Sequence가 겹치는 것을 막을 수 있습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event|Condition")
	TArray<FName> MutuallyExclusiveEventIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event")
	bool StartsEnabled = true;

	/**
	 * true면 연출 중 Pawn 입력과 Controller 이동/시야를 잠급니다.
	 * 공격 입력이 별도 팀원 컴포넌트에서 처리된다면 그 파트와 추가 연결이 필요합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event")
	bool LockPlayerInputDuringEvent = false;

	/** Level Sequence가 없는 짧은 Sound/Light 이벤트를 시간으로 자동 종료할 때 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event")
	bool CompleteAutomatically = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Event", meta = (EditCondition = "CompleteAutomatically", ClampMin = "0.0"))
	float AutomaticCompletionDelaySeconds = 1.0f;

	/**
	 * Event 예약에 성공한 뒤 호출합니다. Blueprint에서 Ghost/Sound/Light를 켜고
	 * Sequence를 재생합니다. Sequence가 끝나면 CompleteHorrorEvent를 연결합니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Horror Event")
	void OnHorrorEventStarted(APawn* PlayerPawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "Horror Event")
	void OnHorrorEventFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "Horror Event")
	void OnHorrorEventCancelled();

	/** 체크포인트 복구 시 Ghost와 Light를 완료 전 상태로 되돌리는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Horror Event")
	void OnHorrorEventStateRestored(bool WasCompleted);

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool FromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase);

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	bool TryStartHorrorEvent(APawn* PlayerPawn);
	/** Unreal Timer는 반환값이 없는 함수를 요구하므로 bool 완료 함수를 대신 호출하는 중간 함수입니다. */
	void HandleAutomaticCompletion();
	void SetStoredPlayerInputEnabled(bool ShouldEnableInput);
	void RefreshStateFromGameMode();
	bool AreStartRequirementsMet(const ADeadHospitalGameMode* GameMode) const;

	/** 시작 가능 여부. 이미 완료된 Event는 체크포인트 복구 때도 다시 활성화되지 않습니다. */
	bool EventEnabled = true;
	bool EventRunning = false;
	bool PlayerInputWasLocked = false;
	/** 입력 복구를 위해 이벤트에 들어온 Player를 일시적으로 기억하는 약한 참조입니다. */
	TWeakObjectPtr<APawn> StoredPlayerPawn;
	FTimerHandle CompletionTimerHandle;
};
