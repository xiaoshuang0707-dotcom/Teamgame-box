// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TEAMBOXDeviceBase.h"
#include "TEAMBOXDeviceDoor.generated.h"

class UBoxComponent;

/**
 * Device that moves between two positions based on activation state.
 * When activated: moves from StartPosition to EndPosition
 * When deactivated: moves from EndPosition back to StartPosition
 * Movement can be interrupted and reversed immediately when state changes.
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXDeviceDoor : public ATEAMBOXDeviceBase
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXDeviceDoor();

	/** Collision component for movement sweep */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UBoxComponent* CollisionComponent;

	/** Runtime start world position captured from placed actor location at BeginPlay */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	FVector StartPosition;

	/** End offset relative to placed start position (position B = StartPosition + EndPosition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (MakeEditWidget = true, DisplayName = "End Offset"))
	FVector EndPosition;

	/** Movement speed in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (ClampMin = "1.0", ClampMax = "10000.0"))
	float MovementSpeed;

	/** Whether to use smooth curve (ease in/out) for movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bUseSmoothCurve;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Override ActivateDevice to start movement to EndPosition */
	virtual void ActivateDevice_Implementation() override;

	/** Override DeactivateDevice to start movement to StartPosition */
	virtual void DeactivateDevice_Implementation() override;

	/** Update door movement */
	void UpdateMovement(float DeltaTime);

	/** Current target position */
	FVector CurrentTargetPosition;

	/** Whether door is currently moving */
	bool bIsMoving;
};
