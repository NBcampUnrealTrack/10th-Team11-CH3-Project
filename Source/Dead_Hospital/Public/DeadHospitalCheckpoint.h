// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalCheckpoint.generated.h"

class APawn;
class UBoxComponent;
class USceneComponent;

/**
 * 체크포인트는 "사망했을 때 여기서 다시 시작"할 지점입니다.
 * Player가 TriggerBox 범위에 들어오면 GameMode가 그 순간의 진행 단계,
 * 퍼즐/일회성 연출, 플레이 시간, 인벤토리와 RespawnPoint의 위치를 기억합니다.
 * GameOver 화면의 재시작은 이 저장 내용을 이용합니다. 디스크 파일 저장과는 다르게
 * 현재 실행 중인 게임의 마지막 체크포인트 한 개를 보관하는 구조입니다.
 * L_MainLevel에서 안전한 바닥에 범위를 놓고, RespawnPoint가 벽과 겹치지 않는지 확인합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalCheckpoint();

	/**
	 * 보통 범위에 들어오면 자동 호출됩니다. 다른 연출에서 직접 호출할 때는 Player Pawn을
	 * 전달합니다. true는 새 체크포인트 저장 성공, false는 저장 불가를 뜻합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool ActivateCheckpoint(APawn* PlayerPawn);

	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	void SetCheckpointEnabled(bool ShouldEnable);

	// bool은 true/false 두 값만 갖는 타입입니다. BlueprintPure는 상태 조회 함수입니다.

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	bool HasBeenActivated() const { return HasActivated; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 보이지 않는 상자 충돌 범위입니다. Overlap은 다른 Actor가 범위에 들어올 때 발생합니다.
	 * 벽처럼 막는 충돌이 아니라 Pawn만 겹침 이벤트를 받도록 설정합니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	UBoxComponent* TriggerBox;

	/** 재시작한 Player가 실제로 생성될 위치와 방향입니다. 바닥보다 조금 위에 두어야 합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	USceneComponent* RespawnPoint;

	/** 지점마다 다른 이름(예: CP_B2_Start). None이면 어느 지점인지 저장할 수 없습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool StartsEnabled = true;

	/** true면 한 번 밟은 지점은 다시 저장하지 않습니다. 체크포인트를 불러오면 저장 시점 상태로 돌아갑니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool OneActivationOnly = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnCheckpointActivated(FName ActivatedCheckpointId);

	// 아래 Event는 C++의 저장 판정을 바꾸지 않고 Blueprint에서 메시지/소리를 넣는 자리입니다.

	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnCheckpointActivationFailed();

private:
	UFUNCTION()
	void HandleCheckpointRestored(FName RestoredCheckpointId);

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool FromSweep,
		const FHitResult& SweepResult
	);

	bool CheckpointEnabled = true;

	/** 에디터 설정 StartsEnabled와 별개인 현재 게임 중 활성 여부입니다. */
	bool HasActivated = false;
};
