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
	Super::BeginPlay();

	IsActivated = StartsActivated;
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalTeleportTrigger::HandleTriggerBeginOverlap);
}

void ADeadHospitalTeleportTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TeleportTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalTeleportTrigger::ActivateTeleporter()
{
	if (!OneUseOnly || !HasBeenUsed)
	{
		IsActivated = true;
	}
}

void ADeadHospitalTeleportTrigger::DeactivateTeleporter()
{
	IsActivated = false;
}

void ADeadHospitalTeleportTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	if (!IsActivated || (OneUseOnly && HasBeenUsed) || !IsValid(DestinationActor) || !CanBeginTeleport())
	{
		return;
	}

	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	HasBeenUsed = true;
	PendingPlayerPawn = PlayerPawn;

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (IsValid(PlayerController))
	{
		// 암전 중 이동과 시점 입력을 막고, 도착 위치가 준비된 뒤 다시 허용합니다.
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);

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
	APawn* PlayerPawn = PendingPlayerPawn.Get();
	if (!IsValid(PlayerPawn) || !IsValid(DestinationActor))
	{
		PendingPlayerPawn.Reset();
		HasBeenUsed = false;
		return;
	}

	const FVector DestinationLocation = DestinationActor->GetActorLocation() + DestinationOffset;
	const FRotator DestinationRotation = DestinationActor->GetActorRotation();
	const bool TeleportSucceeded = PlayerPawn->TeleportTo(DestinationLocation, DestinationRotation, false, true);

	APlayerController* PlayerController = Cast<APlayerController>(PlayerPawn->GetController());
	if (IsValid(PlayerController))
	{
		PlayerController->SetControlRotation(DestinationRotation);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);

		if (IsValid(PlayerController->PlayerCameraManager) && FadeDurationSeconds > 0.0f)
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

	if (!TeleportSucceeded)
	{
		HasBeenUsed = false;
		PendingPlayerPawn.Reset();
		UE_LOG(LogTemp, Warning, TEXT("Teleport failed. Check destination and collision."));
		return;
	}

	// 실제 위치 이동과 조작 복구가 끝난 뒤에만 다음 게임 단계를 시작합니다.
	if (ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		if (TeleportPurpose == EDeadHospitalTeleportPurpose::EnterFinalObjective)
		{
			DeadHospitalGameMode->StartFinalObjective();
		}
		else if (TeleportPurpose == EDeadHospitalTeleportPurpose::ReturnToHospitalAndStartEscape)
		{
			DeadHospitalGameMode->StartEscapePhase(EscapeDurationSeconds);
		}
	}

	PendingPlayerPawn.Reset();
	UE_LOG(LogTemp, Log, TEXT("Player teleport completed."));
}

bool ADeadHospitalTeleportTrigger::CanBeginTeleport() const
{
	const ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(DeadHospitalGameMode))
	{
		return false;
	}

	switch (TeleportPurpose)
	{
	case EDeadHospitalTeleportPurpose::EnterFinalObjective:
		return DeadHospitalGameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing
			&& !DeadHospitalGameMode->HasLifeSupportBeenShutdown();

	case EDeadHospitalTeleportPurpose::ReturnToHospitalAndStartEscape:
		return DeadHospitalGameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::ReturningToHospital
			&& DeadHospitalGameMode->HasLifeSupportBeenShutdown();

	case EDeadHospitalTeleportPurpose::RegularTransition:
	default:
		return DeadHospitalGameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing
			|| DeadHospitalGameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Escape;
	}
}
