#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackoutScareTrigger.generated.h"

class UBoxComponent;
class AZombieCharacter;
class ACharacter;

// 블루프린트로 연출 종료를 알릴 이벤트 디스패처 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBlackoutScareFinished, ACharacter*, Player);

UCLASS()
class DEAD_HOSPITAL_API ABlackoutScareTrigger : public AActor
{
	GENERATED_BODY()
	
public:
    ABlackoutScareTrigger();

    // 외부(문)에서 겹침 없이 수동으로 연출을 시작할 때 호출
    UFUNCTION(BlueprintCallable, Category = "JumpScare|Event")
    void StartScare(ACharacter* Player);

    // 텔레포트 완료 후 조명을 켜고 액터를 파괴하기 위한 새 함수
    UFUNCTION(BlueprintCallable, Category = "JumpScare|Event")
    void ResetLightsAndDestroy();

    // 연출 마지막에 조명을 꺼진 상태로 둘지 여부 (JS06은 true)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    bool bKeepLightsOffAtEnd = false;

    // 연출 마지막에 이동 입력을 즉시 복구할지 여부 (JS06은 false)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    bool bRestoreInputWhenScareEnds = true;

    // 연출 종료 시 호출되는 이벤트 디스패처
    UPROPERTY(BlueprintAssignable, Category = "JumpScare|Event")
    FOnBlackoutScareFinished OnScareFinished;

protected:
    // 플레이어가 밟을 트리거
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    UBoxComponent* TriggerBox;

    // 귀신이 나타날 3가지 위치 (에디터에서 화살표로 직관적 배치 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* FarPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* MidPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Component")
    USceneComponent* ClosePoint;

    // 소환할 귀신(좀비) 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    TSubclassOf<AZombieCharacter> GhostClass;

    // 불이 켜져 있는 시간 (기본 0.4초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    float LightOnDuration = 0.4f;

    // 불이 꺼져 있는 시간 (기본 0.2초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Setting")
    float LightOffDuration = 0.2f;

    // 블루프린트에서 조명 끄고 켜기를 구현할 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "JumpScare|Event")
    void OnToggleLights(bool bTurnOn);

    // 맵에 배치된 조명 액터들을 담을 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Light")
    TArray<class ALight*> TargetLights;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Audio")
    class USoundBase* JumpScareSound;

    // 블루프린트에서 볼륨을 쉽게 조절하기 위한 변수 (기본값 0.5로 낮춤)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Audio")
    float JumpScareVolumeMultiplier = 0.5f;

    // 재생 중인 점프스케어 사운드를 추적하여 멈추기 위한 참조 변수
    UPROPERTY()
    class UAudioComponent* SpawnedJumpScareAudio = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Audio")
    class UAudioComponent* TensionAudioComp;

private:
    // 연출 단계를 진행하는 핵심 함수
    void AdvanceScareSequence();

    FTimerHandle SequenceTimerHandle;
    int32 CurrentStage = 0;
    AZombieCharacter* SpawnedGhost = nullptr;

    // 연출을 당하는 플레이어를 기억해두기 위한 변수
    UPROPERTY()
    ACharacter* TargetPlayer = nullptr;

};
