// Fill out your copyright notice in the Description page of Project Settings.

#include "DeadHospitalLifeSupportDevice.h"

#include "DeadHospitalGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ADeadHospitalLifeSupportDevice::ADeadHospitalLifeSupportDevice()
{
	PrimaryActorTick.bCanEverTick = false;

	DeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
	SetRootComponent(DeviceMesh);
}

bool ADeadHospitalLifeSupportDevice::TryShutdownDevice()
{
	if (DeviceShutdown)
	{
		return false;
	}

	ADeadHospitalGameMode* DeadHospitalGameMode = GetWorld()->GetAuthGameMode<ADeadHospitalGameMode>();
	if (!IsValid(DeadHospitalGameMode) || !DeadHospitalGameMode->CompleteLifeSupportShutdown())
	{
		return false;
	}

	DeviceShutdown = true;
	OnDeviceShutdownAccepted();
	return true;
}
