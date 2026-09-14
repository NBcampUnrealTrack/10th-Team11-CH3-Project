// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalGameMode.h"

#include "InventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ADeadHospitalGameMode::ADeadHospitalGameMode()
{
	// FText는 나중에 Blueprint 자식에서 다른 문구로 바꿀 수 있습니다.
	FirstObjectiveText = FText::FromString(TEXT("병실을 조사하라"));
	FinalObjectiveText = FText::FromString(TEXT("생명유지장치를 종료하라"));
	EscapeObjectiveText = FText::FromString(TEXT("제한시간 안에 병원을 탈출하라"));

	// 이 목록에 등록된 KeyItem은 인벤토리 UI에서 폐기/소비 버튼을 막는 기준으로 사용합니다.
	ProtectedKeyItemIds = { TEXT("PZ02_HiddenKey") };
}

void ADeadHospitalGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 실제 게임은 인트로와 Player 조작 복구가 끝난 순간 StartGame을 호출합니다.
	// StartAutomatically는 인트로가 아직 없는 개발 초기 테스트에서만 사용합니다.
	if (StartAutomatically)
	{
		StartGame();
	}
}

void ADeadHospitalGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool ADeadHospitalGameMode::StartGame()
{
	if (CurrentGamePhase != EDeadHospitalGamePhase::Waiting)
	{
		return false;
	}

	KillCount = 0;
	ElapsedPlayTimeSeconds = 0;
	EscapeRemainingTimeSeconds = 0;
	LifeSupportShutdown = false;
	ClearResultConfirmed = false;
	GameOverReason = EDeadHospitalGameOverReason::None;
	FinalClearTimeSeconds = 0;
	FinalRank = EDeadHospitalRank::Unranked;
	CompletedPuzzleIds.Reset();
	CompletedEventIds.Reset();
	RunningEventIds.Reset();
	CountedEnemies.Reset();
	LastCheckpoint = FDeadHospitalCheckpointData();

	SetGamePhase(EDeadHospitalGamePhase::Playing);
	SetCurrentObjective(FirstObjectiveId, FirstObjectiveText);
	RestartGameTimer();

	OnGameRecordsUpdated.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("Dead Hospital game started after intro."));
	return true;
}

