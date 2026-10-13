// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXDeviceMovingPlatform.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

ATEAMBOXDeviceMovingPlatform::ATEAMBOXDeviceMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create collision component
	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	// Allow pushing pawns and other movable objects
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// Initialize default values
	// StartPosition will be captured from placed location at BeginPlay.
	StartPosition = FVector::ZeroVector;
	EndPosition = FVector(0.0f, 0.0f, 100.0f); // Relative offset from start: 100cm up
	MovementSpeed = 100.0f; // 100 cm/s
	PauseDuration = 1.0f; // 1 second pause
	bUseSmoothCurve = true;
	bMovingToEnd = true; // Start by moving to end
	bIsMoving = false;
	PauseTimeRemaining = 0.0f;
}

void ATEAMBOXDeviceMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	// Use placed actor location as runtime start position.
	StartPosition = GetActorLocation();
	bMovingToEnd = true;
	bIsMoving = false;
	PauseTimeRemaining = 0.0f;
}

void ATEAMBOXDeviceMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only update if device is active
	if (!bIsActive)
	{
		return;
	}

	// Handle pause at endpoint
	if (PauseTimeRemaining > 0.0f)
	{
		PauseTimeRemaining -= DeltaTime;
		if (PauseTimeRemaining <= 0.0f)
		{
			PauseTimeRemaining = 0.0f;
			bIsMoving = true; // Resume movement after pause
		}
		return; // Don't move during pause
	}

	// Update movement if not paused
	if (bIsMoving)
	{
		UpdateMovement(DeltaTime);
	}
}

void ATEAMBOXDeviceMovingPlatform::ActivateDevice_Implementation()
{
	// Start or resume continuous movement
	FVector CurrentLocation = GetActorLocation();
	const FVector EndWorldPosition = StartPosition + EndPosition;
	float DistanceToStart = FVector::Dist(CurrentLocation, StartPosition);
	float DistanceToEnd = FVector::Dist(CurrentLocation, EndWorldPosition);

	// If very close to an endpoint, determine direction based on which endpoint
	if (DistanceToStart < 1.0f)
	{
		// At start position, move to end
		bMovingToEnd = true;
		bIsMoving = true;
		PauseTimeRemaining = 0.0f;
	}
	else if (DistanceToEnd < 1.0f)
	{
		// At end position, move to start
		bMovingToEnd = false;
		bIsMoving = true;
		PauseTimeRemaining = 0.0f;
	}
	else
	{
		// Somewhere in between, continue current direction (or start moving to end if not moving)
		if (!bIsMoving)
		{
			// Determine direction based on which endpoint is closer
			bMovingToEnd = (DistanceToEnd < DistanceToStart);
		}
		bIsMoving = true;
		PauseTimeRemaining = 0.0f;
	}
}

void ATEAMBOXDeviceMovingPlatform::DeactivateDevice_Implementation()
{
	// Stop movement immediately, keep current position
	bIsMoving = false;
	PauseTimeRemaining = 0.0f;
}

