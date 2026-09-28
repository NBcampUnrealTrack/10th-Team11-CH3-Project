#include "WeepingAngelZombie.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/KismetMathLibrary.h"

AWeepingAngelZombie::AWeepingAngelZombie()
{
    PrimaryActorTick.bCanEverTick = true;

    ChaseSpeed = 800.0f;

    // 오디오 컴포넌트 생성 및 부착
    MovementAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("MovementAudioComp"));
    MovementAudioComp->SetupAttachment(RootComponent);
    // 게임 시작 시에는 기본적으로 소리가 나지 않게 설정
    MovementAudioComp->bAutoActivate = false;
}

// 공격 불가
float AWeepingAngelZombie::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    // 아무리 데미지가 들어와도 0.0f를 반환하여 체력이 깎이지 않게 방어
    UE_LOG(LogTemp, Warning, TEXT("우는 천사는 공격할 수 없습니다"));
    return 0.0f;
}

// 얼음/땡 처리
void AWeepingAngelZombie::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    bool bIsSeenNow = CheckIfSeenByPlayer();

    // 상태가 변했을 때만 속도와 애니메이션 업데이트
    if (bIsSeenNow != bWasSeenLastFrame)
    {
        if (bIsSeenNow)
        {
            // [얼음] 플레이어가 쳐다볼 때: 발을 묶고 애니메이션을 멈춤
            GetCharacterMovement()->MaxWalkSpeed = 0.0f;
            GetMesh()->bPauseAnims = true;

            // 움직임이 멈추면 돌 갈리는 소리 정지
            if (MovementAudioComp->IsPlaying())
            {
                MovementAudioComp->Stop();
            }
        }
        else
        {
            // [땡] 시야에서 벗어났을 때: 돌진하며 애니메이션 재생
            GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
            GetMesh()->bPauseAnims = false;

            // 시야에서 벗어나 움직이기 시작하면 돌 갈리는 소리 재생
            if (!MovementAudioComp->IsPlaying())
            {
                MovementAudioComp->Play();
            }
        }

        bWasSeenLastFrame = bIsSeenNow;
    }
    // 시야에서 벗어나 있을 때(땡 상태) 항상 플레이어 방향으로 회전
    if (!bIsSeenNow)
    {
        APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
        if (Player != nullptr)
        {
            FVector AngelLocation = GetActorLocation();
            FVector PlayerLocation = Player->GetActorLocation();

            // 플레이어를 향하는 타겟 회전값 계산
            FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(AngelLocation, PlayerLocation);

            // 천사가 위아래로 눕지 않도록 좌우(Yaw) 회전값만 적용
            SetActorRotation(FRotator(0.f, LookAtRotation.Yaw, 0.f));
        }
    }
}

// 시야 판정
bool AWeepingAngelZombie::CheckIfSeenByPlayer()
{
    APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (Player == nullptr) return false;

    FVector PlayerLocation;
    FRotator PlayerRotation;
    Player->GetActorEyesViewPoint(PlayerLocation, PlayerRotation);

    FVector DirToAngel = (GetActorLocation() - PlayerLocation).GetSafeNormal();
    FVector PlayerForward = PlayerRotation.Vector();

    // 시야각(FOV) 검사 
    // 내적 값이 0.5 이상이면 정면을 기준으로 약 60도 원뿔 시야 내에 있다는 뜻
    float DotProduct = FVector::DotProduct(PlayerForward, DirToAngel);
    if (DotProduct > 0.5f)
    {
        // 장애물(벽) 가림 검사
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(Player); // 플레이어 자신은 통과

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            PlayerLocation,
            GetActorLocation(),
            ECC_Visibility,
            QueryParams
        );

        // 시야에 있고 + 광선을 쐈을 때 벽에 막히지 않고 우는 천사를 정확히 맞췄다면
        if (bHit && HitResult.GetActor() == this)
        {
            return true; // 플레이어가 나를 쳐다보고 있다는 뜻
        }
    }
    return false;
}

void AWeepingAngelZombie::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);

    // 닿은 대상이 플레이어인지 확인
    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);

    // 플레이어가 유효하고, 아직 죽지 않은 상태라면
    if (Player != nullptr && !Player->IsDead())
    {
        UE_LOG(LogTemp, Warning, TEXT("우는 천사에게 붙잡혔습니다! 즉사 발동."));

        // 데미지를 주기 직전에 사운드 재생
        if (CatchSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, CatchSound, GetActorLocation());
        }

        // 플레이어에게 99999의 압도적인 데미지를 가해 기존 사망 로직(Die)을 강제로 실행시킴
        UGameplayStatics::ApplyDamage(
            Player,
            99999.0f, // 즉사 데미지
            GetController(),
            this,
            UDamageType::StaticClass()
        );
    }
}