#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamShaderPassModule.h"

#include "MaterialDomain.h"
#include "MaterialSceneTextureId.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneRenderTargetParameters.h"
#include "SystemTextures.h"

namespace UE::DreamPass
{
	namespace Private
	{
		static const FDreamPassBufferBinding* FindReadForInput(const FSnapshotPass& Pass, FName InputName)
		{
			// `read Mask = Blurred` binds the material's UserSceneTexture "Mask"; `read Blurred` the one called Blurred.
			return Pass.Reads.FindByPredicate([InputName](const FDreamPassBufferBinding& Read)
			{
				return Read.Slot.IsNone() ? Read.Buffer == InputName : Read.Slot == InputName;
			});
		}
	}

	bool IsMaterialReadyToDraw(const FMaterialRenderProxy& Proxy, ERHIFeatureLevel::Type FeatureLevel)
	{
		// In the editor a material's shader map compiles on demand: its jobs are submitted when
		// FMaterialRenderProxy::GetMaterialWithFallback meets the map incomplete (E/Private/Materials/MaterialRenderProxy.cpp:
		// 870-893). A material only a pass draws with -- an override, a pass's post-process material -- is met by no primitive
		// and no post-process volume, so asking GetMaterialNoFallback alone would wait for a compile nothing ever starts.
		// A fallback answer means not yet; it is assigned only when the chain is walked.
		const FMaterialRenderProxy* Fallback = nullptr;
		Proxy.GetMaterialWithFallback(FeatureLevel, Fallback);
		return Fallback == nullptr;
	}

	bool ExecuteFullscreenMaterialPass(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		const FSceneView& View = Context.GetView();
		FRDGBuilder& GraphBuilder = Context.GraphBuilder;
		FInjectionContext& Injection = Context.Injection;

		const UMaterialInterface* Material = Pass.Material;
		const FMaterialRenderProxy* Proxy = Material ? Material->GetRenderProxy() : nullptr;
		const bool bReady = Proxy && IsMaterialReadyToDraw(*Proxy, View.GetFeatureLevel());
		const FMaterial* Resource = bReady ? Proxy->GetMaterialNoFallback(View.GetFeatureLevel()) : nullptr;

		// While a material compiles, the engine's pass would quietly draw the default post-process material in its
		// place (R/Private/PostProcess/PostProcessMaterial.cpp, GetMaterialInfo); the pass waits instead.
		if (!Resource)
		{
			WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".Compiling"),
				FString::Printf(TEXT("DreamPass: %s.%s waits for its material to finish compiling (reported once)."), *Context.Pipeline.DebugName, *Pass.Name.ToString()));
			return false;
		}
		if (Resource->GetMaterialDomain() != MD_PostProcess)
		{
			WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".Domain"),
				FString::Printf(TEXT("DreamPass: %s.%s: a fullscreen pass needs a Post Process material; the pass is skipped."), *Context.Pipeline.DebugName, *Pass.Name.ToString()));
			return false;
		}
		const FMaterialShaderMap* ShaderMap = Resource->GetRenderingThreadShaderMap();

		if (Pass.Writes.Num() != 1)
		{
			return false;
		}
		const FDreamPassBufferBinding& Write = Pass.Writes[0];
		const FScreenPassRenderTarget Output = ResolveWrite(Context, Write);
		if (!Output.IsValid())
		{
			return false;
		}

		FPostProcessMaterialInputs Inputs;

		// On the post-process chain the material gets what the engine would give a material at the same blend location
		// -- the separate translucency, the velocity -- and the chain's current colour as the scene colour.
		if (Injection.PostProcessInputs)
		{
			Inputs.Textures = Injection.PostProcessInputs->Textures;
			Inputs.SceneWithoutWaterTextures = Injection.PostProcessInputs->SceneWithoutWaterTextures;
		}

		FDreamPassBufferBinding SceneColorBinding;
		SceneColorBinding.Buffer = BuiltinBuffers::SceneColor;
		FScreenPassTexture SceneColor = ResolveRead(Context, SceneColorBinding);
		if (!SceneColor.IsValid())
		{
			SceneColor = FScreenPassTexture(GSystemTextures.GetBlackDummy(GraphBuilder), FIntRect(0, 0, 1, 1));
		}
		Inputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SceneColor, SceneColor);
		if (Injection.Injection == EDreamPassInjection::PostProcessTranslucencyAfterDOF)
		{
			Inputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SeparateTranslucency, Injection.SceneColor);
		}

		// Named buffers go where the engine's own material chain puts UserSceneTexture inputs
		// (R/Private/PostProcess/PostProcessMaterial.cpp:1173-1223): from slot 0 up, skipping every slot a SceneTexture
		// node of the material already reads, in the order the material lists them, after the instance's renames.
		const TConstArrayView<FScriptName> UserInputs = ShaderMap->GetUserSceneTextureInputs();
		int32 UserIndex = 0;
		for (int32 Slot = 0; Slot < int32(kPostProcessMaterialInputCountMax) && UserIndex < UserInputs.Num(); ++Slot)
		{
			if (ShaderMap->UsesSceneTexture(uint32(PPI_PostProcessInput0) + uint32(Slot)))
			{
				continue;
			}

			FName InputName(UserInputs[UserIndex]);
			Proxy->GetUserSceneTextureOverride(InputName);
			++UserIndex;

			FScreenPassTexture Texture;
			if (const FDreamPassBufferBinding* Read = Private::FindReadForInput(Pass, InputName))
			{
				Texture = ResolveRead(Context, *Read);
				if (!Texture.IsValid())
				{
					return false;
				}
			}
			else if (InputName == BuiltinBuffers::SceneColor)
			{
				// As in the engine: a UserSceneTexture called SceneColor is the scene colour.
				Texture = SceneColor;
			}
			else
			{
				WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".") + InputName.ToString(),
					FString::Printf(TEXT("DreamPass: %s.%s: the material reads UserSceneTexture '%s', which no `read` binds; it samples black."),
						*Context.Pipeline.DebugName, *Pass.Name.ToString(), *InputName.ToString()));
				continue;
			}

			Inputs.SetUserSceneTextureInput(EPostProcessMaterialInput(Slot), FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Texture));
		}

		Inputs.OverrideOutput = Output;
		Inputs.bAllowSceneColorInputAsOutput = false;
		Inputs.SceneTextures = Injection.SceneTextures
			? GetSceneTextureShaderParameters(Injection.SceneTextures)
			: CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All);
		Inputs.CustomDepthTexture = Injection.CustomDepth;
		// Nanite's custom stencil in a texture of its own cannot be tested by the rasterizer (FPostProcessingInputs).
		Inputs.bManualStencilTest = Injection.bSeparateCustomStencil;

		const FScreenPassTexture Result = AddPostProcessMaterialPass(GraphBuilder, View, Inputs, Material);

		if (IsSceneColorWrite(Context, Write))
		{
			CommitSceneColor(Context, Result.IsValid() ? Result : FScreenPassTexture(Output));
		}
		return true;
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
