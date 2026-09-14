// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runner for the 2.0 middle end. Enumerates every fixture under Tests/Corpus/IR and
// takes it through ParseDreamShaderLang -> BindDreamShaderLang -> BuildDreamShaderIR ->
// RunDreamShaderIRPasses -> ValidateDreamShaderIR, asserting each against its
// `"entryPoint": "ir"` golden. Pure Core: no asset I/O, no reflection, no editor state — the fast
// layer of the new pipeline, and the one that can gate a PR.
//
// The builtin catalog is the hand-built MakeDreamShaderTestBuiltinCatalog(), not the engine's, so a
// fixture here may only name the builtins that catalog declares (see DreamShaderTestCommon.h for
// the list and the reasoning). Anything that needs real reflection belongs in Tests/Corpus/Compile.
//
// Add coverage: drop a .dss under Tests/Corpus/IR/<Area>/ (+ optional .expected.json). No new C++,
// no recompile. A fixture named `*.bad.dss` is expected to be refused even without a golden; a
// golden's `errorContains` names the DSHnnnn code, never the English.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusIRTest,
	"DreamShader.Lang2.CorpusIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusIRTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(TEXT("IR"), Cases);

	for (const FCorpusCase& Case : Cases)
	{
		OutBeautifiedNames.Add(Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
		OutTestCommands.Add(Case.SourcePath);
	}
}

bool FDreamShaderCorpusIRTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader IR corpus test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	return RunDreamShaderIRCorpusCase(*this, Case);
}

#endif // WITH_DEV_AUTOMATION_TESTS
