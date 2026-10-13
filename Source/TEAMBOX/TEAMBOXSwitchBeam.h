// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TEAMBOXSwitchBase.h"
#include "TEAMBOXSwitchBeam.generated.h"

class UBoxComponent;
class ATEAMBOXDeviceA;
class ATEAMBOXEnergyCore;

/**
 * Switch that activates when hit by beams from DeviceA or EnergyCore.
 * Deactivates when no beams are detected.
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXSwitchBeam : public ATEAMBOXSwitchBase
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXSwitchBeam();

	/** Collision component for detecting beam hits */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UBoxComponent* DetectionBox;

	/** Collision channel to use for beam detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Switch")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Enable debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Switch")
	bool bEnableDebugVisualization;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Check if any beams are hitting this switch */
	void CheckBeamHits();

private:
	/** Whether a beam was detected in the last frame */
	bool bWasBeamDetected;
};
