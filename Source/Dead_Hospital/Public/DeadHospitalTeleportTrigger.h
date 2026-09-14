// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalTeleportTrigger.generated.h"

class AActor;
class APawn;
class UBoxComponent;

/** 같은 L_MainLevel 안에서 사용하는 순간이동의 목적을 구분합니다. */
UENUM(BlueprintType)
enum class EDeadHospitalTeleportPurpose : uint8
{
	RegularTransition,			// 병원 구역 사이를 이동하며 게임 단계는 바꾸지 않음
	EnterFinalObjective,			// 생명유지장치 구역으로 이동한 뒤 마지막 목표를 시작함
	ReturnToHospitalAndStartEscape	// 지하 2층으로 복귀한 뒤 조작을 돌려주고 붕괴 카운트다운을 시작함
};

/**
 * 한 개의 L_MainLevel 안에서 서로 떨어진 병원 구역을 연결하는 순간이동 트리거입니다.
 * 화면을 암전한 뒤 DestinationActor 위치로 이동하므로 실제 레벨을 바꾸지 않고 구역 전환을 연출합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalTeleportTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalTeleportTrigger();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void ActivateTeleporter();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void DeactivateTeleporter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 플레이어 진입을 감지하는 충돌 영역입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport")
	UBoxComponent* TriggerBox;

	/** 도착 위치와 방향을 제공할 Target Point 또는 다른 Actor입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	AActor* DestinationActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	EDeadHospitalTeleportPurpose TeleportPurpose = EDeadHospitalTeleportPurpose::RegularTransition;

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

	/** 병원 복귀용일 때 사용할 탈출 시간입니다. 0 이하면 GameMode 기본값을 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "-1"))
	int32 EscapeDurationSeconds = -1;

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

	bool IsActivated = true;
	bool HasBeenUsed = false;
	TWeakObjectPtr<APawn> PendingPlayerPawn;
	FTimerHandle TeleportTimerHandle;
};
