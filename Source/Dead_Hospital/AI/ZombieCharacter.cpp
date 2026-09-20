#include "ZombieCharacter.h"
#include "ZombieAIController.h"
#include "DeadHospitalGameMode.h"
#include "ItemPickup.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
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

	//추격 거리
	MinChaseDistance = 150.0f;
	MaxChaseDistance = 800.0f;
	//추격 속도
	MinChaseSpeed = 200.0f;
	MaxChaseSpeed = 320.0f;


	//스폰되자마자 즉시 어그로(Chase 진입)를 걸지 여부
	bAggroOnSpawn = false;
	//P2P 순찰 로직에서 "지금 몇 번째 순찰 지점을 향하고 있는지"를 가리키는 인덱스
	CurrentPatrolIndex = 0;
	//Search(두리번거림) 상태에 진입한 시점의 기준 Yaw 값.
	SearchBaseYaw = 0.0f;
	//데미지를 받았을 때 경직 지속 시간
	HitstunDuration = 0.5f;

	//좀비 공격 범위 콜리전
	AttackRangeComp = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangeComp"));
	AttackRangeComp->SetupAttachment(GetCapsuleComponent());
	AttackRangeComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackRangeComp->SetSphereRadius(AttackRange);
	//공격 범위 오버랩 이벤트 바인딩
	AttackRangeComp->OnComponentBeginOverlap.AddDynamic(this, &AZombieCharacter::OnAttackOverlap);
	AttackRangeComp->OnComponentEndOverlap.AddDynamic(this, &AZombieCharacter::OnAttackEndOverlap);

	//ROV Avoidance: 좀비끼리(또는 다른 AI) 서로 겹치지 않고 부드럽게 비껴가게 하는 기능
	//좀비끼리(또는 다른 AI/장애물과) 서로 겹치지 않고 부드럽게 비껴가도록 RVO Avoidance 활성화
	//BeginPlay가 아니라 생성자에서 세팅해야함 - AvoidanceManager 등록(SetUpdatedComponent)이
	//Character의 경우 BeginPlay보다 먼저 실행되기 때문에, BeginPlay에서 세팅하면 이미 등록 타이밍을 놓쳐 무효화 됨
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 150.0f;//이 반경 안의 다른 Agent를 고려 대상으로 삼음
	GetCharacterMovement()->AvoidanceWeight = 0.5f;//명시적으로 설정(기본값에 의존하지 않음)
	GetCharacterMovement()->SetAvoidanceGroup(1);//이 좀비가 속한 그룸 (좀비끼리 같은 그룹으로)
	GetCharacterMovement()->SetGroupsToAvoid(1);//회피할 그룹(자기 그룹, 즉 다른 좀비들을 피함)
	GetCharacterMovement()->SetGroupsToIgnore(0);//무시할 그룹(없음)

	//디버그용 화살표 컴포넌트
	//DebugArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DebugArrow"));
	//DebugArrow->SetupAttachment(RootComponent);
	//DebugArrow->ArrowSize = 2.0f;
	//DebugArrow->SetHiddenInGame(false);
}

//이동속도/회전속도 초기값 세팅, 메시 기본 위치/회전 값 캐싱(히트스턴 복원용)
void AZombieCharacter::BeginPlay()
{
	Super::BeginPlay();

	//좀비 실제 이동속도를 PatrolSpeed로 변경해준다.
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	//좀비가 이동방향으로 몸을 돌릴때 회전을 부드럽게 해주기 위해 사용
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);
	GetCharacterMovement()->MaxAcceleration = 20000;
	
	//메시(스켈레탈 메시)의 "기본 상대 회전/위치"를 지금 시점 값으로 저장
	//나중에 Root Motion 등으로 메시 위치/회전이 흐트러졌을 때 "원래 자리로 되돌리는"
	//기준값으로 쓰기 위한 캐싱. BeginPlay에서 저장해야 실제 에디터에서 세팅한
	//초기 위치값이 정확히 들어간다. (Tick등 늦은 시점에서 읽으면 이미 틀어진 값을 캐싱해버릴 위험이 있음)
	DefaultMeshRelativeRotation = GetMesh()->GetRelativeRotation();
	DefaultMeshRelativeLocation = GetMesh()->GetRelativeLocation();

	bUseControllerRotationYaw = false;

	//"가짜 죽음(위장 시체)" 상태로 시작해야 한다면 그 처리부터,
	//아니면 스폰하자마자 바로 어그로를 걸어야 한다면 그 처리를 실행
	if (bIsFakeDead)
	{
		EnterFakeDead();
	}
	else if (bAggroOnSpawn)
	{
		AggroOnSpawn();
	}
}

