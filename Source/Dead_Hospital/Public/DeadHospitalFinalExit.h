// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalFinalExit.generated.h"

class UBoxComponent;

/**
 * 병원 출구에서 마지막으로 E 키를 눌렀을 때 "정말 탈출했는가"를 판정합니다.
 * 생명유지장치 종료, Escape 단계, 제한시간이 남았는지를 GameMode에서 확인합니다.
 * 성공하면 Ending 단계로 바꾸고 Blueprint에 엔딩 연출 시작을 요청합니다.
 * 연출이 끝나야 CompleteEnding으로 Cleared/결과 데이터를 확정합니다.
 * 문짝 Mesh/애니메이션은 이 Actor가 제공하지 않으므로 실제 출구 외형과
 * 상호작용 가능한 이 Actor를 L_MainLevel에서 같은 위치에 연결해 배치합니다.
 * 기본은 E 상호작용이고, 통과 자동 판정은 TriggerOnPlayerOverlap을 켰을 때만 사용합니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalFinalExit : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalFinalExit();

	/**
	 * 최종 탈출을 시도합니다.
	 * GameMode에 Ending 전환을 요청합니다. true는 조건 통과, false는 아직 탈출 불가입니다.
	 * 보통은 Player의 Interface Interact가 호출하고 이 함수는 내부에서 사용됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Final Exit")
	bool TryUseFinalExit();

	/**
	 * 엔딩 Level Sequence의 Finished 출력에서 호출합니다. '출구를 열었다'와
	 * '엔딩을 끝냈다'를 구분해 결과 화면이 연출보다 먼저 나타나지 않게 합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Final Exit")
	void CompleteEnding();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

	/**
	 * 최종 탈출 조건을 통과한 순간 블루프린트에서 실행되는 이벤트입니다.
	 * 카메라 전환, 플레이어 입력 정지, 엔딩 Level Sequence 재생을 이곳에 연결할 수 있습니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Final Exit")
	void OnEndingRequested();

	/** Escape 상태가 아니거나 이미 사용한 출구를 다시 조사했을 때 UI 피드백에 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Final Exit")
	void OnExitUseDenied();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Overlap 자동 방식을 선택했을 때 사용하는 상자 범위입니다. E 방식에서는 기본적으로 자동 발동하지 않습니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Final Exit")
	UBoxComponent* TriggerBox;

	/**
	 * true이면 Player가 TriggerBox에 들어왔을 때 자동으로 탈출을 시도합니다.
	 * 기획서의 "최종 탈출문 E 상호작용"을 사용하려면 기본값 false를 그대로 둡니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Exit")
	bool TriggerOnPlayerOverlap = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/**
	 * true이면 임시 엔딩 대기 시간이 끝난 뒤 자동으로 결과를 확정합니다.
	 * 아직 연출이 없는 개발 테스트에서는 잠시 기다렸다 자동으로 결과를 확정합니다.
	 * 실제 엔딩 Sequence가 생기면 false로 바꾸고 Finished에서 CompleteEnding을 연결합니다.
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

	/** TimerHandle은 '나중에 실행하기로 예약한 작업'을 구분할 손잡이입니다. EndPlay에서 취소합니다. */
	FTimerHandle EndingTimerHandle;
};
