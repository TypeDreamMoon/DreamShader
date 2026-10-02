#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamShaderPassModule.h"

#include "RenderGraphUtils.h"
#include "ScreenPass.h"

namespace UE::DreamPass
{
	bool ExecuteClearPass(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		if (Pass.Writes.Num() != 1)
		{
			return false;
		}
		const FDreamPassBufferBinding& Write = Pass.Writes[0];
		FRDGBuilder& GraphBuilder = Context.GraphBuilder;

		const int32 BufferIndex = IsBuiltinBuffer(Write.Buffer) ? INDEX_NONE : Context.Pipeline.FindBuffer(Write.Buffer);
		if (Context.Pipeline.Buffers.IsValidIndex(BufferIndex))
		{
			const FSnapshotBuffer& Buffer = Context.Pipeline.Buffers[BufferIndex];
			FRDGTextureRef Texture = GetOrCreateBuffer(Context, BufferIndex);

			if (Buffer.Desc.Format == EDreamPassBufferFormat::Depth32)
			{
				// `Value` is the depth: reverse Z, so 0 is the far plane and 1 the near one.
				AddClearDepthStencilPass(GraphBuilder, Texture, true, Pass.ClearValue.R, true, 0);
			}
			else if (Buffer.Desc.Format == EDreamPassBufferFormat::R32U || Buffer.Desc.Format == EDreamPassBufferFormat::RG32U)
			{
				const FUintVector4 Value(uint32(Pass.ClearValue.R), uint32(Pass.ClearValue.G), uint32(Pass.ClearValue.B), uint32(Pass.ClearValue.A));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), Value);
			}
			else if (EnumHasAnyFlags(Texture->Desc.Flags, TexCreate_RenderTargetable))
			{
				AddClearRenderTargetPass(GraphBuilder, Texture, Pass.ClearValue);
			}
			else
			{
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), Pass.ClearValue);
			}
			return true;
		}

		// The scene colour (or a GBuffer target at AfterBasePass): only the view's own rect is cleared.
		const FScreenPassRenderTarget Target = ResolveWrite(Context, Write);
		if (!Target.IsValid())
		{
			return false;
		}
		AddClearRenderTargetPass(GraphBuilder, Target.Texture, Pass.ClearValue, Target.ViewRect);
		if (IsSceneColorWrite(Context, Write))
		{
			CommitSceneColor(Context, Target);
		}
		return true;
	}

	bool ExecuteCopyPass(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		if (Pass.Reads.Num() != 1 || Pass.Writes.Num() != 1)
		{
			return false;
		}

		const FScreenPassTexture Source = ResolveRead(Context, Pass.Reads[0]);
		if (!Source.IsValid())
		{
			return false;
		}
		const FScreenPassRenderTarget Target = ResolveWrite(Context, Pass.Writes[0]);
		if (!Target.IsValid())
		{
			return false;
		}

		// A plain copy when format and size agree, a scaled, converting draw otherwise -- a copy of the scene colour
		// into a half-resolution buffer is the usual case.
		const bool bSameShape = Source.Texture->Desc.Format == Target.Texture->Desc.Format && Source.ViewRect.Size() == Target.ViewRect.Size();
		if (bSameShape)
		{
			AddCopyTexturePass(Context.GraphBuilder, Source.Texture, Target.Texture, Source.ViewRect.Min, Target.ViewRect.Min, Source.ViewRect.Size());
		}
		else
		{
			AddDrawTexturePass(Context.GraphBuilder, FScreenPassViewInfo(Context.GetView()), Source, Target);
		}

		if (IsSceneColorWrite(Context, Pass.Writes[0]))
		{
			CommitSceneColor(Context, Target);
		}
		return true;
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
