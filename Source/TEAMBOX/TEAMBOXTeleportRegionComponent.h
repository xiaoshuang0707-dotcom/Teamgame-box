#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TEAMBOXTeleportRegionComponent.generated.h"

class UBoxComponent;

UCLASS(ClassGroup=(TEAMBOX), meta=(BlueprintSpawnableComponent))
class TEAMBOX_API UTEAMBOXTeleportRegionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTEAMBOXTeleportRegionComponent();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTeleportBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	UPROPERTY()
	UBoxComponent* TeleportBox;

	bool bBound;
};

