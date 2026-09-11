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
	ChaseSpeed = 500.0f;
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
}


//공격 범위 안에 플레이어가 들어오면 Attack 상태로 전환(실제 공격은 아님)
void AZombieCharacter::OnAttackOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Player = Cast<ACharacter>(OtherActor))
	{
		if (Player->IsPlayerControlled())
		{
			CurrentState = EZombieState::Attack;
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
			CurrentState = EZombieState::Chase;
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
	float ActualDamage = DamageAmount * (1 - Defense / 100.0f);
	Health -= ActualDamage;
	Health = FMath::Max(Health, 0.0f);
	if (Health <= 0)
	{
		Die();
	}
	return ActualDamage;
}

//좀비 사망 처리 - 상태 변경, 콜리전 비활성화, 일정 시간 후 제거 예약
void AZombieCharacter::Die()
{
	CurrentState = EZombieState::Dead;
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
