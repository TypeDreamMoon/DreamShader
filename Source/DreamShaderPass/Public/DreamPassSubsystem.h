#pragma once

#include "CoreMinimal.h"
#include "DreamPassTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "DreamPassSubsystem.generated.h"

class FDreamPassSceneViewExtension;
class FOutputDevice;
class FSceneView;
class UDreamPassPipeline;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;

/** What the subsystem asks about a view when it decides which pipelines apply to it. Game thread data only. */
struct FDreamPassViewQuery
{
	/** Where the view is: volumes weigh themselves by it. */
	FVector ViewLocation = FVector::ZeroVector;

	/** The local player the view belongs to, INDEX_NONE for an editor or capture view. */
	int32 PlayerIndex = INDEX_NONE;

	/** AActor::GetUniqueID of the view target (FSceneView::ViewActor), 0 when there is none. */
	uint32 ViewActorUniqueId = 0;

	/** Which kind of view this is: exactly one flag. */
	EDreamPassViewFlags ViewKind = EDreamPassViewFlags::Game;

	/** What the view offers, for pipelines' Requires. */
	EDreamPassRequirementFlags Capabilities = EDreamPassRequirementFlags::None;
};

/** One source's wish to run a pipeline in a view. Valid during the gather call only. */
struct FDreamPassActivation
{
	UDreamPassPipeline* Pipeline = nullptr;

	/** Higher wins: overrides are applied in ascending priority, each blended in by its weight. */
	float Priority = 0.0f;

	/** 0..1. The pipeline runs when the largest weight of all its activations is above 0. */
	float Weight = 1.0f;

	TConstArrayView<FDreamPassParameterOverride> Overrides;
};

/** Something that activates pipelines: ADreamPassVolume, UDreamPassComponent. Registered with the subsystem. */
class DREAMSHADERPASS_API IDreamPassActivationSource
{
public:
	virtual ~IDreamPassActivationSource() = default;

	/** Appends this source's activations for the view (none, when it does not apply there). */
	virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const = 0;
};

/** One pipeline as it applies to one view: the result of every activation of it, blended. */
struct FDreamPassResolvedPipeline
{
	UDreamPassPipeline* Pipeline = nullptr;

	/** The largest weight of its activations. Above 0. */
	float Weight = 0.0f;

	/** One per Pipeline->Parameters, in that order: defaults with every override blended in. */
	TArray<FDreamPassParameterValue> Values;

	const FDreamPassParameterValue* FindValue(FName Name) const;
};

/** A handle to a pipeline activated through the API. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int64 Id = 0;

	bool IsValid() const { return Id != 0; }
};

/** A pipeline activated through UDreamPassSubsystem::AddPipeline. */
USTRUCT()
struct FDreamPassApiActivation
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UDreamPassPipeline> Pipeline = nullptr;

	UPROPERTY()
	float Priority = 0.0f;

	UPROPERTY()
	float Weight = 1.0f;

	UPROPERTY()
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY()
	TArray<FDreamPassParameterOverride> Overrides;
};

/** The material instances one pass material is drawn through: one per use in a frame, kept across frames. */
USTRUCT()
struct FDreamPassMaterialPool
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Base = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Instances;

	/** How many of Instances this frame has handed out. */
	int32 Used = 0;

	/** GFrameCounter of the last frame anything was handed out; a pool idle for long is dropped. */
	uint64 LastUsedFrame = 0;
};

/**
 * Which Custom Pass pipelines apply where, in one world.
 *
 * Four kinds of source activate a pipeline: the project settings' global pipelines, ADreamPassVolume,
 * UDreamPassComponent and this subsystem's own API. For each view the subsystem gathers every activation, groups
 * them by pipeline -- a pipeline runs once per view however many sources ask for it -- and blends their parameter
 * overrides in ascending priority, each by its weight, as post-process volumes do. Priority decides whose values
 * win; a pipeline's Order decides when it runs; the two are independent.
 *
 * It also keeps what mesh passes select by: the pass layers primitives have (UDreamPassLayerComponent) and the
 * named lists the API fills.
 *
 * Game thread only. The scene view extension it owns turns its answers into an immutable snapshot per view family;
 * nothing here is read by the render thread.
 */
