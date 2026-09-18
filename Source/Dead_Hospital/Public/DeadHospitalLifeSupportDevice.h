// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalLifeSupportDevice.generated.h"

class APawn;
class UStaticMeshComponent;
enum class EDeadHospitalGamePhase : uint8;

/**
 * 기획서의 최종 핵심 사건은 보스 처치가 아니라 생명유지장치 종료입니다.
 * 이 Actor는 E 상호작용, 연출이 끝났다는 알림, 목적지 이동, 탈출 타이머 시작을
 * 하나의 순서로 묶습니다. 순서를 바꾸면 장치가 꺼지기도 전에 제한시간이
 * 흐르거나 암전 중 Player가 움직이는 문제가 생기므로 단계별로 검사합니다.
 *
 * 반드시 다음 순서로만 진행됩니다.
 * E 상호작용 → 장치 종료 승인 → Player 입력 잠금 → 육신 사망/귀신 성불 연출
 * → 화면 암전 → 지하 2층 이동 → Player 입력 복구 → Escape Timer 시작
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalLifeSupportDevice : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalLifeSupportDevice();

	/** 전달할 Player가 없을 때 첫 로컬 Player Pawn을 찾아 아래 함수로 넘기는 편의 함수입니다. */
	UFUNCTION(BlueprintCallable, Category = "Life Support")
	bool TryShutdownDevice();

	/**
	 * E 키를 누른 Player Actor를 검사하고 GameMode에 장치 종료를 요청합니다.
	 * 성공이면 입력을 잠근 뒤 Blueprint에 육신 사망/귀신 성불 연출을 요청합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Life Support")
	bool TryShutdownDeviceForInteractor(AActor* Interactor);

	/** 귀신 성불 Sequence가 끝났을 때 호출합니다. 그때 암전하고 지하 2층으로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "Life Support")
	bool CompleteAscensionSequence();

	/**
	 * 목적지가 멀거나 스트리밍 대상이면 로딩 시작 때 false, 바닥/연출/착지 공간
	 * 준비 완료 때 true로 설정합니다. 기본 true는 가까이 배치된 준비된 공간용입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Life Support")
	void SetEscapeDestinationReady(bool IsReady);

	UFUNCTION(BlueprintPure, Category = "Life Support")
	bool IsDeviceShutdown() const { return DeviceShutdown; }

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life Support")
	UStaticMeshComponent* DeviceMesh;

	/**
	 * L_MainLevel의 지하 2층 안전한 바닥에 TargetPoint를 놓고 Details에서 연결합니다.
	 * nullptr이면 갈 목적지가 없어 장치 E 입력을 거부합니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Life Support|Teleport")
	AActor* EscapeDestinationActor = nullptr;

	/** 장치 종료 후 돌아오는 병원 구역 ID입니다. 체크포인트의 현재 구역 저장에 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Teleport")
	FName EscapeDestinationAreaId = TEXT("Hospital_B2");

	/** FVector(X,Y,Z)만큼 TargetPoint 위치에 더합니다. 기본 Z=20은 바닥 겹침을 줄입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Teleport")
	FVector DestinationOffset = FVector(0.0f, 0.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Teleport")
	bool EscapeDestinationReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Teleport", meta = (ClampMin = "0.0"))
	float FadeDurationSeconds = 0.5f;

	/** 0 이하이면 GameMode 기본 제한시간을 사용하고, 양수면 별도 초 단위 제한시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Teleport", meta = (ClampMin = "-1"))
	int32 EscapeDurationSeconds = -1;

	/** true는 개발 중 임시 타이머로 성불 완료를 흉내 냅니다. 실제 Sequence를 연결하면 false로 둡니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Sequence")
	bool AutomaticallyCompleteAscensionSequence = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Life Support|Sequence", meta = (EditCondition = "AutomaticallyCompleteAscensionSequence", ClampMin = "0.0"))
	float AutomaticSequenceDelaySeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/** 장치 정지, 육신 사망, 귀신 성불 Level Sequence를 시작하는 Blueprint 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnDeviceShutdownAccepted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnDeviceShutdownRejected();

	/** 순간이동과 조작 복구를 끝내고 Escape Timer를 시작한 직후 실행됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnEscapeTeleportCompleted();

	/** 목적지 충돌이나 설정 오류로 이동하지 못했을 때 개발자가 확인할 수 있는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnEscapeTeleportFailed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnLifeSupportStateRestored(bool WasShutdown);

private:
	void PerformEscapeTeleport();
	void SetStoredPlayerInputEnabled(bool ShouldEnableInput);
	void StartAutomaticSequenceCompletion();
	/** Unreal Timer에서 bool 함수를 직접 호출하지 않도록 사용하는 반환값 없는 중간 함수입니다. */
	void HandleAutomaticSequenceCompletion();
	void RestoreAfterTeleportFailure();

	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	/** 연출 도중 GameOver가 되면 늦게 실행되는 Timer와 Teleport를 안전하게 취소합니다. */
	UFUNCTION()
	void HandleGamePhaseChanged(EDeadHospitalGamePhase NewGamePhase);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Life Support", meta = (AllowPrivateAccess = "true"))
	bool DeviceShutdown = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Life Support", meta = (AllowPrivateAccess = "true"))
	bool SequenceCompletionStarted = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Life Support", meta = (AllowPrivateAccess = "true"))
	bool EscapeSequenceFinished = false;

	/**
	 * 장치를 E로 사용한 Player를 연출이 끝날 때까지 기억합니다. Weak 포인터는
	 * 사망 등으로 Pawn이 사라졌을 때 IsValid/IsValid()로 확인할 수 있는 참조입니다.
	 */
	TWeakObjectPtr<APawn> StoredPlayerPawn;
	FTimerHandle SequenceTimerHandle;
	FTimerHandle TeleportTimerHandle;
};
