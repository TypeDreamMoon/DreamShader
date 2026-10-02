#pragma once

#include "CoreMinimal.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassFrame.h"
#include "RendererInterface.h"
#include "SceneViewExtension.h"

class UDreamPassSubsystem;

/**
 * Runs the Custom Pass pipelines of one world inside the renderer.
 *
 * Game thread: BeginRenderViewFamily asks the world's UDreamPassSubsystem what applies to each view of the family
 * and attaches the answer, an immutable snapshot, to the family.
 *
 * Render thread: every injection point callback finds the family's state in its graph's blackboard and runs the
 * passes the view has there. The renderer's post-opaque delegate is global rather than per extension, so the module
 * registers one handler for it (OnPostOpaqueRender) that finds the extension through the same blackboard.
 *
 * Owned by the subsystem through FSceneViewExtensions::NewExtension; a family being rendered holds a strong
 * reference of its own, so the extension outlives every frame it was gathered for.
 */
class FDreamPassSceneViewExtension final : public FWorldSceneViewExtension
{
public:
	FDreamPassSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, UDreamPassSubsystem* InSubsystem);
	virtual ~FDreamPassSceneViewExtension() override;

	// Game thread.
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	// Render thread.
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) override;
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	/** The AfterOpaque point, reached through the renderer's post-opaque delegate. */
	void RenderAfterOpaque(FRDGBuilder& GraphBuilder, UE::DreamPass::FFamilyState& Family, const FPostOpaqueRenderParameters& Parameters);

	/** Registers the module-wide post-opaque handler the first time an extension exists. Game thread. */
	static void EnsurePostOpaqueHandler();

	/** Removes it again; module shutdown. */
	static void RemovePostOpaqueHandler();

	/** Last frame's history texture of one buffer in one view, by a key the buffers code makes. Render thread. */
	struct FHistoryEntry
	{
		TRefCountPtr<IPooledRenderTarget> Texture;
		uint64 LastUsedFrame = 0;
	};

	struct FHistoryKey
	{
		uint32 ViewKey = 0;
		const void* Pipeline = nullptr;
		FName Buffer;

		bool operator==(const FHistoryKey& Other) const
		{
			return ViewKey == Other.ViewKey && Pipeline == Other.Pipeline && Buffer == Other.Buffer;
		}

		friend uint32 GetTypeHash(const FHistoryKey& Key)
		{
			return HashCombineFast(HashCombineFast(GetTypeHash(Key.ViewKey), GetTypeHash(Key.Pipeline)), GetTypeHash(Key.Buffer));
		}
	};

	/** Null when the buffer has no history in that view yet. Render thread. */
	FHistoryEntry* FindHistory(const FHistoryKey& Key);

	/** The entry an extraction fills; its address is stable until the entry is dropped. Render thread. */
	FHistoryEntry& FindOrAddHistory(const FHistoryKey& Key);

	/** Per-frame counters for `DreamPass.Dump` and `stat DreamPass`, written on the render thread. */
	struct FFrameReport
	{
		uint64 Frame = 0;
		int32 ViewsWithPasses = 0;
		int32 PassesRun = 0;
		int32 PassesSkipped = 0;
		TArray<FString> SkippedPasses;
	};

	/** A copy of the last finished family's report. Any thread. */
	FFrameReport GetLastReport() const;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	FScreenPassTexture PostProcessCallback(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, EDreamPassInjection Injection);

	void FinishFamily(FRDGBuilder& GraphBuilder, UE::DreamPass::FFamilyState& Family);

	TWeakObjectPtr<UDreamPassSubsystem> Subsystem;

	/** Render thread only. Entries live behind a pointer so an extraction can hold their address. */
	TMap<FHistoryKey, TUniquePtr<FHistoryEntry>> History;

	mutable FCriticalSection ReportLock;
	FFrameReport LastReport;
};

#endif // DREAMSHADER_WITH_CUSTOM_PASS
