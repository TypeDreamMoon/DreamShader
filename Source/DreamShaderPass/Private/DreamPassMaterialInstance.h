#pragma once

#include "Materials/MaterialInstanceDynamic.h"

#include "DreamPassMaterialInstance.generated.h"

/** A pool-owned MID whose reset never changes the shared parent material's editor previews. */
UCLASS(Transient, NotBlueprintable)
class UDreamPassMaterialInstance final : public UMaterialInstanceDynamic
{
	GENERATED_BODY()

public:
	void InitializeForPool(UMaterialInterface* Base) { InitializeMID(Base); }
	void ResetPoolParameters();
};
