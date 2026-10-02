// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Pass.Render.* and DreamShader.Pass.Lifecycle.* -- what the Custom Pass runtime draws, read back from real
// frames. Each case builds its pipelines (and, for mesh and fullscreen passes, its materials) in C++, renders a 64x64
// scene capture of a small game world through them, reads the capture back and asserts on texels: a pass's write where the
// frame shows it, at every injection point a scene capture reaches; history from one frame to the next; what keeps a
// pipeline out of a view; the sources that activate one; the r.DreamPass.Visualize corner; mesh passes by list, layer and
// stencil, in each material mode and depth mode; fullscreen material passes that write a buffer and read one; and edits,
// removals and teardown between frames.
//
// They render, so they need a real RHI. NonNullRHI keeps them out of the -nullrhi suite; the Rhi preset of
// Tools/Tests/Invoke-DreamShaderTests.ps1 runs them with -RenderOffscreen -d3d12. What the subsystem decides without
// rendering is DreamShader.Pass.Logic.*. The HLSL passes, exported buffers and the documented example pipelines come later;
// the end of the file lists them.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

// The runtime's renderer half exists on UE 5.8 and later only (DreamShaderPass.Build.cs). Below that there is nothing to
// render through, and so no case.
#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassComponent.h"
#include "DreamPassLayerComponent.h"
#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamPassSubsystem.h"
#include "DreamPassTypes.h"
#include "DreamPassVolume.h"
#include "Materials/MaterialExpressionDreamPassOutput.h"

