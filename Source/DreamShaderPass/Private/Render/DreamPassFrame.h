#pragma once

#include "CoreMinimal.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassSnapshot.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "SceneTexturesConfig.h"
#include "ScreenPass.h"

class FDreamPassSceneViewExtension;
class FMaterialRenderProxy;
struct FPostProcessMaterialInputs;

/**
 * The render thread side of a Custom Pass frame.
 *
 * One FFamilyState per view family, in that family's render graph blackboard: created when the family begins
 * (PreRenderViewFamily_RenderThread), gone when its graph executes. Every injection point callback of the family
 * receives the same FRDGBuilder (R/Private/SceneRenderBuilder.cpp:873-916), so an FRDGTextureRef a pass creates at
 * one point is still valid at every later point of the same family -- and never in another family's graph, which
 * is why nothing here outlives the blackboard except the history textures, which are extracted.
 */
namespace UE::DreamPass
{
	/** What an injection point offers its passes. */
	struct FInjectionContext
	{
		EDreamPassInjection Injection = EDreamPassInjection::BeginView;

		/** The scene textures uniform buffer, where the point has scene textures. */
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = nullptr;

		/** The colour the point's passes read as SceneColor: the scene colour, the post-process chain's input, the view family texture. */
		FScreenPassTexture SceneColor;

		/** Whether a pass here may write SceneColor at all. */
		bool bSceneColorWritable = false;

		/**
		 * A post-process subscription: a pass that writes SceneColor hands the chain a new texture (SceneColor moves on
		 * to it) instead of copying its result back into the one it read.
		 */
		bool bPostProcessChain = false;

		FRDGTextureRef SceneDepth = nullptr;
		FRDGTextureRef CustomDepth = nullptr;

		/** Nanite wrote custom stencil into a texture of its own (R/Private/Nanite/NaniteComposition.cpp:709-736). */
		bool bSeparateCustomStencil = false;

		/** The post-process chain's inputs, at a PostProcess.* point. */
		const FPostProcessMaterialInputs* PostProcessInputs = nullptr;

		/** The rect of this injection point's colour/output: render resolution before the upscaler, output after. */
		FIntRect SceneViewRect;

		/** Depth, GBuffer and velocity stay at render resolution even when the colour chain has been upscaled. */
		FIntRect SceneTextureViewRect;
	};

	struct FViewState
	{
		const FSceneView* View = nullptr;
		const FViewSnapshot* Snapshot = nullptr;
		int32 FamilyViewIndex = INDEX_NONE;

		/** [pipeline][buffer]: this frame's texture of every own buffer, created on first use. */
		TArray<TArray<FRDGTextureRef>> Buffers;

		/** [pipeline][buffer]: last frame's texture of every History buffer that has one. */
		TArray<TArray<FRDGTextureRef>> PreviousBuffers;

		/** One bit per FViewSnapshot::Order entry: the pass ran. */
		TBitArray<> Executed;

		/**
		 * One per FViewSnapshot::Order entry: why a pass that was tried did not run, for `DreamPass.Dump`. Empty for a pass
		 * that ran or was never tried -- its injection point did not happen in this view.
		 */
		TArray<FString> SkipReasons;

		bool bFinished = false;

		bool IsActive() const { return View && Snapshot; }
	};

	struct FFamilyState
	{
		FDreamPassSceneViewExtension* Extension = nullptr;
		const FSceneViewFamily* Family = nullptr;

		/** Keeps the snapshot alive however the family's own copy is released. */
		FFamilySnapshotPtr Snapshot;

		/** One per family view, by index. */
		TArray<FViewState> Views;

		/** Mesh passes: the scene's primitives that render custom depth, collected once per family (Render/DreamPassMesh.cpp). */
		bool bCustomDepthPrimitivesCollected = false;
		TArray<uint32> CustomDepthPrimitiveIndices;

		int32 FindViewIndex(const FSceneView& View) const
		{
			return Views.IndexOfByPredicate([&View](const FViewState& State) { return State.View == &View; });
		}
	};

	/** Everything an executor needs for one pass in one view. */
	struct FExecuteContext
	{
		FRDGBuilder& GraphBuilder;
		FFamilyState& Family;
		FViewState& ViewState;
		FInjectionContext& Injection;
		const FSnapshotPipeline& Pipeline;
		int32 PipelineIndex;
		const FSnapshotPass& Pass;
		int32 OrderIndex;

		/** Set by an executor that returns false, where a short reason helps `DreamPass.Dump`: "its material is still compiling". */
		FString SkipReason;

