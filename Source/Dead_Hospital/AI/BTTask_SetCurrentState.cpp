#include "BTTask_SetCurrentState.h"
#include "AIController.h"
#include "ZombieCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

//Behavior Tree에서 이 Task 노드가 실행될 때 호출된다.
//이 Task를 소유(빙의)한 AI 컨트롤러 -> 그 컨트롤러가 조종하는 Pawn -> 좀비 캐릭터 순으로
//타고 내려가서, 좀비의 CurrentState를 이 노드에 설정된 NewState 값으로 바꿔주는 역할만 한다.
EBTNodeResult::Type UBTTask_SetCurrentState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	//이 Behavior Tree 실행 중인 AI 컨트롤러를 가져온다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	// AI 컨트롤러가 없으면(비정상 상황) Task를 실패로 종료한다.
	if (!AIController) return EBTNodeResult::Failed;

	//AI 컨트롤러가 빙의 중인 Pawn을 가져온다.
	APawn* MyPawn = AIController->GetPawn();
	//빙의된 Pawn이 없으면 Task를 실패로 종료한다.
	if (!MyPawn) return EBTNodeResult::Failed;

	//Pawn을 실제로 상태를 갖고 있는 AZombieCharacter로 캐스팅한다.
	AZombieCharacter* Zombie = Cast<AZombieCharacter>(MyPawn);
	//좀비 캐릭터가 아니라면(다른 종류의 Pawn이라면) Task를 실패로 종료한다.
	if (!Zombie) return EBTNodeResult::Failed;

	/*UE_LOG(LogTemp, Warning, TEXT("[%s] SetCurrentState Called! NewState: %d"),
		*Zombie->GetName(), (int32)NewState);*/

	//실제 상태 변경 - NewState는 이 Task 노드의 프로퍼티로,
	//Behavior Tree 에디터에서 노드마다 다르게 지정해둔 목표 상태 값이다.
	Zombie->SetCurrentState(NewState);

	if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsEnum(TEXT("State"), static_cast<uint8>(NewState));
	}

	//여기까지 문제 없이 왔으면 Task 성공으로 종료한다.
	return EBTNodeResult::Succeeded;
}