bool ADeadHospitalGameMode::StartFinalObjective()
{
	if (CurrentGamePhase != EDeadHospitalGamePhase::Playing || LifeSupportShutdown)
	{
		return false;
	}

	SetGamePhase(EDeadHospitalGamePhase::FinalObjective);
	SetCurrentObjective(FinalObjectiveId, FinalObjectiveText);
	OnFinalObjectiveStarted.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::CompleteLifeSupportShutdown()
{
	if (CurrentGamePhase != EDeadHospitalGamePhase::FinalObjective || LifeSupportShutdown)
	{
		return false;
	}

	LifeSupportShutdown = true;
	SetGamePhase(EDeadHospitalGamePhase::ReturningToHospital);
	ClearCurrentObjective();

	// 아직 탈출 타이머를 시작하지 않습니다.
	// 성불 연출, 암전, 지하 2층 이동, 조작 복구가 모두 끝난 뒤 StartEscapePhase가 호출됩니다.
	OnLifeSupportShutdown.Broadcast();
	OnGameRecordsUpdated.Broadcast();
	return true;
}

void ADeadHospitalGameMode::RegisterEnemyKill()
{
	// Enemy Actor 정보가 없는 예전 연결을 위한 임시 함수입니다.
	// 새 연결에서는 중복 검사가 가능한 RegisterEnemyKillOnce를 사용합니다.
	if (!IsActiveGameplayPhase())
	{
		return;
	}

	++KillCount;
	OnGameRecordsUpdated.Broadcast();
}

bool ADeadHospitalGameMode::RegisterEnemyKillOnce(AActor* DefeatedEnemy)
{
	if (!IsActiveGameplayPhase() || !IsValid(DefeatedEnemy))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> EnemyKey(DefeatedEnemy);
	if (CountedEnemies.Contains(EnemyKey))
	{
		return false;
	}

	CountedEnemies.Add(EnemyKey);
	++KillCount;
	OnGameRecordsUpdated.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::StartEscapePhase(int32 DurationSeconds)
{
	if (!LifeSupportShutdown || CurrentGamePhase != EDeadHospitalGamePhase::ReturningToHospital)
	{
		return false;
	}

	const int32 SelectedDuration = DurationSeconds > 0
		? DurationSeconds
		: DefaultEscapeTimeLimitSeconds;

	EscapeRemainingTimeSeconds = FMath::Max(SelectedDuration, 1);
	SetGamePhase(EDeadHospitalGamePhase::Escape);
	SetCurrentObjective(EscapeObjectiveId, EscapeObjectiveText, 0, EscapeRemainingTimeSeconds);

	OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);
	OnGameRecordsUpdated.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::TryStartEnding()
{
	if (!LifeSupportShutdown
		|| CurrentGamePhase != EDeadHospitalGamePhase::Escape
		|| EscapeRemainingTimeSeconds <= 0
		|| ClearResultConfirmed)
	{
		return false;
	}

	// 최종 출구 E 상호작용 순간의 값을 복사한 뒤 다시 변경하지 않습니다.
	FinalClearTimeSeconds = ElapsedPlayTimeSeconds;
	FinalRank = CalculateRankFromClearTime(FinalClearTimeSeconds);
	ClearResultConfirmed = true;
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	ClearCurrentObjective();
	SetGamePhase(EDeadHospitalGamePhase::Ending);
	SetLocalPlayerInputEnabled(false);

	OnGameRecordsUpdated.Broadcast();
	OnEndingStarted.Broadcast();
	return true;
}

void ADeadHospitalGameMode::CompleteGameClear()
{
	if (CurrentGamePhase != EDeadHospitalGamePhase::Ending || !ClearResultConfirmed)
	{
		return;
	}

	SetGamePhase(EDeadHospitalGamePhase::Cleared);
	OnGameCleared.Broadcast(FinalClearTimeSeconds, FinalRank);
}

void ADeadHospitalGameMode::HandlePlayerDeath()
{
	FinishWithGameOver(EDeadHospitalGameOverReason::PlayerDied);
}

bool ADeadHospitalGameMode::SetCurrentObjective(
	FName ObjectiveId,
	FText ObjectiveText,
	int32 CurrentProgress,
	int32 TargetProgress)
{
	if (ObjectiveId.IsNone() || IsTerminalPhase() || CurrentGamePhase == EDeadHospitalGamePhase::Ending)
	{
		return false;
	}

	CurrentObjective.ObjectiveId = ObjectiveId;
	CurrentObjective.ObjectiveText = ObjectiveText;
	CurrentObjective.TargetProgress = FMath::Max(TargetProgress, 0);
	CurrentObjective.CurrentProgress = CurrentObjective.TargetProgress > 0
		? FMath::Clamp(CurrentProgress, 0, CurrentObjective.TargetProgress)
		: 0;
	CurrentObjective.IsActive = true;

	OnObjectiveChanged.Broadcast(CurrentObjective);
	return true;
}

bool ADeadHospitalGameMode::UpdateObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress)
{
	if (!CurrentObjective.IsActive || CurrentObjective.ObjectiveId != ObjectiveId || TargetProgress < 0)
	{
		return false;
	}

	CurrentObjective.TargetProgress = TargetProgress;
	CurrentObjective.CurrentProgress = TargetProgress > 0
		? FMath::Clamp(CurrentProgress, 0, TargetProgress)
		: 0;

	OnObjectiveChanged.Broadcast(CurrentObjective);
	return true;
}

void ADeadHospitalGameMode::ClearCurrentObjective()
{
	if (!CurrentObjective.IsActive && CurrentObjective.ObjectiveId.IsNone())
	{
		return;
	}

	CurrentObjective = FDeadHospitalObjectiveState();
	OnObjectiveChanged.Broadcast(CurrentObjective);
}

bool ADeadHospitalGameMode::CompletePuzzle(FName PuzzleId)
{
	// 일반 퍼즐은 탐색 상태에서만 완료할 수 있습니다.
	// Escape 또는 GameOver 상태에서 늦게 도착한 UI 입력이 진행을 바꾸지 못하게 막습니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::Playing || PuzzleId.IsNone())
	{
		return false;
	}

	if (CompletedPuzzleIds.Contains(PuzzleId))
	{
		return false;
	}

	CompletedPuzzleIds.Add(PuzzleId);
	OnPuzzleCompleted.Broadcast(PuzzleId);
	return true;
}

bool ADeadHospitalGameMode::IsPuzzleCompleted(FName PuzzleId) const
{
	return !PuzzleId.IsNone() && CompletedPuzzleIds.Contains(PuzzleId);
}

bool ADeadHospitalGameMode::TryStartOneTimeEvent(FName EventId)
{
	if (!IsActiveGameplayPhase() || EventId.IsNone())
	{
		return false;
	}

	if (CompletedEventIds.Contains(EventId) || RunningEventIds.Contains(EventId))
	{
		return false;
	}

	RunningEventIds.Add(EventId);
	return true;
}

bool ADeadHospitalGameMode::CompleteOneTimeEvent(FName EventId)
{
	if (EventId.IsNone() || CompletedEventIds.Contains(EventId))
	{
		return false;
	}

	RunningEventIds.Remove(EventId);
	CompletedEventIds.Add(EventId);
	OnOneTimeEventCompleted.Broadcast(EventId);
	return true;
}

void ADeadHospitalGameMode::CancelOneTimeEvent(FName EventId)
{
	RunningEventIds.Remove(EventId);
}

bool ADeadHospitalGameMode::IsOneTimeEventCompleted(FName EventId) const
{
	return !EventId.IsNone() && CompletedEventIds.Contains(EventId);
}

bool ADeadHospitalGameMode::IsOneTimeEventRunning(FName EventId) const
{
	return !EventId.IsNone() && RunningEventIds.Contains(EventId);
}

bool ADeadHospitalGameMode::IsProtectedKeyItem(FName ItemId) const
{
	return !ItemId.IsNone() && ProtectedKeyItemIds.Contains(ItemId);
}

bool ADeadHospitalGameMode::SaveCheckpoint(
	FName CheckpointId,
	AActor* PlayerActor,
	const FTransform& RespawnTransform)
{
	if (!IsActiveGameplayPhase() || CheckpointId.IsNone() || !IsValid(PlayerActor))
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(PlayerActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return false;
	}

	// 같은 체크포인트 Trigger를 다시 밟았을 때 저장 이벤트가 계속 반복되는 것을 막습니다.
	if (LastCheckpoint.IsValid && LastCheckpoint.CheckpointId == CheckpointId)
	{
		return false;
	}

	FDeadHospitalCheckpointData NewCheckpoint;
	NewCheckpoint.IsValid = true;
	NewCheckpoint.CheckpointId = CheckpointId;
	NewCheckpoint.PlayerRespawnTransform = RespawnTransform;
	NewCheckpoint.SavedGamePhase = CurrentGamePhase;
	NewCheckpoint.SavedObjective = CurrentObjective;
	NewCheckpoint.SavedPlayTimeSeconds = ElapsedPlayTimeSeconds;
	NewCheckpoint.SavedKillCount = KillCount;
	NewCheckpoint.SavedEscapeTimeSeconds = EscapeRemainingTimeSeconds;
	NewCheckpoint.SavedLifeSupportShutdown = LifeSupportShutdown;
	NewCheckpoint.CompletedPuzzleIds = GetCompletedPuzzleIds();
	NewCheckpoint.CompletedEventIds = GetCompletedOneTimeEventIds();

	// InventoryComponent는 팀원 코드를 수정하지 않고 공개된 데이터와 함수만 사용합니다.
	if (const UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>())
	{
		NewCheckpoint.HasInventorySnapshot = true;
		NewCheckpoint.InventoryItems = Inventory->Items;
		NewCheckpoint.EquippedWeaponId = Inventory->GetEquippedWeaponID();
	}

	LastCheckpoint = MoveTemp(NewCheckpoint);
	OnCheckpointSaved.Broadcast(LastCheckpoint.CheckpointId);
	return true;
}

bool ADeadHospitalGameMode::RestartFromLastCheckpoint()
{
	if (CurrentGamePhase != EDeadHospitalGamePhase::GameOver || !LastCheckpoint.IsValid)
	{
		return false;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		return false;
	}

	// 사망한 Pawn은 HP, 충돌, 이동 상태가 이미 꺼져 있습니다.
	// Player 코드를 억지로 되돌리지 않고 GameMode의 표준 Respawn 방식으로 새 Pawn을 만듭니다.
	APawn* OldPawn = PlayerController->GetPawn();
	PlayerController->UnPossess();

	if (IsValid(OldPawn))
	{
		OldPawn->Destroy();
	}

	RestartPlayerAtTransform(PlayerController, LastCheckpoint.PlayerRespawnTransform);
	APawn* NewPlayerPawn = PlayerController->GetPawn();
	if (!IsValid(NewPlayerPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: DefaultPawnClass could not spawn."));
		return false;
	}

	if (LastCheckpoint.HasInventorySnapshot)
	{
		if (UInventoryComponent* Inventory = NewPlayerPawn->FindComponentByClass<UInventoryComponent>())
		{
			Inventory->Items = LastCheckpoint.InventoryItems;

			if (!LastCheckpoint.EquippedWeaponId.IsNone())
			{
				Inventory->EquipWeapon(LastCheckpoint.EquippedWeaponId);
			}
		}
	}

	RestoreInternalCheckpointState();
	SetLocalPlayerInputEnabled(true);
	RestartGameTimer();

	OnCheckpointRestored.Broadcast(LastCheckpoint.CheckpointId);
	OnObjectiveChanged.Broadcast(CurrentObjective);
	OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);
	OnGameRecordsUpdated.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::RequestReturnToMainMenu()
{
	OnMainMenuRequested.Broadcast();

	if (MainMenuLevelName.IsNone())
	{
		return false;
	}

	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
	return true;
}

FText ADeadHospitalGameMode::GetGameOverTitle() const
{
	if (GameOverReason == EDeadHospitalGameOverReason::EscapeTimeExpired)
	{
		return FText::FromString(TEXT("탈출에 실패했습니다."));
	}

	if (GameOverReason == EDeadHospitalGameOverReason::PlayerDied)
	{
		return FText::FromString(TEXT("사망했습니다."));
	}

	return FText::GetEmpty();
}

FText ADeadHospitalGameMode::GetGameOverTip() const
{
	if (GameOverReason == EDeadHospitalGameOverReason::EscapeTimeExpired)
	{
		return FText::FromString(TEXT("TIP: 제한시간 안에 병원을 탈출하세요."));
	}

	if (GameOverReason == EDeadHospitalGameOverReason::PlayerDied)
	{
		return FText::FromString(TEXT("TIP: 싸우는 것만이 정답은 아닙니다."));
	}

	return FText::GetEmpty();
}

EDeadHospitalRank ADeadHospitalGameMode::GetProjectedRank() const
{
	if (CurrentGamePhase == EDeadHospitalGamePhase::Waiting)
	{
		return EDeadHospitalRank::Unranked;
	}

	return CalculateRankFromClearTime(ElapsedPlayTimeSeconds);
}

int32 ADeadHospitalGameMode::GetMaximumTimeForRank(EDeadHospitalRank Rank) const
{
	switch (Rank)
	{
	case EDeadHospitalRank::S:
		return SRankMaximumTimeSeconds;
	case EDeadHospitalRank::A:
		return ARankMaximumTimeSeconds;
	case EDeadHospitalRank::B:
		return BRankMaximumTimeSeconds;
	case EDeadHospitalRank::C:
		return CRankMaximumTimeSeconds;
	case EDeadHospitalRank::D:
	case EDeadHospitalRank::Unranked:
	default:
		return -1;
	}
}

TArray<FName> ADeadHospitalGameMode::GetCompletedPuzzleIds() const
{
	return CompletedPuzzleIds.Array();
}

TArray<FName> ADeadHospitalGameMode::GetCompletedOneTimeEventIds() const
{
	return CompletedEventIds.Array();
}

void ADeadHospitalGameMode::UpdateGameTimers()
{
	if (!IsActiveGameplayPhase())
	{
		return;
	}

	++ElapsedPlayTimeSeconds;

	if (CurrentGamePhase == EDeadHospitalGamePhase::Escape)
	{
		EscapeRemainingTimeSeconds = FMath::Max(EscapeRemainingTimeSeconds - 1, 0);
		UpdateObjectiveProgress(EscapeObjectiveId, 0, EscapeRemainingTimeSeconds);
		OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);

		if (EscapeRemainingTimeSeconds <= 0)
		{
			FinishWithGameOver(EDeadHospitalGameOverReason::EscapeTimeExpired);
			return;
		}
	}

	OnGameRecordsUpdated.Broadcast();
}

EDeadHospitalRank ADeadHospitalGameMode::CalculateRankFromClearTime(int32 ClearTimeSeconds) const
{
	if (ClearTimeSeconds <= SRankMaximumTimeSeconds)
	{
		return EDeadHospitalRank::S;
	}

	if (ClearTimeSeconds <= ARankMaximumTimeSeconds)
	{
		return EDeadHospitalRank::A;
	}

	if (ClearTimeSeconds <= BRankMaximumTimeSeconds)
	{
		return EDeadHospitalRank::B;
	}

	if (ClearTimeSeconds <= CRankMaximumTimeSeconds)
	{
		return EDeadHospitalRank::C;
	}

	return EDeadHospitalRank::D;
}

void ADeadHospitalGameMode::SetGamePhase(EDeadHospitalGamePhase NewGamePhase)
{
	if (CurrentGamePhase == NewGamePhase)
	{
		return;
	}

	CurrentGamePhase = NewGamePhase;
	OnGamePhaseChanged.Broadcast(CurrentGamePhase);
}

void ADeadHospitalGameMode::FinishWithGameOver(EDeadHospitalGameOverReason NewGameOverReason)
{
	if (IsTerminalPhase() || CurrentGamePhase == EDeadHospitalGamePhase::Ending)
	{
		return;
	}

	GameOverReason = NewGameOverReason;
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	RunningEventIds.Reset();
	ClearCurrentObjective();
	SetGamePhase(EDeadHospitalGamePhase::GameOver);
	SetLocalPlayerInputEnabled(false);

	OnGameRecordsUpdated.Broadcast();
	OnGameOver.Broadcast(GameOverReason);
}

bool ADeadHospitalGameMode::IsActiveGameplayPhase() const
{
	return CurrentGamePhase == EDeadHospitalGamePhase::Playing
		|| CurrentGamePhase == EDeadHospitalGamePhase::FinalObjective
		|| CurrentGamePhase == EDeadHospitalGamePhase::ReturningToHospital
		|| CurrentGamePhase == EDeadHospitalGamePhase::Escape;
}

bool ADeadHospitalGameMode::IsTerminalPhase() const
{
	return CurrentGamePhase == EDeadHospitalGamePhase::GameOver
		|| CurrentGamePhase == EDeadHospitalGamePhase::Cleared;
}

void ADeadHospitalGameMode::SetLocalPlayerInputEnabled(bool ShouldEnableInput)
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(!ShouldEnableInput);
	PlayerController->SetIgnoreLookInput(!ShouldEnableInput);

	if (APawn* PlayerPawn = PlayerController->GetPawn())
	{
		if (ShouldEnableInput)
		{
			PlayerPawn->EnableInput(PlayerController);
		}
		else
		{
			PlayerPawn->DisableInput(PlayerController);
		}
	}
}

