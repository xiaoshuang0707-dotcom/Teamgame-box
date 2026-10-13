#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TEAMBOXLevelLinkVolume.generated.h"

class UBoxComponent;

/**
 * 关卡衔接传送点：玩家进入后打开目标关卡，并在下一关将玩家可召唤能量核心上限设为指定值（默认 2）。
 * 目标关卡通过“地图资源名”（不含路径、不含 .umap）在 /Game 下搜索匹配。
 */
UCLASS(Blueprintable, BlueprintType)
class TEAMBOX_API ATEAMBOXLevelLinkVolume : public AActor
{
	GENERATED_BODY()

public:
	ATEAMBOXLevelLinkVolume();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components)
	UBoxComponent* VolumeBox;

	/** 目标关卡地图名（例如 FirstPersonMap），不含路径与扩展名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Link")
	FString TargetMapName;

	/** 进入目标关卡后，玩家可召唤能量核心的最大数量（固定设为该值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Link", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxEnergyCoreCountAfterTransition;

private:
	bool bHasTriggered;
};
