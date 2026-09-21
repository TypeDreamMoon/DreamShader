// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runner for `.dsi` sources end to end (batch 2, M5): Tests/Corpus/Instance.
//
// A fixture is a `.dsi`; the `.dss` (or `.dsi`) its `#pragma instance(Parent = ...)` names sits beside it in the
// corpus and is copied into the same scratch directory, so a bare-name Parent resolves among sibling products the way
// it does in a project and the compile builds the stale parent first by itself. The golden is a `compile` golden:
// every asset under the fixture's package path, the parent's included, with the instance dumped as kind
// `MaterialInstance` (parent path, parameters, no nodes). Graph is pinned, so the parent is a plain UMaterial unless
// the fixture asks for ThinCustom.
//
// The engine-free half of an instance -- binding against the parent's schema, the DSH725x-727x refusals, the IR dump
// of the payload -- is Tests/Corpus/IR/Instances, run by DreamShader.Lang2.CorpusIR.
//
// Add coverage: drop `MI_X.dsi` and its parent under Tests/Corpus/Instance/<Area>/. A parent shared by several
// fixtures is fine: each fixture gets its own copy.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusInstanceTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.CorpusInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusInstanceTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Only the instances are tests; the parents beside them are what they are built against.
	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("Instance"), Cases);
	for (const FCorpusCase& Case : Cases)
	{
		if (Case.Extension.Equals(TEXT("dsi"), ESearchCase::IgnoreCase))
		{
			OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
			OutTestCommands.Add(Case.SourcePath);
		}
	}
}

bool FDreamShaderCorpusInstanceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader instance corpus test invoked without a source path."));
		return false;
	}

	FDreamShaderCompileCorpusLayer Layer;
	Layer.LayerDir = TEXT("Instance");
	Layer.ScratchArea = TEXT("Instance2");
	Layer.Extensions = { TEXT("dsi") };
	return RunDreamShaderCompileCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters), Layer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
