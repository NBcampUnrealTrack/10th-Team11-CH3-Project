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
	// 생성자는 GameMode가 처음 만들어질 때 기본값을 넣는 곳입니다.
	// FText는 화면에 보여 주는 글자이며 GameMode Blueprint의 기본값에서 문구를 바꿀 수 있습니다.
	FirstObjectiveText = FText::FromString(TEXT("병실을 조사하라"));
	FinalObjectiveText = FText::FromString(TEXT("생명유지장치를 종료하라"));
	EscapeObjectiveText = FText::FromString(TEXT("제한시간 안에 병원을 탈출하라"));

	// 보호할 진행 아이템의 고유 ID 목록입니다. 인벤토리 파트는
	// IsProtectedKeyItem(ItemId)을 물어보고 폐기/소비 버튼을 막을 수 있습니다.
	// 이 목록을 만든 것만으로 팀원 인벤토리 UI가 자동으로 달라지지는 않습니다.
	ProtectedKeyItemIds = {
		TEXT("PZ02_HiddenKey"),
		TEXT("PZ02_MiddleAgedManPainting")
	};
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
	// 맵 이동/게임 종료 시 예약된 1초 타이머를 취소합니다.
	// Super::EndPlay는 부모 GameMode의 기본 종료 처리를 이어서 수행합니다.
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool ADeadHospitalGameMode::StartGame()
{
	// 게임을 두 번 시작하면 이미 모은 퍼즐/킬 기록이 지워지므로 Waiting에서만 허용합니다.
	// bool 함수의 true는 성공, false는 이번 호출이 규칙상 거절되었다는 뜻입니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::Waiting)
	{
		return false;
	}

	// 처음 시작하는 게임이므로 이전 플레이의 숫자·완료 목록·체크포인트를 비웁니다.
	// Reset은 TSet 자료형 안에 들어 있던 원소들을 모두 지우는 함수입니다.
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
	ActivatedCheckpointIds.Reset();
	CountedEnemies.Reset();
	LastCheckpoint = FDeadHospitalCheckpointData();

	// 기록을 초기화한 다음 일반 탐색으로 넘어가 첫 목표와 1초 타이머를 설정합니다.
	// Broadcast는 UI 등 On... 이벤트에 연결된 외부 동작에 변경 사실을 알립니다.
	SetGamePhase(EDeadHospitalGamePhase::Playing);
	SetCurrentObjective(FirstObjectiveId, FirstObjectiveText);
	RestartGameTimer();

	OnGameRecordsUpdated.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("Dead Hospital game started after intro."));
	return true;
}

bool ADeadHospitalGameMode::StartFinalObjective()
{
	// 단순히 Teleport Trigger에 닿았다는 이유만으로 마지막 구역을 시작하면 안 됩니다.
	// GameMode가 필수 퍼즐과 진행 Event를 다시 검사하므로, 잘못 연결된 Blueprint 호출도 차단됩니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::Playing
		|| LifeSupportShutdown
		|| !AreFinalObjectiveRequirementsMet())
	{
		return false;
	}

	SetGamePhase(EDeadHospitalGamePhase::FinalObjective);
	SetCurrentObjective(FinalObjectiveId, FinalObjectiveText);
	OnFinalObjectiveStarted.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::AreFinalObjectiveRequirementsMet() const
{
	// TArray는 값이 여러 개 들어 있는 목록입니다. for는 각 ID를 하나씩 꺼냅니다.
	// '필수'로 설정한 ID가 None(비어 있음)이거나 완료 목록에 없다면 false입니다.
	// 목록 자체가 비어 있으면 검사할 조건이 없어 true이므로 Blueprint 설정을 확인해야 합니다.
	for (const FName RequiredPuzzleId : RequiredPuzzleIdsForFinalObjective)
	{
		if (RequiredPuzzleId.IsNone() || !CompletedPuzzleIds.Contains(RequiredPuzzleId))
		{
			return false;
		}
	}

	for (const FName RequiredEventId : RequiredEventIdsForFinalObjective)
	{
		if (RequiredEventId.IsNone() || !CompletedEventIds.Contains(RequiredEventId))
		{
			return false;
		}
	}

	return true;
}

