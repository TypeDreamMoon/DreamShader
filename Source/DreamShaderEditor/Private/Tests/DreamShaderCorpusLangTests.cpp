// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runner for the DreamShaderLang 2.0 front end. Enumerates every fixture under
// Tests/Corpus/Lang and asserts each against its `"entryPoint": "lang"` golden via
// ParseDreamShaderLang (pure Core, no editor asset I/O — the fast layer).
// Each fixture surfaces as its own automation sub-test under "DreamShader.Lang2.Corpus.*".
//
// Add coverage: drop a .dss/.dsh under Tests/Corpus/Lang/<Area>/ (+ optional .expected.json).
// No new C++, no recompile — the runner discovers it on the next run. A fixture named `*.bad.dss`
// is expected to fail even without a golden; a golden's `errorContains` names the DSHnnnn code.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusLangTest,
	"DreamShader.Lang2.Corpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusLangTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("Lang"), Cases);

	for (const FCorpusCase& Case : Cases)
	{
		OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
		OutTestCommands.Add(Case.SourcePath);
	}
}

bool FDreamShaderCorpusLangTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader lang corpus test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	return RunDreamShaderLangCorpusCase(*this, Case);
}

#endif // WITH_DEV_AUTOMATION_TESTS
