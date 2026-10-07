// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassFrame.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Misc/AutomationTest.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPassDepthInitialClearTest,
	"DreamShader.Pass.Render.DepthInitialClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamPassDepthInitialClearTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamPass;
	constexpr int32 Size = 8;
	TStrongObjectPtr<UTextureRenderTarget2D> Witness(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
	Witness->InitCustomFormat(Size, Size, PF_FloatRGBA, /*bInForceLinearGamma*/ true);
	Witness->UpdateResourceImmediate(/*bClearRenderTarget*/ true);
	FTextureRenderTargetResource* WitnessResource = Witness->GameThread_GetRenderTargetResource();

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime::GetTimeSinceAppStart()));
	FSceneViewInitOptions Init;
	Init.ViewFamily = &ViewFamily;
	Init.SetViewRectangle(FIntRect(0, 0, Size, Size));
	Init.ViewOrigin = FVector::ZeroVector;
	Init.ViewRotationMatrix = FMatrix::Identity;
	Init.ProjectionMatrix = FMatrix::Identity;
	FSceneView* View = new FSceneView(Init);
	ViewFamily.Views.Add(View);

	const auto ReadDepth = [&](float ClearDepth, bool bExplicitClear)
	{
		ENQUEUE_RENDER_COMMAND(DreamPassTestDepthInitialClear)(
			[View, WitnessResource, ClearDepth, bExplicitClear](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder GraphBuilder(RHICmdList);
				FFamilyState Family;
				FViewState ViewState;
				ViewState.View = View;
				ViewState.Buffers.SetNum(1);
				ViewState.Buffers[0].Init(nullptr, 1);
				FInjectionContext Injection;
				FSnapshotPipeline Pipeline;
				FSnapshotBuffer& Buffer = Pipeline.Buffers.AddDefaulted_GetRef();
				Buffer.Desc.Name = TEXT("OwnDepth");
				Buffer.Desc.Format = EDreamPassBufferFormat::Depth32;
				Buffer.Desc.Resolution = EDreamPassBufferResolution::Fixed;
				Buffer.Desc.FixedSize = FIntPoint(Size, Size);
				Buffer.Desc.ClearValue = FLinearColor(ClearDepth, ClearDepth, ClearDepth, ClearDepth);
				FSnapshotPass Pass;
				Pass.Kind = EDreamPassKind::Clear;
				Pass.ClearValue = Buffer.Desc.ClearValue;
				Pass.Writes.AddDefaulted_GetRef().Buffer = Buffer.Desc.Name;
				FExecuteContext Context{ GraphBuilder, Family, ViewState, Injection, Pipeline, 0, Pass, 0 };
				FRDGTextureRef Depth = GetOrCreateBuffer(Context, 0);
				if (bExplicitClear)
				{
					ExecuteClearPass(Context);
				}

				// Sample the actual depth SRV into a readable color target. This keeps the clear alive
				// in RDG and observes its GPU result, rather than only checking the texture descriptor.
				FRDGTextureRef Output = RegisterExternalTexture(GraphBuilder, WitnessResource->GetRenderTargetTexture(), TEXT("DreamPass.DepthClearWitness"));
				AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(GMaxRHIFeatureLevel),
					FScreenPassTexture(Depth), FScreenPassRenderTarget(Output, ERenderTargetLoadAction::ENoAction));
				GraphBuilder.SetTextureAccessFinal(Output, ERHIAccess::SRVMask);
				GraphBuilder.Execute();
			});
		FlushRenderingCommands();
		TArray<FLinearColor> Pixels;
		if (!TestTrue(TEXT("the depth witness can be read from the GPU"), WitnessResource->ReadLinearColorPixels(Pixels))
			|| !TestEqual(TEXT("the entire witness was read"), Pixels.Num(), Size * Size))
		{
			return;
		}
		for (const FLinearColor& Pixel : Pixels)
		{
			if (!FMath::IsNearlyEqual(Pixel.R, ClearDepth))
			{
				TestEqual(FString::Printf(TEXT("%s depth clear writes %.3f"), bExplicitClear ? TEXT("explicit") : TEXT("initial"), ClearDepth), Pixel.R, ClearDepth);
				break;
			}
		}
	};

	ReadDepth(0.0f, false);
	ReadDepth(0.375f, true); // The explicit clear pass already honors the requested depth.
	ReadDepth(0.375f, false);
	ReadDepth(1.0f, false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS
