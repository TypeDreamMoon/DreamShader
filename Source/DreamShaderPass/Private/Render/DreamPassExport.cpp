#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassPipeline.h"
#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"

#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "TextureResource.h"

/**
 * Exported buffers: a buffer with `Export = true` is copied, after the last pass that writes it, into the render target
 * asset the compiler made for it, which ordinary materials, Blueprints, UMG and Niagara read like any texture.
 *
 * One render target, one picture: only the export view writes it -- the first view each frame of a world that is a
 * first local player's game view or an editor viewport -- so split screen, a second editor viewport or a scene capture
 * never overwrite it. Materials read whatever it held when they rendered: this frame's picture when they render
 * after the writer, last frame's when before (a pass at AfterOpaque and a material in the base pass).
 */
namespace UE::DreamPass
{
	namespace Private::Export
	{
		static void CopyBuffer(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef Source,
			FTextureRenderTargetResource& Resource)
		{
			// Fetch the resource every frame: a resize on the game thread replaces its RHI texture.
			FRHITexture* TargetRHI = Resource.GetRenderTargetTexture();
			if (!Source || !TargetRHI)
			{
				return;
			}
			FRDGTextureRef Target = RegisterExternalTexture(GraphBuilder, TargetRHI, TEXT("DreamPass.Export"));
			const FIntRect SourceRect(FIntPoint::ZeroValue, Source->Desc.Extent);
			const FIntRect TargetRect(FIntPoint::ZeroValue, Target->Desc.Extent);
			if (Source->Desc.Format == Target->Desc.Format && Source->Desc.Extent == Target->Desc.Extent)
			{
				AddCopyTexturePass(GraphBuilder, Source, Target);
			}
			else
			{
				AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(View),
					FScreenPassTexture(Source, SourceRect),
					FScreenPassRenderTarget(Target, TargetRect, ERenderTargetLoadAction::ENoAction));
			}

			// Materials sample through a texture reference RDG does not track. Later passes
			// must see the exported texture in a shader-readable state.
			GraphBuilder.UseExternalAccessMode(Target, ERHIAccess::SRVMask);
		}
	}

	void PrepareExportTargets(UDreamPassSubsystem& Subsystem, FSnapshotPipeline& Pipeline, const UDreamPassPipeline& Asset, const FSceneView& View)
	{
		for (FSnapshotBuffer& Buffer : Pipeline.Buffers)
		{
			if (!Buffer.Desc.bExport)
			{
				continue;
			}

			UTextureRenderTarget2D* Target = Asset.GetExportTarget(Buffer.Desc.Name);
			if (!Target)
			{
				continue;
			}

			// Sized from the view's output rect on the game thread, where the render target can be resized at all; the
			// copy scales a render-resolution buffer into it. A resize recreates the resource and clears it, so the first
			// frame after a resolution change reads a cleared target.
			FIntPoint Size;
			if (Buffer.Desc.Resolution == EDreamPassBufferResolution::Fixed)
			{
				Size = Buffer.Desc.FixedSize;
			}
			else
			{
				const FIntPoint ViewSize = View.UnscaledViewRect.Size();
				Size = FIntPoint(FMath::CeilToInt(float(ViewSize.X) * Buffer.Desc.Scale), FMath::CeilToInt(float(ViewSize.Y) * Buffer.Desc.Scale));
			}
			Size = FIntPoint(FMath::Max(Size.X, 1), FMath::Max(Size.Y, 1));

			if (Target->SizeX != Size.X || Target->SizeY != Size.Y)
			{
				Target->ResizeTarget(uint32(Size.X), uint32(Size.Y));
			}
			Buffer.ExportResource = Target->GameThread_GetRenderTargetResource();
			Subsystem.NoteExportTarget(Target, Buffer.Desc.bClear ? Buffer.Desc.ClearValue : FLinearColor::Transparent);
		}
	}

	void AfterPassWrites(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		for (const FDreamPassBufferBinding& Write : Pass.Writes)
		{
			const int32 BufferIndex = IsBuiltinBuffer(Write.Buffer) ? INDEX_NONE : Context.Pipeline.FindBuffer(Write.Buffer);
			if (!Context.Pipeline.Buffers.IsValidIndex(BufferIndex))
			{
				continue;
			}

			const FSnapshotBuffer& Buffer = Context.Pipeline.Buffers[BufferIndex];
			if (!Buffer.ExportResource || Buffer.LastWriterOrder != Context.OrderIndex)
			{
				continue;
			}

			Private::Export::CopyBuffer(Context.GraphBuilder, Context.GetView(),
				Context.ViewState.Buffers[Context.PipelineIndex][BufferIndex], *Buffer.ExportResource);
		}
	}

	void FinishViewExports(FRDGBuilder& GraphBuilder, FViewState& ViewState)
	{
		if (!ViewState.IsActive())
		{
			return;
		}
		const FViewSnapshot& Snapshot = *ViewState.Snapshot;
		for (int32 PipelineIndex = 0; PipelineIndex < Snapshot.Pipelines.Num(); ++PipelineIndex)
		{
			const FSnapshotPipeline& Pipeline = Snapshot.Pipelines[PipelineIndex];
			for (int32 BufferIndex = 0; BufferIndex < Pipeline.Buffers.Num(); ++BufferIndex)
			{
				const FSnapshotBuffer& Buffer = Pipeline.Buffers[BufferIndex];
				if (!Buffer.ExportResource || !ViewState.Executed.IsValidIndex(Buffer.LastWriterOrder)
					|| ViewState.Executed[Buffer.LastWriterOrder])
				{
					// The normal last writer already exported immediately after it ran. A missing
					// render resource on that path is not a reason to attempt the same copy again.
					continue;
				}

				// An injection can be absent, or its pass can fail (for example while a material
				// compiles). Preserve the last successful write instead of keeping an older frame.
				// A buffer allocated by a read alone is not evidence that any writer succeeded.
				for (int32 OrderIndex = Buffer.LastWriterOrder - 1; OrderIndex >= 0; --OrderIndex)
				{
					const FScheduledPass& Scheduled = Snapshot.Order[OrderIndex];
					if (Scheduled.Pipeline != PipelineIndex || !ViewState.Executed[OrderIndex])
					{
						continue;
					}
					if (Pipeline.Passes[Scheduled.Pass].Writes.ContainsByPredicate([&Buffer](const FDreamPassBufferBinding& Write)
						{ return Write.Buffer == Buffer.Desc.Name; }))
					{
						Private::Export::CopyBuffer(GraphBuilder, *ViewState.View,
							ViewState.Buffers[PipelineIndex][BufferIndex], *Buffer.ExportResource);
						break;
					}
				}
			}
		}
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
