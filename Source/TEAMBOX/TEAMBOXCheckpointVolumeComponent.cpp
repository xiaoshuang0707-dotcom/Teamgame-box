#include "TEAMBOXCheckpointVolumeComponent.h"

#include "TEAMBOXCharacter.h"
#include "TEAMBOXCheckpointStorage.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"

UTEAMBOXCheckpointVolumeComponent::UTEAMBOXCheckpointVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CheckpointBox = nullptr;
	bBound = false;
}

void UTEAMBOXCheckpointVolumeComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	// Expect the blueprint to have a Box Collision component.
	if (!CheckpointBox)
	{
		CheckpointBox = Owner->FindComponentByClass<UBoxComponent>();
	}

	if (CheckpointBox == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheckpointVolume] %s: BoxComponent not found on owner."), *Owner->GetName());
		return;
	}

	if (bBound)
	{
		return;
	}

	CheckpointBox->OnComponentBeginOverlap.AddDynamic(this, &UTEAMBOXCheckpointVolumeComponent::OnCheckpointBoxBeginOverlap);
	bBound = true;
}

void UTEAMBOXCheckpointVolumeComponent::OnCheckpointBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ATEAMBOXCharacter* Character = Cast<ATEAMBOXCharacter>(OtherActor);
	if (Character == nullptr)
	{
		return;
	}

	// Record the current checkpoint box center (component location).
	TEAMBOXCheckpointStorage::GLastCheckpointCenter = CheckpointBox ? CheckpointBox->GetComponentLocation() : FVector::ZeroVector;
	TEAMBOXCheckpointStorage::bHasCheckpoint = true;
}

