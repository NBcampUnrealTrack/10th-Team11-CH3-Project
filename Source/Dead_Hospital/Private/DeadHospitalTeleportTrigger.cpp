// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalTeleportTrigger.h"

#include "DeadHospitalGameMode.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ADeadHospitalTeleportTrigger::ADeadHospitalTeleportTrigger()
{
	// 이 Actor는 매 프레임(Tick) 이동을 검사하지 않습니다. Player가 Box에
	// 들어오는 Overlap 알림에만 반응해 저사양 개발 중에도 불필요한 계산을 줄입니다.
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
}

void ADeadHospitalTeleportTrigger::BeginPlay()
{
	// 게임 시작에 맵 Details의 기본값을 읽고, GameMode에서 완료된 Event를 조회합니다.
	// 저장 당시 사용했던 단방향 통로는 다시 밟아도 작동하지 않게 합니다.
	Super::BeginPlay();

	IsActivated = StartsActivated;
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalTeleportTrigger::HandleTriggerBeginOverlap);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		// 게임이 도중에 끝나거나 체크포인트로 돌아갈 때 진행 중인 Teleport도 함께 정리해야 합니다.
		GameMode->OnGamePhaseChanged.AddDynamic(this, &ADeadHospitalTeleportTrigger::HandleGamePhaseChanged);
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalTeleportTrigger::HandleCheckpointRestored);

		HasBeenUsed = OneUseOnly && GameMode->IsOneTimeEventCompleted(TeleportEventId);
		IsActivated = StartsActivated && (!OneUseOnly || !HasBeenUsed);
	}

	if (OneUseOnly && TeleportEventId.IsNone())
	{
		// 고유 ID가 없으면 체크포인트 복구 뒤 사용 여부를 정확히 되돌릴 수 없습니다.
		// 조용히 잘못 동작하게 두지 않고 에디터 설정 누락을 로그로 알려 줍니다.
		UE_LOG(LogTemp, Error, TEXT("%s: OneUseOnly Teleport requires a unique TeleportEventId."), *GetName());
	}

	if (!IsValid(DestinationActor))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: DestinationActor is not assigned."), *GetName());
	}

	OnTeleportStateRestored(HasBeenUsed);
}

void ADeadHospitalTeleportTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TeleportTimerHandle);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnGamePhaseChanged.RemoveDynamic(this, &ADeadHospitalTeleportTrigger::HandleGamePhaseChanged);
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalTeleportTrigger::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalTeleportTrigger::ActivateTeleporter()
{
	// 진행 연출에서 통로를 열 때 사용합니다. 이미 한 번 사용했거나
	// 현재 암전/이동 중인 Trigger는 다시 켜지지 않습니다.
	if (!TeleportInProgress && (!OneUseOnly || !HasBeenUsed))
	{
		IsActivated = true;
	}
}

void ADeadHospitalTeleportTrigger::DeactivateTeleporter()
{
	IsActivated = false;
}

void ADeadHospitalTeleportTrigger::SetDestinationReady(bool IsReady)
{
	DestinationReady = IsReady;
}

void ADeadHospitalTeleportTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	// Overlap 이벤트는 다른 Actor가 Box에 들어왔다는 뜻입니다.
	// 먼저 통로/단계/목적지가 준비됐는지, 그다음 Player Pawn인지 확인합니다.
	if (!CanBeginTeleport())
	{
		return;
	}

	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return;
	}

	// 한 번만 쓰는 Teleport는 이동 전에 EventId를 "실행 중"으로 예약합니다.
	// 같은 프레임에 Overlap이 여러 번 들어와도 두 번째 요청은 예약 검사에서 막힙니다.
	if (OneUseOnly && !GameMode->TryStartOneTimeEvent(TeleportEventId))
	{
		return;
	}

	TeleportInProgress = true;
	// 출발 Player와 원래 위치를 기억해 중간 단계에서 실패하면 복구할 수 있게 합니다.
	PendingPlayerPawn = PlayerPawn;
	PendingSourceTransform = PlayerPawn->GetActorTransform();

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (IsValid(PlayerController))
	{
		// 암전 중 이동과 시점 입력을 막고, 도착 위치가 준비된 뒤 다시 허용합니다.
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		PlayerPawn->DisableInput(PlayerController);
		PlayerInputWasLocked = true;

		if (IsValid(PlayerController->PlayerCameraManager) && FadeDurationSeconds > 0.0f)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(
				0.0f,
				1.0f,
				FadeDurationSeconds,
				FLinearColor::Black,
				false,
				true
			);
		}
	}

	// Blueprint는 이 알림에서 엘리베이터 문/효과음을 시작할 수 있습니다.
	OnTeleportStarted(PlayerPawn);

	if (FadeDurationSeconds <= 0.0f)
	{
		PerformTeleport();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			TeleportTimerHandle,
			this,
			&ADeadHospitalTeleportTrigger::PerformTeleport,
			FadeDurationSeconds,
			false
		);
	}
}

