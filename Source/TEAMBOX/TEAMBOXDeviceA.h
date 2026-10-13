// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXDeviceA.generated.h"

class UArrowComponent;
class UNiagaraComponent;
class UBoxComponent;
class USoundBase;

UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXDeviceA : public AActor
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXDeviceA();

	/** Arrow component indicating the direction and starting point of the beam */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UArrowComponent* ArrowComponent;

	/** Niagara Beam component for the particle ray */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* BeamComponent;

	/** Maximum range of the beam (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A", meta = (ClampMin = "100.0", ClampMax = "100000.0"))
	float MaxBeamRange;

	/** Collision channel to use for line trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Enable debug visualization (draws debug lines and spheres) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A")
	bool bEnableDebugVisualization;

	/** Use local space coordinates for Niagara beam (if false, uses world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A")
	bool bUseLocalSpaceForBeam;

	// ========== Receive-to-Activate (新增) ==========

	/** true = 初始就发射光线；false = 初始不发射，需通过接收光线激活 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A|Activation")
	bool bInitiallyEmitting;

	/** true = 接收到一次光线后永久激活；false = 必须持续接收光线才保持激活，未接收时关闭 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A|Activation")
	bool bOneShotActivate;

	/** 接收器从未激活切换为激活时播放的音效（在 Details/BP 中配置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device A|Activation")
	USoundBase* ActivationSFX;

	/** 在 Blueprint 中创建的接收用 Box 组件，通过 SetReceiverComponent 赋值后才会参与“接收光线”检测 */
	UFUNCTION(BlueprintCallable, Category = "Device A|Beam Receiver")
	void SetReceiverComponent(UBoxComponent* InReceiverComponent) { ReceiverComponent = InReceiverComponent; }

	/** 返回接收器组件（可为 null） */
	UFUNCTION(BlueprintPure, Category = "Device A|Beam Receiver")
	UBoxComponent* GetReceiverComponent() const { return ReceiverComponent; }

	/** 当前是否处于激活状态（激活时发射光线） */
	UFUNCTION(BlueprintPure, Category = "Device A|Activation")
	bool IsActivated() const { return bIsActivated; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 根据接收光线更新激活状态（仅当非“初始发射”时有效） */
	void UpdateReceiveState();

	/** Update the beam based on line trace result（仅激活时调用） */
	void UpdateBeam();

private:
	/** Current beam start location */
	FVector BeamStartLocation;

	/** Current beam end location */
	FVector BeamEndLocation;

	/** 接收光线用的碰撞体，在 Blueprint 中创建并通过 SetReceiverComponent 赋值 */
	UPROPERTY()
	UBoxComponent* ReceiverComponent;

	/** 当前是否激活（激活时才发射并更新光束） */
	bool bIsActivated;

	/** 是否曾经接收到过光线（用于一次激活模式） */
	bool bHasEverReceivedBeam;
};
