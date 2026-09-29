#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerCharacterController.generated.h"

class UInputMappingContext;
class UInputAction;


UCLASS()
class DEAD_HOSPITAL_API APlayerCharacterController : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerCharacterController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* SitAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* SprintAction;

	// E키 상호작용용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* InteractAction;

	// F키 손전등용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* FlashlightAction;

	// I키 인벤토리용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* InventoryAction;
	
	// J키 문서창용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* DocumentAction;

	// O키 - 조합창 열기 / 전환
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* CraftAction;

	// ESC키 - 현재 열려 있는 메뉴 UI 닫기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* CloseUIAction;

	// P키 - 게임 일시정지 / 해제
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* PauseAction;

	// 1/2/3키 퀵슬롯용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* QuickSlot1Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* QuickSlot2Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* QuickSlot3Action;

	// R키 재장전용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* ReloadAction;

	// 마우스 좌클릭 총발사(전투)용 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* FireAction;

	// UI 열림/닫힘에 따라 마우스 커서 표시 + Input Mode 전환
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetUIInputMode(bool bUIOpen);

	virtual void BeginPlay() override;
	
};
