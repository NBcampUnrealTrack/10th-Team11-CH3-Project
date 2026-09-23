// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalGameMode.h"

#include "DocumentComponent.h"
#include "InventoryComponent.h"
#include "PlayerCharacter.h"
#include "../AI/ZombieCharacter.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "CombatComponent.h"
#include "DeadHospitalSaveSubsystem.h"
#include "Engine/GameInstance.h"

ADeadHospitalGameMode::ADeadHospitalGameMode()
{
	// 생성자는 GameMode가 처음 만들어질 때 기본값을 넣는 곳입니다.
	// FText는 화면에 보여 주는 글자이며 GameMode Blueprint의 기본값에서 문구를 바꿀 수 있습니다.
	FirstObjectiveText = FText::FromString(TEXT("병실을 조사하세요"));
	FinalObjectiveText = FText::FromString(TEXT("생명유지장치를 정지하세요"));
	EscapeObjectiveText = FText::FromString(TEXT("제한시간 안에 병원에서 탈출하세요"));

	// 게임 진행에 필요한 중요 아이템 목록입니다.
	// 카드키와 그림은 일반적인 방법으로 버리지 못하도록 보호합니다.
	ProtectedKeyItemIds = {
		TEXT("CardKeyA"),
		TEXT("CardKeyB"),
		TEXT("MasterCardKey"),
		TEXT("Painting")
	};

	// 최신 GDD에서 특수중환자격리실은 7개 퍼즐을 모두 해결한 후에만 진입할 수 있습니다.
	// 이 목록을 생성자에서 채워 두면 C++ GameMode를 그대로 사용해도 필수 조건이 비어 있는 사고를 막을 수 있습니다.
	// Blueprint 자식에서 배열을 직접 바꾸면 그 Blueprint 값이 우선하므로, 배치 전에 PZ01~PZ07이 모두 들어 있는지 확인해야 합니다.
	RequiredPuzzleIdsForFinalObjective = {
		TEXT("PZ01"),
		TEXT("PZ02"),
		TEXT("PZ03"),
		TEXT("PZ04"),
		TEXT("PZ05"),
		TEXT("PZ06"),
		TEXT("PZ07")
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
	CurrentAreaId = NAME_None;
	CurrentObjective = FDeadHospitalObjectiveState();
	CurrentSubObjective = FDeadHospitalObjectiveState();
	ActiveSubObjectives.Reset();

	// 기록을 초기화한 다음 일반 탐색으로 넘어가 첫 목표와 1초 타이머를 설정합니다.
	// Broadcast는 UI 등 On... 이벤트에 연결된 외부 동작에 변경 사실을 알립니다.
	SetGamePhase(EDeadHospitalGamePhase::Playing);
	SetMainObjective(FirstObjectiveId, FirstObjectiveText);
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
	SetMainObjective(FinalObjectiveId, FinalObjectiveText);
	ClearSubObjective();
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
	ClearMainObjective();
	ClearSubObjective();

	// 아직 탈출 타이머를 시작하지 않습니다.
	// 최신 GDD에서는 순간이동하지 않고, 특수중환자격리실에서 성불 연출과 조작 복구가 모두 끝난 뒤 StartEscapePhase가 호출됩니다.
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
	SetMainObjective(EscapeObjectiveId, EscapeObjectiveText);
	ClearSubObjective();

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
	ClearMainObjective();
	ClearSubObjective();
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
	// 이 함수는 기존 Blueprint에 이미 배치된 Set Current Objective 노드를 깨지 않기 위한 호환용 입구입니다.
	// 최신 기획에서는 메인 목표와 서브 목표를 동시에 관리하므로, 예전 함수는 메인 목표를 바꾸는 새 함수로 연결합니다.
	return SetMainObjective(ObjectiveId, ObjectiveText, CurrentProgress, TargetProgress);
}

bool ADeadHospitalGameMode::SetMainObjective(
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

	// OnObjectiveChanged는 예전 UI를 위한 알림이고 OnMainObjectiveChanged는 새 UI를 위한 알림입니다.
	// 둘 다 보내면 기존 Blueprint를 깨지 않으면서 메인/서브 UI를 나누어 연동할 수 있습니다.
	OnObjectiveChanged.Broadcast(CurrentObjective);
	OnMainObjectiveChanged.Broadcast(CurrentObjective);
	return true;
}

bool ADeadHospitalGameMode::SetSubObjective(
	FName ObjectiveId,
	FText ObjectiveText,
	int32 CurrentProgress,
	int32 TargetProgress)
{
	// 서브 목표는 메인 목표를 건드리지 않고 ActiveSubObjectives 배열에 따로 저장합니다.
	// 같은 ID가 있으면 새로 중복 추가하지 않고 기존 항목을 갱신합니다.
	if (ObjectiveId.IsNone() || IsTerminalPhase() || CurrentGamePhase == EDeadHospitalGamePhase::Ending)
	{
		return false;
	}

	FDeadHospitalObjectiveState NewState;
	NewState.ObjectiveId = ObjectiveId;
	NewState.ObjectiveText = ObjectiveText;
	NewState.TargetProgress = FMath::Max(TargetProgress, 0);
	NewState.CurrentProgress = NewState.TargetProgress > 0
		? FMath::Clamp(CurrentProgress, 0, NewState.TargetProgress)
		: 0;
	NewState.IsActive = true;

	bool ExistingObjectiveUpdated = false;
	for (FDeadHospitalObjectiveState& ActiveState : ActiveSubObjectives)
	{
		if (ActiveState.ObjectiveId == ObjectiveId)
		{
			ActiveState = NewState;
			ExistingObjectiveUpdated = true;
			break;
		}
	}

	if (!ExistingObjectiveUpdated)
	{
		ActiveSubObjectives.Add(NewState);
	}

	// 기존 UI가 GetCurrentSubObjective로 하나만 읽는 경우를 위해 가장 최근 변경 항목도 따로 기억합니다.
	CurrentSubObjective = NewState;

	OnSubObjectiveChanged.Broadcast(CurrentSubObjective);
	return true;
}

bool ADeadHospitalGameMode::UpdateObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress)
{
	// 예전 Blueprint 노드는 메인 목표 진행도를 바꾸는 함수로 연결합니다.
	return UpdateMainObjectiveProgress(ObjectiveId, CurrentProgress, TargetProgress);
}

