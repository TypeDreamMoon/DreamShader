#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassConsole.h"

#include "RHIStaticStates.h"
#include "SceneView.h"
#include "ScreenPass.h"

/**
 * r.DreamPass.Visualize <Pipeline>.<Buffer>: this frame's picture of one buffer, drawn into the lower left corner of every
 * view that runs the pipeline.
 *
 * It runs last in the view -- PostRenderView_RenderThread, after the EndOfView passes and before the UI -- so it shows the
 * buffer as the frame's last writer left it, and nothing the frame renders afterwards reads the corner back. The pipeline
 * is named as r.DreamPass.DisablePipelines and DreamPass.Dump name it, by the asset's short name; only the pipeline's own
 * buffers can be shown, not the built-in ones.
 */
namespace UE::DreamPass
{
	namespace Private::Visualize
	{
		/** The draw samples the buffer as a float texture, which a depth or an integer buffer cannot be read as. */
		static bool CanSample(EDreamPassBufferFormat Format)
		{
			return Format != EDreamPassBufferFormat::Depth32 && Format != EDreamPassBufferFormat::R32U && Format != EDreamPassBufferFormat::RG32U;
		}

		/**
		 * Where a picture of Extent goes in Rect: the lower left corner, a quarter of the rect's width, the picture's aspect.
		 * Never taller than half the rect -- a far taller than wide buffer narrows instead -- so the corner never starts at the
		 * texture's origin: there, given a buffer as large as the target, the draw would take itself for a write of the whole
		 * target and not load the rest of it (R/Private/ScreenPass.cpp:348-351).
		 */
		static FIntRect GetCornerRect(const FIntRect& Rect, FIntPoint Extent)
		{
			const FIntPoint RectSize = Rect.Size();
			if (RectSize.X < 4 || RectSize.Y < 2 || Extent.X <= 0 || Extent.Y <= 0)
			{
				return FIntRect();
			}

			const int32 MaxWidth = RectSize.X / 4;
			const int32 MaxHeight = RectSize.Y / 2;

			FIntPoint Size(MaxWidth, FMath::Max(FMath::RoundToInt(float(MaxWidth) * float(Extent.Y) / float(Extent.X)), 1));
			if (Size.Y > MaxHeight)
			{
				Size.Y = MaxHeight;
				Size.X = FMath::Clamp(FMath::RoundToInt(float(MaxHeight) * float(Extent.X) / float(Extent.Y)), 1, MaxWidth);
			}

			const FIntPoint Min(Rect.Min.X, Rect.Max.Y - Size.Y);
			return FIntRect(Min, Min + Size);
		}
	}