void ADeadHospitalTeleportTrigger::PerformTeleport()
{
	// FadeDurationSeconds를 기다린 뒤에도 목적지/Player/퍼즐 조건을 다시 확인합니다.
	// 기다리는 동안 Player가 사망하거나 진행 단계가 변했을 수 있기 때문입니다.
	APawn* PlayerPawn = PendingPlayerPawn.Get();
	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!TeleportInProgress
		|| !IsValid(PlayerPawn)
		|| !IsValid(GameMode)
		|| !IsValid(DestinationActor)
		|| !DestinationReady
		|| !AreProgressRequirementsMet(GameMode)
		|| !IsPurposeAllowedInCurrentPhase(GameMode))
	{
		CancelPendingTeleport(true);
		return;
	}

	const FVector DestinationLocation = DestinationActor->GetActorLocation() + DestinationOffset;
	const FRotator DestinationRotation = DestinationActor->GetActorRotation();
	// 마지막 bNoCheck를 false로 두면 벽이나 바닥 안으로 억지 이동하지 않습니다.
	// 충돌 때문에 안전한 위치를 찾지 못하면 Teleport가 false를 반환하고 원래 위치를 유지합니다.
	const bool TeleportSucceeded = PlayerPawn->TeleportTo(DestinationLocation, DestinationRotation, false, false);
	if (!TeleportSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Teleport failed because the destination is blocked or invalid."), *GetName());
		CancelPendingTeleport(true);
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (IsValid(PlayerController))
	{
		PlayerController->SetControlRotation(DestinationRotation);
	}

	// 탈출 시작 Teleport는 가이드 순서대로 입력을 먼저 복구한 다음 Countdown을 시작합니다.
	// 다른 목적은 단계 변경이 성공한 뒤 공통 마무리에서 입력을 복구합니다.
	if (TeleportPurpose == EDeadHospitalTeleportPurpose::ReturnToHospitalAndStartEscape)
	{
		SetPendingPlayerInputEnabled(true);
	}

	// GameMode 단계 전환 자체가 OnGamePhaseChanged를 호출합니다. 이 bool은
	// "지금 이 Trigger가 의도해서 단계를 바꾸는 중"임을 표시해 자기 자신이 취소하지 않게 합니다.
	ApplyingPurposeTransition = true;
	const bool PurposeApplied = ApplyTeleportPurpose(GameMode);
	ApplyingPurposeTransition = false;

	if (!PurposeApplied)
	{
		// 위치 이동 뒤 단계 변경이 실패하면 Player를 출발 위치로 되돌립니다.
		// 이렇게 해야 최종 구역에 갇히거나 진행 단계를 건너뛰는 상태가 남지 않습니다.
		PlayerPawn->TeleportTo(
			PendingSourceTransform.GetLocation(),
			PendingSourceTransform.Rotator(),
			false,
			false
		);

		if (IsValid(PlayerController))
		{
			PlayerController->SetControlRotation(PendingSourceTransform.Rotator());
		}

		UE_LOG(LogTemp, Error, TEXT("%s: Teleport purpose could not change the game phase."), *GetName());
		CancelPendingTeleport(true);
		return;
	}

	// 실제 위치 이동과 목적에 맞는 GamePhase 전환이 모두 성공한 뒤에만 현재 구역을 바꿉니다.
	// 실패 전에 바꾸면 체크포인트/UI는 새 구역인데 Player는 이전 위치인 모순이 생길 수 있습니다.
	if (!DestinationAreaId.IsNone())
	{
		GameMode->SetCurrentAreaId(DestinationAreaId);
	}

	if (OneUseOnly)
	{
		// 기록 실패는 Output Log에 남깁니다. 이 경우 체크포인트에서 사용 여부
		// 복구가 부족할 수 있으므로 맵 연결/ID 설정을 플레이 테스트에서 확인해야 합니다.
		// 실제 이동과 단계 변경이 모두 끝난 뒤에만 완료 기록을 남깁니다.
		// 체크포인트는 이 완료 ID를 저장하고 복구하여 Teleport 사용 상태를 정확히 되돌립니다.
		if (!GameMode->CompleteOneTimeEvent(TeleportEventId))
		{
			UE_LOG(LogTemp, Error, TEXT("%s: Teleport succeeded, but EventId could not be completed."), *GetName());
		}

		HasBeenUsed = true;
		IsActivated = false;
	}

	SetPendingPlayerInputEnabled(true);
	StartFadeIn();
	TeleportInProgress = false;
	APawn* CompletedPlayerPawn = PendingPlayerPawn.Get();
	PendingPlayerPawn.Reset();
	PlayerInputWasLocked = false;
	OnTeleportCompleted(CompletedPlayerPawn);
	UE_LOG(LogTemp, Log, TEXT("Player teleport completed."));
}

