#pragma once

#include "CoreMinimal.h"
#include "DreamPassTypes.h"
#include "UObject/Object.h"

#include "DreamPassPipeline.generated.h"

class UDreamPassPipeline;
class UTextureRenderTarget2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDreamPassPipelineChanged, UDreamPassPipeline*);

/** Where one `param` of an HLSL slot pass lives in the slot's DP_Params[] block. */
struct FDreamPassSlotParamLocation
{
	FName Name;
	EDreamPassParameterType Type = EDreamPassParameterType::Float;
	/** Index into DP_Params. */
	int32 Vector = 0;
	/** First component, 0..3. */
	int32 Component = 0;
	/** 1..4. */
	int32 Width = 1;
};

namespace UE::DreamPass
{
	/** The type a `param` binding produces: its parameter's, its constant's, or Float for the weight. */
	DREAMSHADERPASS_API EDreamPassParameterType GetParamBindingType(const UDreamPassPipeline& Pipeline, const FDreamPassParamBinding& Binding);

	/**
	 * Lays the `param` bindings of an HLSL pass out in the slot's float4 block, in declaration order: a float4 takes a
	 * vector of its own, a float3 the xyz of the next vector, a float2 the xy or zw half that is free, a float, an int
	 * or a bool the next free component. The compiler writes the registry from this and the runtime packs values with
	 * it, so the two cannot disagree. False when the block (MaxSlotParamVectors) is too small or a texture is bound.
	 */
	DREAMSHADERPASS_API bool LayoutSlotParameters(TConstArrayView<FName> Names, TConstArrayView<EDreamPassParameterType> Types, TArray<FDreamPassSlotParamLocation>& OutLocations);
}

/**
 * A Custom Pass pipeline: named buffers, an ordered list of passes each with its own injection point, and the
 * parameters an activation can override. What a `.dsp` source compiles to, and what it decompiles from.
 *
 * Plain data. Nothing here runs: UDreamPassSubsystem decides which pipelines apply to a view and with what
 * parameter values, and the scene view extension runs their passes. Editing the asset in the details panel
 * takes effect on the next frame; Adopt Into Source writes the edit back to the `.dsp`.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Dream Pass Pipeline"))
class DREAMSHADERPASS_API UDreamPassPipeline : public UObject
{
	GENERATED_BODY()

public:
	/** Pipelines that have passes at the same injection point run in ascending Order, then by asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	int32 Order = 0;

	/** The injection point of a pass that does not name one. Informational once compiled: every pass carries its own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	EDreamPassInjection DefaultInjection = EDreamPassInjection::BeforePostProcess;

	/** EDreamPassViewFlags: which kinds of view the pipeline runs in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline", meta = (Bitmask, BitmaskEnum = "/Script/DreamShaderPass.EDreamPassViewFlags"))
	int32 Views = int32(EDreamPassViewFlags::Game | EDreamPassViewFlags::Editor);

	/** EDreamPassRequirementFlags: what a view must offer for the pipeline to run in it at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline", meta = (Bitmask, BitmaskEnum = "/Script/DreamShaderPass.EDreamPassRequirementFlags"))
	int32 Requires = 0;

	/** A Bool parameter; the whole pipeline is off in a view where it is false. None: always on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	FName EnabledParameter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TArray<FDreamPassParameterDesc> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TArray<FDreamPassBufferDesc> Buffers;

	/** In declaration order, which is execution order within an injection point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TArray<FDreamPassDesc> Passes;

	/** The render target asset of every exported buffer, made by the compiler next to this asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline")
	TMap<FName, TObjectPtr<UTextureRenderTarget2D>> ExportTargets;

	/** The `.dsp` this asset was compiled from, project-relative. Also in the package's DreamShader.SourceFile metadata. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source", AssetRegistrySearchable)
	FString SourceFilePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourceHash;

public:
	const FDreamPassBufferDesc* FindBuffer(FName Name) const;
	const FDreamPassParameterDesc* FindParameter(FName Name) const;
	int32 FindPassIndex(FName Name) const;

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass")
	UTextureRenderTarget2D* GetExportTarget(FName Buffer) const;

	EDreamPassViewFlags GetViewFlags() const { return EDreamPassViewFlags(uint8(Views)); }
	EDreamPassRequirementFlags GetRequirements() const { return EDreamPassRequirementFlags(uint8(Requires)); }

	/**
	 * The structural checks the runtime relies on: unique names, every binding naming a declared or built-in buffer,
	 * every pass having what its kind needs. A `.dsp` that compiled passes them all; an asset edited by hand may not,
	 * and the runtime then skips the passes that fail (each named once in the log). True when nothing failed.
	 */
	bool Validate(TArray<FText>* OutProblems = nullptr) const;

	/** Whether pass PassIndex passed Validate; refreshed by NotifyChanged. */
	bool IsPassUsable(int32 PassIndex) const { return UsablePasses.IsValidIndex(PassIndex) && UsablePasses[PassIndex]; }

	/** A counter that moves whenever the asset changes; snapshots compare it to tell a rebuilt pipeline. */
	uint32 GetRevision() const { return Revision; }

	/** Re-validates, moves the revision on and broadcasts OnPipelineChanged. The compiler calls it after a rebuild. */
	void NotifyChanged();

	/** Broadcast on the game thread whenever a pipeline asset changes (rebuilt, edited, loaded). */
	static FOnDreamPassPipelineChanged OnPipelineChanged;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RefreshUsablePasses();

	TBitArray<> UsablePasses;
	uint32 Revision = 0;
};
