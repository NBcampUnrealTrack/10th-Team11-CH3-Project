#include "WeepingAngelZombie.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerCharacter.h"

AWeepingAngelZombie::AWeepingAngelZombie()
{
    PrimaryActorTick.bCanEverTick = true;
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
        }
        else
        {
            // [땡] 시야에서 벗어났을 때: 돌진하며 애니메이션 재생
            GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
            GetMesh()->bPauseAnims = false;
        }

        bWasSeenLastFrame = bIsSeenNow;
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