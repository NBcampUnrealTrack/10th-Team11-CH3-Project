#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_RotateToTarget.generated.h"

UCLASS()
class DEAD_HOSPITAL_API UANS_RotateToTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

//protected:
//	//회전 속도 (값이 높을 수록 빠르게 회전
//	UPROPERTY(EditAnywhere, Category = "Rotation")
//	FName RootBoneName = "Hips";
//
//private:
//	FRotator PreviousBoneRotation;
//	bool bHasPreviousRotation = false;
};
