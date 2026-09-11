// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DeadHospitalGameMode.generated.h"

/**
 * 게임이 현재 어떤 단계에 있는지 구분하기 위한 열거형입니다.
 * 게임은 아래에 작성한 단계 중 하나의 단계에만 속할 수 있습니다.
 */
UENUM(BlueprintType)
enum class EDeadHospitalGamePhase : uint8
{
    Waiting,   //게임 시작 전
    Playing,   //병원을 탐색하는 일반 플레이
    BossBattle,   //보스전 진행 중
    Escape,   //보스 처치 후 제한 시간 탈출 중
    Ending,   //최종 탈출문을 열고 엔딩 연출 중
    GameOver,   //사망 또는 시간 초과로 실패
    Cleared   //엔딩이 끝나고 최종 결과가 확정됨
};
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	/*
     * 레벨에 배치된 액터들의 준비가 끝나고 게임 플레이가 시작될 때
     * 언리얼 엔진이 자동으로 한 번 호출하는 함수입니다.
     */
	virtual void BeginPlay() override;

private:
    /**
     * 현재 게임이 어떤 진행 단계에 있는지 저장합니다.
     * 게임 모드가 처음 생성됐을 때는 아직 플레이가 시작되지 않았으므로
     * 기본값을 Waiting으로 설정합니다.
     */
    EDeadHospitalGamePhase CurrentGamePhase = EDeadHospitalGamePhase::Waiting;

};
