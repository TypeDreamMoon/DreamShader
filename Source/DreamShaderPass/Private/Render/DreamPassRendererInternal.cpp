// The one file of the module that includes from the Renderer's Internal folder (see DreamShaderPass.Build.cs):
// FPostProcessingInputs, the argument of the BeforePostProcess injection point. Internal headers are the least
// stable part of the renderer between engine versions, so their use stays here, in one place to fix.

#include "Render/DreamPassSceneViewExtension.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "PostProcess/PostProcessInputs.h"

void FDreamPassSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
	using namespace UE::DreamPass;

	FFamilyState* Family = FindFamilyState(GraphBuilder);
	const int32 ViewIndex = Family ? Family->FindViewIndex(InView) : INDEX_NONE;
	if (ViewIndex == INDEX_NONE || !Inputs.SceneTextures)
	{
		return;
	}

	// Every view's PrePostProcessPass runs before any view's post-process chain (R/Private/DeferredShadingRenderer.cpp:4244-4271).
	// The scene colour here already holds lighting, fog and the translucency rendered before DOF; translucency
	// rendered after DOF is still a texture of its own, which the PostProcess.TranslucencyAfterDOF point reaches.
	FInjectionContext Context;
	Context.Injection = EDreamPassInjection::BeforePostProcess;
	Context.SceneTextures = Inputs.SceneTextures;
	Context.SceneViewRect = GetRenderViewRect(InView);
	Context.SceneColor = FScreenPassTexture(Inputs.SceneTextures->GetContents()->SceneColorTexture, Context.SceneViewRect);
	Context.bSceneColorWritable = Context.SceneColor.IsValid();
	Context.SceneDepth = Inputs.SceneTextures->GetContents()->SceneDepthTexture;
	Context.CustomDepth = Inputs.CustomDepthTexture;
	Context.bSeparateCustomStencil = Inputs.bSeparateCustomStencil;
	RunInjection(GraphBuilder, *Family, ViewIndex, Context);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