#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionUserSceneTexture.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "RHIShaderPlatform.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::PassRenderTests
{
	/** The capture is this many texels square: small, because every case renders several frames of it. */
	static constexpr int32 CaptureSize = 64;

	/** The backdrop pipelines' Order: before every pipeline a case builds, so whatever those write lands on top of it. */
	static constexpr int32 BackdropOrder = -100;

	/**
	 * Where the mesh cases put the engine's 100 cm cube. The capture sits at the origin looking down +X with a 90 degree field
	 * of view, so a plane d in front of it shows 2d by 2d: the centre cube's front face, 150 cm away, covers the middle third
	 * of the view (texels 21 to 42 each way); the front face of the upper cube, 110 cm higher and clear of it, rows 0 to 19 of
	 * the same columns.
	 */
	static const FVector CenterCubeLocation(200.0, 0.0, 0.0);
	static const FVector UpperCubeLocation(200.0, 0.0, 110.0);

	/** A texel of the centre cube, one of the upper cube, and one no cube covers. */
	static const FIntPoint CenterTexel(CaptureSize / 2, CaptureSize / 2);
	static const FIntPoint UpperTexel(CaptureSize / 2, 8);
	static const FIntPoint BackgroundTexel(2, 2);

	/** The mesh output material's vector parameter, which its Output0 writes. */
	static const TCHAR* const MeshColorParameter = TEXT("Color");

	/** How a texel is compared with the colour a pass wrote. Every colour written is pure: each channel 0 or 1. */
	enum class ETexelMatch : uint8
	{
		/** As written, give or take a rounding step: the write reached the picture after the tonemapper. */
		Exact,
		/** Its one lit channel stands out: the write went through the tonemapper, whose curve moves every channel. */
		Dominant,
		/** Black: the empty world, with nothing drawn over it. */
		Dark,
	};

	// --- pipelines --------------------------------------------------------------------------------------------------

	static FDreamPassBufferBinding MakeBufferBinding(FName Slot, FName Buffer, bool bPrevious = false)
	{
		FDreamPassBufferBinding Binding;
		Binding.Slot = Slot;
		Binding.Buffer = Buffer;
		Binding.bPrevious = bPrevious;
		return Binding;
	}

	/**
	 * A transient pipeline that runs in scene captures only, the one kind of view these tests render, with nothing in it
	 * yet. Its name is unique for the session, which also gives the runtime's warn-once keys (pipeline.pass, pipeline.buffer)
	 * a fresh start in every case.
	 */
	static UDreamPassPipeline* MakeCapturePipeline(const TCHAR* Name, int32 Order = 0)
	{
		UPackage* Package = GetTransientPackage();
		UDreamPassPipeline* Pipeline = NewObject<UDreamPassPipeline>(Package, MakeUniqueObjectName(Package, UDreamPassPipeline::StaticClass(), Name), RF_Transient);
		Pipeline->Order = Order;
		Pipeline->Views = int32(EDreamPassViewFlags::SceneCapture);
		return Pipeline;
	}

	/** An RGBA16F buffer at render resolution. ClearValue is what it holds where it is created, before any pass writes it. */
	static void AddColorBuffer(UDreamPassPipeline& Pipeline, FName Name, const FLinearColor& ClearValue, bool bHistory = false)
	{
		FDreamPassBufferDesc& Buffer = Pipeline.Buffers.AddDefaulted_GetRef();
		Buffer.Name = Name;
		Buffer.Format = EDreamPassBufferFormat::RGBA16F;
		Buffer.Resolution = EDreamPassBufferResolution::Render;
		Buffer.ClearValue = ClearValue;
		Buffer.bHistory = bHistory;
	}

	/** A clear has no slot of its own; the binding is named after its buffer. */
	static FDreamPassDesc& AddClearPass(UDreamPassPipeline& Pipeline, FName Name, EDreamPassInjection Injection, FName Target, const FLinearColor& Value)
	{
		FDreamPassDesc& Pass = Pipeline.Passes.AddDefaulted_GetRef();
		Pass.Name = Name;
		Pass.Kind = EDreamPassKind::Clear;
		Pass.Injection = Injection;
		Pass.Writes.Add(MakeBufferBinding(Target, Target));
		Pass.Clear.Value = Value;
		return Pass;
	}

	/** Neither has a copy; both bindings are named after their buffers. */
	static FDreamPassDesc& AddCopyPass(UDreamPassPipeline& Pipeline, FName Name, EDreamPassInjection Injection, FName Source, FName Target, bool bSourcePrevious = false)
	{
		FDreamPassDesc& Pass = Pipeline.Passes.AddDefaulted_GetRef();
		Pass.Name = Name;
		Pass.Kind = EDreamPassKind::Copy;
		Pass.Injection = Injection;
		Pass.Reads.Add(MakeBufferBinding(Source, Source, bSourcePrevious));
		Pass.Writes.Add(MakeBufferBinding(Target, Target));
		return Pass;
	}

	/**
	 * A mesh pass drawing what Selection selects, its Output0 into Target: Override with OverrideMaterial, Own without. The
	 * write slots of a mesh pass are the output node's pins, Output<i> being SV_Target<i> (Render/DreamPassMesh.cpp, ParseWrites).
	 */
	static FDreamPassDesc& AddMeshPass(UDreamPassPipeline& Pipeline, FName Name, EDreamPassInjection Injection, const FDreamPassFilterTerm& Selection, FName Target, UMaterialInterface* OverrideMaterial)
	{
		FDreamPassDesc& Pass = Pipeline.Passes.AddDefaulted_GetRef();
		Pass.Name = Name;
		Pass.Kind = EDreamPassKind::Mesh;
		Pass.Injection = Injection;
		Pass.Writes.Add(MakeBufferBinding(TEXT("Output0"), Target));
		Pass.Mesh.Filter.AnyOf.AddDefaulted_GetRef().AllOf.Add(Selection);
		Pass.Mesh.OverrideMaterial = OverrideMaterial;
		Pass.Mesh.Mode = OverrideMaterial ? EDreamPassMeshMode::Override : EDreamPassMeshMode::Own;
		return Pass;
	}

	/** A fullscreen material pass into Target. A material pass's one write has no slot: the material's output is it. */
	static FDreamPassDesc& AddFullscreenPass(UDreamPassPipeline& Pipeline, FName Name, EDreamPassInjection Injection, UMaterialInterface* Material, FName Target)
	{
		FDreamPassDesc& Pass = Pipeline.Passes.AddDefaulted_GetRef();
		Pass.Name = Name;
		Pass.Kind = EDreamPassKind::Fullscreen;
		Pass.Injection = Injection;
		Pass.Fullscreen.Material = Material;
		Pass.Writes.Add(MakeBufferBinding(NAME_None, Target));
		return Pass;
	}

	static FDreamPassFilterTerm MakeListTerm(FName List)
	{
		FDreamPassFilterTerm Term;
		Term.Kind = EDreamPassFilterKind::List;
		Term.ListName = List;
		return Term;
	}

	static FDreamPassFilterTerm MakeLayerTerm(FName Layer, uint32 LayerMask)
	{
		FDreamPassFilterTerm Term;
		Term.Kind = EDreamPassFilterKind::Layer;
		Term.LayerMask = int32(LayerMask);
		Term.LayerNames.Add(Layer);
		return Term;
	}

	static FDreamPassFilterTerm MakeStencilTerm(int32 Value)
	{
		FDreamPassFilterTerm Term;
		Term.Kind = EDreamPassFilterKind::Stencil;
		Term.StencilValue = Value;
		Term.StencilMask = 255;
		return Term;
	}

	static void AddBoolParameter(UDreamPassPipeline& Pipeline, FName Name, bool bDefault)
	{
		FDreamPassParameterDesc& Parameter = Pipeline.Parameters.AddDefaulted_GetRef();
		Parameter.Name = Name;
		Parameter.Default = FDreamPassParameterValue::MakeBool(bDefault);
	}

	/** One clear of SceneColor at EndOfView, after the tonemapper: the whole picture Color. Checked and ready to run. */
	static UDreamPassPipeline* MakeFillPipeline(const TCHAR* Name, const FLinearColor& Color, int32 Order = 0)
	{
		UDreamPassPipeline* Pipeline = MakeCapturePipeline(Name, Order);
		AddClearPass(*Pipeline, TEXT("Fill"), EDreamPassInjection::EndOfView, UE::DreamPass::BuiltinBuffers::SceneColor, Color);
		Pipeline->NotifyChanged();
		return Pipeline;
	}

	static FDreamPassHandle Activate(UDreamPassSubsystem& Subsystem, UDreamPassPipeline* Pipeline)
	{
		return Subsystem.AddPipeline(Pipeline, /*Priority*/ 0.0f, TArray<FDreamPassParameterOverride>());
	}

	/**
	 * Whether the test capture reaches an injection point. A deferred family that resolves the scene and post-processes
	 * calls every scene view extension hook and the post-opaque delegate, and the chain calls its after-pass subscriptions
	 * whether or not the pass itself is on (R/Private/PostProcess/PostProcessing.cpp:1292-1293, 1637, 1651) -- every point but
	 * PostProcess.TranslucencyAfterDOF. That one is called only when translucency was drawn after DOF into a texture of its
	 * own (PostProcessing.cpp:1027-1039), which needs separate translucency -- a capture's show flags turn it off
	 * (E/Private/Components/SceneCaptureComponent.cpp:187) -- and a translucent primitive in that pass
	 * (R/Private/TranslucentRendering.cpp:1022-1052). The test world has neither.
	 */
	static bool IsReachedByTestCapture(EDreamPassInjection Injection)
	{
		return Injection != EDreamPassInjection::PostProcessTranslucencyAfterDOF && Injection != EDreamPassInjection::Count;
	}

	/**
	 * Where a pass may write SceneColor (Docs/runtime/index.md, Built-in buffers): from AfterBasePass on, but not at
	 * PostProcess.TranslucencyAfterDOF, which carries the translucency instead.
	 */
	static bool IsSceneColorWritable(EDreamPassInjection Injection)
	{
		switch (Injection)
		{
		case EDreamPassInjection::BeginView:
		case EDreamPassInjection::BeforeBasePass:
		case EDreamPassInjection::PostProcessTranslucencyAfterDOF:
		case EDreamPassInjection::Count:
			return false;
		default:
			return true;
		}
	}

	/** One sub-test per injection point Filter keeps, named and parameterized by the point's `.dsp` spelling. */
	static void AddInjectionTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands, TFunctionRef<bool(EDreamPassInjection)> Filter)
	{
		for (int32 Index = 0; Index < int32(EDreamPassInjection::Count); ++Index)
		{
			const EDreamPassInjection Injection = EDreamPassInjection(Index);
			if (Filter(Injection))
			{
				const FString Name = UE::DreamPass::LexToString(Injection);
				OutBeautifiedNames.Add(Name);
				OutTestCommands.Add(Name);
			}
		}
	}

	// --- texels -----------------------------------------------------------------------------------------------------

	/** Texel (X, Y) of a capture read back row by row, Y = 0 at the top. */
	static FColor TexelAt(const TArray<FColor>& Texels, int32 X, int32 Y)
	{
		const int32 Index = Y * CaptureSize + X;
		return Texels.IsValidIndex(Index) ? Texels[Index] : FColor(0, 0, 0, 0);
	}

	static FString DescribeTexel(const FColor& Texel)
	{
		return FString::Printf(TEXT("(%d, %d, %d)"), Texel.R, Texel.G, Texel.B);
	}

	static FString DescribeColor(const FLinearColor& Color, ETexelMatch Match)
	{
		if (Match == ETexelMatch::Dark || Color.Equals(FLinearColor::Black))
		{
			return TEXT("black");
		}

		FString Name;
		if (Color.Equals(FLinearColor::Red))
		{
			Name = TEXT("red");
		}
		else if (Color.Equals(FLinearColor::Green))
		{
			Name = TEXT("green");
		}
		else if (Color.Equals(FLinearColor::Blue))
		{
			Name = TEXT("blue");
		}
		else
		{
			Name = Color.ToString();
		}
		return Match == ETexelMatch::Dominant ? FString::Printf(TEXT("mostly %s"), *Name) : Name;
	}

	static bool TexelMatches(const FColor& Texel, const FLinearColor& Written, ETexelMatch Match)
	{
		const int32 Actual[3] = { Texel.R, Texel.G, Texel.B };
		switch (Match)
		{
		case ETexelMatch::Exact:
		{
			// The target is PF_B8G8R8A8 with linear gamma and ReadPixels hands 8-bit texels back as they are
			// (E/Public/UnrealClient.h:97-113): a 0 or 1 written reads back as 0 or 255.
			const FColor Bytes = Written.ToFColor(/*bSRGB*/ false);
			const int32 Expected[3] = { Bytes.R, Bytes.G, Bytes.B };
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				if (FMath::Abs(Actual[Channel] - Expected[Channel]) > 2)
				{
					return false;
				}
			}
			return true;
		}
		case ETexelMatch::Dominant:
		{
			// The tonemapper's curve and gamut handling move every channel of a pure colour, but not which one leads.
			const float Lit[3] = { Written.R, Written.G, Written.B };
			int32 Channel = 0;
			for (int32 Other = 1; Other < 3; ++Other)
			{
				if (Lit[Other] > Lit[Channel])
				{
					Channel = Other;
				}
			}
			const int32 Value = Actual[Channel];
			return Value >= 64 && Value >= Actual[(Channel + 1) % 3] + 40 && Value >= Actual[(Channel + 2) % 3] + 40;
		}
		default:
			return Actual[0] <= 8 && Actual[1] <= 8 && Actual[2] <= 8;
		}
	}

	static bool ExpectTexel(FAutomationTestBase& Test, const FString& What, const TArray<FColor>& Texels, const FIntPoint& Texel, const FLinearColor& Written, ETexelMatch Match)
	{
		const FColor Actual = TexelAt(Texels, Texel.X, Texel.Y);
		return Test.TestTrue(
			FString::Printf(TEXT("%s: texel (%d, %d) is %s (it reads %s)"), *What, Texel.X, Texel.Y, *DescribeColor(Written, Match), *DescribeTexel(Actual)),
			TexelMatches(Actual, Written, Match));
	}

	/** The whole view, as these tests check it: its centre and a texel near each corner. */
	static bool ExpectWholeView(FAutomationTestBase& Test, const FString& What, const TArray<FColor>& Texels, const FLinearColor& Written, ETexelMatch Match)
	{
		const int32 Far = CaptureSize - 3;
		const FIntPoint Samples[] = { CenterTexel, FIntPoint(2, 2), FIntPoint(Far, 2), FIntPoint(2, Far), FIntPoint(Far, Far) };
		bool bAll = true;
		for (const FIntPoint& Sample : Samples)
		{
			if (!ExpectTexel(Test, What, Texels, Sample, Written, Match))
			{
				bAll = false;
			}
		}
		return bAll;
	}

	/** Every texel where Where holds matches; the first that does not is named. */
	static bool ExpectEveryTexel(FAutomationTestBase& Test, const FString& What, const TArray<FColor>& Texels, TFunctionRef<bool(int32 X, int32 Y)> Where, const FLinearColor& Written, ETexelMatch Match)
	{
		int32 Misses = 0;
		FIntPoint FirstMiss(INDEX_NONE, INDEX_NONE);
		for (int32 Y = 0; Y < CaptureSize; ++Y)
		{
			for (int32 X = 0; X < CaptureSize; ++X)
			{
				if (Where(X, Y) && !TexelMatches(TexelAt(Texels, X, Y), Written, Match))
				{
					if (Misses++ == 0)
					{
						FirstMiss = FIntPoint(X, Y);
					}
				}
			}
		}
		if (!Test.TestEqual(FString::Printf(TEXT("%s: texels that are not %s"), *What, *DescribeColor(Written, Match)), Misses, 0))
		{
			Test.AddInfo(FString::Printf(TEXT("%s: the first is (%d, %d), which reads %s."), *What, FirstMiss.X, FirstMiss.Y, *DescribeTexel(TexelAt(Texels, FirstMiss.X, FirstMiss.Y))));
			return false;
		}
		return true;
	}

	// --- console variables ------------------------------------------------------------------------------------------

	static IConsoleVariable* FindTestVariable(FAutomationTestBase& Test, const TCHAR* Name)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		Test.TestNotNull(FString::Printf(TEXT("the console variable %s exists"), Name), Variable);
		return Variable;
	}

	/**
	 * Sets a console variable as typing it at the console does -- the priority every set and every restore here uses, so
	 * none is refused for being below the one before it, and a value a session typed in does not win over the test -- and
	 * says whether it took.
	 */
	static bool SetTestVariable(FAutomationTestBase& Test, IConsoleVariable& Variable, const TCHAR* Name, const FString& Value)
	{
		Variable.Set(*Value, ECVF_SetByConsole);
		return Test.TestEqual(FString::Printf(TEXT("%s takes '%s'"), Name, *Value), Variable.GetString(), Value);
	}

	// --- materials --------------------------------------------------------------------------------------------------

	/** A transient material, never saved. */
	static UMaterial* NewTestMaterial(const TCHAR* Name)
	{
		UPackage* Package = GetTransientPackage();
		return NewObject<UMaterial>(Package, MakeUniqueObjectName(Package, UMaterial::StaticClass(), Name), RF_Transient);
	}

	/**
	 * Compiles Material for this editor's shader platform and waits. PostEditChange caches the shader map without submitting
	 * its jobs (EMaterialShaderPrecompileMode::None, E/Private/Materials/Material.cpp:5418) -- they would wait for the first draw
	 * that asks -- so EnsureIsComplete submits them and blocks until they are done (E/Private/Materials/MaterialInterface.cpp:
	 * 2591-2619). The flush gets the finished map to the rendering thread, whose copy the passes check.
	 */
	static bool CompileTestMaterial(FAutomationTestBase& Test, UMaterial& Material)
	{
		Material.PreEditChange(nullptr);
		Material.PostEditChange();
		Material.EnsureIsComplete();
		FlushRenderingCommands();

		const FMaterialResource* Resource = Material.GetMaterialResource(GMaxRHIShaderPlatform);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s has a resource for this shader platform"), *Material.GetName()), Resource))
		{
			return false;
		}
		if (Resource->GetCompileErrors().Num() > 0)
		{
			Test.AddError(FString::Printf(TEXT("%s does not compile: %s"), *Material.GetName(), *FString::Join(Resource->GetCompileErrors(), TEXT(" | "))));
			return false;
		}
		return Test.TestTrue(FString::Printf(TEXT("%s has every shader for this shader platform"), *Material.GetName()), Resource->IsGameThreadShaderMapComplete());
	}

	/** Roots a compiled test material for the rest of the session; see GetMeshOutputMaterial for why. */
	static UMaterial* KeepTestMaterial(TWeakObjectPtr<UMaterial>& Cache, UMaterial* Material)
	{
		Material->AddToRoot();
		Cache = Material;
		return Material;
	}

	/**
	 * Surface, Unlit, with UE.DreamPassOutput whose Output0 is the vector parameter Color, red by default: what a mesh pass
	 * draws with, as its override material or -- through an instance that sets Color -- as a primitive's own.
	 *
	 * Built and compiled once for the session and kept: every new material has a new state id, so one built afresh in each
	 * case would compile its shaders afresh (this machine is slow at that). Its instances share its shader map.
	 */
	static UMaterial* GetMeshOutputMaterial(FAutomationTestBase& Test)
	{
		static TWeakObjectPtr<UMaterial> Cache;
		if (UMaterial* Kept = Cache.Get())
		{
			return Kept;
		}

		UMaterial* Material = NewTestMaterial(TEXT("M_DreamPassTestMeshOutput"));
		Material->MaterialDomain = MD_Surface;
		Material->SetShadingModel(MSM_Unlit);

		UMaterialExpressionVectorParameter* Color = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass()));
		UMaterialExpression* Output = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionDreamPassOutput::StaticClass());
		if (!Test.TestNotNull(TEXT("the mesh output material gets a vector parameter"), Color)
			|| !Test.TestNotNull(TEXT("the mesh output material gets a Dream Pass Output node"), Output))
		{
			return nullptr;
		}
		Color->ParameterName = MeshColorParameter;
		Color->DefaultValue = FLinearColor::Red;

		// The node's pins are found by name, Output0..Output3 (MaterialExpressionDreamPassOutput.h); the parameter's first output is its RGB.
		if (!Test.TestTrue(TEXT("Color connects to the output node's Output0"), UMaterialEditingLibrary::ConnectMaterialExpressions(Color, FString(), Output, TEXT("Output0")))
			|| !CompileTestMaterial(Test, *Material))
		{
			return nullptr;
		}
		return KeepTestMaterial(Cache, Material);
	}

	/**
	 * A Post Process material that outputs constant green. A data target, as a fullscreen pass into a buffer is: no pre-exposure
	 * scale (UMaterial::bDisablePreExposureScale), so the buffer receives the value as written. Kept like the mesh output material.
	 */
	static UMaterial* GetFillPostProcessMaterial(FAutomationTestBase& Test)
	{
		static TWeakObjectPtr<UMaterial> Cache;
		if (UMaterial* Kept = Cache.Get())
		{
			return Kept;
		}

		UMaterial* Material = NewTestMaterial(TEXT("PP_DreamPassTestFill"));
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorAfterDOF;
		Material->bDisablePreExposureScale = true;

		UMaterialExpressionConstant3Vector* Constant = Cast<UMaterialExpressionConstant3Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant3Vector::StaticClass()));
		if (!Test.TestNotNull(TEXT("the fill material gets a constant"), Constant))
		{
			return nullptr;
		}
		Constant->Constant = FLinearColor::Green;

		if (!Test.TestTrue(TEXT("the constant connects to Emissive Color"), UMaterialEditingLibrary::ConnectMaterialProperty(Constant, FString(), MP_EmissiveColor))
			|| !CompileTestMaterial(Test, *Material))
		{
			return nullptr;
		}
		return KeepTestMaterial(Cache, Material);
	}

	/** The UserSceneTexture the read material samples; a fullscreen pass binds a buffer to it with `read Mark = <buffer>`. */
	static const TCHAR* const ReadTextureName = TEXT("Mark");

	/** A Post Process material that outputs the UserSceneTexture Mark, point-sampled at the viewport UV. Kept like the others. */
	static UMaterial* GetReadPostProcessMaterial(FAutomationTestBase& Test)
	{
		static TWeakObjectPtr<UMaterial> Cache;
		if (UMaterial* Kept = Cache.Get())
		{
			return Kept;
		}

		UMaterial* Material = NewTestMaterial(TEXT("PP_DreamPassTestRead"));
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorAfterDOF;
		Material->bDisablePreExposureScale = true;

		UMaterialExpressionUserSceneTexture* Read = Cast<UMaterialExpressionUserSceneTexture>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionUserSceneTexture::StaticClass()));
		if (!Test.TestNotNull(TEXT("the read material gets a UserSceneTexture node"), Read))
		{
			return nullptr;
		}
		Read->UserSceneTexture = ReadTextureName;

		if (!Test.TestTrue(TEXT("the UserSceneTexture's Color connects to Emissive Color"), UMaterialEditingLibrary::ConnectMaterialProperty(Read, TEXT("Color"), MP_EmissiveColor))
			|| !CompileTestMaterial(Test, *Material))
		{
			return nullptr;
		}
		return KeepTestMaterial(Cache, Material);
	}

	// --- the fixture ------------------------------------------------------------------------------------------------

	/**
	 * A game world with its Custom Pass subsystem and one scene capture, built for one case and torn down after it.
	 *
	 * - A Game world (as DreamShader.Pass.Logic's): it gets the subsystem, whose scene view extension is a
	 *   FWorldSceneViewExtension active for this world's views only (E/Private/SceneViewExtension.cpp:50-53, the context
	 *   being the capture's scene, R/Private/SceneCaptureRendering.cpp:915-916), and an FScene. No viewport shows it.
	 * - The capture renders SCS_FinalColorLDR: its family resolves the scene and keeps post processing on
	 *   (SceneCaptureRendering.cpp:908-913, 746-750), so every injection point but TranslucencyAfterDOF exists in it. Its view
	 *   is a scene capture's (bIsSceneCapture, :735), which the runtime classifies as SceneCapture. The extensions'
	 *   BeginRenderViewFamily runs as the renderer is created (R/Private/SceneRenderBuilder.cpp:509-512), and each capture
	 *   is one renderer with one render graph of its own (:873).
	 * - Its target is PF_B8G8R8A8 with linear gamma: an LDR capture's usual format, read back as the bytes the last pass
	 *   wrote, so a pure 0/1 colour compares exactly and nothing converts gamma on the way. The capture clears it to black
	 *   before it renders (SceneCaptureRendering.cpp:472-475).
	 * - Exposure is fixed at 1 and what in the chain would move a flat colour is off (E/Private/SceneView.cpp:2178-2258;
	 *   eye adaptation off is manual exposure locked to 1, R/Private/PostProcess/PostProcessEyeAdaptation.cpp:437-441,
	 *   671-682): a colour written before the tonemapper comes out as the tonemapper alone makes it. The tonemapper stays --
	 *   the PostProcess.* points are around it.
	 * - Without a view state unless a case asks for one (bAlwaysPersistRenderingState, E/Private/Components/
	 *   SceneCaptureComponent.cpp:407-433): a view state gives the view a key, and history.
	 * - Custom Pass is on, the project's global pipelines and pass layers are set aside, and r.DreamPass.Enable,
	 *   DisablePipelines and Visualize are at their defaults for the case, so only what the case activates runs, as it
	 *   activates it. All of it comes back in Teardown.
	 */
	class FPassCaptureFixture
	{
	public:
		explicit FPassCaptureFixture(FAutomationTestBase& InTest)
			: Test(InTest)
		{
			IsolateProject();
			CreateWorldAndCapture();
		}

		~FPassCaptureFixture()
		{
			Teardown(/*bFlushFirst*/ true);
		}

		FPassCaptureFixture(const FPassCaptureFixture&) = delete;
		FPassCaptureFixture& operator=(const FPassCaptureFixture&) = delete;

		bool IsReady() const { return World && Subsystem && Capture && Target.IsValid(); }

		UWorld* GetWorld() const { return World; }
		UDreamPassSubsystem& GetSubsystem() const { return *Subsystem; }
		USceneCaptureComponent2D* GetCapture() const { return Capture; }

		/**
		 * Renders one frame of the capture and reads it back. CaptureScene builds the family and executes it at once
		 * (E/Private/Components/SceneCaptureComponent.cpp:823-837); the flush waits for its render commands, the read for the GPU.
		 */
		bool CaptureFrame(TArray<FColor>& OutTexels)
		{
			OutTexels.Reset();
			if (!IsReady())
			{
				Test.AddError(TEXT("The capture fixture is not ready; nothing was rendered."));
				return false;
			}

			Capture->CaptureScene();
			FlushRenderingCommands();

			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!Test.TestTrue(TEXT("the capture's target reads back"), Resource != nullptr && Resource->ReadPixels(OutTexels)))
			{
				return false;
			}
			return Test.TestEqual(TEXT("the capture reads back one colour per texel"), OutTexels.Num(), CaptureSize * CaptureSize);
		}

		/** Renders a frame and checks the whole view: its centre and a texel near each corner. */
		bool ExpectFrame(const FString& What, const FLinearColor& Written, ETexelMatch Match)
		{
			TArray<FColor> Texels;
			return CaptureFrame(Texels) && ExpectWholeView(Test, What, Texels, Written, Match);
		}

		/** Queues a frame of the capture and does not wait for it. */
		void QueueFrame()
		{
			if (IsReady())
			{
				Capture->CaptureScene();
			}
		}

		/**
		 * The engine's 100 cm cube on an actor of its own at Location, scaled, registered. Nanite is disallowed on it: a
		 * Nanite primitive has no mesh batch a mesh pass can draw again, and the engine cube may have Nanite on.
		 */
		UStaticMeshComponent* AddCube(const FVector& Location, const FVector& Scale = FVector::OneVector)
		{
			if (!IsReady())
			{
				return nullptr;
			}

			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (!Test.TestNotNull(TEXT("the engine cube /Engine/BasicShapes/Cube loads"), CubeMesh))
			{
				return nullptr;
			}
			{
				// Its render data, built or fetched asynchronously on first load in the editor.
				TArray<UObject*> Compiling;
				Compiling.Add(CubeMesh);
				FAssetCompilingManager::Get().FinishCompilationForObjects(Compiling);
			}

			AActor* Owner = World->SpawnActor<AActor>();
			if (!Test.TestNotNull(TEXT("a cube's actor spawns in the test world"), Owner))
			{
				return nullptr;
			}
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
			Mesh->bDisallowNanite = true;
			Mesh->SetStaticMesh(CubeMesh);
			Mesh->SetRelativeLocation(Location);
			Mesh->SetRelativeScale3D(Scale);
			Owner->SetRootComponent(Mesh);
			Mesh->RegisterComponent();
			return Mesh;
		}

		/**
		 * Destroys the world and puts the project back. bFlushFirst false leaves what the last frame queued in flight, as a world
		 * unloaded in the middle of a frame does; it runs at the flush after the world is gone.
		 */
		void Teardown(bool bFlushFirst)
		{
			if (World)
			{
				if (bFlushFirst)
				{
					FlushRenderingCommands();
				}
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(/*bInformEngineOfWorld*/ false);
				World = nullptr;
				Subsystem = nullptr;
				Capture = nullptr;
				FlushRenderingCommands();
			}
			Target.Reset();

			for (const FSavedVariable& Saved : SavedVariables)
			{
				Saved.Variable->Set(*Saved.Value, ECVF_SetByConsole);
			}
			SavedVariables.Reset();

			if (bSettingsSaved)
			{
				UDreamPassSettings* Settings = GetMutableDefault<UDreamPassSettings>();
				Settings->bEnabled = bSavedEnabled;
				Settings->GlobalPipelines = SavedGlobalPipelines;
				Settings->LayerNames = SavedLayerNames;
				bSettingsSaved = false;
			}
		}

	private:
		struct FSavedVariable
		{
			IConsoleVariable* Variable = nullptr;
			FString Value;
		};

		void IsolateProject()
		{
			// The world's subsystem loads the global pipelines as it initializes, so they go before the world exists.
			UDreamPassSettings* Settings = GetMutableDefault<UDreamPassSettings>();
			bSavedEnabled = Settings->bEnabled;
			SavedGlobalPipelines = Settings->GlobalPipelines;
			SavedLayerNames = Settings->LayerNames;
			bSettingsSaved = true;
			Settings->bEnabled = true;
			Settings->GlobalPipelines.Reset();

			struct FDefault
			{
				const TCHAR* Name;
				const TCHAR* Value;
			};
			const FDefault Defaults[] = {
				{ TEXT("r.DreamPass.Enable"), TEXT("1") },
				{ TEXT("r.DreamPass.DisablePipelines"), TEXT("") },
				{ TEXT("r.DreamPass.Visualize"), TEXT("") },
			};
			for (const FDefault& Default : Defaults)
			{
				if (IConsoleVariable* Variable = FindTestVariable(Test, Default.Name))
				{
					SavedVariables.Add({ Variable, Variable->GetString() });
					Variable->Set(Default.Value, ECVF_SetByConsole);
				}
			}
		}

		void CreateWorldAndCapture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);

			Subsystem = UDreamPassSubsystem::Get(World);
			if (!Test.TestNotNull(TEXT("the test world has the Custom Pass subsystem"), Subsystem)
				|| !Test.TestNotNull(TEXT("the test world has a scene to render"), World->Scene))
			{
				return;
			}

			UTextureRenderTarget2D* NewTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
			NewTarget->ClearColor = FLinearColor::Black;
			NewTarget->InitCustomFormat(CaptureSize, CaptureSize, PF_B8G8R8A8, /*bInForceLinearGamma*/ true);
			NewTarget->UpdateResourceImmediate(/*bClearRenderTarget*/ true);
			Target.Reset(NewTarget);

			AActor* Owner = World->SpawnActor<AActor>();
			if (!Test.TestNotNull(TEXT("the capture's actor spawns in the test world"), Owner))
			{
				return;
			}

			// Everything that decides how the capture renders is set before it registers: on movement it would queue a
			// deferred capture of its own, every frame it would tick one.
			USceneCaptureComponent2D* NewCapture = NewObject<USceneCaptureComponent2D>(Owner, TEXT("DreamPassTestCapture"));
			NewCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			NewCapture->TextureTarget = NewTarget;
			NewCapture->bCaptureEveryFrame = false;
			NewCapture->bCaptureOnMovement = false;
			NewCapture->bAlwaysPersistRenderingState = false;

			// The show flags go in as settings, not straight into ShowFlags: every registration reads the flags back from the
			// class defaults and applies the settings on top (E/Private/Components/SceneCaptureComponent.cpp:292, 435-451), and
			// a change of r.CustomDepth registers every component again (E/Private/SceneUtils.cpp:25-31). Each flag off also
			// clears its settings from the view's final post-process settings.
			const TCHAR* const FlagsOff[] = {
				TEXT("EyeAdaptation"), TEXT("LocalExposure"), TEXT("Bloom"), TEXT("LensFlares"), TEXT("Vignette"), TEXT("Grain"),
				TEXT("SceneColorFringe"), TEXT("DepthOfField"), TEXT("MotionBlur"), TEXT("AntiAliasing"), TEXT("TemporalAA"),
			};
			TArray<FEngineShowFlagsSetting> FlagSettings;
			for (const TCHAR* Flag : FlagsOff)
			{
				FEngineShowFlagsSetting& Setting = FlagSettings.AddDefaulted_GetRef();
				Setting.ShowFlagName = Flag;
				Setting.Enabled = false;
			}
			NewCapture->SetShowFlagSettings(FlagSettings);

			Owner->SetRootComponent(NewCapture);
			NewCapture->RegisterComponent();

			// A flag name the engine does not know is ignored without a word (UpdateShowFlags); this says so.
			for (const TCHAR* Flag : FlagsOff)
			{
				const int32 Index = FEngineShowFlags::FindIndexByName(Flag);
				Test.TestTrue(FString::Printf(TEXT("the capture's show flag %s is off"), Flag), Index != INDEX_NONE && !NewCapture->ShowFlags.GetSingleFlag(uint32(Index)));
			}

			Capture = NewCapture;
		}

		FAutomationTestBase& Test;

		UWorld* World = nullptr;
		UDreamPassSubsystem* Subsystem = nullptr;
		USceneCaptureComponent2D* Capture = nullptr;
		TStrongObjectPtr<UTextureRenderTarget2D> Target;

		bool bSettingsSaved = false;
		bool bSavedEnabled = true;
		TArray<FDreamPassGlobalPipeline> SavedGlobalPipelines;
		TArray<FName> SavedLayerNames;
		TArray<FSavedVariable> SavedVariables;
	};
}

