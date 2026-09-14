// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadHospitalLifeSupportDevice.generated.h"

class UStaticMeshComponent;

/**
 * 최종 구역의 생명유지장치를 나타내는 Actor입니다.
 * 플레이어 상호작용 파트가 완성되면 E 상호작용 성공 지점에서 TryShutdownDevice를 호출하면 됩니다.
 */
UCLASS()
class DEAD_HOSPITAL_API ADeadHospitalLifeSupportDevice : public AActor
{
	GENERATED_BODY()

public:
	ADeadHospitalLifeSupportDevice();

	/** 장치를 한 번만 종료하고 GameMode에 최종 목표 완료를 알립니다. */
	UFUNCTION(BlueprintCallable, Category = "Life Support")
	bool TryShutdownDevice();

	UFUNCTION(BlueprintPure, Category = "Life Support")
	bool IsDeviceShutdown() const { return DeviceShutdown; }

	/**
	 * 장치 종료가 승인된 순간 Blueprint에서 실행됩니다.
	 * 장치 정지 효과, 육신 사망, 귀신 성불 연출을 이 이벤트에 연결할 수 있습니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Life Support")
	void OnDeviceShutdownAccepted();

protected:
	/** Blueprint 자식에서 실제 생명유지장치 Mesh를 지정할 수 있는 기본 Component입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life Support")
	UStaticMeshComponent* DeviceMesh;

private:
	/** 중복 상호작용으로 성불 연출과 복귀 흐름이 두 번 실행되는 것을 막습니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Life Support", meta = (AllowPrivateAccess = "true"))
	bool DeviceShutdown = false;
};
