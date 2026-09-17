#include "ANS_RotateToTarget.h"
#include "ZombieCharacter.h"
#include "AIController.h"
#include "GameFramework/Character.h"


void UANS_RotateToTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	NotifyElapsedTime = 0.0f;
	NotifyTotalDuration = FMath::Max(TotalDuration, 0.01f);
}

void UANS_RotateToTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp) return;

	AZombieCharacter* Zombie = Cast<AZombieCharacter>(MeshComp->GetOwner());
	if (!Zombie) return;

	NotifyElapsedTime += FrameDeltaTime;
	const float Alpha = FMath::Clamp(NotifyElapsedTime / NotifyTotalDuration, 0.0f, 1.0f);

	//좌우 대칭이면 -1, 아니면 1
	const float SignedOffset = Zombie->bSearchTurnMirrored ? -SwingOffsetDegrees : SwingOffsetDegrees;

	//0~1 진행률에 맞춰 BaseYaw에서 목표 오프셋 각도까지 부드럽게 이동(Sin 곡선으로 자연스럽게)
	const float SmoothAlpha = FMath::Sin(Alpha * PI * 0.5f);
	const float NewYaw = Zombie->GetSearchBaseYaw() + SignedOffset * SmoothAlpha;

	FRotator NewRot(0.0f, NewYaw, 0.0f);

	if (USceneComponent* RootComp = Zombie->GetRootComponent())
	{
		RootComp->SetWorldRotation(NewRot, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (USkeletalMeshComponent* MeshCompToFix = Zombie->GetMesh())
	{
		MeshCompToFix->SetRelativeRotation(Zombie->GetDefaultMeshRelativeRotation());
	}
}
