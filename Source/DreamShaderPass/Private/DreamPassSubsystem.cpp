#include "DreamPassSubsystem.h"

#include "DreamPassConsole.h"
#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamShaderPassModule.h"

#include "Algo/StableSort.h"
#include "ClearQuad.h"
#include "Components/PrimitiveComponent.h"
#include "CoreGlobals.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDevice.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "TextureResource.h"

#if DREAMSHADER_WITH_CUSTOM_PASS
#include "Render/DreamPassSceneViewExtension.h"
#endif

namespace UE::DreamPass::Private
{
	/** A pool unused for this many frames lets its instances go. */
	static constexpr uint64 MaterialPoolIdleFrames = 600;
}

const FDreamPassParameterValue* FDreamPassResolvedPipeline::FindValue(FName Name) const
{
	if (!Pipeline)
	{
		return nullptr;
	}
	const int32 Index = Pipeline->Parameters.IndexOfByPredicate([Name](const FDreamPassParameterDesc& Desc) { return Desc.Name == Name; });
	return Values.IsValidIndex(Index) ? &Values[Index] : nullptr;
}

UDreamPassSubsystem* UDreamPassSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UDreamPassSubsystem>() : nullptr;
}

bool UDreamPassSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game, PIE and editor worlds (UWorldSubsystem::DoesSupportWorldType). An editor preview world -- the material
	// editor's, a Blueprint editor's -- gets none: its views are not what a pipeline is written for.
	return Super::ShouldCreateSubsystem(Outer);
}

void UDreamPassSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RefreshGlobalPipelines();

#if WITH_EDITOR
	SettingsChangedHandle = GetMutableDefault<UDreamPassSettings>()->OnSettingChanged().AddUObject(this, &UDreamPassSubsystem::OnSettingsChanged);
#endif

	EndFrameHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UDreamPassSubsystem::OnEndFrame);

#if DREAMSHADER_WITH_CUSTOM_PASS
	// One extension per world: FWorldSceneViewExtension is active for the views of this world only, so a PIE world and
	// the editor world beside it never run each other's pipelines.
	ViewExtension = FSceneViewExtensions::NewExtension<FDreamPassSceneViewExtension>(GetWorld(), this);
#endif
}

void UDreamPassSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (SettingsChangedHandle.IsValid())
	{
		GetMutableDefault<UDreamPassSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
#endif

	FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
	EndFrameHandle.Reset();
	ReleaseNaniteStencils(true);
	// The render targets are assets every world shares: what this world last wrote would stay in them, and show in the
	// editor's viewports once a PIE session ends.
	ReleaseExportTargets(true);

	ApiActivations.Reset();
	Sources.Reset();
	Lists.Reset();
	PrimitiveLayers.Reset();
	GlobalPipelines.Reset();
	MaterialPools.Reset();

	// A family being rendered holds a reference of its own; the extension goes when the last of those is done.
	ViewExtension.Reset();

	Super::Deinitialize();
}

void UDreamPassSubsystem::OnSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	RefreshGlobalPipelines();
}

void UDreamPassSubsystem::RefreshGlobalPipelines()
{
	GlobalPipelines.Reset();
	for (const FDreamPassGlobalPipeline& Entry : UDreamPassSettings::Get().GlobalPipelines)
	{
		// Loaded once here rather than on first use: the first frame a pipeline shows up in must not stall the game
		// thread on a synchronous load in the middle of BeginRenderViewFamily.
		GlobalPipelines.Add(Entry.bEnabled ? Entry.Pipeline.LoadSynchronous() : nullptr);
	}
}

FDreamPassHandle UDreamPassSubsystem::AddPipeline(UDreamPassPipeline* Pipeline, float Priority, const TArray<FDreamPassParameterOverride>& Overrides, int32 PlayerIndex)
{
	FDreamPassHandle Handle;
	if (!Pipeline)
	{
		return Handle;
	}

	Handle.Id = NextHandleId++;
	FDreamPassApiActivation& Activation = ApiActivations.Add(Handle.Id);
	Activation.Pipeline = Pipeline;
	Activation.Priority = Priority;
	Activation.PlayerIndex = PlayerIndex;
	Activation.Overrides = Overrides;
	return Handle;
}

bool UDreamPassSubsystem::RemovePipeline(FDreamPassHandle Handle)
{
	return ApiActivations.Remove(Handle.Id) > 0;
}

