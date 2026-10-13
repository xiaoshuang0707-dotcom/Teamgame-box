// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXEnergyCore.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;
class UArrowComponent;
class UNiagaraComponent;
class UPhysicsHandleComponent;

UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXEnergyCore : public AActor
{
	GENERATED_BODY()
	
public:	
	ATEAMBOXEnergyCore();

	/** Static mesh component for the energy core */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Mesh, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* EnergyCoreMesh;

	/** Collision component for interaction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Collision, meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionComponent;

	/** Physics Handle component for elastic movement - OLD SYSTEM (COMMENTED OUT) */
	// OLD SYSTEM: PhysicsHandle is no longer used in Kinematic Hold system
	// Keeping for backward compatibility but all calls are commented out
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Physics, meta = (AllowPrivateAccess = "true"))
	UPhysicsHandleComponent* PhysicsHandle;

	/** Reserved interface for future behaviors - can be implemented in Blueprint */
	UFUNCTION(BlueprintImplementableEvent, Category = "Energy Core")
	void OnEnergyCoreBehavior();

	/** Set the target character for position updates */
	void SetTargetCharacter(class ATEAMBOXCharacter* InCharacter);

	/** Update position to stay in front of camera using kinematic control */
	void UpdatePosition();

	/** Release the energy core (drop it) */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void Release();

	/** Grab the energy core using kinematic control */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void Grab();

	/** Check if this energy core is currently being held */
	UFUNCTION(BlueprintPure, Category = "Energy Core")
	bool IsBeingHeld() const;

	/** Reset the rotation to initial state (zero rotation) */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void ResetRotation();

	/** Rotate the energy core on Z axis (Roll) by 90 degrees */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void RotateZ(float Degrees);

	/** Rotate the energy core on Y axis (Pitch) by 90 degrees */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void RotateY(float Degrees);

	/** Adjust the target distance from camera (in cm) - will smoothly transition to new distance */
	UFUNCTION(BlueprintCallable, Category = "Energy Core")
	void SetTargetDistance(float NewDistance);

	/** Get the current target distance from camera (in cm) */
	UFUNCTION(BlueprintPure, Category = "Energy Core")
	float GetTargetDistance() const { return TargetDistance; }

	/** Minimum distance from camera (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core", meta = (ClampMin = "100.0", ClampMax = "10000.0"))
	float MinDistance;

	/** Maximum distance from camera (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core", meta = (ClampMin = "100.0", ClampMax = "10000.0"))
	float MaxDistance;

	/** Current target distance from camera (in cm) - used for smooth transitions */
	UPROPERTY(BlueprintReadOnly, Category = "Energy Core")
	float TargetDistance;

	/** Speed of distance transition (cm per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core", meta = (ClampMin = "10.0", ClampMax = "10000.0"))
	float DistanceTransitionSpeed;

	/** Spring stiffness for physics handle (higher = stiffer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float SpringStiffness;

	/** Damping for physics handle (higher = more damping) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float Damping;

	/** Maximum range of the outgoing beam (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Receiver", meta = (ClampMin = "100.0", ClampMax = "100000.0"))
	float MaxBeamRange;

	/** Collision channel to use for line trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Receiver")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Enable debug visualization (draws debug lines and shows receiver collision) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Receiver")
	bool bEnableBeamDebugVisualization;

	/** Use local space coordinates for Niagara beam (if false, uses world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam Receiver")
	bool bUseLocalSpaceForBeam;

	// ========== Rotation Constraint System Parameters ==========
	
	/** Follow speed for free state rotation response (higher = faster response) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Constraint", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float FollowSpeed;

	/** Block angle threshold - angle error to detect blocking (in degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Constraint", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float BlockAngle;

	/** Block angular velocity threshold - angular velocity to detect rotation failure (in degrees per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Constraint", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float BlockAngularVelocity;

	/** Recover speed - alignment speed after leaving wall (lower = slower recovery) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Constraint", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float RecoverSpeed;

	/** Enable debug visualization for rotation constraint system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Constraint")
	bool bEnableRotationDebugVisualization;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Check if receiver is being hit by any beam and update outgoing beam */
	void UpdateBeamReceiver();

	// OLD SYSTEM: UpdateRotationConstraint is replaced by direct kinematic control
	// void UpdateRotationConstraint(float DeltaTime); // COMMENTED OUT

