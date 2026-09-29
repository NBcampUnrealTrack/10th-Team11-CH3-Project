// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalLifeSupportDevice.h"

#include "DeadHospitalGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace DeadHospitalLifeSupportEventIds
{
	// 장치 연출 완료를 GameMode/체크포인트에서 같은 이름으로 구분합니다.
	// namespace는 이 이름이 다른 파일의 우연히 같은 이름과 충돌하지 않게 묶는 공간입니다.
	const FName ShutdownSequence = TEXT("LifeSupportShutdown");
}

ADeadHospitalLifeSupportDevice::ADeadHospitalLifeSupportDevice()
{
	PrimaryActorTick.bCanEverTick = false;

	DeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
	SetRootComponent(DeviceMesh);
	InteractionText = FText::FromString(TEXT("E 키로 생명유지장치 종료"));
}

void ADeadHospitalLifeSupportDevice::BeginPlay()
{
	// 게임 시작/체크포인트 복구 시 GameMode 상태를 읽어 장치 외형을 맞춥니다.
	// 저장된 상태가 이미 Escape이면 장치를 또 종료하지 않아야 합니다.
	Super::BeginPlay();

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		DeviceShutdown = GameMode->HasLifeSupportBeenShutdown();
		EscapeSequenceFinished = GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Escape;
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalLifeSupportDevice::HandleCheckpointRestored);
		GameMode->OnGamePhaseChanged.AddDynamic(this, &ADeadHospitalLifeSupportDevice::HandleGamePhaseChanged);
	}

	OnLifeSupportStateRestored(DeviceShutdown);
}

void ADeadHospitalLifeSupportDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SequenceTimerHandle);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalLifeSupportDevice::HandleCheckpointRestored);
		GameMode->OnGamePhaseChanged.RemoveDynamic(this, &ADeadHospitalLifeSupportDevice::HandleGamePhaseChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalLifeSupportDevice::Interact_Implementation(AActor* Interactor)
{
	TryShutdownDeviceForInteractor(Interactor);
}

bool ADeadHospitalLifeSupportDevice::CanInteract_Implementation(AActor* Interactor) const
{
	// 이미 장치를 껐거나 성불 완료 처리를 시작했다면 같은 E 입력을 다시 받지 않습니다.
	// 최신 GDD는 같은 위치에서 탈출을 시작하므로 예전 EscapeDestinationActor의 연결 여부는 검사하지 않습니다.
	if (DeviceShutdown || SequenceCompletionStarted)
	{
		return false;
	}

	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	return IsValid(PlayerPawn)
		&& PlayerPawn->IsPlayerControlled()
		&& IsValid(GameMode)
		&& GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::FinalObjective;
}

FText ADeadHospitalLifeSupportDevice::GetInteractionText_Implementation() const
{
	return InteractionText;
}

bool ADeadHospitalLifeSupportDevice::TryShutdownDevice()
{
	return TryShutdownDeviceForInteractor(UGameplayStatics::GetPlayerPawn(this, 0));
}

bool ADeadHospitalLifeSupportDevice::TryShutdownDeviceForInteractor(AActor* Interactor)
{
	// 순서: 유효한 Player 검사 -> 일회성 Event 예약 -> GameMode 장치 종료 기록 ->
	// Actor 상태 저장 -> 입력 잠금 -> Blueprint 성불 연출 시작.
	// 연출 자체는 Blueprint 담당이므로 C++ 함수만으로 귀신 애니메이션이 생기지는 않습니다.
	if (!CanInteract_Implementation(Interactor))
	{
		OnDeviceShutdownRejected();
		return false;
	}

	APawn* PlayerPawn = Cast<APawn>(Interactor);
	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(PlayerPawn)
		|| !IsValid(GameMode)
		|| !GameMode->TryStartOneTimeEvent(DeadHospitalLifeSupportEventIds::ShutdownSequence))
	{
		OnDeviceShutdownRejected();
		return false;
	}

	if (!GameMode->CompleteLifeSupportShutdown())
	{
		GameMode->CancelOneTimeEvent(DeadHospitalLifeSupportEventIds::ShutdownSequence);
		OnDeviceShutdownRejected();
		return false;
	}

	// 상태 값을 연출 호출보다 먼저 바꾸면 E 키를 빠르게 연타해도 두 번째 호출은 즉시 차단됩니다.
	DeviceShutdown = true;
	StoredPlayerPawn = PlayerPawn;
	SetStoredPlayerInputEnabled(false);
	OnDeviceShutdownAccepted();

	if (AutomaticallyCompleteAscensionSequence)
	{
		StartAutomaticSequenceCompletion();
	}

	return true;
}

bool ADeadHospitalLifeSupportDevice::CompleteAscensionSequence()
{
	// Level Sequence의 Finished 알림이 오면 Player를 옮기지 않고 현재 위치에서 조작과 Escape를 시작합니다.
	// 저장한 Player가 이미 사라졌거나 같은 Finished 알림이 두 번 왔다면 진행 상태를 다시 바꾸지 않습니다.
	if (!DeviceShutdown
		|| SequenceCompletionStarted
		|| EscapeSequenceFinished
		|| !StoredPlayerPawn.IsValid())
	{
		return false;
	}

	// true로 먼저 바꾸어 같은 Finished 알림이 중복 도착해도 탈출 타이머가 두 번 시작되지 않게 합니다.
	SequenceCompletionStarted = true;
	FinishAscensionAndStartEscape();
	return EscapeSequenceFinished;
}

void ADeadHospitalLifeSupportDevice::SetEscapeDestinationReady(bool IsReady)
{
	// 이전 Blueprint에 이미 연결된 노드를 깨지 않기 위해 값은 저장하지만 최신 흐름에서는 사용하지 않습니다.
	// 현재는 순간이동 목적지가 없으므로 이 값이 false여도 장치 정지와 탈출 시작을 막지 않습니다.
	EscapeDestinationReady = IsReady;
}

