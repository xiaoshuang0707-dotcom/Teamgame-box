// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TEAMBOX : ModuleRules
{
	public TEAMBOX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore", "Niagara", "AssetRegistry", "UMG" });
	}
}
