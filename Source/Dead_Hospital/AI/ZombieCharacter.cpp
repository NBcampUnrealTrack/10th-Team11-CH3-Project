#include "ZombieCharacter.h"
#include "ZombieAIController.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieCharacter::AZombieCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	//이 좀비가 스폰될 때 어떤 AI컨트롤러를 자동으로 빙의시킬지, 그 설계도(클래스 정보)를 지정해두는 것. 어떤 클래스를 쓸지만 정해두는 역할
	AIControllerClass = AZombieAIController::StaticClass();
	//월드에 위치하거나 스폰됐을 때 자동으로 AI 컨트롤러가 빙의된다.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	MaxHealth = 100.0f;
	Health = MaxHealth;
	ZombieName = "ZombieA";
	Defense = 30.0f;
	Power = 20.0f;
	AttackRange = 100.0f;
	PatrolSpeed = 300.0f;
	ChaseSpeed = 300.0f;
	ChaseTarget = nullptr;
	CurrentState = EZombieState::Patrol;

	//좀비 공격 범위 콜리전
	AttackRangeComp = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangeComp"));
	AttackRangeComp->SetupAttachment(GetCapsuleComponent());
	AttackRangeComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackRangeComp->SetSphereRadius(AttackRange);
	//공격 범위 오버랩 이벤트 바인딩
	AttackRangeComp->OnComponentBeginOverlap.AddDynamic(this, &AZombieCharacter::OnAttackOverlap);
	AttackRangeComp->OnComponentEndOverlap.AddDynamic(this, &AZombieCharacter::OnAttackEndOverlap);

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
}


//공격 범위 안에 플레이어가 들어오면 Attack 상태로 전환(실제 공격은 아님)
void AZombieCharacter::OnAttackOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Player = Cast<ACharacter>(OtherActor))
	{
		if (Player->IsPlayerControlled())
		{
			// 직접 CurrentState를 바꾸지 않고
			// SetCurrentState()를 통해 상태를 변경한다.
			SetCurrentState(EZombieState::Attack);

			// 공격 범위 안에 들어온 플레이어를 추격 대상으로 저장한다.
			ChaseTarget = Player;
		}
	}
}
//오버랩 끝나면 상태를 Chase로 변경되고 다시 플레이어를 쫓아간다.
void AZombieCharacter::OnAttackEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ACharacter* Player = Cast<ACharacter>(OtherActor))
	{
		if (Player->IsPlayerControlled() && Player == ChaseTarget)
		{
			// 직접 상태를 바꾸지 않고 SetCurrentState()를 호출해서
			// Chase 상태와 추격 속도를 함께 적용한다.
			SetCurrentState(EZombieState::Chase);
		}
	}
}

//사망 후 일정 시간이 지나면 호출 - 액터를 완전히 제거
void AZombieCharacter::OnDeathTimerExpired()
{
	Destroy();
}

//받은 데미지 계산
float AZombieCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{

	// 이미 죽은 상태라면 추가 데미지를 받지 않는다.
	if (CurrentState == EZombieState::Dead)
	{
		return 0.0f;
	}

	// 방어력은 0 ~ 100 사이의 값으로 제한한다.
	// 실수로 100보다 큰 값이 들어가도
	// 데미지가 음수가 되는 것을 방지한다.
	float ClampedDefense = FMath::Clamp(Defense, 0.0f, 100.0f);

	// 방어력을 적용한 실제 데미지 계산
	float ActualDamage =
		DamageAmount * (1.0f - ClampedDefense / 100.0f);

	// 실제 데미지만큼 체력 감소
	Health -= ActualDamage;

	// 체력이 0보다 작아지지 않도록 제한
	Health = FMath::Max(Health, 0.0f);

	// 체력이 0 이하가 되면 사망 처리
	if (Health <= 0.0f)
	{
		Die();
	}

	return ActualDamage;
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

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackRangeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetWorldTimerManager().SetTimer(
		DisappearTimerHandle,
		this,
		&AZombieCharacter::OnDeathTimerExpired,
		30.0,
		false
	);
}

//Anim Notify에서 호출 - ChaseTarget이 범위 내에 있으면 데미지 적용
void AZombieCharacter::Attack()
{
	if (ChaseTarget)
	{
		if (AttackRangeComp->IsOverlappingActor(ChaseTarget))
		{
			//상대가 만들어놓은 TakeDamage를 호출하겠다
			//FDamageEvent()를 사용할 때는 "Engine/DamageEvent.h"를 사용해야한다.
			ChaseTarget->TakeDamage(Power, FDamageEvent(), GetController(), this);
		}
	}
}

//체력 값 직접 설정
void AZombieCharacter::SetHealth(float NewHealth)
{
	Health = NewHealth;
}

void AZombieCharacter::SetCurrentState(EZombieState NewState)
{
	CurrentState = NewState;

	switch (CurrentState)
	{
	case EZombieState::Patrol:
	case EZombieState::Idle:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		break;
	case EZombieState::Chase:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		break;
	default:
		break;
	}
}
