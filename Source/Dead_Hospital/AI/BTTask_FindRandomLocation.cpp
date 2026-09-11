#include "BTTask_FindRandomLocation.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_FindRandomLocation::UBTTask_FindRandomLocation()
{
	//Behavior Tree 에디터 화면에 이 노드가 어떤 이름으로 표시될지 정하는 것
	NodeName = TEXT("Find Random Location");

	//에디터에서 Location Key의 드롭다운에서 Vector 타입 키만 후보로 뜨게 필터링 해주는 코드
	LocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomLocation, LocationKey));
}

EBTNodeResult::Type UBTTask_FindRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	//1단계: 필요한 것들 가져오기
	//지금 이 Behavior Tree를 실행 중인 AI 컨트롤러(좀비의 "뇌")를 가져온다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	//좀비가 파괴되는 등의 이유로 컨트롤러를 못 찾으면 안전하게 실패 처리
	if (!AIController) return EBTNodeResult::Failed;

	//컨트롤러가 조종 중인 실제 좀비 캐릭터(Pawn)를 가져옴 - 위치 정보를 얻으려면 Pawn이 필요함
	APawn* MyPawn = AIController->GetPawn();
	//컨트롤러가 아직 Pawn을 못 잡았거나 놓친 경우를 대비한 안전장치
	//컨트롤러는 "판단하고 명령을 내리는 뇌"같은 존재라서 그 자체로는 위치라는 개념이 없다.
	if (!MyPawn) return EBTNodeResult::Failed;

	//현재 월드에서 동작 중인 네비게이션 시스템을 찾아옴
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	//레벨에 네비게이션이 하나도 배치되어 있지 않은 등의 예외 상황을 막는 안전장치
	if (!NavSystem) return EBTNodeResult::Failed;

	//2단계: 랜덤 위치 찾기
	
	//뽑아낸 랜덤 위치 값을 저장하기 위해 만든 지역 변수(아직 비어있음)
	FNavLocation RandomLocation;
	//MyPawn 위치를 중심으로 SearchRadius 반경 안에서 도달 가능한 랜덤 지점을 찾음
	//성공하면 RandomLocation에 결과가 채워지고, bFound가 true가 된다.
	bool bFound = NavSystem->GetRandomReachablePointInRadius(
		MyPawn->GetActorLocation(),
		SearchRadius,
		RandomLocation
	);
	
	//3단계: 찾았으면 Blackboard에 저장

	//위치값을 찾는데 성공했으면 실행
	if (bFound)
	{
		//현재 가지고 있는 블랙보드 컴포넌트를 가져와서 사용하겠다.
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		//블랙보드 컴포넌트 확인용 안전장치
		if (BlackboardComp)
		{
			//LocationKey로 지정된(에디터에서 선택할) Blackboard 키에 찾아낸 위치를 저장
			BlackboardComp->SetValueAsVector(LocationKey.SelectedKeyName, RandomLocation.Location);
			//저장까지 성공했으니 이 Task는 성공으로 처리한다.
			return EBTNodeResult::Succeeded;
		}
	}

	//위치값을 못 찾았거나, 위치를 찾았지만 저장할 곳이 없는 경우
	//두 경우 모두 결국 이 Task가 제대로 처리되지 않은 것이므로 실패로 반환
	return EBTNodeResult::Failed;
}