//AI/플레이어가 이 좀비를 "보는" 기준점(눈 위치/방향)을 정의.
//AI Perception(시야 감지) 등에서 이 값을 기준으로 시야 판정을 하게 된다.
void AZombieCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	//눈 위치 = 발밑 기준 위치 + 눈높이 만큼 위로
	OutLocation = GetActorLocation() + FVector(0, 0, BaseEyeHeight);
	//시선 방향 = 캡슐이 실제로 향하고 있는 방향 그대로
	OutRotation = GetActorRotation();
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

//공격 범위에서 벗어나면 InAttackRange 갱신
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


//받은 데미지 계산 - 체력 감소, 사망/경직 분기
float AZombieCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! DamageAmount: %f, DamageCauser: %s"),
		*GetName(), DamageAmount, DamageCauser ? *DamageCauser->GetName() : TEXT("nullptr"));

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
	else
	{
		EnterHitstun();
	}

	return DamageAmount;
}

//좀비 사망 처리 - 상태 변경, 아이템 드랍, 킬 등록, 콜리전/AI 정지, 제거 예약
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

	//게임모드에 킬 등록(중복 방지)
	if (ADeadHospitalGameMode* GameMode = Cast<ADeadHospitalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->RegisterEnemyKillOnce(this);
	}

	//충돌체 비활성화. 죽은 좀비와 더 이상 충돌/오버랩이 발생하지 않도록 콜리전을 모두 끈다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//AI 컨트롤러 정지 및 UnPossess 처리로 뇌(Brain) 정지
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		//블랙보드 값도 사망에 맞게 정리
		if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
		{
			BlackboardComp->SetValueAsBool(AZombieAIController::BBKey_InAttackRange, false);
			BlackboardComp->ClearValue(AZombieAIController::BBKey_ChaseTarget);
			BlackboardComp->SetValueAsEnum(AZombieAIController::BBKey_State, static_cast<uint8>(EZombieState::Dead));
		}

		AIController->StopMovement();

		//비헤이비어 트리(brain) 완전히 정지
		if (UBrainComponent* BrainComp = AIController->GetBrainComponent())
		{
			BrainComp->StopLogic(TEXT("Zombie Died"));
		}

		//시야 감지도 꺼서 더 이상 아무것도 감지하지 않게 함
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

//피격 경직 진입 - 이동/BT 일시정지, 피격 애니메이션 재생, 일정 시간 뒤 복귀 예약
void AZombieCharacter::EnterHitstun()
{
	//이미 경직 중이면 복귀할 상태를 덮어쓰지 않음(경직 중 재피격 대비)
	if (CurrentState != EZombieState::Hitstun)
	{
		PreHitstunState = CurrentState;
	}

	if (AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController()))
	{
		ZombieController->StopMovement();

		//BT 일시정지(Stop과 달리 나중에 이어서 재개 가능) 
		if (UBrainComponent* Brain = ZombieController->GetBrainComponent())
		{
			Brain->PauseLogic(TEXT("Hitstun"));
		}

		ZombieController->SetZombieState(EZombieState::Hitstun);
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
	}

	//경직 중 재피격 시 SetTimer가 자동으로 갱신되어 경직시간이 자연스럽게 연장됨
	GetWorldTimerManager().SetTimer(
		HitstunTimerHandle,
		this,
		&AZombieCharacter::OnHitstunEnded,
		HitstunDuration,
		false
	);
}

//경직 시간 종료 시 호출 - BT 재개, 경직 전 상태로 복귀
void AZombieCharacter::OnHitstunEnded()
{
	if (CurrentState == EZombieState::Dead)
	{
		return;
	}

	if (AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController()))
	{
		if (UBrainComponent* Brain = ZombieController->GetBrainComponent())
		{
			Brain->ResumeLogic(TEXT("Hitstun Ended"));
		}

		ZombieController->SetZombieState(PreHitstunState);
	}
}

