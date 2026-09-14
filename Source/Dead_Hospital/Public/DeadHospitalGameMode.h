// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ItemData.h"
#include "DeadHospitalGameMode.generated.h"

class AActor;
class APlayerController;

/**
 * 게임이 현재 어느 단계에 있는지 나타냅니다.
 * 단계 값을 하나만 사용하면 서로 같이 실행되면 안 되는 진행 로직을 안전하게 막을 수 있습니다.
 */
UENUM(BlueprintType)
enum class EDeadHospitalGamePhase : uint8
{
	Waiting,				// 메인 레벨은 열렸지만 인트로가 끝나지 않아 게임 시간이 흐르지 않는 상태
	Playing,				// 병원을 탐색하고 전투와 퍼즐을 진행하는 일반 플레이 상태
	FinalObjective,		// 생명유지장치 구역에 도착하여 마지막 목표를 수행하는 상태
	ReturningToHospital,	// 장치를 끈 뒤 성불 연출과 지하 2층 복귀를 기다리는 상태
	Escape,				// 병원 붕괴 제한시간 안에 최종 출구로 이동하는 상태
	Ending,				// 최종 출구 상호작용 후 엔딩 연출을 재생하는 상태
	GameOver,			// 플레이어 사망 또는 탈출 시간 초과로 실패한 상태
	Cleared				// 엔딩이 끝나고 결과 화면을 표시할 수 있는 상태
};

/** GameOver UI가 실패 원인에 맞는 문구와 도움말을 고를 때 사용하는 값입니다. */
UENUM(BlueprintType)
enum class EDeadHospitalGameOverReason : uint8
{
	None,
	PlayerDied,
	EscapeTimeExpired
};

/** 최종 클리어 시간만으로 계산하는 S/A/B/C/D 등급입니다. */
UENUM(BlueprintType)
enum class EDeadHospitalRank : uint8
{
	Unranked,	// 아직 최종 출구에 도달하지 않아 확정 등급이 없는 상태
	D,
	C,
	B,
	A,
	S
};

/**
 * UI가 표시할 현재 목표 한 개를 묶어서 보관합니다.
 * UI는 이 값을 읽고 표시만 하며, 목표 변경 판단은 GameMode가 담당합니다.
 */
USTRUCT(BlueprintType)
struct FDeadHospitalObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FName ObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FText ObjectiveText;

	/** 진행도가 필요 없는 목표라면 CurrentProgress와 TargetProgress를 모두 0으로 둡니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	int32 CurrentProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	int32 TargetProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool IsActive = false;
};

/**
 * 마지막 체크포인트에서 복구해야 하는 게임 진행 데이터입니다.
 * L_MainLevel 안에서 새 Player Pawn을 체크포인트에 생성한 뒤 이 값으로 진행 상태를 되돌립니다.
 */
