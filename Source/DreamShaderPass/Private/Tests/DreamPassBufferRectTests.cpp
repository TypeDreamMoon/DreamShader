// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassFrame.h"
#include "Misc/AutomationTest.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderingThread.h"
#include "SceneRenderTargetParameters.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPassSceneTextureRectTest,
	"DreamShader.Pass.Logic.SceneTextureRectsAfterUpscale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamPassSceneTextureRectTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamPass;
	// The second view of a split-screen family can have a nonzero render origin; TSR's
	// colour chain may meanwhile be a separate, larger texture starting at zero.
	const FIntRect RenderRect(64, 32, 1024, 572);
	const FIntRect OutputRect(0, 0, 1920, 1080);
	const FIntRect TranslucencyRect(16, 8, 496, 278);
	const FName Names[] = { BuiltinBuffers::SceneColor, BuiltinBuffers::SceneDepth,
		BuiltinBuffers::CustomDepth, BuiltinBuffers::Velocity, BuiltinBuffers::GBufferA,
		BuiltinBuffers::GBufferB, BuiltinBuffers::GBufferC, BuiltinBuffers::GBufferD,
		BuiltinBuffers::GBufferE, BuiltinBuffers::GBufferF, BuiltinBuffers::Translucency };
	TArray<FIntRect> Rects;
	Rects.SetNum(UE_ARRAY_COUNT(Names));
	bool bAllTexturesCorrect = true;

	ENQUEUE_RENDER_COMMAND(DreamPassTestSceneTextureRects)(
		[&](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			const auto MakeTexture = [&GraphBuilder](FIntPoint Extent, const TCHAR* Name)
			{
				return GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA,
					FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable), Name);
			};
			FRDGTextureRef SceneTexture = MakeTexture(FIntPoint(1024, 576), TEXT("DreamPass.TestSceneTexture"));
			FRDGTextureRef Color = MakeTexture(OutputRect.Size(), TEXT("DreamPass.TestUpscaledColor"));
			FRDGTextureRef Translucency = MakeTexture(FIntPoint(512, 288), TEXT("DreamPass.TestTranslucency"));
			auto* Textures = GraphBuilder.AllocParameters<FSceneTextureUniformParameters>();
			Textures->GBufferATexture = SceneTexture;
			Textures->GBufferBTexture = SceneTexture;
			Textures->GBufferCTexture = SceneTexture;
			Textures->GBufferDTexture = SceneTexture;
			Textures->GBufferETexture = SceneTexture;
			Textures->GBufferFTexture = SceneTexture;
			Textures->GBufferVelocityTexture = SceneTexture;
			FPostProcessMaterialInputs PostInputs;
			PostInputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SeparateTranslucency,
				FScreenPassTexture(Translucency, TranslucencyRect));
			FInjectionContext Injection;
			Injection.Injection = EDreamPassInjection::PostProcessAfterTonemap;
			Injection.SceneViewRect = OutputRect;
			Injection.SceneTextureViewRect = RenderRect;
			Injection.SceneColor = FScreenPassTexture(Color, OutputRect);
			Injection.SceneDepth = SceneTexture;
			Injection.CustomDepth = SceneTexture;
			Injection.SceneTextures = GraphBuilder.CreateUniformBuffer(Textures);
			Injection.PostProcessInputs = &PostInputs;
			FFamilyState Family;
			FViewState View;
			FSnapshotPipeline Pipeline;
			FSnapshotPass Pass;
			FExecuteContext Context{ GraphBuilder, Family, View, Injection, Pipeline, 0, Pass, 0 };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
			{
				FDreamPassBufferBinding Binding;
				Binding.Buffer = Names[Index];
				const FScreenPassTexture Read = ResolveRead(Context, Binding);
				Rects[Index] = Read.ViewRect;
				const FRDGTextureRef ExpectedTexture = Index == 0 ? Color : Names[Index] == BuiltinBuffers::Translucency ? Translucency : SceneTexture;
				bAllTexturesCorrect &= Read.Texture == ExpectedTexture;
			}
			GraphBuilder.Execute();
		});
	FlushRenderingCommands();

	TestTrue(TEXT("each binding keeps its original texture"), bAllTexturesCorrect);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		const FIntRect Expected = Index == 0 ? OutputRect : Names[Index] == BuiltinBuffers::Translucency ? TranslucencyRect : RenderRect;
		TestEqual(FString::Printf(TEXT("%s uses the rect of its own texture"), *Names[Index].ToString()), Rects[Index], Expected);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS
