// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalObjectiveTrigger.h"

#include "Components/BoxComponent.h"
#include "DocumentComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InventoryComponent.h"

ADeadHospitalObjectiveTrigger::ADeadHospitalObjectiveTrigger()
{
	// 목표는 Player가 구역 경계에 들어오는 순간만 바꾸면 되므로 매 프레임 Tick할 필요가 없습니다.
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
}

void ADeadHospitalObjectiveTrigger::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalObjectiveTrigger::HandleTriggerBeginOverlap);

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalObjectiveTrigger::HandleCheckpointRestored);
		HasBeenUsed = OneUseOnly && GameMode->IsOneTimeEventCompleted(TriggerEventId);
	}

	if (OneUseOnly && TriggerEventId.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: OneUseOnly ObjectiveTrigger requires TriggerEventId."), *GetName());
	}
}

void ADeadHospitalObjectiveTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalObjectiveTrigger::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalObjectiveTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	TryApplyObjectives(OtherActor);
}

bool ADeadHospitalObjectiveTrigger::TryApplyObjectives(AActor* Interactor)
{
	const APawn* PlayerPawn = Cast<APawn>(Interactor);
	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();

	// AI나 물리 오브젝트의 Overlap은 목표를 바꾸면 안 됩니다. 실제 로컬 Player가 조종하는 Pawn만 허용합니다.
	if (!IsValid(PlayerPawn)
		|| !PlayerPawn->IsPlayerControlled()
		|| !IsValid(GameMode)
		|| (OneUseOnly && (HasBeenUsed || TriggerEventId.IsNone()))
		|| !AreRequirementsMet(GameMode, Interactor))
	{
		OnObjectiveApplicationRejected();
		return false;
	}

	// 일반 Trigger는 메인/서브 중 하나 이상을 설정해야 합니다.
	// 최종 구역 Trigger는 StartFinalObjective가 M08을 자동 설정하므로 반대로 두 수동 설정을 끌 것을 요구합니다.
	if ((!StartFinalObjectiveOnTrigger
			&& !SetMainObjectiveOnTrigger
			&& !SetSubObjectiveOnTrigger
			&& SubObjectiveIdsToClearOnSuccess.IsEmpty())
		|| (SetMainObjectiveOnTrigger && MainObjectiveId.IsNone())
		|| (SetSubObjectiveOnTrigger && SubObjectiveId.IsNone())
		|| (StartFinalObjectiveOnTrigger && (SetMainObjectiveOnTrigger || SetSubObjectiveOnTrigger)))
	{
		OnObjectiveApplicationRejected();
		return false;
	}

	if (OneUseOnly && !GameMode->TryStartOneTimeEvent(TriggerEventId))
	{
		OnObjectiveApplicationRejected();
		return false;
	}

	// 특수중환자격리실은 순간이동하지 않고 문을 통과해 진입한 위치에서 FinalObjective를 시작합니다.
	// StartFinalObjective 내부가 PZ01~PZ07 완료를 다시 검사하고 M08을 설정하므로 중복 로직을 만들지 않습니다.
	if (StartFinalObjectiveOnTrigger)
	{
		const bool FinalObjectiveStarted = GameMode->StartFinalObjective();
		const bool EventCompleted = FinalObjectiveStarted
			&& (!OneUseOnly || GameMode->CompleteOneTimeEvent(TriggerEventId));

		if (!FinalObjectiveStarted || !EventCompleted)
		{
			if (OneUseOnly)
			{
				GameMode->CancelOneTimeEvent(TriggerEventId);
			}
			OnObjectiveApplicationRejected();
			return false;
		}

		HasBeenUsed = OneUseOnly;
		OnObjectivesApplied();
		return true;
	}

	// 뒤 단계가 실패했을 때 원래 목표로 돌아갈 수 있도록 바꾸기 전의 두 상태를 복사합니다.
	const FDeadHospitalObjectiveState PreviousMain = GameMode->GetCurrentMainObjective();
	const TArray<FDeadHospitalObjectiveState> PreviousSubObjectives = GameMode->GetActiveSubObjectives();

	bool MainSucceeded = true;
	bool SubSucceeded = true;
	if (SetMainObjectiveOnTrigger)
	{
		MainSucceeded = GameMode->SetMainObjective(
			MainObjectiveId,
			MainObjectiveText,
			MainCurrentProgress,
			MainTargetProgress);
	}
	if (SetSubObjectiveOnTrigger)
	{
		SubSucceeded = GameMode->SetSubObjective(
			SubObjectiveId,
			SubObjectiveText,
			SubCurrentProgress,
			SubTargetProgress);
	}

	const bool EventCompleted = !OneUseOnly || GameMode->CompleteOneTimeEvent(TriggerEventId);
	if (!MainSucceeded || !SubSucceeded || !EventCompleted)
	{
		// 한쪽만 바뀐 부분 성공 상태를 남기지 않고 두 목표를 모두 진입 전 상태로 되돌립니다.
		RestoreObjectiveState(GameMode, PreviousMain, PreviousSubObjectives);
		if (OneUseOnly)
		{
			GameMode->CancelOneTimeEvent(TriggerEventId);
		}
		OnObjectiveApplicationRejected();
		return false;
	}

	// 목표 설정과 일회성 기록이 모두 성공한 뒤에만 완료할 서브 목표를 지웁니다.
	// 먼저 지우면 뒤 단계 실패 시 UI 목록을 정확히 원상복구하기 어려우므로 항상 마지막에 처리합니다.
	for (const FName CompletedSubObjectiveId : SubObjectiveIdsToClearOnSuccess)
	{
		if (!CompletedSubObjectiveId.IsNone())
		{
			GameMode->ClearSubObjectiveById(CompletedSubObjectiveId);
		}
	}

	HasBeenUsed = OneUseOnly;
	OnObjectivesApplied();
	return true;
}

