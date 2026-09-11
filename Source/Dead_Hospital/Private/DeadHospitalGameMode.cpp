// Fill out your copyright notice in the Description page of Project Settings.


#include "DeadHospitalGameMode.h"

void ADeadHospitalGameMode::BeginPlay()
{
	// 부모 클래스인 AGameModeBase가 원래 수행해야 하는
    // 게임 시작 준비 작업을 먼저 실행합니다.
	Super::BeginPlay();

	// 레벨의 게임 플레이가 시작되었으므로
	// 현재 게임 진행 단계를 Waiting에서 Playing으로 변경합니다.
	CurrentGamePhase = EDeadHospitalGamePhase::Playing;

	/*작성한 BeginPlay 함수가 실제로 실행됐는지 확인하기 위해
	언리얼 에디터의 Output Log 창에 테스트 메세지를 출력합니다.
	UE_LOG(LogTemp, Warning, TEXT("Dead Hospital 게임이 시작되었습니다."));*/
}