// ---------------------------------------------------------------------------------------------------------------- EndOfView

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderEndOfViewClearTest,
	"DreamShader.Pass.Render.EndOfViewClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderEndOfViewClearTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	// The harness first: with nothing activated the capture is the empty world, black.
	Fixture.ExpectFrame(TEXT("no pipeline"), FLinearColor::Black, ETexelMatch::Dark);

	UDreamPassPipeline* Pipeline = MakeFillPipeline(TEXT("CP_RenderEndOfView"), FLinearColor::Red);
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	// EndOfView writes the view family texture -- the capture's own target, after the tonemapper -- so every texel holds the
	// clear value as written.
	TArray<FColor> Texels;
	if (!Fixture.CaptureFrame(Texels))
	{
		return false;
	}
	ExpectEveryTexel(*this, TEXT("SceneColor cleared to red at EndOfView"), Texels, [](int32, int32) { return true; }, FLinearColor::Red, ETexelMatch::Exact);
	return true;
}

// ------------------------------------------------------------------------------------------------------- Injection points

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderPassRenderInjectionPointsTest,
	"DreamShader.Pass.Render.InjectionPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

void FDreamShaderPassRenderInjectionPointsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::PassRenderTests::AddInjectionTests(OutBeautifiedNames, OutTestCommands, [](EDreamPassInjection) { return true; });
}

bool FDreamShaderPassRenderInjectionPointsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	EDreamPassInjection Injection = EDreamPassInjection::Count;
	if (!UE::DreamPass::LexTryParse(Parameters, Injection))
	{
		AddError(FString::Printf(TEXT("'%s' is not an injection point."), *Parameters));
		return false;
	}

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	// The G1 case: one pass writes at the point and another reads there. Write clears Mark to red; Read, at the same point,
	// copies Mark into Relay; Show, at EndOfView, copies Relay into the scene colour. Each buffer's clear value tells a miss
	// apart: blue, nothing ran at the point; green, the read did and the write did not.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderPoint"));
	AddColorBuffer(*Pipeline, TEXT("Mark"), FLinearColor::Green);
	AddColorBuffer(*Pipeline, TEXT("Relay"), FLinearColor::Blue);
	AddClearPass(*Pipeline, TEXT("Write"), Injection, TEXT("Mark"), FLinearColor::Red);
	AddCopyPass(*Pipeline, TEXT("Read"), Injection, TEXT("Mark"), TEXT("Relay"));
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Relay"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	if (IsReachedByTestCapture(Injection))
	{
		Fixture.ExpectFrame(FString::Printf(TEXT("written and read at %s, shown at EndOfView"), *Parameters), FLinearColor::Red, ETexelMatch::Exact);
	}
	else
	{
		// A point the view does not have is not reached: Write and Read are skipped, and Show, the first to use Relay, finds
		// it as created -- its clear value (Docs/runtime/index.md, When passes run).
		Fixture.ExpectFrame(FString::Printf(TEXT("%s is not reached in a scene capture; Relay keeps its clear value"), *Parameters), FLinearColor::Blue, ETexelMatch::Exact);
	}
	return true;
}

