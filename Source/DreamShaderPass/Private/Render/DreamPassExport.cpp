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

			FRDGTextureRef Source = Context.ViewState.Buffers[Context.PipelineIndex][BufferIndex];
			FRHITexture* TargetRHI = Buffer.ExportResource->GetRenderTargetTexture();
			if (!Source || !TargetRHI)
			{
				continue;
			}

			// The render target is fetched from its resource every frame, never cached: a resize on the game thread
			// replaces the RHI texture (E/Private/TextureRenderTarget2D.cpp:177-206).
			FRDGTextureRef Target = RegisterExternalTexture(Context.GraphBuilder, TargetRHI, TEXT("DreamPass.Export"));

			const FIntRect SourceRect(FIntPoint::ZeroValue, Source->Desc.Extent);
			const FIntRect TargetRect(FIntPoint::ZeroValue, Target->Desc.Extent);
			if (Source->Desc.Format == Target->Desc.Format && Source->Desc.Extent == Target->Desc.Extent)
			{
				AddCopyTexturePass(Context.GraphBuilder, Source, Target);
			}
			else
			{
				AddDrawTexturePass(Context.GraphBuilder, FScreenPassViewInfo(Context.GetView()),
					FScreenPassTexture(Source, SourceRect),
					FScreenPassRenderTarget(Target, TargetRect, ERenderTargetLoadAction::ENoAction));
			}

			// Materials sample the render target through a texture reference the render graph does not track; leaving
			// it in a shader-readable state for every later pass is what makes that safe (as SceneCapture and Water do).
			Context.GraphBuilder.UseExternalAccessMode(Target, ERHIAccess::SRVMask);
		}
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
