// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXEnergyCore.h"
#include "TEAMBOXCharacter.h"
#include "TEAMBOXDeviceA.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "NiagaraComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ATEAMBOXEnergyCore::ATEAMBOXEnergyCore()
{
	PrimaryActorTick.bCanEverTick = true;
	TargetCharacter = nullptr;
	DistanceFromCamera = 500.0f; // Default 5 meters
	MinDistance = 300.0f; // 3 meters
	MaxDistance = 1000.0f; // 10 meters
	TargetDistance = 500.0f; // Initial target distance 5 meters
	DistanceTransitionSpeed = 500.0f; // 5 meters per second transition speed
	// Spring stiffness and damping for physics handle
	// Higher SpringStiffness = more rigid (less flexible)
	// Higher Damping = less sensitive (faster settling, less oscillation)
	SpringStiffness = 5000.0f; // Increased from 2000.0f for more rigidity
	Damping = 200.0f; // Increased from 100.0f for less sensitivity
	
	// OLD ROTATION CONSTRAINT SYSTEM (COMMENTED OUT)
	/*
	FollowSpeed = 16.0f;
	BlockAngle = 20.0f;
	BlockAngularVelocity = 8.0f;
	RecoverSpeed = 6.0f;
	bEnableRotationDebugVisualization = true;
	RotationConstraintState = ERotationConstraintState::FreeFollow;
	StateTime = 0.0f;
	PreviousAngularVelocity = 0.0f;
	BlockDetectionFrameCount = 0;
	DesiredRotation = FQuat::Identity;
	TargetRotation = FQuat::Identity;
	*/
	
	// NEW KINEMATIC HOLD SYSTEM
	HoldState = EHoldState::Idle;
	HoldDistance = 500.0f; // Default 5 meters
	HorizontalOffset = 0.0f;
	VerticalOffset = 0.0f;
	LookAtTransitionSpeed = 180.0f; // Default: 180 degrees per second (3 seconds for 180° rotation)
	RotationAnimationDuration = 0.1f; // Default: 0.1 seconds for Q/E rotation
	CurrentRotation = FQuat::Identity;
	GrabTime = 0.0f;
	LocalLookAtDirection = FVector::ForwardVector; // Default: forward axis (will be updated at grab)
	InitialPitch = 0.0f; // Initial pitch at grab moment
	TargetRotationForAnimation = FQuat::Identity;
	AnimationStartRotation = FQuat::Identity;
	bIsRotating = false;
	RotationAnimationProgress = 0.0f;
	bIsBeingHeld = false;
	AccumulatedRotation = FQuat::Identity; // Kept for backward compatibility
	
	// Beam receiver defaults
	MaxBeamRange = 10000.0f; // 100 meters in cm
	TraceChannel = ECC_Visibility;
	bEnableBeamDebugVisualization = false;
	bUseLocalSpaceForBeam = false;
	bIsReceivingBeam = false;
	
	// Initialize component pointers to nullptr (will be set in Blueprint)
	ReceiverComponent = nullptr;
	BeamStartComponent = nullptr;
	BeamComponent = nullptr;

	// Create static mesh component (will be root component)
	EnergyCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyCoreMesh"));
	RootComponent = EnergyCoreMesh;
	
	// Set up collision for the mesh component - QueryAndPhysics for kinematic control
	// Collision must be enabled for sweep detection when using SetWorldLocationAndRotation
	EnergyCoreMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	EnergyCoreMesh->SetCollisionObjectType(ECC_PhysicsBody);
	EnergyCoreMesh->SetCollisionResponseToAllChannels(ECR_Block);
	EnergyCoreMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	EnergyCoreMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	
	// Start with physics simulation enabled (will be disabled when held)
	EnergyCoreMesh->SetSimulatePhysics(true);
	EnergyCoreMesh->SetEnableGravity(true); // Gravity enabled by default
	// Default damping values (not used in kinematic mode, but kept for physics mode)
	EnergyCoreMesh->SetLinearDamping(0.5f);
	EnergyCoreMesh->SetAngularDamping(1.0f);
	
	// Create optional collision component (can be used in Blueprint if needed)
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(50.0f);
	CollisionComponent->SetupAttachment(RootComponent);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disabled by default, enable in Blueprint if needed

	// OLD SYSTEM: PhysicsHandle is no longer used in Kinematic Hold system
	// Keeping for backward compatibility but all calls are commented out
	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
}

void ATEAMBOXEnergyCore::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize accumulated rotation from current mesh rotation
	if (EnergyCoreMesh != nullptr)
	{
		AccumulatedRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
		AccumulatedRotation.Normalize();
	}
	
	// Don't auto-grab here - SetTargetCharacter will handle it
	// This ensures TargetCharacter is set before we try to grab
	
	// Call the reserved behavior interface
	OnEnergyCoreBehavior();
}

void ATEAMBOXEnergyCore::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Update position to stay in front of camera
	UpdatePosition();
	
	// Update beam receiver (check for incoming beams and emit outgoing beam)
	UpdateBeamReceiver();
}

void ATEAMBOXEnergyCore::SetTargetCharacter(ATEAMBOXCharacter* InCharacter)
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("[SET_TARGET_CHARACTER] Called on energy core: %s, Character: %s"), 
		*GetName(), InCharacter ? *InCharacter->GetName() : TEXT("nullptr"));
	
	TargetCharacter = InCharacter;
	
	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	// If we have a target character and haven't been grabbed yet, grab it now
	// This handles the case when summoned by F key
	if (TargetCharacter != nullptr && !bIsBeingHeld && EnergyCoreMesh)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("[SET_TARGET_CHARACTER] Auto-grabbing energy core after SetTargetCharacter"));
		
		// Call Grab() to initialize kinematic hold system
		Grab();
		
		UE_LOG(LogTemplateCharacter, Log, TEXT("[SET_TARGET_CHARACTER] Energy core auto-grabbed, bIsBeingHeld: %d"), bIsBeingHeld);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("[SET_TARGET_CHARACTER] Cannot auto-grab: TargetCharacter=%d, bIsBeingHeld=%d, EnergyCoreMesh=%d"),
			TargetCharacter != nullptr, bIsBeingHeld, EnergyCoreMesh != nullptr);
	}
	
	// ========== OLD PHYSICS HANDLE SYSTEM (COMMENTED OUT) ==========
	/*
	if (TargetCharacter != nullptr && !bIsBeingHeld && PhysicsHandle && EnergyCoreMesh)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Auto-grabbing energy core after SetTargetCharacter"));
		
		// Initialize rotation constraint system
		if (EnergyCoreMesh != nullptr)
		{
			FQuat ObjectRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
			TargetRotation = ObjectRotation;
			DesiredRotation = ObjectRotation;
			
			RotationConstraintState = ERotationConstraintState::FreeFollow;
			StateTime = 0.0f;
			PreviousAngularVelocity = 0.0f;
			BlockDetectionFrameCount = 0;
		}
		
		AccumulatedRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
		AccumulatedRotation.Normalize();
		
		TargetDistance = 500.0f;
		DistanceFromCamera = 500.0f;
		
		EnergyCoreMesh->SetEnableGravity(false);
		EnergyCoreMesh->SetLinearDamping(Damping);
		EnergyCoreMesh->SetAngularDamping(FMath::Min(Damping * 0.1f, 20.0f));
		
		if (EnergyCoreMesh->GetBodyInstance())
		{
			EnergyCoreMesh->SetMassOverrideInKg(NAME_None, 10.0f, true);
		}
		
		PhysicsHandle->GrabComponentAtLocation(EnergyCoreMesh, NAME_None, GetActorLocation());
		bIsBeingHeld = true;
	}
	*/
}