bool ADeadHospitalGameMode::UpdateMainObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress)
{
	// 지금 화면에 떠 있는 메인 목표와 ID가 같을 때만 숫자를 바꿉니다.
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
	OnMainObjectiveChanged.Broadcast(CurrentObjective);
	return true;
}

bool ADeadHospitalGameMode::UpdateSubObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress)
{
	// S03이 유지된 채 S04가 추가될 수 있으므로 "가장 최근 목표 하나"만 검사하지 않습니다.
	// 활성 배열에서 같은 ID를 찾아 그 항목만 바꾸며, 다른 서브 목표는 그대로 남깁니다.
	if (ObjectiveId.IsNone() || TargetProgress < 0)
	{
		return false;
	}

	for (FDeadHospitalObjectiveState& ActiveState : ActiveSubObjectives)
	{
		if (!ActiveState.IsActive || ActiveState.ObjectiveId != ObjectiveId)
		{
			continue;
		}

		ActiveState.TargetProgress = TargetProgress;
		ActiveState.CurrentProgress = TargetProgress > 0
			? FMath::Clamp(CurrentProgress, 0, TargetProgress)
			: 0;
		CurrentSubObjective = ActiveState;
		OnSubObjectiveChanged.Broadcast(CurrentSubObjective);
		return true;
	}

	return false;
}

