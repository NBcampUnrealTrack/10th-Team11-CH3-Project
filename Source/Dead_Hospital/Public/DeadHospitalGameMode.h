// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DocumentData.h"
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

/**
 * GameOver가 된 이유를 구분합니다. enum class는 미리 정한 선택지 중 하나만 담는 자료형입니다.
 * 예를 들어 HP가 0이 된 경우 PlayerDied, 탈출 타이머가 0이 된 경우 EscapeTimeExpired입니다.
 * UI는 GetGameOverReason()으로 이 값을 읽어 원인별 문구를 표시할 수 있습니다.
 */
UENUM(BlueprintType)
enum class EDeadHospitalGameOverReason : uint8
{
	None,
	PlayerDied,
	EscapeTimeExpired
};

/**
 * 플레이 결과 등급입니다. 현재 구현은 클리어 시간만으로 S/A/B/C/D를 계산합니다.
 * KillCount는 게임 기록에 별도로 표시되지만 등급 계산에 합산되지 않습니다.
 * 아직 엔딩을 확정하지 않았을 때 최종 결과 값은 Unranked입니다.
 */
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
 * 여러 값을 한데 묶는 struct입니다. 화면에 표시할 '현재 목표 하나'의 ID, 문구,
 * 진행도, 활성 여부를 같이 담습니다. 예: 문서 2/3개 조사라면 2와 3을 저장합니다.
 * UI는 이 묶음을 읽어 화면에 보여 주고, 실제 목표 변경은 GameMode에서 처리합니다.
 */
USTRUCT(BlueprintType)
struct FDeadHospitalObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FName ObjectiveId = NAME_None;

	/** FText는 화면에 표시하는 글자입니다. FName ID와 달리 사용자에게 읽히는 문장입니다. */
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
 * 마지막 체크포인트에서 되돌릴 값들을 묶은 '게임 안의 임시 저장 기록'입니다.
 * 디스크에 저장하는 세이브 파일이 아니므로 게임을 종료했다가 다시 켜면 유지되지 않습니다.
 * L_MainLevel 안에서 새 Player Pawn을 체크포인트 위치에 만든 다음 이 값으로 진행 상태를 복구합니다.
 * 여기에는 HP 같은 Player 고유 값은 없고, 아래에 적힌 진행/시간/아이템 정보만 들어 있습니다.
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

	/**
	 * 체크포인트를 저장한 진행 구역의 이름입니다.
	 * 예: Hospital_B2, Ward_1F, FinalObjectiveArea.
	 * 실제 부활 위치는 위 Transform이 담당하고, 이 ID는 UI와 진행 로직이 현재 구역을 구분할 때 사용합니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName SavedAreaId = NAME_None;

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

	/** 저장 시점까지 실제로 밟은 체크포인트입니다. 재시작 뒤 한 번만 활성화되는 상태를 복구합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FName> ActivatedCheckpointIds;

	/**
	 * Player의 InventoryComponent에서 아이템을 읽어 왔는지 표시합니다.
	 * 체크포인트 저장 함수는 컴포넌트가 없으면 성공 처리하지 않으므로,
	 * 정상적으로 저장된 기록이라면 아이템 목록도 함께 보존됩니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool HasInventorySnapshot = false;

	/**
	 * 저장 시점에 비어 있지 않았던 인벤토리 슬롯들의 아이템 자료입니다.
	 * FItemData 안의 Quantity까지 복사되므로 아이템 종류와 개수를 함께 기억합니다.
	 * 재시작할 때는 팀원 InventoryComponent의 AddItem()을 이용해 새 Pawn에게 다시 넣습니다.
	 * 슬롯 번호 자체는 저장하지 않으므로 재시작 뒤 칸 배치는 앞쪽부터 다시 정리될 수 있습니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FItemData> InventoryItems;

	// 체크포인트 저장 시 플레이어가 보유한 Coin 수량
	// Coin은 InventorySlots에 들어가지 않으므로 별도로 저장
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	int32 SavedCoinQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName EquippedWeaponId = NAME_None;

	/**
	 * 저장할 Player에게 DocumentComponent가 실제로 있었는지 표시합니다.
	 * 문서는 인벤토리 아이템이 아니므로 HasInventorySnapshot과 별도로 관리합니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool HasDocumentSnapshot = false;

	/**
	 * 체크포인트 시점까지 획득한 문서의 전체 자료입니다.
	 * DocumentID뿐 아니라 제목과 본문도 함께 보관하므로 Respawn 후 새 Pawn의
	 * DocumentComponent에 AddDocument()로 원래 목록을 복원할 수 있습니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	TArray<FDocumentData> Documents;

	// 체크포인트 저장 시 플레이어의 퀵슬롯 아이템 ID를 저장합니다.
	// Index 0, 1, 2는 각각 퀵슬롯 1, 2, 3에 해당합니다.
	UPROPERTY()
	TArray<FName> SavedQuickSlots;

	// 체크포인트 저장 시 HandGun의 현재 탄창에 남아 있는 탄약 수를 저장합니다.
	UPROPERTY()
	int32 SavedHandGunMagazineAmmo = 0;

	// 체크포인트 저장 시 Magnum의 현재 탄창에 남아 있는 탄약 수를 저장합니다.
	UPROPERTY()
	int32 SavedMagnumMagazineAmmo = 0;
};

/**
 * 아래 DECLARE_DYNAMIC_MULTICAST_DELEGATE들은 '나중에 알려 줄 소식의 형식'을 선언합니다.
 * 아직 여기서 UI를 호출하는 것은 아닙니다. cpp에서 Broadcast()를 실행하면
 * Blueprint/다른 파트가 해당 이벤트에 연결한 동작들이 실행됩니다.
 * OneParam은 소식과 함께 값 하나, TwoParams는 값 둘을 전달한다는 뜻입니다.
 */
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointPlayerRespawnedSignature, AActor*, RespawnedPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuRequestedSignature);