void ADeadHospitalLifeSupportDevice::FinishAscensionAndStartEscape()
{
	// Player의 위치와 회전은 전혀 바꾸지 않습니다. 특수중환자격리실에서 직접 정문까지 달려가야 하기 때문입니다.
	// StoredPlayerPawn은 위치 이동용이 아니라 연출 동안 잠갔던 바로 그 Player의 입력을 되돌리기 위해 보관한 참조입니다.
	APawn* PlayerPawn = StoredPlayerPawn.Get();
	if (!IsValid(PlayerPawn))
	{
		RestoreAfterEscapeStartFailure();
		return;
	}

	// 최신 GDD 순서인 "성불 연출 완료 → 입력 복구 → Escape 상태와 Timer 시작"을 지킵니다.
	SetStoredPlayerInputEnabled(true);

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode) || !GameMode->StartEscapePhase(EscapeDurationSeconds))
	{
		RestoreAfterEscapeStartFailure();
		return;
	}

	if (!GameMode->CompleteOneTimeEvent(DeadHospitalLifeSupportEventIds::ShutdownSequence))
	{
		// 이 지점은 Escape 전환이 이미 성공한 뒤이므로 게임 진행은 유지합니다.
		// 다만 완료 ID가 저장되지 않으면 체크포인트 중복 방지가 약해지므로 반드시 로그로 알려 줍니다.
		UE_LOG(LogTemp, Error, TEXT("LifeSupportShutdown EventId could not be completed after Escape started."));
	}
	EscapeSequenceFinished = true;
	SequenceCompletionStarted = false;

	StoredPlayerPawn.Reset();
	OnEscapeTeleportCompleted();
}

void ADeadHospitalLifeSupportDevice::SetStoredPlayerInputEnabled(bool ShouldEnableInput)
{
	// PlayerController의 이동/시야 입력과 Pawn의 입력을 함께 잠그거나 다시 켭니다.
	// 이 작업을 빼면 검은 화면/성불 연출 도중 Player가 걸어갈 수 있습니다.
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

void ADeadHospitalLifeSupportDevice::StartAutomaticSequenceCompletion()
{
	// 실제 Sequence가 없는 개발 테스트에서는 설정 초가 지난 뒤 자동으로
	// CompleteAscensionSequence를 호출해 나머지 동선을 검증합니다.
	if (AutomaticSequenceDelaySeconds <= 0.0f)
	{
		CompleteAscensionSequence();
		return;
	}

	GetWorldTimerManager().SetTimer(
		SequenceTimerHandle,
		this,
		&ADeadHospitalLifeSupportDevice::HandleAutomaticSequenceCompletion,
		AutomaticSequenceDelaySeconds,
		false
	);
}

void ADeadHospitalLifeSupportDevice::HandleAutomaticSequenceCompletion()
{
	// 타이머는 bool 반환값을 사용할 수 없으므로 이 void 함수가 실제 완료 함수를 한 번 호출합니다.
	CompleteAscensionSequence();
}

void ADeadHospitalLifeSupportDevice::RestoreAfterEscapeStartFailure()
{
	// Escape 시작에 실패했는데 Player 입력까지 꺼져 있으면 진행할 수 없습니다.
	// 다만 GameOver/Ending/Cleared 상태에서는 사망/엔딩 입력 잠금이 우선입니다.
	SequenceCompletionStarted = false;

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	const bool GameMustKeepInputLocked = IsValid(GameMode)
		&& (GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::GameOver
			|| GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Ending
			|| GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Cleared);

	// GameOver 화면이 떠 있는 동안 입력을 다시 켜면 사망한 Player가 움직일 수 있습니다.
	// 일반적인 연동 실패일 때만 입력을 원상 복구하여 진행이 완전히 멈추지 않게 합니다.
	if (!GameMustKeepInputLocked)
	{
		SetStoredPlayerInputEnabled(true);
	}

	OnEscapeTeleportFailed();
}

void ADeadHospitalLifeSupportDevice::HandleCheckpointRestored(FName CheckpointId)
{
	// 저장 시점으로 되돌아갈 때 이전 실행의 타이머/Player 참조를 버립니다.
	// 저장 당시 장치를 껐는지, Escape 단계였는지 GameMode에서 다시 읽습니다.
	GetWorldTimerManager().ClearTimer(SequenceTimerHandle);
	StoredPlayerPawn.Reset();
	SequenceCompletionStarted = false;

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	DeviceShutdown = IsValid(GameMode) && GameMode->HasLifeSupportBeenShutdown();
	EscapeSequenceFinished = IsValid(GameMode)
		&& GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Escape;

	OnLifeSupportStateRestored(DeviceShutdown);
}

void ADeadHospitalLifeSupportDevice::HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase)
{
	if (NewGamePhase != EDeadHospitalGamePhase::GameOver
		&& NewGamePhase != EDeadHospitalGamePhase::Ending
		&& NewGamePhase != EDeadHospitalGamePhase::Cleared)
	{
		return;
	}

	// 사망이나 게임 종료 뒤에도 Sequence Timer가 남아 있으면 늦게 Escape가 시작될 수 있습니다.
	// Timer를 지우고 실행 중 Event 예약도 취소하여 체크포인트 재시작을 방해하지 않게 합니다.
	GetWorldTimerManager().ClearTimer(SequenceTimerHandle);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->CancelOneTimeEvent(DeadHospitalLifeSupportEventIds::ShutdownSequence);
	}

	SequenceCompletionStarted = false;
	StoredPlayerPawn.Reset();
}
