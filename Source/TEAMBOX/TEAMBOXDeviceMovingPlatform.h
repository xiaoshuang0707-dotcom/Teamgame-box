// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TEAMBOXDeviceBase.h"
#include "TEAMBOXDeviceMovingPlatform.generated.h"

class UBoxComponent;

/**
 * Device that moves back and forth between two positions when activated.
 * When activated: continuously moves between StartPosition and EndPosition
 * When deactivated: stops at current position
 * When reactivated: continues from current position in current direction
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXDeviceMovingPlatform : public ATEAMBOXDeviceBase
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXDeviceMovingPlatform();

	/** Collision component for movement sweep */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UBoxComponent* CollisionComponent;

	/** Runtime start world position captured from placed actor location at BeginPlay */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Platform")
	FVector StartPosition;

	/** End offset relative to placed start position (position B = StartPosition + EndPosition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform", meta = (MakeEditWidget = true, DisplayName = "End Offset"))
	FVector EndPosition;

	/** Movement speed in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform", meta = (ClampMin = "1.0", ClampMax = "10000.0"))
	float MovementSpeed;

	/** Pause duration at endpoints before reversing direction (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float PauseDuration;

	/** Whether to use smooth curve (ease in/out) for movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Platform")
	bool bUseSmoothCurve;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Override ActivateDevice to start/resume movement */
	virtual void ActivateDevice_Implementation() override;

	/** Override DeactivateDevice to stop movement */
	virtual void DeactivateDevice_Implementation() override;

	/** Update platform movement */
	void UpdateMovement(float DeltaTime);

	/** Current movement direction: true = moving to EndPosition, false = moving to StartPosition */
	bool bMovingToEnd;

	/** Whether platform is currently moving */
	bool bIsMoving;

	/** Time remaining in pause at endpoint */
	float PauseTimeRemaining;
};
