// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalMagnumPuzzle.h"

#include "DeadHospitalDoor.h"
#include "DeadHospitalProgressionItem.h"

ADeadHospitalMagnumPuzzle::ADeadHospitalMagnumPuzzle()
{
	// PZ-03 세부 정답이 바뀌어도 저장과 보상 연결에서 사용하는 ID는 그대로 유지합니다.
	PuzzleId = TEXT("PZ_03_Magnum");
	InteractionText = FText::FromString(TEXT("E 키로 매그넘 보관함 퍼즐 조사"));
}

bool ADeadHospitalMagnumPuzzle::TryCompletePuzzle(AActor* Interactor)
{
	// Super는 부모 ADeadHospitalPuzzleBase의 함수를 뜻합니다.
	// 먼저 공통 코드가 정답 조건 검사, 완료 기록, 보관함 문 잠금 해제를 모두 성공해야 합니다.
	if (!Super::TryCompletePuzzle(Interactor))
	{
		return false;
	}

	// 인벤토리가 가득 찬 경우 AddItem()이 실패할 수 있습니다. 퍼즐 완료 자체는 이미 저장됐으므로
	// false라고 거짓 보고하지 않고, 매그넘 Actor를 맵에 남겨 공간을 만든 뒤 다시 주울 수 있게 합니다.
	if (!IsValid(MagnumPickup) || !MagnumPickup->HasBeenCollected())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PZ-03 completed, but Magnum could not be added to Inventory. The pickup remains available."), *GetName());
	}

	return true;
}

void ADeadHospitalMagnumPuzzle::BeginPlay()
{
	// BeginPlay는 게임에서 Actor가 등장해 플레이를 시작할 때 실행됩니다.
	// Super::BeginPlay()는 부모 퍼즐의 초기 검사/상태 연결을 먼저 실행한다는 뜻입니다.
	Super::BeginPlay();

	// Blueprint나 기존 맵 Actor에 다른 값이 저장되어 있어도 실제 플레이에서는
	// DT_ItemData와 팀원 Inventory가 사용하는 최종 ID "Magnum"으로 통일합니다.
	MagnumItemId = TEXT("Magnum");

	if (!IsValid(MagnumPickup))
	{
		// 보상 Actor가 없는데 퍼즐만 완료되면 핵심 무기를 영원히 얻을 수 없습니다.
		// 게임을 조용히 진행시키지 않고 설정 누락을 Output Log에 분명하게 남깁니다.
		UE_LOG(LogTemp, Error, TEXT("%s: MagnumPickup is not assigned. PZ-03 reward cannot be obtained."), *GetName());
		return;
	}

	// 보관함 퍼즐과 아이템은 별개의 Actor입니다. 이 함수가 두 Actor의
	// 퍼즐 ID/아이템 ID/Event ID를 맞춰 주어 한쪽만 잘못 설정되는 실수를 줄입니다.
	// ProgressionItem 쪽에 같은 ID들을 자동으로 전달합니다.
	// 디자이너가 StartsEnabled나 RequiredPuzzleId를 잘못 입력해도 퍼즐 전에는 매그넘이 나타나지 않습니다.
	MagnumPickup->ConfigureAsPuzzleReward(
		PuzzleId,
		MagnumItemId,
		EItemType::Weapon,
		MagnumPickupEventId
	);
}

bool ADeadHospitalMagnumPuzzle::CanCompletePuzzle(AActor* Interactor) const
{
	// &&는 모든 조건이 true일 때만 true입니다. IsValid는 참조한 Actor가 실제로
	// 존재하는지 검사합니다. 부모 검사도 통과해야 매그넘 퍼즐이 완료됩니다.
	// 보관함 문과 매그넘 보상 중 하나라도 빠져 있으면 완료 기록을 남기지 않습니다.
	// 에디터 설정을 고친 뒤 다시 입력할 수 있으므로 진행 불가 상태가 저장되는 것을 피할 수 있습니다.
	return IsValid(ConnectedDoor)
		&& IsValid(MagnumPickup)
		&& Super::CanCompletePuzzle(Interactor);
}
