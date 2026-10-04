#pragma once

#include "CoreMinimal.h"
#include "DreamPassTypes.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "DreamPassSettings.generated.h"

class UDreamPassPipeline;

/** A pipeline that applies to every view of every world, with the parameter values it applies with. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassGlobalPipeline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TSoftObjectPtr<UDreamPassPipeline> Pipeline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	bool bEnabled = true;

	/** Where its parameter overrides sit among every other activation of the same pipeline: higher wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	float Priority = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TArray<FDreamPassParameterOverride> Overrides;
};

/** Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass. */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "DreamShader Custom Pass"))
class DREAMSHADERPASS_API UDreamPassSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("DreamPlugin"); }
	virtual FName GetSectionName() const override { return TEXT("DreamShaderCustomPass"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("DreamShader.Pass.Settings", "SectionText", "DreamShader Custom Pass"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("DreamShader.Pass.Settings", "SectionDescription", "Pipelines that run in every world, and the names of the layers mesh passes select by."); }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static const UDreamPassSettings& Get() { return *GetDefault<UDreamPassSettings>(); }

	/** The bit of LayerNames[i] is 1 << i. INDEX_NONE for a name that is not in the table. */
	int32 FindLayerIndex(FName Layer) const;

	/** The mask of every name in Layers that the table has; OutUnknown receives the names it does not. */
	uint32 MakeLayerMask(TConstArrayView<FName> Layers, TArray<FName>* OutUnknown = nullptr) const;

	/** Off: no pipeline runs anywhere, whatever activates it. r.DreamPass.Enable turns it off for a session. */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime")
	bool bEnabled = true;

	/** Pipelines that apply to every view of every world. */
	UPROPERTY(Config, EditAnywhere, Category = "Pipelines")
	TArray<FDreamPassGlobalPipeline> GlobalPipelines;

	/**
	 * The pass layers, at most 32: a UDreamPassLayerComponent gives primitives some of them, and a mesh pass's
	 * `Filter = Layer(Name)` selects the primitives that have one. The order is the bit order; renaming one is safe,
	 * moving one re-targets every compiled pipeline that names it until those are compiled again.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers")
	TArray<FName> LayerNames;

	static constexpr int32 MaxLayers = 32;
};