bool UDreamPassSubsystem::SetParameter(FDreamPassHandle Handle, FName Name, const FDreamPassParameterValue& Value)
{
	FDreamPassApiActivation* Activation = ApiActivations.Find(Handle.Id);
	if (!Activation || !Activation->Pipeline)
	{
		return false;
	}

	const FDreamPassParameterDesc* Desc = Activation->Pipeline->FindParameter(Name);
	if (!Desc || Desc->Default.Type != Value.Type)
	{
		return false;
	}

	if (FDreamPassParameterOverride* Existing = Activation->Overrides.FindByPredicate([Name](const FDreamPassParameterOverride& Override) { return Override.Name == Name; }))
	{
		Existing->Value = Value;
	}
	else
	{
		Activation->Overrides.Add({ Name, Value });
	}
	return true;
}

bool UDreamPassSubsystem::SetWeight(FDreamPassHandle Handle, float Weight)
{
	FDreamPassApiActivation* Activation = ApiActivations.Find(Handle.Id);
	if (!Activation)
	{
		return false;
	}
	Activation->Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
	return true;
}

bool UDreamPassSubsystem::IsActive(FDreamPassHandle Handle) const
{
	return ApiActivations.Contains(Handle.Id);
}

void UDreamPassSubsystem::AddToList(FName List, UPrimitiveComponent* Primitive)
{
	if (List.IsNone() || !Primitive)
	{
		return;
	}
	TArray<TWeakObjectPtr<UPrimitiveComponent>>& Members = Lists.FindOrAdd(List);
	Members.AddUnique(Primitive);
}

void UDreamPassSubsystem::RemoveFromList(FName List, UPrimitiveComponent* Primitive)
{
	if (TArray<TWeakObjectPtr<UPrimitiveComponent>>* Members = Lists.Find(List))
	{
		Members->Remove(Primitive);
		if (Members->IsEmpty())
		{
			Lists.Remove(List);
		}
	}
}

void UDreamPassSubsystem::ClearList(FName List)
{
	Lists.Remove(List);
}

void UDreamPassSubsystem::SetPrimitiveLayers(UPrimitiveComponent* Primitive, uint32 LayerMask)
{
	if (!Primitive)
	{
		return;
	}
	if (LayerMask == 0)
	{
		PrimitiveLayers.Remove(Primitive);
	}
	else
	{
		PrimitiveLayers.Add(Primitive, LayerMask);
	}
}

uint32 UDreamPassSubsystem::GetPrimitiveLayers(const UPrimitiveComponent* Primitive) const
{
	const uint32* Mask = PrimitiveLayers.Find(MakeWeakObjectPtr(const_cast<UPrimitiveComponent*>(Primitive)));
	return Mask ? *Mask : 0;
}

void UDreamPassSubsystem::ForEachPrimitiveInLayers(uint32 LayerMask, TFunctionRef<void(UPrimitiveComponent&)> Visit) const
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, uint32>& Pair : PrimitiveLayers)
	{
		UPrimitiveComponent* Primitive = Pair.Key.Get();
		if (Primitive && (Pair.Value & LayerMask) != 0)
		{
			Visit(*Primitive);
		}
	}
}

void UDreamPassSubsystem::ForEachPrimitiveInList(FName List, TFunctionRef<void(UPrimitiveComponent&)> Visit) const
{
	if (const TArray<TWeakObjectPtr<UPrimitiveComponent>>* Members = Lists.Find(List))
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Member : *Members)
		{
			if (UPrimitiveComponent* Primitive = Member.Get())
			{
				Visit(*Primitive);
			}
		}
	}
}

void UDreamPassSubsystem::RegisterSource(UObject* Object, IDreamPassActivationSource* Source)
{
	if (!Object || !Source)
	{
		return;
	}
	for (FSourceEntry& Entry : Sources)
	{
		if (Entry.Object.Get() == Object)
		{
			Entry.Source = Source;
			return;
		}
	}
	Sources.Add({ Object, Source });
}

void UDreamPassSubsystem::UnregisterSource(UObject* Object)
{
	Sources.RemoveAll([Object](const FSourceEntry& Entry) { return !Entry.Object.IsValid() || Entry.Object.Get() == Object; });
}

