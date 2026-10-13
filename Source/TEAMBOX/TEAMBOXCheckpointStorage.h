#pragma once

#include "CoreMinimal.h"

// Runtime-only shared storage for checkpoint position.
// No SaveGame, no persistence across sessions.
namespace TEAMBOXCheckpointStorage
{
	inline FVector GLastCheckpointCenter = FVector::ZeroVector;
	inline bool bHasCheckpoint = false;
}

