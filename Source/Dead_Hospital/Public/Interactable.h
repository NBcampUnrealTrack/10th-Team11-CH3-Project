// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AActor;

/**
 * Interface(인터페이스)는 "이 기능을 제공하겠다"는 공통 약속입니다.
 * 문, 키패드, 그림은 서로 다른 Actor라서 Player가 각각의 상세 코드를 알기 어렵습니다.
 * 대신 모두 IInteractable 약속을 지키면 Player는 대상을 보고 E 키를 눌렀을 때
 * "이 Actor와 상호작용할 수 있나?"만 물어보면 됩니다.
 *
 * Unreal에서는 아래 UInteractable과 IInteractable을 한 쌍으로 만듭니다.
 * UInteractable은 Unreal의 Blueprint/반영 시스템이 이 약속을 찾게 해 주는 부분이고,
 * 실제 함수 목록은 밑의 IInteractable에 적습니다. 둘 중 하나만 있는 구조가 아닙니다.
 */
UINTERFACE(BlueprintType)
class DEAD_HOSPITAL_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 이 아래의 세 함수는 상호작용 Actor라면 제공해야 하는 공통 질문/행동입니다.
 * 함수 이름 뒤에 붙는 _Implementation은 각 Actor가 실제 답을 적는 C++ 함수입니다.
 * 예를 들어 문은 Interact_Implementation에서 문을 열고, 그림은 그림을 떼어 냅니다.
 *
 * BlueprintNativeEvent는 C++ 기본 동작을 만들면서 Blueprint에서도 교체 가능한 형태입니다.
 * Player 담당 코드에서 Interface를 호출할 때는 대상 Actor에 대한
 * IInteractable::Execute_Interact(대상, Player)를 사용해야 합니다.
 * 현재 팀원의 Player 코드에 이 호출이 연결되어야 실제 E 키로 작동합니다.
 */
class DEAD_HOSPITAL_API IInteractable
{
	GENERATED_BODY()

public:
	/**
	 * E 키를 눌렀을 때 수행할 행동입니다. Interactor는 "누가 눌렀는가"를 뜻하며
	 * 보통 Player Actor가 들어옵니다. 아이템은 이를 이용해 Player의 인벤토리를 찾습니다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	/**
	 * true는 지금 E 키 사용 가능, false는 사용 불가입니다.
	 * 예를 들어 이미 가져간 열쇠나 GameOver 이후에는 false여야 중복 실행을 막습니다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** UI 안내 문구입니다. FText는 Unreal에서 화면에 표시할 글자를 담는 자료형입니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText() const;
};