// --------------------------------------------------------------------------------------------------------- SceneColor write

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderPassRenderSceneColorWriteTest,
	"DreamShader.Pass.Render.SceneColorWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

void FDreamShaderPassRenderSceneColorWriteTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	AddInjectionTests(OutBeautifiedNames, OutTestCommands, [](EDreamPassInjection Injection) { return IsSceneColorWritable(Injection); });
}

bool FDreamShaderPassRenderSceneColorWriteTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	EDreamPassInjection Injection = EDreamPassInjection::Count;
	if (!UE::DreamPass::LexTryParse(Parameters, Injection) || !IsSceneColorWritable(Injection))
	{
		AddError(FString::Printf(TEXT("'%s' is not an injection point where SceneColor can be written."), *Parameters));
		return false;
	}

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderSceneColor"));
	AddClearPass(*Pipeline, TEXT("Fill"), Injection, UE::DreamPass::BuiltinBuffers::SceneColor, FLinearColor::Green);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	// Green over an empty, black world: whatever the frame does after the point -- lighting, the tonemapper -- the view must
	// still be green where the clear reached, which is all of it.
	Fixture.ExpectFrame(FString::Printf(TEXT("SceneColor cleared to green at %s"), *Parameters), FLinearColor::Green, ETexelMatch::Dominant);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------ History

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderHistoryTest,
	"DreamShader.Pass.Render.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderHistoryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	// A blue backdrop under everything: a frame the history pipeline did not run in stays blue, so black below is a read.
	Activate(Subsystem, MakeFillPipeline(TEXT("CP_RenderBackdrop"), FLinearColor::Blue, BackdropOrder));

	// At EndOfView, ShowPrevious puts last frame's Trail on screen, then Write gives Trail this frame's value.
	auto MakeHistoryPipeline = [this](const TCHAR* Name) -> UDreamPassPipeline*
	{
		UDreamPassPipeline* Pipeline = MakeCapturePipeline(Name);
		AddColorBuffer(*Pipeline, TEXT("Trail"), FLinearColor::Transparent, /*bHistory*/ true);
		AddCopyPass(*Pipeline, TEXT("ShowPrevious"), EDreamPassInjection::EndOfView, TEXT("Trail"), UE::DreamPass::BuiltinBuffers::SceneColor, /*bSourcePrevious*/ true);
		AddClearPass(*Pipeline, TEXT("Write"), EDreamPassInjection::EndOfView, TEXT("Trail"), FLinearColor::Red);
		Pipeline->NotifyChanged();
		TestTrue(FString::Printf(TEXT("%s passes its own checks"), Name), Pipeline->Validate());
		return Pipeline;
	};

	// With a view state the view has a key, and Trail is kept per (view, pipeline, buffer) from one frame to the next.
	Fixture.GetCapture()->bAlwaysPersistRenderingState = true;
	UDreamPassPipeline* Kept = MakeHistoryPipeline(TEXT("CP_RenderHistory"));
	const int32 WriteIndex = Kept->FindPassIndex(TEXT("Write"));
	if (!TestTrue(TEXT("the history pipeline has its Write pass"), Kept->Passes.IsValidIndex(WriteIndex)))
	{
		return false;
	}
	const FDreamPassHandle KeptHandle = Activate(Subsystem, Kept);

	Fixture.ExpectFrame(TEXT("view state, frame 1: .Previous of a history that has none yet reads black"), FLinearColor::Black, ETexelMatch::Exact);

	// Frame 2 writes green, so red on screen can only be what frame 1 left.
	Kept->Passes[WriteIndex].Clear.Value = FLinearColor::Green;
	Kept->NotifyChanged();
	Fixture.ExpectFrame(TEXT("view state, frame 2: .Previous reads what frame 1 wrote (red), not what frame 2 writes"), FLinearColor::Red, ETexelMatch::Exact);
	Fixture.ExpectFrame(TEXT("view state, frame 3: .Previous reads what frame 2 wrote"), FLinearColor::Green, ETexelMatch::Exact);
	Subsystem.RemovePipeline(KeptHandle);

	// Without one the view has no key and keeps nothing: every frame's .Previous is black, said once
	// (Render/DreamPassBuffers.cpp, GetOrCreateBuffer). A pipeline of its own, so its name starts the warning's key afresh.
	Fixture.GetCapture()->bAlwaysPersistRenderingState = false;
	UDreamPassPipeline* Lost = MakeHistoryPipeline(TEXT("CP_RenderNoHistory"));
	AddExpectedMessagePlain(
		FString::Printf(TEXT("%s.Trail keeps history, but a view without a view state"), *Lost->GetName()),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Activate(Subsystem, Lost);

	Fixture.ExpectFrame(TEXT("no view state, frame 1: .Previous reads black"), FLinearColor::Black, ETexelMatch::Exact);
	Fixture.ExpectFrame(TEXT("no view state, frame 2: .Previous still reads black, not frame 1's red"), FLinearColor::Black, ETexelMatch::Exact);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------ Filters

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderFiltersTest,
	"DreamShader.Pass.Render.Filters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderFiltersTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	Activate(Subsystem, MakeFillPipeline(TEXT("CP_RenderBackdrop"), FLinearColor::Blue, BackdropOrder));

	// The pipeline under test clears green, then red over it, at EndOfView. Red: it ran whole. Green: it ran but its second
	// pass did not. Blue: the backdrop alone, it did not run at all.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderFiltered"));
	AddBoolParameter(*Pipeline, TEXT("On"), true);
	AddBoolParameter(*Pipeline, TEXT("PassOn"), true);
	Pipeline->EnabledParameter = TEXT("On");
	AddClearPass(*Pipeline, TEXT("Green"), EDreamPassInjection::EndOfView, UE::DreamPass::BuiltinBuffers::SceneColor, FLinearColor::Green);
	AddClearPass(*Pipeline, TEXT("Red"), EDreamPassInjection::EndOfView, UE::DreamPass::BuiltinBuffers::SceneColor, FLinearColor::Red).EnabledParameter = TEXT("PassOn");
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	const FDreamPassHandle Handle = Activate(Subsystem, Pipeline);

	if (!Fixture.ExpectFrame(TEXT("nothing filters it: it runs whole"), FLinearColor::Red, ETexelMatch::Exact))
	{
		return false;
	}

	// Views: a pipeline for game and editor views only is not a scene capture's.
	Pipeline->Views = int32(EDreamPassViewFlags::Game | EDreamPassViewFlags::Editor);
	Fixture.ExpectFrame(TEXT("Views without SceneCapture: it does not run in a capture"), FLinearColor::Blue, ETexelMatch::Exact);
	Pipeline->Views = int32(EDreamPassViewFlags::SceneCapture);
	Fixture.ExpectFrame(TEXT("SceneCapture back in its Views: it runs"), FLinearColor::Red, ETexelMatch::Exact);

	// Weight 0.
	TestTrue(TEXT("SetWeight takes the handle"), Subsystem.SetWeight(Handle, 0.0f));
	Fixture.ExpectFrame(TEXT("weight 0: it does not run"), FLinearColor::Blue, ETexelMatch::Exact);
	Subsystem.SetWeight(Handle, 1.0f);

	// The pipeline's EnabledParameter.
	TestTrue(TEXT("SetParameter takes On"), Subsystem.SetParameter(Handle, TEXT("On"), FDreamPassParameterValue::MakeBool(false)));
	Fixture.ExpectFrame(TEXT("its EnabledParameter false: the pipeline does not run"), FLinearColor::Blue, ETexelMatch::Exact);
	Subsystem.SetParameter(Handle, TEXT("On"), FDreamPassParameterValue::MakeBool(true));

	// A pass's own EnabledParameter: the pipeline runs without that pass.
	TestTrue(TEXT("SetParameter takes PassOn"), Subsystem.SetParameter(Handle, TEXT("PassOn"), FDreamPassParameterValue::MakeBool(false)));
	Fixture.ExpectFrame(TEXT("a pass's EnabledParameter false: the rest of the pipeline runs without it"), FLinearColor::Green, ETexelMatch::Exact);
	Subsystem.SetParameter(Handle, TEXT("PassOn"), FDreamPassParameterValue::MakeBool(true));
	Fixture.ExpectFrame(TEXT("every parameter back on: it runs whole"), FLinearColor::Red, ETexelMatch::Exact);

	// The console. Both variables go back to what they were whichever way the test leaves.
	IConsoleVariable* Enable = FindTestVariable(*this, TEXT("r.DreamPass.Enable"));
	IConsoleVariable* Disable = FindTestVariable(*this, TEXT("r.DreamPass.DisablePipelines"));
	if (!Enable || !Disable)
	{
		return false;
	}
	const FString SavedEnable = Enable->GetString();
	const FString SavedDisable = Disable->GetString();
	ON_SCOPE_EXIT
	{
		Enable->Set(*SavedEnable, ECVF_SetByConsole);
		Disable->Set(*SavedDisable, ECVF_SetByConsole);
	};

	// r.DreamPass.Enable 0 turns the extension off for every view: nothing runs, the backdrop neither.
	if (SetTestVariable(*this, *Enable, TEXT("r.DreamPass.Enable"), TEXT("0")))
	{
		Fixture.ExpectFrame(TEXT("r.DreamPass.Enable 0: nothing runs"), FLinearColor::Black, ETexelMatch::Dark);
	}
	SetTestVariable(*this, *Enable, TEXT("r.DreamPass.Enable"), TEXT("1"));

	// Another name first and the pipeline's in lower case: the list is comma-separated, trimmed, and not case-sensitive.
	if (SetTestVariable(*this, *Disable, TEXT("r.DreamPass.DisablePipelines"), FString::Printf(TEXT("CP_NoSuchPipeline, %s"), *Pipeline->GetName().ToLower())))
	{
		Fixture.ExpectFrame(TEXT("r.DreamPass.DisablePipelines names it: it does not run, the backdrop does"), FLinearColor::Blue, ETexelMatch::Exact);
	}
	SetTestVariable(*this, *Disable, TEXT("r.DreamPass.DisablePipelines"), FString());

	Fixture.ExpectFrame(TEXT("the console lifted: it runs whole again"), FLinearColor::Red, ETexelMatch::Exact);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------ Sources

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderSourcesTest,
	"DreamShader.Pass.Render.Sources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderSourcesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();
	UWorld* World = Fixture.GetWorld();

	// A blue backdrop through the API, under the red pipeline the sources run: red means a source ran it, blue that none did.
	Activate(Subsystem, MakeFillPipeline(TEXT("CP_RenderBackdrop"), FLinearColor::Blue, BackdropOrder));
	UDreamPassPipeline* Pipeline = MakeFillPipeline(TEXT("CP_RenderSourced"), FLinearColor::Red);
	Fixture.ExpectFrame(TEXT("nothing activates the pipeline yet"), FLinearColor::Blue, ETexelMatch::Exact);

	// An unbound volume applies to every view of its world, the capture's included.
	ADreamPassVolume* Volume = World->SpawnActor<ADreamPassVolume>();
	if (!TestNotNull(TEXT("a volume spawns in the test world"), Volume))
	{
		return false;
	}
	Volume->Pipeline = Pipeline;
	Volume->bUnbound = true;
	Fixture.ExpectFrame(TEXT("an unbound volume runs its pipeline in the capture"), FLinearColor::Red, ETexelMatch::Exact);

	// Bounded, a volume spawned at runtime has no brush, and so no inside for the capture to be in.
	Volume->bUnbound = false;
	Fixture.ExpectFrame(TEXT("a bounded volume without a brush runs it nowhere"), FLinearColor::Blue, ETexelMatch::Exact);
	Volume->bUnbound = true;
	Fixture.ExpectFrame(TEXT("unbound again: it runs"), FLinearColor::Red, ETexelMatch::Exact);

	Volume->Destroy();
	Fixture.ExpectFrame(TEXT("the volume destroyed: it no longer runs"), FLinearColor::Blue, ETexelMatch::Exact);

	// A component with Scope = World applies to every view of its owner's world.
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("an actor spawns in the test world"), Owner))
	{
		return false;
	}
	UDreamPassComponent* Component = NewObject<UDreamPassComponent>(Owner);
	Component->Pipeline = Pipeline;
	Component->Scope = EDreamPassComponentScope::World;
	Component->RegisterComponent();
	Fixture.ExpectFrame(TEXT("a component with Scope = World runs its pipeline in the capture"), FLinearColor::Red, ETexelMatch::Exact);

	// ViewTarget follows a player's view target. A scene capture has none (USceneCaptureComponent::GetViewOwner is null,
	// E/Classes/Components/SceneCaptureComponent.h:273), so it never runs one.
	Component->Scope = EDreamPassComponentScope::ViewTarget;
	Fixture.ExpectFrame(TEXT("Scope = ViewTarget: not in a scene capture, which has no view target"), FLinearColor::Blue, ETexelMatch::Exact);
	Component->Scope = EDreamPassComponentScope::World;

	Component->DestroyComponent();
	Fixture.ExpectFrame(TEXT("the component destroyed: it no longer runs"), FLinearColor::Blue, ETexelMatch::Exact);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------- Visualize

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderVisualizeTest,
	"DreamShader.Pass.Render.Visualize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderVisualizeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	IConsoleVariable* Visualize = FindTestVariable(*this, TEXT("r.DreamPass.Visualize"));
	if (!Visualize)
	{
		return false;
	}
	const FString SavedVisualize = Visualize->GetString();
	ON_SCOPE_EXIT
	{
		Visualize->Set(*SavedVisualize, ECVF_SetByConsole);
	};

	// A buffer the pipeline fills and no pass shows: nothing writes the scene colour, so only the visualizer can put Shown in
	// the picture.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderVisualize"));
	AddColorBuffer(*Pipeline, TEXT("Shown"), FLinearColor::Transparent);
	AddClearPass(*Pipeline, TEXT("Fill"), EDreamPassInjection::BeginView, TEXT("Shown"), FLinearColor::Green);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	// Where the visualizer draws (Render/DreamPassVisualize.cpp, GetCornerRect): the lower left, a quarter of the view wide, at
	// the buffer's aspect -- square here.
	const int32 CornerSize = CaptureSize / 4;
	const FIntRect Corner(0, CaptureSize - CornerSize, CornerSize, CaptureSize);
	const FIntPoint CornerTexel(CornerSize / 2, CaptureSize - CornerSize / 2);
	const FString Shown = FString::Printf(TEXT("%s.Shown"), *Pipeline->GetName());
	const TCHAR* const VariableName = TEXT("r.DreamPass.Visualize");

	TArray<FColor> Texels;
	SetTestVariable(*this, *Visualize, VariableName, FString());
	if (Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("off"), Texels, CornerTexel, FLinearColor::Black, ETexelMatch::Dark);
	}

	if (SetTestVariable(*this, *Visualize, VariableName, Shown) && Fixture.CaptureFrame(Texels))
	{
		// The corner shows the buffer as its last writer left it; everything more than a texel outside the corner is the
		// scene's black as it was. The texel on the corner's edge is left to the rasterizer.
		ExpectEveryTexel(*this, FString::Printf(TEXT("%s, inside the lower left corner"), *Shown), Texels,
			[&Corner](int32 X, int32 Y) { return X > Corner.Min.X && X < Corner.Max.X - 1 && Y > Corner.Min.Y && Y < Corner.Max.Y - 1; },
			FLinearColor::Green, ETexelMatch::Exact);
		ExpectEveryTexel(*this, FString::Printf(TEXT("%s, outside the corner"), *Shown), Texels,
			[&Corner](int32 X, int32 Y) { return X > Corner.Max.X || Y < Corner.Min.Y - 1; },
			FLinearColor::Black, ETexelMatch::Dark);
	}

	// The names are not case-sensitive: the pipeline is matched by its asset name, the buffer as an FName.
	if (SetTestVariable(*this, *Visualize, VariableName, FString::Printf(TEXT("%s.SHOWN"), *Pipeline->GetName().ToUpper())) && Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("named in upper case"), Texels, CornerTexel, FLinearColor::Green, ETexelMatch::Exact);
	}

	// A pipeline this view does not run draws nothing, and says nothing: another view may run it.
	if (SetTestVariable(*this, *Visualize, VariableName, TEXT("CP_NoSuchPipeline.Shown")) && Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("another pipeline named"), Texels, CornerTexel, FLinearColor::Black, ETexelMatch::Dark);
	}

	// A buffer the pipeline does not have draws nothing, and says so once.
	AddExpectedMessagePlain(FString::Printf(TEXT("%s has no buffer 'NoSuchBuffer'"), *Pipeline->GetName()), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	if (SetTestVariable(*this, *Visualize, VariableName, FString::Printf(TEXT("%s.NoSuchBuffer"), *Pipeline->GetName())) && Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("a buffer the pipeline does not have"), Texels, CornerTexel, FLinearColor::Black, ETexelMatch::Dark);
	}
	return true;
}