bool ADeadHospitalGameMode::CompleteLifeSupportShutdown()
{
	// 마지막 구역 안에서 아직 장치가 켜져 있을 때만 한 번 승인합니다.
	// 다른 단계에서 누르거나 다시 누르면 아무 변경 없이 false를 돌려줍니다.
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
	// 어느 Enemy인지 모르면 동일한 사망 알림인지 확인할 방법이 없습니다.
	// 잘못된 중복 카운트를 만드는 것보다 아무 값도 바꾸지 않고 연결 오류를 로그로 알리는 편이 안전합니다.
	UE_LOG(LogTemp, Warning, TEXT("RegisterEnemyKill is deprecated. Use RegisterEnemyKillOnce(DefeatedEnemy)."));
}

bool ADeadHospitalGameMode::RegisterEnemyKillOnce(AActor* DefeatedEnemy)
{
	// IsValid는 들어온 Actor가 null이거나 이미 파괴된 참조인지 검사합니다.
	// Waiting/Ending/GameOver에서는 사망 알림이 늦게 들어와도 기록하지 않습니다.
	if (!IsActiveGameplayPhase() || !IsValid(DefeatedEnemy))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> EnemyKey(DefeatedEnemy);
	// 같은 몬스터의 죽음 콜백이 두 번 실행되어도 CountedEnemies에 이미 있으면 거절합니다.
	// TWeakObjectPtr은 몬스터 생존 기간을 늘리지 않으면서 동일 Actor를 구별합니다.
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
	// 성불/복귀 연출이 아직 진행 중이라면 병원이 폭발하는 타이머를 먼저 켜지 않습니다.
	if (!LifeSupportShutdown || CurrentGamePhase != EDeadHospitalGamePhase::ReturningToHospital)
	{
		return false;
	}

	// ?:는 조건에 따라 두 값 중 하나를 고르는 연산자입니다.
	// 호출자가 양수 제한시간을 주면 그것을 쓰고, 0 이하이면 기본 설정을 씁니다.
	const int32 SelectedDuration = DurationSeconds > 0
		? DurationSeconds
		: DefaultEscapeTimeLimitSeconds;

	EscapeRemainingTimeSeconds = FMath::Max(SelectedDuration, 1);
	SetGamePhase(EDeadHospitalGamePhase::Escape);
	SetCurrentObjective(EscapeObjectiveId, EscapeObjectiveText);

	OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);
	OnGameRecordsUpdated.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::TryStartEnding()
{
	// 병원 최종 출구에 도착했더라도 장치 종료·Escape 단계·남은 시간의
	// 세 조건이 맞아야 합니다. 확정 결과를 두 번 덮어쓰는 것도 막습니다.
	if (!LifeSupportShutdown
		|| CurrentGamePhase != EDeadHospitalGamePhase::Escape
		|| EscapeRemainingTimeSeconds <= 0
		|| ClearResultConfirmed)
	{
		return false;
	}

	// 최종 출구 E 상호작용 순간의 값을 복사한 뒤 다시 변경하지 않습니다.
	// 등급은 클리어 시간 컷으로만 계산하며 KillCount는 별도 기록으로 남습니다.
	// 타이머를 끄므로 엔딩 영상이 길어져도 클리어 시간이 더 늘지 않습니다.
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
	// Ending 연출이 실제로 끝나기 전에는 결과 화면을 확정하면 안 됩니다.
	// OnGameCleared는 결과 UI에 확정 플레이 시간과 등급을 함께 전합니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::Ending || !ClearResultConfirmed)
	{
		return;
	}

	SetGamePhase(EDeadHospitalGamePhase::Cleared);
	OnGameCleared.Broadcast(FinalClearTimeSeconds, FinalRank);
}