bool ADeadHospitalGameMode::AdvanceSubObjectiveProgress(
	FName ObjectiveId,
	FText ObjectiveText,
	int32 ProgressToAdd,
	int32 TargetProgress)
{
	// ProgressToAdd가 0 이하거나 목표 수가 0 이하면 의미 있는 진행이 아니므로 거절합니다.
	if (ObjectiveId.IsNone() || ProgressToAdd <= 0 || TargetProgress <= 0)
	{
		return false;
	}

	// 이미 활성화된 같은 ID를 배열에서 찾아 기존 숫자에 더합니다.
	// 예를 들어 S04가 추가로 표시 중이어도 S03 그림 수집 숫자는 계속 올라갑니다.
	for (const FDeadHospitalObjectiveState& ActiveState : ActiveSubObjectives)
	{
		if (ActiveState.IsActive && ActiveState.ObjectiveId == ObjectiveId)
		{
			return UpdateSubObjectiveProgress(
				ObjectiveId,
				ActiveState.CurrentProgress + ProgressToAdd,
				TargetProgress);
		}
	}

	// 아직 없는 ID라면 0에서 시작하는 새 서브 목표를 추가합니다.
	return SetSubObjective(ObjectiveId, ObjectiveText, ProgressToAdd, TargetProgress);
}

void ADeadHospitalGameMode::ClearCurrentObjective()
{
	// 예전 Blueprint를 위한 함수이며, 현재는 메인 목표만 비웁니다.
	ClearMainObjective();
}

void ADeadHospitalGameMode::ClearMainObjective()
{
	// 이미 비어 있으면 같은 '목표 제거' 이벤트를 여러 번 보내지 않습니다.
	if (!CurrentObjective.IsActive && CurrentObjective.ObjectiveId.IsNone())
	{
		return;
	}

	// 새 기본 struct를 대입하면 ID=None, 숫자=0, IsActive=false가 됩니다.
	CurrentObjective = FDeadHospitalObjectiveState();
	OnObjectiveChanged.Broadcast(CurrentObjective);
	OnMainObjectiveChanged.Broadcast(CurrentObjective);
}

void ADeadHospitalGameMode::ClearSubObjective()
{
	// 메인 목표는 유지하고 현재 활성화된 모든 서브 목표를 비웁니다.
	// Ending/GameOver처럼 HUD에서 서브 목표 영역 전체를 숨겨야 할 때 사용합니다.
	if (ActiveSubObjectives.IsEmpty()
		&& !CurrentSubObjective.IsActive
		&& CurrentSubObjective.ObjectiveId.IsNone())
	{
		return;
	}

	ActiveSubObjectives.Reset();
	CurrentSubObjective = FDeadHospitalObjectiveState();
	OnSubObjectiveChanged.Broadcast(CurrentSubObjective);
}

bool ADeadHospitalGameMode::ClearSubObjectiveById(FName ObjectiveId)
{
	if (ObjectiveId.IsNone())
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveSubObjectives.Num(); ++Index)
	{
		if (ActiveSubObjectives[Index].ObjectiveId != ObjectiveId)
		{
			continue;
		}

		// UI가 어느 항목이 사라졌는지 알 수 있게 ID는 남기고 IsActive=false로 알립니다.
		FDeadHospitalObjectiveState RemovedState = ActiveSubObjectives[Index];
		RemovedState.IsActive = false;
		ActiveSubObjectives.RemoveAt(Index);

		// 기존 Getter를 위한 대표 값은 남은 목표 중 가장 뒤 항목으로 바꾸고, 없으면 비웁니다.
		CurrentSubObjective = ActiveSubObjectives.IsEmpty()
			? FDeadHospitalObjectiveState()
			: ActiveSubObjectives.Last();
		OnSubObjectiveChanged.Broadcast(RemovedState);

		// 기존 UI 중에는 여러 목표 목록을 직접 그리지 않고, 마지막으로 전달받은 목표 하나만
		// 화면에 표시하는 것도 있을 수 있습니다. 그런 UI가 방금 제거된 목표에서 멈추지 않도록
		// 남아 있는 대표 목표가 있다면 한 번 더 알려 줍니다. 배열형 UI는 이 두 알림을 받아
		// RemovedState의 ID만 지우고 CurrentSubObjective를 갱신하면 됩니다.
		if (CurrentSubObjective.IsActive)
		{
			OnSubObjectiveChanged.Broadcast(CurrentSubObjective);
		}
		return true;
	}

	return false;
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
	// PZ03의 CardKeyA가 실제로 인벤토리에 들어간 후에만 호출합니다.
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

