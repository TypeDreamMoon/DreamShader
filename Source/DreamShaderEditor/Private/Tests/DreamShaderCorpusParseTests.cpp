// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven parse-layer runner. Enumerates every fixture under Tests/Corpus/Parse and asserts
// each against its golden through the legacy front end (ParseDreamShaderLang, Auto front end; pure,
// no editor asset I/O -- the fast layer). Since 2.0 deleted the 1.x runtime parser, these
// fixtures are the parse-equivalence set: accepted or refused exactly as 1.x did.
// Each fixture surfaces as its own automation sub-test under "DreamShader.Lang.Parse.*".
//
// Add a keyword: drop a .dsm/.dsf/.dsh under Tests/Corpus/Parse/<Layer>/ (+ optional .expected.json).
// No new C++, no recompile — the runner discovers it on the next run.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusParseTest,
	"DreamShader.Lang.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusParseTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("Parse"), Cases);

	for (const FCorpusCase& Case : Cases)
	{
		OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
		OutTestCommands.Add(Case.SourcePath);
	}
}

bool FDreamShaderCorpusParseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader parse corpus test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	return RunDreamShaderParseCorpusCase(*this, Case);
}

#endif // WITH_DEV_AUTOMATION_TESTS
