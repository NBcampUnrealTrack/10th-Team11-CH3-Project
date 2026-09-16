#include "ZombieCharacter.h"
#include "ZombieAIController.h"
//#include "GameManager.h"
#include "ItemPickup.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

AZombieCharacter::AZombieCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	//이 좀비가 스폰될 때 어떤 AI컨트롤러를 자동으로 빙의시킬지, 그 설계도(클래스 정보)를 지정해두는 것. 어떤 클래스를 쓸지만 정해두는 역할
	AIControllerClass = AZombieAIController::StaticClass();
	//월드에 위치하거나 스폰됐을 때 자동으로 AI 컨트롤러가 빙의된다.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	//좀비의 최대 체력
	MaxHealth = 100.0f;
	//현재 체력
	Health = MaxHealth;
	//이 좀비 개체(또는 종류)를 구분하기 위한 이름
	ZombieName = "ZombieA";
	//공격 시 플레이어에게 주는 데미지 값
	Power = 20.0f;
	//공격 판정 콜리전의 반지름
	AttackRange = 100.0f;
	//Patrol/Idle 상태의 이동속도
	PatrolSpeed = 300.0f;
	//추격시 이동 속도
	ChaseSpeed = 300.0f;
	//현재 추적 중인 대상 -초기에는 없음
	ChaseTarget = nullptr;
	//좀비의 현재 상태 - 초기는 순찰
	CurrentState = EZombieState::Patrol;
	//좀비 어그로
	bAggroOnSpawn = false;

	//좀비 공격 범위 콜리전
	AttackRangeComp = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangeComp"));
	AttackRangeComp->SetupAttachment(GetCapsuleComponent());
	AttackRangeComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackRangeComp->SetSphereRadius(AttackRange);
	//공격 범위 오버랩 이벤트 바인딩
	AttackRangeComp->OnComponentBeginOverlap.AddDynamic(this, &AZombieCharacter::OnAttackOverlap);
	AttackRangeComp->OnComponentEndOverlap.AddDynamic(this, &AZombieCharacter::OnAttackEndOverlap);

	//좀비끼리(또는 다른 AI/장애물과) 서로 겹치지 않고 부드럽게 비껴가도록 RVO Avoidance 활성화
	//BeginPlay가 아니라 생성자에서 세팅해야함 - AvoidanceManager 등록(SetUpdatedComponent)이
	//Character의 경우 BeginPlay보다 먼저 실행되기 때문에, BeginPlay에서 세팅하면 이미 등록 타이밍을 놓쳐 무효화 됨
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 150.0f;//이 반경 안의 다른 Agent를 고려 대상으로 삼음
	GetCharacterMovement()->AvoidanceWeight = 0.5f;//명시적으로 설정(기본값에 의존하지 않음)
	GetCharacterMovement()->SetAvoidanceGroup(1);//이 좀비가 속한 그룸 (좀비끼리 같은 그룹으로)
	GetCharacterMovement()->SetGroupsToAvoid(1);//회피할 그룹(자기 그룹, 즉 다른 좀비들을 피함)
	GetCharacterMovement()->SetGroupsToIgnore(0);//무시할 그룹(없음)

}

void AZombieCharacter::BeginPlay()
{
	Super::BeginPlay();

	//좀비 실제 이동속도를 PatrolSpeed로 변경해준다.
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	//좀비가 이동방향으로 몸을 돌릴때 회전을 부드럽게 해주기 위해 사용
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);

	bUseControllerRotationYaw = false;

	if (bIsFakeDead)
	{
		EnterFakeDead();
	}
	else if (bAggroOnSpawn)
	{
		AggroOnSpawn();
	}
}


//공격 범위 안에 플레이어가 들어오면 Attack 상태로 전환(실제 공격은 아님)
void AZombieCharacter::OnAttackOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (CurrentState == EZombieState::Dead)
	{
		return;
	}

	//공격 범위에 들어온 Actor가 플레이어인지 확인한다.
	ACharacter* Player = Cast<ACharacter>(OtherActor);
	if (!Player || !Player->IsPlayerControlled())
	{
		return;
	}

	// 현재 ChaseTarget이 공격 범위 안에 있는지 다시 계산
	RefreshAttackRange();
}