bool ADeadHospitalGameMode::SetCurrentAreaId(FName AreaId)
{
	// None은 "이름이 정해지지 않았다"는 뜻이므로 현재의 정상적인 구역 값을 지우지 않습니다.
	// Teleport와 Checkpoint가 같은 함수를 사용하면 구역 이름을 한 곳에서 일관되게 관리할 수 있습니다.
	if (AreaId.IsNone())
	{
		return false;
	}

	CurrentAreaId = AreaId;
	return true;
}

bool ADeadHospitalGameMode::SaveCheckpoint(
	FName CheckpointId,
	AActor* PlayerActor,
	const FTransform& RespawnTransform,
	FName AreaId)
{
	// ReturningToHospital은 예전 enum 이름을 호환용으로 유지한 것이며, 현재는 성불 Sequence가 진행 중인 불안정한 단계입니다.
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
	// Checkpoint Actor에 AreaId가 있으면 그 값을 가장 먼저 사용합니다.
	// 비어 있다면 마지막 Teleport가 기록한 CurrentAreaId를 사용하고, 둘 다 없다면
	// 최소한 현재 체크포인트 지점을 구별할 수 있도록 CheckpointId를 대신 저장합니다.
	NewCheckpoint.SavedAreaId = !AreaId.IsNone()
		? AreaId
		: (!CurrentAreaId.IsNone() ? CurrentAreaId : CheckpointId);
	NewCheckpoint.SavedGamePhase = CurrentGamePhase;
	NewCheckpoint.SavedObjective = CurrentObjective;
	NewCheckpoint.SavedSubObjective = CurrentSubObjective;
	NewCheckpoint.SavedSubObjectives = ActiveSubObjectives;
	NewCheckpoint.SavedPlayTimeSeconds = ElapsedPlayTimeSeconds;
	NewCheckpoint.SavedKillCount = KillCount;
	NewCheckpoint.SavedEscapeTimeSeconds = EscapeRemainingTimeSeconds;
	NewCheckpoint.SavedLifeSupportShutdown = LifeSupportShutdown;
	NewCheckpoint.CompletedPuzzleIds = GetCompletedPuzzleIds();
	NewCheckpoint.CompletedEventIds = GetCompletedOneTimeEventIds();
	NewCheckpoint.ActivatedCheckpointIds = ActivatedCheckpointIds.Array();
	NewCheckpoint.ActivatedCheckpointIds.AddUnique(CheckpointId);

	// APlayerCharacter로 Cast에 성공하면 Player 팀이 공개한 Getter로 체력과 손전등 보유 상태를 읽습니다.
	// GameMode가 PlayerCharacter의 private 변수를 직접 바꾸지 않기 때문에 팀원 파트의 컡슐화를 깨지 않습니다.
	if (const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(PlayerActor))
	{
		NewCheckpoint.HasPlayerHealthSnapshot = true;
		NewCheckpoint.SavedPlayerHealth = PlayerCharacter->GetCurrentHP();
		NewCheckpoint.SavedPlayerHadFlashlight = PlayerCharacter->HasFlashlight();
	}
	else
	{
		// 다른 Pawn 클래스로 테스트하는 경우에도 인벤토리와 진행 상태 저장은 계속할 수 있게 합니다.
		// 단, 이 경우는 PlayerCharacter 전용 HP/손전등만 복구할 수 없다는 경고를 남깁니다.
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Checkpoint %s saved without PlayerCharacter health/flashlight data."),
			*CheckpointId.ToString());
	}

	// InventoryComponent는 팀원 코드를 수정하지 않고 공개된 데이터와 함수만 사용합니다.
	NewCheckpoint.HasInventorySnapshot = true;
	// GetInventorySlots()는 현재 인벤토리의 모든 칸을 읽기 전용으로 보여 주는 공개 함수입니다.
	// 빈 칸은 저장할 필요가 없으므로 건너뛰고, 실제 아이템이 들어 있는 칸의 ItemData만 복사합니다.
	// ItemData 안에는 ItemID뿐 아니라 Quantity도 있으므로 같은 아이템의 현재 수량도 함께 저장됩니다.
	for (const FInventorySlot& Slot : Inventory->GetInventorySlots())
	{
		if (!Slot.bIsEmpty)
		{
			NewCheckpoint.InventoryItems.Add(Slot.ItemData);
		}
	}
	// Coin은 InventorySlots에 들어가지 않으므로
	// 현재 보유 중인 Coin 수량을 별도로 체크포인트에 저장
	NewCheckpoint.SavedCoinQuantity = Inventory->GetCoinQuantity();

	NewCheckpoint.EquippedWeaponId = Inventory->GetEquippedWeaponID();

	// 퀵슬롯 1, 2, 3에 등록된 아이템 ID를 체크포인트에 저장합니다.
	// 내부 인덱스 0, 1, 2가 화면의 퀵슬롯 1, 2, 3에 해당합니다.
	NewCheckpoint.SavedQuickSlots.SetNum(3);

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < 3; ++QuickSlotIndex)
	{
		NewCheckpoint.SavedQuickSlots[QuickSlotIndex]
			= Inventory->GetQuickSlotItem(QuickSlotIndex);
	}

	// 플레이어의 CombatComponent를 가져옵니다.
	if (UCombatComponent* CombatComp =
		PlayerPawn->FindComponentByClass<UCombatComponent>())
	{
		// HandGun과 Magnum의 현재 탄창에 남아 있는 탄약 수를
		// 체크포인트 데이터에 각각 저장합니다.
		NewCheckpoint.SavedHandGunMagazineAmmo = CombatComp->GetWeaponCurrentAmmo(FName(TEXT("HandGun")));

		NewCheckpoint.SavedMagnumMagazineAmmo = CombatComp->GetWeaponCurrentAmmo(FName(TEXT("Magnum")));
	}

	// 문서는 Inventory Item이 아니므로 별도의 DocumentComponent에서 따로 복사합니다.
	// 팀원 컴포넌트의 GetDocuments()는 읽기 전용 공개 함수이므로 내부 배열을 직접 수정하지 않습니다.
	const UDocumentComponent* DocumentComponent = PlayerActor->FindComponentByClass<UDocumentComponent>();
	if (IsValid(DocumentComponent))
	{
		NewCheckpoint.HasDocumentSnapshot = true;
		NewCheckpoint.Documents = DocumentComponent->GetDocuments();
	}
	else
	{
		// 아직 Player Blueprint에 DocumentComponent가 연결되지 않은 개발 상태에서도
		// 위치와 인벤토리 체크포인트는 저장합니다. 문서 기능을 사용하려면 컴포넌트를 연결해야 합니다.
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Checkpoint %s saved without Documents: Player has no DocumentComponent."),
			*CheckpointId.ToString());
	}

	TArray<AActor*> ZombieActors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AZombieCharacter::StaticClass(),
		ZombieActors);

	for (AActor* Actor : ZombieActors)
	{
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(Actor);

		if (!IsValid(Zombie)
			|| Zombie->GetCurrentState() == EZombieState::Dead)
		{
			continue;
		}

		FDeadHospitalZombieTransformData ZombieData;
		ZombieData.ZombieActorName = Zombie->GetFName();
		ZombieData.Transform = Zombie->GetActorTransform();
		ZombieData.Health = Zombie->GetHealth();

		NewCheckpoint.ZombieTransforms.Add(ZombieData);
	}

	// 모든 자료를 채운 뒤 마지막 기록을 교체합니다. MoveTemp는 배열처럼
	// 내부 데이터를 복사하지 않고 새 저장 기록으로 넘기기 위한 도구입니다.
	LastCheckpoint = MoveTemp(NewCheckpoint);
	CurrentAreaId = LastCheckpoint.SavedAreaId;
	ActivatedCheckpointIds.Add(CheckpointId);
	OnCheckpointSaved.Broadcast(LastCheckpoint.CheckpointId);

	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		if (UDeadHospitalSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDeadHospitalSaveSubsystem>())
		{
			SaveSubsystem->SaveNextAutoSlot();
		}
	}

	return true;
}

