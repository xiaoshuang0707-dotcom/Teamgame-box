// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXSwitchBase.h"

ATEAMBOXSwitchBase::ATEAMBOXSwitchBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize default values
	bInitialState = false;
	bIsActivated = false;
}

void ATEAMBOXSwitchBase::BeginPlay()
{
	Super::BeginPlay();
	
	// Set initial state
	bIsActivated = bInitialState;

	// Broadcast initial state so devices can sync even with BeginPlay order differences.
	if (bIsActivated)
	{
		OnSwitchActivated.Broadcast(this);
	}
	else
	{
		OnSwitchDeactivated.Broadcast(this);
	}
}

void ATEAMBOXSwitchBase::ActivateSwitch()
{
	if (!bIsActivated)
	{
		bIsActivated = true;
		OnSwitchActivated.Broadcast(this);
	}
}

void ATEAMBOXSwitchBase::DeactivateSwitch()
{
	if (bIsActivated)
	{
		bIsActivated = false;
		OnSwitchDeactivated.Broadcast(this);
	}
}

void ATEAMBOXSwitchBase::ResetSwitch()
{
	if (bIsActivated != bInitialState)
	{
		bIsActivated = bInitialState;
		if (bIsActivated)
		{
			OnSwitchActivated.Broadcast(this);
		}
		else
		{
			OnSwitchDeactivated.Broadcast(this);
		}
	}
}