void UDreamPassSubsystem::PruneStaleEntries()
{
	Sources.RemoveAll([](const FSourceEntry& Entry) { return !Entry.Object.IsValid(); });

	for (auto It = PrimitiveLayers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = Lists.CreateIterator(); It; ++It)
	{
		It.Value().RemoveAll([](const TWeakObjectPtr<UPrimitiveComponent>& Member) { return !Member.IsValid(); });
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

bool UDreamPassSubsystem::RequestNaniteStencil(UPrimitiveComponent& Primitive, int32 Value)
{
	FNaniteStencilAssignment* Assignment = NaniteStencilAssignments.Find(MakeWeakObjectPtr(&Primitive));
	if (!Assignment)
	{
		Assignment = &NaniteStencilAssignments.Add(MakeWeakObjectPtr(&Primitive));
		Assignment->bOriginalRenderCustomDepth = Primitive.bRenderCustomDepth != 0;
		Assignment->OriginalStencilValue = Primitive.CustomDepthStencilValue;
	}
	else if (Assignment->LastRequestFrame == GFrameCounter && Assignment->Value != Value)
	{
		return false;
	}

	Assignment->Value = Value;
	Assignment->LastRequestFrame = GFrameCounter;

	// Both setters return at once when the value is already there, so asking every frame costs nothing after the first.
	// Turning custom depth on recreates the primitive's proxy, which the next frame renders.
	Primitive.SetRenderCustomDepth(true);
	Primitive.SetCustomDepthStencilValue(Value);
	return true;
}

void UDreamPassSubsystem::ReleaseNaniteStencils(bool bAll)
{
	for (auto It = NaniteStencilAssignments.CreateIterator(); It; ++It)
	{
		const FNaniteStencilAssignment& Assignment = It.Value();
		if (!bAll && Assignment.LastRequestFrame == GFrameCounter)
		{
			continue;
		}
		if (UPrimitiveComponent* Primitive = It.Key().Get())
		{
			Primitive->SetCustomDepthStencilValue(Assignment.OriginalStencilValue);
			Primitive->SetRenderCustomDepth(Assignment.bOriginalRenderCustomDepth);
		}
		It.RemoveCurrent();
	}
}

void UDreamPassSubsystem::OnEndFrame()
{
	// Nothing can fill an export any more: all of them go back to their clear values. Otherwise, when a view that could
	// claim the export rendered this frame, every export still filled was noted during it; one that was not belongs to a
	// pipeline that stopped running there. A frame that rendered no such view -- a minimised window, an editor viewport
	// that is not real-time -- keeps them all.
	if (!ExportTargets.IsEmpty())
	{
		if (!HasAnyActivation() || !UE::DreamPass::IsEnabledByConsole())
		{
			ReleaseExportTargets(true);
		}
		else if (ExportViewFrame == GFrameCounter)
		{
			ReleaseExportTargets(false);
		}
	}

	if (NaniteStencilAssignments.IsEmpty())
	{
		return;
	}

	// Nothing can ask any more.
	if (!HasAnyActivation() || !UE::DreamPass::IsEnabledByConsole())
	{
		ReleaseNaniteStencils(true);
		return;
	}

	// BuildFamilySnapshot begins every family it sets up with BeginMaterialFrame. When this frame rendered with the
	// extension at all, every pass that still wants a primitive asked for it during the frame, and the rest go back.
	// When nothing rendered -- a minimised window, an editor viewport that is not real-time -- what was asked last stays.
	if (MaterialFrame == GFrameCounter)
	{
		ReleaseNaniteStencils(false);
	}
}

bool UDreamPassSubsystem::HasAnyActivation() const
{
	if (!UDreamPassSettings::Get().bEnabled)
	{
		return false;
	}
	for (const TObjectPtr<UDreamPassPipeline>& Pipeline : GlobalPipelines)
	{
		if (Pipeline)
		{
			return true;
		}
	}
	return !ApiActivations.IsEmpty() || !Sources.IsEmpty();
}

void UDreamPassSubsystem::GatherActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const
{
	const TArray<FDreamPassGlobalPipeline>& Globals = UDreamPassSettings::Get().GlobalPipelines;
	for (int32 Index = 0; Index < GlobalPipelines.Num() && Index < Globals.Num(); ++Index)
	{
		if (UDreamPassPipeline* Pipeline = GlobalPipelines[Index])
		{
			FDreamPassActivation& Activation = OutActivations.AddDefaulted_GetRef();
			Activation.Pipeline = Pipeline;
			Activation.Priority = Globals[Index].Priority;
			Activation.Weight = 1.0f;
			Activation.Overrides = Globals[Index].Overrides;
		}
	}

	for (const TPair<int64, FDreamPassApiActivation>& Pair : ApiActivations)
	{
		const FDreamPassApiActivation& Api = Pair.Value;
		if (!Api.Pipeline || (Api.PlayerIndex != INDEX_NONE && Api.PlayerIndex != View.PlayerIndex))
		{
			continue;
		}
		FDreamPassActivation& Activation = OutActivations.AddDefaulted_GetRef();
		Activation.Pipeline = Api.Pipeline;
		Activation.Priority = Api.Priority;
		Activation.Weight = Api.Weight;
		Activation.Overrides = Api.Overrides;
	}

	for (const FSourceEntry& Entry : Sources)
	{
		if (Entry.Object.IsValid() && Entry.Source)
		{
			Entry.Source->GatherDreamPassActivations(View, OutActivations);
		}
	}
}

void UDreamPassSubsystem::ResolveView(const FDreamPassViewQuery& View, TArray<FDreamPassResolvedPipeline>& OutPipelines) const
{
	OutPipelines.Reset();
	if (!UDreamPassSettings::Get().bEnabled)
	{
		return;
	}

	TArray<FDreamPassActivation> Activations;
	GatherActivations(View, Activations);

	// Ascending priority, stable: an activation applied later blends over the ones before it, so the highest
	// priority is the one whose values win -- as post-process volumes do.
	Algo::StableSortBy(Activations, &FDreamPassActivation::Priority);

	TMap<UDreamPassPipeline*, int32> IndexByPipeline;
	for (const FDreamPassActivation& Activation : Activations)
	{
		UDreamPassPipeline* Pipeline = Activation.Pipeline;
		if (!Pipeline || Activation.Weight <= 0.0f || UE::DreamPass::IsPipelineDisabledByConsole(Pipeline->GetName()))
		{
			continue;
		}

		if (!EnumHasAnyFlags(Pipeline->GetViewFlags(), View.ViewKind)
			|| !EnumHasAllFlags(View.Capabilities, Pipeline->GetRequirements()))
		{
			continue;
		}

		int32* Found = IndexByPipeline.Find(Pipeline);
		if (!Found)
		{
			const int32 NewIndex = OutPipelines.AddDefaulted();
			FDreamPassResolvedPipeline& Resolved = OutPipelines[NewIndex];
			Resolved.Pipeline = Pipeline;
			Resolved.Values.Reserve(Pipeline->Parameters.Num());
			for (const FDreamPassParameterDesc& Desc : Pipeline->Parameters)
			{
				Resolved.Values.Add(Desc.Default);
			}
			Found = &IndexByPipeline.Add(Pipeline, NewIndex);
		}

		FDreamPassResolvedPipeline& Resolved = OutPipelines[*Found];
		Resolved.Weight = FMath::Max(Resolved.Weight, FMath::Min(Activation.Weight, 1.0f));
		for (const FDreamPassParameterOverride& Override : Activation.Overrides)
		{
			const int32 ParameterIndex = Pipeline->Parameters.IndexOfByPredicate([&Override](const FDreamPassParameterDesc& Desc) { return Desc.Name == Override.Name; });
			if (Resolved.Values.IsValidIndex(ParameterIndex))
			{
				Resolved.Values[ParameterIndex] = Resolved.Values[ParameterIndex].BlendTowards(Override.Value, Activation.Weight);
			}
		}
	}

	OutPipelines.RemoveAll([](const FDreamPassResolvedPipeline& Resolved)
	{
		if (Resolved.Weight <= 0.0f)
		{
			return true;
		}
		if (!Resolved.Pipeline->EnabledParameter.IsNone())
		{
			const FDreamPassParameterValue* Enabled = Resolved.FindValue(Resolved.Pipeline->EnabledParameter);
			return Enabled && Enabled->Type == EDreamPassParameterType::Bool && !Enabled->Bool;
		}
		return false;
	});

	OutPipelines.Sort([](const FDreamPassResolvedPipeline& A, const FDreamPassResolvedPipeline& B)
	{
		if (A.Pipeline->Order != B.Pipeline->Order)
		{
			return A.Pipeline->Order < B.Pipeline->Order;
		}
		return A.Pipeline->GetPathName() < B.Pipeline->GetPathName();
	});
}

void UDreamPassSubsystem::GatherPrimitiveSelections(TMap<uint32, uint32>& OutLayersByPrimitiveId, TMap<FName, TArray<uint32>>& OutListMembers) const
{
	OutLayersByPrimitiveId.Reset();
	OutListMembers.Reset();

	// A component that is not registered has no scene primitive yet -- or no longer -- and its id is 0; it is left
	// out rather than matched against nothing. The id stays the same across a re-register (PrimitiveSceneInfo.h).
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, uint32>& Pair : PrimitiveLayers)
	{
		const UPrimitiveComponent* Primitive = Pair.Key.Get();
		if (Primitive && Primitive->IsRegistered() && Primitive->GetPrimitiveSceneId().IsValid())
		{
			OutLayersByPrimitiveId.Add(Primitive->GetPrimitiveSceneId().PrimIDValue, Pair.Value);
		}
	}

	for (const TPair<FName, TArray<TWeakObjectPtr<UPrimitiveComponent>>>& Pair : Lists)
	{
		TArray<uint32>& Ids = OutListMembers.FindOrAdd(Pair.Key);
		for (const TWeakObjectPtr<UPrimitiveComponent>& Member : Pair.Value)
		{
			const UPrimitiveComponent* Primitive = Member.Get();
			if (Primitive && Primitive->IsRegistered() && Primitive->GetPrimitiveSceneId().IsValid())
			{
				Ids.Add(Primitive->GetPrimitiveSceneId().PrimIDValue);
			}
		}
	}
}

/**
 * Clears Target to Color after the render commands queued so far: UKismetRenderingLibrary::ClearRenderTarget2D
 * (E/Private/KismetRenderingLibrary.cpp:55-76) without the world it asks for, which a world being torn down may no
 * longer give. The resource outlives the command: releasing it is a render command too, queued after this one.
 */
static void ClearExportTarget(UTextureRenderTarget2D& Target, const FLinearColor& Color)
{
	FTextureRenderTargetResource* Resource = Target.GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(DreamPassClearExport)([Resource, Color](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* Texture = Resource->GetRenderTargetTexture();
		if (!Texture)
		{
			return;
		}
		RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::Unknown, ERHIAccess::RTV));
		FRHIRenderPassInfo PassInfo(Texture, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(PassInfo, TEXT("DreamPassClearExport"));
		DrawClearQuad(RHICmdList, Color);
		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::RTV, ERHIAccess::SRVMask));
	});
}

