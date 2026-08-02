// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven Generate-layer runner. Enumerates fixtures under Tests/Corpus/Generate and drives the
// real material generator (FMaterialGenerator, transient) per fixture, asserting outcome + message
// against the golden. Slow layer (EditorContext) — separate namespace "DreamShader.Gen.*" so CI can
// run it in nightly while the fast "DreamShader.Lang.*" parse layer gates PRs.
//
// Some fixtures are deliberately RED: they encode a known generator bug. Each turns green when the
// corresponding bug is fixed; the fixture's own .expected.json records the expected outcome.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusGenerateTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderGenerateCorpusTestBase,
	"DreamShader.Gen.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusGenerateTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("Generate"), Cases);

	for (const FCorpusCase& Case : Cases)
	{
		OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
		OutTestCommands.Add(Case.SourcePath);
	}
}

bool FDreamShaderCorpusGenerateTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader generate corpus test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	return RunDreamShaderGenerateCorpusCase(*this, Case);
}

#endif // WITH_DEV_AUTOMATION_TESTS
