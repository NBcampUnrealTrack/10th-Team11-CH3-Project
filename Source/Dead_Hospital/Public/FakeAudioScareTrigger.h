#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FakeAudioScareTrigger.generated.h"

class UBoxComponent;
class UAudioComponent;
class USoundBase;

UCLASS()
class DEAD_HOSPITAL_API AFakeAudioScareTrigger : public AActor
{
	GENERATED_BODY()
	
public:
    AFakeAudioScareTrigger();

protected:
    // 소리가 실제로 들려올 위치 (문짝, 혹은 빈 방의 구석)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Components")
    USceneComponent* SoundOrigin;

    // 밟으면 소리가 시작됨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Components")
    UBoxComponent* PlayTrigger;

    // 밟으면 소리가 멈춤 (빈 방에 도착했을 때)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Components")
    UBoxComponent* StopTrigger;

    // 콧노래처럼 계속 재생할 때 사용할 오디오 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpScare|Components")
    UAudioComponent* AudioComp;

    // 재생할 공포 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Settings")
    USoundBase* ScareSound;

    // true면 쾅 소리 한 번만 재생(문 닫힘), false면 루프 재생(콧노래)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpScare|Settings")
    bool bIsOneShot = true;

    UFUNCTION()
    void OnPlayTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnStopTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
