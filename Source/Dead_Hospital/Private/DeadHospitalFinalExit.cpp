// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalFinalExit.h"

#include "DeadHospitalGameMode.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ADeadHospitalFinalExit::ADeadHospitalFinalExit()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	// 최종 탈출 판정에는 플레이어 Pawn과의 겹침만 필요합니다.
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
}

void ADeadHospitalFinalExit::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalFinalExit::HandleTriggerBeginOverlap);
}

void ADeadHospitalFinalExit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EndingTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool ADeadHospitalFinalExit::TryUseFinalExit()
{
	if (ExitSucceeded)
	{
		return false;
	}

	ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(DeadHospitalGameMode) || !DeadHospitalGameMode->TryStartEnding())
	{
		return false;
	}

	ExitSucceeded = true;

	// 먼저 블루프린트에 엔딩 시작을 알린 뒤, 필요하다면 임시 자동 완료 타이머를 시작합니다.
	OnEndingRequested();

	if (AutomaticallyCompleteEnding)
	{
		if (AutomaticEndingDelaySeconds <= 0.0f)
		{
			CompleteEnding();
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				EndingTimerHandle,
				this,
				&ADeadHospitalFinalExit::CompleteEnding,
				AutomaticEndingDelaySeconds,
				false
			);
		}
	}

	return true;
}

void ADeadHospitalFinalExit::CompleteEnding()
{
	if (!ExitSucceeded)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(EndingTimerHandle);

	if (ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		DeadHospitalGameMode->CompleteGameClear();
	}
}

void ADeadHospitalFinalExit::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	if (!TriggerOnPlayerOverlap)
	{
		return;
	}

	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	TryUseFinalExit();
}
