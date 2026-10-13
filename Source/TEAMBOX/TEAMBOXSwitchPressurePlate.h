// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TEAMBOXSwitchBase.h"
#include "TEAMBOXSwitchPressurePlate.generated.h"

class UBoxComponent;

/**
 * 压感开关：当玩家或任意物体（排除本 Actor 自身组件）与上方碰撞盒重叠时激活，无重叠时取消激活。
 * 碰撞盒需在蓝图中创建并通过 SetTriggerComponent 指定。
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXSwitchPressurePlate : public ATEAMBOXSwitchBase
{
	GENERATED_BODY()

public:
	ATEAMBOXSwitchPressurePlate();

	/** 在蓝图中创建的检测用 Box，通过 SetTriggerComponent 赋值 */
	UFUNCTION(BlueprintCallable, Category = "Pressure Plate")
	void SetTriggerComponent(UBoxComponent* InTriggerComponent) { TriggerComponent = InTriggerComponent; }

	UFUNCTION(BlueprintPure, Category = "Pressure Plate")
	UBoxComponent* GetTriggerComponent() const { return TriggerComponent; }

protected:
	virtual void BeginPlay() override;

	/** 有外部物体进入触发盒（排除本 Actor 的组件） */
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 有外部物体离开触发盒 */
	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 根据当前重叠数量更新开关状态 */
	void UpdateActivationFromOverlapCount();

private:
	UPROPERTY()
	UBoxComponent* TriggerComponent;

	/** 当前与触发盒重叠的“外部” Actor 集合（不含本 Actor），用于正确计数同一 Actor 多组件重叠 */
	TSet<TWeakObjectPtr<AActor>> OverlappingExternalActors;
};