// --------------------------------------------------------------------------------------------------------------- Mesh passes

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderPassRenderMeshFilterTest,
	"DreamShader.Pass.Render.Mesh.Filter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

void FDreamShaderPassRenderMeshFilterTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Kind : { TEXT("List"), TEXT("Layer"), TEXT("Stencil") })
	{
		OutBeautifiedNames.Add(Kind);
		OutTestCommands.Add(Kind);
	}
}

bool FDreamShaderPassRenderMeshFilterTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UMaterial* OutputMaterial = GetMeshOutputMaterial(*this);
	if (!OutputMaterial)
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	// The cube the filter selects, in the middle of the view, and one it leaves out, above it.
	UStaticMeshComponent* Selected = Fixture.AddCube(CenterCubeLocation);
	UStaticMeshComponent* Unselected = Fixture.AddCube(UpperCubeLocation);
	if (!Selected || !Unselected)
	{
		return false;
	}

	// r.CustomDepth goes back to what it was whichever way the test leaves; only the stencil case changes it.
	IConsoleVariable* CustomDepth = nullptr;
	FString SavedCustomDepth;
	ON_SCOPE_EXIT
	{
		if (CustomDepth)
		{
			CustomDepth->Set(*SavedCustomDepth, ECVF_SetByConsole);
		}
	};

	FDreamPassFilterTerm Selection;
	if (Parameters == TEXT("List"))
	{
		const FName List(TEXT("DreamPassTestList"));
		Subsystem.AddToList(List, Selected);
		Selection = MakeListTerm(List);
	}
	else if (Parameters == TEXT("Layer"))
	{
		// The project's layer table holds only the test's layer for the case; the fixture puts the table back.
		const FName Layer(TEXT("DreamPassTestLayer"));
		UDreamPassSettings* Settings = GetMutableDefault<UDreamPassSettings>();
		Settings->LayerNames.Reset();
		Settings->LayerNames.Add(Layer);
		TArray<FName> TestLayers;
		TestLayers.Add(Layer);
		const uint32 LayerMask = Settings->MakeLayerMask(TestLayers);
		if (!TestEqual(TEXT("the test layer is bit 0"), LayerMask, 1u))
		{
			return false;
		}

		UDreamPassLayerComponent* Layers = NewObject<UDreamPassLayerComponent>(Selected->GetOwner());
		Layers->Layers.Add(Layer);
		Layers->RegisterComponent();
		TestEqual(TEXT("the layer component gives the selected cube the layer"), Subsystem.GetPrimitiveLayers(Selected), LayerMask);
		Selection = MakeLayerTerm(Layer, LayerMask);
	}
	else if (Parameters == TEXT("Stencil"))
	{
		// Selection reads each primitive's stencil value from its proxy, not the custom stencil texture, but r.CustomDepth 3 --
		// custom depth with stencil -- is what a project that selects by stencil runs with. Changing it registers every
		// component of every world again (E/Private/SceneUtils.cpp:25-31), the capture's included; its show flags are settings,
		// which a registration applies again.
		CustomDepth = FindTestVariable(*this, TEXT("r.CustomDepth"));
		if (!CustomDepth)
		{
			return false;
		}
		SavedCustomDepth = CustomDepth->GetString();
		SetTestVariable(*this, *CustomDepth, TEXT("r.CustomDepth"), TEXT("3"));

		// The cube left out renders custom depth too, with another value: the stencil, not the custom depth, selects.
		Selected->SetRenderCustomDepth(true);
		Selected->SetCustomDepthStencilValue(7);
		Unselected->SetRenderCustomDepth(true);
		Unselected->SetCustomDepthStencilValue(3);
		Selection = MakeStencilTerm(7);
	}
	else
	{
		AddError(FString::Printf(TEXT("'%s' is not a mesh pass filter kind."), *Parameters));
		return false;
	}

	// The mesh pass draws the selection with the override material, red, into Mask (blue where nothing is drawn) at
	// BeforePostProcess, where the scene depth it tests against is complete; Show puts Mask on screen.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderMeshFilter"));
	AddColorBuffer(*Pipeline, TEXT("Mask"), FLinearColor::Blue);
	AddMeshPass(*Pipeline, TEXT("Draw"), EDreamPassInjection::BeforePostProcess, Selection, TEXT("Mask"), OutputMaterial);
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Mask"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Subsystem, Pipeline);

	TArray<FColor> Texels;
	if (!Fixture.CaptureFrame(Texels))
	{
		return false;
	}
	const FString What = FString::Printf(TEXT("selected by %s"), *Parameters);
	ExpectTexel(*this, What + TEXT(", the selected cube carries the override's red"), Texels, CenterTexel, FLinearColor::Red, ETexelMatch::Exact);
	ExpectTexel(*this, What + TEXT(", the cube left out stays out"), Texels, UpperTexel, FLinearColor::Blue, ETexelMatch::Exact);
	ExpectTexel(*this, What + TEXT(", the background stays out"), Texels, BackgroundTexel, FLinearColor::Blue, ETexelMatch::Exact);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderMeshModesTest,
	"DreamShader.Pass.Render.Mesh.Modes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderMeshModesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UMaterial* OutputMaterial = GetMeshOutputMaterial(*this);
	if (!OutputMaterial)
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	// Both cubes selected. The centre one's own material is the output material through an instance that writes green; the
	// upper one keeps the engine cube's material, which has no UE.DreamPassOutput.
	UStaticMeshComponent* OwnCube = Fixture.AddCube(CenterCubeLocation);
	UStaticMeshComponent* PlainCube = Fixture.AddCube(UpperCubeLocation);
	if (!OwnCube || !PlainCube)
	{
		return false;
	}
	UMaterialInstanceDynamic* GreenOutput = UMaterialInstanceDynamic::Create(OutputMaterial, OwnCube);
	GreenOutput->SetVectorParameterValue(MeshColorParameter, FLinearColor::Green);
	OwnCube->SetMaterial(0, GreenOutput);

	const FName List(TEXT("DreamPassTestList"));
	Subsystem.AddToList(List, OwnCube);
	Subsystem.AddToList(List, PlainCube);

	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderMeshModes"));
	AddColorBuffer(*Pipeline, TEXT("Mask"), FLinearColor::Blue);
	AddMeshPass(*Pipeline, TEXT("Draw"), EDreamPassInjection::BeforePostProcess, MakeListTerm(List), TEXT("Mask"), OutputMaterial);
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Mask"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	const int32 DrawIndex = Pipeline->FindPassIndex(TEXT("Draw"));
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()) || !Pipeline->Passes.IsValidIndex(DrawIndex))
	{
		return false;
	}
	Activate(Subsystem, Pipeline);

	auto ExpectCubes = [this, &Fixture](const TCHAR* Mode, const FLinearColor& Center, const FLinearColor& Upper)
	{
		TArray<FColor> Texels;
		if (Fixture.CaptureFrame(Texels))
		{
			ExpectTexel(*this, FString::Printf(TEXT("%s, the cube whose material has the output"), Mode), Texels, CenterTexel, Center, ETexelMatch::Exact);
			ExpectTexel(*this, FString::Printf(TEXT("%s, the cube whose material has none"), Mode), Texels, UpperTexel, Upper, ETexelMatch::Exact);
			ExpectTexel(*this, FString::Printf(TEXT("%s, the background"), Mode), Texels, BackgroundTexel, FLinearColor::Blue, ETexelMatch::Exact);
		}
	};

	// Override: every selected primitive with the pass's material, whatever its own.
	ExpectCubes(TEXT("Override"), FLinearColor::Red, FLinearColor::Red);

	// Own: every selected primitive with its own material; one without the output is skipped.
	Pipeline->Passes[DrawIndex].Mesh.Mode = EDreamPassMeshMode::Own;
	Pipeline->Passes[DrawIndex].Mesh.OverrideMaterial = nullptr;
	Pipeline->NotifyChanged();
	TestTrue(TEXT("Own needs no material"), Pipeline->Validate());
	ExpectCubes(TEXT("Own"), FLinearColor::Green, FLinearColor::Blue);

	// OwnOrOverride: its own material where that has the output, the pass's otherwise.
	Pipeline->Passes[DrawIndex].Mesh.Mode = EDreamPassMeshMode::OwnOrOverride;
	Pipeline->Passes[DrawIndex].Mesh.OverrideMaterial = OutputMaterial;
	Pipeline->NotifyChanged();
	ExpectCubes(TEXT("OwnOrOverride"), FLinearColor::Green, FLinearColor::Red);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderMeshDepthTest,
	"DreamShader.Pass.Render.Mesh.Depth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderMeshDepthTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UMaterial* OutputMaterial = GetMeshOutputMaterial(*this);
	if (!OutputMaterial)
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	// The selected cube, and an occluder that is not selected: a slab 100 cm away, 30 cm square, in front of the cube's middle.
	// It covers the five texels around the centre each way; the cube's face reaches ten.
	UStaticMeshComponent* Cube = Fixture.AddCube(CenterCubeLocation);
	UStaticMeshComponent* Occluder = Fixture.AddCube(FVector(100.0, 0.0, 0.0), FVector(0.1, 0.3, 0.3));
	if (!Cube || !Occluder)
	{
		return false;
	}
	const FName List(TEXT("DreamPassTestList"));
	Subsystem.AddToList(List, Cube);

	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderMeshDepth"));
	AddColorBuffer(*Pipeline, TEXT("Mask"), FLinearColor::Blue);
	AddMeshPass(*Pipeline, TEXT("Draw"), EDreamPassInjection::BeforePostProcess, MakeListTerm(List), TEXT("Mask"), OutputMaterial);
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Mask"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	const int32 DrawIndex = Pipeline->FindPassIndex(TEXT("Draw"));
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()) || !Pipeline->Passes.IsValidIndex(DrawIndex))
	{
		return false;
	}
	Activate(Subsystem, Pipeline);

	const FIntPoint LeftOfOccluder(CaptureSize / 2 - 8, CaptureSize / 2);
	const FIntPoint RightOfOccluder(CaptureSize / 2 + 8, CaptureSize / 2);

	// TestScene, the default: the scene depth hides the cube where the occluder is in front of it, and nowhere else -- the
	// cube's own depth must not fight the depth the prepass wrote for it.
	TArray<FColor> Texels;
	if (Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("TestScene, behind the occluder"), Texels, CenterTexel, FLinearColor::Blue, ETexelMatch::Exact);
		ExpectTexel(*this, TEXT("TestScene, beside the occluder, on the cube"), Texels, LeftOfOccluder, FLinearColor::Red, ETexelMatch::Exact);
		ExpectTexel(*this, TEXT("TestScene, on the cube's other side"), Texels, RightOfOccluder, FLinearColor::Red, ETexelMatch::Exact);
	}

	// None: no depth test, so the cube shows through the occluder.
	Pipeline->Passes[DrawIndex].Mesh.Depth = EDreamPassDepthMode::None;
	Pipeline->NotifyChanged();
	if (Fixture.CaptureFrame(Texels))
	{
		ExpectTexel(*this, TEXT("Depth None, behind the occluder"), Texels, CenterTexel, FLinearColor::Red, ETexelMatch::Exact);
		ExpectTexel(*this, TEXT("Depth None, the background"), Texels, BackgroundTexel, FLinearColor::Blue, ETexelMatch::Exact);
	}
	return true;
}

