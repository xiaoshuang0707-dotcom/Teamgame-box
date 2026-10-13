// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TEAMBOXSwitchBase.h"
#include "TEAMBOXSwitchLever.generated.h"

class UBoxComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * 拉杆开关：玩家与碰撞盒重叠时按交互键（G）切换激活/取消激活。
 * 碰撞盒需在蓝图中创建并通过 SetTriggerComponent 指定。
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXSwitchLever : public ATEAMBOXSwitchBase
{
	GENERATED_BODY()

public:
	ATEAMBOXSwitchLever();

	/** 在蓝图中创建的检测用 Box，通过 SetTriggerComponent 赋值 */
	UFUNCTION(BlueprintCallable, Category = "Lever Switch")
	void SetTriggerComponent(UBoxComponent* InTriggerComponent) { TriggerComponent = InTriggerComponent; }

	UFUNCTION(BlueprintPure, Category = "Lever Switch")
	UBoxComponent* GetTriggerComponent() const { return TriggerComponent; }

	/** 交互时调用：激活则取消激活，未激活则激活 */
	UFUNCTION(BlueprintCallable, Category = "Lever Switch")
	void ToggleSwitch();

	/** 交互成功切换状态时播放的音效（在 Details/BP 中配置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever Switch|Audio")
	USoundBase* ToggleSFX;

protected:
	virtual void BeginPlay() override;

	/** 根据当前开关状态同步子物体 SM_PullSwitch_02 的相对旋转（X: 关=0, 开=90） */
	void SyncLeverMeshRotationWithState();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UPROPERTY()
	UBoxComponent* TriggerComponent;

	UPROPERTY()
	UStaticMeshComponent* LeverMeshComponent;
};
