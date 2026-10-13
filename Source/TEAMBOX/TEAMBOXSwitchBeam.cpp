// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXSwitchBeam.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "TEAMBOXDeviceA.h"
#include "TEAMBOXEnergyCore.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ATEAMBOXSwitchBeam::ATEAMBOXSwitchBeam()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create detection box component
	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
	RootComponent = DetectionBox;
	DetectionBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	DetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionBox->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Initialize default values
	TraceChannel = ECC_Visibility;
	bEnableDebugVisualization = false;
	bWasBeamDetected = false;
}

void ATEAMBOXSwitchBeam::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize state
	bWasBeamDetected = false;
}

void ATEAMBOXSwitchBeam::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CheckBeamHits();
}

void ATEAMBOXSwitchBeam::CheckBeamHits()
{
	UWorld* World = GetWorld();
	if (World == nullptr || DetectionBox == nullptr)
	{
		return;
	}

	bool bBeamDetected = false;

	// Get detection box bounds
	FVector DetectionLocation = DetectionBox->GetComponentLocation();
	FBoxSphereBounds DetectionBounds = DetectionBox->CalcBounds(DetectionBox->GetComponentTransform());

	// Check all DeviceA beams
	TArray<AActor*> DeviceAActors;
	UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXDeviceA::StaticClass(), DeviceAActors);

	for (AActor* Actor : DeviceAActors)
	{
		ATEAMBOXDeviceA* DeviceA = Cast<ATEAMBOXDeviceA>(Actor);
		if (DeviceA == nullptr || DeviceA->ArrowComponent == nullptr)
		{
			continue;
		}

		FVector DeviceAArrowLocation = DeviceA->ArrowComponent->GetComponentLocation();
		FVector DeviceAArrowForward = DeviceA->ArrowComponent->GetForwardVector();
		FVector DeviceATraceEnd = DeviceAArrowLocation + (DeviceAArrowForward * DeviceA->MaxBeamRange);

		// Perform line trace to check if it hits detection box
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
			// Check if hit component is our detection box
			if (HitResult.Component == DetectionBox || HitResult.GetActor() == this)
			{
				bBeamDetected = true;
				if (bEnableDebugVisualization)
				{
					DrawDebugLine(World, DeviceAArrowLocation, HitResult.Location, FColor::Green, false, 0.0f, 0, 2.0f);
				}
				break;
			}
		}
	}

	// Check all EnergyCore beams (if they are receiving beams)
	if (!bBeamDetected)
	{
		TArray<AActor*> EnergyCoreActors;
		UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXEnergyCore::StaticClass(), EnergyCoreActors);

		for (AActor* Actor : EnergyCoreActors)
		{
			ATEAMBOXEnergyCore* EnergyCore = Cast<ATEAMBOXEnergyCore>(Actor);
			if (EnergyCore == nullptr || EnergyCore->GetBeamStartComponent() == nullptr)
			{
				continue;
			}

			// Only check energy cores that are receiving beams (they emit beams when receiving)
			if (!EnergyCore->IsReceivingBeam())
			{
				continue;
			}

			// Trace from energy core's beam start to check if it hits our detection box
			FVector EnergyCoreBeamStart = EnergyCore->GetBeamStartComponent()->GetComponentLocation();
			FVector EnergyCoreBeamForward = EnergyCore->GetBeamStartComponent()->GetForwardVector();
			FVector EnergyCoreTraceEnd = EnergyCoreBeamStart + (EnergyCoreBeamForward * EnergyCore->MaxBeamRange);

			// Perform line trace
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(EnergyCore);
			QueryParams.bTraceComplex = true;

			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				EnergyCoreBeamStart,
				EnergyCoreTraceEnd,
				EnergyCore->TraceChannel,
				QueryParams
			);

			if (bHit && HitResult.bBlockingHit)
			{
				// Check if hit component is our detection box
				if (HitResult.Component == DetectionBox || HitResult.GetActor() == this)
				{
					bBeamDetected = true;
					if (bEnableDebugVisualization)
					{
						DrawDebugLine(World, EnergyCoreBeamStart, HitResult.Location, FColor::Cyan, false, 0.0f, 0, 2.0f);
					}
					break;
				}
			}
		}
	}

	// Update switch state based on beam detection
	if (bBeamDetected != bWasBeamDetected)
	{
		bWasBeamDetected = bBeamDetected;
		if (bBeamDetected)
		{
			ActivateSwitch();
		}
		else
		{
			DeactivateSwitch();
		}
	}

	// Debug visualization
	if (bEnableDebugVisualization)
	{
		FColor BoxColor = bBeamDetected ? FColor::Green : FColor::Red;
		DrawDebugBox(World, DetectionLocation, DetectionBox->GetScaledBoxExtent(), 
			DetectionBox->GetComponentRotation().Quaternion(), BoxColor, false, 0.0f, 0, 2.0f);
	}
}
