#pragma once

#include "DreamPassSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "DreamPassMaterialInstance.generated.h"

/** A pool-owned MID whose reset never changes the shared parent material's editor previews. */
UCLASS(Transient, NotBlueprintable)
class UDreamPassMaterialInstance final : public UMaterialInstanceDynamic
{
	GENERATED_BODY()

public:
	void InitializeForPool(UMaterialInterface* Base) { InitializeMID(Base); }

	/**
	 * Readies the instance for a use that sets Parameters, or sets parameters not known in advance when null. The
	 * overrides the previous use left are cleared, restoring the parent's values, unless both uses are known and this
	 * one sets every parameter the previous one did: then it overwrites all of them, and clearing would only cost an
	 * InitResources and an editor material-change notification every frame.
	 */
	void PrepareForPoolUse(const TConstArrayView<FDreamPassMaterialParameterKey>* Parameters);

private:
	void ResetPoolParameters();
	bool HasPoolParameterOverrides() const;

	/** What the previous use set, when it was known. */
	TArray<FDreamPassMaterialParameterKey> PoolParameters;
	bool bPoolParametersKnown = false;
};