static bool CanClaimExport(const FSceneView& View, EDreamPassViewFlags ViewKind)
{
	return (ViewKind == EDreamPassViewFlags::Game && View.PlayerIndex <= 0) || ViewKind == EDreamPassViewFlags::Editor;
}

bool UDreamPassSubsystem::TryClaimExportView(const FSceneView& View, EDreamPassViewFlags ViewKind)
{
	if (!CanClaimExport(View, ViewKind) || ExportClaimFrame == GFrameCounter)
	{
		return false;
	}
	ExportClaimFrame = GFrameCounter;
	return true;
}

void UDreamPassSubsystem::NoteViewForExport(const FSceneView& View, EDreamPassViewFlags ViewKind)
{
	if (CanClaimExport(View, ViewKind))
	{
		ExportViewFrame = GFrameCounter;
	}
}

void UDreamPassSubsystem::NoteExportTarget(UTextureRenderTarget2D* Target, const FLinearColor& ClearValue)
{
	if (!Target)
	{
		return;
	}
	FExportTarget* Entry = ExportTargets.FindByPredicate([Target](const FExportTarget& Candidate) { return Candidate.Target.Get() == Target; });
	if (!Entry)
	{
		Entry = &ExportTargets.AddDefaulted_GetRef();
		Entry->Target = Target;
	}
	Entry->ClearValue = ClearValue;
	Entry->LastFrame = GFrameCounter;
}