private:
	/** Reference to the character that spawned this energy core */
	UPROPERTY()
	class ATEAMBOXCharacter* TargetCharacter;

	/** Distance from camera (in cm) */
	float DistanceFromCamera;

	/** Current target location for physics handle */
	FVector TargetLocation;

	/** Accumulated rotation stored as quaternion to avoid gimbal lock */
	UPROPERTY()
	FQuat AccumulatedRotation;

	/** Whether this energy core is currently being held by a character */
	bool bIsBeingHeld;

	/** Whether the receiver is currently being hit by a beam */
	bool bIsReceivingBeam;

	/** Current beam start location */
	FVector BeamStartLocation;

	/** Current beam end location */
	FVector BeamEndLocation;

	// ========== OLD ROTATION CONSTRAINT SYSTEM (COMMENTED OUT) ==========
	// OLD SYSTEM: All rotation constraint logic is replaced by Kinematic Look-At
	// Keeping for reference but not used
	
	/*
	// Desired rotation (Look-At camera position, calculated each frame)
	FQuat DesiredRotation;

	// Target rotation (what physics handle is trying to achieve)
	FQuat TargetRotation;

	// Current rotation constraint state
	enum class ERotationConstraintState
	{
		FreeFollow,      // Free following state
		Blocked,         // Blocked by obstacle
		Recover          // Recovering from block
	};

	// Current state of rotation constraint
	ERotationConstraintState RotationConstraintState;

	// Time spent in current state (for state transition delays)
	float StateTime;

	// Previous angular velocity (for change detection)
	float PreviousAngularVelocity;

	// Block detection frame count (to avoid single-frame false positives)
	int32 BlockDetectionFrameCount;
	*/

	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	
	/** Hold state enumeration */
	enum class EHoldState
	{
		Idle,                // Not held, physics simulation active
		Held_Kinematic,      // Held using kinematic control
		Released_Physics     // Just released, physics simulation active
	};

	/** Current hold state */
	EHoldState HoldState;

	/** Hold distance from camera (in cm) - clamped between MinDistance and MaxDistance */
	float HoldDistance;

	/** Current rotation (used for smooth interpolation to Look-At) */
	FQuat CurrentRotation;

	/** Time since grab (used for smooth transition) */
	float GrabTime;

	/** Local direction vector that should point to camera (recorded at grab moment) */
	/** This is the direction in object's local space that was facing the camera when grabbed */
	/** For example: if A-face was facing camera at grab, this vector points to A-face direction */
	FVector LocalLookAtDirection;

	/** Initial pitch rotation at grab moment (to maintain vertical orientation) */
	float InitialPitch;

	/** Target rotation for smooth rotation animation (Q/E keys) */
	FQuat TargetRotationForAnimation;

	/** Whether rotation animation is in progress */
	bool bIsRotating;

	/** Rotation animation progress (0.0 to 1.0) */
	float RotationAnimationProgress;

	/** Starting rotation when animation began (for interpolation) */
	FQuat AnimationStartRotation;

public:
	/** Horizontal offset from camera forward (in cm) - for future use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematic Hold", meta = (ClampMin = "-1000.0", ClampMax = "1000.0"))
	float HorizontalOffset;

	/** Vertical offset from camera forward (in cm) - for future use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematic Hold", meta = (ClampMin = "-1000.0", ClampMax = "1000.0"))
	float VerticalOffset;

	/** Speed of rotation transition to Look-At state (higher = faster transition, in degrees per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematic Hold", meta = (ClampMin = "1.0", ClampMax = "720.0"))
	float LookAtTransitionSpeed;

	/** Rotation animation duration for Q/E keys (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematic Hold|Rotation", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float RotationAnimationDuration;
	/** Returns EnergyCoreMesh subobject */
	UStaticMeshComponent* GetEnergyCoreMesh() const { return EnergyCoreMesh; }
	
	/** Returns CollisionComponent subobject */
	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }

	// Note: ReceiverComponent, BeamStartComponent, and BeamComponent are private members
	// Use the setter functions in Blueprint to assign components created in Blueprint

	/** Set the receiver component (call this in Blueprint to assign the component created in Blueprint) */
	UFUNCTION(BlueprintCallable, Category = "Beam Receiver")
	void SetReceiverComponent(UBoxComponent* InReceiverComponent) { ReceiverComponent = InReceiverComponent; }

	/** Set the beam start component (call this in Blueprint to assign the component created in Blueprint) */
	UFUNCTION(BlueprintCallable, Category = "Beam Receiver")
	void SetBeamStartComponent(UArrowComponent* InBeamStartComponent) { BeamStartComponent = InBeamStartComponent; }

	/** Set the beam component (call this in Blueprint to assign the component created in Blueprint) */
	UFUNCTION(BlueprintCallable, Category = "Beam Receiver")
	void SetBeamComponent(UNiagaraComponent* InBeamComponent) { BeamComponent = InBeamComponent; }

	/** Returns ReceiverComponent subobject */
	UBoxComponent* GetReceiverComponent() const { return ReceiverComponent; }

	/** Returns BeamStartComponent subobject */
	UArrowComponent* GetBeamStartComponent() const { return BeamStartComponent; }

	/** Returns BeamComponent subobject */
	UNiagaraComponent* GetBeamComponent() const { return BeamComponent; }

	/** Check if this energy core is currently receiving a beam */
	UFUNCTION(BlueprintPure, Category = "Beam Receiver")
	bool IsReceivingBeam() const { return bIsReceivingBeam; }

private:
	/** Receiver box collision component for detecting incoming beams (set via SetReceiverComponent in Blueprint) */
	UPROPERTY()
	UBoxComponent* ReceiverComponent;

	/** Arrow component indicating the beam start direction (set via SetBeamStartComponent in Blueprint) */
	UPROPERTY()
	UArrowComponent* BeamStartComponent;

	/** Niagara Beam component for the outgoing beam (set via SetBeamComponent in Blueprint) */
	UPROPERTY()
	UNiagaraComponent* BeamComponent;
};