void ADeadHospitalGameMode::HandlePlayerDeath()
{
	// 인트로(Waiting), 엔딩, 이미 끝난 게임에서 늦게 들어온 사망 알림은 무시합니다.
	// 이 검사로 GameOver 이벤트와 UI가 두 번 실행되는 것도 막을 수 있습니다.
	if (!IsActiveGameplayPhase())
	{
		return;
	}

	FinishWithGameOver(EDeadHospitalGameOverReason::PlayerDied);
}

bool ADeadHospitalGameMode::SetCurrentObjective(
	FName ObjectiveId,
	FText ObjectiveText,
	int32 CurrentProgress,
	int32 TargetProgress)
{
	// ObjectiveId는 코드가 목표를 구별하는 이름이고 ObjectiveText는 UI에 읽히는 문구입니다.
	// None 또는 종료/엔딩 단계라면 더 이상 새 목표를 화면에 띄우지 않습니다.
	if (ObjectiveId.IsNone() || IsTerminalPhase() || CurrentGamePhase == EDeadHospitalGamePhase::Ending)
	{
		return false;
	}

	CurrentObjective.ObjectiveId = ObjectiveId;
	CurrentObjective.ObjectiveText = ObjectiveText;
	// Max는 음수 목표 개수를 0으로 보정하고 Clamp는 현재 개수를 0~목표 개수에 가둡니다.
	// 목표 개수가 0이라면 '3개 중 1개' 같은 수치 목표가 아니므로 현재 숫자도 0입니다.
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
	// 지금 화면에 떠 있는 목표와 ID가 같을 때만 숫자를 바꿉니다.
	// 다른 목표 진행도가 우연히 들어와도 현재 목표를 덮어쓰지 않습니다.
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
	// 이미 비어 있으면 같은 '목표 제거' 이벤트를 여러 번 보내지 않습니다.
	if (!CurrentObjective.IsActive && CurrentObjective.ObjectiveId.IsNone())
	{
		return;
	}

	// 새 기본 struct를 대입하면 ID=None, 숫자=0, IsActive=false가 됩니다.
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

	// 완료 ID를 먼저 기록한 뒤 알립니다. 이벤트를 받는 다른 Actor가
	// IsPuzzleCompleted(PuzzleId)를 바로 검사해도 이미 true가 나옵니다.
	CompletedPuzzleIds.Add(PuzzleId);
	OnPuzzleCompleted.Broadcast(PuzzleId);
	return true;
}

bool ADeadHospitalGameMode::CompletePickupEventAndPuzzle(FName PickupEventId, FName PuzzleId)
{
	// PZ-02의 Key가 실제로 인벤토리에 들어간 후에만 호출합니다.
	// 아이템 Event 예약이 없거나 이미 퍼즐이 끝났다면 어느 상태도 변경하지 않습니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::Playing
		|| PickupEventId.IsNone()
		|| PuzzleId.IsNone()
		|| !RunningEventIds.Contains(PickupEventId)
		|| CompletedEventIds.Contains(PickupEventId)
		|| CompletedPuzzleIds.Contains(PuzzleId))
	{
		return false;
	}

	// Broadcast는 Blueprint나 다른 Actor 코드를 실행시킬 수 있습니다.
	// 그래서 알림보다 먼저 두 결과를 모두 저장해 부분 성공 상태를 만들지 않습니다.
	RunningEventIds.Remove(PickupEventId);
	CompletedEventIds.Add(PickupEventId);
	CompletedPuzzleIds.Add(PuzzleId);
	OnOneTimeEventCompleted.Broadcast(PickupEventId);
	OnPuzzleCompleted.Broadcast(PuzzleId);
	return true;
}

bool ADeadHospitalGameMode::IsPuzzleCompleted(FName PuzzleId) const
{
	return !PuzzleId.IsNone() && CompletedPuzzleIds.Contains(PuzzleId);
}

