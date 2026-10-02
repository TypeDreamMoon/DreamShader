#include "Render/DreamPassSceneViewExtension.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassConsole.h"
#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"
#include "Render/DreamPassSnapshot.h"

#include "CoreGlobals.h"
#include "EngineModule.h"
#include "FXRenderingUtils.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderingThread.h"
#include "SceneRenderTargetParameters.h"
#include "ScreenPass.h"

namespace UE::DreamPass::Private
{
	static FDelegateHandle PostOpaqueHandle;
	static FDelegateHandle PostEngineInitHandle;

	/** History entries unused for this many frames are dropped: a view that went away, a pipeline no longer active. */
	static constexpr uint64 HistoryIdleFrames = 120;

	static void OnPostOpaqueRender(FPostOpaqueRenderParameters& Parameters)
	{
		if (!Parameters.GraphBuilder)
		{
			return;
		}
		FFamilyState* Family = FindFamilyState(*Parameters.GraphBuilder);
		if (Family && Family->Extension)
		{
			Family->Extension->RenderAfterOpaque(*Parameters.GraphBuilder, *Family, Parameters);
		}
	}

	static bool MapPostProcessingPass(ISceneViewExtension::EPostProcessingPass Pass, EDreamPassInjection& OutInjection)
	{
		using EPass = ISceneViewExtension::EPostProcessingPass;
		switch (Pass)
		{
		case EPass::BeforeDOF:            OutInjection = EDreamPassInjection::PostProcessBeforeDOF; return true;
		case EPass::AfterDOF:             OutInjection = EDreamPassInjection::PostProcessAfterDOF; return true;
		case EPass::TranslucencyAfterDOF: OutInjection = EDreamPassInjection::PostProcessTranslucencyAfterDOF; return true;
		case EPass::ReplacingTonemapper:  OutInjection = EDreamPassInjection::PostProcessReplaceTonemapper; return true;
		case EPass::MotionBlur:           OutInjection = EDreamPassInjection::PostProcessAfterMotionBlur; return true;
		case EPass::Tonemap:              OutInjection = EDreamPassInjection::PostProcessAfterTonemap; return true;
		case EPass::FXAA:                 OutInjection = EDreamPassInjection::PostProcessAfterFXAA; return true;
		default:                          return false;
		}
	}

	/** The family's snapshot and the view's entry in it, from the family data the game thread attached. */
	static const FViewSnapshot* FindViewSnapshot(const FSceneView& View)
	{
		if (!View.Family)
		{
			return nullptr;
		}
		const FDreamPassFamilyData* Data = View.Family->GetExtentionData<FDreamPassFamilyData>();
		if (!Data || !Data->Snapshot)
		{
			return nullptr;
		}
		const int32 ViewIndex = View.Family->Views.IndexOfByKey(&View);
		return Data->Snapshot->FindView(ViewIndex);
	}
}

FDreamPassSceneViewExtension::FDreamPassSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, UDreamPassSubsystem* InSubsystem)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
	, Subsystem(InSubsystem)
{
	EnsurePostOpaqueHandler();
}

FDreamPassSceneViewExtension::~FDreamPassSceneViewExtension()
{
}

void FDreamPassSceneViewExtension::EnsurePostOpaqueHandler()
{
	using namespace UE::DreamPass::Private;
	check(IsInGameThread());

	if (PostOpaqueHandle.IsValid() || PostEngineInitHandle.IsValid())
	{
		return;
	}

	// The renderer's post-opaque delegate is one multicast for the whole process, broadcast on the render thread. It
	// is registered once, from the game thread, before the engine renders its first frame -- adding to it while a
	// frame is being rendered would race the broadcast. An extension created before then (a world loaded during
	// engine init) defers the registration to the end of init.
	auto Register = []()
	{
		if (!PostOpaqueHandle.IsValid())
		{
			PostOpaqueHandle = GetRendererModule().RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateStatic(&OnPostOpaqueRender));
		}
	};

	if (GIsRunning)
	{
		FlushRenderingCommands();
		Register();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda(Register);
	}
}

void FDreamPassSceneViewExtension::RemovePostOpaqueHandler()
{
	using namespace UE::DreamPass::Private;
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
	if (PostOpaqueHandle.IsValid())
	{
		// Module shutdown: the renderer module was loaded after this one and may already be gone; rendering has
		// stopped either way, so there is no broadcast to race.
		if (IRendererModule* Renderer = FModuleManager::GetModulePtr<IRendererModule>(TEXT("Renderer")))
		{
			Renderer->RemovePostOpaqueRenderDelegate(PostOpaqueHandle);
		}
		PostOpaqueHandle.Reset();
	}
}

