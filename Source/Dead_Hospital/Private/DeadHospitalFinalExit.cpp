// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalFinalExit.h"

#include "DeadHospitalGameMode.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ADeadHospitalFinalExit::ADeadHospitalFinalExit()
{
	// Tick 없이 E 입력/Overlap 이벤트가 있을 때만 검사합니다.
	PrimaryActorTick.bCanEverTick = false;
	InteractionText = FText::FromString(TEXT("E 키로 병원 탈출"));

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	// 최종 탈출 판정에는 플레이어 Pawn과의 겹침만 필요합니다.
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
}

void ADeadHospitalFinalExit::Interact_Implementation(AActor* Interactor)
{
	// E 키를 누른 대상이 Player이며 탈출 단계인지 검사한 뒤 엔딩 시작을 요청합니다.
	// 두 검사 중 하나라도 실패하면 Blueprint에서 '아직 탈출할 수 없음' 피드백을 냅니다.
	if (!CanInteract_Implementation(Interactor) || !TryUseFinalExit())
	{
		OnExitUseDenied();
	}
}

bool ADeadHospitalFinalExit::CanInteract_Implementation(AActor* Interactor) const
{
	// Cast는 전달된 Actor가 Player가 조종하는 Pawn인지 확인할 준비입니다.
	// GameMode에는 생명유지장치/게임 단계/남은 시간이라는 공통 진행 기록이 있습니다.
	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();

	// 최종 출구는 Player가 직접 조사하고, 생명유지장치 종료 후 Escape 단계일 때만 사용할 수 있습니다.
	// 따라서 탐색 중 실수로 출구를 발견해도 바로 엔딩으로 건너뛸 수 없습니다.
	return !ExitSucceeded
		&& IsValid(PlayerPawn)
		&& PlayerPawn->IsPlayerControlled()
		&& IsValid(GameMode)
		&& GameMode->HasLifeSupportBeenShutdown()
		&& GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Escape
		&& GameMode->GetEscapeRemainingTimeSeconds() > 0;
}

FText ADeadHospitalFinalExit::GetInteractionText_Implementation() const
{
	return InteractionText;
}

void ADeadHospitalFinalExit::BeginPlay()
{
	// Overlap 알림은 항상 등록하지만 아래 Handler가 TriggerOnPlayerOverlap=false이면
	// 곧바로 종료합니다. 따라서 기본 E 상호작용을 방해하지 않습니다.
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
	// ExitSucceeded는 E 연타나 Overlap 중복으로 엔딩이 여러 번 시작되지 않게 합니다.
	// GameMode의 TryStartEnding도 Escape/시간/장치 종료 조건을 다시 확인합니다.
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
	// 성공을 먼저 표시하고 Blueprint 연출을 호출해야 연출에서 다시 E가 들어와도
	// 두 번째 엔딩이 실행되지 않습니다.

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
	// 엔딩이 요청되지 않았으면 아무 결과도 만들지 않습니다. 성공 뒤에는
	// 예약된 자동 타이머를 지우고 GameMode에 최종 클리어/등급 확정을 맡깁니다.
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
	// Overlap은 충돌 범위에 들어온 다른 Actor를 알립니다. 옵션이 켜져 있을 때만
	// Player Pawn을 검사하고 엔딩을 요청하므로 AI 진입은 무시합니다.
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