void ATEAMBOXEnergyCore::UpdatePosition()
{
	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	// Only update if in kinematic hold state
	if (HoldState != EHoldState::Held_Kinematic || TargetCharacter == nullptr || EnergyCoreMesh == nullptr)
	{
		return;
	}

	UCameraComponent* CameraComponent = TargetCharacter->GetFirstPersonCameraComponent();
	if (CameraComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Get camera transform
	FVector CameraLocation = CameraComponent->GetComponentLocation();
	FVector CameraForward = CameraComponent->GetForwardVector();
	FVector CameraRight = CameraComponent->GetRightVector();
	FVector CameraUp = CameraComponent->GetUpVector();

	// Step 1: Smoothly transition HoldDistance towards TargetDistance
	float DeltaTime = World->GetDeltaSeconds();
	float DistanceDiff = TargetDistance - HoldDistance;
	float MaxDistanceChange = DistanceTransitionSpeed * DeltaTime;
	
	if (FMath::Abs(DistanceDiff) > MaxDistanceChange)
	{
		HoldDistance += FMath::Sign(DistanceDiff) * MaxDistanceChange;
	}
	else
	{
		HoldDistance = TargetDistance;
	}
	
	// Clamp hold distance
	HoldDistance = FMath::Clamp(HoldDistance, MinDistance, MaxDistance);
	
	// Update DistanceFromCamera for backward compatibility
	DistanceFromCamera = HoldDistance;

	// Step 2: Calculate target position
	// TargetPos = CameraLocation + CameraForward * HoldDistance + CameraRight * HorizontalOffset + CameraUp * VerticalOffset
	FVector TargetPos = CameraLocation 
		+ (CameraForward * HoldDistance)
		+ (CameraRight * HorizontalOffset)
		+ (CameraUp * VerticalOffset);

	// Step 3: Perform line trace to check for obstacles (wall detection)
	// Use line trace to detect walls before moving
	FVector CurrentLocation = EnergyCoreMesh->GetComponentLocation();
	FVector MoveDirection = TargetPos - CurrentLocation;
	float MoveDistance = MoveDirection.Size();
	
	// Only trace if we need to move
	if (MoveDistance > 0.1f)
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(TargetCharacter);
		QueryParams.bTraceComplex = true;
		
		// Get mesh bounds for trace offset
		FVector MeshExtent = EnergyCoreMesh->Bounds.BoxExtent;
		float MeshRadius = FMath::Max3(MeshExtent.X, MeshExtent.Y, MeshExtent.Z);
		
		// Trace from camera to target position
		FVector TraceStart = CameraLocation + (CameraForward * MeshRadius);
		FVector TraceEnd = TargetPos;
		
		bool bHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_WorldStatic, // Use WorldStatic channel for wall detection
			QueryParams
		);
		
		if (bHit && HitResult.bBlockingHit)
		{
			// Hit a wall - place object at hit location minus mesh radius
			TargetPos = HitResult.Location - (CameraForward * MeshRadius);
			
			// Ensure we don't go behind the camera
			FVector ToTarget = TargetPos - CameraLocation;
			float DistanceToTarget = ToTarget.Size();
			float MinDistanceFromCamera = 50.0f; // Minimum 50cm from camera
			if (DistanceToTarget < MinDistanceFromCamera)
			{
				TargetPos = CameraLocation + (CameraForward * MinDistanceFromCamera);
			}
		}
	}

	// Step 4: Calculate target Look-At rotation (horizontal only - Yaw rotation)
	// Key: Only apply horizontal Look-At (left/right), maintain vertical orientation (Pitch) from grab moment
	FVector ObjectLocation = EnergyCoreMesh->GetComponentLocation();
	FVector DirectionToCamera = CameraLocation - ObjectLocation;
	float DistanceToCamera = DirectionToCamera.Size();
	
	FQuat TargetLookAtRotation;
	if (DistanceToCamera > 1.0f) // At least 1cm away
	{
		DirectionToCamera.Normalize();
		
		// Project DirectionToCamera to horizontal plane (X-Y plane, Z=0)
		// This gives us the horizontal direction to camera (for Yaw rotation only)
		FVector HorizontalDirectionToCamera = FVector(DirectionToCamera.X, DirectionToCamera.Y, 0.0f);
		float HorizontalDistance = HorizontalDirectionToCamera.Size();
		
		if (HorizontalDistance > 0.01f) // Valid horizontal direction
		{
			HorizontalDirectionToCamera.Normalize();
			
			// Calculate where LocalLookAtDirection currently points in world space
			FVector CurrentLocalDirectionWorld = CurrentRotation.RotateVector(LocalLookAtDirection);
			CurrentLocalDirectionWorld.Normalize();
			
			// Project current local direction to horizontal plane
			FVector CurrentHorizontalDirection = FVector(CurrentLocalDirectionWorld.X, CurrentLocalDirectionWorld.Y, 0.0f);
			float CurrentHorizontalDistance = CurrentHorizontalDirection.Size();
			
			if (CurrentHorizontalDistance > 0.01f)
			{
				CurrentHorizontalDirection.Normalize();
				
				// Find the rotation that aligns CurrentHorizontalDirection to HorizontalDirectionToCamera (Yaw only)
				FQuat HorizontalAlignRotation = FQuat::FindBetweenNormals(CurrentHorizontalDirection, HorizontalDirectionToCamera);
				HorizontalAlignRotation.Normalize();
				
				// Apply horizontal alignment rotation to current rotation
				FQuat HorizontalTargetRotation = HorizontalAlignRotation * CurrentRotation;
				HorizontalTargetRotation.Normalize();
				
				// Extract Yaw from horizontal target rotation
				FRotator HorizontalTargetRotator = HorizontalTargetRotation.Rotator();
				
				// Combine: Use Yaw from horizontal Look-At, but keep Pitch and Roll from initial grab
				FRotator FinalRotator;
				FinalRotator.Yaw = HorizontalTargetRotator.Yaw; // Horizontal Look-At (left/right)
				FinalRotator.Pitch = InitialPitch; // Maintain vertical orientation from grab
				FinalRotator.Roll = HorizontalTargetRotator.Roll; // Keep roll from current rotation (or could use initial)
				
				TargetLookAtRotation = FinalRotator.Quaternion();
				TargetLookAtRotation.Normalize();
			}
			else
			{
				// Current local direction is vertical, use current rotation with initial pitch
				FRotator CurrentRotator = CurrentRotation.Rotator();
				CurrentRotator.Pitch = InitialPitch;
				TargetLookAtRotation = CurrentRotator.Quaternion();
				TargetLookAtRotation.Normalize();
			}
		}
		else
		{
			// Camera is directly above/below, maintain current rotation with initial pitch
			FRotator CurrentRotator = CurrentRotation.Rotator();
			CurrentRotator.Pitch = InitialPitch;
			TargetLookAtRotation = CurrentRotator.Quaternion();
			TargetLookAtRotation.Normalize();
		}
	}
	else
	{
		// Too close, use current rotation with initial pitch
		FRotator CurrentRotator = CurrentRotation.Rotator();
		CurrentRotator.Pitch = InitialPitch;
		TargetLookAtRotation = CurrentRotator.Quaternion();
		TargetLookAtRotation.Normalize();
	}

	// Step 5: Handle rotation animation (Q/E keys) if in progress
	if (bIsRotating)
	{
		// Update rotation animation progress
		RotationAnimationProgress += DeltaTime / RotationAnimationDuration;
		
		if (RotationAnimationProgress >= 1.0f)
		{
			// Animation complete
			RotationAnimationProgress = 1.0f;
			bIsRotating = false;
			CurrentRotation = TargetRotationForAnimation;
			CurrentRotation.Normalize();
			
			// CRITICAL: After E key rotation, we need to update LocalLookAtDirection
			// to reflect the new face that is pointing to the camera
			// This ensures Look-At system continues to work correctly after rotation
			FVector NewDirectionToCamera = CameraLocation - ObjectLocation;
			float NewDistanceToCamera = NewDirectionToCamera.Size();
			
			if (NewDistanceToCamera > 1.0f)
			{
				NewDirectionToCamera.Normalize();
				FQuat ObjectRotation = CurrentRotation;
				LocalLookAtDirection = ObjectRotation.Inverse().RotateVector(NewDirectionToCamera);
				LocalLookAtDirection.Normalize();
				
				// Also update InitialPitch to maintain vertical orientation
				FRotator NewRotator = CurrentRotation.Rotator();
				InitialPitch = NewRotator.Pitch;
				
				UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATION] Animation complete"));
				UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATION] Updated LocalLookAtDirection: X=%.4f Y=%.4f Z=%.4f"), 
					LocalLookAtDirection.X, LocalLookAtDirection.Y, LocalLookAtDirection.Z);
				UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATION] Updated InitialPitch: %.2f"), InitialPitch);
			}
		}
		else
		{
			// Interpolate rotation during animation
			// Use AnimationStartRotation (stored when animation began) as the start rotation
			CurrentRotation = FQuat::Slerp(AnimationStartRotation, TargetRotationForAnimation, RotationAnimationProgress);
			CurrentRotation.Normalize();
			
			// During animation, skip Look-At calculation (we'll use CurrentRotation directly)
		}
	}
	else
	{
		// Step 6: Smoothly interpolate from current rotation to Look-At rotation (only when not animating)
		// Calculate angular distance between current and target rotation
		FQuat RotationDiff = CurrentRotation.Inverse() * TargetLookAtRotation;
		RotationDiff.Normalize();
		float AngleDiff = FMath::RadiansToDegrees(2.0f * FMath::Acos(FMath::Clamp(FMath::Abs(RotationDiff.W), 0.0f, 1.0f)));
		
		// Calculate interpolation alpha based on transition speed
		// Transition speed is in degrees per second
		float MaxRotationChange = LookAtTransitionSpeed * DeltaTime; // Maximum rotation change this frame
		float InterpAlpha = 1.0f;
		
		if (AngleDiff > 0.1f) // Only interpolate if there's a significant difference
		{
			// Calculate how much we should rotate this frame
			InterpAlpha = FMath::Clamp(MaxRotationChange / AngleDiff, 0.0f, 1.0f);
		}
		
		// Spherical interpolation from current rotation to target Look-At rotation
		FQuat InterpolatedRotation = FQuat::Slerp(CurrentRotation, TargetLookAtRotation, InterpAlpha);
		InterpolatedRotation.Normalize();
		
		// Update current rotation for next frame
		CurrentRotation = InterpolatedRotation;
	}
	
	// Convert to rotator for SetWorldLocationAndRotation
	FRotator FinalRotation = CurrentRotation.Rotator();

	// Step 6: Apply transform using SetWorldLocationAndRotation with sweep
	// Sweep=true ensures collision detection and prevents penetration
	FHitResult MoveHitResult;
	EnergyCoreMesh->SetWorldLocationAndRotation(TargetPos, FinalRotation, true, &MoveHitResult);
	
	// If sweep blocked, the location will be automatically adjusted by SetWorldLocationAndRotation
	// No need to manually correct - the function handles it
	
	// Store target location for backward compatibility
	TargetLocation = EnergyCoreMesh->GetComponentLocation();
	
	// Sync AccumulatedRotation from actual mesh rotation for backward compatibility
	AccumulatedRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
	AccumulatedRotation.Normalize();
	
	// ========== OLD PHYSICS HANDLE SYSTEM (COMMENTED OUT) ==========
	/*
	// Only update position if we're being held
	if (!bIsBeingHeld || TargetCharacter == nullptr || PhysicsHandle == nullptr || EnergyCoreMesh == nullptr)
	{
		return;
	}

	UCameraComponent* CameraComponent = TargetCharacter->GetFirstPersonCameraComponent();
	if (CameraComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Get camera location and forward vector
	FVector CameraLocation = CameraComponent->GetComponentLocation();
	FVector CameraForwardVector = CameraComponent->GetForwardVector();

	// Smoothly transition DistanceFromCamera towards TargetDistance
	float DeltaTime = World->GetDeltaSeconds();
	float DistanceDiff = TargetDistance - DistanceFromCamera;
	float MaxDistanceChange = DistanceTransitionSpeed * DeltaTime;
	
	if (FMath::Abs(DistanceDiff) > MaxDistanceChange)
	{
		DistanceFromCamera += FMath::Sign(DistanceDiff) * MaxDistanceChange;
	}
	else
	{
		DistanceFromCamera = TargetDistance;
	}

	// Calculate ideal target location
	FVector IdealLocation = CameraLocation + (CameraForwardVector * DistanceFromCamera);

	// Perform line trace to check for obstacles
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(TargetCharacter);
	QueryParams.bTraceComplex = true;

	FVector TraceEnd = IdealLocation;
	FVector TraceStart = CameraLocation;
	
	FVector MeshExtent = EnergyCoreMesh->Bounds.BoxExtent;
	float MeshRadius = FMath::Max3(MeshExtent.X, MeshExtent.Y, MeshExtent.Z);
	
	TraceStart += CameraForwardVector * MeshRadius;
	
	ECollisionChannel PositionTraceChannel = ECC_Visibility;
	
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		PositionTraceChannel,
		QueryParams
	);

	FVector ActualTargetLocation;
	if (bHit && HitResult.bBlockingHit)
	{
		ActualTargetLocation = HitResult.Location - (CameraForwardVector * MeshRadius);
		
		FVector ToTarget = ActualTargetLocation - CameraLocation;
		float DistanceToTarget = ToTarget.Size();
		if (DistanceToTarget < MeshRadius * 2.0f)
		{
			ActualTargetLocation = CameraLocation + (CameraForwardVector * MeshRadius * 2.0f);
		}
	}
	else
	{
		ActualTargetLocation = IdealLocation;
	}

	TargetLocation = ActualTargetLocation;

	// Update rotation constraint system
	UpdateRotationConstraint(DeltaTime);
	
	FRotator TargetRotator = TargetRotation.Rotator();
	
	PhysicsHandle->SetTargetLocationAndRotation(ActualTargetLocation, TargetRotator);
	
	FRotator ActualMeshRot = EnergyCoreMesh->GetComponentRotation();
	AccumulatedRotation = ActualMeshRot.Quaternion();
	AccumulatedRotation.Normalize();
	*/
}

