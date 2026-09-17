#include "ANS_RotateToTarget.h"
#include "ZombieCharacter.h"
#include "GameFramework/Character.h"


void UANS_RotateToTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	//bHasPreviousRotation = false;
}

void UANS_RotateToTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	//if (!MeshComp) return;

	//AActor* Owner = MeshComp->GetOwner();
	//if (!Owner) return;

	////현재 프레임의 본 회전값(월드 기준)을 가져온다.
	//FRotator CurrentBoneRotation = MeshComp->GetBoneQuaternion(RootBoneName, EBoneSpaces::ComponentSpace).Rotator();

	//if (!bHasPreviousRotation)
	//{
	//	PreviousBoneRotation = CurrentBoneRotation;
	//	bHasPreviousRotation = true;
	//	return;
	//}

	////이번 프레임에 애니메이션이 실제로 돈 각도(Yaw)만 델타로 추출
	//float DeltaYaw = FRotator::NormalizeAxis(CurrentBoneRotation.Yaw - PreviousBoneRotation.Yaw);

	//if (!FMath::IsNearlyZero(DeltaYaw))
	//{
	//	Owner->AddActorWorldRotation(FRotator(0.0f, DeltaYaw, 0.0f));
	//}

	//PreviousBoneRotation = CurrentBoneRotation;
}
