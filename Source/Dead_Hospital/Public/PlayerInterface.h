#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerInterface.generated.h"

UINTERFACE(MinimalAPI)
class UPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

class DEAD_HOSPITAL_API IPlayerInterface
{
	GENERATED_BODY()

public:
	// 상호작용 처리(아이템 줍기, 문 열기)
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void Interact(AActor* Interactor);

	// 지금 상호작용 상태인지
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	bool CanInteract(AActor* Interactor) const;


	// 은신 중에도 허용되는 상호작용
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	bool IsAllowedWhileHiding() const;

	// 상호작용 프롬프트("[E] 문 열기" 등)에 표시할 텍스트
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	FText GetInteractionText(AActor* Interactor) const;


	// Interactor(플레이어)가 사망 등으로 강제로 상호작용을 중단해야 할 때 호출.
	// 기본 구현은 아무것도 하지 않으며, 상태를 들고 있는 액터(HidingSpotActor 등)가 오버라이드한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void ForceRelease(AActor* Interactor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interact")
	void OnInteractFailed(AActor* Interactor, const FText& Reason);
};