		const FSceneView& GetView() const { return *ViewState.View; }
	};

	// --- buffers (Render/DreamPassBuffers.cpp) ------------------------------------------------------------------

	/** The view rect the renderer draws at, before the upscaler. */
	FIntRect GetRenderViewRect(const FSceneView& View);

	/** The view rect after the upscaler. */
	FIntRect GetOutputViewRect(const FSceneView& View);

	/** The extent an own buffer is created with in a view. */
	FIntPoint GetBufferExtent(const FSceneView& View, const FDreamPassBufferDesc& Desc);

	/** This frame's texture of own buffer BufferIndex of the context's pipeline; created and cleared on first use. */
	FRDGTextureRef GetOrCreateBuffer(FExecuteContext& Context, int32 BufferIndex);

	/**
	 * The texture a pass reads for a binding, with the rect that holds its picture: an own buffer (whole extent),
	 * last frame's copy of one, or a built-in (the view rect of the scene textures). Invalid when the binding names
	 * something the injection point does not have -- the reason is logged once.
	 */
	FScreenPassTexture ResolveRead(FExecuteContext& Context, const FDreamPassBufferBinding& Binding);

	/**
	 * The target a pass writes for a binding. An own buffer, loaded (it was cleared when created). For SceneColor, a
	 * scratch texture with the scene colour's extent and format; the pass hands its result to CommitSceneColor.
	 */
	FScreenPassRenderTarget ResolveWrite(FExecuteContext& Context, const FDreamPassBufferBinding& Binding);

	/** Whether a write binding targets what the point carries: the scene colour, or the translucency at TranslucencyAfterDOF. */
	bool IsSceneColorWrite(const FExecuteContext& Context, const FDreamPassBufferBinding& Binding);

	/** A pass's result for SceneColor: copied back into the scene colour, or handed on along the post-process chain. */
	void CommitSceneColor(FExecuteContext& Context, const FScreenPassTexture& Result);

	/** The view is done: queues the extraction of its History buffers for the next frame. */
	void FinishViewBuffers(FRDGBuilder& GraphBuilder, FFamilyState& Family, FViewState& ViewState);

	/** After a pass ran: the copy of every exported buffer it was the last writer of (Render/DreamPassExport.cpp). */
	void AfterPassWrites(FExecuteContext& Context);

	/** At view end: exports successful earlier writes whose planned last writer did not run. */
	void FinishViewExports(FRDGBuilder& GraphBuilder, FViewState& ViewState);

	// --- executors (one file each) ------------------------------------------------------------------------------

	// Each returns whether the pass ran; one that could not (a material still compiling, a target the injection point
	// lacks) logs why once and leaves its targets as they were.

	bool ExecuteFullscreenMaterialPass(FExecuteContext& Context);
	bool ExecuteFullscreenSlotPass(FExecuteContext& Context);
	bool ExecuteComputePass(FExecuteContext& Context);
	bool ExecuteMeshPass(FExecuteContext& Context);
	bool ExecuteClearPass(FExecuteContext& Context);
	bool ExecuteCopyPass(FExecuteContext& Context);

	/**
	 * Whether a material a pass draws with has every shader of its map at FeatureLevel, asked the way the engine asks
	 * (Render/DreamPassFullscreen.cpp). False while it compiles -- and the asking starts the compile in the editor.
	 */
	bool IsMaterialReadyToDraw(const FMaterialRenderProxy& Proxy, ERHIFeatureLevel::Type FeatureLevel);

	// --- debugging (Render/DreamPassVisualize.cpp) --------------------------------------------------------------

	/** r.DreamPass.Visualize: draws the named buffer of the view's pipeline into the corner of the view family texture. */
	void AddVisualizePass(FRDGBuilder& GraphBuilder, FFamilyState& Family, FViewState& ViewState, const FSceneView& View);

	// --- scheduling (Render/DreamPassScheduler.cpp) -------------------------------------------------------------

	/** The family state of the graph being built, or null when no pipeline applies to its family. */
	FFamilyState* FindFamilyState(FRDGBuilder& GraphBuilder);

	/** Runs every pass the view has at Context.Injection, in order. */
	void RunInjection(FRDGBuilder& GraphBuilder, FFamilyState& Family, int32 ViewIndex, FInjectionContext& Context);

	/** Logs a message once per (key) for the session: problems that would otherwise repeat every frame. */
	void WarnOnce(const FString& Key, const FString& Message);
}

RDG_REGISTER_BLACKBOARD_STRUCT(UE::DreamPass::FFamilyState);

#endif // DREAMSHADER_WITH_CUSTOM_PASS
