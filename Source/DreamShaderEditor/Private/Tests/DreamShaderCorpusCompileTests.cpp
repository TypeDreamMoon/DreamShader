// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runner for the whole 2.0 pipeline. Enumerates every fixture under
// Tests/Corpus/Compile, copies it under the project's DShader root, compiles it through
// FMaterialGenerator::GenerateAssetsFromFile (whose `.dss` hook routes it into the new pipeline),
// and asserts the assets it produced against the fixture's `"entryPoint": "compile"` golden —
// outcome, the DSHnnnn codes, the asset list, and the normalised `dump-graph` JSON.
//
// Slow layer: editor, reflection, real /Game packages. The 2.0 pipeline has no transient request
// (Compiler/DreamShaderCompilerPipeline.h), so a fixture really does write assets; the runner's
// fixture object deletes both the copy and every asset it made on the way out, whatever happened
// in between.
//
// Add coverage: drop a .dss under Tests/Corpus/Compile/<Area>/ (+ optional .expected.json). No new
// C++, no recompile.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusCompileTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Corpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusCompileTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("Compile"), Cases);

	for (const FCorpusCase& Case : Cases)
	{
		OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
		OutTestCommands.Add(Case.SourcePath);
	}
}

bool FDreamShaderCorpusCompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader compile corpus test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	return RunDreamShaderCompileCorpusCase(*this, Case);
}

#endif // WITH_DEV_AUTOMATION_TESTS