bool ADeadHospitalTeleportTrigger::CanBeginTeleport() const
{
	// ||는 어느 하나라도 사용 불가이면 false라는 뜻입니다.
	// 비활성/이미 사용/빈 ID/목적지 미준비 등은 Fade나 입력 잠금 전에 차단합니다.
	if (!IsActivated
		|| TeleportInProgress
		|| (OneUseOnly && HasBeenUsed)
		|| (OneUseOnly && TeleportEventId.IsNone())
		|| !DestinationReady
		|| !IsValid(DestinationActor))
	{
		return false;
	}

	const ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(DeadHospitalGameMode))
	{
		return false;
	}

	return AreProgressRequirementsMet(DeadHospitalGameMode)
		&& IsPurposeAllowedInCurrentPhase(DeadHospitalGameMode);
}

bool ADeadHospitalTeleportTrigger::AreProgressRequirementsMet(const ADeadHospitalGameMode* GameMode) const
{
	// for는 Details에 나열한 퍼즐/Event ID를 하나씩 확인하는 반복문입니다.
	// 조건 목록이 비어 있으면 추가 제한이 없고, 하나라도 미완료면 이동하지 않습니다.
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

	return true;
}

bool ADeadHospitalTeleportTrigger::IsPurposeAllowedInCurrentPhase(const ADeadHospitalGameMode* GameMode) const
{
	// switch는 이 Trigger의 목적 enum에 맞는 경우만 선택합니다.
	// 탐색 통로는 Playing/Escape, 최종구역 진입은 Playing+필수 목표,
	// 탈출 복귀는 ReturningToHospital+장치 종료여야 합니다.
	if (!IsValid(GameMode))
	{
		return false;
	}

	if (RequireSpecificGamePhase && GameMode->GetCurrentGamePhase() != RequiredGamePhase)
	{
		return false;
	}

	switch (TeleportPurpose)
	{
	case EDeadHospitalTeleportPurpose::EnterFinalObjective:
		return GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing
			&& !GameMode->HasLifeSupportBeenShutdown()
			&& GameMode->AreFinalObjectiveRequirementsMet();

	case EDeadHospitalTeleportPurpose::ReturnToHospitalAndStartEscape:
		return GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::ReturningToHospital
			&& GameMode->HasLifeSupportBeenShutdown();

	case EDeadHospitalTeleportPurpose::RegularTransition:
	default:
		return GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing
			|| GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Escape;
	}
}

bool ADeadHospitalTeleportTrigger::ApplyTeleportPurpose(ADeadHospitalGameMode* GameMode)
{
	if (!IsValid(GameMode))
	{
		return false;
	}

	switch (TeleportPurpose)
	{
	case EDeadHospitalTeleportPurpose::EnterFinalObjective:
		return GameMode->StartFinalObjective();

	case EDeadHospitalTeleportPurpose::ReturnToHospitalAndStartEscape:
		return GameMode->StartEscapePhase(EscapeDurationSeconds);

	case EDeadHospitalTeleportPurpose::RegularTransition:
	default:
		return true;
	}
}

