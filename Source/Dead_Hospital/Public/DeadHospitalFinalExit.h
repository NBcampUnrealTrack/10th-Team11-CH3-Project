// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalFinalExit.generated.h"

class UBoxComponent;

/**
 * 병원 최종 탈출문 뒤에 배치하여 게임 클리어를 판정하는 Actor입니다.
 *
 * 실제 문의 잠금 해제와 열림 애니메이션은 문 블루프린트가 담당하고,
 * 플레이어가 문 뒤의 TriggerBox까지 통과하면 이 Actor가 GameMode에 최종 탈출을 요청합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalFinalExit : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalFinalExit();

	/**
	 * 최종 탈출을 시도합니다.
	 * 문 블루프린트가 상호작용 성공 순간 직접 호출할 수도 있으며, 성공한 경우 true를 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Final Exit")
	bool TryUseFinalExit();

	/** 실제 엔딩 Level Sequence가 끝난 순간 호출하여 점수와 등급을 확정합니다. */
	UFUNCTION(BlueprintCallable, Category = "Final Exit")
	void CompleteEnding();

	/**
	 * 최종 탈출 조건을 통과한 순간 블루프린트에서 실행되는 이벤트입니다.
	 * 카메라 전환, 플레이어 입력 정지, 엔딩 Level Sequence 재생을 이곳에 연결할 수 있습니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Final Exit")
	void OnEndingRequested();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 최종 탈출문을 통과했는지 검사하는 충돌 영역입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Final Exit")
	UBoxComponent* TriggerBox;

	/** true이면 플레이어가 TriggerBox에 들어왔을 때 자동으로 최종 탈출을 시도합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Exit")
	bool TriggerOnPlayerOverlap = true;

	/**
	 * true이면 임시 엔딩 대기 시간이 끝난 뒤 자동으로 결과를 확정합니다.
	 * 실제 엔딩 Level Sequence를 연결할 때는 false로 바꾸고 CompleteEnding을 직접 호출해야 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Exit")
	bool AutomaticallyCompleteEnding = true;

	/** 자동 완료를 사용할 때 엔딩 연출을 기다리는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Exit", meta = (ClampMin = "0.0"))
	float AutomaticEndingDelaySeconds = 3.0f;

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

	/** 중복 엔딩 실행을 막기 위해 탈출 성공 여부를 기억합니다. */
	bool ExitSucceeded = false;

	/** 임시 엔딩 대기 후 CompleteEnding을 호출하는 타이머입니다. */
	FTimerHandle EndingTimerHandle;
};
