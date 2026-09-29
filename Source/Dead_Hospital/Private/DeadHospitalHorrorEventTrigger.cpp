// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalHorrorEventTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ADeadHospitalHorrorEventTrigger::ADeadHospitalHorrorEventTrigger()
{
	// Tick 대신 Player가 TriggerBox에 들어오는 Overlap 이벤트만 듣습니다.
	// QueryOnly + Pawn Overlap은 Player를 막는 벽이 아니라 진입 감지 범위입니다.
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(120.0f, 120.0f, 120.0f));
}

void ADeadHospitalHorrorEventTrigger::BeginPlay()
{
	// 게임 시작 시 Box/단계 변경/체크포인트 복구 알림을 연결하고,
	// GameMode 저장 목록에서 이미 본 공포 이벤트인지 읽습니다.
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalHorrorEventTrigger::HandleTriggerBeginOverlap);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnGamePhaseChanged.AddDynamic(this, &ADeadHospitalHorrorEventTrigger::HandleGamePhaseChanged);
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalHorrorEventTrigger::HandleCheckpointRestored);
	}

	if (EventId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: EventId is None, so this horror event cannot run."), *GetName());
	}

	RefreshStateFromGameMode();
}

void ADeadHospitalHorrorEventTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CompletionTimerHandle);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnGamePhaseChanged.RemoveDynamic(this, &ADeadHospitalHorrorEventTrigger::HandleGamePhaseChanged);
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalHorrorEventTrigger::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalHorrorEventTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	TryStartHorrorEvent(PlayerPawn);
}

bool ADeadHospitalHorrorEventTrigger::TryStartHorrorEvent(APawn* PlayerPawn)
{
	// 시작 가능 여부 -> GamePhase -> 필요한 퍼즐/Event -> GameMode 중복 예약 순서입니다.
	// 조건 미달이면 연출을 재생하지 않고 Player도 잠그지 않습니다.
	if (!EventEnabled || EventRunning || EventId.IsNone() || !IsValid(PlayerPawn))
	{
		return false;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return false;
	}

	if (RequireSpecificGamePhase && GameMode->GetCurrentGamePhase() != RequiredGamePhase)
	{
		return false;
	}

	if (!AreStartRequirementsMet(GameMode))
	{
		return false;
	}

	// TryStartOneTimeEvent가 Running ID도 검사하므로 Trigger가 겹쳐도 같은 연출은 하나만 시작됩니다.
	if (!GameMode->TryStartOneTimeEvent(EventId))
	{
		return false;
	}

	EventRunning = true;
	// GameMode의 RunningEventIds는 전역 중복 방지, 이 bool은 이 Actor의 실행 상태입니다.
	StoredPlayerPawn = PlayerPawn;

	if (LockPlayerInputDuringEvent)
	{
		SetStoredPlayerInputEnabled(false);
		PlayerInputWasLocked = true;
	}

	OnHorrorEventStarted(PlayerPawn);
	// 이 호출은 Blueprint에 "이제 연출 재생"을 알립니다. 여기서 즉시 완료할 수 있으므로
	// 아래 자동 타이머를 만들기 전에 EventRunning 상태를 한 번 더 봅니다.

	// Blueprint가 OnHorrorEventStarted 안에서 즉시 완료했을 수도 있으므로 EventRunning을 다시 확인합니다.
	if (CompleteAutomatically && EventRunning)
	{
		if (AutomaticCompletionDelaySeconds <= 0.0f)
		{
			CompleteHorrorEvent();
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				CompletionTimerHandle,
				this,
				&ADeadHospitalHorrorEventTrigger::HandleAutomaticCompletion,
				AutomaticCompletionDelaySeconds,
				false
			);
		}
	}

	return true;
}

bool ADeadHospitalHorrorEventTrigger::CompleteHorrorEvent()
{
	// 실제 연출 종료 시 GameMode에 EventId를 기록하고, 다음 진입을 비활성화합니다.
	// 기록에 실패하면 CancelHorrorEvent가 예약과 입력 잠금을 복구합니다.
	if (!EventRunning)
	{
		return false;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode) || !GameMode->CompleteOneTimeEvent(EventId))
	{
		CancelHorrorEvent();
		return false;
	}

	GetWorldTimerManager().ClearTimer(CompletionTimerHandle);
	EventRunning = false;
	EventEnabled = false;

	if (PlayerInputWasLocked)
	{
		SetStoredPlayerInputEnabled(true);
	}

	PlayerInputWasLocked = false;
	StoredPlayerPawn.Reset();
	OnHorrorEventFinished();
	return true;
}