void ATEAMBOXEnergyCore::Release()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("[RELEASE] Release called on energy core: %s"), *GetName());
	
	if (EnergyCoreMesh == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[RELEASE] EnergyCoreMesh is nullptr"));
		return;
	}
	
	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	// Only release if currently held
	if (HoldState == EHoldState::Held_Kinematic)
	{
		// Step 1: Re-enable physics simulation
		EnergyCoreMesh->SetSimulatePhysics(true);
		
		// Step 2: Re-enable gravity (natural falling)
		EnergyCoreMesh->SetEnableGravity(true);
		
		// Step 3: Reset damping to default values (for natural physics behavior)
		EnergyCoreMesh->SetLinearDamping(0.5f);
		EnergyCoreMesh->SetAngularDamping(1.0f);
		
		// Step 4: DO NOT apply any impulse or force
		// Let physics handle everything naturally
		
		// Step 5: Update state
		HoldState = EHoldState::Released_Physics;
		bIsBeingHeld = false;
		TargetCharacter = nullptr;
		
		// Reset transition tracking
		GrabTime = 0.0f;
		// Note: Do NOT reset CurrentRotation, LocalLookAtDirection, or InitialPitch
		// These should be preserved after release (rotation state is retained)
		
		// Reset rotation animation state
		bIsRotating = false;
		RotationAnimationProgress = 0.0f;
		TargetRotationForAnimation = FQuat::Identity;
		AnimationStartRotation = FQuat::Identity;
		
		UE_LOG(LogTemplateCharacter, Log, TEXT("[RELEASE] ========== Kinematic Hold System Deactivated =========="));
		UE_LOG(LogTemplateCharacter, Log, TEXT("[RELEASE] Physics simulation enabled, gravity enabled, no impulse applied"));
	}
	
	// ========== OLD PHYSICS HANDLE SYSTEM (COMMENTED OUT) ==========
	/*
	if (PhysicsHandle == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("PhysicsHandle is nullptr in Release()"));
		return;
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("Releasing physics handle, bIsBeingHeld before: %d"), bIsBeingHeld);
	
	// Release the physics handle first
	PhysicsHandle->ReleaseComponent();
	bIsBeingHeld = false;
	TargetCharacter = nullptr;
	
	// Reset physics parameters to default values for natural falling behavior
	EnergyCoreMesh->SetLinearDamping(0.5f);
	EnergyCoreMesh->SetAngularDamping(1.0f);
	
	if (EnergyCoreMesh->GetBodyInstance())
	{
		EnergyCoreMesh->SetMassOverrideInKg(NAME_None, 0.0f, false);
	}
	
	EnergyCoreMesh->SetEnableGravity(true);
	
	// Reset rotation constraint system
	RotationConstraintState = ERotationConstraintState::FreeFollow;
	StateTime = 0.0f;
	PreviousAngularVelocity = 0.0f;
	BlockDetectionFrameCount = 0;
	DesiredRotation = FQuat::Identity;
	TargetRotation = FQuat::Identity;
	*/
}