bool ADeadHospitalGameMode::TryStartOneTimeEvent(FName EventId)
{
	// 연출 시작 '예약'입니다. 아직 연출 완료 기록은 아닙니다.
	// 하나의 ID에 이미 완료 또는 실행 중 표시가 있으면 중복 재생을 거절합니다.
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
	// TryStartOneTimeEvent로 먼저 예약된 이벤트만 완료할 수 있습니다.
	// 이 검사 덕분에 GameOver 뒤 늦게 도착한 Sequence 콜백이 새 완료 기록을 만들지 못합니다.
	if (EventId.IsNone()
		|| CompletedEventIds.Contains(EventId)
		|| !RunningEventIds.Contains(EventId))
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
	// 연출 시작/아이템 획득이 실패했으면 예약만 지워 나중에 다시 시도할 수 있게 합니다.
	// 이미 '완료'한 ID는 여기서 지워지지 않습니다.
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
	// ReturningToHospital은 성불 Sequence와 순간이동이 진행 중인 불안정한 단계입니다.
	// 이 순간을 저장하면 복구 후 Sequence를 이어갈 주체가 없어질 수 있으므로 체크포인트를 만들지 않습니다.
	const bool IsStableCheckpointPhase = CurrentGamePhase == EDeadHospitalGamePhase::Playing
		|| CurrentGamePhase == EDeadHospitalGamePhase::FinalObjective
		|| CurrentGamePhase == EDeadHospitalGamePhase::Escape;

	if (!IsStableCheckpointPhase || CheckpointId.IsNone() || !IsValid(PlayerActor))
	{
		return false;
	}

	// Cast<APawn>은 Actor가 실제 Pawn 계열인지 확인하고 맞을 때만 Pawn으로 취급합니다.
	// 다른 Actor나 AI Pawn이 체크포인트에 들어와도 Player 기록으로 저장하지 않습니다.
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

	// 아이템을 저장하지 못하는 체크포인트는 나중에 Key를 잃고도 '저장 성공'으로
	// 보일 수 있습니다. 팀원의 InventoryComponent 연결 전에는 안전하게 실패합니다.
	const UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();
	if (!IsValid(Inventory))
	{
		UE_LOG(LogTemp, Warning, TEXT("Checkpoint %s cannot save: Player has no InventoryComponent."), *CheckpointId.ToString());
		return false;
	}

	FDeadHospitalCheckpointData NewCheckpoint;
	// 임시 struct에 모든 값과 복구 위치를 먼저 담습니다. 이전 기록 LastCheckpoint를
	// 곧바로 하나씩 고치지 않아 중간에 실패해도 과거 기록이 남도록 합니다.
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
	NewCheckpoint.ActivatedCheckpointIds = ActivatedCheckpointIds.Array();
	NewCheckpoint.ActivatedCheckpointIds.AddUnique(CheckpointId);

	// InventoryComponent는 팀원 코드를 수정하지 않고 공개된 데이터와 함수만 사용합니다.
	NewCheckpoint.HasInventorySnapshot = true;
	NewCheckpoint.InventoryItems = Inventory->Items;
	NewCheckpoint.EquippedWeaponId = Inventory->GetEquippedWeaponID();

	// 모든 자료를 채운 뒤 마지막 기록을 교체합니다. MoveTemp는 배열처럼
	// 내부 데이터를 복사하지 않고 새 저장 기록으로 넘기기 위한 도구입니다.
	LastCheckpoint = MoveTemp(NewCheckpoint);
	ActivatedCheckpointIds.Add(CheckpointId);
	OnCheckpointSaved.Broadcast(LastCheckpoint.CheckpointId);
	return true;
}

bool ADeadHospitalGameMode::HasCheckpointBeenActivated(FName CheckpointId) const
{
	return !CheckpointId.IsNone() && ActivatedCheckpointIds.Contains(CheckpointId);
}