bool ADeadHospitalGameMode::HasCheckpointBeenActivated(FName CheckpointId) const
{
	return !CheckpointId.IsNone() && ActivatedCheckpointIds.Contains(CheckpointId);
}

bool ADeadHospitalGameMode::GetCheckpointSnapshot(FDeadHospitalCheckpointData& OutCheckpoint) const
{
	if (!LastCheckpoint.IsValid) return false;
	OutCheckpoint = LastCheckpoint;
	return true;
}

bool ADeadHospitalGameMode::RestoreCheckpointSnapshot(const FDeadHospitalCheckpointData& InCheckpoint)
{
	if (!InCheckpoint.IsValid) return false;
	LastCheckpoint = InCheckpoint;
	CurrentAreaId = LastCheckpoint.SavedAreaId;
	return RestartFromLastCheckpoint();
}

bool ADeadHospitalGameMode::RestartFromLastCheckpoint()
{
	// 메모리에 마지막 기록이 있고 GameOver인 경우에만 재시작합니다.
	// 중간에 한 단계라도 실패하면 기존 Pawn/저장 기록을 가능하면 그대로 둡니다.
	if (!LastCheckpoint.IsValid)
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

		// 새 Pawn의 인벤토리는 BeginPlay에서 빈 슬롯들로 초기화됩니다.
		// 저장해 둔 각 ItemData를 팀원 파트의 공개 함수 AddItem()으로 다시 넣습니다.
		// 예전처럼 존재하지 않는 Items 배열을 직접 대입하지 않기 때문에 병합된 인벤토리 구조와 맞습니다.
		for (const FItemData& SavedItem : LastCheckpoint.InventoryItems)
		{
			if (!Inventory->AddItem(SavedItem))
			{
				// 하나라도 복구하지 못하면 일부 아이템만 가진 잘못된 상태로 게임을 계속하지 않습니다.
				// 아직 OldPawn을 삭제하기 전이므로 새 Pawn을 없애고 이전 Pawn을 다시 조종하게 합니다.
				PlayerController->UnPossess();
				NewPlayerPawn->Destroy();
				if (IsValid(OldPawn))
				{
					PlayerController->Possess(OldPawn);
				}
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Checkpoint restart failed: inventory item %s could not be restored."),
					*SavedItem.ItemID.ToString());
				return false;
			}
		}

		// 체크포인트에 저장된 Coin 수량 복구
		// Coin은 InventorySlots가 아닌 별도 CoinQuantity로 관리
		Inventory->SetCoinQuantity(LastCheckpoint.SavedCoinQuantity);

		// EquipWeapon()은 해당 무기가 인벤토리에 실제로 있는지도 검사하므로 성공 여부를 확인합니다.
		// 아이템을 모두 넣은 다음, 저장 당시 장착 중이던 무기가 있다면 다시 장착합니다.
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


		// ==================== 퀵슬롯 복원 ====================

		// 체크포인트에 저장된 퀵슬롯 1, 2, 3을 복원합니다.
		// 인벤토리 아이템을 먼저 복원한 뒤 SetQuickSlot()을 호출해야
		// 해당 아이템을 실제로 보유하고 있는지 정상적으로 검사할 수 있습니다.
		for (int32 QuickSlotIndex = 0;
			QuickSlotIndex < LastCheckpoint.SavedQuickSlots.Num();
			++QuickSlotIndex)
		{
			const FName SavedQuickSlotItemId =
				LastCheckpoint.SavedQuickSlots[QuickSlotIndex];

			// 저장 당시 비어 있던 퀵슬롯은 건너뜁니다.
			if (SavedQuickSlotItemId.IsNone())
			{
				continue;
			}

			if (!Inventory->SetQuickSlot(QuickSlotIndex, SavedQuickSlotItemId))
			{
				PlayerController->UnPossess();
				NewPlayerPawn->Destroy();

				if (IsValid(OldPawn))
				{
					PlayerController->Possess(OldPawn);
				}

				UE_LOG(
					LogTemp,
					Error,
					TEXT("Checkpoint restart failed: QuickSlot %d item %s could not be restored."),
					QuickSlotIndex + 1,
					*SavedQuickSlotItemId.ToString());

				return false;
			}
		}
		// 새로 생성된 플레이어의 CombatComponent를 가져옵니다.
		if (UCombatComponent* CombatComp =
			NewPlayerPawn->FindComponentByClass<UCombatComponent>())
		{
			// 체크포인트에 저장되어 있던 HandGun 탄창 수를 복원합니다.
			if (!CombatComp->SetWeaponCurrentAmmo(
				FName(TEXT("HandGun")),
				LastCheckpoint.SavedHandGunMagazineAmmo))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Checkpoint restart: Failed to restore HandGun magazine ammo.")
				);
			}

			// 체크포인트에 저장되어 있던 Magnum 탄창 수를 복원합니다.
			if (!CombatComp->SetWeaponCurrentAmmo(
				FName(TEXT("Magnum")),
				LastCheckpoint.SavedMagnumMagazineAmmo))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Checkpoint restart: Failed to restore Magnum magazine ammo.")
				);
			}
		}
	}
	if (LastCheckpoint.HasDocumentSnapshot)
	{
		UDocumentComponent* DocumentComponent = NewPlayerPawn->FindComponentByClass<UDocumentComponent>();
		if (!IsValid(DocumentComponent))
		{
			// 저장 당시에는 문서 컴포넌트가 있었는데 새 Pawn에서 사라졌다면 문서를 복구할 수 없습니다.
			// 일부 자료가 빠진 상태로 진행하지 않고 새 Pawn을 제거한 뒤 기존 Pawn으로 돌아갑니다.
			PlayerController->UnPossess();
			NewPlayerPawn->Destroy();
			if (IsValid(OldPawn))
			{
				PlayerController->Possess(OldPawn);
			}
			UE_LOG(LogTemp, Error, TEXT("Checkpoint restart failed: new Pawn has no DocumentComponent."));
			return false;
		}

		// DocumentComponent에는 별도의 공개 복원 함수가 없으므로 저장된 문서를 한 개씩 AddDocument()로 넣습니다.
		// 새 Pawn의 문서 목록은 비어 있는 것이 정상이지만, 이미 같은 ID가 있다면 중복 추가 없이 넘어갑니다.
		for (const FDocumentData& SavedDocument : LastCheckpoint.Documents)
		{
			if (DocumentComponent->HasDocument(SavedDocument.DocumentID))
			{
				continue;
			}

			if (!DocumentComponent->AddDocument(SavedDocument))
			{
				PlayerController->UnPossess();
				NewPlayerPawn->Destroy();
				if (IsValid(OldPawn))
				{
					PlayerController->Possess(OldPawn);
				}
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Checkpoint restart failed: Document %s could not be restored."),
					*SavedDocument.DocumentID.ToString());
				return false;
			}
		}
	}

	// 인벤토리와 문서를 모두 복구한 뒤 PlayerCharacter 전용 상태를 복구합니다.
	// 손전등은 AcquireFlashlight()라는 팀원의 공개 함수를 통해 복구하므로 손전등 UI/상태 알림도 기존 흐름대로 실행됩니다.
	if (APlayerCharacter* NewPlayerCharacter = Cast<APlayerCharacter>(NewPlayerPawn))
	{
		if (LastCheckpoint.SavedPlayerHadFlashlight && !NewPlayerCharacter->HasFlashlight())
		{
			NewPlayerCharacter->AcquireFlashlight();
		}

		if (LastCheckpoint.HasPlayerHealthSnapshot)
		{
			// 새 Pawn은 기본적으로 최대 HP로 시작합니다. 저장 HP가 그보다 낮으면 차이만큼만 TakeDamage를 적용합니다.
			// PlayerCharacter에 공개 SetHealth 함수가 없으므로 private CurrentHP를 건드리지 않고, 공개된 정상 피해 흐름을 이용해 UI에도 HP 변경을 알립니다.
			const float SafeSavedHealth = FMath::Clamp(
				LastCheckpoint.SavedPlayerHealth,
				0.0f,
				NewPlayerCharacter->GetMaxHP());
			const float HealthToRemove = NewPlayerCharacter->GetCurrentHP() - SafeSavedHealth;

			if (HealthToRemove > 0.0f)
			{
				FDamageEvent CheckpointRestoreDamageEvent;
				NewPlayerCharacter->TakeDamage(
					HealthToRemove,
					CheckpointRestoreDamageEvent,
					nullptr,
					nullptr);
			}
		}
	}

	// 생성/연결/아이템 복구가 모두 성공한 뒤에만 이전 Pawn을 없앱니다.
	if (IsValid(OldPawn))
	{
		OldPawn->Destroy();
	}

	// 현재 플레이 숫자/메인·서브 목표/퍼즐 목록을 저장 시점으로 되돌리고,
	// 새 Pawn 입력과 1초 타이머를 복구합니다. HP와 손전등은 바로 위에서 이미 복구되었습니다.
	RestoreInternalCheckpointState();

	TMap<FName, FTransform> SavedZombieTransforms;

	for (const FDeadHospitalZombieTransformData& ZombieData :
		LastCheckpoint.ZombieTransforms)
	{
		SavedZombieTransforms.Add(
			ZombieData.ZombieActorName,
			ZombieData.Transform);
	}

	TArray<AActor*> ZombieActors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AZombieCharacter::StaticClass(),
		ZombieActors);

	for (AActor* Actor : ZombieActors)
	{
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(Actor);

		if (!IsValid(Zombie))
		{
			continue;
		}

		if (const FTransform* SavedTransform =
			SavedZombieTransforms.Find(Zombie->GetFName()))
		{
			Zombie->RestoreCheckpointTransform(*SavedTransform);
		}
	}

	SetLocalPlayerInputEnabled(true);
	RestartGameTimer();

	// 다른 담당 파트가 새 Pawn과 새 InventoryComponent를 다시 찾을 수 있도록 먼저 알려 줍니다.
	// 이 이벤트가 발생하는 시점에는 아래의 게임 진행 상태와 인벤토리 복구가 모두 끝난 상태입니다.
	OnCheckpointPlayerRespawned.Broadcast(NewPlayerPawn);
	OnCheckpointRestored.Broadcast(LastCheckpoint.CheckpointId);
	OnObjectiveChanged.Broadcast(CurrentObjective);
	OnMainObjectiveChanged.Broadcast(CurrentObjective);
	// UI가 사망 직전의 임시 목표를 먼저 비우고, 저장된 모든 서브 목표를 다시 만들 수 있게 빈 상태를 먼저 보냅니다.
	OnSubObjectiveChanged.Broadcast(FDeadHospitalObjectiveState());
	for (const FDeadHospitalObjectiveState& RestoredSubObjective : ActiveSubObjectives)
	{
		OnSubObjectiveChanged.Broadcast(RestoredSubObjective);
	}
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
	ClearMainObjective();
	ClearSubObjective();
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
	CurrentSubObjective = LastCheckpoint.SavedSubObjective;
	ActiveSubObjectives = LastCheckpoint.SavedSubObjectives;
	// 이전 버전의 체크포인트 자료에는 배열이 없을 수 있으므로, 단일 저장값이 활성 상태라면 배열에 한 번 복구합니다.
	if (ActiveSubObjectives.IsEmpty() && CurrentSubObjective.IsActive)
	{
		ActiveSubObjectives.Add(CurrentSubObjective);
	}
	CurrentAreaId = LastCheckpoint.SavedAreaId;
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
