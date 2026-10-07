// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassFrame.h"
#include "Render/DreamPassSceneViewExtension.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace UE::DreamPass::ExportTests
{
	static constexpr int32 Size = 16;

	static UTextureRenderTarget2D* MakeTarget()
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		Target->ClearColor = FLinearColor::Blue;
		Target->InitCustomFormat(Size, Size, PF_FloatRGBA, /*bInForceLinearGamma*/ true);
		Target->UpdateResourceImmediate(/*bClearRenderTarget*/ true);
		return Target;
	}

	static bool ExpectColor(FAutomationTestBase& Test, const TCHAR* What, UTextureRenderTarget2D& Target, FColor Expected)
	{
		TArray<FColor> Pixels;
		if (!Test.TestTrue(FString::Printf(TEXT("%s can be read from the GPU"), What),
			Target.GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
			|| !Test.TestEqual(TEXT("the whole target was read"), Pixels.Num(), Size * Size))
		{
			return false;
		}
		for (const FColor& Pixel : Pixels)
		{
			if (Pixel != Expected)
			{
				return Test.TestEqual(FString::Printf(TEXT("%s matches the last successful write"), What), Pixel, Expected);
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPassExportSkippedWriterTest,
	"DreamShader.Pass.Render.ExportSkippedLastWriter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamPassExportSkippedWriterTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamPass;
	using namespace UE::DreamPass::ExportTests;

	TStrongObjectPtr<UTextureRenderTarget2D> Export(MakeTarget());
	TStrongObjectPtr<UTextureRenderTarget2D> Witness(MakeTarget());
	// An inactive extension: only this test dispatches its callbacks, so no project world or
	// pipeline settings need changing. The frame's passes and copies still execute on the GPU.
	TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage(), NAME_None, RF_Transient));
	const auto Extension = FSceneViewExtensions::NewExtension<FDreamPassSceneViewExtension>(World.Get(), nullptr);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime::GetTimeSinceAppStart()));
	FSceneViewInitOptions Init;
	Init.ViewFamily = &ViewFamily;
	Init.SetViewRectangle(FIntRect(0, 0, Size, Size));
	Init.ViewOrigin = FVector::ZeroVector;
	Init.ViewRotationMatrix = FMatrix::Identity;
	Init.ProjectionMatrix = FMatrix::Identity;
	FSceneView* View = new FSceneView(Init);
	View->bIsGameView = true;
	ViewFamily.Views.Add(View);

	// ScheduleView records the planned last writer before knowing whether its injection
	// happens. Reproduce that snapshot without requiring a viewport or advancing GFrameCounter.
	TSharedRef<FFamilySnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<FFamilySnapshot, ESPMode::ThreadSafe>();
	FViewSnapshot& ViewSnapshot = Snapshot->Views.AddDefaulted_GetRef();
	ViewSnapshot.bActive = true;
	ViewSnapshot.bExportView = true;
	FSnapshotPipeline& Pipeline = ViewSnapshot.Pipelines.AddDefaulted_GetRef();
	Pipeline.DebugName = TEXT("ExportSkippedLastWriter");
	FSnapshotBuffer& Buffer = Pipeline.Buffers.AddDefaulted_GetRef();
	Buffer.Desc.Name = TEXT("Mark");
	Buffer.Desc.Resolution = EDreamPassBufferResolution::Fixed;
	Buffer.Desc.FixedSize = FIntPoint(Size, Size);
	Buffer.Desc.Format = EDreamPassBufferFormat::RGBA16F;
	Buffer.Desc.ClearValue = FLinearColor::Blue;
	Buffer.Desc.bExport = true;
	Buffer.ExportResource = Export->GameThread_GetRenderTargetResource();
	Buffer.LastWriterOrder = 1;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FSnapshotPass& Pass = Pipeline.Passes.AddDefaulted_GetRef();
		Pass.Name = Index == 0 ? TEXT("Early") : TEXT("Late");
		Pass.Kind = EDreamPassKind::Clear;
		Pass.Injection = Index == 0 ? EDreamPassInjection::BeginView : EDreamPassInjection::PostProcessAfterTonemap;
		Pass.ClearValue = Index == 0 ? FLinearColor::Red : FLinearColor::Green;
		Pass.Writes.AddDefaulted_GetRef().Buffer = Buffer.Desc.Name;
		ViewSnapshot.Order.Add({ 0, Index });
		ViewSnapshot.RangeByInjection[int32(Pass.Injection)] = { Index, Index + 1 };
		ViewSnapshot.InjectionMask |= 1u << uint32(Pass.Injection);
	}
	Snapshot->InjectionMask = ViewSnapshot.InjectionMask;
	ViewFamily.GetOrCreateExtentionData<FDreamPassFamilyData>()->Snapshot = Snapshot;
	FTextureRenderTargetResource* WitnessResource = Witness->GameThread_GetRenderTargetResource();

	const auto RenderFrame = [&](bool bRunLate, bool bRunEarly = true)
	{
		// UE does not subscribe PostProcess.* callbacks when PostProcessing is off
		// (PostProcessing.cpp:760,825-849). Dispatch exactly the callbacks that remain.
		ViewFamily.EngineShowFlags.SetPostProcessing(bRunLate);
		ENQUEUE_RENDER_COMMAND(DreamPassTestExportSkippedWriter)(
			[&ViewFamily, View, Extension, WitnessResource, bRunLate, bRunEarly](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder GraphBuilder(RHICmdList);
				Extension->PreRenderViewFamily_RenderThread(GraphBuilder, ViewFamily);
				FFamilyState& Family = *FindFamilyState(GraphBuilder);
				FInjectionContext Injection;
				Injection.SceneViewRect = FIntRect(0, 0, Size, Size);
				Injection.SceneTextureViewRect = Injection.SceneViewRect;
				if (bRunEarly)
				{
					RunInjection(GraphBuilder, Family, 0, Injection);
				}
				else
				{
					// A read can allocate/clear a buffer even when no writer succeeded.
					// Merely finding that texture must not publish it as a successful write.
					const FSnapshotPipeline& FramePipeline = Family.Views[0].Snapshot->Pipelines[0];
					FExecuteContext Read{ GraphBuilder, Family, Family.Views[0], Injection,
						FramePipeline, 0, FramePipeline.Passes[0], 0 };
					ResolveRead(Read, FramePipeline.Passes[0].Writes[0]);
				}
				if (bRunLate)
				{
					Injection.Injection = EDreamPassInjection::PostProcessAfterTonemap;
					RunInjection(GraphBuilder, Family, 0, Injection);
				}
				Extension->PostRenderView_RenderThread(GraphBuilder, *View);

				// Independently read the frame-local buffer too: this keeps its GPU clear alive
				// even when the broken exporter skips the copy, and proves the early write ran.
				FRDGTextureRef WitnessTexture = RegisterExternalTexture(GraphBuilder,
					WitnessResource->GetRenderTargetTexture(), TEXT("DreamPass.ExportTestWitness"));
				AddCopyTexturePass(GraphBuilder, Family.Views[0].Buffers[0][0], WitnessTexture);
				GraphBuilder.SetTextureAccessFinal(WitnessTexture, ERHIAccess::SRVMask);
				GraphBuilder.Execute();
			});
		FlushRenderingCommands();
	};

	RenderFrame(/*bRunLate*/ true);
	ExpectColor(*this, TEXT("the control frame's own buffer"), *Witness, FColor::Green);
	ExpectColor(*this, TEXT("the control frame's export"), *Export, FColor::Green);
	RenderFrame(/*bRunLate*/ false, /*bRunEarly*/ false);
	ExpectColor(*this, TEXT("the unwritten buffer has only its initial clear"), *Witness, FColor::Blue);
	ExpectColor(*this, TEXT("no successful writer leaves the export alone"), *Export, FColor::Green);
	TestEqual(TEXT("the negative control ran no writers"), Extension->GetLastReport().PassesRun, 0);
	RenderFrame(/*bRunLate*/ false);
	ExpectColor(*this, TEXT("the skipped-writer frame's own buffer"), *Witness, FColor::Red);
	ExpectColor(*this, TEXT("the skipped-writer frame's export"), *Export, FColor::Red);
	const FDreamPassSceneViewExtension::FFrameReport Report = Extension->GetLastReport();
	TestEqual(TEXT("the early pass actually ran"), Report.PassesRun, 1);
	TestEqual(TEXT("the later injection was absent"), Report.PassesSkipped, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_WITH_CUSTOM_PASS
