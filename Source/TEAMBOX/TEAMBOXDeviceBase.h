// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXSwitchBase.h"
#include "TEAMBOXDeviceBase.generated.h"

class UArrowComponent;

/**
 * Base class for all devices in the puzzle system.
 * Devices subscribe to switches and activate/deactivate based on switch states.
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class TEAMBOX_API ATEAMBOXDeviceBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXDeviceBase();

	/** Array of switches that control this device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device", meta = (AllowedClasses = "TEAMBOXSwitchBase"))
	TArray<ATEAMBOXSwitchBase*> TargetSwitches;

	/** Whether all switches must be activated for device to activate (true) or any switch (false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device")
	bool bRequireAllSwitches;

	/** Get the current activation state of the device */
	UFUNCTION(BlueprintPure, Category = "Device")
	bool GetIsActive() const { return bIsActive; }

	/** Enable/disable visualization of connection lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device|Visualization")
	bool bShowConnectionLines;

	/** Color for active connection lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device|Visualization")
	FLinearColor ActiveLineColor;

	/** Color for inactive connection lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device|Visualization")
	FLinearColor InactiveLineColor;

	/** Color for unconnected switches (null reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device|Visualization")
	FLinearColor UnconnectedLineColor;

	/** Thickness of connection lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device|Visualization", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float ConnectionLineThickness;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Called when a switch state changes */
	UFUNCTION()
	void OnSwitchStateChanged(ATEAMBOXSwitchBase* Switch);

	/** Update device state based on switch states */
	void UpdateDeviceState();

	/** Activate the device (can be overridden in C++ or Blueprint) */
	UFUNCTION(BlueprintNativeEvent, Category = "Device")
	void ActivateDevice();
	virtual void ActivateDevice_Implementation();

	/** Deactivate the device (can be overridden in C++ or Blueprint) */
	UFUNCTION(BlueprintNativeEvent, Category = "Device")
	void DeactivateDevice();
	virtual void DeactivateDevice_Implementation();

	/** Draw connection lines to switches */
	void DrawConnectionLines();

	/** Current activation state */
	UPROPERTY(BlueprintReadOnly, Category = "Device")
	bool bIsActive;

	/** Whether device activation/deactivation is in progress (for async support) */
	UPROPERTY(BlueprintReadWrite, Category = "Device")
	bool bIsTransitioning;

	/** Set the transitioning state (called by Blueprint when animation completes) */
	UFUNCTION(BlueprintCallable, Category = "Device")
	void SetTransitioningComplete() { bIsTransitioning = false; }

private:
	/** Arrow component for visualization (optional, created if needed) */
	UPROPERTY()
	TArray<UArrowComponent*> ConnectionArrowComponents;
};
