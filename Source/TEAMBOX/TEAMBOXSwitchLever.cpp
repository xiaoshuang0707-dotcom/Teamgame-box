// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXSwitchLever.h"
#include "TEAMBOXCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ATEAMBOXSwitchLever::ATEAMBOXSwitchLever()
{
	PrimaryActorTick.bCanEverTick = false;
	TriggerComponent = nullptr;
	LeverMeshComponent = nullptr;
}

void ATEAMBOXSwitchLever::BeginPlay()
{
	Super::BeginPlay();

	// Find the lever mesh by exact component name in BP_Switch_Lever.
	if (LeverMeshComponent == nullptr)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		GetComponents<UStaticMeshComponent>(MeshComponents);
		for (UStaticMeshComponent* MeshComp : MeshComponents)
		{
			if (MeshComp && MeshComp->GetName() == TEXT("SM_PullSwitch_02"))
			{
				LeverMeshComponent = MeshComp;
				break;
			}
		}
	}

	if (TriggerComponent)
	{
		TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &ATEAMBOXSwitchLever::OnTriggerBeginOverlap);
		TriggerComponent->OnComponentEndOverlap.AddDynamic(this, &ATEAMBOXSwitchLever::OnTriggerEndOverlap);
	}

	// Initialize mesh visual to match current initial switch state.
	SyncLeverMeshRotationWithState();
}

void ATEAMBOXSwitchLever::ToggleSwitch()
{
	const bool bWasActivated = GetIsActivated();

	if (bWasActivated)
	{
		DeactivateSwitch();
	}
	else
	{
		ActivateSwitch();
	}

	if (ToggleSFX && bWasActivated != GetIsActivated())
	{
		UGameplayStatics::PlaySoundAtLocation(this, ToggleSFX, GetActorLocation());
	}

	if (bWasActivated != GetIsActivated())
	{
		SyncLeverMeshRotationWithState();
	}
}

void ATEAMBOXSwitchLever::SyncLeverMeshRotationWithState()
{
	if (LeverMeshComponent == nullptr)
	{
		return;
	}

	FRotator RelativeRot = LeverMeshComponent->GetRelativeRotation();
	RelativeRot.Roll = GetIsActivated() ? 90.0f : 0.0f; // X axis (UE first/red field)
	LeverMeshComponent->SetRelativeRotation(RelativeRot);
}

void ATEAMBOXSwitchLever::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ATEAMBOXCharacter* Character = Cast<ATEAMBOXCharacter>(OtherActor);
	if (Character)
	{
		Character->RegisterOverlappingLever(this);
	}
}

void ATEAMBOXSwitchLever::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATEAMBOXCharacter* Character = Cast<ATEAMBOXCharacter>(OtherActor);
	if (Character)
	{
		Character->UnregisterOverlappingLever(this);
	}
}
