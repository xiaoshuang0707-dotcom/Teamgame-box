// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXSwitchBase.generated.h"

/**
 * Base class for all switches in the puzzle system.
 * Switches can be activated/deactivated and broadcast events to connected devices.
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class TEAMBOX_API ATEAMBOXSwitchBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXSwitchBase();

	/** Initial state of the switch (can be configured in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	bool bInitialState;

	/** Get the current activation state of the switch */
	UFUNCTION(BlueprintPure, Category = "Switch")
	bool GetIsActivated() const { return bIsActivated; }

	/** Activate the switch (internal call, subclasses trigger this) */
	UFUNCTION(BlueprintCallable, Category = "Switch")
	void ActivateSwitch();

	/** Deactivate the switch (internal call, subclasses trigger this) */
	UFUNCTION(BlueprintCallable, Category = "Switch")
	void DeactivateSwitch();

	/** Reset the switch to its initial state */
	UFUNCTION(BlueprintCallable, Category = "Switch")
	void ResetSwitch();

	/** Event dispatched when switch is activated */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwitchActivated, ATEAMBOXSwitchBase*, Switch);
	UPROPERTY(BlueprintAssignable, Category = "Switch")
	FOnSwitchActivated OnSwitchActivated;

	/** Event dispatched when switch is deactivated */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwitchDeactivated, ATEAMBOXSwitchBase*, Switch);
	UPROPERTY(BlueprintAssignable, Category = "Switch")
	FOnSwitchDeactivated OnSwitchDeactivated;

protected:
	virtual void BeginPlay() override;

	/** Current activation state */
	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	bool bIsActivated;
};
