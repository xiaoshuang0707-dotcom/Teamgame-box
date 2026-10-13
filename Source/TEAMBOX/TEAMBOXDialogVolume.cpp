#include "TEAMBOXDialogVolume.h"

#include "TEAMBOXCharacter.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

ATEAMBOXDialogVolume::ATEAMBOXDialogVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBox"));
	RootComponent = VolumeBox;
	VolumeBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	VolumeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VolumeBox->SetCollisionObjectType(ECC_WorldDynamic);
	VolumeBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	VolumeBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	VolumeBox->SetGenerateOverlapEvents(true);

	DialogWidgetClass = nullptr;
	DialogText = FText::GetEmpty();
	DialogTextFunctionName = TEXT("SetDialogText");
	DialogTextPropertyName = TEXT("DialogText");
	ActiveDialogWidget = nullptr;
}

void ATEAMBOXDialogVolume::BeginPlay()
{
	Super::BeginPlay();

	if (VolumeBox)
	{
		VolumeBox->OnComponentBeginOverlap.AddDynamic(this, &ATEAMBOXDialogVolume::OnOverlapBegin);
		VolumeBox->OnComponentEndOverlap.AddDynamic(this, &ATEAMBOXDialogVolume::OnOverlapEnd);
	}
}

void ATEAMBOXDialogVolume::OnOverlapBegin(
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

	if (DialogWidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DialogVolume] %s: DialogWidgetClass is not set."), *GetName());
		return;
	}

	if (ActiveDialogWidget == nullptr)
	{
		APlayerController* PC = Cast<APlayerController>(Character->GetController());
		if (PC == nullptr)
		{
			return;
		}

		ActiveDialogWidget = CreateWidget<UUserWidget>(PC, DialogWidgetClass);
		if (ActiveDialogWidget == nullptr)
		{
			return;
		}
	}

	ApplyDialogTextToWidget(ActiveDialogWidget);

	if (!ActiveDialogWidget->IsInViewport())
	{
		ActiveDialogWidget->AddToViewport();
	}
}

void ATEAMBOXDialogVolume::OnOverlapEnd(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (Cast<ATEAMBOXCharacter>(OtherActor) == nullptr)
	{
		return;
	}

	if (ActiveDialogWidget != nullptr)
	{
		ActiveDialogWidget->RemoveFromParent();
	}
}

void ATEAMBOXDialogVolume::ApplyDialogTextToWidget(UUserWidget* WidgetInstance) const
{
	if (WidgetInstance == nullptr)
	{
		return;
	}

	if (!DialogTextFunctionName.IsNone())
	{
		if (UFunction* SetTextFunction = WidgetInstance->FindFunction(DialogTextFunctionName))
		{
			struct FDialogTextFunctionParams
			{
				FText InText;
			};

			FDialogTextFunctionParams Params;
			Params.InText = DialogText;
			WidgetInstance->ProcessEvent(SetTextFunction, &Params);
			return;
		}
	}

	if (!DialogTextPropertyName.IsNone())
	{
		if (FTextProperty* TextProperty = FindFProperty<FTextProperty>(WidgetInstance->GetClass(), DialogTextPropertyName))
		{
			TextProperty->SetPropertyValue_InContainer(WidgetInstance, DialogText);
			return;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[DialogVolume] %s: Could not apply dialog text. Missing function '%s' and property '%s' on widget '%s'."),
		*GetName(),
		*DialogTextFunctionName.ToString(),
		*DialogTextPropertyName.ToString(),
		*GetNameSafe(WidgetInstance->GetClass()));
}