	void AddVisualizePass(FRDGBuilder& GraphBuilder, FFamilyState& Family, FViewState& ViewState, const FSceneView& View)
	{
		// Every view that runs a pipeline comes through here every frame: while the variable is empty, which it nearly always
		// is, one read of it is all this costs.
		const FString Target = GetVisualizeTarget_RenderThread();
		if (Target.IsEmpty() || !ViewState.Snapshot || !View.Family || !View.Family->RenderTarget)
		{
			return;
		}

		FString PipelineName;
		FString BufferName;
		if (Target.Split(TEXT("."), &PipelineName, &BufferName))
		{
			PipelineName.TrimStartAndEndInline();
			BufferName.TrimStartAndEndInline();
		}
		if (PipelineName.IsEmpty() || BufferName.IsEmpty())
		{
			WarnOnce(TEXT("Visualize|Malformed|") + Target,
				FString::Printf(TEXT("DreamPass: r.DreamPass.Visualize '%s' is not <Pipeline>.<Buffer>; nothing is drawn."), *Target));
			return;
		}

		// A view that does not run the pipeline draws nothing, and says nothing: another view of the frame may well run it.
		const TArray<FSnapshotPipeline>& Pipelines = ViewState.Snapshot->Pipelines;
		const int32 PipelineIndex = Pipelines.IndexOfByPredicate([&PipelineName](const FSnapshotPipeline& Candidate)
		{
			return Candidate.DebugName.Equals(PipelineName, ESearchCase::IgnoreCase);
		});
		if (PipelineIndex == INDEX_NONE)
		{
			return;
		}
		const FSnapshotPipeline& Pipeline = Pipelines[PipelineIndex];

		// FNAME_Find keeps a mistyped name out of the name table; a name that is not in it names no buffer. FName comparison
		// ignores case, as the pipeline lookup does.
		const FName BufferFName(*BufferName, FNAME_Find);
		const int32 BufferIndex = BufferFName.IsNone() ? INDEX_NONE : Pipeline.FindBuffer(BufferFName);
		if (BufferIndex == INDEX_NONE)
		{
			WarnOnce(TEXT("Visualize|Unknown|") + Pipeline.DebugName + TEXT(".") + BufferName,
				FString::Printf(TEXT("DreamPass: r.DreamPass.Visualize: %s has no buffer '%s' (only a pipeline's own buffers can be shown); nothing is drawn."),
					*Pipeline.DebugName, *BufferName));
			return;
		}

		const FDreamPassBufferDesc& Desc = Pipeline.Buffers[BufferIndex].Desc;
		if (!Private::Visualize::CanSample(Desc.Format))
		{
			WarnOnce(TEXT("Visualize|Format|") + Pipeline.DebugName + TEXT(".") + Desc.Name.ToString(),
				FString::Printf(TEXT("DreamPass: r.DreamPass.Visualize: %s.%s is a %s buffer, which cannot be drawn -- only float and unorm colour buffers can; nothing is drawn."),
					*Pipeline.DebugName, *Desc.Name.ToString(), LexToString(Desc.Format)));
			return;
		}

		// A buffer is created on its first use in the view (GetOrCreateBuffer): one that no pass of this view wrote or read
		// this frame has no picture to show.
		const bool bHasTexture = ViewState.Buffers.IsValidIndex(PipelineIndex) && ViewState.Buffers[PipelineIndex].IsValidIndex(BufferIndex);
		FRDGTextureRef Texture = bHasTexture ? ViewState.Buffers[PipelineIndex][BufferIndex] : nullptr;
		if (!Texture)
		{
			return;
		}

		// The same RDG texture the EndOfView passes wrote, if they did: an external texture is registered once per graph
		// (RC/Public/RenderGraphUtils.h:271-279).
		FRDGTextureRef FamilyTexture = TryCreateViewFamilyTexture(GraphBuilder, *View.Family);
		if (!FamilyTexture)
		{
			return;
		}

		FIntRect OutputRect = GetOutputViewRect(View);
		OutputRect.Clip(FIntRect(FIntPoint::ZeroValue, FamilyTexture->Desc.Extent));
		const FIntRect Corner = Private::Visualize::GetCornerRect(OutputRect, Texture->Desc.Extent);
		if (Corner.Width() <= 0 || Corner.Height() <= 0)
		{
			return;
		}

		RDG_EVENT_SCOPE(GraphBuilder, "DreamPass.Visualize %s.%s", *Pipeline.DebugName, *BufferName);

		// The overload with an input and an output rect of their own: a hardware copy when the formats and sizes agree, a
		// bilinear draw that loads the rest of the target otherwise (R/Private/ScreenPass.cpp:322-364). The shader reads mip
		// 0, whatever mips the buffer has (Shaders/Private/ScreenPass.usf:25-36).
		AddDrawTexturePass(
			GraphBuilder,
			FScreenPassViewInfo(View),
			Texture,
			FamilyTexture,
			FIntPoint::ZeroValue,
			Texture->Desc.Extent,
			Corner.Min,
			Corner.Size(),
			TStaticSamplerState<SF_Bilinear>::GetRHI());
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