//사망 후 일정 시간이 지나면 호출 - 액터를 완전히 제거
void AZombieCharacter::OnDeathTimerExpired()
{
	Destroy();
}

void AZombieCharacter::HandleItemDrop()
{
	if (!ZombieDropTable || !ItemDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HandleItemDrop: DropTable/ItemDataTable 설정 누락"), *GetName());
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

	//ItemID로 ItemDataTable에서 실제 아이템 상세 데이터 조회
	FItemData* FoundItemData = ItemDataTable->FindRow<FItemData>(SelectedEntry->ItemID, TEXT("HandleItemDrop"));

	if (!FoundItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HandleItemDrop: ItemDataTable에서 ItemID(%s)를 찾지 못함"),
			*GetName(), *SelectedEntry->ItemID.ToString());
		return;
	}

	// 이 드롭 항목에 연결된 Pickup BP가 없으면 종료
	if (!SelectedEntry->PickupClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] HandleItemDrop: PickupClass가 비어 있음(ItemID: %s)"),
			*GetName(), *SelectedEntry->ItemID.ToString());
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AItemPickup* SpawnedPickup = GetWorld()->SpawnActor<AItemPickup>(
		SelectedEntry->PickupClass,
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams
	);

	if (!SpawnedPickup)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[%s] HandleItemDrop: ItemPickup 스폰 실패(ItemID: %s)"),
			*GetName(), *SelectedEntry->ItemID.ToString());
		return;
	}

	//드랍 수량으로 덮어써서 픽업에 세팅
	FItemData NewItemData = *FoundItemData;
	NewItemData.Quantity = SelectedEntry->DropQuantity;
	SpawnedPickup->SetItemData(NewItemData);

	UE_LOG(LogTemp, Log, TEXT("[%s] HandleItemDrop: %s x %d 드랍됨"),
		*GetName(), *SelectedEntry->ItemID.ToString(), SelectedEntry->DropQuantity);

}

//공격 애니메이션의 타격 타이밍(Anim Notify)에서 호출 - 대상이 범위 안이면 데미지 적용
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
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
			FString::Printf(TEXT("TakeDamage FIRED - Power: %f"), Power));
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
//상태별 이동속도/회전값 동기화, Search 이탈 시 타이머/몽타주 정리
void AZombieCharacter::SetCurrentState(EZombieState NewState)
{
	EZombieState OldState = CurrentState;
	CurrentState = NewState;

	//Search에서 다른 상태로 벗어나면 타이머/몽타주 정리(방치 시 잘못 재생될 수 있음)
	if (OldState == EZombieState::Search && NewState != EZombieState::Search)
	{
		GetWorldTimerManager().ClearTimer(SearchTimerHandle);

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && SearchTurnMontage)
		{
			AnimInstance->Montage_Stop(0.1f, SearchTurnMontage);
		}
	}

	switch (CurrentState)
	{
	case EZombieState::Patrol:
	case EZombieState::Attacking:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);
		GetCharacterMovement()->bOrientRotationToMovement = true;
		break;
	case EZombieState::Chase:
	case EZombieState::Investigating:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		GetCharacterMovement()->bOrientRotationToMovement = true;
		break;
	case EZombieState::Search:
		//제자리 두리번거림 - 자동회전은 끄고 SearchTurn 몽타주(루트모션) 가 회전을 담당
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
		break;
	case EZombieState::Hitstun:
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		break;
	default:
		break;
	}
}

//Search 진입 시점의 기준 Yaw 저장
void AZombieCharacter::SetSearchBaseYaw(float NewYaw)
{
	SearchBaseYaw = NewYaw;
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

	//몽타주 종료 시 OnAttackMontageEnded 호출되도록 델리게이트 등록
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AZombieCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
}