void ATEAMBOXEnergyCore::Grab()
{
	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	if (EnergyCoreMesh && TargetCharacter != nullptr)
	{
		UCameraComponent* CameraComponent = TargetCharacter->GetFirstPersonCameraComponent();
		if (CameraComponent == nullptr)
		{
			UE_LOG(LogTemplateCharacter, Warning, TEXT("[GRAB] CameraComponent is nullptr"));
			return;
		}

		// Step 1: Disable physics simulation (switch to kinematic)
		EnergyCoreMesh->SetSimulatePhysics(false);
		
		// Step 2: Disable gravity (not needed in kinematic mode, but keep for consistency)
		EnergyCoreMesh->SetEnableGravity(false);
		
		// Step 3: Keep collision enabled for sweep detection (QueryAndPhysics)
		// Collision must remain enabled so SetWorldLocationAndRotation can detect walls
		EnergyCoreMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		
		// Step 4: Record hold distance (clamp between MinDistance and MaxDistance)
		FVector CameraLocation = CameraComponent->GetComponentLocation();
		FVector ObjectLocation = EnergyCoreMesh->GetComponentLocation();
		float CurrentDistance = FVector::Dist(CameraLocation, ObjectLocation);
		HoldDistance = FMath::Clamp(CurrentDistance, MinDistance, MaxDistance);
		
		// Step 5: Set initial target distance for smooth transition
		TargetDistance = HoldDistance;
		DistanceFromCamera = HoldDistance;
		
		// Step 6: Update state
		HoldState = EHoldState::Held_Kinematic;
		bIsBeingHeld = true;
		
		// Step 7: Record the local direction that is facing the camera at grab moment
		// This is the key: we record which face/direction was facing the player when grabbed
		// Note: ObjectLocation and CameraLocation are already declared above
		FVector DirectionToCamera = CameraLocation - ObjectLocation;
		float DistanceToCamera = DirectionToCamera.Size();
		
		if (DistanceToCamera > 1.0f)
		{
			DirectionToCamera.Normalize();
			
			// Get object's current rotation
			FQuat ObjectRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
			ObjectRotation.Normalize();
			
			// Convert world direction to camera into object's local space
			// This tells us which local direction (face) was facing the camera
			LocalLookAtDirection = ObjectRotation.Inverse().RotateVector(DirectionToCamera);
			LocalLookAtDirection.Normalize();
			
			UE_LOG(LogTemplateCharacter, Log, TEXT("[GRAB] Local Look-At Direction: X=%.4f Y=%.4f Z=%.4f"), 
				LocalLookAtDirection.X, LocalLookAtDirection.Y, LocalLookAtDirection.Z);
		}
		else
		{
			// Too close, use forward direction as default
			LocalLookAtDirection = FVector::ForwardVector;
		}
		
		// Step 8: Initialize current rotation from actual mesh rotation (for smooth interpolation)
		// This preserves the rotation state at grab moment
		CurrentRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
		CurrentRotation.Normalize();
		
		// Record initial pitch to maintain vertical orientation (only horizontal Look-At)
		FRotator InitialRotator = CurrentRotation.Rotator();
		InitialPitch = InitialRotator.Pitch;
		
		// Reset grab time for transition tracking
		GrabTime = 0.0f;
		
		// Initialize accumulated rotation from current mesh rotation (for backward compatibility)
		AccumulatedRotation = CurrentRotation;
		AccumulatedRotation.Normalize();
		
		UE_LOG(LogTemplateCharacter, Log, TEXT("[GRAB] ========== Kinematic Hold System Activated =========="));
		UE_LOG(LogTemplateCharacter, Log, TEXT("[GRAB] Hold Distance: %.2f cm (Min: %.2f, Max: %.2f)"), 
			HoldDistance, MinDistance, MaxDistance);
		FRotator InitialRot = CurrentRotation.Rotator();
		UE_LOG(LogTemplateCharacter, Log, TEXT("[GRAB] Initial Rotation: Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
			InitialRot.Pitch, InitialRot.Yaw, InitialRot.Roll);
	}
	
	// ========== OLD PHYSICS HANDLE SYSTEM (COMMENTED OUT) ==========
	/*
	if (PhysicsHandle && EnergyCoreMesh && TargetCharacter != nullptr)
	{
		// Disable gravity
		EnergyCoreMesh->SetEnableGravity(false);
		
		// Apply damping values to make the spring arm more rigid
		EnergyCoreMesh->SetLinearDamping(Damping);
		EnergyCoreMesh->SetAngularDamping(FMath::Min(Damping * 0.1f, 20.0f));
		
		if (EnergyCoreMesh->GetBodyInstance())
		{
			EnergyCoreMesh->SetMassOverrideInKg(NAME_None, 10.0f, true);
		}
		
		// Initialize rotation constraint system
		if (EnergyCoreMesh != nullptr)
		{
			FQuat ObjectRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
			TargetRotation = ObjectRotation;
			DesiredRotation = ObjectRotation;
			
			RotationConstraintState = ERotationConstraintState::FreeFollow;
			StateTime = 0.0f;
			PreviousAngularVelocity = 0.0f;
			BlockDetectionFrameCount = 0;
		}
		
		AccumulatedRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
		AccumulatedRotation.Normalize();
		
		TargetDistance = 500.0f;
		DistanceFromCamera = 500.0f;
		
		PhysicsHandle->GrabComponentAtLocation(EnergyCoreMesh, NAME_None, GetActorLocation());
		bIsBeingHeld = true;
	}
	*/
}

bool ATEAMBOXEnergyCore::IsBeingHeld() const
{
	return bIsBeingHeld;
}

void ATEAMBOXEnergyCore::ResetRotation()
{
	if (EnergyCoreMesh == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("EnergyCoreMesh is nullptr in ResetRotation()"));
		return;
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[RESET_ROTATION] Resetting rotation for energy core: %s"), *GetName());
	
	// ========== NEW KINEMATIC HOLD SYSTEM ==========
	// In kinematic hold system, ResetRotation sets rotation to Look-At camera direction
	if (HoldState == EHoldState::Held_Kinematic && TargetCharacter != nullptr)
	{
		UCameraComponent* CameraComponent = TargetCharacter->GetFirstPersonCameraComponent();
		if (CameraComponent != nullptr && EnergyCoreMesh != nullptr)
		{
			FVector ObjectLocation = EnergyCoreMesh->GetComponentLocation();
			FVector CameraLocation = CameraComponent->GetComponentLocation();
			FVector DirectionToCamera = CameraLocation - ObjectLocation;
			float DistanceToCamera = DirectionToCamera.Size();
			
			if (DistanceToCamera > 1.0f)
			{
				DirectionToCamera.Normalize();
				// Use MakeFromXZ for stable Look-At rotation
				FVector WorldUp = FVector::UpVector;
				FMatrix LookAtMatrix = FRotationMatrix::MakeFromXZ(DirectionToCamera, WorldUp);
				FRotator LookAtRotator = LookAtMatrix.Rotator();
				
				// In kinematic mode, directly set the rotation
				EnergyCoreMesh->SetWorldRotation(LookAtRotator);
				
				// Sync AccumulatedRotation for backward compatibility
				AccumulatedRotation = LookAtRotator.Quaternion();
				AccumulatedRotation.Normalize();
			}
		}
	}
	
	// ========== OLD RESET LOGIC (COMMENTED OUT) ==========
	/*
	// Reset accumulated rotation to identity (zero rotation)
	AccumulatedRotation = FQuat::Identity;
	AccumulatedRotation.Normalize();
	
	// Set the actor and mesh rotation using quaternion
	SetActorRotation(AccumulatedRotation);
	EnergyCoreMesh->SetWorldRotation(AccumulatedRotation);
	
	// CRITICAL: Sync AccumulatedRotation from actual mesh rotation to ensure consistency
	FRotator ActualMeshRot = EnergyCoreMesh->GetComponentRotation();
	AccumulatedRotation = ActualMeshRot.Quaternion();
	AccumulatedRotation.Normalize();
	
	// If using physics handle, update the target rotation
	if (PhysicsHandle != nullptr && bIsBeingHeld)
	{
		FVector CurrentLocation = GetActorLocation();
		FRotator MeshRotForPhysicsHandle = EnergyCoreMesh->GetComponentRotation();
		PhysicsHandle->SetTargetLocationAndRotation(CurrentLocation, MeshRotForPhysicsHandle);
	}
	*/
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[RESET_ROTATION] Rotation reset complete"));
}

void ATEAMBOXEnergyCore::RotateZ(float Degrees)
{
	// ========== NEW Q/E ROTATION SYSTEM ==========
	// Q key: Rotate 90 degrees around LocalLookAtDirection axis (the face pointing to player)
	if (EnergyCoreMesh == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEZ] EnergyCoreMesh is nullptr"));
		return;
	}
	
	if (HoldState != EHoldState::Held_Kinematic)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEZ] Energy core is not being held, cannot rotate"));
		return;
	}
	
	// Ignore Degrees parameter, always rotate 90 degrees
	const float RotationDegrees = 90.0f;
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEZ] ========== Q Key Rotation (LocalLookAtDirection axis) =========="));
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEZ] Rotation: %.2f degrees around LocalLookAtDirection axis"), RotationDegrees);
	
	// Get current rotation (use actual mesh rotation to avoid desync)
	FQuat CurrentRot = EnergyCoreMesh->GetComponentRotation().Quaternion();
	CurrentRot.Normalize();
	CurrentRotation = CurrentRot; // Sync CurrentRotation with actual mesh rotation
	
	// Store start rotation for animation
	AnimationStartRotation = CurrentRot;
	
	// Get LocalLookAtDirection in world space (this is the axis we want to rotate around)
	FVector RotationAxisWorld = CurrentRot.RotateVector(LocalLookAtDirection);
	RotationAxisWorld.Normalize();
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEZ] Rotation Axis (World): X=%.4f Y=%.4f Z=%.4f"), 
		RotationAxisWorld.X, RotationAxisWorld.Y, RotationAxisWorld.Z);
	
	// Create rotation quaternion around this axis (clockwise = positive angle)
	FQuat DeltaRotation = FQuat(RotationAxisWorld, FMath::DegreesToRadians(RotationDegrees));
	DeltaRotation.Normalize();
	
	// Calculate target rotation
	TargetRotationForAnimation = DeltaRotation * CurrentRot;
	TargetRotationForAnimation.Normalize();
	
	// Start rotation animation
	bIsRotating = true;
	RotationAnimationProgress = 0.0f;
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEZ] Rotation animation started (duration: %.2f seconds)"), RotationAnimationDuration);
	
	// DIAGNOSTIC: Log current state BEFORE rotation
	FRotator CurrentRotBefore = AccumulatedRotation.Rotator();
	FRotator MeshRotBefore = EnergyCoreMesh->GetComponentRotation();
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] BEFORE - AccumulatedRotation Quat: W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		AccumulatedRotation.W, AccumulatedRotation.X, AccumulatedRotation.Y, AccumulatedRotation.Z);
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] BEFORE - AccumulatedRotation Rotator: Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
		CurrentRotBefore.Pitch, CurrentRotBefore.Yaw, CurrentRotBefore.Roll);
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] BEFORE - Mesh Actual Rotation: Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
		MeshRotBefore.Pitch, MeshRotBefore.Yaw, MeshRotBefore.Roll);
	
	// Check for gimbal lock condition (Pitch near ±90 degrees)
	if (FMath::Abs(CurrentRotBefore.Pitch) > 85.0f && FMath::Abs(CurrentRotBefore.Pitch) < 95.0f)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("[DIAG] ⚠️ GIMBAL LOCK DETECTED! Pitch=%.2f (near ±90°)"), CurrentRotBefore.Pitch);
	}
	
	// Create rotation quaternion around local Z axis (Roll axis) in local space
	// Get the local Z axis from accumulated rotation (for "dice roll" effect)
	FVector LocalZAxis = AccumulatedRotation.GetAxisZ();
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] LocalZAxis: X=%.4f Y=%.4f Z=%.4f"), 
		LocalZAxis.X, LocalZAxis.Y, LocalZAxis.Z);
	const FQuat DeltaQuat = FQuat(LocalZAxis, FMath::DegreesToRadians(Degrees));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] DeltaQuat: W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		DeltaQuat.W, DeltaQuat.X, DeltaQuat.Y, DeltaQuat.Z);
	
	// Accumulate rotation using quaternion multiplication (local space rotation)
	AccumulatedRotation = DeltaQuat * AccumulatedRotation;
	AccumulatedRotation.Normalize(); // Avoid numerical drift
	
	// DIAGNOSTIC: Log state AFTER quaternion multiplication but BEFORE setting
	FRotator RotAfterQuat = AccumulatedRotation.Rotator();
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] AFTER Quat Multiply - AccumulatedRotation Quat: W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		AccumulatedRotation.W, AccumulatedRotation.X, AccumulatedRotation.Y, AccumulatedRotation.Z);
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] AFTER Quat Multiply - AccumulatedRotation Rotator: Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
		RotAfterQuat.Pitch, RotAfterQuat.Yaw, RotAfterQuat.Roll);
	
	// ========== OLD ROTATION LOGIC - COMMENTED OUT ==========
	// This old logic directly sets rotation, which conflicts with the new rotation constraint system.
	// The new system uses physics handle target rotation instead of direct rotation setting.
	// TODO: User will rewrite Q/E rotation logic to work with the new constraint system.
	
	/*
	// Set the actor and mesh rotation using accumulated quaternion
	SetActorRotation(AccumulatedRotation);
	EnergyCoreMesh->SetWorldRotation(AccumulatedRotation);
	
	// CRITICAL: Sync AccumulatedRotation from actual mesh rotation to avoid gimbal lock inconsistencies
	// After setting the mesh rotation, read it back and update AccumulatedRotation to ensure consistency
	// This is especially important near gimbal lock positions (Pitch ≈ ±90°)
	FRotator ActualMeshRot = EnergyCoreMesh->GetComponentRotation();
	AccumulatedRotation = ActualMeshRot.Quaternion();
	AccumulatedRotation.Normalize();
	
	// CRITICAL: Also update physics handle rotation to prevent it from overriding our rotation
	// IMPORTANT: Use the actual mesh rotation instead of converting from quaternion to avoid gimbal lock issues
	if (PhysicsHandle != nullptr && bIsBeingHeld)
	{
		FVector CurrentLocation = GetActorLocation();
		// Get the actual rotation from the mesh (after setting) to avoid gimbal lock conversion errors
		FRotator MeshRotForPhysicsHandle = EnergyCoreMesh->GetComponentRotation();
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[DIAG] Setting PhysicsHandle with Mesh Rotator: Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
			MeshRotForPhysicsHandle.Pitch, MeshRotForPhysicsHandle.Yaw, MeshRotForPhysicsHandle.Roll);
		// Update physics handle with both location and rotation (use mesh rotation to avoid gimbal lock)
		PhysicsHandle->SetTargetLocationAndRotation(CurrentLocation, MeshRotForPhysicsHandle);
	}
	*/
	
	// ========== NEW APPROACH: Q/E rotation not implemented yet ==========
	// The new Look-At system will handle rotation automatically
	// Q/E rotation functionality needs to be redesigned to work with Look-At system
	UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEZ] Rotation requested but Q/E rotation not implemented for Look-At system yet."));
	
	// Reset angular velocity to prevent continued rotation
	if (EnergyCoreMesh->IsSimulatingPhysics())
	{
		EnergyCoreMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("========== RotateZ complete =========="));
}