//오버랩 끝나면 상태를 Chase로 변경되고 다시 플레이어를 쫓아간다.
void AZombieCharacter::OnAttackEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (CurrentState == EZombieState::Dead) return;

	//공격 범위에서 벗어난 Actor가 플레이어인지 확인한다.
	ACharacter* Player = Cast<ACharacter>(OtherActor);
	if (!Player || !Player->IsPlayerControlled())
	{
		return;
	}

	// 현재 ChaseTarget이 공격 범위 안에 있는지 다시 계산
	RefreshAttackRange();
}


//받은 데미지 계산
float AZombieCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// 이미 죽은 상태라면 추가 데미지를 받지 않는다.
	if (CurrentState == EZombieState::Dead)
	{
		return 0.0f;
	}

	// 실제 데미지만큼 체력 감소
	Health -= DamageAmount;
	// 체력이 0보다 작아지지 않도록 제한
	Health = FMath::Max(Health, 0.0f);

	// 체력이 0 이하가 되면 사망 처리
	if (Health <= 0.0f)
	{
		Die();
	}

	return DamageAmount;
}

//좀비 사망 처리 - 상태 변경, 콜리전 비활성화, 일정 시간 후 제거 예약
void AZombieCharacter::Die()
{
	// 이미 죽은 상태라면 사망 처리를 다시 실행하지 않는다.
	if (CurrentState == EZombieState::Dead)
	{
		return;
	}

	// Dead 상태로 변경
	SetCurrentState(EZombieState::Dead);

	HandleItemDrop();

	/*if (AGameManager* GameManager = Cast<AGameManager>(UGameplayStatics::GetGameMode(this)))
	{
		GameManager->AddKillCount();
	}*/

	//충돌체 비활성화. 죽은 좀비와 더 이상 충돌/오버랩이 발생하지 않도록 콜리전을 모두 끈다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//AI 컨트롤러 정지 및 UnPossess 처리로 뇌(Brain) 정지
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
		{
			BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_InAttackRange, false);
			BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_IsAttacking, false);
			BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_bCanSeeTarget, false);
			BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_bInvestigatingNoise, false);
			BlackboardComp->ClearValue(AZombieAIController::BBKey_ChaseTarget);
		}

		AIController->StopMovement();

		//비헤이비어 트리(brain) 완전히 정지
		if (UBrainComponent* BrainComp = AIController->GetBrainComponent())
		{
			BrainComp->StopLogic(TEXT("Zombie Died"));
		}

		if (UAIPerceptionComponent* PerceptionComp = AIController->GetAIPerceptionComponent())
		{
			PerceptionComp->ForgetAll();
			PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		}

		AIController->UnPossess();
	}

	//30초 뒤 OndeathTimerExpired가 호출되어 액터가 완전히 제거되도록 타이머 예약
	//(죽는 애니메이션 등을 보여줄 시간을 벌어주는 용도, 시간은 조절)
	GetWorldTimerManager().SetTimer(
		DisappearTimerHandle,
		this,
		&AZombieCharacter::OnDeathTimerExpired,
		30.0f,
		false
	);
}

//사망 후 일정 시간이 지나면 호출 - 액터를 완전히 제거
void AZombieCharacter::OnDeathTimerExpired()
{
	Destroy();
}