bool ADeadHospitalGameMode::RestartFromLastCheckpoint()
{
	// 메모리에 마지막 기록이 있고 GameOver인 경우에만 재시작합니다.
	// 중간에 한 단계라도 실패하면 기존 Pawn/저장 기록을 가능하면 그대로 둡니다.
	if (CurrentGamePhase != EDeadHospitalGamePhase::GameOver || !LastCheckpoint.IsValid)
	{
		return false;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	// Controller는 Player가 현재 어느 Pawn을 조작하는지 관리하는 객체입니다.
	// 0은 첫 번째 로컬 플레이어를 뜻합니다.
	if (!IsValid(PlayerController))
	{
		return false;
	}

	// 사망한 Pawn은 HP, 충돌, 이동 상태가 이미 꺼져 있을 수 있으므로,
	// Player 담당 코드를 억지로 초기화하지 않고 새 Pawn을 생성해서 교체합니다.
	//
	// 여기서 중요한 점은 기존 Pawn을 먼저 삭제하면 안 된다는 것입니다.
	// DefaultPawnClass가 비어 있거나 생성 위치가 잘못되어 새 Pawn 생성에 실패할 수도 있습니다.
	// 따라서 아래 순서처럼 "새 Pawn 생성 성공 확인 -> Controller 연결 -> 기존 Pawn 삭제" 순서로 처리합니다.
	APawn* OldPawn = PlayerController->GetPawn();
	APawn* NewPlayerPawn = SpawnDefaultPawnAtTransform(PlayerController, LastCheckpoint.PlayerRespawnTransform);
	if (!IsValid(NewPlayerPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: DefaultPawnClass could not spawn."));
		return false;
	}

	// Unreal의 FinishRestartPlayer는 Controller가 현재 가리키는 Pawn을 Possess합니다.
	// 그래서 새 Pawn을 Controller에 먼저 지정한 다음, 엔진의 표준 재시작 마무리 함수를 호출합니다.
	PlayerController->UnPossess();
	PlayerController->SetPawn(NewPlayerPawn);
	FinishRestartPlayer(PlayerController, LastCheckpoint.PlayerRespawnTransform.Rotator());

	// Possess 과정에서 예상치 못한 문제가 생겨 새 Pawn 연결이 실패하면,
	// 기존 Pawn을 다시 연결할 수 있도록 기존 Pawn을 아직 삭제하지 않은 상태로 검사합니다.
	if (PlayerController->GetPawn() != NewPlayerPawn)
	{
		NewPlayerPawn->Destroy();

		if (IsValid(OldPawn))
		{
			PlayerController->Possess(OldPawn);
		}

		UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: PlayerController could not possess the new Pawn."));
		return false;
	}

	if (LastCheckpoint.HasInventorySnapshot)
	{
		UInventoryComponent* Inventory = NewPlayerPawn->FindComponentByClass<UInventoryComponent>();
		if (!IsValid(Inventory))
		{
			// 새 Player에 인벤토리가 빠져 있으면 Key 등 저장 아이템을 복구할 수 없습니다.
			// 잘못된 빈 인벤토리로 진행하기보다 이전 Pawn을 보존하고 재시작을 실패 처리합니다.
			PlayerController->UnPossess();
			NewPlayerPawn->Destroy();
			if (IsValid(OldPawn))
			{
				PlayerController->Possess(OldPawn);
			}
			UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: new Pawn has no InventoryComponent."));
			return false;
		}

		// 이미 존재하는 팀원 InventoryComponent의 공개 자료와 함수를 사용합니다.
		// 장착할 무기가 기록되어 있다면 EquipWeapon 성공까지 확인하고 넘어갑니다.
		Inventory->Items = LastCheckpoint.InventoryItems;
		if (!LastCheckpoint.EquippedWeaponId.IsNone()
			&& !Inventory->EquipWeapon(LastCheckpoint.EquippedWeaponId))
		{
			PlayerController->UnPossess();
			NewPlayerPawn->Destroy();
			if (IsValid(OldPawn))
			{
				PlayerController->Possess(OldPawn);
			}
			UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: equipped weapon could not be restored."));
			return false;
		}
	}

	// 생성/연결/아이템 복구가 모두 성공한 뒤에만 이전 Pawn을 없앱니다.
	if (IsValid(OldPawn))
	{
		OldPawn->Destroy();
	}

	// 현재 플레이 숫자/목표/퍼즐 목록을 저장 시점으로 되돌리고,
	// 새 Pawn 입력과 1초 타이머를 복구합니다. 새 Pawn의 HP 등은
	// DefaultPawnClass 기본 설정에서 시작하며 이 struct에서는 저장하지 않습니다.
	RestoreInternalCheckpointState();
	SetLocalPlayerInputEnabled(true);
	RestartGameTimer();

	// 다른 담당 파트가 새 Pawn과 새 InventoryComponent를 다시 찾을 수 있도록 먼저 알려 줍니다.
	// 이 이벤트가 발생하는 시점에는 아래의 게임 진행 상태와 인벤토리 복구가 모두 끝난 상태입니다.
	OnCheckpointPlayerRespawned.Broadcast(NewPlayerPawn);
	OnCheckpointRestored.Broadcast(LastCheckpoint.CheckpointId);
	OnObjectiveChanged.Broadcast(CurrentObjective);
	OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);
	OnGameRecordsUpdated.Broadcast();
	return true;
}

bool ADeadHospitalGameMode::RequestReturnToMainMenu()
{
	// 우선 UI에 '메뉴 요청' 소식을 보냅니다. 메뉴 레벨 이름이 설정되었다면
	// OpenLevel로 레벨을 이동하며, 이름이 비었다면 UI가 별도 처리해야 합니다.
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
	// GameOver 이유가 서로 다르므로 UI는 이 함수를 호출해 알맞은 제목을 읽습니다.
	// 이유가 아직 None이라면 빈 텍스트를 반환합니다.
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
	// 실패 원인에 따라 화면에 보일 간단한 힌트를 고릅니다.
	// 실제 체력이나 남은 시간의 변경은 여기서 수행하지 않습니다.
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
	// 옵션의 '게임 기록' 화면에서 현재 시간을 최종 시간이라고 가정한 임시 등급입니다.
	// 실제 클리어 등급이 아니고 진행 중 시간이 늘면 달라질 수 있습니다.
	if (CurrentGamePhase == EDeadHospitalGamePhase::Waiting)
	{
		return EDeadHospitalRank::Unranked;
	}

	return CalculateRankFromClearTime(ElapsedPlayTimeSeconds);
}

int32 ADeadHospitalGameMode::GetMaximumTimeForRank(EDeadHospitalRank Rank) const
{
	// CalculateRankFromClearTime과 같은 보정 규칙을 사용해야 UI에 보이는 컷과 실제 판정이 일치합니다.
	// 예: S=1800을 1800초 이내라는 숫자로 반환합니다. D에는 상한 컷이 없어 -1입니다.
	const int32 SafeSMaximum = FMath::Max(SRankMaximumTimeSeconds, 1);
	const int32 SafeAMaximum = FMath::Max(ARankMaximumTimeSeconds, SafeSMaximum);
	const int32 SafeBMaximum = FMath::Max(BRankMaximumTimeSeconds, SafeAMaximum);
	const int32 SafeCMaximum = FMath::Max(CRankMaximumTimeSeconds, SafeBMaximum);

	switch (Rank)
	{
	case EDeadHospitalRank::S:
		return SafeSMaximum;
	case EDeadHospitalRank::A:
		return SafeAMaximum;
	case EDeadHospitalRank::B:
		return SafeBMaximum;
	case EDeadHospitalRank::C:
		return SafeCMaximum;
	case EDeadHospitalRank::D:
	case EDeadHospitalRank::Unranked:
	default:
		return -1;
	}
}

TArray<FName> ADeadHospitalGameMode::GetCompletedPuzzleIds() const
{
	// 내부 TSet을 Blueprint에서 다루기 쉬운 TArray 목록으로 복사해 돌려줍니다.
	// 순서는 완료 순서가 아니므로 UI에서 순서가 필요하면 별도 정렬해야 합니다.
	return CompletedPuzzleIds.Array();
}

TArray<FName> ADeadHospitalGameMode::GetCompletedOneTimeEventIds() const
{
	// 현재 완료된 연출/일회성 획득 ID만 담습니다. Running 예약은 제외됩니다.
	return CompletedEventIds.Array();
}

void ADeadHospitalGameMode::UpdateGameTimers()
{
	// RestartGameTimer가 1초 간격으로 예약한 함수입니다. Pause 중에는 월드 타이머가
	// 진행되지 않습니다. 플레이 단계 밖이면 기록도 늘리지 않습니다.
	if (!IsActiveGameplayPhase())
	{
		return;
	}

	// ++는 현재 숫자에 1을 더합니다. 일반 진행 중에는 총 플레이 시간만 늘어납니다.
	++ElapsedPlayTimeSeconds;

	if (CurrentGamePhase == EDeadHospitalGamePhase::Escape)
	{
		// 탈출 단계에서만 남은 시간을 1초 줄이고 UI에 새 숫자를 전달합니다.
		// Max(..., 0)으로 음수 남은 시간이 화면에 나오지 않게 합니다.
		EscapeRemainingTimeSeconds = FMath::Max(EscapeRemainingTimeSeconds - 1, 0);
		OnEscapeTimeChanged.Broadcast(EscapeRemainingTimeSeconds);

		if (EscapeRemainingTimeSeconds <= 0)
		{
			// 0이 된 순간 실패를 확정합니다. 그 아래 기록 갱신까지 진행하지 않도록 return합니다.
			FinishWithGameOver(EDeadHospitalGameOverReason::EscapeTimeExpired);
			return;
		}
	}

	OnGameRecordsUpdated.Broadcast();
}

EDeadHospitalRank ADeadHospitalGameMode::CalculateRankFromClearTime(int32 ClearTimeSeconds) const
{
	// 에디터에서 시간 값을 잘못 입력해도 S <= A <= B <= C 순서가 무너지지 않게 보정합니다.
	// 예를 들어 A 컷을 S 컷보다 작게 입력했다면, 최소한 S 컷과 같은 값으로 취급합니다.
	// FMath::Max(a, b)는 두 숫자 중 큰 쪽을 고릅니다. '컷 이하'일 때 해당 등급입니다.
	// 모든 컷보다 오래 걸리면 D이며, 몬스터 킬 수는 이 함수에서 계산하지 않습니다.
	const int32 SafeSMaximum = FMath::Max(SRankMaximumTimeSeconds, 1);
	const int32 SafeAMaximum = FMath::Max(ARankMaximumTimeSeconds, SafeSMaximum);
	const int32 SafeBMaximum = FMath::Max(BRankMaximumTimeSeconds, SafeAMaximum);
	const int32 SafeCMaximum = FMath::Max(CRankMaximumTimeSeconds, SafeBMaximum);
	const int32 SafeClearTime = FMath::Max(ClearTimeSeconds, 0);

	if (SafeClearTime <= SafeSMaximum)
	{
		return EDeadHospitalRank::S;
	}

	if (SafeClearTime <= SafeAMaximum)
	{
		return EDeadHospitalRank::A;
	}

	if (SafeClearTime <= SafeBMaximum)
	{
		return EDeadHospitalRank::B;
	}

	if (SafeClearTime <= SafeCMaximum)
	{
		return EDeadHospitalRank::C;
	}

	return EDeadHospitalRank::D;
}

void ADeadHospitalGameMode::SetGamePhase(EDeadHospitalGamePhase NewGamePhase)
{
	// 단계가 이미 같다면 불필요한 이벤트를 다시 실행시키지 않습니다.
	// 실제로 변경한 뒤 알리므로 UI에서 Getter를 읽어도 새 단계가 나옵니다.
	if (CurrentGamePhase == NewGamePhase)
	{
		return;
	}

	CurrentGamePhase = NewGamePhase;
	OnGamePhaseChanged.Broadcast(CurrentGamePhase);
}

void ADeadHospitalGameMode::FinishWithGameOver(EDeadHospitalGameOverReason NewGameOverReason)
{
	// 이미 결과가 끝났거나 엔딩을 시작한 뒤라면 늦게 온 사망/시간초과를 무시합니다.
	// 이 검사 때문에 OnGameOver가 중복으로 방송되지 않습니다.
	if (IsTerminalPhase() || CurrentGamePhase == EDeadHospitalGamePhase::Ending)
	{
		return;
	}

	// 현재 실행 중인 타이머와 완료 전 연출 예약은 취소하지만,
	// 마지막 체크포인트는 남겨 두어 GameOver UI에서 재시작할 수 있습니다.
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
	// ||는 어느 조건 하나만 참이어도 true라는 뜻입니다.
	// ReturningToHospital도 진행 단계이므로 총 플레이 시간은 계속 흐르지만
	// Escape 남은 시간은 Escape 단계에서만 줄어듭니다.
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
	// 이 함수는 첫 번째 로컬 Player의 Controller/Pawn 입력을 건드립니다.
	// Player 담당 파트의 개별 공격/상호작용 상태를 직접 초기화하는 함수는 아닙니다.
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (ShouldEnableInput)
	{
		// SetIgnoreMoveInput/LookInput은 여러 시스템이 잠그면 횟수가 누적되는 방식입니다.
		// 사망 직전 Teleport와 공포 연출이 동시에 입력을 막았다면 false를 한 번 호출하는 것만으로는
		// 잠금이 전부 풀리지 않습니다. 체크포인트의 새 Pawn은 모든 이전 잠금을 버려야 하므로 Reset을 사용합니다.
		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
	}
	else
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}

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
	// 앞선 1초 타이머 예약을 먼저 지워 재시작을 여러 번 눌러도
	// 시간이 한 번에 2초씩 올라가는 중복 타이머를 만들지 않습니다.
	GetWorldTimerManager().ClearTimer(GameTimerHandle);

	if (!IsActiveGameplayPhase())
	{
		return;
	}

	// World Timer는 게임 Pause 중 자동으로 멈춥니다. 옵션 화면을 실제로
	// Pause 처리한 경우에만 그동안 플레이 시간 기록이 제외됩니다.
	// 1.0f는 1초 간격, 마지막 true는 반복 실행을 뜻합니다.
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
	// 저장 struct의 값을 실제 GameMode 기록으로 되돌립니다.
	// 재시작 직전의 사망 원인/확정 등급은 버리고 저장 당시 목표·시간을 다시 사용합니다.
	ElapsedPlayTimeSeconds = LastCheckpoint.SavedPlayTimeSeconds;
	KillCount = LastCheckpoint.SavedKillCount;
	EscapeRemainingTimeSeconds = LastCheckpoint.SavedEscapeTimeSeconds;
	LifeSupportShutdown = LastCheckpoint.SavedLifeSupportShutdown;
	CurrentObjective = LastCheckpoint.SavedObjective;
	GameOverReason = EDeadHospitalGameOverReason::None;
	ClearResultConfirmed = false;
	FinalClearTimeSeconds = 0;
	FinalRank = EDeadHospitalRank::Unranked;

	// 체크포인트 이후 해결한 퍼즐과 연출은 없어지고 저장 시점 목록만 남습니다.
	// TArray에서 각 이름을 읽어 TSet에 Add하므로 ID가 중복되어도 하나로 정리됩니다.
	CompletedPuzzleIds.Reset();
	for (const FName PuzzleId : LastCheckpoint.CompletedPuzzleIds)
	{
		CompletedPuzzleIds.Add(PuzzleId);
	}

	CompletedEventIds.Reset();
	for (const FName EventId : LastCheckpoint.CompletedEventIds)
	{
		CompletedEventIds.Add(EventId);
	}
	// 끝나지 않은 연출 '예약'과 게임오버 직전 몬스터 중복 기록은 복구하지 않습니다.
	// 체크포인트 활성화 목록은 저장 시점 목록으로 되돌립니다.
	RunningEventIds.Reset();
	ActivatedCheckpointIds.Reset();
	for (const FName CheckpointId : LastCheckpoint.ActivatedCheckpointIds)
	{
		ActivatedCheckpointIds.Add(CheckpointId);
	}
	CountedEnemies.Reset();

	SetGamePhase(LastCheckpoint.SavedGamePhase);
}