void ATEAMBOXEnergyCore::RotateY(float Degrees)
{
	// ========== NEW Q/E ROTATION SYSTEM ==========
	// E key: Toggle BeamStartArrow between horizontal and vertical using quaternion algorithm
	if (EnergyCoreMesh == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEY] EnergyCoreMesh is nullptr"));
		return;
	}
	
	if (HoldState != EHoldState::Held_Kinematic)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEY] Energy core is not being held, cannot rotate"));
		return;
	}
	
	if (BeamStartComponent == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATEY] BeamStartComponent (BeamStartArrow) is nullptr"));
		return;
	}
	
	// Get current rotation (use actual mesh rotation to avoid desync)
	FQuat CurrentRot = EnergyCoreMesh->GetComponentRotation().Quaternion();
	CurrentRot.Normalize();
	CurrentRotation = CurrentRot; // Sync CurrentRotation with actual mesh rotation
	
	// Store start rotation for animation
	AnimationStartRotation = CurrentRot;
	
	// Step 1: Get BeamStartArrow's current direction in world space
	FVector ArrowCurrentDirection = BeamStartComponent->GetForwardVector();
	ArrowCurrentDirection.Normalize();
	
	// Step 2: Determine current state (horizontal or vertical)
	FVector WorldUp = FVector::UpVector;      // Z axis (0,0,1)
	FVector WorldForward = FVector::ForwardVector; // Y axis (in UE5: Y is forward, 0,1,0)
	
	float VerticalDot = FMath::Abs(FVector::DotProduct(ArrowCurrentDirection, WorldUp));
	
	const float HorizontalThreshold = 0.3f; // If dot < 0.3, consider horizontal (perpendicular to Z)
	const float VerticalThreshold = 0.7f;   // If dot > 0.7, consider vertical (parallel to Z)
	
	bool bCurrentlyHorizontal = (VerticalDot < HorizontalThreshold);
	bool bCurrentlyVertical = (VerticalDot > VerticalThreshold);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] ========== E Key Rotation Started =========="));
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Arrow Current Direction: X=%.4f Y=%.4f Z=%.4f"), 
		ArrowCurrentDirection.X, ArrowCurrentDirection.Y, ArrowCurrentDirection.Z);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Vertical Dot Product (with Z axis): %.4f"), VerticalDot);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] State: Horizontal=%d, Vertical=%d"), bCurrentlyHorizontal, bCurrentlyVertical);
	
	// Step 3: Calculate target direction
	FVector TargetDirection;
	
	if (bCurrentlyHorizontal)
	{
		// Currently horizontal (perpendicular to Z) → rotate to parallel with Z axis (vertical)
		// Choose Z axis direction (up or down) based on which is closer
		float UpDot = FVector::DotProduct(ArrowCurrentDirection, WorldUp);
		if (UpDot >= 0.0f)
		{
			TargetDirection = WorldUp; // Rotate to +Z (up)
		}
		else
		{
			TargetDirection = -WorldUp; // Rotate to -Z (down)
		}
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] State: Horizontal → Rotate to Z axis (vertical)"));
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Target Direction: X=%.4f Y=%.4f Z=%.4f"), 
			TargetDirection.X, TargetDirection.Y, TargetDirection.Z);
	}
	else if (bCurrentlyVertical)
	{
		// Currently vertical (parallel to Z) → rotate to parallel with Y axis (horizontal)
		// Use world Y axis (forward) as target direction
		// Choose +Y or -Y based on which is closer to current horizontal component
		FVector HorizontalComponent = FVector(ArrowCurrentDirection.X, ArrowCurrentDirection.Y, 0.0f);
		float HorizontalLength = HorizontalComponent.Size();
		
		if (HorizontalLength > 0.01f)
		{
			// Use the horizontal component direction, but align it to Y axis
			// Project onto Y axis direction
			float YDot = FVector::DotProduct(HorizontalComponent.GetSafeNormal(), WorldForward);
			TargetDirection = (YDot >= 0.0f) ? WorldForward : -WorldForward;
		}
		else
		{
			// Arrow is perfectly vertical, use default Y axis direction
			TargetDirection = WorldForward; // +Y axis
		}
		TargetDirection.Normalize();
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] State: Vertical → Rotate to Y axis (horizontal)"));
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Target Direction: X=%.4f Y=%.4f Z=%.4f"), 
			TargetDirection.X, TargetDirection.Y, TargetDirection.Z);
	}
	else
	{
		// In between state (neither clearly horizontal nor vertical)
		// Default to toggling to vertical (Z axis) if closer to horizontal, otherwise to horizontal (Y axis)
		if (VerticalDot < 0.5f)
		{
			// Closer to horizontal, rotate to vertical (Z axis)
			float UpDot = FVector::DotProduct(ArrowCurrentDirection, WorldUp);
			TargetDirection = (UpDot >= 0.0f) ? WorldUp : -WorldUp;
		}
		else
		{
			// Closer to vertical, rotate to horizontal (Y axis)
			TargetDirection = WorldForward; // +Y axis
		}
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] State: In-between → Rotate to %s"), 
			(VerticalDot < 0.5f) ? TEXT("Z axis (vertical)") : TEXT("Y axis (horizontal)"));
		UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Target Direction: X=%.4f Y=%.4f Z=%.4f"), 
			TargetDirection.X, TargetDirection.Y, TargetDirection.Z);
	}
	
	// Step 4: Calculate the rotation needed using quaternion algorithm
	// The key insight: BeamStartComponent is a child of EnergyCoreMesh
	// Current arrow world forward = CurrentRot * BeamStartRelativeRot * LocalArrowForward
	// Target arrow world forward = TargetDirection
	// We need: TargetRot * BeamStartRelativeRot * LocalArrowForward = TargetDirection
	// Therefore: TargetRot = TargetDirection * (BeamStartRelativeRot * LocalArrowForward)^(-1)
	// But we can simplify: since ArrowCurrentDirection = CurrentRot * BeamStartRelativeRot * LocalArrowForward,
	// we can calculate the rotation delta: RotationDelta * ArrowCurrentDirection = TargetDirection
	// So: RotationDelta = TargetDirection * ArrowCurrentDirection^(-1) (as quaternion rotation)
	
	// Step 4: Calculate rotation using quaternion algorithm
	// The key insight: We need to rotate the object such that BeamStartComponent's forward
	// points to TargetDirection. Since BeamStartComponent is a child of EnergyCoreMesh,
	// we can directly calculate the rotation from current arrow direction to target direction.
	
	// Use FQuat::FindBetweenNormals to calculate the rotation quaternion
	// This finds the shortest rotation from ArrowCurrentDirection to TargetDirection
	FQuat RotationDelta = FQuat::FindBetweenNormals(ArrowCurrentDirection, TargetDirection);
	RotationDelta.Normalize();
	
	// Apply this rotation delta to the object's current rotation using quaternion multiplication
	// This gives us the target rotation for the object
	TargetRotationForAnimation = RotationDelta * CurrentRot;
	TargetRotationForAnimation.Normalize();
	
	// Get BeamStartComponent's relative rotation for verification
	FQuat BeamStartRelativeRot = BeamStartComponent->GetRelativeRotation().Quaternion();
	BeamStartRelativeRot.Normalize();
	
	// ========== LOGGING ==========
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] ========== QUATERNION ROTATION CALCULATION =========="));
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Current Object Rotation (Quat): W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		CurrentRot.W, CurrentRot.X, CurrentRot.Y, CurrentRot.Z);
	FRotator CurrentRotRotator = CurrentRot.Rotator();
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Current Object Rotation (Rotator): Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
		CurrentRotRotator.Pitch, CurrentRotRotator.Yaw, CurrentRotRotator.Roll);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] BeamStartComponent Relative Rotation (Quat): W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		BeamStartRelativeRot.W, BeamStartRelativeRot.X, BeamStartRelativeRot.Y, BeamStartRelativeRot.Z);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Arrow Current World Direction: X=%.4f Y=%.4f Z=%.4f"), 
		ArrowCurrentDirection.X, ArrowCurrentDirection.Y, ArrowCurrentDirection.Z);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Arrow Target World Direction: X=%.4f Y=%.4f Z=%.4f"), 
		TargetDirection.X, TargetDirection.Y, TargetDirection.Z);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Rotation Delta (Quat): W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		RotationDelta.W, RotationDelta.X, RotationDelta.Y, RotationDelta.Z);
	
	// Calculate rotation angle
	float RotationAngle = FMath::RadiansToDegrees(2.0f * FMath::Acos(FMath::Clamp(FMath::Abs(RotationDelta.W), 0.0f, 1.0f)));
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Rotation Delta Angle: %.2f degrees"), RotationAngle);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Target Rotation (Quat): W=%.4f X=%.4f Y=%.4f Z=%.4f"), 
		TargetRotationForAnimation.W, TargetRotationForAnimation.X, TargetRotationForAnimation.Y, TargetRotationForAnimation.Z);
	FRotator TargetRotRotator = TargetRotationForAnimation.Rotator();
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Target Rotation (Rotator): Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
		TargetRotRotator.Pitch, TargetRotRotator.Yaw, TargetRotRotator.Roll);
	
	// Verify: Calculate expected arrow direction after rotation
	// BeamStartComponent's local forward vector (in its own local space)
	FVector LocalArrowForward = FVector::ForwardVector;
	FVector ExpectedArrowForward = TargetRotationForAnimation.RotateVector(BeamStartRelativeRot.RotateVector(LocalArrowForward));
	ExpectedArrowForward.Normalize();
	float AlignmentDot = FVector::DotProduct(ExpectedArrowForward, TargetDirection);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Expected Arrow Forward After Rotation: X=%.4f Y=%.4f Z=%.4f"), 
		ExpectedArrowForward.X, ExpectedArrowForward.Y, ExpectedArrowForward.Z);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Alignment Dot Product (should be ~1.0): %.4f"), AlignmentDot);
	
	// Start rotation animation
	bIsRotating = true;
	RotationAnimationProgress = 0.0f;
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] Animation started (duration: %.2f seconds)"), RotationAnimationDuration);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[ROTATEY] =========================================="));
}