void AZombieCharacter::HandleItemDrop()
{
	if (!ZombieDropTable || !ItemDataTable || !ItemPickupClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HandleItemDrop: DropTable/ItemDataTable/PickupClass 설정 누락"), *GetName());
		return;
	}

	TArray<FZombieDropEntry*> AllRows;
	ZombieDropTable->GetAllRows<FZombieDropEntry>(TEXT("HandleItemDrop"), AllRows);

	if (AllRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HandleItemDrop: DropTable에 행이 없음"), *GetName());
		return;
	}

	float TotalWeight = 0.0f;
	for (const FZombieDropEntry* Row : AllRows)
	{
		TotalWeight += Row->Weight;
	}

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HandleItemDrop: 전체 가중치 합이 0이하"), *GetName());
		return;
	}

	//가중치 기반 랜덤 뽑기(누적합 방식)
	float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	const FZombieDropEntry* SelectedEntry = nullptr;

	for (const FZombieDropEntry* Row : AllRows)
	{
		AccumulatedWeight += Row->Weight;
		if (RandomValue <= AccumulatedWeight)
		{
			SelectedEntry = Row;
			break;
		}
	}

	//부동소수점 오차로 인해 SelectedEntry를 찾지 못했으나,
	//실제 드랍 테이블 행이 존재한다면 마지막 행을 기본값으로 할당
	//안전장치
	if (!SelectedEntry && AllRows.Num() > 0)
	{
		SelectedEntry = AllRows.Last();
	}

	//"드랍 없음"도 정상 결과로 처리
	if (!SelectedEntry || SelectedEntry->ItemID.IsNone())
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] HandleItemDrop: 드랍 없음"), *GetName());
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AItemPickup* SpawnedPickup = GetWorld()->SpawnActor<AItemPickup>(
		ItemPickupClass,
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams
	);

	if (!SpawnedPickup)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] HandleItemDrop: ItemPickup 스폰 실패(ItemID: %s)"),
			*GetName(), *SelectedEntry->ItemID.ToString());
		return;
	}

	//FItemData NewItemData = *FoundItemData;
	//NewItemData.Quantity = SelectedEntry->DropQuantity;
	//SpawnedPickup->SetItemData(NewItemData);

	//UE_LOG(LogTemp, Log, TEXT("[%s] HandleItemDrop: %s x %d 드랍됨"),
	//	*GetName(), *SelectedEntry->ItemID.ToString(), SelectedEntry->DropQuantity);

}

//Anim Notify에서 호출 - ChaseTarget이 범위 내에 있으면 데미지 적용
void AZombieCharacter::Attack()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}
	
	//Blackboard에 저장된 ChaseTarget(현재 추적/공격 대상)을 가져온다.
	UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent();
	AActor* Target = BlackboardComp
		? Cast<AActor>(BlackboardComp->GetValueAsObject(AZombieAIController::BBKey_ChaseTarget)) : nullptr;
	//대상이 존재하고, 실제로 공격 범위 콜리전 안에 있는 경우에만 데미지를 적용한다.
	//(애니메이션 재생 중 대상이 범위를 벗어났을 수 있으므로 여기서 다시 확인)
	if (IsValid(Target) && AttackRangeComp->IsOverlappingActor(Target))
	{
		Target->TakeDamage(Power, FDamageEvent(), GetController(), this);
	}
}

//체력 값 직접 설정 - 외부(아이템, 디버그, 치트 등)에서 체력을 강제로 바꿔야 할 때 사용
void AZombieCharacter::SetHealth(float NewHealth)
{
	Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	if (Health <= 0.0f && CurrentState != EZombieState::Dead)
	{
		Die();
	}
}


//좀비의 상태(Idle/Patrol/Chase/Dead 등)를 변경하고,
//상태에 맞는 이동 속도로 캐릭터 무브먼트를 동기화해준다.
void AZombieCharacter::SetCurrentState(EZombieState NewState)
{
	EZombieState OldState = CurrentState;
	CurrentState = NewState;

	if (OldState == EZombieState::Search && NewState != EZombieState::Search)
	{
		GetWorldTimerManager().ClearTimer(SearchTimerHandle);

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && SearchTurnMontage)
		{
			AnimInstance->Montage_Stop(0.1f, SearchTurnMontage);
		}
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_bInvestigatingNoise, false);
			}
		}
	}

	switch (CurrentState)
	{
	case EZombieState::Patrol:
	case EZombieState::Idle:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);
		break;
	case EZombieState::Chase:
	case EZombieState::Search:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		break;
	default:
		break;
	}
}

//공격 애니메이션이 끝나는 순간 호출된다.
void AZombieCharacter::OnAttackAnimationFinished()
{
#if WITH_EDITOR
	GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Attack Finished Called"));
#endif
	//이 좀비를 조종하는 AI Controller를 가져온다.
	if (AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController()))
	{
		//Blackboard의 IsAttacking을 false로 바꾼다.
		//그러면 BT가 다시 Chase 또는 다음 Attack을 선택할 수 있다.
		ZombieController->FinishAttack();
	}
}