bool FDreamPassSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if (!FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context) || !UE::DreamPass::IsEnabledByConsole())
	{
		return false;
	}
	const UDreamPassSubsystem* Owner = Subsystem.Get();
	return Owner && Owner->HasAnyActivation();
}

void FDreamPassSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	UDreamPassSubsystem* Owner = Subsystem.Get();
	if (!Owner)
	{
		return;
	}

	// The snapshot rides in the family's extension data, which the renderer copies into its own family along with
	// everything else (BeginRenderViewFamily runs before the renderer is constructed, R/Private/SceneRenderBuilder.cpp:509-516).
	if (UE::DreamPass::FFamilySnapshotPtr Snapshot = UE::DreamPass::BuildFamilySnapshot(*Owner, InViewFamily))
	{
		InViewFamily.GetOrCreateExtentionData<UE::DreamPass::FDreamPassFamilyData>()->Snapshot = MoveTemp(Snapshot);
	}
}

void FDreamPassSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	using namespace UE::DreamPass;

	const FDreamPassFamilyData* Data = InViewFamily.GetExtentionData<FDreamPassFamilyData>();
	if (!Data || !Data->Snapshot || GraphBuilder.Blackboard.GetMutable<FFamilyState>())
	{
		return;
	}

	FFamilyState& Family = GraphBuilder.Blackboard.Create<FFamilyState>();
	Family.Extension = this;
	Family.Family = &InViewFamily;
	Family.Snapshot = Data->Snapshot;
	Family.Views.SetNum(InViewFamily.Views.Num());

	// The renderer's family lists its own FViewInfo of every view, in the order of the game thread family the
	// snapshot was built from -- the same index finds the same view.
	for (int32 ViewIndex = 0; ViewIndex < InViewFamily.Views.Num(); ++ViewIndex)
	{
		FViewState& State = Family.Views[ViewIndex];
		State.View = InViewFamily.Views[ViewIndex];
		State.FamilyViewIndex = ViewIndex;
		State.Snapshot = Data->Snapshot->FindView(ViewIndex);
		if (!State.Snapshot)
		{
			continue;
		}

		State.Buffers.SetNum(State.Snapshot->Pipelines.Num());
		State.PreviousBuffers.SetNum(State.Snapshot->Pipelines.Num());
		for (int32 PipelineIndex = 0; PipelineIndex < State.Snapshot->Pipelines.Num(); ++PipelineIndex)
		{
			const int32 BufferCount = State.Snapshot->Pipelines[PipelineIndex].Buffers.Num();
			State.Buffers[PipelineIndex].Init(nullptr, BufferCount);
			State.PreviousBuffers[PipelineIndex].Init(nullptr, BufferCount);
		}
		State.Executed.Init(false, State.Snapshot->Order.Num());
	}
}

void FDreamPassSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	if (!Family)
	{
		return;
	}

	// AllViews includes the views of custom render passes, which belong to another family: not ours to run in.
	const int32 ViewIndex = Family->FindViewIndex(InView);
	if (ViewIndex == INDEX_NONE)
	{
		return;
	}

	FInjectionContext Context;
	Context.Injection = EDreamPassInjection::BeginView;
	Context.SceneViewRect = GetRenderViewRect(InView);
	RunInjection(GraphBuilder, *Family, ViewIndex, Context);
}

void FDreamPassSceneViewExtension::PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	if (!Family || !(Family->Snapshot->InjectionMask & (1u << uint32(EDreamPassInjection::BeforeBasePass))))
	{
		return;
	}

	// This callback has no view: it runs once per family, so the passes of every view run from here.
	for (int32 ViewIndex = 0; ViewIndex < Family->Views.Num(); ++ViewIndex)
	{
		const FViewState& State = Family->Views[ViewIndex];
		if (!State.IsActive() || !State.Snapshot->HasPassesAt(EDreamPassInjection::BeforeBasePass))
		{
			continue;
		}

		FInjectionContext Context;
		Context.Injection = EDreamPassInjection::BeforeBasePass;
		Context.SceneTextures = CreateSceneTextureUniformBuffer(GraphBuilder, *State.View, ESceneTextureSetupMode::SceneDepth);
		Context.SceneDepth = Context.SceneTextures->GetContents()->SceneDepthTexture;
		Context.SceneViewRect = GetRenderViewRect(*State.View);
		RunInjection(GraphBuilder, *Family, ViewIndex, Context);
	}
}

void FDreamPassSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	const int32 ViewIndex = Family ? Family->FindViewIndex(InView) : INDEX_NONE;
	if (ViewIndex == INDEX_NONE)
	{
		return;
	}

	FInjectionContext Context;
	Context.Injection = EDreamPassInjection::AfterBasePass;
	// The uniform buffer the base pass hands over has placeholders where the GBuffer is (it is rebuilt after this
	// callback, R/Private/DeferredShadingRenderer.cpp:3122); a fresh one binds every texture that has been produced.
	Context.SceneTextures = CreateSceneTextureUniformBuffer(GraphBuilder, InView, ESceneTextureSetupMode::All);
	Context.SceneViewRect = GetRenderViewRect(InView);
	Context.SceneColor = FScreenPassTexture(RenderTargets[0].GetTexture(), Context.SceneViewRect);
	Context.bSceneColorWritable = Context.SceneColor.IsValid();
	Context.SceneDepth = RenderTargets.DepthStencil.GetTexture();
	Context.CustomDepth = Context.SceneTextures->GetContents()->CustomDepthTexture;
	RunInjection(GraphBuilder, *Family, ViewIndex, Context);
}

void FDreamPassSceneViewExtension::RenderAfterOpaque(FRDGBuilder& GraphBuilder, UE::DreamPass::FFamilyState& Family, const FPostOpaqueRenderParameters& Parameters)
{
	using namespace UE::DreamPass;

	if (!(Family.Snapshot->InjectionMask & (1u << uint32(EDreamPassInjection::AfterOpaque))))
	{
		return;
	}

	// The delegate hands over the renderer's private FViewInfo; the view is found by its view uniform buffer instead,
	// which every view of the family has by now and which tells them apart.
	int32 ViewIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Family.Views.Num(); ++Index)
	{
		const FSceneView* View = Family.Views[Index].View;
		if (View && View->ViewUniformBuffer.GetReference() == Parameters.ViewUniformBuffer)
		{
			ViewIndex = Index;
			break;
		}
	}
	if (ViewIndex == INDEX_NONE)
	{
		return;
	}

	FInjectionContext Context;
	Context.Injection = EDreamPassInjection::AfterOpaque;
	Context.SceneTextures = Parameters.SceneTexturesUniformParams;
	Context.SceneViewRect = Parameters.ViewportRect;
	Context.SceneColor = FScreenPassTexture(Parameters.ColorTexture, Parameters.ViewportRect);
	Context.bSceneColorWritable = Context.SceneColor.IsValid();
	Context.SceneDepth = Parameters.DepthTexture;
	Context.CustomDepth = Context.SceneTextures ? Context.SceneTextures->GetContents()->CustomDepthTexture : nullptr;
	RunInjection(GraphBuilder, Family, ViewIndex, Context);
}

void FDreamPassSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	EDreamPassInjection Injection;
	if (!UE::DreamPass::Private::MapPostProcessingPass(Pass, Injection))
	{
		return;
	}

	// Subscribing costs the chain a delegate call (and, for the later passes, possibly a copy into the override
	// output), so only a view that has passes at the point subscribes.
	const UE::DreamPass::FViewSnapshot* Snapshot = UE::DreamPass::Private::FindViewSnapshot(InView);
	if (Snapshot && Snapshot->HasPassesAt(Injection))
	{
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateSP(this, &FDreamPassSceneViewExtension::PostProcessCallback, Injection));
	}
}

FScreenPassTexture FDreamPassSceneViewExtension::PostProcessCallback(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, EDreamPassInjection Injection)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	const int32 ViewIndex = Family ? Family->FindViewIndex(View) : INDEX_NONE;
	if (ViewIndex == INDEX_NONE)
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	// What the chain carries at this point: the separate translucency at TranslucencyAfterDOF, the scene colour
	// everywhere else (R/Private/PostProcess/PostProcessing.cpp:383-410, 1024-1038).
	const EPostProcessMaterialInput ChainInput = Injection == EDreamPassInjection::PostProcessTranslucencyAfterDOF
		? EPostProcessMaterialInput::SeparateTranslucency
		: EPostProcessMaterialInput::SceneColor;

	FInjectionContext Context;
	Context.Injection = Injection;
	Context.PostProcessInputs = &Inputs;
	Context.bPostProcessChain = true;
	Context.bSceneColorWritable = true;
	Context.SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(ChainInput));
	Context.SceneViewRect = Context.SceneColor.ViewRect;
	Context.SceneTextures = Inputs.SceneTextures.SceneTextures;
	Context.SceneDepth = Context.SceneTextures ? Context.SceneTextures->GetContents()->SceneDepthTexture : nullptr;
	Context.CustomDepth = Inputs.CustomDepthTexture;

	const FScreenPassTexture Untouched = Context.SceneColor;
	RunInjection(GraphBuilder, *Family, ViewIndex, Context);

	if (Context.SceneColor.Texture == Untouched.Texture)
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	// The last pass of the chain receives the view family texture as its override output and must write it
	// (E/Public/SceneViewExtension.h:218-221).
	if (Inputs.OverrideOutput.IsValid())
	{
		AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(View), Context.SceneColor, Inputs.OverrideOutput);
		return Inputs.OverrideOutput;
	}
	return Context.SceneColor;
}

void FDreamPassSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	const int32 ViewIndex = Family ? Family->FindViewIndex(InView) : INDEX_NONE;
	if (ViewIndex == INDEX_NONE)
	{
		return;
	}

	FViewState& State = Family->Views[ViewIndex];
	if (State.IsActive() && State.Snapshot->HasPassesAt(EDreamPassInjection::EndOfView) && InView.Family)
	{
		FInjectionContext Context;
		Context.Injection = EDreamPassInjection::EndOfView;
		Context.SceneViewRect = GetOutputViewRect(InView);
		if (FRDGTextureRef FamilyTexture = TryCreateViewFamilyTexture(GraphBuilder, *InView.Family))
		{
			Context.SceneColor = FScreenPassTexture(FamilyTexture, Context.SceneViewRect);
			Context.bSceneColorWritable = true;
		}
		Context.SceneTextures = CreateSceneTextureUniformBuffer(GraphBuilder, InView, ESceneTextureSetupMode::All);
		Context.SceneDepth = Context.SceneTextures->GetContents()->SceneDepthTexture;
		RunInjection(GraphBuilder, *Family, ViewIndex, Context);
	}

	if (State.IsActive())
	{
		FinishViewBuffers(GraphBuilder, *Family, State);
	}
	State.bFinished = true;

	// PostRenderViewFamily_RenderThread comes before the views' PostRenderView (R/Private/SceneRendering.cpp:4949-4963),
	// so the family is finished with its last view.
	if (Family->Views.FindByPredicate([](const FViewState& Other) { return !Other.bFinished; }) == nullptr)
	{
		FinishFamily(GraphBuilder, *Family);
	}
}

void FDreamPassSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FDreamPassSceneViewExtension::FinishFamily(FRDGBuilder& GraphBuilder, UE::DreamPass::FFamilyState& Family)
{
	using namespace UE::DreamPass;

	FFrameReport Report;
	Report.Frame = Family.Snapshot->FrameNumber;
	for (const FViewState& State : Family.Views)
	{
		if (!State.IsActive())
		{
			continue;
		}
		++Report.ViewsWithPasses;
		for (int32 OrderIndex = 0; OrderIndex < State.Snapshot->Order.Num(); ++OrderIndex)
		{
			if (State.Executed[OrderIndex])
			{
				++Report.PassesRun;
				continue;
			}
			// A pass that never ran: its injection point did not happen in this view -- post processing off, a scene
			// capture that does not resolve, path tracing -- or the pass could not get what it needed.
			++Report.PassesSkipped;
			const FScheduledPass& Scheduled = State.Snapshot->Order[OrderIndex];
			const FSnapshotPipeline& Pipeline = State.Snapshot->Pipelines[Scheduled.Pipeline];
			const FSnapshotPass& Pass = Pipeline.Passes[Scheduled.Pass];
			Report.SkippedPasses.Add(FString::Printf(TEXT("%s.%s (view %d, %s)"), *Pipeline.DebugName, *Pass.Name.ToString(), State.FamilyViewIndex, LexToString(Pass.Injection)));
		}
	}

	for (auto It = History.CreateIterator(); It; ++It)
	{
		if (Report.Frame > It.Value()->LastUsedFrame + UE::DreamPass::Private::HistoryIdleFrames)
		{
			It.RemoveCurrent();
		}
	}

	FScopeLock Lock(&ReportLock);
	LastReport = MoveTemp(Report);
}

FDreamPassSceneViewExtension::FHistoryEntry* FDreamPassSceneViewExtension::FindHistory(const FHistoryKey& Key)
{
	TUniquePtr<FHistoryEntry>* Found = History.Find(Key);
	return Found ? Found->Get() : nullptr;
}

FDreamPassSceneViewExtension::FHistoryEntry& FDreamPassSceneViewExtension::FindOrAddHistory(const FHistoryKey& Key)
{
	TUniquePtr<FHistoryEntry>& Entry = History.FindOrAdd(Key);
	if (!Entry)
	{
		Entry = MakeUnique<FHistoryEntry>();
	}
	return *Entry;
}

FDreamPassSceneViewExtension::FFrameReport FDreamPassSceneViewExtension::GetLastReport() const
{
	FScopeLock Lock(&ReportLock);
	return LastReport;
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