void ATEAMBOXEnergyCore::SetTargetDistance(float NewDistance)
{
	// Clamp the new distance to the valid range
	TargetDistance = FMath::Clamp(NewDistance, MinDistance, MaxDistance);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("SetTargetDistance called: %.2f cm (clamped to %.2f - %.2f)"), 
		NewDistance, MinDistance, MaxDistance);
}

void ATEAMBOXEnergyCore::UpdateBeamReceiver()
{
	// Check if components are valid
	if (ReceiverComponent == nullptr || BeamStartComponent == nullptr || BeamComponent == nullptr)
	{
		// Components not set up in Blueprint, disable beam
		if (bIsReceivingBeam)
		{
			bIsReceivingBeam = false;
			// Disable beam component
			BeamComponent->SetVisibility(false);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Get receiver bounds for overlap detection
	FVector ReceiverLocation = ReceiverComponent->GetComponentLocation();
	FBoxSphereBounds ReceiverBounds = ReceiverComponent->CalcBounds(ReceiverComponent->GetComponentTransform());
	
	// Check if receiver is being hit by any beam
	bool bWasReceivingBeam = bIsReceivingBeam;
	bIsReceivingBeam = false;

	// Find all DeviceA actors
	TArray<AActor*> DeviceAActors;
	UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXDeviceA::StaticClass(), DeviceAActors);
	
	// Check each DeviceA's beam
	for (AActor* Actor : DeviceAActors)
	{
		ATEAMBOXDeviceA* DeviceA = Cast<ATEAMBOXDeviceA>(Actor);
		if (DeviceA == nullptr)
		{
			continue;
		}

		// Get DeviceA's beam start and end (we need to access private members, so we'll use a different approach)
		// Instead, we'll trace from DeviceA's arrow to its beam end
		// For now, let's use a simpler approach: check if DeviceA's beam line intersects with receiver
		// We'll need to add a public getter in DeviceA or use a different method
		
		// For now, let's trace from all DeviceA arrow components to check if they hit receiver
		if (DeviceA->ArrowComponent != nullptr)
		{
			FVector DeviceAArrowLocation = DeviceA->ArrowComponent->GetComponentLocation();
			FVector DeviceAArrowForward = DeviceA->ArrowComponent->GetForwardVector();
			FVector DeviceATraceEnd = DeviceAArrowLocation + (DeviceAArrowForward * DeviceA->MaxBeamRange);

			// Perform line trace to check if it hits receiver
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(DeviceA);
			QueryParams.bTraceComplex = true;

			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				DeviceAArrowLocation,
				DeviceATraceEnd,
				DeviceA->TraceChannel,
				QueryParams
			);

			if (bHit && HitResult.bBlockingHit)
			{
				// Check if hit actor is this energy core's receiver
				if (HitResult.Component == ReceiverComponent || HitResult.GetActor() == this)
				{
					bIsReceivingBeam = true;
					break;
				}
			}
		}
	}

	// Find all EnergyCore actors (excluding self)
	TArray<AActor*> EnergyCoreActors;
	UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXEnergyCore::StaticClass(), EnergyCoreActors);
	
	// Check each EnergyCore's beam (if it's emitting one)
	for (AActor* Actor : EnergyCoreActors)
	{
		ATEAMBOXEnergyCore* OtherEnergyCore = Cast<ATEAMBOXEnergyCore>(Actor);
		if (OtherEnergyCore == nullptr || OtherEnergyCore == this || !OtherEnergyCore->bIsReceivingBeam)
		{
			continue;
		}

		// Check if other energy core's beam hits this receiver
		if (OtherEnergyCore->BeamStartComponent != nullptr)
		{
			FVector OtherBeamStart = OtherEnergyCore->BeamStartComponent->GetComponentLocation();
			FVector OtherBeamForward = OtherEnergyCore->BeamStartComponent->GetForwardVector();
			FVector OtherBeamEnd = OtherBeamStart + (OtherBeamForward * OtherEnergyCore->MaxBeamRange);

			// Perform line trace to check if it hits receiver
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(OtherEnergyCore);
			QueryParams.bTraceComplex = true;

			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				OtherBeamStart,
				OtherBeamEnd,
				OtherEnergyCore->TraceChannel,
				QueryParams
			);

			if (bHit && HitResult.bBlockingHit)
			{
				// Check if hit actor is this energy core's receiver
				if (HitResult.Component == ReceiverComponent || HitResult.GetActor() == this)
				{
					bIsReceivingBeam = true;
					break;
				}
			}
		}
	}

	// Update outgoing beam if receiving
	if (bIsReceivingBeam)
	{
		// Get beam start location and direction from BeamStartComponent
		FVector BeamStart = BeamStartComponent->GetComponentLocation();
		FVector BeamForward = BeamStartComponent->GetForwardVector();
		FVector TraceEnd = BeamStart + (BeamForward * MaxBeamRange);

		// Perform line trace
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;

		bool bHit = World->LineTraceSingleByChannel(
			HitResult,
			BeamStart,
			TraceEnd,
			TraceChannel,
			QueryParams
		);

		// Update beam locations
		BeamStartLocation = BeamStart;
		
		if (bHit && HitResult.bBlockingHit)
		{
			// Hit something, end beam at hit location
			BeamEndLocation = HitResult.Location;
		}
		else
		{
			// No hit, use max range
			BeamEndLocation = TraceEnd;
		}

		// Update Niagara Beam
		FVector BeamStartForNiagara = BeamStartLocation;
		FVector BeamEndForNiagara = BeamEndLocation;
		
		if (bUseLocalSpaceForBeam)
		{
			// Convert world coordinates to local space relative to BeamComponent
			FTransform ComponentTransform = BeamComponent->GetComponentTransform();
			BeamStartForNiagara = ComponentTransform.InverseTransformPosition(BeamStartLocation);
			BeamEndForNiagara = ComponentTransform.InverseTransformPosition(BeamEndLocation);
		}

		// Update Niagara beam parameters
		// Method 1: Using SetNiagaraVariableVec3 (most reliable method)
		BeamComponent->SetNiagaraVariableVec3(FString("User.BeamStart"), BeamStartForNiagara);
		BeamComponent->SetNiagaraVariableVec3(FString("User.BeamEnd"), BeamEndForNiagara);
		
		// Method 2: Fallback - using SetVectorParameter
		BeamComponent->SetVectorParameter(FName("User.BeamStart"), BeamStartForNiagara);
		BeamComponent->SetVectorParameter(FName("User.BeamEnd"), BeamEndForNiagara);
		
		// Enable beam visibility
		BeamComponent->SetVisibility(true);
	}
	else
	{
		// Not receiving beam, disable beam visibility
		BeamComponent->SetVisibility(false);
	}

	// Debug visualization
	if (bEnableBeamDebugVisualization && World)
	{
		// Draw receiver collision box
		if (ReceiverComponent != nullptr)
		{
			FVector ReceiverExtent = ReceiverComponent->GetScaledBoxExtent();
			DrawDebugBox(World, ReceiverLocation, ReceiverExtent, 
				ReceiverComponent->GetComponentRotation().Quaternion(), 
				bIsReceivingBeam ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
		}

		// Draw outgoing beam if active
		if (bIsReceivingBeam)
		{
			DrawDebugLine(World, BeamStartLocation, BeamEndLocation, FColor::Cyan, false, 0.0f, 0, 3.0f);
			DrawDebugSphere(World, BeamStartLocation, 10.0f, 12, FColor::Yellow, false, 0.0f, 0, 2.0f);
			DrawDebugSphere(World, BeamEndLocation, 10.0f, 12, FColor::Magenta, false, 0.0f, 0, 2.0f);
		}
	}
}

