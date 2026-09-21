// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeadHospitalGameMode.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalTeleportTrigger.generated.h"

class AActor;
class APawn;
class UBoxComponent;

/**
 * enum은 여러 선택지 중 하나를 고르는 타입입니다. 이동은 모두 L_MainLevel 안의
 * 위치 이동이며, 아래 목적에 따라 GameMode 단계 변경 여부만 달라집니다.
 */
UENUM(BlueprintType)
enum class EDeadHospitalTeleportPurpose : uint8
{
	RegularTransition,			// 병원 구역 사이를 이동하며 게임 단계는 바꾸지 않음
	EnterFinalObjective,			// 예전 기획의 값. 현재는 ObjectiveTrigger로 특수중환자격리실 진입을 처리함
	ReturnToHospitalAndStartEscape	// 예전 기획의 값. Blueprint 호환만 유지하며 현재는 안전하게 사용 거부함
};

/**
 * Player가 보이지 않는 Box에 들어오면 화면 암전 -> 목적지 이동 -> 화면 복귀를 합니다.
 * L_MainLevel 자체를 새로 열지 않으므로 퍼즐/킬 수/플레이 시간 기록을 유지합니다.
 *
 * 이 Actor와 도착 위치를 표시할 TargetPoint를 L_MainLevel에 각각 놓고
 * DestinationActor를 연결해야 합니다. OneUseOnly인 이동은 TeleportEventId가
 * 있어야 체크포인트 복구 시 이미 지나간 통로인지 알 수 있습니다.
 * 화면 Fade는 C++ 카메라 기능, 통로 소리/문 연출은 Blueprint 이벤트에 연결합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalTeleportTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalTeleportTrigger();

	/** 진행 연출이 끝나 Teleport를 사용할 수 있게 만들 때 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void ActivateTeleporter();

	/** 아직 이동시키면 안 되는 동안 Teleport 진입을 막을 때 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void DeactivateTeleporter();

	/** 멀리 떨어진 목적지를 스트리밍한다면 로딩 전 false, 바닥/Actor 로딩 뒤 true를 전달합니다. */
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void SetDestinationReady(bool IsReady);

	/** 암전 또는 실제 위치 이동이 진행 중인지 Blueprint가 확인할 때 사용합니다. */
	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsTeleportInProgress() const { return TeleportInProgress; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** BoxComponent는 보이지 않는 상자 범위입니다. Player Pawn이 범위에 들어오면 Overlap을 받습니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport")
	UBoxComponent* TriggerBox;

	/**
	 * EditInstanceOnly는 실제 맵에 놓은 이 Trigger마다 다른 목적지를 지정한다는 뜻입니다.
	 * nullptr이면 아직 연결되지 않아 이동을 시작하지 않습니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	AActor* DestinationActor = nullptr;

	/**
	 * 체크포인트 복구와 중복 실행 방지에 사용하는 고유 ID입니다.
	 * OneUseOnly=true이면 예: Teleport_B2_To_B1처럼 다른 Trigger와 다른 이름을 넣습니다.
	 * 이 ID의 완료 기록이 체크포인트에 들어갑니다. OneUseOnly=false인 왕복 통로는
	 * 매번 사용할 수 있지만 체크포인트의 "사용 완료" 표시는 적용하지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	FName TeleportEventId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	EDeadHospitalTeleportPurpose TeleportPurpose = EDeadHospitalTeleportPurpose::RegularTransition;

	/**
	 * 이동이 성공한 뒤 GameMode에 기록할 도착 구역 이름입니다. 예: Ward_1F, FinalObjectiveArea.
	 * 비워 두면 위치만 이동하고 현재 구역 ID는 바꾸지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	FName DestinationAreaId = NAME_None;

	/**
	 * 기본 true는 가까운 곳에 준비된 바닥/Actor가 이미 있는 경우입니다.
	 * World Partition 먼 지역이면 로딩이 시작되기 전에 false로 바꾸고
	 * 실제 준비 완료를 확인한 시점에 SetDestinationReady(true)를 호출해야 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport|Condition")
	bool DestinationReady = true;

	/**
	 * 같은 통로를 탐색 때와 탈출 때 다르게 사용해야 하면 true로 설정합니다.
	 * 예를 들어 탈출 전용 순간이동은 RequiredGamePhase를 Escape로 지정합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport|Condition")
	bool RequireSpecificGamePhase = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport|Condition", meta = (EditCondition = "RequireSpecificGamePhase"))
	EDeadHospitalGamePhase RequiredGamePhase = EDeadHospitalGamePhase::Playing;

	/** TArray는 여러 퍼즐 이름의 목록입니다. 목록이 비면 조건이 없고, 채우면 전부 완료여야 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport|Condition")
	TArray<FName> RequiredPuzzleIds;

	/** 이 목록의 일회성 Event가 모두 완료된 뒤에만 Teleport를 허용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport|Condition")
	TArray<FName> RequiredCompletedEventIds;

	/** Fade Out과 Fade In에 각각 사용하는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float FadeDurationSeconds = 0.5f;

	/** 도착 지점 바닥에 끼이지 않도록 목적지 위치에 더하는 높이입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	FVector DestinationOffset = FVector(0.0f, 0.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	bool OneUseOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	bool StartsActivated = true;

	/** 예전 복귀 Teleport의 탈출 시간 설정입니다. 에셋 호환용으로 남기지만 현재 흐름에서는 사용하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "-1"))
	int32 EscapeDurationSeconds = -1;

	/** 암전과 입력 잠금이 시작된 순간 Blueprint 연출에 알립니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport")
	void OnTeleportStarted(APawn* PlayerPawn);

	/** 위치 이동, 진행 단계 변경, 입력 복구가 모두 성공한 뒤 한 번 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport")
	void OnTeleportCompleted(APawn* PlayerPawn);

	/** 설정 오류, 충돌, 목적지 미준비 등으로 이동을 취소했을 때 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport")
	void OnTeleportFailed();

	/** 체크포인트 복구 뒤 Blueprint 외형과 효과를 사용 여부에 맞출 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport")
	void OnTeleportStateRestored(bool WasAlreadyUsed);

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool FromSweep,
		const FHitResult& SweepResult
	);

	/** 암전이 완료된 뒤 실제 이동과 게임 단계 전환을 처리합니다. */
	void PerformTeleport();

	/** 현재 게임 단계에서 선택된 목적의 순간이동을 시작해도 되는지 검사합니다. */
	bool CanBeginTeleport() const;
	bool AreProgressRequirementsMet(const ADeadHospitalGameMode* GameMode) const;
	bool IsPurposeAllowedInCurrentPhase(const ADeadHospitalGameMode* GameMode) const;
	bool ApplyTeleportPurpose(ADeadHospitalGameMode* GameMode);
	void CancelPendingTeleport(bool ShouldRestorePlayerInput);
	void SetPendingPlayerInputEnabled(bool ShouldEnableInput);
	void StartFadeIn();

	UFUNCTION()
	void HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase);

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	/** StartsActivated는 에디터 시작 설정, IsActivated는 플레이 중 현재 상태입니다. */
	bool IsActivated = true;
	bool HasBeenUsed = false;
	bool TeleportInProgress = false;
	bool PlayerInputWasLocked = false;
	bool ApplyingPurposeTransition = false;
	TWeakObjectPtr<APawn> PendingPlayerPawn;
	/** 출발 위치+회전입니다. 도착 뒤 단계 변경 실패 시 Player를 되돌릴 때 사용합니다. */
	FTransform PendingSourceTransform;
	FTimerHandle TeleportTimerHandle;
};
