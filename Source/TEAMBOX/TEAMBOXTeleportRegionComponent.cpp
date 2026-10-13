#include "TEAMBOXTeleportRegionComponent.h"

#include "TEAMBOXCharacter.h"
#include "TEAMBOXCheckpointStorage.h"
#include "Components/BoxComponent.h"

UTEAMBOXTeleportRegionComponent::UTEAMBOXTeleportRegionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TeleportBox = nullptr;
	bBound = false;
}

void UTEAMBOXTeleportRegionComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	if (!TeleportBox)
	{
		TeleportBox = Owner->FindComponentByClass<UBoxComponent>();
	}

	if (TeleportBox == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TeleportRegion] %s: BoxComponent not found on owner."), *Owner->GetName());
		return;
	}

	if (bBound)
	{
		return;
	}

	TeleportBox->OnComponentBeginOverlap.AddDynamic(this, &UTEAMBOXTeleportRegionComponent::OnTeleportBoxBeginOverlap);
	bBound = true;
}

void UTEAMBOXTeleportRegionComponent::OnTeleportBoxBeginOverlap(
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

	if (!TEAMBOXCheckpointStorage::bHasCheckpoint)
	{
		// Per requirements: before any checkpoint, do nothing.
		return;
	}

	const FVector TargetCenter = TEAMBOXCheckpointStorage::GLastCheckpointCenter;
	Character->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);
}

