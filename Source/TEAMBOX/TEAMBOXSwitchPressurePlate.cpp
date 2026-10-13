// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXSwitchPressurePlate.h"
#include "Components/BoxComponent.h"

ATEAMBOXSwitchPressurePlate::ATEAMBOXSwitchPressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;
	TriggerComponent = nullptr;
}

void ATEAMBOXSwitchPressurePlate::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerComponent)
	{
		TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &ATEAMBOXSwitchPressurePlate::OnTriggerBeginOverlap);
		TriggerComponent->OnComponentEndOverlap.AddDynamic(this, &ATEAMBOXSwitchPressurePlate::OnTriggerEndOverlap);

		TArray<AActor*> OverlappingActors;
		TriggerComponent->GetOverlappingActors(OverlappingActors);
		for (AActor* A : OverlappingActors)
		{
			if (A && A != this)
			{
				OverlappingExternalActors.Add(A);
			}
		}
		UpdateActivationFromOverlapCount();
	}
}

void ATEAMBOXSwitchPressurePlate::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	OverlappingExternalActors.Add(OtherActor);
	UpdateActivationFromOverlapCount();
}

void ATEAMBOXSwitchPressurePlate::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	OverlappingExternalActors.Remove(OtherActor);
	UpdateActivationFromOverlapCount();
}

void ATEAMBOXSwitchPressurePlate::UpdateActivationFromOverlapCount()
{
	// 移除已销毁的弱引用
	for (auto It = OverlappingExternalActors.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	const int32 Count = OverlappingExternalActors.Num();

	if (Count > 0 && !GetIsActivated())
	{
		ActivateSwitch();
	}
	else if (Count == 0 && GetIsActivated())
	{
		DeactivateSwitch();
	}
}