UCLASS()
class DREAMSHADERPASS_API UDreamPassSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDreamPassSubsystem* Get(const UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- the API source --------------------------------------------------------------------------------------

	/** Runs Pipeline in this world's views -- every view, or PlayerIndex's only -- until RemovePipeline. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (AutoCreateRefTerm = "Overrides"))
	FDreamPassHandle AddPipeline(UDreamPassPipeline* Pipeline, float Priority, const TArray<FDreamPassParameterOverride>& Overrides, int32 PlayerIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool RemovePipeline(FDreamPassHandle Handle);

	/** Sets, or adds, one parameter override of an activation. False for an unknown handle or parameter, or the wrong type. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetParameter(FDreamPassHandle Handle, FName Name, const FDreamPassParameterValue& Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetWeight(FDreamPassHandle Handle, float Weight);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool IsActive(FDreamPassHandle Handle) const;

	// --- lists and layers ------------------------------------------------------------------------------------

	/** Puts a primitive in a named list, which a mesh pass selects with `Filter = List(Name)`. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void AddToList(FName List, UPrimitiveComponent* Primitive);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void RemoveFromList(FName List, UPrimitiveComponent* Primitive);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void ClearList(FName List);

	/** The pass layer mask of a primitive (bits of UDreamPassSettings::LayerNames). 0 forgets the primitive. */
	void SetPrimitiveLayers(UPrimitiveComponent* Primitive, uint32 LayerMask);

	uint32 GetPrimitiveLayers(const UPrimitiveComponent* Primitive) const;

	// --- sources ---------------------------------------------------------------------------------------------

	/** A volume or component registers itself while it is registered with the world. */
	void RegisterSource(UObject* Object, IDreamPassActivationSource* Source);
	void UnregisterSource(UObject* Object);

	// --- resolution ------------------------------------------------------------------------------------------

	/**
	 * Every pipeline that applies to the view, with its blended parameter values and weight, in execution order
	 * (Order, then asset path). Skips pipelines whose Views or Requires the view does not meet, and those whose
	 * EnabledParameter resolves to false.
	 */
	void ResolveView(const FDreamPassViewQuery& View, TArray<FDreamPassResolvedPipeline>& OutPipelines) const;

	/** Whether anything at all could apply: the extension is inactive while this is false. */
	bool HasAnyActivation() const;

	/** The primitives every layer has (by FPrimitiveComponentId value) and the members of every list, for a snapshot. */
	void GatherPrimitiveSelections(TMap<uint32, uint32>& OutLayersByPrimitiveId, TMap<FName, TArray<uint32>>& OutListMembers) const;

	/**
	 * A material instance of Base that is this use's alone for the rest of the frame: parameters set on it reach only
	 * the pass and view it is handed out for. Two families rendered from one frame each get their own.
	 */
	UMaterialInstanceDynamic* AcquireMaterialInstance(UMaterialInterface* Base);

	/** Starts a frame of AcquireMaterialInstance: hands every instance out afresh, drops pools idle for a while. */
	void BeginMaterialFrame();

	/**
	 * Whether View is the one that writes exported buffers into their render targets this frame: the first view of
	 * the frame that is a first local player's game view or an editor viewport. One picture per render target, so
	 * split screen, a second viewport or a scene capture never overwrite it.
	 */
	bool TryClaimExportView(const FSceneView& View, EDreamPassViewFlags ViewKind);

	/** The project settings' global pipelines were edited, or the module asked: loads them again. */
	void RefreshGlobalPipelines();

	/** `DreamPass.Dump`: every source, list and layer of this world, and what the last frame ran. */
	void DumpState(FOutputDevice& Ar) const;

private:
	struct FSourceEntry
	{
		TWeakObjectPtr<UObject> Object;
		IDreamPassActivationSource* Source = nullptr;
	};

	void GatherActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const;
	void OnSettingsChanged(UObject* Settings, struct FPropertyChangedEvent& Event);
	void PruneStaleEntries();

	/** The global pipelines of the project settings, loaded; parallel to the settings' array. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDreamPassPipeline>> GlobalPipelines;

	UPROPERTY(Transient)
	TMap<int64, FDreamPassApiActivation> ApiActivations;

	UPROPERTY(Transient)
	TArray<FDreamPassMaterialPool> MaterialPools;

	TArray<FSourceEntry> Sources;
	TMap<FName, TArray<TWeakObjectPtr<UPrimitiveComponent>>> Lists;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, uint32> PrimitiveLayers;

	int64 NextHandleId = 1;
	uint64 MaterialFrame = 0;
	uint64 ExportClaimFrame = ~uint64(0);
	FDelegateHandle SettingsChangedHandle;

	TSharedPtr<FDreamPassSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
