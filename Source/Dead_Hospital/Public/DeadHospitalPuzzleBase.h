// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DeadHospitalPuzzleBase.generated.h"

class ADeadHospitalDoor;
class UStaticMeshComponent;

/**
 * "부모 클래스"는 자식 퍼즐들이 함께 쓰는 기능을 한곳에 모아 놓은 코드입니다.
 * 키패드와 매그넘 퍼즐은 정답 방식이 다르지만, 둘 다 E 조사, 중복 완료 방지,
 * GameMode에 완료 저장, 체크포인트 복구가 필요합니다. 이 반복 작업을 여기서 처리합니다.
 *
 * 이 Actor는 IInteractable을 구현하므로 Player의 E 상호작용 대상이 될 수 있습니다.
 * E 조사만으로 정답이 되지는 않습니다. 자식 퍼즐 또는 UI가 정답을 확인한 후
 * TryCompletePuzzle을 호출해야 완료됩니다. L_MainLevel에 배치할 때 퍼즐마다
 * 고유 PuzzleId와 필요한 문 ConnectedDoor를 Details에서 설정해야 합니다.
 */
UCLASS(Blueprintable)
class DEAD_HOSPITAL_API ADeadHospitalPuzzleBase : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeadHospitalPuzzleBase();

	/**
	 * 정답 확인 -> 중복/게임 단계/문 연결 검사 -> GameMode 저장 -> 문 해제 순서입니다.
	 * return true는 GameMode가 퍼즐 완료 기록을 승인했다는 뜻입니다.
	 * 이후 연결 문 잠금 해제에 실패해도 오류 로그를 남기고 true를 반환할 수 있으므로
	 * 문 연결 오류 여부는 Output Log에서도 확인해야 합니다. false는 사용 불가입니다.
	 * 부모는 정답을 스스로 검사하지 않으니 이 함수를 아무 입력에서나 호출하면 안 됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	virtual bool TryCompletePuzzle(AActor* Interactor);

	/**
	 * 아직 UI/정답 방식이 없는 개발용 맵에서 이후 동선이 막히지 않는지 검사합니다.
	 * Shipping(최종 배포) 빌드에서는 항상 false여서 출시 게임의 우회 버튼이 아닙니다.
	 * 개발 빌드에서는 정답 확인만 생략하고 일반 완료 검사는 그대로 적용됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Debug")
	bool CompletePuzzleForTesting(AActor* Interactor);

	/** true면 사용 가능, false면 잠시 사용 불가. 이미 완료한 퍼즐은 true로도 되살리지 않습니다. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SetPuzzleEnabled(bool ShouldEnable);

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsPuzzleSolved() const { return PuzzleSolved; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	FName GetPuzzleId() const { return PuzzleId; }

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Mesh는 화면에 보이는 모양입니다. C++ 상태와 Blueprint/에디터 외형을 분리합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	UStaticMeshComponent* PuzzleMesh;

	/**
	 * FName은 퍼즐을 구분할 짧은 이름입니다. GameMode는 이 이름을 완료 목록에 기록합니다.
	 * PuzzleId가 None이면 기록할 이름이 없어 완료하지 못합니다. 서로 다른 퍼즐에
	 * 같은 이름을 쓰면 둘의 체크포인트 상태가 섞이므로 반드시 각각 다르게 지정합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FName PuzzleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	bool StartsEnabled = true;

	// UPROPERTY는 아래 값을 Unreal Editor의 Details/Blueprint에 노출하고 저장하는 표기입니다.
	// EditInstanceOnly인 참조는 L_MainLevel에 실제 Actor를 놓고 그 옆 Actor를 지정합니다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/**
	 * 실제 맵에 놓인 Door Actor를 가리킵니다. nullptr이면 연결되지 않은 상태입니다.
	 * 공통 부모는 문이 없어도 퍼즐 자체는 기록할 수 있지만 PZ01/PZ02와 배치 퍼즐 자식은
	 * 문이 필수라서 연결하지 않으면 성공을 막습니다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle")
	ADeadHospitalDoor* ConnectedDoor = nullptr;

	/** 퍼즐 성공 후 다음 Objective가 확정된 경우에만 값을 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName NextObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText NextObjectiveText;

	/**
	 * BlueprintImplementableEvent는 C++가 "이제 화면을 열어 주세요"라고 알리는 자리입니다.
	 * 이 함수를 구현하지 않으면 E를 눌러도 UI가 저절로 생기지 않습니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleInteractionRequested(AActor* Interactor);

	/** 완료가 승인된 정확한 순간 소리, 빛, 애니메이션을 한 번 재생하는 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleSolved(AActor* Interactor);

	/** 체크포인트 복구 후 Mesh와 연출 상태를 맞추는 Blueprint 연결 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleStateRestored(bool IsSolved);

	/** virtual은 자식이 같은 이름의 함수를 바꾸어 추가 조건을 넣을 수 있다는 뜻입니다. */
	virtual bool CanCompletePuzzle(AActor* Interactor) const;

private:
	UFUNCTION()
	void HandleCheckpointRestored(FName CheckpointId);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Puzzle", meta = (AllowPrivateAccess = "true"))
	bool PuzzleEnabled = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Puzzle", meta = (AllowPrivateAccess = "true"))
	bool PuzzleSolved = false;
};
