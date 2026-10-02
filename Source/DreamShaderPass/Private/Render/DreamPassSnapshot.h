#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "DreamPassTypes.h"
#include "SceneView.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

class FMaterialRenderProxy;
class FSceneViewFamily;
class FTextureRenderTargetResource;
class UDreamPassPipeline;
class UDreamPassSubsystem;
class UMaterialInterface;

/**
 * What the render thread knows of one frame of Custom Passes: an immutable copy of everything the game thread
 * decided in BeginRenderViewFamily, made per view family and attached to it.
 *
 * Plain values, render resource pointers whose owners the game thread keeps alive past the frame (render
 * proxies and render target resources are released through the render command queue, which this frame's
 * commands are ahead of), and -- for the fullscreen material pass only -- a UMaterialInterface pointer, because
 * the engine's exported AddPostProcessMaterialPass takes one and resolves its proxy on the render thread itself.
 */
namespace UE::DreamPass
{
	struct FSnapshotBuffer
	{
		FDreamPassBufferDesc Desc;

		/** Some compute pass writes it: created with a UAV. */
		bool bNeedsUAV = false;

		/** Some raster pass writes it: created render-targetable (always, unless it is Depth32). */
		bool bNeedsRenderTarget = true;

		/** The exported render target's resource, in the view that exports; null in every other view. */
		FTextureRenderTargetResource* ExportResource = nullptr;

		/** Index into FViewSnapshot::Order of the last pass that writes it, INDEX_NONE when none does. */
		int32 LastWriterOrder = INDEX_NONE;
	};

	struct FSnapshotPass
	{
		FName Name;
		int32 PassIndex = INDEX_NONE;
		EDreamPassKind Kind = EDreamPassKind::Fullscreen;
		EDreamPassInjection Injection = EDreamPassInjection::BeforePostProcess;

		TArray<FDreamPassBufferBinding> Reads;
		TArray<FDreamPassBufferBinding> Writes;

		/** Fullscreen material: the instance of this use, with the pass's parameters set on it. */
		const UMaterialInterface* Material = nullptr;

		/** Fullscreen through FDreamPassPS, or compute through FDreamPassCS. */
		int32 Slot = INDEX_NONE;
		FIntVector ThreadGroupSize = FIntVector(8, 8, 1);
		EDreamPassDispatchMode DispatchMode = EDreamPassDispatchMode::Buffer;
		FName DispatchBuffer;
		float DispatchScale = 1.0f;
		FIntVector DispatchSize = FIntVector(1, 1, 1);

		/** The HLSL slot's DP_Params block, already packed (LayoutSlotParameters). */
		TArray<FVector4f> SlotParams;

		/** Mesh. The override material's render proxy, with the pass's parameters on its instance. */
		FDreamPassMeshSettings Mesh;
		const FMaterialRenderProxy* OverrideMaterialProxy = nullptr;

		/** Clear. */
		FLinearColor ClearValue = FLinearColor::Transparent;

		/** The pipeline's weight in the view. */
		float Weight = 1.0f;
	};

	struct FSnapshotPipeline
	{
		/** The asset's name, for event names and the log. */
		FString DebugName;

		/** The asset itself, as an opaque key: history buffers are kept per (view, pipeline, buffer). */
		const void* Identity = nullptr;

		int32 Order = 0;
		float Weight = 1.0f;

		TArray<FSnapshotBuffer> Buffers;
		TArray<FSnapshotPass> Passes;

		int32 FindBuffer(FName Name) const
		{
			return Buffers.IndexOfByPredicate([Name](const FSnapshotBuffer& Buffer) { return Buffer.Desc.Name == Name; });
		}
	};

	/** One pass in the order a view runs them. */
	struct FScheduledPass
	{
		int32 Pipeline = 0;
		int32 Pass = 0;
	};

	struct FViewSnapshot
	{
		/** Whether any pipeline applies to the view at all. */
		bool bActive = false;

		/** The view that writes exported buffers into their render targets this frame. */
		bool bExportView = false;

		uint32 ViewKey = 0;

		TArray<FSnapshotPipeline> Pipelines;

		/** Every pass of every pipeline, in frame order: injection point, then pipeline Order, then declaration. */
		TArray<FScheduledPass> Order;

		/** For each injection point, the range of Order that runs there. */
		TStaticArray<TPair<int32, int32>, int32(EDreamPassInjection::Count)> RangeByInjection;

		/** Bit i: something runs at EDreamPassInjection(i). */
		uint32 InjectionMask = 0;

		bool HasPassesAt(EDreamPassInjection Injection) const { return (InjectionMask & (1u << uint32(Injection))) != 0; }
	};

	struct FFamilySnapshot
	{
		/** One per FSceneViewFamily::Views, by index; an inactive entry for a view nothing applies to. */
		TArray<FViewSnapshot> Views;

		/** The pass layer mask of every primitive that has one, by FPrimitiveComponentId value. */
		TMap<uint32, uint32> LayersByPrimitiveId;

		/** The members of every named list, by FPrimitiveComponentId value. */
		TMap<FName, TArray<uint32>> ListMembers;

		/** The union of the views' masks. */
		uint32 InjectionMask = 0;

		/** r.CustomDepth == 3 when the frame was set up, so CustomStencil holds values. */
		bool bCustomStencilWritten = false;

		/** r.DreamPass.Visualize when the frame was set up: `<Pipeline>.<Buffer>`, empty for none. */
		FString VisualizeTarget;

		uint64 FrameNumber = 0;

		const FViewSnapshot* FindView(int32 FamilyViewIndex) const
		{
			return Views.IsValidIndex(FamilyViewIndex) && Views[FamilyViewIndex].bActive ? &Views[FamilyViewIndex] : nullptr;
		}
	};

	using FFamilySnapshotRef = TSharedRef<const FFamilySnapshot, ESPMode::ThreadSafe>;
	using FFamilySnapshotPtr = TSharedPtr<const FFamilySnapshot, ESPMode::ThreadSafe>;

	/** The family's data slot the snapshot travels in, from BeginRenderViewFamily to the renderer's own copy of the family. */
	class FDreamPassFamilyData final : public ISceneViewFamilyExtentionData
	{
	public:
		inline static const TCHAR* const GSubclassIdentifier = TEXT("FDreamPassFamilyData");
		virtual const TCHAR* GetSubclassIdentifier() const override { return GSubclassIdentifier; }

		FFamilySnapshotPtr Snapshot;
	};

	/** Which kind of view a view is, for a pipeline's Views; None for one that never runs passes (hit proxies, VT). */
	EDreamPassViewFlags ClassifyView(const FSceneViewFamily& Family, const FSceneView& View);

	/** What the view offers, for a pipeline's Requires. */
	EDreamPassRequirementFlags GetViewCapabilities(const FSceneViewFamily& Family, const FSceneView& View);

	/** The export view's half of an exported buffer: sizes its render target and takes its resource. Game thread. */
	void PrepareExportTargets(UDreamPassSubsystem& Subsystem, FSnapshotPipeline& Pipeline, const UDreamPassPipeline& Asset, const FSceneView& View);

	/**
	 * Builds the snapshot of one family: resolves the pipelines of every view through the subsystem, sets the
	 * parameters of each material pass on a material instance of its own, packs the HLSL slot parameters, resizes
	 * exported render targets for the view that exports, and orders every view's passes. Game thread. Null when
	 * nothing applies to any view of the family.
	 */
	FFamilySnapshotPtr BuildFamilySnapshot(UDreamPassSubsystem& Subsystem, FSceneViewFamily& Family);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