bool ADeadHospitalObjectiveTrigger::AreRequirementsMet(
	const ADeadHospitalGameMode* GameMode,
	const AActor* Interactor) const
{
	if (!IsValid(GameMode) || !IsValid(Interactor))
	{
		return false;
	}

	if (RequireSpecificGamePhase && GameMode->GetCurrentGamePhase() != RequiredGamePhase)
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

	if (!RequiredItemIds.IsEmpty())
	{
		// 아이템 조건이 있을 때만 Player의 InventoryComponent를 찾습니다.
		// 조건이 비어 있는 Trigger까지 인벤토리 유무 때문에 실패하지 않도록 검사 범위를 좁힙니다.
		const UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
		if (!IsValid(Inventory))
		{
			return false;
		}

		for (const FName RequiredItemId : RequiredItemIds)
		{
			if (RequiredItemId.IsNone() || !Inventory->HasItem(RequiredItemId))
			{
				return false;
			}
		}
	}

	if (!RequiredDocumentIds.IsEmpty())
	{
		// 문서는 일반 인벤토리와 별도인 DocumentComponent에 저장되므로 따로 확인합니다.
		const UDocumentComponent* DocumentComponent = Interactor->FindComponentByClass<UDocumentComponent>();
		if (!IsValid(DocumentComponent))
		{
			return false;
		}

		for (const FName RequiredDocumentId : RequiredDocumentIds)
		{
			if (RequiredDocumentId.IsNone() || !DocumentComponent->HasDocument(RequiredDocumentId))
			{
				return false;
			}
		}
	}

	return true;
}

void ADeadHospitalObjectiveTrigger::RestoreObjectiveState(
	ADeadHospitalGameMode* GameMode,
	const FDeadHospitalObjectiveState& PreviousMain,
	const TArray<FDeadHospitalObjectiveState>& PreviousSubObjectives) const
{
	if (!IsValid(GameMode))
	{
		return;
	}

	if (PreviousMain.IsActive)
	{
		GameMode->SetMainObjective(
			PreviousMain.ObjectiveId,
			PreviousMain.ObjectiveText,
			PreviousMain.CurrentProgress,
			PreviousMain.TargetProgress);
	}
	else
	{
		GameMode->ClearMainObjective();
	}

	// 실패 중 새로 추가된 서브 목표까지 먼저 모두 지운 뒤, 진입 전 배열을 순서대로 다시 넣습니다.
	GameMode->ClearSubObjective();
	for (const FDeadHospitalObjectiveState& PreviousSub : PreviousSubObjectives)
	{
		if (PreviousSub.IsActive)
		{
			GameMode->SetSubObjective(
				PreviousSub.ObjectiveId,
				PreviousSub.ObjectiveText,
				PreviousSub.CurrentProgress,
				PreviousSub.TargetProgress);
		}
	}
}

void ADeadHospitalObjectiveTrigger::HandleCheckpointRestored(FName CheckpointId)
{
	// GameMode가 저장 시점의 완료 Event 목록을 먼저 복구한 뒤 이 함수를 호출합니다.
	// 따라서 Trigger 자체의 임시 bool을 믿지 않고 GameMode의 기록을 다시 읽어 저장 시점과 맞춥니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	HasBeenUsed = OneUseOnly
		&& IsValid(GameMode)
		&& GameMode->IsOneTimeEventCompleted(TriggerEventId);
}
