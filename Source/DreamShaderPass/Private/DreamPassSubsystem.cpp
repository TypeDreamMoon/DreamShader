#include "DreamPassSubsystem.h"

#include "DreamPassConsole.h"
#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamShaderPassModule.h"

#include "Algo/StableSort.h"
#include "Components/PrimitiveComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/OutputDevice.h"
#include "SceneViewExtension.h"

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
