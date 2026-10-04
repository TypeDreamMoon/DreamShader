#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamShaderPassModule.h"
#include "Render/DreamPassSceneViewExtension.h"

#include "FXRenderingUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"

namespace UE::DreamPass
{
	namespace Private
	{
		static bool IsUnsignedFormat(EDreamPassBufferFormat Format)
		{
			return Format == EDreamPassBufferFormat::R32U || Format == EDreamPassBufferFormat::RG32U;
		}

		/** A built-in texture of the scene textures uniform buffer, by its `.dsp` name; null for one it does not hold. */
		static FRDGTextureRef GetSceneTexture(const FSceneTextureUniformParameters& SceneTextures, FName Name)
		{
			using namespace BuiltinBuffers;
			if (Name == SceneColor)    return SceneTextures.SceneColorTexture;
			if (Name == SceneDepth)    return SceneTextures.SceneDepthTexture;
			if (Name == CustomDepth)   return SceneTextures.CustomDepthTexture;
			if (Name == GBufferA)      return SceneTextures.GBufferATexture;
			if (Name == GBufferB)      return SceneTextures.GBufferBTexture;
			if (Name == GBufferC)      return SceneTextures.GBufferCTexture;
			if (Name == GBufferD)      return SceneTextures.GBufferDTexture;
			if (Name == GBufferE)      return SceneTextures.GBufferETexture;
			if (Name == GBufferF)      return SceneTextures.GBufferFTexture;
			if (Name == Velocity)      return SceneTextures.GBufferVelocityTexture;
			return nullptr;
		}

		static bool IsGBufferName(FName Name)
		{
			using namespace BuiltinBuffers;
			return Name == GBufferA || Name == GBufferB || Name == GBufferC || Name == GBufferD || Name == GBufferE || Name == GBufferF;
		}

		/** What a write to SceneColor targets at this point: the separate translucency at TranslucencyAfterDOF. */
		static FName GetChainBufferName(const FInjectionContext& Injection)
		{
			return Injection.Injection == EDreamPassInjection::PostProcessTranslucencyAfterDOF ? BuiltinBuffers::Translucency : BuiltinBuffers::SceneColor;
		}

