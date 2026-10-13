#include "TEAMBOXLevelLinkVolume.h"

#include "TEAMBOXCharacter.h"
#include "TEAMBOXRuntimeEnergyCoreLimit.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Modules/ModuleManager.h"

ATEAMBOXLevelLinkVolume::ATEAMBOXLevelLinkVolume()
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

	TargetMapName = TEXT("");
	MaxEnergyCoreCountAfterTransition = 2;
	bHasTriggered = false;
}

void ATEAMBOXLevelLinkVolume::BeginPlay()
{
	Super::BeginPlay();

	if (VolumeBox)
	{
		VolumeBox->OnComponentBeginOverlap.AddDynamic(this, &ATEAMBOXLevelLinkVolume::OnOverlapBegin);
	}
}

void ATEAMBOXLevelLinkVolume::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasTriggered)
	{
		return;
	}

	ATEAMBOXCharacter* Character = Cast<ATEAMBOXCharacter>(OtherActor);
	if (Character == nullptr)
	{
		return;
	}

	if (TargetMapName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LevelLinkVolume] %s: TargetMapName is empty."), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Resolve map by short name under /Game via Asset Registry (no hard-coded folder path).
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	FSoftObjectPath TargetWorldPath;
	for (const FAssetData& Asset : Assets)
	{
		const FString PackageName = Asset.PackageName.ToString();
		if (!PackageName.StartsWith(TEXT("/Game/")))
		{
			continue;
		}

		if (Asset.AssetName.ToString() == TargetMapName)
		{
			TargetWorldPath = Asset.ToSoftObjectPath();
			break;
		}
	}

	if (!TargetWorldPath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[LevelLinkVolume] %s: Could not find map named '%s' under /Game."), *GetName(), *TargetMapName);
		return;
	}

	TEAMBOXRuntimeEnergyCoreLimit::PendingMaxEnergyCoreCount = MaxEnergyCoreCountAfterTransition;
	TEAMBOXRuntimeEnergyCoreLimit::bHasPending = true;

	bHasTriggered = true;

	UGameplayStatics::OpenLevelBySoftObjectPtr(World, TSoftObjectPtr<UWorld>(TargetWorldPath));
}
