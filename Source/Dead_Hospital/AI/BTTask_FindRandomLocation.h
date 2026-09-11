#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindRandomLocation.generated.h"

//AI의 현재 위치를 기준으로 네비게이션 가능한 랜덤 위치를 찾아 지정된 Blackboard 키에 저장하는 Task
UCLASS()
class DEAD_HOSPITAL_API UBTTask_FindRandomLocation : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_FindRandomLocation();

protected:
	//Behavior Tree가 이 Task 노드 차례가 될 때 호출됨 - 랜덤 위치를 찾아 Blackboard에 저장
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	//찾아낸 랜덤 위치를 Blackboard의 어느 키에 저장할지 지정해두는 자리
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector LocationKey;

	//이 Task를 실행하는 AI의 현재 위치를 중심으로 반경 안에서 랜덤 지점을 찾을지 정하는 거리값
	UPROPERTY(EditAnywhere, Category = "Search", meta = (ClampMin = "100.0"))
	float SearchRadius = 1000.0f;
};