void ADeadHospitalTeleportTrigger::CancelPendingTeleport(bool ShouldRestorePlayerInput)
{
	// 실패/게임 종료 시 타이머와 Event 예약을 취소합니다. 일반 실패는 Player 입력과
	// 화면 밝기를 되돌리고, GameOver/Ending에서는 종료 화면의 입력 잠금을 존중합니다.
	GetWorldTimerManager().ClearTimer(TeleportTimerHandle);

	if (OneUseOnly)
	{
		if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
		{
			// 실패한 이동은 완료가 아니므로 실행 중 예약만 취소합니다.
			GameMode->CancelOneTimeEvent(TeleportEventId);
		}
	}

	if (ShouldRestorePlayerInput)
	{
		const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
		const bool GameMustKeepInputLocked = IsValid(GameMode)
			&& (GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::GameOver
				|| GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Ending
				|| GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Cleared);

		if (!GameMustKeepInputLocked)
		{
			SetPendingPlayerInputEnabled(true);
			StartFadeIn();
		}
	}

	TeleportInProgress = false;
	PlayerInputWasLocked = false;
	ApplyingPurposeTransition = false;
	PendingPlayerPawn.Reset();
	OnTeleportFailed();
}

void ADeadHospitalTeleportTrigger::SetPendingPlayerInputEnabled(bool ShouldEnableInput)
{
	// Controller 이동/시야와 Pawn 입력을 함께 잠급니다. 다시 켤 때는
	// 이 Trigger가 실제 잠갔던 입력에 대해서만 EnableInput을 실행합니다.
	APawn* PlayerPawn = PendingPlayerPawn.Get();
	if (!IsValid(PlayerPawn))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	// 잠그지 않았던 입력을 실수로 EnableInput하지 않도록 실제 잠금 여부를 함께 검사합니다.
	if (ShouldEnableInput && !PlayerInputWasLocked)
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(!ShouldEnableInput);
	PlayerController->SetIgnoreLookInput(!ShouldEnableInput);

	if (ShouldEnableInput)
	{
		PlayerPawn->EnableInput(PlayerController);
		PlayerInputWasLocked = false;
	}
	else
	{
		PlayerPawn->DisableInput(PlayerController);
		PlayerInputWasLocked = true;
	}
}

void ADeadHospitalTeleportTrigger::StartFadeIn()
{
	APawn* PlayerPawn = PendingPlayerPawn.Get();
	APlayerController* PlayerController = IsValid(PlayerPawn)
		? Cast<APlayerController>(PlayerPawn->GetController())
		: nullptr;

	if (IsValid(PlayerController)
		&& IsValid(PlayerController->PlayerCameraManager)
		&& FadeDurationSeconds > 0.0f)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(
			1.0f,
			0.0f,
			FadeDurationSeconds,
			FLinearColor::Black,
			false,
			false
		);
	}
}

void ADeadHospitalTeleportTrigger::HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase)
{
	if (!TeleportInProgress || ApplyingPurposeTransition)
	{
		return;
	}

	// 암전 대기 중 Player가 사망하거나 다른 시스템이 진행 단계를 바꿨다면,
	// 늦게 도착한 Timer가 Player를 엉뚱한 구역으로 이동시키지 못하도록 즉시 취소합니다.
	if (NewGamePhase == EDeadHospitalGamePhase::GameOver
		|| NewGamePhase == EDeadHospitalGamePhase::Ending
		|| NewGamePhase == EDeadHospitalGamePhase::Cleared
		|| !IsPurposeAllowedInCurrentPhase(GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>()))
	{
		CancelPendingTeleport(NewGamePhase != EDeadHospitalGamePhase::GameOver
			&& NewGamePhase != EDeadHospitalGamePhase::Ending
			&& NewGamePhase != EDeadHospitalGamePhase::Cleared);
	}
}

void ADeadHospitalTeleportTrigger::HandleCheckpointRestored(FName CheckpointId)
{
	// 재시작하면 예전 Player Pawn을 위한 타이머/참조는 무효입니다.
	// GameMode가 새 Pawn과 저장 상태를 복구한 뒤 이 알림을 주므로,
	// 여기서는 저장 당시 사용했던 단방향 이동인지 다시 읽기만 합니다.
	// GameMode가 새 Player의 입력을 이미 복구한 뒤 이 이벤트를 보냅니다.
	// 따라서 이전 Pawn에 대한 입력 함수를 호출하지 않고 오래된 예약과 참조만 버립니다.
	GetWorldTimerManager().ClearTimer(TeleportTimerHandle);
	TeleportInProgress = false;
	PlayerInputWasLocked = false;
	ApplyingPurposeTransition = false;
	PendingPlayerPawn.Reset();

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	HasBeenUsed = OneUseOnly
		&& IsValid(GameMode)
		&& GameMode->IsOneTimeEventCompleted(TeleportEventId);
	IsActivated = StartsActivated && (!OneUseOnly || !HasBeenUsed);
	OnTeleportStateRestored(HasBeenUsed);
}