/**
 * Dead Hospital의 진행 상태를 모아서 관리하는 GameMode입니다.
 * GameMode는 맵에 배치하는 일반 Actor와 달리 플레이 중인 월드의 게임 규칙을 담당합니다.
 * A로 시작하는 이름은 Unreal의 Actor 계열 클래스라는 뜻이며, GameMode도 이를 상속합니다.
 *
 * Player/AI는 죽음·킬처럼 확정된 사실을 함수를 통해 전달하고, UI는 Get... 함수로
 * 값을 읽거나 On... 이벤트를 구독합니다. BlueprintCallable은 Blueprint에서
 * 함수를 호출할 수 있다는 뜻, BlueprintAssignable은 이벤트 연결이 가능하다는 뜻입니다.
 * 이 파일에서 팀원 구현을 직접 대신하지 않으며, 연결이 없는 부분은 팀 통합 때 이어야 합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADeadHospitalGameMode();

	/**
	 * 인트로가 끝나고 Player 조작이 가능해진 순간 호출합니다.
	 * Waiting일 때만 성공하며, 이전 진행 기록을 초기화하고 Playing으로 넘어가
	 * 1초마다 플레이 시간을 기록하는 타이머를 시작합니다. 성공 true, 아니면 false입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartGame();

	/** 생명유지장치 구역으로 순간이동을 마친 뒤 마지막 목표를 시작합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartFinalObjective();

	/**
	 * 최종 구역에 들어가기 위한 필수 퍼즐과 진행 Event가 전부 완료됐는지 확인합니다.
	 * Teleport Actor와 UI는 이 결과를 읽을 수 있지만, 완료 목록 자체를 직접 바꾸지는 않습니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Game Flow")
	bool AreFinalObjectiveRequirementsMet() const;

	/** 생명유지장치 종료를 한 번만 승인하고 성불·복귀 단계로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool CompleteLifeSupportShutdown();

	/**
	 * Enemy Actor 정보가 없는 예전 연결을 위한 함수입니다.
	 * Actor별 중복 검사를 할 수 없으므로 더 이상 KillCount를 올리지 않으며 새 연결에서는 사용하지 않습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Records", meta = (DeprecatedFunction, DeprecationMessage = "RegisterEnemyKillOnce에 사망한 Enemy Actor를 전달하세요."))
	void RegisterEnemyKill();

	/**
	 * 몬스터의 '사망이 확정된 순간'에 그 몬스터 Actor를 전달받습니다.
	 * 같은 Actor가 여러 번 알려 와도 TSet에 기록된 몬스터는 다시 세지 않습니다.
	 * 팀원의 AI 사망 함수와 연결하기 전에는 자동으로 킬 수가 올라가지 않습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Records")
	bool RegisterEnemyKillOnce(AActor* DefeatedEnemy);

	/**
	 * 장치 종료 후 보스 구역에서 병원 지하 2층으로 돌아오고 조작이 복구된 뒤 호출합니다.
	 * DurationSeconds가 0 이하이면 에디터의 기본 제한시간을 사용합니다.
	 * ReturningToHospital에서 Escape로 바꾸며 그때부터 남은 시간을 줄입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool StartEscapePhase(int32 DurationSeconds = -1);

	/**
	 * '병원 맵의 최종 탈출문' 사용 시 호출합니다. 보스 구역 문에서 호출하는 함수가 아닙니다.
	 * 탈출 시간 안에 도착했다면 현재 총 플레이 시간과 등급을 확정하고 Ending으로 갑니다.
	 * 실제 엔딩 영상 재생은 OnEndingStarted에 연결한 연출 담당 Blueprint가 수행합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool TryStartEnding();

	/**
	 * 엔딩 연출이 끝났다는 소식을 연출 담당 쪽에서 받은 후 호출합니다.
	 * 이미 확정한 플레이 시간/등급을 변경하지 않고 Cleared로 넘어가 결과 이벤트를 보냅니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void CompleteGameClear();

	/**
	 * 플레이어 HP가 0 이하라는 '사망 확정'을 Player 담당 파트에서 전달받습니다.
	 * GameMode는 피해 계산이나 HP 감소를 직접 수행하지 않습니다.
	 * 이 함수가 호출되어야 GameOverReason이 PlayerDied로 기록됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void HandlePlayerDeath();

	/* ------------------------------ Objective ------------------------------ */

	/**
	 * 현재 목표의 ID(구분용 이름), Text(표시 문구), 진행도 숫자를 저장합니다.
	 * 진행도는 0부터 TargetProgress 사이로 보정하며, 완료 상태에서는 새 목표를 받지 않습니다.
	 * UI는 이 함수를 호출해 규칙을 바꾸기보다 OnObjectiveChanged의 값을 표시합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	bool SetCurrentObjective(FName ObjectiveId, FText ObjectiveText, int32 CurrentProgress = 0, int32 TargetProgress = 0);

	/** 같은 목표 ID의 숫자만 갱신합니다. 예: 1/3에서 2/3. ID가 다르면 false를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	bool UpdateObjectiveProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress);

	/** GameOver, Ending, Clear처럼 목표를 더 표시하면 안 되는 순간 현재 목표를 비웁니다. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void ClearCurrentObjective();

	/* ------------------------------- Puzzle -------------------------------- */

	/**
	 * 퍼즐 Actor가 성공 판정을 내린 뒤 고유 PuzzleId를 저장합니다.
	 * Playing이 아니거나 같은 ID가 이미 기록되어 있으면 false를 돌려줍니다.
	 * 성공하면 OnPuzzleCompleted로 UI/연결 Actor에 완료 사실을 전달합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	bool CompletePuzzle(FName PuzzleId);

	/**
	 * 아이템을 집는 순간 퍼즐까지 끝나는 경우(PZ-02)에만 사용합니다.
	 * 퍼즐 완료와 아이템 획득 Event를 먼저 함께 저장하고, 그 다음에 각각 알립니다.
	 * 따로 완료 함수를 두 번 호출하면 첫 번째 알림 중에 게임 상태가 바뀌어
	 * 한쪽만 저장되는 문제가 생길 수 있으므로 이 함수로 둘을 묶습니다.
	 */
	bool CompletePickupEventAndPuzzle(FName PickupEventId, FName PuzzleId);

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

	/**
	 * 체크포인트 Actor가 Player와 부활 위치를 넘겨 현재 게임 기록을 저장합니다.
	 * Player가 아니거나 InventoryComponent가 없으면 아이템 복구가 불가능해서 저장을 거부합니다.
	 * 이전 세이브를 덮어쓰는 메모리 기록이며, 디스크 저장과는 다릅니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool SaveCheckpoint(
		FName CheckpointId,
		AActor* PlayerActor,
		const FTransform& RespawnTransform,
		FName AreaId);

	/**
	 * GameOver 화면의 재시작 버튼에서 호출합니다. 새 Pawn을 먼저 안전하게 만들고
	 * Controller 조작과 저장된 Inventory/진행 기록을 복구한 뒤 이전 Pawn을 제거합니다.
	 * 체크포인트가 없거나 새 Pawn 생성·장비 복구가 실패하면 false입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool RestartFromLastCheckpoint();

	/** MainMenuLevelName이 설정되면 해당 레벨로 이동하고, 비어 있으면 UI 연결 이벤트만 보냅니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool RequestReturnToMainMenu();

	/* ------------------------------- Getters ------------------------------- */
	// Getter는 현재 값을 '읽기만' 하는 함수입니다. BlueprintPure는 실행 핀 없이
	// 값을 가져오는 노드라는 뜻입니다. 여기를 호출해도 게임 진행 기록은 바뀌지 않습니다.

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

	/** 현재 Player가 진행 중인 맵 구역 ID를 바꿉니다. None은 유효한 구역이 아니므로 거절합니다. */
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool SetCurrentAreaId(FName AreaId);

	/** 체크포인트 또는 Teleport가 마지막으로 기록한 현재 진행 구역을 읽습니다. */
	UFUNCTION(BlueprintPure, Category = "Game Flow")
	FName GetCurrentAreaId() const { return CurrentAreaId; }

	/** 특정 체크포인트가 현재 플레이/복구 시점에 이미 활성화되었는지 확인합니다. */
	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	bool HasCheckpointBeenActivated(FName CheckpointId) const;

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	TArray<FName> GetCompletedPuzzleIds() const;

	UFUNCTION(BlueprintPure, Category = "One Time Event")
	TArray<FName> GetCompletedOneTimeEventIds() const;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnGamePhaseChangedSignature OnGamePhaseChanged;

	// 아래 On... 변수는 UI/팀원 Actor가 듣는 알림 창구입니다. Blueprint에서
	// Bind Event를 연결하면 cpp의 Broadcast() 순간 그 연결된 동작이 호출됩니다.

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

	/**
	 * 체크포인트 재시작으로 새 Player Pawn이 만들어지고 Inventory 복구까지 끝난 뒤 발생합니다.
	 * Player, Combat, Inventory, UI 담당자는 자기 코드를 GameMode에 직접 결합하지 않고
	 * 이 이벤트를 받아 새 Pawn 참조와 화면 표시를 다시 연결할 수 있습니다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnCheckpointPlayerRespawnedSignature OnCheckpointPlayerRespawned;

	UPROPERTY(BlueprintAssignable, Category = "Game Events")
	FOnMainMenuRequestedSignature OnMainMenuRequested;

protected:
	// protected는 이 GameMode를 상속한 Blueprint/C++ 자식에서 설정하거나 사용할 부분입니다.
	// BeginPlay는 게임 월드 시작, EndPlay는 종료 때 Unreal이 자동으로 호출합니다.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 인트로가 없는 초기 테스트에서만 true로 사용하고, 실제 게임에서는 false를 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	bool StartAutomatically = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objective")
	FName FirstObjectiveId = TEXT("ExplorePatientRoom");

	// ID는 코드가 같은 목표를 구별하기 위한 이름이고, Text는 플레이어가 읽을 문장입니다.
	// EditDefaultsOnly는 GameMode Blueprint의 기본값에서 문구와 ID를 설정할 수 있다는 뜻입니다.
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

	/**
	 * 생명유지장치 구역으로 이동하기 전에 반드시 해결해야 하는 퍼즐 ID 목록입니다.
	 * PZ-02와 PZ-03의 세부 ID는 팀에서 확정한 값을 Blueprint GameMode 기본값에 넣습니다.
	 * 목록이 비어 있으면 퍼즐 조건을 검사하지 않으므로, 실제 제출용 Blueprint에서는 반드시 확인해야 합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Final Objective")
	TArray<FName> RequiredPuzzleIdsForFinalObjective;

	/**
	 * 문서 확인이나 안내 연출처럼 최종 구역 진입 전에 끝나야 하는 일회성 Event ID 목록입니다.
	 * 기획상 필수 Event가 없으면 비워 두어도 됩니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Final Objective")
	TArray<FName> RequiredEventIdsForFinalObjective;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Escape", meta = (ClampMin = "1"))
	int32 DefaultEscapeTimeLimitSeconds = 300;

	/**
	 * 아래 등급 컷은 '총 클리어 시간(초)'의 임시 설정값입니다.
	 * 1800초(30분) 이내 S, 그다음 2400초(40분) 이내 A처럼 순서대로 검사합니다.
	 * KillCount는 기록에는 남지만 현재 등급 조건에는 포함되지 않습니다.
	 * 회색 박스 맵을 실제로 완주한 뒤 플레이 시간에 맞춰 조정할 값입니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 SRankMaximumTimeSeconds = 1800;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 ARankMaximumTimeSeconds = 2400;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 BRankMaximumTimeSeconds = 3000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 CRankMaximumTimeSeconds = 3600;

	/**
	 * UI/Inventory에서 플레이어의 일반 폐기를 막아야 하는 진행 아이템 ID 목록입니다.
	 * 문 퍼즐이 Key를 정상적으로 사용할 때는 일반 폐기가 아니므로 ConsumeKeyItem()으로 소비할 수 있습니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle")
	TArray<FName> ProtectedKeyItemIds;

	/** 예: L_MainMenu. 비어 있으면 OnMainMenuRequested 이벤트만 발생합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	FName MainMenuLevelName = NAME_None;

private:
	// private는 이 클래스 내부에서만 사용하는 구현 도구와 기록입니다.
	// 밖에서는 public 함수로 결과를 전달하고 Getter로 현재 값을 확인해야 합니다.
	/** 1초마다 플레이 시간을 더하고 Escape 중에는 남은 시간을 줄입니다. */
	void UpdateGameTimers();
	/** 초로 받은 플레이 시간을 에디터에 설정한 등급 컷과 비교합니다. */
	EDeadHospitalRank CalculateRankFromClearTime(int32 ClearTimeSeconds) const;
	/** 단계가 실제로 바뀔 때만 기록하고 OnGamePhaseChanged 알림을 보냅니다. */
	void SetGamePhase(EDeadHospitalGamePhase NewGamePhase);
	/** 사망/시간초과 원인을 기록하고 타이머·입력을 멈춰 GameOver로 바꿉니다. */
	void FinishWithGameOver(EDeadHospitalGameOverReason NewGameOverReason);
	/** Waiting/Ending/실패·완료를 제외한 실제 진행 단계인지 검사합니다. */
	bool IsActiveGameplayPhase() const;
	/** 더 이상 진행하면 안 되는 GameOver 또는 Cleared인지 검사합니다. */
	bool IsTerminalPhase() const;
	/** 로컬 Player Controller와 Pawn의 이동/시점 및 Pawn 입력을 잠그거나 복구합니다. */
	void SetLocalPlayerInputEnabled(bool ShouldEnableInput);
	/** 기존 예약을 지운 후 진행 단계에서만 1초 간격 월드 타이머를 다시 겁니다. */
	void RestartGameTimer();
	/** LastCheckpoint에 저장해 둔 시간·목표·퍼즐·Event 등을 GameMode에 되돌립니다. */
	void RestoreInternalCheckpointState();

	// TimerHandle은 '나중에 다시 실행될 함수'의 예약 표입니다. 이 표를 보관해야
	// 엔딩/사망/맵 종료 때 ClearTimer로 이전 예약을 안전하게 취소할 수 있습니다.
	FTimerHandle GameTimerHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	EDeadHospitalGamePhase CurrentGamePhase = EDeadHospitalGamePhase::Waiting;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Records", meta = (AllowPrivateAccess = "true"))
	EDeadHospitalGameOverReason GameOverReason = EDeadHospitalGameOverReason::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objective", meta = (AllowPrivateAccess = "true"))
	FDeadHospitalObjectiveState CurrentObjective;

	/**
	 * Player가 현재 어느 구역에 있는지 나타내는 논리적인 이름입니다.
	 * 위치 좌표 자체가 아니라 Hospital_B2 같은 진행용 ID이며 체크포인트에 함께 저장됩니다.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Game Flow", meta = (AllowPrivateAccess = "true"))
	FName CurrentAreaId = NAME_None;

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

	/**
	 * TSet은 같은 값을 Add해도 하나만 보관하는 자료형입니다.
	 * Completed는 끝난 퍼즐/연출, Running은 시작을 예약하고 아직 끝나지 않은 연출입니다.
	 * 체크포인트와 킬에도 같은 원리를 적용해 중복 발동을 막습니다.
	 * CountedEnemies의 TWeakObjectPtr은 몬스터를 강제로 계속 살아 있게 붙잡지 않는 참조입니다.
	 */
	TSet<FName> CompletedPuzzleIds;
	TSet<FName> CompletedEventIds;
	TSet<FName> RunningEventIds;
	TSet<FName> ActivatedCheckpointIds;
	TSet<TWeakObjectPtr<AActor>> CountedEnemies;
};
