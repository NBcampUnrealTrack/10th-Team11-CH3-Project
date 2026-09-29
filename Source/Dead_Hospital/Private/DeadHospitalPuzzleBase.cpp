// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalPuzzleBase.h"

#include "DeadHospitalDoor.h"
#include "DeadHospitalGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ADeadHospitalPuzzleBase::ADeadHospitalPuzzleBase()
{
	// Actor가 처음 생성될 때 한 번 기본 부품을 만듭니다. Tick을 끄면 매 프레임
	// 불필요하게 검사하지 않아도 되며, 퍼즐은 E 입력/이벤트가 있을 때만 작동합니다.
	PrimaryActorTick.bCanEverTick = false;

	PuzzleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuzzleMesh"));
	// RootComponent는 Actor의 위치 기준입니다. Mesh를 Root로 쓰면 Actor 이동과
	// 외형 이동이 함께 이루어져 배치할 때 기준이 분명합니다.
	SetRootComponent(PuzzleMesh);

	InteractionText = FText::FromString(TEXT("E 키로 조사"));
}

void ADeadHospitalPuzzleBase::BeginPlay()
{
	// BeginPlay는 에디터에 놓인 Actor가 실제 게임에서 시작할 때 호출됩니다.
	// Super::BeginPlay()는 부모 AActor의 시작 작업을 먼저 수행한다는 뜻입니다.
	Super::BeginPlay();

	if (PuzzleId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PuzzleId is None. This puzzle cannot save or complete."), *GetName());
	}

	// 에디터에서 설정한 시작 상태를 실제 플레이 상태에 복사합니다.
	PuzzleEnabled = StartsEnabled;

	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		// 체크포인트 상태가 먼저 복구되어 있다면 Actor도 그 결과에 맞게 시작합니다.
		PuzzleSolved = GameMode->IsPuzzleCompleted(PuzzleId);
		PuzzleEnabled = PuzzleEnabled && !PuzzleSolved;
		// AddDynamic은 GameMode의 "체크포인트를 불러왔다" 알림에 이 Actor의 함수를
		// 연결합니다. 불러오기가 발생할 때마다 HandleCheckpointRestored가 실행됩니다.
		GameMode->OnCheckpointRestored.AddDynamic(this, &ADeadHospitalPuzzleBase::HandleCheckpointRestored);
	}

	OnPuzzleStateRestored(PuzzleSolved);
}

void ADeadHospitalPuzzleBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay는 Actor/레벨이 끝나거나 제거될 때 호출됩니다. 시작 때 연결한
	// 알림을 여기서 해제해야 이미 사라진 Actor에 알림이 전달되지 않습니다.
	if (ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>())
	{
		GameMode->OnCheckpointRestored.RemoveDynamic(this, &ADeadHospitalPuzzleBase::HandleCheckpointRestored);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeadHospitalPuzzleBase::Interact_Implementation(AActor* Interactor)
{
	// 이 함수는 Player의 E 키 호출을 받아 UI 열기만 요청합니다.
	// _Implementation은 Interface에 선언된 Interact의 실제 C++ 구현입니다.
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	// 부모 클래스는 정답을 임의로 맞혔다고 처리하지 않습니다.
	// UI 또는 자식 퍼즐이 입력을 받은 뒤 실제 정답을 확인해야 합니다.
	OnPuzzleInteractionRequested(Interactor);
}

bool ADeadHospitalPuzzleBase::CanInteract_Implementation(AActor* Interactor) const
{
	// ||는 어느 하나라도 문제가 있으면 실패한다는 뜻입니다.
	// 사용 불가 상태, 이미 완료, 유효하지 않은 상호작용 Actor, 빈 ID를 한 번에 검사합니다.
	// 이 공통 함수만으로 실제 PlayerControlled Pawn인지 확인하지는 않습니다.
	if (!PuzzleEnabled || PuzzleSolved || !IsValid(Interactor) || PuzzleId.IsNone())
	{
		return false;
	}

	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	// &&는 두 조건이 모두 true여야 합니다. 탐색 중(Playing)에만 퍼즐을 사용합니다.
	return IsValid(GameMode) && GameMode->GetCurrentGamePhase() == EDeadHospitalGamePhase::Playing;
}

FText ADeadHospitalPuzzleBase::GetInteractionText_Implementation() const
{
	return InteractionText;
}

bool ADeadHospitalPuzzleBase::TryCompletePuzzle(AActor* Interactor)
{
	// 먼저 E 사용 가능 여부와 자식 퍼즐의 추가 조건을 확인합니다.
	// 여기서 실패하면 GameMode 완료 목록이나 Door 상태는 아직 건드리지 않습니다.
	if (!CanInteract_Implementation(Interactor) || !CanCompletePuzzle(Interactor))
	{
		return false;
	}

	// 문 연결이 있는 퍼즐은 GameMode에 완료를 기록하기 전에 Door 설정이 올바른지 먼저 확인합니다.
	// 이렇게 해야 잘못된 Door 또는 PuzzleId를 연결했을 때 "퍼즐은 완료됐지만 문은 영원히 잠긴" 상태가 되지 않습니다.
	if (IsValid(ConnectedDoor) && !ConnectedDoor->CanUnlockFromPuzzle(PuzzleId))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ConnectedDoor RequiredPuzzleId does not match %s."), *GetName(), *PuzzleId.ToString());
		return false;
	}

	ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode) || !GameMode->CompletePuzzle(PuzzleId))
	{
		return false;
	}

	// GameMode 기록이 성공한 뒤 Actor 상태를 바꿔야 저장 상태와 눈에 보이는 상태가 어긋나지 않습니다.
	PuzzleSolved = true;
	PuzzleEnabled = false;
	// PuzzleSolved는 실제 맵 Actor의 현재 상태입니다. GameMode의 완료 목록이
	// 저장/복구 기준이고, 이 값은 화면/상호작용이 그 목록과 맞게 보이도록 합니다.

	if (IsValid(ConnectedDoor) && ConnectedDoor->GetIsLocked())
	{
		if (!ConnectedDoor->UnlockFromPuzzle(PuzzleId))
		{
			// 단일 플레이 게임에서 사전 검사를 통과한 뒤 실패할 가능성은 매우 낮지만,
			// 연결 오류를 조용히 숨기지 않도록 명확한 로그를 남깁니다.
			UE_LOG(LogTemp, Error, TEXT("%s: Puzzle completed, but ConnectedDoor could not unlock."), *GetName());
		}
	}

	if (!NextObjectiveId.IsNone() && !NextObjectiveText.IsEmpty())
	{
		// 퍼즐의 다음 목표는 최신 GDD의 M00~M09 메인 목표 칸에 표시합니다.
		// 서브 목표는 별도 ObjectiveTrigger/수집 Actor가 관리하므로 여기서 지우거나 덮어쓰지 않습니다.
		GameMode->SetMainObjective(NextObjectiveId, NextObjectiveText);
	}

	OnPuzzleSolved(Interactor);
	return true;
}

bool ADeadHospitalPuzzleBase::CompletePuzzleForTesting(AActor* Interactor)
{
#if UE_BUILD_SHIPPING
	// 제출용 Shipping 빌드에서는 치트처럼 사용될 수 있는 강제 해결을 완전히 차단합니다.
	return false;
#else
	// 개발 빌드에서도 활성 상태, 유효한 상호작용 Actor, 문 연결, 중복 완료 검사는
	// 기존 공통 함수를 그대로 사용합니다. Player 소유 여부는 이 부모 함수에서 검사하지 않습니다.
	return TryCompletePuzzle(Interactor);
#endif
}

void ADeadHospitalPuzzleBase::SetPuzzleEnabled(bool ShouldEnable)
{
	// 한 번 완료된 퍼즐은 외부에서 다시 활성화해도 되돌아가지 않습니다.
	PuzzleEnabled = ShouldEnable && !PuzzleSolved;
}

bool ADeadHospitalPuzzleBase::CanCompletePuzzle(AActor* Interactor) const
{
	return IsValid(Interactor);
}

void ADeadHospitalPuzzleBase::HandleCheckpointRestored(FName CheckpointId)
{
	// 복구되면 게임을 저장 시점으로 되돌립니다. 앞에서 풀었더라도 체크포인트
	// 당시 미완료였다면 다시 사용할 수 있어야 하므로 GameMode 목록을 다시 읽습니다.
	const ADeadHospitalGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(GameMode))
	{
		return;
	}

	PuzzleSolved = GameMode->IsPuzzleCompleted(PuzzleId);
	PuzzleEnabled = StartsEnabled && !PuzzleSolved;
	OnPuzzleStateRestored(PuzzleSolved);
}