		static void ClearBuffer(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture, const FSnapshotBuffer& Buffer)
		{
			const FDreamPassBufferDesc& Desc = Buffer.Desc;
			if (Desc.Format == EDreamPassBufferFormat::Depth32)
			{
				// Reverse Z: the far plane is 0.
				AddClearDepthStencilPass(GraphBuilder, Texture, true, 0.0f, true, 0);
				return;
			}
			if (IsUnsignedFormat(Desc.Format))
			{
				const FUintVector4 Value(uint32(Desc.ClearValue.R), uint32(Desc.ClearValue.G), uint32(Desc.ClearValue.B), uint32(Desc.ClearValue.A));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), Value);
				return;
			}
			if (Buffer.bNeedsRenderTarget)
			{
				AddClearRenderTargetPass(GraphBuilder, Texture, Desc.ClearValue);
			}
			else
			{
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), Desc.ClearValue);
			}
		}

		static FString DescribeBinding(const FExecuteContext& Context, FName Buffer)
		{
			return FString::Printf(TEXT("%s.%s ('%s' at %s)"), *Context.Pipeline.DebugName, *Context.Pass.Name.ToString(), *Buffer.ToString(), LexToString(Context.Injection.Injection));
		}
	}

	FIntRect GetRenderViewRect(const FSceneView& View)
	{
		return UE::FXRenderingUtils::GetRawViewRectUnsafe(View);
	}

	FIntRect GetOutputViewRect(const FSceneView& View)
	{
		return View.UnscaledViewRect;
	}

	FIntPoint GetBufferExtent(const FSceneView& View, const FDreamPassBufferDesc& Desc)
	{
		FIntPoint Size;
		switch (Desc.Resolution)
		{
		case EDreamPassBufferResolution::Fixed:
			Size = Desc.FixedSize;
			break;
		case EDreamPassBufferResolution::Output:
			Size = GetOutputViewRect(View).Size();
			break;
		default:
			Size = GetRenderViewRect(View).Size();
			break;
		}

		if (Desc.Resolution != EDreamPassBufferResolution::Fixed)
		{
			Size = FIntPoint(FMath::CeilToInt(float(Size.X) * Desc.Scale), FMath::CeilToInt(float(Size.Y) * Desc.Scale));
		}
		return FIntPoint(FMath::Max(Size.X, 1), FMath::Max(Size.Y, 1));
	}

	FRDGTextureRef GetOrCreateBuffer(FExecuteContext& Context, int32 BufferIndex)
	{
		TArray<FRDGTextureRef>& Textures = Context.ViewState.Buffers[Context.PipelineIndex];
		if (FRDGTextureRef Existing = Textures[BufferIndex])
		{
			return Existing;
		}

		const FSnapshotBuffer& Buffer = Context.Pipeline.Buffers[BufferIndex];
		const FDreamPassBufferDesc& Desc = Buffer.Desc;
		const FIntPoint Extent = GetBufferExtent(Context.GetView(), Desc);
		const EPixelFormat Format = GetPixelFormat(Desc.Format);

		ETextureCreateFlags Flags = TexCreate_ShaderResource;
		if (Desc.Format == EDreamPassBufferFormat::Depth32)
		{
			Flags |= TexCreate_DepthStencilTargetable;
		}
		else
		{
			if (Buffer.bNeedsRenderTarget)
			{
				Flags |= TexCreate_RenderTargetable;
			}
			// Integer formats are cleared through a UAV whatever writes them: a clear colour means nothing to them.
			if (Buffer.bNeedsUAV || Private::IsUnsignedFormat(Desc.Format))
			{
				Flags |= TexCreate_UAV;
			}
		}

		const FClearValueBinding ClearBinding = Desc.Format == EDreamPassBufferFormat::Depth32
			? FClearValueBinding::DepthFar
			: FClearValueBinding(Desc.ClearValue);

		FRDGTextureRef Texture = Context.GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(Extent, Format, ClearBinding, Flags, uint8(FMath::Clamp(Desc.Mips, 1, 14))),
			TEXT("DreamPass.Buffer"));

		// Cleared even when the buffer says `Clear = None`: the binder already refused a read before every write, and
		// a write that loads a texture nothing produced is what the render graph's validation reports. The clear is a
		// fast clear on every desktop RHI.
		Private::ClearBuffer(Context.GraphBuilder, Texture, Buffer);
		Textures[BufferIndex] = Texture;

		if (Desc.bHistory)
		{
			const uint32 ViewKey = Context.ViewState.Snapshot->ViewKey;
			if (ViewKey == 0)
			{
				WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Desc.Name.ToString() + TEXT(".NoViewState"),
					FString::Printf(TEXT("DreamPass: %s.%s keeps history, but a view without a view state (a scene capture with neither bCaptureEveryFrame nor bAlwaysPersistRenderingState, a thumbnail) has none; '.Previous' reads black there."),
						*Context.Pipeline.DebugName, *Desc.Name.ToString()));
			}
			else if (FDreamPassSceneViewExtension::FHistoryEntry* Entry = Context.Family.Extension->FindHistory({ ViewKey, Context.Pipeline.Identity, Desc.Name }))
			{
				// Last frame's texture is only reused when it still matches: a resized view starts its history over.
				if (Entry->Texture.IsValid() && Entry->Texture->GetDesc().Extent == Extent && Entry->Texture->GetDesc().Format == Format)
				{
					Context.ViewState.PreviousBuffers[Context.PipelineIndex][BufferIndex] = Context.GraphBuilder.RegisterExternalTexture(Entry->Texture, TEXT("DreamPass.History"));
				}
			}
		}

		return Texture;
	}

	FScreenPassTexture ResolveRead(FExecuteContext& Context, const FDreamPassBufferBinding& Binding)
	{
		const FName Name = Binding.Buffer;

		if (!IsBuiltinBuffer(Name))
		{
			const int32 BufferIndex = Context.Pipeline.FindBuffer(Name);
			if (!Context.Pipeline.Buffers.IsValidIndex(BufferIndex))
			{
				WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s names no buffer of the pipeline; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
				return FScreenPassTexture();
			}

			FRDGTextureRef Current = GetOrCreateBuffer(Context, BufferIndex);
			if (!Binding.bPrevious)
			{
				return FScreenPassTexture(Current, FIntRect(FIntPoint::ZeroValue, Current->Desc.Extent));
			}

			// The first frame of a history, or a view that cannot keep one, reads black.
			FRDGTextureRef Previous = Context.ViewState.PreviousBuffers[Context.PipelineIndex][BufferIndex];
			if (!Previous)
			{
				Previous = GSystemTextures.GetBlackDummy(Context.GraphBuilder);
			}
			return FScreenPassTexture(Previous, FIntRect(FIntPoint::ZeroValue, Previous->Desc.Extent));
		}

		FInjectionContext& Injection = Context.Injection;
		const FName ChainName = Private::GetChainBufferName(Injection);

		if (Name == ChainName)
		{
			if (!Injection.SceneColor.IsValid())
			{
				WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s: there is no scene colour at this injection point; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
			}
			return Injection.SceneColor;
		}

		// At TranslucencyAfterDOF the chain is the translucency, and the scene colour is the chain's other input.
		if (Injection.PostProcessInputs && (Name == BuiltinBuffers::SceneColor || Name == BuiltinBuffers::Translucency))
		{
			const EPostProcessMaterialInput Input = Name == BuiltinBuffers::SceneColor ? EPostProcessMaterialInput::SceneColor : EPostProcessMaterialInput::SeparateTranslucency;
			const FScreenPassTextureSlice Slice = Injection.PostProcessInputs->GetInput(Input);
			if (Slice.IsValid())
			{
				return FScreenPassTexture::CopyFromSlice(Context.GraphBuilder, Slice);
			}
		}

		if (Name == BuiltinBuffers::CustomStencil)
		{
			WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s: CustomStencil is an integer view of the custom depth texture and cannot be bound as an input. Read it through the scene textures -- a SceneTexture node in a material, CalcSceneCustomStencil in HLSL."), *Private::DescribeBinding(Context, Name)));
			return FScreenPassTexture();
		}

		FRDGTextureRef Texture = nullptr;
		if (Name == BuiltinBuffers::SceneDepth && Injection.SceneDepth)
		{
			Texture = Injection.SceneDepth;
		}
		else if (Name == BuiltinBuffers::CustomDepth && Injection.CustomDepth)
		{
			Texture = Injection.CustomDepth;
		}
		else if (Injection.SceneTextures)
		{
			Texture = Private::GetSceneTexture(*Injection.SceneTextures->GetContents(), Name);
		}

		if (!Texture)
		{
			WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s: the injection point has no such texture; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
			return FScreenPassTexture();
		}
		return FScreenPassTexture(Texture, Injection.SceneViewRect);
	}

	FScreenPassRenderTarget ResolveWrite(FExecuteContext& Context, const FDreamPassBufferBinding& Binding)
	{
		const FName Name = Binding.Buffer;
		FInjectionContext& Injection = Context.Injection;

		if (Name == Private::GetChainBufferName(Injection))
		{
			if (!Injection.bSceneColorWritable || !Injection.SceneColor.IsValid())
			{
				WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s: the scene colour cannot be written at this injection point; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
				return FScreenPassRenderTarget();
			}

			// A scratch texture the pass writes and CommitSceneColor brings back, so a pass that reads the scene colour
			// never reads and writes one texture. One that covers only part of the view (a mesh pass) starts from a
			// copy of what is underneath.
			const FRDGTextureDesc& SceneDesc = Injection.SceneColor.Texture->Desc;
			ETextureCreateFlags Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
			if (Context.Pass.Kind == EDreamPassKind::Compute)
			{
				Flags |= TexCreate_UAV;
			}
			FRDGTextureRef Scratch = Context.GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(SceneDesc.Extent, SceneDesc.Format, FClearValueBinding::Black, Flags),
				TEXT("DreamPass.SceneColor"));

			ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ENoAction;
			if (Context.Pass.Kind != EDreamPassKind::Fullscreen)
			{
				AddCopyTexturePass(Context.GraphBuilder, Injection.SceneColor.Texture, Scratch,
					Injection.SceneColor.ViewRect.Min, Injection.SceneColor.ViewRect.Min, Injection.SceneColor.ViewRect.Size());
				LoadAction = ERenderTargetLoadAction::ELoad;
			}
			return FScreenPassRenderTarget(Scratch, Injection.SceneColor.ViewRect, LoadAction);
		}

		if (IsBuiltinBuffer(Name))
		{
			// The GBuffer is a render target of the base pass and nothing has read it yet: written in place.
			if (Injection.Injection == EDreamPassInjection::AfterBasePass && Private::IsGBufferName(Name) && Injection.SceneTextures)
			{
				if (FRDGTextureRef GBuffer = Private::GetSceneTexture(*Injection.SceneTextures->GetContents(), Name))
				{
					return FScreenPassRenderTarget(GBuffer, Injection.SceneViewRect, ERenderTargetLoadAction::ELoad);
				}
			}
			WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s: a built-in texture that cannot be written here; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
			return FScreenPassRenderTarget();
		}

		const int32 BufferIndex = Context.Pipeline.FindBuffer(Name);
		if (!Context.Pipeline.Buffers.IsValidIndex(BufferIndex) || Context.Pipeline.Buffers[BufferIndex].Desc.Format == EDreamPassBufferFormat::Depth32)
		{
			WarnOnce(Private::DescribeBinding(Context, Name), FString::Printf(TEXT("DreamPass: %s is not a colour buffer of the pipeline; the pass is skipped."), *Private::DescribeBinding(Context, Name)));
			return FScreenPassRenderTarget();
		}

		FRDGTextureRef Texture = GetOrCreateBuffer(Context, BufferIndex);
		return FScreenPassRenderTarget(Texture, FIntRect(FIntPoint::ZeroValue, Texture->Desc.Extent), ERenderTargetLoadAction::ELoad);
	}

	bool IsSceneColorWrite(const FExecuteContext& Context, const FDreamPassBufferBinding& Binding)
	{
		return Binding.Buffer == Private::GetChainBufferName(Context.Injection);
	}

	void CommitSceneColor(FExecuteContext& Context, const FScreenPassTexture& Result)
	{
		FInjectionContext& Injection = Context.Injection;
		if (!Result.IsValid() || !Injection.SceneColor.IsValid())
		{
			return;
		}

		if (Injection.bPostProcessChain)
		{
			Injection.SceneColor = Result;
			return;
		}

		const FScreenPassTexture& Target = Injection.SceneColor;
		if (Result.Texture->Desc.Format == Target.Texture->Desc.Format && Result.ViewRect.Size() == Target.ViewRect.Size())
		{
			AddCopyTexturePass(Context.GraphBuilder, Result.Texture, Target.Texture, Result.ViewRect.Min, Target.ViewRect.Min, Result.ViewRect.Size());
		}
		else
		{
			AddDrawTexturePass(Context.GraphBuilder, FScreenPassViewInfo(Context.GetView()), Result, FScreenPassRenderTarget(Target, ERenderTargetLoadAction::ELoad));
		}
	}

	void FinishViewBuffers(FRDGBuilder& GraphBuilder, FFamilyState& Family, FViewState& ViewState)
	{
		if (!ViewState.Snapshot || ViewState.Snapshot->ViewKey == 0 || !Family.Extension)
		{
			return;
		}

		for (int32 PipelineIndex = 0; PipelineIndex < ViewState.Snapshot->Pipelines.Num(); ++PipelineIndex)
		{
			const FSnapshotPipeline& Pipeline = ViewState.Snapshot->Pipelines[PipelineIndex];
			for (int32 BufferIndex = 0; BufferIndex < Pipeline.Buffers.Num(); ++BufferIndex)
			{
				FRDGTextureRef Texture = ViewState.Buffers[PipelineIndex][BufferIndex];
				if (!Texture || !Pipeline.Buffers[BufferIndex].Desc.bHistory)
				{
					continue;
				}

				// The entry is heap-allocated and stays where it is until it goes idle, so the extraction may hold its
				// address until the graph executes.
				FDreamPassSceneViewExtension::FHistoryEntry& Entry = Family.Extension->FindOrAddHistory({ ViewState.Snapshot->ViewKey, Pipeline.Identity, Pipeline.Buffers[BufferIndex].Desc.Name });
				Entry.LastUsedFrame = Family.Snapshot->FrameNumber;
				GraphBuilder.QueueTextureExtraction(Texture, &Entry.Texture);
			}
		}
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
