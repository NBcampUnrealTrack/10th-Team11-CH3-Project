#include "BTTask_Attack.h"
#include "ZombieAIController.h"

UBTTask_Attack::UBTTask_Attack()
{
	//좀비마다 이 Task가 독립적인 델리게이트/OwnerComp를 들고 있어야 해서
	//기본 공유 인스턴스 대신 좀비별로 따로 인스턴스를 만들게 함
	bCreateNodeInstance = true;
	NodeName = TEXT("Attack");
}

EBTNodeResult::Type UBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AZombieAIController* AIController = Cast<AZombieAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AIController->StartAttack();

	CachedOwnerComp = &OwnerComp;
	AttackFinishedDelegateHandle = AIController->OnAttackSequenceFinished.AddUObject(
		this, &UBTTask_Attack::HandleAttackFinished
	);

	return EBTNodeResult::InProgress;
}

void UBTTask_Attack::HandleAttackFinished()
{
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	if (!OwnerComp)
	{
		return;
	}

	if (AZombieAIController* AIController = Cast<AZombieAIController>(OwnerComp->GetAIOwner()))
	{
		AIController->OnAttackSequenceFinished.Remove(AttackFinishedDelegateHandle);
	}
	
	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_Attack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	//Decorator에 의해 중간에 강제 Abort된 경우에도 델리게이트 구독 해제
	if (AZombieAIController* AIController = Cast<AZombieAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->OnAttackSequenceFinished.Remove(AttackFinishedDelegateHandle);
	}
	return EBTNodeResult::Aborted;
}
