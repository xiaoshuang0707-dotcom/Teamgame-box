#pragma once

#include "CoreMinimal.h"

// Runtime-only: pending max energy core count to apply on next character spawn (single-player).
namespace TEAMBOXRuntimeEnergyCoreLimit
{
	inline int32 PendingMaxEnergyCoreCount = 0;
	inline bool bHasPending = false;
}