// ========== OLD ROTATION CONSTRAINT SYSTEM (COMMENTED OUT) ==========
// This function is replaced by direct kinematic control in UpdatePosition()
/*
void ATEAMBOXEnergyCore::UpdateRotationConstraint(float DeltaTime)
{
	// Only update if being held
	if (!bIsBeingHeld || TargetCharacter == nullptr || EnergyCoreMesh == nullptr)
	{
		static int32 SkipCount = 0;
		if (SkipCount++ % 300 == 0) // Log every 5 seconds
		{
			UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] SKIPPED - bIsBeingHeld=%d, TargetCharacter=%d, EnergyCoreMesh=%d"), 
				bIsBeingHeld, TargetCharacter != nullptr, EnergyCoreMesh != nullptr);
		}
		return;
	}
	
	static int32 CallCount = 0;
	if (CallCount++ % 60 == 0) // Log every 60 frames
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] UpdateRotationConstraint called (frame %d)"), CallCount);
	}

	UCameraComponent* CameraComponent = TargetCharacter->GetFirstPersonCameraComponent();
	if (CameraComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Step 1: Calculate Desired Rotation using Look-At (object faces camera position)
	// Get object world position
	FVector ObjectLocation = EnergyCoreMesh->GetComponentLocation();
	
	// Get camera world position
	FVector CameraLocation = CameraComponent->GetComponentLocation();
	
	// Calculate direction vector: Object → Camera
	FVector DirectionToCamera = CameraLocation - ObjectLocation;
	float DistanceToCamera = DirectionToCamera.Size();
	
	// Only calculate Look-At if distance is valid (avoid division by zero)
	if (DistanceToCamera > 1.0f) // At least 1cm away
	{
		DirectionToCamera.Normalize();
		
		// Generate Look-At rotation using FindLookAtRotation
		// This calculates a rotation that makes the object's forward axis point toward the camera
		// We need to specify both forward direction and up direction for stable rotation
		// Use world up (Z axis) as the up direction
		FVector WorldUp = FVector::UpVector; // Z axis up
		
		// Use MakeFromXZ to create a rotation matrix with forward (X) and up (Z) axes
		// This ensures stable rotation without roll
		FMatrix LookAtMatrix = FRotationMatrix::MakeFromXZ(DirectionToCamera, WorldUp);
		FRotator LookAtRotator = LookAtMatrix.Rotator();
		DesiredRotation = LookAtRotator.Quaternion();
		DesiredRotation.Normalize();
	}
	else
	{
		// Too close, calculate Look-At anyway but log a warning
		DirectionToCamera.Normalize();
		FVector WorldUp = FVector::UpVector;
		FMatrix LookAtMatrix = FRotationMatrix::MakeFromXZ(DirectionToCamera, WorldUp);
		FRotator LookAtRotator = LookAtMatrix.Rotator();
		DesiredRotation = LookAtRotator.Quaternion();
		DesiredRotation.Normalize();
	}
	
	// Initialize TargetRotation if not set (should be set in Grab/SetTargetCharacter, but fallback here)
	if (TargetRotation.IsIdentity())
	{
		// Initialize to DesiredRotation (Look-At direction)
		TargetRotation = DesiredRotation;
		RotationConstraintState = ERotationConstraintState::FreeFollow;
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Initialized TargetRotation to DesiredRotation on first UpdateRotationConstraint call"));
	}
	
	// Debug: Log rotation calculations every frame (can be disabled later)
	static int32 DebugFrameCount = 0;
	if (DebugFrameCount++ % 60 == 0) // Log every 60 frames (about once per second at 60fps)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] === Frame %d ==="), DebugFrameCount);
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Object Location: X=%.2f Y=%.2f Z=%.2f"), 
			ObjectLocation.X, ObjectLocation.Y, ObjectLocation.Z);
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Camera Location: X=%.2f Y=%.2f Z=%.2f"), 
			CameraLocation.X, CameraLocation.Y, CameraLocation.Z);
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Direction To Camera: X=%.4f Y=%.4f Z=%.4f"), 
			DirectionToCamera.X, DirectionToCamera.Y, DirectionToCamera.Z);
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Desired Rot (Look-At): Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
			DesiredRotation.Rotator().Pitch, DesiredRotation.Rotator().Yaw, DesiredRotation.Rotator().Roll);
	}

	// Step 2: Get Actual Rotation (from physics simulation)
	FQuat ActualRotation = EnergyCoreMesh->GetComponentRotation().Quaternion();
	ActualRotation.Normalize();

	// Step 3: Calculate Angular Error (quaternion angular distance)
	// Angular distance = 2 * acos(|dot(q1, q2)|)
	FQuat ErrorQuat = ActualRotation.Inverse() * DesiredRotation;
	ErrorQuat.Normalize();
	float AngleError = FMath::RadiansToDegrees(2.0f * FMath::Acos(FMath::Clamp(FMath::Abs(ErrorQuat.W), 0.0f, 1.0f)));

	// Step 4: Calculate Angular Velocity
	FVector AngularVelocityVec = EnergyCoreMesh->GetPhysicsAngularVelocityInRadians();
	float AngularVelocity = FMath::RadiansToDegrees(AngularVelocityVec.Size());
	
	// Smooth angular velocity for stability
	float SmoothedAngularVelocity = FMath::Lerp(PreviousAngularVelocity, AngularVelocity, FMath::Clamp(DeltaTime * 10.0f, 0.0f, 1.0f));
	PreviousAngularVelocity = SmoothedAngularVelocity;

	// Step 5: Update State Time
	StateTime += DeltaTime;

	// Step 6: State Machine
	const float BlockDetectionTime = 0.08f; // Time tolerance for block detection
	const float FreeFollowThreshold = 5.0f; // Angle threshold to enter free follow

	switch (RotationConstraintState)
	{
	case ERotationConstraintState::FreeFollow:
	{
		// Check for blocking condition
		bool bAngleErrorLarge = AngleError > BlockAngle;
		bool bAngularVelocityLow = SmoothedAngularVelocity < BlockAngularVelocity;
		
		if (bAngleErrorLarge && bAngularVelocityLow)
		{
			BlockDetectionFrameCount++;
			// Require sustained condition for block detection time
			if (StateTime >= BlockDetectionTime || BlockDetectionFrameCount >= 3)
			{
				RotationConstraintState = ERotationConstraintState::Blocked;
				StateTime = 0.0f;
				BlockDetectionFrameCount = 0;
			}
		}
		else
		{
			// Reset block detection if condition not met
			BlockDetectionFrameCount = 0;
			StateTime = 0.0f;
		}

		// Update Target Rotation using exponential Slerp (QInterpTo)
		TargetRotation = FQuat::Slerp(TargetRotation, DesiredRotation, FMath::Clamp(DeltaTime * FollowSpeed, 0.0f, 1.0f));
		TargetRotation.Normalize();
		
		// Debug: Log target rotation update
		if (DebugFrameCount % 60 == 0)
		{
			UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Target Rot (FreeFollow): Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
				TargetRotation.Rotator().Pitch, TargetRotation.Rotator().Yaw, TargetRotation.Rotator().Roll);
		}
		break;
	}

	case ERotationConstraintState::Blocked:
	{
		// Freeze Target Rotation (don't update it)
		// TargetRotation remains unchanged

		// Check for recovery condition
		bool bAngularVelocityHigh = SmoothedAngularVelocity > BlockAngularVelocity;
		bool bAngleErrorDecreased = AngleError < (BlockAngle * 0.7f); // 70% of block angle

		if (bAngularVelocityHigh || bAngleErrorDecreased)
		{
			RotationConstraintState = ERotationConstraintState::Recover;
			StateTime = 0.0f;
		}
		break;
	}

	case ERotationConstraintState::Recover:
	{
		// Slowly recover towards Desired Rotation
		TargetRotation = FQuat::Slerp(TargetRotation, DesiredRotation, FMath::Clamp(DeltaTime * RecoverSpeed, 0.0f, 1.0f));
		TargetRotation.Normalize();
		
		// Debug: Log target rotation update
		if (DebugFrameCount % 60 == 0)
		{
			UE_LOG(LogTemplateCharacter, Warning, TEXT("[ROTATION_CONSTRAINT] Target Rot (Recover): Pitch=%.2f Yaw=%.2f Roll=%.2f"), 
				TargetRotation.Rotator().Pitch, TargetRotation.Rotator().Yaw, TargetRotation.Rotator().Roll);
		}

		// Check if recovered enough to enter free follow
		if (AngleError < FreeFollowThreshold)
		{
			RotationConstraintState = ERotationConstraintState::FreeFollow;
			StateTime = 0.0f;
		}
		break;
	}
	}

	// Step 7: Debug Visualization
	if (bEnableRotationDebugVisualization && World)
	{
		FVector CoreLocation = EnergyCoreMesh->GetComponentLocation();
		float ArrowLength = 50.0f;

		// Green: Actual Rotation
		FVector ActualForward = ActualRotation.GetForwardVector();
		DrawDebugLine(World, CoreLocation, CoreLocation + ActualForward * ArrowLength, FColor::Green, false, 0.0f, 0, 3.0f);
		DrawDebugSphere(World, CoreLocation + ActualForward * ArrowLength, 5.0f, 8, FColor::Green, false, 0.0f, 0, 2.0f);

		// Blue: Desired Rotation (Look-At camera direction)
		FVector DesiredForward = DesiredRotation.GetForwardVector();
		DrawDebugLine(World, CoreLocation, CoreLocation + DesiredForward * ArrowLength, FColor::Blue, false, 0.0f, 0, 3.0f);
		DrawDebugSphere(World, CoreLocation + DesiredForward * ArrowLength, 5.0f, 8, FColor::Blue, false, 0.0f, 0, 2.0f);
		
		// Cyan: Draw line from object to camera (to show Look-At target)
		// CameraLocation is already declared above in Step 1, reuse it
		DrawDebugLine(World, CoreLocation, CameraLocation, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugSphere(World, CameraLocation, 10.0f, 8, FColor::Cyan, false, 0.0f, 0, 2.0f);

		// Yellow: Target Rotation
		FVector TargetForward = TargetRotation.GetForwardVector();
		DrawDebugLine(World, CoreLocation, CoreLocation + TargetForward * ArrowLength, FColor::Yellow, false, 0.0f, 0, 3.0f);
		DrawDebugSphere(World, CoreLocation + TargetForward * ArrowLength, 5.0f, 8, FColor::Yellow, false, 0.0f, 0, 2.0f);

		// Draw state text
		FString StateText;
		switch (RotationConstraintState)
		{
		case ERotationConstraintState::FreeFollow:
			StateText = TEXT("Free Follow");
			break;
		case ERotationConstraintState::Blocked:
			StateText = TEXT("Blocked");
			break;
		case ERotationConstraintState::Recover:
			StateText = TEXT("Recover");
			break;
		}

		FString DebugText = FString::Printf(TEXT("State: %s\nAngle Error: %.2f°\nAngular Vel: %.2f°/s"), 
			*StateText, AngleError, SmoothedAngularVelocity);
		DrawDebugString(World, CoreLocation + FVector(0, 0, 30), DebugText, nullptr, FColor::White, 0.0f, true, 1.5f);
	}
}
*/
