// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalCheckpoint.h"

#include "DeadHospitalGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADeadHospitalCheckpoint::ADeadHospitalCheckpoint()
{
	// 매 프레임 검사(Tick) 대신 Pawn이 범위에 들어오는 순간에만 작동합니다.
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// QueryOnly는 Overlap 검사만 하고 물리적으로 Player를 밀어내지는 않는다는 뜻입니다.
	// 모든 종류의 충돌을 일단 무시한 뒤 Pawn만 겹침으로 등록합니다.
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(150.0f, 150.0f, 120.0f));
	// FVector(X,Y,Z)는 3차원 크기입니다. 이 값은 상자의 중심에서 각 방향까지
	// 거리(반쪽 크기)이므로 레벨에서 필요한 통과 범위에 맞게 조절합니다.

	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(TriggerBox);
	RespawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	// Z를 조금 올려 재시작 Player가 바닥 메시와 겹쳐 생성되지 않게 합니다.
}

void ADeadHospitalCheckpoint::BeginPlay()
{
	// 게임이 시작되면 이미 저장된 활성 목록을 읽고 Overlap 알림에 함수를 연결합니다.
	Super::BeginPlay();

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		HasActivated = GameMode->HasCheckpointBeenActivated(CheckpointId);
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalCheckpoint::HandleCheckpointRestored);
	}
	CheckpointEnabled = StartsEnabled && (!OneActivationOnly || !HasActivated);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADeadHospitalCheckpoint::HandleTriggerBeginOverlap);

	if (CheckpointId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: CheckpointId is None, so this checkpoint cannot save."), *GetName());
	}
}

void ADeadHospitalCheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalCheckpoint::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalCheckpoint::HandleCheckpointRestored(FName RestoredCheckpointId)
{
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return;
	}

	// 재시작은 저장 시점으로 시간을 되돌립니다. 그 이후에만 활성화된 체크포인트는
	// 다시 밟을 수 있어야 하고, 그 이전에 밟은 지점은 자동 저장을 반복하면 안 됩니다.
	HasActivated = GameMode->HasCheckpointBeenActivated(CheckpointId);
	CheckpointEnabled = StartsEnabled && (!OneActivationOnly || !HasActivated);
}

bool ADeadHospitalCheckpoint::ActivateCheckpoint(APawn* PlayerPawn)
{
	// ||는 하나라도 문제가 있으면 거부한다는 뜻입니다. Player가 아닌 Pawn,
	// 빈 ID, 이미 한번 사용한 지점은 체크포인트 저장을 요청하지 않습니다.
	if (!CheckpointEnabled
		|| (OneActivationOnly && HasActivated)
		|| CheckpointId.IsNone()
		|| !IsValid(PlayerPawn)
		|| !PlayerPawn->IsPlayerControlled())
	{
		OnCheckpointActivationFailed();
		return false;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode)
		|| !GameMode->SaveCheckpoint(
			CheckpointId,
			PlayerPawn,
			RespawnPoint->GetComponentTransform(),
			AreaId))
	{
		OnCheckpointActivationFailed();
		return false;
	}

	// GameMode가 인벤토리까지 저장했다고 true를 준 뒤에만 지점을 사용 완료로 표시합니다.
	HasActivated = true;
	if (OneActivationOnly)
	{
		CheckpointEnabled = false;
	}

	OnCheckpointActivated(CheckpointId);
	return true;
}

void ADeadHospitalCheckpoint::SetCheckpointEnabled(bool ShouldEnable)
{
	CheckpointEnabled = ShouldEnable && (!OneActivationOnly || !HasActivated);
}

void ADeadHospitalCheckpoint::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool FromSweep,
	const FHitResult& SweepResult)
{
	// Unreal의 Overlap 이벤트는 여러 정보를 전달하지만 여기서는 OtherActor(들어온 대상)가
	// Player Pawn인지가 핵심입니다. AI나 물건이 닿아도 저장하지 않습니다.
	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	ActivateCheckpoint(PlayerPawn);
}