void UDreamPassSubsystem::ReleaseExportTargets(bool bAll)
{
	for (int32 Index = ExportTargets.Num() - 1; Index >= 0; --Index)
	{
		const FExportTarget& Entry = ExportTargets[Index];
		if (!bAll && Entry.LastFrame == GFrameCounter)
		{
			continue;
		}
		// Queued after this frame's rendering, so the frame that still filled it is not cut short.
		UTextureRenderTarget2D* Target = Entry.Target.Get();
		if (Target && !IsEngineExitRequested())
		{
			ClearExportTarget(*Target, Entry.ClearValue);
		}
		ExportTargets.RemoveAtSwap(Index);
	}
}

void UDreamPassSubsystem::BeginMaterialFrame()
{
	if (MaterialFrame == GFrameCounter)
	{
		return;
	}
	MaterialFrame = GFrameCounter;

	PruneStaleEntries();

	MaterialPools.RemoveAll([](const FDreamPassMaterialPool& Pool)
	{
		return !Pool.Base || GFrameCounter - Pool.LastUsedFrame > UE::DreamPass::Private::MaterialPoolIdleFrames;
	});
	for (FDreamPassMaterialPool& Pool : MaterialPools)
	{
		Pool.Used = 0;
	}
}

UMaterialInstanceDynamic* UDreamPassSubsystem::AcquireMaterialInstance(UMaterialInterface* Base)
{
	if (!Base)
	{
		return nullptr;
	}

	BeginMaterialFrame();

	FDreamPassMaterialPool* Pool = MaterialPools.FindByPredicate([Base](const FDreamPassMaterialPool& Candidate) { return Candidate.Base == Base; });
	if (!Pool)
	{
		Pool = &MaterialPools.AddDefaulted_GetRef();
		Pool->Base = Base;
	}

	Pool->LastUsedFrame = GFrameCounter;
	if (Pool->Used == Pool->Instances.Num())
	{
		Pool->Instances.Add(UMaterialInstanceDynamic::Create(Base, this));
	}
	return Pool->Instances[Pool->Used++];
}