void ATEAMBOXDeviceMovingPlatform::UpdateMovement(float DeltaTime)
{
	FVector CurrentLocation = GetActorLocation();
	const FVector EndWorldPosition = StartPosition + EndPosition;
	FVector TargetPosition = bMovingToEnd ? EndWorldPosition : StartPosition;
	FVector Direction = (TargetPosition - CurrentLocation).GetSafeNormal();
	float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);

	// Check if we've reached the target
	if (DistanceToTarget < 1.0f) // 1cm threshold
	{
		SetActorLocation(TargetPosition);
		
		// Reverse direction for next movement
		bMovingToEnd = !bMovingToEnd;
		
		// Start pause timer (if pause duration > 0, movement will resume after pause)
		if (PauseDuration > 0.0f)
		{
			bIsMoving = false;
			PauseTimeRemaining = PauseDuration;
		}
		// If no pause, continue immediately in new direction
		// (bIsMoving remains true, next frame will move in new direction)
		return;
	}

	// Calculate movement distance this frame
	float MovementDistance = MovementSpeed * DeltaTime;

	// Calculate progress for smooth curve (0.0 to 1.0)
	float TotalDistance = FVector::Dist(StartPosition, EndWorldPosition);
	float CurrentProgress = 0.0f;
	
	if (TotalDistance > 0.01f)
	{
		// Calculate how far we've moved along the path
		FVector StartToCurrent = CurrentLocation - StartPosition;
		FVector StartToEnd = EndWorldPosition - StartPosition;
		FVector StartToEndNormalized = StartToEnd.GetSafeNormal();
		
		float ProjectedDistance = FVector::DotProduct(StartToCurrent, StartToEndNormalized);
		CurrentProgress = FMath::Clamp(ProjectedDistance / TotalDistance, 0.0f, 1.0f);
	}

	// Apply smooth curve if enabled
	float SpeedMultiplier = 1.0f;
	if (bUseSmoothCurve)
	{
		// Use SmoothStep for ease in/out
		float SmoothProgress = FMath::SmoothStep(0.0f, 1.0f, CurrentProgress);
		float NextProgress = FMath::Clamp(CurrentProgress + (MovementDistance / TotalDistance), 0.0f, 1.0f);
		float NextSmoothProgress = FMath::SmoothStep(0.0f, 1.0f, NextProgress);
		
		// Adjust speed based on curve derivative
		if (FMath::Abs(NextProgress - CurrentProgress) > 0.0001f)
		{
			SpeedMultiplier = FMath::Abs((NextSmoothProgress - SmoothProgress) / (NextProgress - CurrentProgress));
			SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.1f, 2.0f); // Clamp to reasonable range
		}
	}

	// Calculate new location
	FVector NewLocation = CurrentLocation + (Direction * MovementDistance * SpeedMultiplier);
	FVector MovementDelta = NewLocation - CurrentLocation;

	// Clamp to not overshoot target
	float DistanceAfterMove = FVector::Dist(NewLocation, TargetPosition);
	if (DistanceAfterMove < MovementDistance * SpeedMultiplier || DistanceAfterMove > DistanceToTarget)
	{
		// We would overshoot or have already passed the target
		NewLocation = TargetPosition;
		DistanceAfterMove = 0.0f;
		
		// Reverse direction for next movement
		bMovingToEnd = !bMovingToEnd;
		
		// Start pause timer (if pause duration > 0, movement will resume after pause)
		if (PauseDuration > 0.0f)
		{
			bIsMoving = false;
			PauseTimeRemaining = PauseDuration;
		}
		// If no pause, continue immediately in new direction
		// (bIsMoving remains true, next frame will move in new direction)
	}

	// Use Sweep to detect what we'll hit
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;

	UWorld* World = GetWorld();
	if (World)
	{
		// First, check what we'll hit with a sweep
		bool bHit = World->SweepSingleByChannel(
			HitResult,
			CurrentLocation,
			NewLocation,
			FQuat::Identity,
			CollisionComponent->GetCollisionObjectType(),
			FCollisionShape::MakeBox(CollisionComponent->GetScaledBoxExtent()),
			QueryParams
		);

		// Push any hit actors (like pawns) before moving
		if (bHit && HitResult.bBlockingHit && HitResult.GetActor())
		{
			AActor* HitActor = HitResult.GetActor();
			
			// Check if it's a pawn/character that we should push
			APawn* HitPawn = Cast<APawn>(HitActor);
			ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
			
			if (HitPawn || HitCharacter)
			{
				// Push the pawn/character by the same movement delta
				FVector NewPawnLocation = HitActor->GetActorLocation() + MovementDelta;
				HitActor->SetActorLocation(NewPawnLocation, false);
			}
			else
			{
				// For other actors, try to push them if they have a primitive component
				UPrimitiveComponent* HitComponent = HitResult.GetComponent();
				if (HitComponent && HitComponent->IsSimulatingPhysics())
				{
					// Add impulse to push physics objects
					HitComponent->AddImpulse(MovementDelta * 100.0f, NAME_None, true);
				}
				else if (HitComponent && HitComponent->Mobility == EComponentMobility::Movable)
				{
					// Push movable static actors
					HitActor->SetActorLocation(HitActor->GetActorLocation() + MovementDelta, false);
				}
			}
		}
	}

	// Now move the platform (this may still hit, but we've already pushed movable objects)
	// SetActorLocation with sweep - it should automatically ignore this actor
	bool bMoved = SetActorLocation(NewLocation, true, &HitResult);

	// If blocked by immovable world geometry, move as far as possible
	if (!bMoved && HitResult.bBlockingHit)
	{
		// Check what we hit
		AActor* HitActor = HitResult.GetActor();
		UPrimitiveComponent* HitComp = HitResult.GetComponent();
		
		// Check if the hit component belongs to this actor (shouldn't happen, but check anyway)
		bool bHitOurselves = false;
		if (HitActor == this)
		{
			bHitOurselves = true;
		}
		else if (HitComp)
		{
			// Check if the component belongs to this actor
			AActor* ComponentOwner = HitComp->GetOwner();
			if (ComponentOwner == this)
			{
				bHitOurselves = true;
			}
		}
		
		// Also check all our components to see if the hit component is one of them
		if (!bHitOurselves && HitComp)
		{
			TArray<UActorComponent*> AllComponents;
			GetComponents(AllComponents);
			for (UActorComponent* Comp : AllComponents)
			{
				if (Comp == HitComp)
				{
					bHitOurselves = true;
					break;
				}
			}
		}
		
		if (bHitOurselves)
		{
			// Hit ourselves, this shouldn't happen - ignore and try to move anyway
			SetActorLocation(NewLocation, false);
		}
		else if (HitComp && HitComp->Mobility == EComponentMobility::Static)
		{
			// Blocked by static world geometry
			// Check if we're very close to target - if so, consider it reached
			FVector ActualLocation = HitResult.Location;
			float DistanceToTargetAfterBlock = FVector::Dist(ActualLocation, TargetPosition);
			
			// If very close to target (within 5cm), consider it reached
			if (DistanceToTargetAfterBlock < 5.0f)
			{
				SetActorLocation(TargetPosition);
				
				// Reverse direction for next movement
				bMovingToEnd = !bMovingToEnd;
				
				// Start pause timer (if pause duration > 0, movement will resume after pause)
				if (PauseDuration > 0.0f)
				{
					bIsMoving = false;
					PauseTimeRemaining = PauseDuration;
				}
			}
			else
			{
				// Too far from target, move to collision point and stop
				SetActorLocation(ActualLocation);
				bIsMoving = false;
			}
		}
		else
		{
			// Blocked by something else, try to move anyway (might push it further)
			SetActorLocation(NewLocation, false);
		}
	}
}