//Search 상태에서 두리번거리는 회전 몽타주 발생
void AZombieCharacter::PlaySearchTurnMontage()
{
	//호출 시점에 이미 Search가 아니면 무시(늦게 불린 타이머 장치)
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

//두리번 몽타주 종료 시 방향 반전 후 다음 두리번거림을 다시 예약(좌우 반복)
void AZombieCharacter::OnSearchTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted && CurrentState == EZombieState::Search)
	{
		GetCharacterMovement()->StopMovementImmediately();
		ToggleSearchTurnDirection();
		// NextTick 대신 0.1초의 최소 지연 시간을 부여하여 몽타주 실패 시의 무한 재귀 및 프레임 부하를 완벽히 차단
		//GetWorldTimerManager().SetTimer(SearchTimerHandle, this, &AZombieCharacter::PlaySearchTurnMontage, 0.1f, false);
		PlaySearchTurnMontage();
	}
}

//AttackMontage 종료 콜백 - OnAttackAnimationFinished로 위임
void AZombieCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnAttackAnimationFinished();
}

//가짜 죽음에서 일어나는 몽타주 종료 시 호출 - 살아있으면 BT 재시작
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

//ChaseTarget이 실제 공격 범위 안에 있는지 재계산해서 Blackboard 갱신
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

//Search 두리번거림 방향 반전
//(Root Motion 몽타주는 Mirror 노드로 반전이 안 되는 UE 구조적 한계로 현재는 값만 토글됨)
void AZombieCharacter::ToggleSearchTurnDirection()
{
	bSearchTurnMirrored = !bSearchTurnMirrored;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("SearchTurnMirrored: %s"),
		bSearchTurnMirrored ? TEXT("TRUE") : TEXT("FALSE")
	);
}

//스폰 즉시 플레이어를 어그로(Chase 진입)
void AZombieCharacter::AggroOnSpawn()
{
	AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController());
	UBlackboardComponent* BlackboardComp = ZombieController ? ZombieController->GetBlackboardComponent() : nullptr;

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
	ZombieController->SetZombieState(EZombieState::Chase);

	RefreshAttackRange();
}

//SearchTurn 몽타주 재생 중인지 조회
bool AZombieCharacter::IsPlayingSearchTurn() const
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return AnimInstance && SearchTurnMontage && AnimInstance->Montage_IsPlaying(SearchTurnMontage);
}

//가짜 죽음에서 깨어나기 - 몽타주 있으면 재생 후 BT 재시작, 없으면 즉시 처리
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

//가짜 죽음 진입 - 이동/BT 정지시켜 시체인 척
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

//Search 상태 시야 판정을 AI 컨트롤러로 위임
void AZombieCharacter::CheckSearchTurnSight()
{
	if (AZombieAIController* ZombieController = Cast<AZombieAIController>(GetController()))
	{
		ZombieController->CheckSearchTurnSight();
	}
}

//SearchTurn 미러링 여부 Getter
bool AZombieCharacter::bIsSearchTurnMirrored() const
{
	return bSearchTurnMirrored;
}

//다음 순찰 지점 좌표 반환 - PatrolPoints를 인덱스 기반으로 왕복(핑퐁) 순회
bool AZombieCharacter::GetNextPatrolLocation(FVector& OutLocation)
{
	if (PatrolPoints.Num() == 0)
	{
		return false;
	}

	//순찰 지점이 1개뿐이면 그 자리 반환, 인덱스 갱신은 불필요
	if (PatrolPoints.Num() == 1)
	{
		if (AActor* OnlyPoint = PatrolPoints[0])
		{
			OutLocation = OnlyPoint->GetActorLocation();
			return true;
		}
		return false;
	}

	//배열에 빈 슬롯(nullptr)이 있을 수 있으니 방어
	AActor* TargetPoint = PatrolPoints[CurrentPatrolIndex];
	if (!TargetPoint)
	{
		return false;
	}

	OutLocation = TargetPoint->GetActorLocation();

	//다음 인덱스 계산(왕복)
	const int32 LastIndex = PatrolPoints.Num() - 1;

	if (bPatrolForward)
	{
		if (CurrentPatrolIndex >= LastIndex)
		{
			bPatrolForward = false;
			CurrentPatrolIndex--;
		}
		else
		{
			CurrentPatrolIndex++;
		}
	}
	else
	{
		if (CurrentPatrolIndex <= 0)
		{
			bPatrolForward = true;
			CurrentPatrolIndex++;
		}
		else
		{
			CurrentPatrolIndex--;
		}
	}

	return true;
}
