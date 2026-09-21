// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Data-driven runners for 1.x sources through the 2.0 pipeline: the legacy front end is the only thing
// that reads a `.dsm` / `.dsf` now, and these three layers are what says it reads them the way 1.x did.
//
//   Tests/Corpus/Legacy/Parse     DreamShader.Lang2.CorpusLegacyParse   Core only. ParseDreamShaderLang with trivia: codes,
//                                                                       FLegacyMigrationInfo counts, the printed 2.0 text, and
//                                                                       that text parsing back as 2.0 source.
//   Tests/Corpus/Legacy/IR        DreamShader.Lang2.CorpusLegacyIR      Core only. bind -> build -> passes -> validate with the
//                                                                       hand-built catalog: the documented 1.x rules (L2-L19),
//                                                                       one fixture each. `.dss` twins live here too where a
//                                                                       rule is general and its fixture belongs beside the rest.
//   Tests/Corpus/Legacy/Compile   DreamShader.Compiler2.CorpusLegacy    Editor. The fixtures of the deleted Generate layer, as
//                                                                       `compile` goldens seeded from the 1.x capture of 09-15
//                                                                       (Saved/DreamShader/GraphBaseline/v2-6c2e0b6-generate-corpus).
//                                                                       That capture ran under the project's default backend,
//                                                                       ThinCustom, so this layer pins ThinCustom where the
//                                                                       Compile layer pins Graph.
//
// Add coverage: drop a `.dsm` / `.dsf` (or a `.dsh` for Legacy/Parse) under the layer's directory, with an optional
// `.expected.json`. No new C++, no recompile. A legacy fixture's `Name=` is rewritten by the Compile runner so its
// asset lands in the fixture's scratch package; the asset keeps the fixture file's stem as its name.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

// ------------------------------------------------------------------------------------------ Legacy/Parse

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusLegacyParseTest,
	"DreamShader.Lang2.CorpusLegacyParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusLegacyParseTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Legacy/Parse"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusLegacyParseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader legacy parse corpus test invoked without a source path."));
		return false;
	}
	return RunDreamShaderLegacyParseCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters));
}

// --------------------------------------------------------------------------------------------- Legacy/IR

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusLegacyIRTest,
	"DreamShader.Lang2.CorpusLegacyIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusLegacyIRTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Legacy/IR"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusLegacyIRTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader legacy IR corpus test invoked without a source path."));
		return false;
	}

	FDreamShaderIRCorpusLayer Layer;
	Layer.EntryPoint = TEXT("legacy-ir");
	Layer.Extensions = { TEXT("dsm"), TEXT("dsf"), TEXT("dss") };
	return RunDreamShaderIRCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters), Layer);
}

// ---------------------------------------------------------------------------------------- Legacy/Compile

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderCorpusLegacyCompileTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.CorpusLegacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderCorpusLegacyCompileTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UE::DreamShader::Editor::Private::Tests::AddDreamShaderCorpusTests(TEXT("Legacy/Compile"), TEXT(""), OutBeautifiedNames, OutTestCommands);
}

bool FDreamShaderCorpusLegacyCompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (Parameters.IsEmpty())
	{
		AddError(TEXT("DreamShader legacy compile corpus test invoked without a source path."));
		return false;
	}

	FDreamShaderCompileCorpusLayer Layer;
	Layer.LayerDir = TEXT("Legacy/Compile");
	Layer.ScratchArea = TEXT("Legacy2");
	Layer.Extensions = { TEXT("dsm"), TEXT("dsf") };
	// What the seeded goldens were captured under: the project default of 09-15.
	Layer.PinnedBackend = EDreamShaderDefaultBackend::ThinCustom;
	return RunDreamShaderCompileCorpusCase(*this, MakeDreamShaderCorpusCase(Parameters), Layer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