void ADeadHospitalGameMode::RestartGameTimer()
{
	GetWorldTimerManager().ClearTimer(GameTimerHandle);

	if (!IsActiveGameplayPhase())
	{
		return;
	}

	// World Timer는 게임 Pause 중 자동으로 멈추므로 옵션/전투 통계 화면 시간은 제외됩니다.
	GetWorldTimerManager().SetTimer(
		GameTimerHandle,
		this,
		&ADeadHospitalGameMode::UpdateGameTimers,
		1.0f,
		true
	);
}

void ADeadHospitalGameMode::RestoreInternalCheckpointState()
{
	ElapsedPlayTimeSeconds = LastCheckpoint.SavedPlayTimeSeconds;
	KillCount = LastCheckpoint.SavedKillCount;
	EscapeRemainingTimeSeconds = LastCheckpoint.SavedEscapeTimeSeconds;
	LifeSupportShutdown = LastCheckpoint.SavedLifeSupportShutdown;
	CurrentObjective = LastCheckpoint.SavedObjective;
	GameOverReason = EDeadHospitalGameOverReason::None;
	ClearResultConfirmed = false;
	FinalClearTimeSeconds = 0;
	FinalRank = EDeadHospitalRank::Unranked;

	CompletedPuzzleIds.Reset();
	CompletedPuzzleIds.Append(LastCheckpoint.CompletedPuzzleIds);
	CompletedEventIds.Reset();
	CompletedEventIds.Append(LastCheckpoint.CompletedEventIds);
	RunningEventIds.Reset();
	CountedEnemies.Reset();

	SetGamePhase(LastCheckpoint.SavedGamePhase);
}
