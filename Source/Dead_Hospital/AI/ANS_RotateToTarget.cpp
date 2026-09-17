#include "ANS_RotateToTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Gameframework/Actor.h"

void UANS_RotateToTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn) return;

	//AAIController 및 Blackboard 가져오기
	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
	if (!AIController) 
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, TEXT("ANS:AIController null!"));
		return;
	}

	UBlackboardComponent* BBComp = AIController->GetBlackboardComponent();
	if (!BBComp) return;

	//블랙보드에서 목표 위치 가져오기
	FVector TargetLocation = BBComp->GetValueAsVector(TargetLocationKeyName);
	if (TargetLocation.IsZero()) return;

	//Pitch/Roll은 고정하고 Yaw(Z축 회전)만 계산
	FVector CurrentLocation = Owner->GetActorLocation();
	FVector TargetDir = (TargetLocation - CurrentLocation).GetSafeNormal2D();
	
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("ANS TargetLoc: %s"), *TargetLocation.ToString()));

	if (TargetDir.IsNearlyZero()) return;

	FRotator CurrentRot = Owner->GetActorRotation();
	FRotator TargetRot = TargetDir.Rotation();

	//TargetTor 방향으로 액터를 부드럽게 Interp 회전
	FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, FrameDeltaTime, RotationSpeed);
	Owner->SetActorRotation(NewRot);
}
