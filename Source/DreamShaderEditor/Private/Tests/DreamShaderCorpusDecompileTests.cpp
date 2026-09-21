// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runners for the reverse direction (batch 2, M5): decompile and migrate.
//
//   Tests/Corpus/Decompile    DreamShader.Lang2.CorpusDecompile        Core only. `.dss` -> IR -> RaiseDreamShaderIR ->
//                                                                      BuildDreamShaderAstFromIR -> printed text (golden), and
//                                                                      that text lowering to an equivalent IR
//                                                                      (AreDreamShaderIRModulesEquivalent).
//   IR, Lang/Examples,        DreamShader.Lang2.RoundtripIR            Core only. The same equivalence, without a text golden,
//   Compile                                                            for every fixture of the other 2.0 corpora that builds.
//                                                                      Tests/Corpus/Decompile/roundtrip-skips.json lists the
//                                                                      ones that cannot, each with its reason.
//   Tests/Corpus/Migrate      DreamShader.Lang2.CorpusMigrate          Core only. `.dsm` / `.dsf` / `H_*.dsh` ->
//                                                                      MigrateDreamShaderLegacyModule -> printed text (golden):
//                                                                      no comment lost, builds as 2.0, equivalent IR.
//   Tests/Corpus/Roundtrip    DreamShader.Compiler2.Roundtrip.Corpus   Editor. `.dss` -> assets -> the decompile service ->
//                                                                      text -> assets again, and the two graph dumps are one.
//
// What "equivalent" ignores is said in IR/IRCompare.h; what the engine layer's dump ignores is said in
// Commandlet/DreamShaderGraphDump.h. Neither ignores anything per fixture unless the fixture's golden says so.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------- Decompile

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusDecompileTest,
	"DreamShader.Lang2.CorpusDecompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusDecompileTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Decompile"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusDecompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader decompile corpus test invoked without a source path."));
		return false;
	}
	return RunDreamShaderDecompileCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters), /*bTextGolden*/ true);
}

// ------------------------------------------------------------------------------------------- RoundtripIR

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderRoundtripIRTest,
	"DreamShader.Lang2.RoundtripIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderRoundtripIRTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Every 2.0 corpus whose fixtures the hand-built catalog can bind. Compile's fixtures name real engine classes
	// here and there; the ones the test catalog does not list simply do not build, which this sweep skips by itself.
	AddDreamShaderCorpusTests(TEXT("IR"), TEXT("IR."), OutBeautifiedNames, OutTestCommands);
	AddDreamShaderCorpusTests(TEXT("Lang/Examples"), TEXT("Examples."), OutBeautifiedNames, OutTestCommands);
	AddDreamShaderCorpusTests(TEXT("Compile"), TEXT("Compile."), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderRoundtripIRTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader RoundtripIR test invoked without a source path."));
		return false;
	}

	const FCorpusCase Case = MakeDreamShaderCorpusCase(Parameters);
	if (Case.bBadByName)
	{
		AddInfo(FString::Printf(TEXT("[%s] is a negative fixture; nothing to round-trip."), *Case.SourcePath));
		return true;
	}
	if (const FString* Reason = GetDreamShaderRoundtripSkips().Find(MakeDreamShaderCorpusRelativeName(Case.SourcePath)))
	{
		AddInfo(FString::Printf(TEXT("[%s] skipped: %s"), *Case.SourcePath, **Reason));
		return true;
	}
	return RunDreamShaderDecompileCorpusCase(*this, Case, /*bTextGolden*/ false);
}

// ----------------------------------------------------------------------------------------------- Migrate

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusMigrateTest,
	"DreamShader.Lang2.CorpusMigrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusMigrateTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Migrate"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusMigrateTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader migrate corpus test invoked without a source path."));
		return false;
	}
	return RunDreamShaderMigrateCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters));
}

// ------------------------------------------------------------------------------------- Roundtrip (engine)

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusRoundtripTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Roundtrip.Corpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusRoundtripTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Roundtrip"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader roundtrip corpus test invoked without a source path."));
		return false;
	}
	return RunDreamShaderRoundtripCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters));
}

#endif // WITH_DEV_AUTOMATION_TESTS
