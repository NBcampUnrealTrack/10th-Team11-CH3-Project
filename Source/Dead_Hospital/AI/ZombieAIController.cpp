#include "ZombieAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Pawn.h"

AZombieAIController::AZombieAIController()
{
	//좀비의 감지 기능을 담당할 AIPerceptionn 컴포넌트를 생성
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	//방금 만든 컴포넌트를, 이 AI 컨트롤러의 공식 Perception 컴포넌트로 등록한다.
	SetPerceptionComponent(*AIPerception);

	//시야(Sight) 감각에 대한 세부 설정을 생성
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	//이 거리 안에 들어오면 감지를 시작함
	SightConfig->SightRadius = 1500.0f;
	//이미 감지한 타겟은 이 거리를 넘어야 완전히 놓친 것으로 처리 - SightRadius보다 일부러 더 크게 잡아서,
	//경계선 거리에서 감지가 켜졌다 꺼졌다 하는 떨림 현상(hysteresis)를 방지한다.
	SightConfig->LoseSightRadius = 2000.0f;
	//좀비가 정면 기준으로 좌우 몇 도까지 볼 수 있는지(시야각)
	//예를 들어 90도로 지정해주면 총 정면에서부터 좌우 즉, 180도를 보는거다...
	SightConfig->PeripheralVisionAngleDegrees = 30.0f;

	//감지된 정보가 5초 동안은 유효하게 취급된다 - 타겟이 순간적으로 시야에서 가려져도
	//바로 놓친 것으로 처리하지 않아 부자연스러운 끊김을 방지해준다
	SightConfig->SetMaxAge(5.0f);

	//아직 팀(적/아군) 시스템이 없어서 '적'만 감지하게 하면 아무것도 안 걸릴 수 있음
	//안전하게 적/중립/아군 전부 감지 대상으로 켜둠
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	//방금 설정한 SightConfig를 AIPerception 컴포넌트에 등록
	AIPerception->ConfigureSense(*SightConfig);
	//여러 감각을 등록했을 때 대표(기준)로 삼을 감각을 시야로 지정
	//지금은 시야뿐이라 당장 큰 차이는 없지만, 나중에 청각 등을 추가할 때를 대비한 명시적 설정
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AZombieAIController::BeginPlay()
{
	Super::BeginPlay();

	//AIPerception이 유효한지 확인하는 안전장치
	if (AIPerception)
	{
		//AIPerception이 뭔가를 감지/갱신할 때마다(OnTargetPerceptionUpdated 이벤트가 발생할 때마다),
		//이 객체(this)의 OnPerceptionUpdated 함수가 자동으로 호출되도록 미리 등록해둠
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(
			//지금 이 코드가 실행되고 있는 객체 자신을 가리키는 포인터
			this,
			&AZombieAIController::OnPerceptionUpdated
		);
	}
	
}

//AI 컨트롤러가 특정 Pawn(좀비 캐릭터)에 빙의되는 바로 그 순간에 자동으로 호출되는 함수
void AZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	//BehaviorTreeAsset이 유효한지 확인하는 안전장치
	if (BehaviorTreeAsset)
	{
		//실제 BehaviorTree를 돌리기 시작하는 실행 명령
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

//AIPerception이 뭔가를 감지하거나 놓칠 때마다 호출된다. - 그 결과를 Blackboard의
//ChaseTarget 키에 반영해서, Behavior Tree가 추격할지 순찰할지 판단할 수 있게 해준다.
void AZombieAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	//블랙보드 컴포넌트를 찾지 못하면 아래까지 내려가지 않도록 여기서 끊어버린다.
	if (!BlackboardComp || !Actor) {
		return;
	}

	// 감지된 Actor가 Pawn인지 확인한다.
	APawn* DetectedPawn = Cast<APawn>(Actor);

	// Pawn이 아니거나 플레이어가 조종하는 Pawn이 아니라면
	// 추격 대상으로 사용하지 않고 무시한다.
	if (!DetectedPawn || !DetectedPawn->IsPlayerControlled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		//새로 감지됐으므로, 이 대상을 쫓아가야 한다고 ChaseTarget을 지정해준다.
		BlackboardComp->SetValueAsObject(TEXT("ChaseTarget"), Actor);
	}
	else if (BlackboardComp->GetValueAsObject(TEXT("ChaseTarget")) == Actor)
	{
		//대상을 잃어버렸으므로, ChaseTarget을 비워서 다시 순찰 상태로 돌아가게 한다.
		BlackboardComp->ClearValue(TEXT("ChaseTarget"));
	}
}

void AZombieAIController::StartAttack()
{
	//현재 실행 중인 Move To 등의 이동을 중단시킨다.
	StopMovement();

	//BT가 "좀비가 공격 중인지" 알 수 있게 Blackboard 값을 true로 바꾼다.
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		//IsAttacking = true "지금 공격 중이다"라는 상태를 Blackboard에 저장한다.
		BlackboardComp->SetValueAsBool(TEXT("IsAttacking"), true);
	}
}

//공격 애니메이션이 끝난 뒤 실행해서 "이제 공격이 끝났다"고 알려주는 역할이다. BT가 다시 이동/추적 판단을 할 수 있게 해준다.
void AZombieAIController::FinishAttack()
{
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsBool(TEXT("IsAttacking"), false);
	}
}

