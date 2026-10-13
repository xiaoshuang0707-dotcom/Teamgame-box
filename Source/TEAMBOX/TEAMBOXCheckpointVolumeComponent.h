#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TEAMBOXCheckpointVolumeComponent.generated.h"

class UBoxComponent;

UCLASS(ClassGroup=(TEAMBOX), meta=(BlueprintSpawnableComponent))
class TEAMBOX_API UTEAMBOXCheckpointVolumeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTEAMBOXCheckpointVolumeComponent();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnCheckpointBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	UPROPERTY()
	UBoxComponent* CheckpointBox;

	bool bBound;
};

