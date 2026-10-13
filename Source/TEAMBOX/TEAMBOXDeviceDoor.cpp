// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXDeviceDoor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ATEAMBOXDeviceDoor::ATEAMBOXDeviceDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create collision component
	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);

	// Initialize default values
	// StartPosition will be captured from placed location at BeginPlay.
	StartPosition = FVector::ZeroVector;
	EndPosition = FVector(0.0f, 0.0f, 100.0f); // Relative offset from start: 100cm up
	MovementSpeed = 100.0f; // 100 cm/s
	bUseSmoothCurve = true;
	bIsMoving = false;
	CurrentTargetPosition = FVector::ZeroVector;
}

void ATEAMBOXDeviceDoor::BeginPlay()
{
	Super::BeginPlay();

	// Use placed actor location as runtime start position.
	StartPosition = GetActorLocation();
	const FVector EndWorldPosition = StartPosition + EndPosition;
	CurrentTargetPosition = bIsActive ? EndWorldPosition : StartPosition;
	bIsMoving = false;
}

void ATEAMBOXDeviceDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update movement if active
	if (bIsMoving)
	{
		UpdateMovement(DeltaTime);
	}
}

void ATEAMBOXDeviceDoor::ActivateDevice_Implementation()
{
	// Set target to relative end world position and start moving
	CurrentTargetPosition = StartPosition + EndPosition;
	bIsMoving = true;
}

void ATEAMBOXDeviceDoor::DeactivateDevice_Implementation()
{
	// Set target to StartPosition and start moving (immediately reverses if already moving)
	CurrentTargetPosition = StartPosition;
	bIsMoving = true;
}

void ATEAMBOXDeviceDoor::UpdateMovement(float DeltaTime)
{
	FVector CurrentLocation = GetActorLocation();
	FVector Direction = (CurrentTargetPosition - CurrentLocation).GetSafeNormal();
	float DistanceToTarget = FVector::Dist(CurrentLocation, CurrentTargetPosition);

	// Check if we've reached the target
	if (DistanceToTarget < 1.0f) // 1cm threshold
	{
		SetActorLocation(CurrentTargetPosition);
		bIsMoving = false;
		return;
	}

	// Calculate movement distance this frame
	float MovementDistance = MovementSpeed * DeltaTime;

	// Calculate progress for smooth curve (0.0 to 1.0)
	// Progress is always from StartPosition (0.0) to EndWorldPosition (1.0)
	const FVector EndWorldPosition = StartPosition + EndPosition;
	float TotalDistance = FVector::Dist(StartPosition, EndWorldPosition);
	float CurrentProgress = 0.0f;
	
	if (TotalDistance > 0.01f)
	{
		// Calculate how far we've moved along the path from StartPosition to EndWorldPosition
		FVector StartToCurrent = CurrentLocation - StartPosition;
		FVector StartToEnd = EndWorldPosition - StartPosition;
		FVector StartToEndNormalized = StartToEnd.GetSafeNormal();
		
		float ProjectedDistance = FVector::DotProduct(StartToCurrent, StartToEndNormalized);
		CurrentProgress = FMath::Clamp(ProjectedDistance / TotalDistance, 0.0f, 1.0f);
	}

	// Apply smooth curve if enabled
	float SpeedMultiplier = 1.0f;
	if (bUseSmoothCurve && TotalDistance > 0.01f)
	{
		// Use SmoothStep for ease in/out
		// Calculate speed multiplier based on curve derivative
		float SmoothProgress = FMath::SmoothStep(0.0f, 1.0f, CurrentProgress);
		float NextProgress = FMath::Clamp(CurrentProgress + (MovementDistance / TotalDistance), 0.0f, 1.0f);
		float NextSmoothProgress = FMath::SmoothStep(0.0f, 1.0f, NextProgress);
		
		// Adjust speed based on curve derivative (slower at ends, faster in middle)
		if (FMath::Abs(NextProgress - CurrentProgress) > 0.0001f)
		{
			SpeedMultiplier = FMath::Abs((NextSmoothProgress - SmoothProgress) / (NextProgress - CurrentProgress));
			SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.1f, 2.0f); // Clamp to reasonable range
		}
	}

	// Calculate new location
	FVector NewLocation = CurrentLocation + (Direction * MovementDistance * SpeedMultiplier);

	// Clamp to not overshoot target
	if (FVector::Dist(NewLocation, CurrentTargetPosition) < MovementDistance * SpeedMultiplier)
	{
		NewLocation = CurrentTargetPosition;
		bIsMoving = false;
	}

	// Use Sweep to detect collisions
	FHitResult HitResult;
	bool bMoved = SetActorLocation(NewLocation, true, &HitResult);

	if (!bMoved || HitResult.bBlockingHit)
	{
		// Collision detected, stop movement
		if (HitResult.bBlockingHit)
		{
			// Move to collision point
			SetActorLocation(HitResult.Location);
		}
		bIsMoving = false;
	}
}
