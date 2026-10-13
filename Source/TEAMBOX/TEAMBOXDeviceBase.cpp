// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXDeviceBase.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATEAMBOXDeviceBase::ATEAMBOXDeviceBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values
	bRequireAllSwitches = true;
	bIsActive = false;
	bIsTransitioning = false;

	// Visualization defaults
	bShowConnectionLines = true;
	ActiveLineColor = FLinearColor::Green;
	InactiveLineColor = FLinearColor::Red;
	UnconnectedLineColor = FLinearColor::Gray;
	ConnectionLineThickness = 2.0f;
}

void ATEAMBOXDeviceBase::BeginPlay()
{
	Super::BeginPlay();
	
	// Subscribe to all switch events
	for (ATEAMBOXSwitchBase* Switch : TargetSwitches)
	{
		if (IsValid(Switch))
		{
			Switch->OnSwitchActivated.AddDynamic(this, &ATEAMBOXDeviceBase::OnSwitchStateChanged);
			Switch->OnSwitchDeactivated.AddDynamic(this, &ATEAMBOXDeviceBase::OnSwitchStateChanged);
		}
	}

	// Set initial state based on switch states
	UpdateDeviceState();

	// Run one more sync on next tick to avoid BeginPlay order issues.
	GetWorldTimerManager().SetTimerForNextTick(this, &ATEAMBOXDeviceBase::UpdateDeviceState);
}

void ATEAMBOXDeviceBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Draw connection lines if enabled
	if (bShowConnectionLines)
	{
		DrawConnectionLines();
	}
}

void ATEAMBOXDeviceBase::OnSwitchStateChanged(ATEAMBOXSwitchBase* Switch)
{
	// Update device state when any switch changes
	UpdateDeviceState();
}

void ATEAMBOXDeviceBase::UpdateDeviceState()
{
	// Check if device should be active based on switch states
	bool bShouldBeActive = false;

	if (TargetSwitches.Num() == 0)
	{
		// No switches assigned, device remains inactive
		bShouldBeActive = false;
	}
	else if (bRequireAllSwitches)
	{
		// All switches must be activated
		bShouldBeActive = true;
		for (ATEAMBOXSwitchBase* Switch : TargetSwitches)
		{
			if (!IsValid(Switch) || !Switch->GetIsActivated())
			{
				bShouldBeActive = false;
				break;
			}
		}
	}
	else
	{
		// Any switch activated is enough
		for (ATEAMBOXSwitchBase* Switch : TargetSwitches)
		{
			if (IsValid(Switch) && Switch->GetIsActivated())
			{
				bShouldBeActive = true;
				break;
			}
		}
	}

	// Update device state if changed
	if (bShouldBeActive != bIsActive)
	{
		bIsActive = bShouldBeActive;

		if (bIsActive)
		{
			ActivateDevice();
		}
		else
		{
			DeactivateDevice();
		}
	}
}

void ATEAMBOXDeviceBase::ActivateDevice_Implementation()
{
	// Default implementation (can be overridden in Blueprint or C++ subclasses)
}

void ATEAMBOXDeviceBase::DeactivateDevice_Implementation()
{
	// Default implementation (can be overridden in Blueprint or C++ subclasses)
}

void ATEAMBOXDeviceBase::DrawConnectionLines()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FVector DeviceLocation = GetActorLocation();

	for (ATEAMBOXSwitchBase* Switch : TargetSwitches)
	{
		FLinearColor LineColor = UnconnectedLineColor;
		
		if (IsValid(Switch))
		{
			FVector SwitchLocation = Switch->GetActorLocation();
			
			// Determine line color based on switch state
			if (Switch->GetIsActivated())
			{
				LineColor = ActiveLineColor;
			}
			else
			{
				LineColor = InactiveLineColor;
			}

			// Draw line from device to switch
			DrawDebugLine(
				World,
				DeviceLocation,
				SwitchLocation,
				LineColor.ToFColor(true),
				false, // Persistent lines
				0.0f,  // Lifetime (0 = one frame)
				0,     // Depth priority
				ConnectionLineThickness
			);

			// Draw small sphere at switch location
			DrawDebugSphere(
				World,
				SwitchLocation,
				10.0f,
				12,
				LineColor.ToFColor(true),
				false,
				0.0f,
				0,
				1.0f
			);
		}
		else
		{
			// Draw gray line to indicate invalid reference
			FVector InvalidLocation = DeviceLocation + FVector(0, 0, 50);
			DrawDebugLine(
				World,
				DeviceLocation,
				InvalidLocation,
				UnconnectedLineColor.ToFColor(true),
				false,
				0.0f,
				0,
				ConnectionLineThickness
			);
		}
	}
}