USTRUCT(BlueprintType)
struct FDeadHospitalCheckpointData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool IsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FTransform PlayerRespawnTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	EDeadHospitalGamePhase SavedGamePhase = EDeadHospitalGamePhase::Playing;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FDeadHospitalObjectiveState SavedObjective;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	int32 SavedPlayTimeSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	int32 SavedKillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	int32 SavedEscapeTimeSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool SavedLifeSupportShutdown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FName> CompletedPuzzleIds;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FName> CompletedEventIds;

	/** Player에 InventoryComponent가 있을 때만 true이며, 아래 아이템 목록을 복구합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool HasInventorySnapshot = false;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FItemData> InventoryItems;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName EquippedWeaponId = NAME_None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamePhaseChangedSignature, EDeadHospitalGamePhase, NewGamePhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameRecordsUpdatedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveChangedSignature, FDeadHospitalObjectiveState, ObjectiveState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleCompletedSignature, FName, PuzzleId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOneTimeEventCompletedSignature, FName, EventId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFinalObjectiveStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLifeSupportShutdownSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEscapeTimeChangedSignature, int32, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEndingStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameOverSignature, EDeadHospitalGameOverReason, GameOverReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameClearedSignature, int32, FinalClearTimeSeconds, EDeadHospitalRank, FinalRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointSavedSignature, FName, CheckpointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointRestoredSignature, FName, CheckpointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuRequestedSignature);

/**
 * Dead Hospital의 전체 진행을 관리하는 게임 매니저 역할의 GameMode입니다.
 *
 * Player, AI, UI, Inventory 담당 파일은 수정하지 않습니다. 다른 파트는 아래의
 * BlueprintCallable 함수와 BlueprintAssignable 이벤트를 통해 결과만 주고받습니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADeadHospitalGameMode();

	/** 인트로가 끝나고 Player 조작이 가능해진 정확한 순간 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartGame();

	/** 생명유지장치 구역으로 순간이동을 마친 뒤 마지막 목표를 시작합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartFinalObjective();

	/** 생명유지장치 종료를 한 번만 승인하고 성불·복귀 단계로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool CompleteLifeSupportShutdown();

	/** Enemy Actor 정보가 아직 없는 예전 연결을 위한 임시 함수입니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Records")
	void RegisterEnemyKill();

	/** 같은 Enemy Actor는 여러 번 전달되어도 KillCount를 정확히 한 번만 증가시킵니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Records")
	bool RegisterEnemyKillOnce(AActor* DefeatedEnemy);

	/** 지하 2층 도착과 조작 복구가 끝난 뒤에만 탈출 제한시간을 시작합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartEscapePhase(int32 DurationSeconds = -1);

	/** Escape 상태에서 최종 출구를 사용했을 때 클리어 시간과 등급을 한 번만 확정합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool TryStartEnding();

	/** 엔딩 Level Sequence가 끝난 뒤 결과 화면 단계로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void CompleteGameClear();

	/** Player 담당 파트가 HP 0 사망을 확정한 지점에서 호출할 연결 함수입니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void HandlePlayerDeath();

	/* ------------------------------ Objective ------------------------------ */

	/** GameMode 또는 퍼즐 Actor가 현재 목표를 교체할 때 사용합니다. UI는 이 함수를 호출하지 않습니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	bool SetCurrentObjective(FName ObjectiveId, FText ObjectiveText, int32 CurrentProgress = 0, int32 TargetProgress = 0);

	/** 같은 Objective의 숫자 진행도만 갱신합니다. 다른 ObjectiveId가 들어오면 변경하지 않습니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	bool UpdateObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress);

	/** GameOver, Ending, Clear처럼 목표를 더 표시하면 안 되는 순간 현재 목표를 비웁니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void ClearCurrentObjective();

	/* ------------------------------- Puzzle -------------------------------- */

	/** 퍼즐 ID를 완료 목록에 한 번만 기록하고 UI와 연결 Actor에 알립니다. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	bool CompletePuzzle(FName PuzzleId);

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsPuzzleCompleted(FName PuzzleId) const;

	/** 공포 연출을 시작할 자리를 예약하여 같은 EventId가 동시에 두 번 시작되는 것을 막습니다. */
	UFUNCTION(BlueprintCallable, Category = "One Time Event")
	bool TryStartOneTimeEvent(FName EventId);

	/** 연출이 정상 종료된 뒤 완료 목록에 저장합니다. 체크포인트에는 완료 목록만 저장됩니다. */
	UFUNCTION(BlueprintCallable, Category = "One Time Event")
	bool CompleteOneTimeEvent(FName EventId);

	/** 연출 시작에 실패했을 때 예약을 취소하여 다음 진입에서 다시 시도할 수 있게 합니다. */
	UFUNCTION(BlueprintCallable, Category = "One Time Event")
	void CancelOneTimeEvent(FName EventId);

	UFUNCTION(BlueprintPure, Category = "One Time Event")
	bool IsOneTimeEventCompleted(FName EventId) const;

	UFUNCTION(BlueprintPure, Category = "One Time Event")
	bool IsOneTimeEventRunning(FName EventId) const;

	/** 필수 진행 Key를 UI가 폐기 대상으로 표시하지 않도록 확인할 때 사용합니다. */
	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsProtectedKeyItem(FName ItemId) const;

	/* ----------------------------- Checkpoint ------------------------------ */

	/** 체크포인트 Actor가 Player 위치와 현재 진행 상태를 저장할 때 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool SaveCheckpoint(FName CheckpointId, AActor* PlayerActor, const FTransform& RespawnTransform);

	/** GameOver 이후 새 Player Pawn을 마지막 체크포인트에 생성하고 저장 상태를 복구합니다. */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool RestartFromLastCheckpoint();

	/** MainMenuLevelName이 설정되면 해당 레벨로 이동하고, 비어 있으면 UI 연결 이벤트만 보냅니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool RequestReturnToMainMenu();

	/* ------------------------------- Getters ------------------------------- */

	UFUNCTION(BlueprintPure, Category = "Game Records")
	EDeadHospitalGamePhase GetCurrentGamePhase() const { return CurrentGamePhase; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	EDeadHospitalGameOverReason GetGameOverReason() const { return GameOverReason; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	FText GetGameOverTitle() const;

	UFUNCTION(BlueprintPure, Category = "Game Records")
	FText GetGameOverTip() const;

	UFUNCTION(BlueprintPure, Category = "Game Records")
	int32 GetKillCount() const { return KillCount; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	int32 GetElapsedPlayTimeSeconds() const { return ElapsedPlayTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	int32 GetEscapeRemainingTimeSeconds() const { return EscapeRemainingTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Objective")
	FDeadHospitalObjectiveState GetCurrentObjective() const { return CurrentObjective; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	EDeadHospitalRank GetProjectedRank() const;

	UFUNCTION(BlueprintPure, Category = "Game Records")
	int32 GetMaximumTimeForRank(EDeadHospitalRank Rank) const;

	UFUNCTION(BlueprintPure, Category = "Game Records")
	int32 GetFinalClearTimeSeconds() const { return FinalClearTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Game Records")
	EDeadHospitalRank GetFinalRank() const { return FinalRank; }

	UFUNCTION(BlueprintPure, Category = "Game Flow")
	bool HasLifeSupportBeenShutdown() const { return LifeSupportShutdown; }

	UFUNCTION(BlueprintPure, Category = "Game Flow")
	bool HasClearResultBeenConfirmed() const { return ClearResultConfirmed; }

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	bool HasValidCheckpoint() const { return LastCheckpoint.IsValid; }

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FName GetActiveCheckpointId() const { return LastCheckpoint.CheckpointId; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	TArray<FName> GetCompletedPuzzleIds() const;

	UFUNCTION(BlueprintPure, Category = "One Time Event")
	TArray<FName> GetCompletedOneTimeEventIds() const;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnGamePhaseChangedSignature OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnGameRecordsUpdatedSignature OnGameRecordsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnObjectiveChangedSignature OnObjectiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnPuzzleCompletedSignature OnPuzzleCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnOneTimeEventCompletedSignature OnOneTimeEventCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnFinalObjectiveStartedSignature OnFinalObjectiveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnLifeSupportShutdownSignature OnLifeSupportShutdown;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnEscapeTimeChangedSignature OnEscapeTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnEndingStartedSignature OnEndingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnGameOverSignature OnGameOver;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnGameClearedSignature OnGameCleared;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnCheckpointSavedSignature OnCheckpointSaved;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnCheckpointRestoredSignature OnCheckpointRestored;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnMainMenuRequestedSignature OnMainMenuRequested;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 인트로가 없는 초기 테스트에서만 true로 사용하고, 실제 게임에서는 false를 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	bool StartAutomatically = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FName FirstObjectiveId = TEXT("ExplorePatientRoom");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FText FirstObjectiveText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FName FinalObjectiveId = TEXT("ShutdownLifeSupport");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FText FinalObjectiveText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FName EscapeObjectiveId = TEXT("EscapeHospital");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FText EscapeObjectiveText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Escape", meta = (ClampMin = "1"))
	int32 DefaultEscapeTimeLimitSeconds = 300;

	/** 아래 값은 회색 박스 완주 후 조정할 임시 등급 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 SRankMaximumTimeSeconds = 1800;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 ARankMaximumTimeSeconds = 2400;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 BRankMaximumTimeSeconds = 3000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 CRankMaximumTimeSeconds = 3600;

	/** UI/Inventory 파트가 폐기와 소비를 막아야 하는 필수 KeyItem ID 목록입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle")
	TArray<FName> ProtectedKeyItemIds;

	/** 예: L_MainMenu. 비어 있으면 OnMainMenuRequested 이벤트만 발생합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	FName MainMenuLevelName = NAME_None;

private:
	void UpdateGameTimers();
	EDeadHospitalRank CalculateRankFromClearTime(int32 ClearTimeSeconds) const;
	void SetGamePhase(EDeadHospitalGamePhase NewGamePhase);
	void FinishWithGameOver(EDeadHospitalGameOverReason NewGameOverReason);
	bool IsActiveGameplayPhase() const;
	bool IsTerminalPhase() const;
	void SetLocalPlayerInputEnabled(bool ShouldEnableInput);
	void RestartGameTimer();
	void RestoreInternalCheckpointState();

	FTimerHandle GameTimerHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	EDeadHospitalGamePhase CurrentGamePhase = EDeadHospitalGamePhase::Waiting;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	EDeadHospitalGameOverReason GameOverReason = EDeadHospitalGameOverReason::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objective", meta = (AllowPrivateAccess = "true"))
	FDeadHospitalObjectiveState CurrentObjective;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	int32 KillCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	int32 ElapsedPlayTimeSeconds = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	int32 EscapeRemainingTimeSeconds = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	bool LifeSupportShutdown = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	bool ClearResultConfirmed = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	int32 FinalClearTimeSeconds = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	EDeadHospitalRank FinalRank = EDeadHospitalRank::Unranked;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Checkpoint", meta = (AllowPrivateAccess = "true"))
	FDeadHospitalCheckpointData LastCheckpoint;

	/** TSet은 같은 값을 두 번 Add해도 한 개만 남으므로 중복 실행 방지에 적합합니다. */
	TSet<FName> CompletedPuzzleIds;
	TSet<FName> CompletedEventIds;
	TSet<FName> RunningEventIds;
	TSet<TWeakObjectPtr<AActor>> CountedEnemies;
};