// --------------------------------------------------------------------------------------------------------- Fullscreen passes

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderFullscreenWriteTest,
	"DreamShader.Pass.Render.Fullscreen.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderFullscreenWriteTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UMaterial* FillMaterial = GetFillPostProcessMaterial(*this);
	if (!FillMaterial)
	{
		return false;
	}

	// The material draws green over all of Data (blue where it did not); Show puts Data on screen.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderFullscreenWrite"));
	AddColorBuffer(*Pipeline, TEXT("Data"), FLinearColor::Blue);
	AddFullscreenPass(*Pipeline, TEXT("Draw"), EDreamPassInjection::BeforePostProcess, FillMaterial, TEXT("Data"));
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Data"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	Fixture.ExpectFrame(TEXT("a fullscreen material pass writes its buffer"), FLinearColor::Green, ETexelMatch::Exact);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassRenderFullscreenReadTest,
	"DreamShader.Pass.Render.Fullscreen.ReadUserSceneTexture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassRenderFullscreenReadTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UMaterial* ReadMaterial = GetReadPostProcessMaterial(*this);
	if (!ReadMaterial)
	{
		return false;
	}

	// Fill clears Mark to red; Read draws the material, which samples its UserSceneTexture "Mark" -- bound to the buffer Mark by
	// `read Mark = Mark` -- into Result (blue where nothing is drawn); Show puts Result on screen.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_RenderFullscreenRead"));
	AddColorBuffer(*Pipeline, TEXT("Mark"), FLinearColor::Transparent);
	AddColorBuffer(*Pipeline, TEXT("Result"), FLinearColor::Blue);
	AddClearPass(*Pipeline, TEXT("Fill"), EDreamPassInjection::BeforePostProcess, TEXT("Mark"), FLinearColor::Red);
	FDreamPassDesc& Read = AddFullscreenPass(*Pipeline, TEXT("Read"), EDreamPassInjection::BeforePostProcess, ReadMaterial, TEXT("Result"));
	Read.Reads.Add(MakeBufferBinding(ReadTextureName, TEXT("Mark")));
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Result"), UE::DreamPass::BuiltinBuffers::SceneColor);
	Pipeline->NotifyChanged();
	if (!TestTrue(TEXT("the pipeline passes its own checks"), Pipeline->Validate()))
	{
		return false;
	}
	Activate(Fixture.GetSubsystem(), Pipeline);

	Fixture.ExpectFrame(TEXT("a fullscreen material pass reads a buffer through its UserSceneTexture"), FLinearColor::Red, ETexelMatch::Exact);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------- Lifecycle

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassLifecycleEditTest,
	"DreamShader.Pass.Lifecycle.EditBetweenFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassLifecycleEditTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	UDreamPassPipeline* Pipeline = MakeFillPipeline(TEXT("CP_LifecycleEdit"), FLinearColor::Red);
	Activate(Fixture.GetSubsystem(), Pipeline);
	Fixture.ExpectFrame(TEXT("as built"), FLinearColor::Red, ETexelMatch::Exact);

	// A value edited in place, as the details panel edits it: the next frame's snapshot is built from the edited asset.
	const uint32 RevisionBefore = Pipeline->GetRevision();
	Pipeline->Passes[0].Clear.Value = FLinearColor::Green;
	Pipeline->NotifyChanged();
	TestNotEqual(TEXT("NotifyChanged moves the revision on"), Pipeline->GetRevision(), RevisionBefore);
	Fixture.ExpectFrame(TEXT("its clear value edited to green"), FLinearColor::Green, ETexelMatch::Exact);

	// A pass added after it, at the same point: it runs after it.
	AddClearPass(*Pipeline, TEXT("Over"), EDreamPassInjection::EndOfView, UE::DreamPass::BuiltinBuffers::SceneColor, FLinearColor::Blue);
	Pipeline->NotifyChanged();
	TestTrue(TEXT("the added pass is usable"), Pipeline->IsPassUsable(1));
	Fixture.ExpectFrame(TEXT("a pass added after it"), FLinearColor::Blue, ETexelMatch::Exact);

	// The added pass moved to an earlier point: it now runs before the first, which covers it.
	Pipeline->Passes[1].Injection = EDreamPassInjection::PostProcessAfterTonemap;
	Pipeline->NotifyChanged();
	Fixture.ExpectFrame(TEXT("the added pass moved to PostProcess.AfterTonemap"), FLinearColor::Green, ETexelMatch::Exact);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassLifecycleRemoveTest,
	"DreamShader.Pass.Lifecycle.RemoveActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassLifecycleRemoveTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}
	UDreamPassSubsystem& Subsystem = Fixture.GetSubsystem();

	const FDreamPassHandle Backdrop = Activate(Subsystem, MakeFillPipeline(TEXT("CP_RenderBackdrop"), FLinearColor::Blue, BackdropOrder));
	const FDreamPassHandle Handle = Activate(Subsystem, MakeFillPipeline(TEXT("CP_LifecycleRemoved"), FLinearColor::Red));
	Fixture.ExpectFrame(TEXT("while it is activated"), FLinearColor::Red, ETexelMatch::Exact);

	TestTrue(TEXT("RemovePipeline removes the activation"), Subsystem.RemovePipeline(Handle));
	TestFalse(TEXT("the handle is no longer active"), Subsystem.IsActive(Handle));
	Fixture.ExpectFrame(TEXT("removed: the next frame runs without it"), FLinearColor::Blue, ETexelMatch::Exact);
	TestFalse(TEXT("removing it again removes nothing"), Subsystem.RemovePipeline(Handle));

	// With no activation left the extension is not even gathered for the family: the empty world again.
	Subsystem.RemovePipeline(Backdrop);
	Fixture.ExpectFrame(TEXT("no activation left: nothing runs"), FLinearColor::Black, ETexelMatch::Dark);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassLifecycleDestroyWorldTest,
	"DreamShader.Pass.Lifecycle.DestroyWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassLifecycleDestroyWorldTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	{
		FPassCaptureFixture Fixture(*this);
		if (!Fixture.IsReady())
		{
			return false;
		}

		// History on a view state, so the world goes down with an extension that holds a history texture and a view state
		// that holds a key.
		Fixture.GetCapture()->bAlwaysPersistRenderingState = true;
		UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_LifecycleDestroyed"));
		AddColorBuffer(*Pipeline, TEXT("Trail"), FLinearColor::Transparent, /*bHistory*/ true);
		AddCopyPass(*Pipeline, TEXT("ShowPrevious"), EDreamPassInjection::EndOfView, TEXT("Trail"), UE::DreamPass::BuiltinBuffers::SceneColor, /*bSourcePrevious*/ true);
		AddClearPass(*Pipeline, TEXT("Write"), EDreamPassInjection::EndOfView, TEXT("Trail"), FLinearColor::Red);
		Pipeline->NotifyChanged();
		Activate(Fixture.GetSubsystem(), Pipeline);

		Fixture.ExpectFrame(TEXT("frame 1"), FLinearColor::Black, ETexelMatch::Exact);
		Fixture.ExpectFrame(TEXT("frame 2 reads frame 1's history"), FLinearColor::Red, ETexelMatch::Exact);

		// A third frame queued and not waited for, then the world destroyed under it with the pipeline still active.
		Fixture.QueueFrame();
		Fixture.Teardown(/*bFlushFirst*/ false);
	}
	FlushRenderingCommands();

	// The module-wide half outlives the world: a world made afterwards runs a pipeline at AfterOpaque, which is reached through
	// the renderer's post-opaque delegate the module registered once for every world (Render/DreamPassSceneViewExtension.cpp).
	FPassCaptureFixture Second(*this);
	if (!Second.IsReady())
	{
		return false;
	}
	UDreamPassPipeline* Opaque = MakeCapturePipeline(TEXT("CP_LifecycleSecondWorld"));
	AddClearPass(*Opaque, TEXT("Fill"), EDreamPassInjection::AfterOpaque, UE::DreamPass::BuiltinBuffers::SceneColor, FLinearColor::Green);
	Opaque->NotifyChanged();
	Activate(Second.GetSubsystem(), Opaque);
	Second.ExpectFrame(TEXT("a world made after another was destroyed runs its pipeline, at AfterOpaque too"), FLinearColor::Green, ETexelMatch::Dominant);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassLifecycleInvalidPassTest,
	"DreamShader.Pass.Lifecycle.InvalidPassSkipped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPassLifecycleInvalidPassTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassRenderTests;

	FPassCaptureFixture Fixture(*this);
	if (!Fixture.IsReady())
	{
		return false;
	}

	// NotifyChanged names every problem of a pass it marks unusable (UDreamPassPipeline::RefreshUsablePasses).
	AddExpectedMessagePlain(TEXT("The pass is skipped."), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);

	// Fill clears Mark to green at BeginView; Show copies it to the screen at EndOfView. The two broken passes read
	// Mark.Previous, which a buffer without history does not have -- what an asset edited by hand can end up with. Run, either
	// would put black on screen (an absent history reads black); BrokenLate, after Show, would leave it there.
	UDreamPassPipeline* Pipeline = MakeCapturePipeline(TEXT("CP_LifecycleInvalid"));
	AddColorBuffer(*Pipeline, TEXT("Mark"), FLinearColor::Transparent);
	AddClearPass(*Pipeline, TEXT("Fill"), EDreamPassInjection::BeginView, TEXT("Mark"), FLinearColor::Green);
	AddCopyPass(*Pipeline, TEXT("BrokenEarly"), EDreamPassInjection::EndOfView, TEXT("Mark"), UE::DreamPass::BuiltinBuffers::SceneColor, /*bSourcePrevious*/ true);
	AddCopyPass(*Pipeline, TEXT("Show"), EDreamPassInjection::EndOfView, TEXT("Mark"), UE::DreamPass::BuiltinBuffers::SceneColor);
	AddCopyPass(*Pipeline, TEXT("BrokenLate"), EDreamPassInjection::EndOfView, TEXT("Mark"), UE::DreamPass::BuiltinBuffers::SceneColor, /*bSourcePrevious*/ true);
	Pipeline->NotifyChanged();

	TestFalse(TEXT("the pipeline fails its own checks"), Pipeline->Validate());
	TestTrue(TEXT("Fill is usable"), Pipeline->IsPassUsable(0));
	TestFalse(TEXT("BrokenEarly is not"), Pipeline->IsPassUsable(1));
	TestTrue(TEXT("Show is usable"), Pipeline->IsPassUsable(2));
	TestFalse(TEXT("BrokenLate is not"), Pipeline->IsPassUsable(3));

	Activate(Fixture.GetSubsystem(), Pipeline);
	Fixture.ExpectFrame(TEXT("the passes around the broken ones run and the broken ones do not"), FLinearColor::Green, ETexelMatch::Exact);
	return true;
}

// The next cases. The HLSL ones need the `.dsp` compiler's slot registry, the examples its compiled pipelines, and the
// exports the render targets it makes for them:
// - DreamShader.Pass.Render.Compute.*: a compute HLSL pass writing a buffer, dispatched over a buffer and with a fixed size.
// - DreamShader.Pass.Render.PixelHlsl.*: a pixel HLSL pass with one output and with several, reading buffers by name.
// - DreamShader.Pass.Render.Examples.*: the four example pipelines of Docs/examples/custom-pass.md, compiled from their `.dsp`.
// - DreamShader.Pass.Render.Export.*: exported buffers -- the render target's contents, its size, and which view writes it.

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#endif // WITH_DEV_AUTOMATION_TESTS
