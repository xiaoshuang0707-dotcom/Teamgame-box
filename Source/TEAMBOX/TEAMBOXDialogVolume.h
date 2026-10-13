#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXDialogVolume.generated.h"

class UBoxComponent;
class UUserWidget;

UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXDialogVolume : public AActor
{
	GENERATED_BODY()

public:
	ATEAMBOXDialogVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Trigger volume for showing/hiding the dialog widget.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components)
	UBoxComponent* VolumeBox;

	// Existing dialog widget blueprint/class to display.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TSubclassOf<UUserWidget> DialogWidgetClass;

	// Text to show for this volume instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText DialogText;

	// Optional widget function name that receives one FText parameter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FName DialogTextFunctionName;

	// Optional widget FText property name to assign when no function is found.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FName DialogTextPropertyName;

private:
	void ApplyDialogTextToWidget(UUserWidget* WidgetInstance) const;

	UPROPERTY(Transient)
	UUserWidget* ActiveDialogWidget;
};