//AttackMontage를 재생한다. 보통 BT Task나 Attack() 흐름에서 호출되어
//실제 공격 애니메이션을 재생시키는 역할을 한다.
void AZombieCharacter::PlayAttackMontage()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance || !AttackMontage)
	{
		OnAttackAnimationFinished();
		return;
	}

	const float Duration = AnimInstance->Montage_Play(AttackMontage);

	if (Duration <= 0.0f)
	{
		OnAttackAnimationFinished();
		return;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AZombieCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
}

void AZombieCharacter::PlaySearchTurnMontage()
{
	if (CurrentState != EZombieState::Search)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	if (AnimInstance && SearchTurnMontage)
	{
		AnimInstance->Montage_Play(SearchTurnMontage);

		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AZombieCharacter::OnSearchTurnMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, SearchTurnMontage);
	}
}

void AZombieCharacter::OnSearchTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted && CurrentState == EZombieState::Search)
	{
		ToggleSearchTurnDirection();
		// NextTick 대신 0.1초의 최소 지연 시간을 부여하여 몽타주 실패 시의 무한 재귀 및 프레임 부하를 완벽히 차단
		GetWorldTimerManager().SetTimer(SearchTimerHandle, this, &AZombieCharacter::PlaySearchTurnMontage, 0.1f, false);
	}
}

void AZombieCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnAttackAnimationFinished();
}

void AZombieCharacter::OnGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CurrentState == EZombieState::Dead)
	{
		return;
	}

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}
}

void AZombieCharacter::RefreshAttackRange()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	UBlackboardComponent* BlackboardComp = AIController ? AIController->GetBlackboardComponent() : nullptr;

	if (!BlackboardComp || !AttackRangeComp)
	{
		return;
	}

	AActor* TargetChase = Cast<AActor>(BlackboardComp->GetValueAsObject(AZombieAIController::BBKey_ChaseTarget));

	const bool bTargetInAttackRange = IsValid(TargetChase) && AttackRangeComp->IsOverlappingActor(TargetChase);

	BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_InAttackRange, bTargetInAttackRange);
}

void AZombieCharacter::ToggleSearchTurnDirection()
{
	bSearchTurnMirrored = !bSearchTurnMirrored;
}

void AZombieCharacter::AggroOnSpawn()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	UBlackboardComponent* BlackboardComp = AIController ? AIController->GetBlackboardComponent() : nullptr;

	if (!BlackboardComp)
	{
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!PlayerCharacter)
	{
		return;
	}

	BlackboardComp->SetValueAsObject(AZombieAIController::BBKey_ChaseTarget, PlayerCharacter);
	BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_bCanSeeTarget, true);

	RefreshAttackRange();
}

bool AZombieCharacter::IsPlayingSearchTurn() const
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return AnimInstance && SearchTurnMontage && AnimInstance->Montage_IsPlaying(SearchTurnMontage);
}

void AZombieCharacter::WakeUp()
{
	// 이미 일어났거나, 애초에 일어날 시체가 아니면 무시
	if (!bIsFakeDead) return;

	bIsFakeDead = false;
	if(GetUpMontage)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &AZombieCharacter::OnGetUpMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, GetUpMontage);

			PlayAnimMontage(GetUpMontage);
		}
	}
	else
	{
		//몽타주가 없으면 바로 추적 시작
		OnGetUpMontageEnded(nullptr, false);
	}

	// 비명 소리 재생
	// if (ScreamSound) UGameplayStatics::PlaySoundAtLocation(...);

	// 여기서 AI Controller를 활성화하거나 상태 변경 신호 주기
	UE_LOG(LogTemp, Warning, TEXT("좀비가 깨어납니다."));

}

void AZombieCharacter::EnterFakeDead()
{
	bIsFakeDead = true;

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		AICon->StopMovement();

		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("FakeDead"));
		}
	}
}

void AZombieCharacter::CheckSearchTurnSight()
{
	if (AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController()))
	{
		ZombieController->CheckSearchTurnSight();
	}
}
