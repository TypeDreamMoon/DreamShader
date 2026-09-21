// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Progress reporting: the DSH9012 Custom-code heuristic, the DSH9011 stall threshold, and the
// DSH8298 cancel path (the 1.x DSH9010 retired with the generator in batch 2).
//
// The first two are pure string/number functions and run in microseconds. The third drives the real
// compiler service, because the only claim worth making about cancellation is the one about the asset: a
// cancelled rebuild has to leave it byte-for-byte what it was, which is a statement about the
// rollback and cannot be made without a graph to roll back.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderModule.h"
#include "DreamShaderTestCommon.h"
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderGenerationProgress.h"
#include "DreamShaderCompilerService.h"
// ClassifyGeneratedAsset, so the cancel test can say the restored asset is still ours rather than
// only that it has the same number of nodes.
#include "DreamShaderGeneratedAssets.h"

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
// DSH8298 -- cancelling leaves the asset exactly as it was
//
// Headless cancellation needs the override seam: FSlowTask::ShouldCancel is gated on GIsSlowTask,
// which only the progress dialog sets, so an automation run cannot press the button. The predicate
// used below does better than counting checks anyway -- it fires the moment the graph is actually
// empty, which is precisely the window the rollback exists to cover.
//
// Batch 2 (M4): the seam is declared in DreamShaderGenerationProgress.h and defined, exported, in the compiler
// module (research-relocation section 4.7). An inline definition would give this editor-module test its own copy
// while the pipeline in the compiler module read another, and the test would stop cancelling without failing to
// build. A cancelled 2.0 compile is DSH8298; the 1.x generator's DSH9010 retired with it.
// -------------------------------------------------------------------------------------------

namespace UE::DreamShader::Editor::Private::SlowTaskTests
{
	FString MakeSlowTaskAssetName()
	{
		return FString::Printf(TEXT("M_AutoCancel_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	/** A Graph-backend material with one knob, so two variants produce two different digests. BlockName is the whole Name=. */
	FString MakeSlowTaskMaterialSource(const FString& BlockName, const TCHAR* TintExpression)
	{
		return FString::Printf(TEXT(R"(
Shader(Name="%s")
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
)"), *BlockName, TintExpression);
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCancelledGenerationLeavesAssetTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
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

	// A Graph material always saves since batch 2 (only a ThinCustom product has a memory-only state), so the material
	// is built in a scratch package root whose fixture deletes the source and every asset under it on the way out.
	Tests::FDreamShaderCompile2Fixture Fixture(TEXT("CancelledGeneration"), TEXT("Automation"), TEXT("dsm"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	const FString AssetName = MakeSlowTaskAssetName();
	const FString ObjectPath = Fixture.MakeObjectPath(AssetName);
	Fixture.TrackObjectPath(ObjectPath);

	if (!Fixture.WriteSource(*this, MakeSlowTaskMaterialSource(Fixture.MakeLegacyAssetName(AssetName), TEXT("vec3(1.0, 0.2, 0.2)"))))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError GoodMessage;
	if (!TestTrue(
			FString::Printf(TEXT("The first compile succeeds: %s"), *GoodMessage.Message),
			Tests::CompileDreamShaderTestMaterial(Fixture.GetSourceFilePath(), GoodMessage, /*bForce*/ true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("The compiled material is loadable"), Material))
	{
		return false;
	}

	const int32 GoodExpressionCount = Material->GetExpressions().Num();
	const FString GoodDigest = BuildOutputDigest(Material);
	if (!TestTrue(TEXT("The first compile produced a non-empty graph"), GoodExpressionCount > 0))
	{
		return false;
	}

	// A materially different source, so a rebuild that ran to completion could not possibly leave the
	// digest where it was. Anything the assertions below see is therefore the rollback's work.
	if (!Fixture.WriteSource(*this, MakeSlowTaskMaterialSource(Fixture.MakeLegacyAssetName(AssetName), TEXT("vec3(0.05, 0.6, 0.9)"))))
	{
		return false;
	}

	{
		// Cancel the instant the graph is actually empty: that is after the rollback armed and after
		// the teardown, which is the only window in which cancelling could leave a half-built asset.
		FScopedDreamShaderGenerationCancelOverride CancelOnceGraphIsTornDown(
			[Material]() { return Material->GetExpressions().Num() == 0; });

		UE::DreamShader::FDreamShaderError CancelMessage;
		const bool bRebuilt = Tests::CompileDreamShaderTestMaterial(Fixture.GetSourceFilePath(), CancelMessage, /*bForce*/ true);

		TestFalse(TEXT("A cancelled compile reports failure"), bRebuilt);
		TestEqual(TEXT("Cancellation is reported as DSH8298"), CancelMessage.Code, FString(TEXT("DSH8298")));
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

	// And the seam puts itself away: outside the scope above, the compile is back on the real dialog.
	TestFalse(TEXT("The cancel override is cleared when its scope ends"), static_cast<bool>(GetDreamShaderGenerationCancelOverride()));
	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_DEV_AUTOMATION_TESTS
