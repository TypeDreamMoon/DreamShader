// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Progress reporting: the DSH9012 Custom-code heuristic, the DSH9011 stall threshold, and the
// DSH9010 cancel path.
//
// The first two are pure string/number functions and run in microseconds. The third drives the real
// generator, because the only claim worth making about cancellation is the one about the asset: a
// cancelled rebuild has to leave it byte-for-byte what it was, which is a statement about the
// rollback and cannot be made without a graph to roll back.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderModule.h"
#include "DreamShaderTestCommon.h"
#include "MaterialAssetGeneration/DreamShaderGeneratedAssetDigest.h"
#include "MaterialAssetGeneration/DreamShaderGenerationProgress.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
// ClassifyGeneratedAsset, so the cancel test can say the restored asset is still ours rather than
// only that it has the same number of nodes.
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShaderTests"

// -------------------------------------------------------------------------------------------
// DSH9012 -- the Custom-node heuristic
//
// One test per axis of the rule, because the rule is a conjunction and a conjunction that is
// wrong is usually wrong in exactly one of its terms.
// -------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCustomCodeLoopHeuristicTest,
	"DreamShader.Lang.Progress.CustomCodeLoopHeuristic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCustomCodeLoopHeuristicTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	const TArray<FString> Inputs = { TEXT("StepCount"), TEXT("DensityTexture"), TEXT("UV") };

	// Positive: the shape from issue #29 -- a ray march whose iteration count is a pin, sampling
	// with a call whose mip level the compiler has to derive.
	{
		const FString Code = TEXT(
			"float3 Accum = 0;\n"
			"[loop]\n"
			"for (int Step = 0; Step < StepCount; ++Step)\n"
			"{\n"
			"    Accum += Texture3DSample(DensityTexture, DensityTextureSampler, float3(UV, Step)).rgb;\n"
			"}\n"
			"return Accum;\n");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestTrue(TEXT("input-bound loop + Texture3DSample is reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
		TestTrue(TEXT("the finding is marked found"), Finding.bFound);
		TestEqual(TEXT("the loop bound is named"), Finding.LoopBoundName, FString(TEXT("StepCount")));
		TestEqual(TEXT("the sampler is named"), Finding.SampleCall, FString(TEXT("Texture3DSample")));
	}

	// A `while` bounded by an input counts too.
	{
		const FString Code = TEXT(
			"int Step = 0;\n"
			"while (Step < StepCount) { Accum += Texture2DSample(Tex, TexSampler, UV); ++Step; }\n");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestTrue(TEXT("input-bound while loop is reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
		TestEqual(TEXT("the while bound is named"), Finding.LoopBoundName, FString(TEXT("StepCount")));
	}

	// A member-style sampler is the same problem spelled differently.
	{
		const FString Code = TEXT("for (int i = 0; i < StepCount; ++i) { C += DensityTexture.Sample(S, UV); }");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestTrue(TEXT(".Sample() counts as implicit-mip sampling"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
		TestEqual(TEXT("the member sampler is named"), Finding.SampleCall, FString(TEXT("Sample")));
	}

	// Negative: a literal bound. The compiler knows the iteration count, so unrolling terminates.
	{
		const FString Code = TEXT("for (int i = 0; i < 64; ++i) { C += Texture2DSample(Tex, TexSampler, UV); }");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("a literal bound is not reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
		TestFalse(TEXT("the finding stays empty"), Finding.bFound);
	}

	// Negative: a #define'd bound is static for the same reason, even though it is an identifier.
	{
		const FString Code = TEXT(
			"#define MAX_STEPS 64\n"
			"for (int i = 0; i < MAX_STEPS; ++i) { C += Texture2DSample(Tex, TexSampler, UV); }\n");

		const TArray<FString> InputsWithMacroName = { TEXT("MAX_STEPS") };
		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(
			TEXT("a #define'd bound is not reported even when a pin shares its name"),
			ScanCustomCodeForDynamicLoopSampling(Code, InputsWithMacroName, Finding));
	}

	// Negative: SampleLevel takes the mip as an argument, so there is nothing to derive and nothing
	// forcing the unroll. Same for SampleGrad.
	{
		const FString LevelCode = TEXT("for (int i = 0; i < StepCount; ++i) { C += Texture2DSampleLevel(Tex, TexSampler, UV, 0); }");
		const FString GradCode = TEXT("for (int i = 0; i < StepCount; ++i) { C += DensityTexture.SampleGrad(S, UV, DX, DY); }");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("SampleLevel is not reported"), ScanCustomCodeForDynamicLoopSampling(LevelCode, Inputs, Finding));
		TestFalse(TEXT("SampleGrad is not reported"), ScanCustomCodeForDynamicLoopSampling(GradCode, Inputs, Finding));
	}

	// Negative: no loop at all.
	{
		const FString Code = TEXT("return Texture2DSample(Tex, TexSampler, UV) * StepCount;");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("sampling without a loop is not reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
	}

	// Negative: a loop bounded by a local, which is what most hand-written ray marches actually do.
	{
		const FString Code = TEXT(
			"int Steps = 32;\n"
			"for (int i = 0; i < Steps; ++i) { C += Texture2DSample(Tex, TexSampler, UV); }\n");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("a local-bound loop is not reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
	}

	// Negative: the pair only exists inside a comment or a string, which the scan blanks out first.
	{
		const FString Code = TEXT(
			"// for (int i = 0; i < StepCount; ++i) { Texture2DSample(Tex, S, UV); }\n"
			"return 0;\n");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("a commented-out loop is not reported"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
	}

	// Negative: `format` must not read as `for`, and an empty pin list can never match.
	{
		const FString Code = TEXT("float format = StepCount; return Texture2DSample(Tex, TexSampler, UV) * format;");

		FDreamShaderDynamicLoopSampleFinding Finding;
		TestFalse(TEXT("'format' is not the 'for' keyword"), ScanCustomCodeForDynamicLoopSampling(Code, Inputs, Finding));
		TestFalse(
			TEXT("a node with no inputs can never trip the heuristic"),
			ScanCustomCodeForDynamicLoopSampling(
				TEXT("for (int i = 0; i < StepCount; ++i) { C += Texture2DSample(Tex, S, UV); }"),
				TArray<FString>(),
				Finding));
	}

	return true;
}

// -------------------------------------------------------------------------------------------
// DSH9011 -- the stall threshold
// -------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderShaderCompileStallThresholdTest,
	"DreamShader.Lang.Progress.ShaderCompileStallThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderShaderCompileStallThresholdTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	TestTrue(TEXT("the threshold is 30 seconds"), FMath::IsNearlyEqual(GDreamShaderShaderCompileStallSeconds, 30.0));

	TestFalse(TEXT("an instant compile says nothing"), ShouldWarnOnShaderCompileStall(0.0, false));
	TestFalse(TEXT("just under the threshold says nothing"), ShouldWarnOnShaderCompileStall(29.9, false));
	TestTrue(TEXT("exactly the threshold warns"), ShouldWarnOnShaderCompileStall(GDreamShaderShaderCompileStallSeconds, false));
	TestTrue(TEXT("well past the threshold warns"), ShouldWarnOnShaderCompileStall(600.0, false));

	// Once, not once per poll: whoever already said it must not say it again.
	TestFalse(TEXT("a stage that already warned stays quiet"), ShouldWarnOnShaderCompileStall(600.0, true));

	return true;
}

// -------------------------------------------------------------------------------------------
// DSH9010 -- cancelling leaves the asset exactly as it was
//
// Headless cancellation needs the override seam: FSlowTask::ShouldCancel is gated on GIsSlowTask,
// which only the progress dialog sets, so an automation run cannot press the button. The predicate
// used below does better than counting checks anyway -- it fires the moment the graph is actually
// empty, which is precisely the window the rollback exists to cover.
// -------------------------------------------------------------------------------------------

namespace UE::DreamShader::Editor::Private::SlowTaskTests
{
	FString MakeSlowTaskAssetName()
	{
		return FString::Printf(TEXT("M_AutoCancel_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	FString MakeSlowTaskObjectPath(const FString& AssetName)
	{
		return FString::Printf(TEXT("/Game/DreamShaderTests/Automation/%s.%s"), *AssetName, *AssetName);
	}

	/** A Graph-backend material with one knob, so two variants produce two different digests. */
	FString MakeSlowTaskMaterialSource(const FString& AssetName, const TCHAR* TintExpression)
	{
		return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Settings = {
        Backend = "Graph";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = %s;
    }
}
)"), *AssetName, TintExpression);
	}

	bool WriteSlowTaskSource(FAutomationTestBase& Test, const FString& AssetName, const TCHAR* TintExpression, FString& OutSourceFilePath)
	{
		OutSourceFilePath = UE::DreamShader::NormalizeSourceFilePath(
			FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), TEXT("Tests"), TEXT("Automation"), AssetName + TEXT(".dsm")));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutSourceFilePath), true);
		if (!FFileHelper::SaveStringToFile(
				MakeSlowTaskMaterialSource(AssetName, TintExpression),
				*OutSourceFilePath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write DreamShader automation source file '%s'."), *OutSourceFilePath));
			return false;
		}

		return true;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCancelledGenerationLeavesAssetTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderGenerateCorpusTestBase,
	"DreamShader.Compiler.Progress.CancelledGenerationLeavesAssetUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCancelledGenerationLeavesAssetTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::SlowTaskTests;

	// The default backend is ThinCustom, which emits an instance rather than a UMaterial; pin Graph
	// so LoadObject<UMaterial> has something to find.
	Tests::FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = MakeSlowTaskAssetName();
	const FString ObjectPath = MakeSlowTaskObjectPath(AssetName);

	FString SourceFilePath;
	if (!WriteSlowTaskSource(*this, AssetName, TEXT("vec3(1.0, 0.2, 0.2)"), SourceFilePath))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SourceFilePath, /*bRequireExists*/ false, /*bEvenIfReadOnly*/ true);
	};

	// bTransient: the material is built into an in-memory /Game package and never written to disk,
	// so the test leaves no asset behind -- but it is still a real, loadable UMaterial.
	UE::DreamShader::FDreamShaderError GoodMessage;
	if (!TestTrue(
			FString::Printf(TEXT("The first generation succeeds: %s"), *GoodMessage.Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, GoodMessage, /*bForce*/ true, /*bTransient*/ true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("The generated material is loadable"), Material))
	{
		return false;
	}

	const int32 GoodExpressionCount = Material->GetExpressions().Num();
	const FString GoodDigest = BuildOutputDigest(Material);
	if (!TestTrue(TEXT("The first generation produced a non-empty graph"), GoodExpressionCount > 0))
	{
		return false;
	}

	// A materially different source, so a rebuild that ran to completion could not possibly leave the
	// digest where it was. Anything the assertions below see is therefore the rollback's work.
	if (!WriteSlowTaskSource(*this, AssetName, TEXT("vec3(0.05, 0.6, 0.9)"), SourceFilePath))
	{
		return false;
	}

	{
		// Cancel the instant the graph is actually empty: that is after the rollback armed and after
		// the teardown, which is the only window in which cancelling could leave a half-built asset.
		FScopedDreamShaderGenerationCancelOverride CancelOnceGraphIsTornDown(
			[Material]() { return Material->GetExpressions().Num() == 0; });

		UE::DreamShader::FDreamShaderError CancelMessage;
		const bool bRebuilt = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, CancelMessage, /*bForce*/ true, /*bTransient*/ true);

		TestFalse(TEXT("A cancelled generation reports failure"), bRebuilt);
		TestEqual(TEXT("Cancellation is reported as DSH9010"), CancelMessage.Code, FString(TEXT("DSH9010")));
		AddInfo(FString::Printf(TEXT("Cancellation message: %s"), *CancelMessage.Message));
	}

	Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("The material survives a cancelled rebuild"), Material))
	{
		return false;
	}

	TestEqual(TEXT("The cancelled rebuild left every node in place"), Material->GetExpressions().Num(), GoodExpressionCount);
	TestEqual(TEXT("The cancelled rebuild restored the asset byte-for-byte, by digest"), BuildOutputDigest(Material), GoodDigest);
	TestEqual(
		TEXT("The restored material still classifies as Generated"),
		static_cast<int32>(ClassifyGeneratedAsset(Material)),
		static_cast<int32>(EDreamShaderDigestState::Generated));

	// And the seam puts itself away: outside the scope above, generation is back on the real dialog.
	TestFalse(TEXT("The cancel override is cleared when its scope ends"), static_cast<bool>(GetDreamShaderGenerationCancelOverride()));
	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_DEV_AUTOMATION_TESTS
