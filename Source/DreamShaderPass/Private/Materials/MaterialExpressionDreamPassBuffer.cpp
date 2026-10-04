#include "Materials/MaterialExpressionDreamPassBuffer.h"

#include "DreamPassPipeline.h"

#include "Engine/TextureRenderTarget2D.h"

#if WITH_EDITOR
#include "MaterialCompiler.h"
#endif

#if WITH_EDITOR && DREAMSHADER_WITH_CUSTOM_PASS
#include "Materials/Material.h"
#include "Materials/MaterialExpressionUtils.h"
#endif

#define LOCTEXT_NAMESPACE "DreamShader.Pass.BufferNode"

UMaterialExpressionDreamPassBuffer::UMaterialExpressionDreamPassBuffer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	struct FConstructorStatics
	{
		FText NAME_DreamShader;
		FConstructorStatics()
			: NAME_DreamShader(LOCTEXT("MenuCategory", "DreamShader"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// The render target's own sampler, as a Texture Sample node defaults to. A render target addresses with Wrap unless
	// told otherwise (E/Classes/Engine/TextureRenderTarget2D.h:118-125, used at E/Private/TextureRenderTarget2D.cpp:586),
	// and which is right depends on the buffer -- Clamp for one the size of the view, read at the viewport UV; Wrap for a
	// fixed-size one that tiles the world -- so it belongs on the render target the `.dsp` compiler makes for the buffer,
	// where every reader of it (Blueprints, UMG, Niagara) gets it too, not on this node.
	SamplerSource = SSM_FromTextureAsset;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_DreamShader);

	// The preview is the render target's latest picture, which is exactly what the material reads.
	bCollapsed = false;
#endif
}

UTextureRenderTarget2D* UMaterialExpressionDreamPassBuffer::ResolveExportTarget(const UDreamPassPipeline* InPipeline, FName InBuffer, FText* OutProblem)
{
	auto Fail = [OutProblem](const FText& Problem) -> UTextureRenderTarget2D*
	{
		if (OutProblem)
		{
			*OutProblem = Problem;
		}
		return nullptr;
	};

	if (!InPipeline)
	{
		return Fail(LOCTEXT("NoPipeline", "Dream Pass Buffer: no Pipeline is set."));
	}

	const FText PipelineName = FText::FromString(InPipeline->GetName());
	const FText BufferName = FText::FromName(InBuffer);
	if (InBuffer.IsNone())
	{
		return Fail(FText::Format(LOCTEXT("NoBuffer", "Dream Pass Buffer: no Buffer of '{0}' is set."), PipelineName));
	}

	// SceneColor, CustomStencil and the rest live in the frame's render graph only; nothing copies them out.
	if (UE::DreamPass::IsBuiltinBuffer(InBuffer))
	{
		return Fail(DescribeBuiltinBufferRead(InBuffer, PipelineName));
	}

	const FDreamPassBufferDesc* Desc = InPipeline->FindBuffer(InBuffer);
	if (!Desc)
	{
		return Fail(FText::Format(LOCTEXT("UnknownBuffer", "Dream Pass Buffer: '{1}' has no buffer '{0}'."), BufferName, PipelineName));
	}

	if (!Desc->bExport)
	{
		return Fail(FText::Format(
			LOCTEXT("NotExported", "Dream Pass Buffer: buffer '{0}' of '{1}' is not exported. Declare it with Export = true in the .dsp."),
			BufferName, PipelineName));
	}

	// A material declares the texture as a float Texture2D and samples it with a filtering sampler; an integer or a depth
	// buffer cannot be read that way.
	if (Desc->Format == EDreamPassBufferFormat::R32U || Desc->Format == EDreamPassBufferFormat::RG32U || Desc->Format == EDreamPassBufferFormat::Depth32)
	{
		return Fail(FText::Format(
			LOCTEXT("NotSampleable", "Dream Pass Buffer: buffer '{0}' of '{1}' is {2}, which a material cannot sample. Export a float or normalized buffer instead."),
			BufferName, PipelineName, FText::FromString(UE::DreamPass::LexToString(Desc->Format))));
	}

	UTextureRenderTarget2D* Target = InPipeline->GetExportTarget(InBuffer);
	if (!Target)
	{
		return Fail(FText::Format(
			LOCTEXT("NoTarget", "Dream Pass Buffer: buffer '{0}' of '{1}' is exported but the pipeline has no render target for it. Compile the .dsp again."),
			BufferName, PipelineName));
	}

	return Target;
}

FText UMaterialExpressionDreamPassBuffer::DescribeBuiltinBufferRead(FName InBuffer, const FText& PipelineName)
{
	return FText::Format(
		LOCTEXT("BuiltinBuffer", "Dream Pass Buffer: '{0}' is a built-in buffer, which only passes can read; a material reads a buffer '{1}' declares with Export = true."),
		FText::FromName(InBuffer), PipelineName);
}

#if WITH_EDITOR

int32 UMaterialExpressionDreamPassBuffer::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
#if DREAMSHADER_WITH_CUSTOM_PASS
	FText Problem;
	UTextureRenderTarget2D* Target = ResolveExportTarget(Pipeline, Buffer, &Problem);
	if (!Target)
	{
		return Compiler->Errorf(TEXT("%s"), *Problem.ToString());
	}

	// Compiler->Texture looks the texture up in the material's list of referenced textures and asserts when it is not
	// there (E/Private/Materials/HLSLMaterialTranslator.cpp:8541-8555). The list is built from GetReferencedTexture when
	// the material refreshes its cached expression data (MaterialCachedData.cpp:557-564): on load, on edit and on a forced
	// recompile (Material.cpp:4593, 5414, 7291) -- not when the pipeline changes. A `.dsp` compiled since then that gave
	// the buffer its render target, or a new one, leaves the list behind, and that is an error here instead of an assert
	// there. The list asked is Material's, the one the material's own resource falls back to
	// (E/Private/Materials/MaterialShared.cpp:1663-1680) and the node previews copy (Engine/Source/Editor/MaterialEditor/
	// Private/MaterialEditor.cpp:337). A node in a material function has no Material: the calling material's list is the
	// one that counts, and nothing here can see it.
	if (Material && Material->GetDefaultTextureIdx(Target) == INDEX_NONE)
	{
		return Compiler->Errorf(TEXT("%s"), *FText::Format(
			LOCTEXT("StaleTextureList", "Dream Pass Buffer: the render target of '{0}.{1}' is newer than this material's list of textures. Recompile the material, or the .dss it is built from."),
			FText::FromString(Pipeline->GetName()), FText::FromName(Buffer)).ToString());
	}

	// The sampler type the texture itself calls for (E/Public/Materials/MaterialExpressionUtils.cpp:14-49), which is what
	// a Texture Sample would be verified against (:81-116), so there is nothing left to verify. For a render target as the
	// pipeline makes one (TEXTUREGROUP_RenderTarget, default compression) that is Color or LinearColor by its SRGB flag,
	// and the two decode alike (SH/Private/MaterialTexture.ush:144-166).
	const EMaterialSamplerType SamplerType = MaterialExpressionUtils::GetSamplerTypeForTexture(Target);

	// No derivatives outside the pixel shader: in a vertex shader (World Position Offset) or a compute one, mip 0 at an
	// explicit level. The translator would force the same for TMVM_None (HLSLMaterialTranslator.cpp:7083-7093); saying it
	// here keeps the node independent of that.
	const bool bPixelShader = Compiler->GetCurrentShaderFrequency() == SF_Pixel;
	const ETextureMipValueMode MipValueMode = bPixelShader ? TMVM_None : TMVM_MipLevel;

	int32 TextureReferenceIndex = INDEX_NONE;
	const int32 TextureCode = Compiler->Texture(Target, TextureReferenceIndex, SamplerType, SamplerSource, MipValueMode);
	if (TextureCode == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// The default is ScreenPosition's ViewportUV (MaterialExpressions.cpp:10379-10386), available in vertex and pixel
	// shaders alike (HLSLMaterialTranslator.cpp:5922-5939). The runtime sizes an exported buffer of view resolution from the
	// view rect and copies the whole buffer into the whole render target (Render/DreamPassExport.cpp), so 0..1 over the
	// viewport is 0..1 over the render target.
	const int32 CoordinateCode = Coordinates.GetTracedInput().Expression
		? Coordinates.Compile(Compiler)
		: Compiler->GetViewportUV();

	// No automatic view mip bias: that sharpens textures under temporal upscaling, and a buffer is data read where it is.
	// With it off, a shared sampler is the material's own Clamp/Wrap_WorldGroupSettings (HLSLMaterialTranslator.cpp:7360-7400).
	return Compiler->TextureSample(
		TextureCode,
		CoordinateCode,
		SamplerType,
		bPixelShader ? INDEX_NONE : Compiler->Constant(0.0f),
		INDEX_NONE,
		MipValueMode,
		SamplerSource,
		TGM_None,
		TextureReferenceIndex,
		/*AutomaticViewMipBias*/ false);
#else
	return Compiler->Errorf(TEXT("%s"), *LOCTEXT("NeedsEngine", "Dream Pass Buffer needs Unreal Engine 5.8 or later.").ToString());
#endif
}

void UMaterialExpressionDreamPassBuffer::GetCaption(TArray<FString>& OutCaptions) const
{
	const FString PipelineName = Pipeline ? Pipeline->GetName() : FString(TEXT("None"));
	OutCaptions.Add(FString::Printf(TEXT("Dream Pass Buffer: %s.%s"), *PipelineName, *Buffer.ToString()));
}

FText UMaterialExpressionDreamPassBuffer::GetKeywords() const
{
	return LOCTEXT("Keywords", "dream pass custom pass buffer export exported render target dreamshader");
}

#if DREAMSHADER_WITH_CUSTOM_PASS

EMaterialValueType UMaterialExpressionDreamPassBuffer::GetInputValueType(int32 InputIndex)
{
	// Coordinates, the only pin. A 2D render target takes a float2; the material editor still connects any number to it
	// (MaterialExpressions.cpp:554-589) and the translator truncates or replicates it to one.
	return InputIndex == 0 ? MCT_Float2 : MCT_Unknown;
}

EMaterialValueType UMaterialExpressionDreamPassBuffer::GetOutputValueType(int32 OutputIndex)
{
	// The one unnamed, unmasked output is a float4 sample. Without this the base class says "some float" (MCT_Float,
	// MaterialExpressions.cpp:1929-1970), and DreamShader's catalog would type `UE.DreamPassBuffer(...)` as a float1.
	return OutputIndex == 0 ? MCT_Float4 : MCT_Unknown;
}

void UMaterialExpressionDreamPassBuffer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// A new Pipeline or Buffer is a new referenced texture. The material editor refreshes the material's list in its own
	// PostEditChange, but compiles the "Preview Node" material before that (MaterialEditor.cpp:2685-2733), and a change
	// made by a script refreshes nothing. Refreshed here first, Compile's check above and Compiler->Texture both find
	// the new render target. Before Super, which marks the node for a preview update (MaterialExpressions.cpp:1698-1702).
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (Material && (MemberName == GET_MEMBER_NAME_CHECKED(ThisClass, Pipeline) || MemberName == GET_MEMBER_NAME_CHECKED(ThisClass, Buffer)))
	{
		Material->UpdateCachedExpressionData();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#endif // WITH_EDITOR

#if DREAMSHADER_WITH_CUSTOM_PASS

UObject* UMaterialExpressionDreamPassBuffer::GetReferencedTexture() const
{
	// Collected into the material's referenced textures whenever its cached expression data is rebuilt
	// (MaterialCachedData.cpp:557-564), which is what Compiler->Texture resolves against and what cooks the render target
	// with the material (UMaterialInterface::ReferencedDefaultTextures). Null while the node cannot be resolved, like a
	// Texture Sample without a texture; Compile then reports why.
	return ResolveExportTarget(Pipeline, Buffer);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#undef LOCTEXT_NAMESPACE