void ADeadHospitalHorrorEventTrigger::HandleAutomaticCompletion()
{
	// 타이머는 결과값을 받을 곳이 없기 때문에 void 함수만 직접 연결할 수 있습니다.
	// 실제 성공/실패 판단은 기존 CompleteHorrorEvent 함수가 그대로 담당합니다.
	CompleteHorrorEvent();
}

void ADeadHospitalHorrorEventTrigger::CancelHorrorEvent()
{
	// 연출 준비 실패/사망/엔딩 때 호출합니다. 타이머 취소와 Running 예약 해제를
	// 함께 해야 다음 정상 진입에서 Event가 다시 시작될 수 있습니다.
	if (!EventRunning)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CompletionTimerHandle);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->CancelOneTimeEvent(EventId);
	}

	EventRunning = false;

	// GameOver와 Ending에서는 GameMode가 입력을 계속 막아야 하므로 여기서 다시 켜지 않습니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (PlayerInputWasLocked
		&& IsValid(GameMode)
		&& GameMode->GetCurrentGamePhase() != EDeadHospitalGamePhase::GameOver
		&& GameMode->GetCurrentGamePhase() != EDeadHospitalGamePhase::Ending
		&& GameMode->GetCurrentGamePhase() != EDeadHospitalGamePhase::Cleared)
	{
		SetStoredPlayerInputEnabled(true);
	}

	PlayerInputWasLocked = false;
	StoredPlayerPawn.Reset();
	OnHorrorEventCancelled();
}

void ADeadHospitalHorrorEventTrigger::SetHorrorEventEnabled(bool ShouldEnable)
{
	if (EventRunning)
	{
		return;
	}

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	const bool AlreadyCompleted = IsValid(GameMode) && GameMode->IsOneTimeEventCompleted(EventId);
	EventEnabled = ShouldEnable && !AlreadyCompleted;
}

void ADeadHospitalHorrorEventTrigger::HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase)
{
	if (!EventRunning)
	{
		return;
	}

	if (NewGamePhase == EDeadHospitalGamePhase::GameOver
		|| NewGamePhase == EDeadHospitalGamePhase::Ending
		|| NewGamePhase == EDeadHospitalGamePhase::Cleared)
	{
		CancelHorrorEvent();
	}
}

void ADeadHospitalHorrorEventTrigger::HandleCheckpointRestored(FName CheckpointId)
{
	// 마지막 저장 시점에 이미 완료였으면 다시 표시하지 않고, 미완료였으면
	// 다음 통과에서 다시 연출할 수 있게 실행 중 상태/타이머를 비웁니다.
	GetWorldTimerManager().ClearTimer(CompletionTimerHandle);
	EventRunning = false;
	PlayerInputWasLocked = false;
	StoredPlayerPawn.Reset();
	RefreshStateFromGameMode();
}

void ADeadHospitalHorrorEventTrigger::SetStoredPlayerInputEnabled(bool ShouldEnableInput)
{
	APawn* PlayerPawn = StoredPlayerPawn.Get();
	if (!IsValid(PlayerPawn))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(!ShouldEnableInput);
	PlayerController->SetIgnoreLookInput(!ShouldEnableInput);

	if (ShouldEnableInput)
	{
		PlayerPawn->EnableInput(PlayerController);
	}
	else
	{
		PlayerPawn->DisableInput(PlayerController);
	}
}

void ADeadHospitalHorrorEventTrigger::RefreshStateFromGameMode()
{
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	const bool WasCompleted = IsValid(GameMode) && GameMode->IsOneTimeEventCompleted(EventId);
	EventEnabled = StartsEnabled && !WasCompleted;
	TriggerBox->SetGenerateOverlapEvents(EventEnabled);
	OnHorrorEventStateRestored(WasCompleted);
}

bool ADeadHospitalHorrorEventTrigger::AreStartRequirementsMet(const ADeadHospitalGameMode* GameMode) const
{
	// for는 배열에 넣은 이름을 하나씩 확인하는 반복문입니다. 필요한 퍼즐/Event가
	// 하나라도 미완료면 false입니다. 동시에 실행하면 안 되는 Event가 Running이어도 false.
	if (!IsValid(GameMode))
	{
		return false;
	}

	for (const FName RequiredPuzzleId : RequiredPuzzleIds)
	{
		if (RequiredPuzzleId.IsNone() || !GameMode->IsPuzzleCompleted(RequiredPuzzleId))
		{
			return false;
		}
	}

	for (const FName RequiredEventId : RequiredCompletedEventIds)
	{
		if (RequiredEventId.IsNone() || !GameMode->IsOneTimeEventCompleted(RequiredEventId))
		{
			return false;
		}
	}

	for (const FName OtherEventId : MutuallyExclusiveEventIds)
	{
		if (!OtherEventId.IsNone() && GameMode->IsOneTimeEventRunning(OtherEventId))
		{
			return false;
		}
	}

	return true;
}