void UDreamPassSubsystem::DumpState(FOutputDevice& Ar) const
{
	const UWorld* World = GetWorld();
	Ar.Logf(TEXT("DreamPass: world %s (world type %d)"), World ? *World->GetName() : TEXT("?"), World ? int32(World->WorldType.GetValue()) : -1);
	Ar.Logf(TEXT("  enabled: settings %s, r.DreamPass.Enable %s"),
		UDreamPassSettings::Get().bEnabled ? TEXT("on") : TEXT("off"),
		UE::DreamPass::IsEnabledByConsole() ? TEXT("on") : TEXT("off"));

	const TArray<FDreamPassGlobalPipeline>& Globals = UDreamPassSettings::Get().GlobalPipelines;
	for (int32 Index = 0; Index < Globals.Num(); ++Index)
	{
		const UDreamPassPipeline* Loaded = GlobalPipelines.IsValidIndex(Index) ? GlobalPipelines[Index].Get() : nullptr;
		Ar.Logf(TEXT("  global %s: %s, priority %.2f, %d override(s)%s"),
			*Globals[Index].Pipeline.ToString(),
			Globals[Index].bEnabled ? TEXT("enabled") : TEXT("disabled"),
			Globals[Index].Priority,
			Globals[Index].Overrides.Num(),
			Globals[Index].bEnabled && !Loaded ? TEXT(" -- NOT LOADED") : TEXT(""));
	}

	for (const TPair<int64, FDreamPassApiActivation>& Pair : ApiActivations)
	{
		Ar.Logf(TEXT("  api #%lld %s: priority %.2f, weight %.2f, player %d, %d override(s)"),
			Pair.Key,
			Pair.Value.Pipeline ? *Pair.Value.Pipeline->GetName() : TEXT("None"),
			Pair.Value.Priority, Pair.Value.Weight, Pair.Value.PlayerIndex, Pair.Value.Overrides.Num());
	}

	for (const FSourceEntry& Entry : Sources)
	{
		Ar.Logf(TEXT("  source %s"), Entry.Object.IsValid() ? *Entry.Object->GetPathName() : TEXT("(gone)"));
	}

	for (const TPair<FName, TArray<TWeakObjectPtr<UPrimitiveComponent>>>& Pair : Lists)
	{
		Ar.Logf(TEXT("  list %s: %d primitive(s)"), *Pair.Key.ToString(), Pair.Value.Num());
	}
	Ar.Logf(TEXT("  %d primitive(s) have pass layers"), PrimitiveLayers.Num());

#if DREAMSHADER_WITH_CUSTOM_PASS
	if (ViewExtension.IsValid())
	{
		const FDreamPassSceneViewExtension::FFrameReport Report = ViewExtension->GetLastReport();
		Ar.Logf(TEXT("  last frame %llu: %d view(s) with passes, %d pass(es) run, %d skipped"),
			Report.Frame, Report.ViewsWithPasses, Report.PassesRun, Report.PassesSkipped);
		for (const FString& Skipped : Report.SkippedPasses)
		{
			Ar.Logf(TEXT("    skipped %s"), *Skipped);
		}
	}
#endif
}
