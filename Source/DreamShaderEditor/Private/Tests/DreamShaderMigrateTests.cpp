// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `dsc migrate`, in two halves.
//
// DreamShader.Lang2.Migrate.* -- MigrateDreamShaderLegacyModule (Migrate/LangMigrate.h), Core only: a 1.x text is read by
// the legacy front end, bound against the hand-built catalog, rewritten and printed, and the printed text is looked at
// for what each rule has to have written. Substrings, not whole texts: the corpus (Tests/Corpus/Migrate) pins the
// whole text and proves the meaning (equivalent IR, no comment lost); these pin the rule.
//
// DreamShader.Compiler2.Migrate.* -- MigrateDreamShaderSource (Commandlet/DreamShaderMigrate.h), editor: the real
// catalog, the real destination rules, and what happens to the files. Nothing is compiled into an asset.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commandlet/DreamShaderCommandlet.h"
#include "Commandlet/DreamShaderMigrate.h"
#include "Migrate/LangMigrate.h"

#include "Misc/OutputDeviceRedirector.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::MigrateTests
{
	using namespace UE::DreamShader::Lang;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;

	struct FMigrated
	{
		bool bBuiltAsLegacy = false;
		bool bMigrated = false;
		FString Text;
		TArray<FString> Codes;

		bool HasCode(const TCHAR* Code) const { return Codes.Contains(FString(Code)); }
	};

	/** 1.x text -> migrated 2.0 text. Imports resolve beside the Migrate corpus's Basics fixtures. */
	inline FMigrated Migrate(FAutomationTestBase& Test, const TCHAR* FileName, const TCHAR* LegacyText)
	{
		using namespace UE::DreamShader::Editor::Private::Tests;

		FMigrated Out;
		const FString Directory = FPaths::Combine(GetDreamShaderCorpusRoot(), TEXT("Migrate"), TEXT("Basics"));

		FDreamShaderIRRunOptions Options;
		Options.bKeepTrivia = true;
		Options.IncludeDirectory = Directory;

		FIRRun Run;
		RunDreamShaderIRPipeline(FPaths::Combine(Directory, FileName), LegacyText, Options, Run);
		Out.bBuiltAsLegacy = Run.Parse.Module.IsValid() && Run.Bind.Bound.IsValid() && Run.Errors.Num() == 0;
		if (!Out.bBuiltAsLegacy)
		{
			Test.AddError(FString::Printf(TEXT("[%s] does not build as 1.x: %s"), FileName, *Run.ErrorText()));
			return Out;
		}

		const FLegacyMigrationInfo NoInfo;
		FLangDiagnosticSink Sink(FileName);
		Out.bMigrated = MigrateDreamShaderLegacyModule(
			*Run.Parse.Module,
			Run.Parse.Legacy.IsValid() ? *Run.Parse.Legacy : NoInfo,
			*Run.Bind.Bound,
			FLangMigrateOptions(),
			Sink);
		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Out.Codes.AddUnique(Diagnostic.Code);
		}
		if (Out.bMigrated)
		{
			Out.Text = PrintDreamShaderLang(*Run.Parse.Module);
		}
		return Out;
	}

	inline int32 CountOccurrences(const FString& Text, const TCHAR* Needle)
	{
		int32 Count = 0;
		int32 From = 0;
		const int32 NeedleLength = FCString::Strlen(Needle);
		while ((From = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			++Count;
			From += NeedleLength;
		}
		return Count;
	}

	inline void ExpectContains(FAutomationTestBase& Test, const FMigrated& Migrated, const TCHAR* What, const TCHAR* Needle)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s: the migrated text contains '%s'\n%s"), What, Needle, *Migrated.Text),
			Migrated.Text.Contains(Needle, ESearchCase::CaseSensitive));
	}

	inline void ExpectLacks(FAutomationTestBase& Test, const FMigrated& Migrated, const TCHAR* What, const TCHAR* Needle)
	{
		Test.TestFalse(
			FString::Printf(TEXT("%s: the migrated text does not contain '%s'\n%s"), What, Needle, *Migrated.Text),
			Migrated.Text.Contains(Needle, ESearchCase::CaseSensitive));
	}

	static const TCHAR* const GSplitPrototype = TEXT(
		"VirtualFunction(Name=\"MF_Split\")\n"
		"{\n"
		"    Options = { Asset = Path(Game, \"Functions/MF_Split\"); }\n"
		"    Inputs = { float2 UV; }\n"
		"    Outputs = { float3 Head; float Tail; }\n"
		"}\n");

	static const TCHAR* const GTwoOutputs = TEXT(
		"    Outputs = {\n"
		"        vec3 Color;\n"
		"        float Alpha;\n"
		"        Base.EmissiveColor = Color;\n"
		"        Base.Opacity = Alpha;\n"
		"    }\n");
}

// ---------------------------------------------------------------------------------------------
// Declarations: import, Backend, opt
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateDeclarationsTest,
	"DreamShader.Lang2.Migrate.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateDeclarationsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	{
		const FMigrated M = Migrate(*this, TEXT("Decl_Import.dsm"), TEXT(
			"import \"MigrateShared.dsh\";\n"
			"Shader(Name=\"M_MtImport\")\n"
			"{\n"
			"    Settings = { Backend = \"Instance\"; ShadingModel = \"Unlit\"; }\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = vec3(Luma(vec3(1.0, 0.5, 0.25)), 0.0, 0.0); }\n"
			"}\n"));
		if (M.bMigrated)
		{
			ExpectContains(*this, M, TEXT("import"), TEXT("#include \"MigrateShared.dsh\""));
			ExpectLacks(*this, M, TEXT("import"), TEXT("import "));
			ExpectContains(*this, M, TEXT("Backend = Instance"), TEXT("Backend = ThinCustom"));
			ExpectLacks(*this, M, TEXT("Backend = Instance"), TEXT("Instance"));
		}
	}

	{
		const FMigrated M = Migrate(*this, TEXT("Decl_Opt.dsf"), TEXT(
			"ShaderFunction(Name=\"Functions/MF_MtOpt\")\n"
			"{\n"
			"    Inputs = { vec3 InColor; opt float Strength; opt vec4 Extra; opt float Given = 2.0; }\n"
			"    Outputs = { vec3 OutColor; }\n"
			"    Graph = { OutColor = InColor * Strength * Given + Extra.rgb; }\n"
			"}\n"));
		if (M.bMigrated)
		{
			ExpectContains(*this, M, TEXT("opt float"), TEXT("float Strength = 0.0"));
			// Four wide: the engine's preview value has an alpha of one.
			ExpectContains(*this, M, TEXT("opt vec4"), TEXT("float4 Extra = float4(0.0, 0.0, 0.0, 1.0)"));
			ExpectContains(*this, M, TEXT("opt with a default"), TEXT("float Given = 2.0"));
		}
	}

	{
		// No host answered where the asset goes: `@root` stays, and the rewrite says so.
		const FMigrated M = Migrate(*this, TEXT("Decl_Root.dsm"), TEXT(
			"Shader(Name=\"Folder/M_MtRoot\", Root=\"Game\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = vec3(0.0, 1.0, 0.0); }\n"
			"}\n"));
		if (M.bMigrated)
		{
			ExpectContains(*this, M, TEXT("@root without an answer"), TEXT("@root Game"));
			TestTrue(TEXT("@root without an answer: DSH9094"), M.HasCode(TEXT("DSH9094")));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Spellings: L2, L12, L19
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSpellingsTest,
	"DreamShader.Lang2.Migrate.Spellings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSpellingsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	const FMigrated M = Migrate(*this, TEXT("Spellings.dsm"), TEXT(
		"Shader(Name=\"M_MtSpellings\")\n"
		"{\n"
		"    Properties = { float Strength = 2.0; vec3 A = vec3(1.0, 0.0, 0.0); vec3 B = vec3(0.0, 0.0, 1.0); }\n"
		"    Outputs = { vec3 Color; Base.emissivecolor = Color; }\n"
		"    Graph = {\n"
		"        float2 MyUV = UE.texcoord(index = 0);\n"
		"        vec3 Mixed = mix(A, B, Saturate(myuv.x * strength));\n"
		"        vec3 Scene = UE.SceneTexture(Id = PPI_SceneColor).rgb;\n"
		"        Color = fract(Mixed) + mod(Scene, 0.5);\n"
		"    }\n"
		"}\n"));
	if (!M.bMigrated)
	{
		return false;
	}

	// L2: the op's HLSL name.
	ExpectContains(*this, M, TEXT("mix"), TEXT("lerp("));
	ExpectLacks(*this, M, TEXT("mix"), TEXT("mix("));
	ExpectContains(*this, M, TEXT("fract"), TEXT("frac("));
	ExpectLacks(*this, M, TEXT("fract"), TEXT("fract("));
	ExpectContains(*this, M, TEXT("mod"), TEXT("fmod("));

	// L19: the declaration's spelling, for locals, uniforms, intrinsics, classes, arguments and attributes.
	ExpectContains(*this, M, TEXT("local in another case"), TEXT("MyUV.x"));
	ExpectLacks(*this, M, TEXT("local in another case"), TEXT("myuv"));
	ExpectContains(*this, M, TEXT("uniform in another case"), TEXT("* Strength"));
	ExpectContains(*this, M, TEXT("intrinsic in another case"), TEXT("saturate("));
	ExpectLacks(*this, M, TEXT("class in another case"), TEXT("texcoord"));
	ExpectContains(*this, M, TEXT("argument in another case"), TEXT("CoordinateIndex = 0"));
	ExpectContains(*this, M, TEXT("attribute in another case"), TEXT(".EmissiveColor"));
	ExpectLacks(*this, M, TEXT("attribute in another case"), TEXT("emissivecolor"));

	// L12: the catalog's enumerator.
	ExpectContains(*this, M, TEXT("enumerator with the engine's prefix"), TEXT("SceneTextureId = SceneColor"));
	ExpectLacks(*this, M, TEXT("enumerator with the engine's prefix"), TEXT("PPI_"));
	return true;
}

// ---------------------------------------------------------------------------------------------
// L3b: selections and value calls
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSelectionsTest,
	"DreamShader.Lang2.Migrate.Selections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSelectionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	// Two selections over one call: one call statement, one variable per output.
	{
		const FString Source = FString(GSplitPrototype) + TEXT("Shader(Name=\"M_MtShared\")\n{\n") + GTwoOutputs + TEXT(
			"    Graph = {\n"
			"        float2 uv = UE.TexCoord(Index = 0);\n"
			"        Color = MF_Split(uv, Output = \"Head\");\n"
			"        Alpha = MF_Split(uv, OutputIndex = 1);\n"
			"    }\n"
			"}\n");
		const FMigrated M = Migrate(*this, TEXT("Select_Shared.dsm"), *Source);
		if (M.bMigrated)
		{
			// The prototype, and ONE call.
			TestEqual(FString::Printf(TEXT("shared: `MF_Split(` is written twice, the prototype and one call\n%s"), *M.Text), CountOccurrences(M.Text, TEXT("MF_Split(")), 2);
			ExpectContains(*this, M, TEXT("shared"), TEXT("float3 MF_Split_Head;"));
			ExpectContains(*this, M, TEXT("shared"), TEXT("float MF_Split_Tail;"));
			ExpectContains(*this, M, TEXT("shared"), TEXT("Color = MF_Split_Head;"));
			ExpectContains(*this, M, TEXT("shared"), TEXT("Alpha = MF_Split_Tail;"));
			ExpectLacks(*this, M, TEXT("shared"), TEXT("OutputIndex"));
		}
	}

	// A write to what the call reads, between the two: two calls.
	{
		const FString Source = FString(GSplitPrototype) + TEXT("Shader(Name=\"M_MtInvalidated\")\n{\n") + GTwoOutputs + TEXT(
			"    Graph = {\n"
			"        float2 uv = UE.TexCoord(Index = 0);\n"
			"        Color = MF_Split(uv, Output = \"Head\");\n"
			"        uv = uv * 2.0;\n"
			"        Alpha = MF_Split(uv, Output = \"Tail\");\n"
			"    }\n"
			"}\n");
		const FMigrated M = Migrate(*this, TEXT("Select_Invalidated.dsm"), *Source);
		if (M.bMigrated)
		{
			TestEqual(FString::Printf(TEXT("invalidated: `MF_Split(` is written three times, the prototype and two calls\n%s"), *M.Text), CountOccurrences(M.Text, TEXT("MF_Split(")), 3);
			ExpectContains(*this, M, TEXT("invalidated"), TEXT("MF_Split_Tail_2"));
		}
	}

	// A selection inside a branch is hoisted inside the branch, not in front of the `if`.
	{
		const FString Source = FString(GSplitPrototype) + TEXT("Shader(Name=\"M_MtNested\")\n{\n") + GTwoOutputs + TEXT(
			"    Graph = {\n"
			"        float2 uv = UE.TexCoord(Index = 0);\n"
			"        Alpha = 1.0;\n"
			"        if (uv.x > 0.5) {\n"
			"            Color = MF_Split(uv, Output = \"Head\");\n"
			"        } else {\n"
			"            Color = vec3(0.0, 0.0, 0.0);\n"
			"        }\n"
			"    }\n"
			"}\n");
		const FMigrated M = Migrate(*this, TEXT("Select_Nested.dsm"), *Source);
		if (M.bMigrated)
		{
			const int32 IfAt = M.Text.Find(TEXT("if ("), ESearchCase::CaseSensitive);
			const int32 CallAt = M.Text.Find(TEXT("MF_Split(uv"), ESearchCase::CaseSensitive);
			TestTrue(FString::Printf(TEXT("nested: the call statement is inside the `if`\n%s"), *M.Text), IfAt != INDEX_NONE && CallAt > IfAt);
		}
	}

	// A value call of a function that returns a value, leaving its `out` out: the `out` goes to a variable nobody reads.
	{
		const FMigrated M = Migrate(*this, TEXT("Select_AbsentOuts.dsm"), TEXT(
			"VirtualFunction(Name=\"MF_More\")\n"
			"{\n"
			"    Options = { Asset = Path(Game, \"Functions/MF_More\"); }\n"
			"    Inputs = { float3 Colour; }\n"
			"    Outputs = { float3 Result; float Extra; }\n"
			"}\n"
			"Shader(Name=\"M_MtAbsent\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = MF_More(vec3(1.0, 0.5, 0.25)) * 2.0; }\n"
			"}\n"));
		if (M.bMigrated)
		{
			ExpectContains(*this, M, TEXT("absent outs"), TEXT("float MF_More_Extra;"));
			ExpectContains(*this, M, TEXT("absent outs"), TEXT("Extra = MF_More_Extra)"));
			// Still a value call, in place.
			ExpectContains(*this, M, TEXT("absent outs"), TEXT("Color = MF_More("));
		}
	}

	// A value call of a function that returns nothing: the value is its first `out`.
	{
		const FMigrated M = Migrate(*this, TEXT("Select_VoidValue.dsm"), TEXT(
			"VirtualFunction(Name=\"MF_OnlyOuts\")\n"
			"{\n"
			"    Options = { Asset = Path(Game, \"Functions/MF_OnlyOuts\"); }\n"
			"    Inputs = { float3 Colour; }\n"
			"    Outputs = { float3 Tinted; float Weight; }\n"
			"}\n"
			"Shader(Name=\"M_MtVoid\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = MF_OnlyOuts(vec3(1.0, 0.5, 0.25)) + vec3(0.1, 0.1, 0.1); }\n"
			"}\n"));
		if (M.bMigrated)
		{
			ExpectContains(*this, M, TEXT("void value call"), TEXT("Tinted = MF_OnlyOuts_Tinted"));
			ExpectContains(*this, M, TEXT("void value call"), TEXT("Color = MF_OnlyOuts_Tinted + "));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// L5: statement calls
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateReceiversTest,
	"DreamShader.Lang2.Migrate.Receivers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateReceiversTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	const FMigrated M = Migrate(*this, TEXT("Receivers.dsm"), TEXT(
		"VirtualFunction(Name=\"MF_TwoOut\")\n"
		"{\n"
		"    Options = { Asset = Path(Game, \"Functions/MF_TwoOut\"); }\n"
		"    Inputs = { float3 Colour; opt float Gain = 1.0; }\n"
		"    Outputs = { float3 Result; float Weight; }\n"
		"}\n"
		"Function float Halve(in float value) { return value * 0.5; }\n"
		"Shader(Name=\"M_MtReceivers\")\n"
		"{\n"
		"    Outputs = { vec3 Color; float Alpha; Base.EmissiveColor = Color; Base.Opacity = Alpha; }\n"
		"    Graph = {\n"
		"        // both receivers undeclared, the optional input left out\n"
		"        MF_TwoOut(vec3(1.0, 0.5, 0.25), Shaded, W);\n"
		"        float Halved;\n"
		"        Halve(W, Halved);\n"
		"        Color = Shaded;\n"
		"        Alpha = Halved;\n"
		"    }\n"
		"}\n"));
	if (!M.bMigrated)
	{
		return false;
	}

	// The call declared `Shaded`, so the statement is its declaration; `W` is declared in front of it.
	ExpectContains(*this, M, TEXT("the return value's receiver, undeclared"), TEXT("float3 Shaded = MF_TwoOut("));
	ExpectContains(*this, M, TEXT("an `out` receiver, undeclared"), TEXT("float W;"));
	// Input order and parameter order part ways at the optional input: named from there on.
	ExpectContains(*this, M, TEXT("an `out` behind an omitted optional input"), TEXT("Weight = W)"));
	// A declared receiver is assigned.
	ExpectContains(*this, M, TEXT("the return value's receiver, declared"), TEXT("Halved = Halve(W);"));
	// The comment stays above the line it was written above.
	const int32 CommentAt = M.Text.Find(TEXT("// both receivers undeclared"), ESearchCase::CaseSensitive);
	// The call, not the prototype above it, which spells `MF_TwoOut(float3` too.
	const int32 CallAt = M.Text.Find(TEXT("= MF_TwoOut("), ESearchCase::CaseSensitive);
	TestTrue(FString::Printf(TEXT("the comment is still in front of the call\n%s"), *M.Text), CommentAt != INDEX_NONE && CommentAt < CallAt);
	return true;
}

// ---------------------------------------------------------------------------------------------
// What is refused, and the comment collector
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateRefusalsTest,
	"DreamShader.Lang2.Migrate.Refusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateRefusalsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	// A root-qualified import never reaches the rewrite: the 1.x front end refuses it (DSH2252), for a compile as much as
	// for a migration, because the 2.0 include resolver reads one source root. DSH9091 stays behind it for a tree built
	// by hand.
	{
		using namespace UE::DreamShader::Editor::Private::Tests;

		const FString Directory = FPaths::Combine(GetDreamShaderCorpusRoot(), TEXT("Migrate"), TEXT("Basics"));
		FDreamShaderIRRunOptions Options;
		Options.bKeepTrivia = true;
		Options.IncludeDirectory = Directory;

		FIRRun Run;
		RunDreamShaderIRPipeline(FPaths::Combine(Directory, TEXT("Refuse_RootImport.dsm")), TEXT(
			"import \"Project:X.dsh\";\n"
			"Shader(Name=\"M_MtRootImport\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = Shade(vec3(1.0, 1.0, 1.0)); }\n"
			"}\n"), Options, Run);
		TestTrue(TEXT("a root-qualified import does not build as 1.x"), Run.Errors.Num() > 0);
		TestTrue(FString::Printf(TEXT("and the refusal is DSH2252 (actual: %s)"), *Run.ErrorText()), Run.ErrorText().Contains(TEXT("DSH2252")));
	}

	// The comment collector behind DSH9092: what was said, not how it was fenced.
	{
		TArray<FString> Comments;
		CollectDreamShaderComments(
			FLangSourceText(TEXT("Comments.dss"), TEXT(
				"// line\n"
				"/* block */\n"
				"/// doc text\n"
				"#pragma material(BlendMode = Opaque) // trailing a directive\n"
				"uniform float A = 1; //   padded   \n"
				"//////// ruler ////////\n")),
			/*bIncludeDocComments*/ true,
			Comments);
		TestEqual(TEXT("six comments"), Comments.Num(), 6);
		TestTrue(TEXT("a line comment, without its slashes"), Comments.Contains(TEXT("line")));
		TestTrue(TEXT("a block comment, without its fence"), Comments.Contains(TEXT("block")));
		TestTrue(TEXT("a doc comment"), Comments.Contains(TEXT("doc text")));
		TestTrue(TEXT("a comment trailing a directive"), Comments.Contains(TEXT("trailing a directive")));
		TestTrue(TEXT("trimmed"), Comments.Contains(TEXT("padded")));
		TestTrue(TEXT("a ruler keeps its word"), Comments.Contains(TEXT("ruler")));

		TArray<FString> WithoutDoc;
		CollectDreamShaderComments(FLangSourceText(TEXT("Comments.dss"), TEXT("// line\n/// doc text\n")), /*bIncludeDocComments*/ false, WithoutDoc);
		TestEqual(TEXT("without doc comments: one"), WithoutDoc.Num(), 1);
	}
	return true;
}

// =============================================================================================
// The editor half
// =============================================================================================

namespace UE::DreamShader::Editor::Private::MigrateTests
{
	inline bool ResultHasCode(const FDreamShaderMigrateResult& Result, const TCHAR* Code)
	{
		return Result.Diagnostics.ContainsByPredicate([Code](const FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	inline FString DescribeResult(const FDreamShaderMigrateResult& Result)
	{
		TArray<FString> Lines;
		for (const FLangDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic));
		}
		return Result.Error + TEXT(" | ") + FString::Join(Lines, TEXT(" | "));
	}

	/** A minimal 1.x material whose asset lands under the fixture's package path, where the `.dss` beside it would put it too. */
	inline FString MakeLegacyMaterial(const UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture& Fixture, const TCHAR* AssetName)
	{
		return FString::Printf(TEXT(
			"// kept comment\n"
			"Shader(Name=\"%s\", Root=\"Game\")\n"
			"{\n"
			"    Properties = { float Gain = 0.5; }\n"
			"    Settings = { ShadingModel = \"Unlit\"; }\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = vec3(Gain, Gain, Gain); }\n"
			"}\n"),
			*Fixture.MakeLegacyAssetName(AssetName));
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSourceDryRunTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Migrate.DryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSourceDryRunTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	// The file is named after its asset, so the `.dss` beside it derives the package the 1.x Name= said.
	FDreamShaderCompile2Fixture Fixture(TEXT("M_MtDryRun"), TEXT("Migrate2"), TEXT("dsm"));
	if (!Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtDryRun"))))
	{
		return false;
	}

	FDreamShaderMigrateOptions Options;
	Options.bDryRun = true;
	FDreamShaderMigrateResult Result;
	const bool bMigrated = MigrateDreamShaderSource(Fixture.GetSourceFilePath(), Options, Result);

	TestTrue(FString::Printf(TEXT("the dry run succeeds (%s)"), *DescribeResult(Result)), bMigrated && Result.bSucceeded);
	TestTrue(TEXT("the output is the `.dss` beside the source"), Result.OutputFilePath.Equals(FPaths::ChangeExtension(Fixture.GetSourceFilePath(), TEXT("dss")), ESearchCase::IgnoreCase));
	TestFalse(TEXT("nothing was written"), IFileManager::Get().FileExists(*Result.OutputFilePath));
	TestTrue(TEXT("the source is where it was"), IFileManager::Get().FileExists(*Fixture.GetSourceFilePath()));
	TestTrue(TEXT("no backup was made"), Result.BackupFilePath.IsEmpty());

	TestTrue(TEXT("the text is 2.0"), Result.MigratedText.Contains(TEXT("export void M_MtDryRun(inout material")));
	TestTrue(TEXT("the comment is kept"), Result.MigratedText.Contains(TEXT("// kept comment")));
	// The file's own place already names the asset: neither the 1.x spelling nor a `@name` is needed.
	TestFalse(FString::Printf(TEXT("no `@root` is left\n%s"), *Result.MigratedText), Result.MigratedText.Contains(TEXT("@root")));
	TestFalse(FString::Printf(TEXT("no `@name` is written\n%s"), *Result.MigratedText), Result.MigratedText.Contains(TEXT("@name")));
	TestFalse(FString::Printf(TEXT("the graphs are equivalent: no DSH9098 (%s)"), *DescribeResult(Result)), ResultHasCode(Result, TEXT("DSH9098")));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSourceNameTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Migrate.NameWhereTheFileWouldNotPutIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSourceNameTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	FDreamShaderCompile2Fixture Fixture(TEXT("M_MtNamed"), TEXT("Migrate2"), TEXT("dsm"));

	// Another folder than the file's own: the `.dss` has to say where the asset is.
	const FString Source = MakeLegacyMaterial(Fixture, TEXT("Elsewhere/M_MtNamed"));
	if (!Fixture.WriteSource(*this, Source))
	{
		return false;
	}

	FDreamShaderMigrateOptions Options;
	Options.bCheck = true;
	FDreamShaderMigrateResult Result;
	MigrateDreamShaderSource(Fixture.GetSourceFilePath(), Options, Result);

	TestTrue(FString::Printf(TEXT("the check succeeds (%s)"), *DescribeResult(Result)), Result.bSucceeded);
	TestTrue(
		FString::Printf(TEXT("`@name` carries the package path\n%s"), *Result.MigratedText),
		Result.MigratedText.Contains(FString::Printf(TEXT("@name %s/Elsewhere/M_MtNamed"), *Fixture.GetPackagePath())));
	TestFalse(TEXT("and `@root` is gone"), Result.MigratedText.Contains(TEXT("@root")));
	TestFalse(FString::Printf(TEXT("the asset stays where it is: no DSH9098 (%s)"), *DescribeResult(Result)), ResultHasCode(Result, TEXT("DSH9098")));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSourceFilesTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Migrate.Files",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSourceFilesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	IFileManager& FileManager = IFileManager::Get();

	// A write with a backup: the `.dss` appears, the `.dsm` moves under Saved.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_MtWrite"), TEXT("Migrate2"), TEXT("dsm"));
		if (!Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtWrite"))))
		{
			return false;
		}

		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Result);
		TestTrue(FString::Printf(TEXT("write: succeeds (%s)"), *DescribeResult(Result)), Result.bSucceeded);
		TestTrue(TEXT("write: the `.dss` exists"), FileManager.FileExists(*Result.OutputFilePath));
		TestFalse(TEXT("write: the `.dsm` is gone"), FileManager.FileExists(*Fixture.GetSourceFilePath()));
		TestTrue(TEXT("write: the backup exists"), !Result.BackupFilePath.IsEmpty() && FileManager.FileExists(*Result.BackupFilePath));
		TestTrue(TEXT("write: the backup is under Saved/DreamShader/Migrated"), Result.BackupFilePath.Contains(TEXT("/DreamShader/Migrated/")));

		FString Written;
		FFileHelper::LoadFileToString(Written, *Result.OutputFilePath);
		TestTrue(TEXT("write: the file holds the migrated text"), Written.Equals(Result.MigratedText, ESearchCase::CaseSensitive));

		// A second migration of the same stem finds the `.dss` in its way.
		if (Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtWrite"))))
		{
			FDreamShaderMigrateResult Second;
			MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Second);
			TestFalse(TEXT("existing output: refused"), Second.bSucceeded);
			TestTrue(FString::Printf(TEXT("existing output: DSH9099 (%s)"), *DescribeResult(Second)), ResultHasCode(Second, TEXT("DSH9099")));
			TestTrue(TEXT("existing output: the source stays"), FileManager.FileExists(*Fixture.GetSourceFilePath()));
			TestFalse(TEXT("existing output: the error line is filled"), Second.Error.IsEmpty());
		}

		FileManager.Delete(*Result.OutputFilePath, false, true, true);
		if (!Result.BackupFilePath.IsEmpty())
		{
			FileManager.Delete(*Result.BackupFilePath, false, true, true);
		}
	}

	// -NoBackup: the source is deleted.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_MtNoBackup"), TEXT("Migrate2"), TEXT("dsm"));
		if (!Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtNoBackup"))))
		{
			return false;
		}
		FDreamShaderMigrateOptions Options;
		Options.bNoBackup = true;
		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), Options, Result);
		TestTrue(FString::Printf(TEXT("no backup: succeeds (%s)"), *DescribeResult(Result)), Result.bSucceeded);
		TestTrue(TEXT("no backup: no backup path"), Result.BackupFilePath.IsEmpty());
		TestFalse(TEXT("no backup: the `.dsm` is gone"), FileManager.FileExists(*Fixture.GetSourceFilePath()));
		TestTrue(TEXT("no backup: the `.dss` exists"), FileManager.FileExists(*Result.OutputFilePath));
		FileManager.Delete(*Result.OutputFilePath, false, true, true);
	}

	// -Out: written elsewhere, mirrored under the directory; the source stays and is named as its own backup.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_MtOut"), TEXT("Migrate2"), TEXT("dsm"));
		if (!Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtOut"))))
		{
			return false;
		}
		const FString OutDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Tests"), TEXT("MigrateOut")));
		FileManager.DeleteDirectory(*OutDirectory, false, true);

		FDreamShaderMigrateOptions Options;
		Options.OutputDirectory = OutDirectory;
		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), Options, Result);
		TestTrue(FString::Printf(TEXT("-Out: succeeds (%s)"), *DescribeResult(Result)), Result.bSucceeded);
		TestTrue(TEXT("-Out: the output is under the directory"), Result.OutputFilePath.StartsWith(UE::DreamShader::NormalizeSourceFilePath(OutDirectory), ESearchCase::IgnoreCase));
		TestTrue(TEXT("-Out: mirrored under the source root"), Result.OutputFilePath.EndsWith(TEXT("DreamShaderTests/Migrate2/M_MtOut/M_MtOut.dss"), ESearchCase::IgnoreCase));
		TestTrue(TEXT("-Out: the output exists"), FileManager.FileExists(*Result.OutputFilePath));
		TestTrue(TEXT("-Out: the source stays"), FileManager.FileExists(*Fixture.GetSourceFilePath()));
		TestTrue(TEXT("-Out: the source is its own backup"), Result.BackupFilePath.Equals(Result.SourceFilePath, ESearchCase::IgnoreCase));
		FileManager.DeleteDirectory(*OutDirectory, false, true);
	}
	return true;
}

namespace UE::DreamShader::Editor::Private::MigrateTests
{
	/** Every line logged while it lives; the test base suppresses logged errors, so they are read back here. */
	class FCapturedLog : public FOutputDevice
	{
	public:
		FCapturedLog() { GLog->AddOutputDevice(this); }
		virtual ~FCapturedLog() override { GLog->RemoveOutputDevice(this); }

		using FOutputDevice::Serialize;
		virtual void Serialize(const TCHAR* Line, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			Lines.Add(Line);
		}

		/** How many lines start with Prefix. */
		int32 Count(const TCHAR* Prefix)
		{
			GLog->FlushThreadedLogs();
			return Lines.FilterByPredicate([Prefix](const FString& Line) { return Line.StartsWith(Prefix); }).Num();
		}

	private:
		TArray<FString> Lines;
	};
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateCommandletFlagsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Migrate.CommandletFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateCommandletFlagsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	// Through the commandlet's own Main, so each flag arrives where the engine's parse puts it: `-Check=true` in the
	// param map, not in the switch list. A `-Check` that is not read is a migration that writes.
	FDreamShaderCompile2Fixture Fixture(TEXT("M_MtFlags"), TEXT("Migrate2"), TEXT("dsm"));
	if (!Fixture.WriteSource(*this, MakeLegacyMaterial(Fixture, TEXT("M_MtFlags"))))
	{
		return false;
	}

	IFileManager& FileManager = IFileManager::Get();
	const FString Output = FPaths::ChangeExtension(Fixture.GetSourceFilePath(), TEXT("dss"));
	UDreamShaderCommandlet* const Commandlet = NewObject<UDreamShaderCommandlet>();
	const auto Run = [&Fixture, Commandlet](const TCHAR* Flags)
	{
		return Commandlet->Main(FString::Printf(TEXT("migrate -Source=\"%s\" %s"), *Fixture.GetSourceFilePath(), Flags));
	};
	const auto ExpectUntouched = [this, &FileManager, &Fixture, &Output](const TCHAR* Flags)
	{
		TestFalse(FString::Printf(TEXT("%s: no `.dss` was written"), Flags), FileManager.FileExists(*Output));
		TestTrue(FString::Printf(TEXT("%s: the source is where it was"), Flags), FileManager.FileExists(*Fixture.GetSourceFilePath()));
	};

	for (const TCHAR* const Flags : { TEXT("-Check=true"), TEXT("-Check=1"), TEXT("-DryRun=yes"), TEXT("-Check=false -DryRun=On") })
	{
		TestEqual(FString::Printf(TEXT("%s: the check exits 0"), Flags), Run(Flags), 0);
		ExpectUntouched(Flags);
	}

	// A value that says neither: DSH9110, and the run does nothing -- no migration, and no check either.
	for (const TCHAR* const Flags : { TEXT("-Check=banana"), TEXT("-NoBackup=") })
	{
		FCapturedLog Log;
		TestEqual(FString::Printf(TEXT("%s: the run exits 1"), Flags), Run(Flags), 1);
		// Unlocated: a flag is on the command line, and no file is to blame for it.
		TestEqual(FString::Printf(TEXT("%s: DSH9110 is logged once, with no location"), Flags), Log.Count(TEXT("DSH9110: '-")), 1);
		TestEqual(FString::Printf(TEXT("%s: no file was looked at"), Flags), Log.Count(TEXT("Checked '")), 0);
		ExpectUntouched(Flags);
	}
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMigrateSourceRefusalsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Migrate.Refusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMigrateSourceRefusalsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::MigrateTests;

	IFileManager& FileManager = IFileManager::Get();

	// `#if`: only one branch would survive.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_MtConditional"), TEXT("Migrate2"), TEXT("dsm"));
		const FString Source = FString(TEXT("#if DS_NEVER_DEFINED_FOR_TESTS\n#endif\n")) + MakeLegacyMaterial(Fixture, TEXT("M_MtConditional"));
		if (!Fixture.WriteSource(*this, Source))
		{
			return false;
		}
		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Result);
		TestFalse(TEXT("#if: refused"), Result.bSucceeded);
		TestTrue(FString::Printf(TEXT("#if: DSH9090 (%s)"), *DescribeResult(Result)), ResultHasCode(Result, TEXT("DSH9090")));
		TestTrue(TEXT("#if: the source stays"), FileManager.FileExists(*Fixture.GetSourceFilePath()));
		TestFalse(TEXT("#if: no `.dss`"), FileManager.FileExists(*FPaths::ChangeExtension(Fixture.GetSourceFilePath(), TEXT("dss"))));
	}

	// Not a 1.x source.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_MtAlready"), TEXT("Migrate2"), TEXT("dss"));
		if (!Fixture.WriteSource(*this, TEXT("export void M_MtAlready(inout material m)\n{\n    m.Opacity = 1;\n}\n")))
		{
			return false;
		}
		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Result);
		TestFalse(TEXT(".dss: refused"), Result.bSucceeded);
		TestTrue(FString::Printf(TEXT(".dss: DSH9095 (%s)"), *DescribeResult(Result)), ResultHasCode(Result, TEXT("DSH9095")));
	}

	// A header with nothing 1.x in it, and one with a Function block: rewritten in place, the old text backed up.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("MtHeader"), TEXT("Migrate2"), TEXT("dsh"));
		if (!Fixture.WriteSource(*this, TEXT("float Twice(float x)\n{\n    return x * 2.0;\n}\n")))
		{
			return false;
		}
		FDreamShaderMigrateResult Nothing;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Nothing);
		TestFalse(TEXT("2.0 header: nothing to migrate"), Nothing.bSucceeded);
		TestTrue(FString::Printf(TEXT("2.0 header: DSH9093 (%s)"), *DescribeResult(Nothing)), ResultHasCode(Nothing, TEXT("DSH9093")));

		if (!Fixture.WriteSource(*this, TEXT("// header comment\nFunction Remap01(in float value, out float result) {\n    result = saturate(value * 0.5 + 0.5);\n}\n")))
		{
			return false;
		}
		FDreamShaderMigrateResult Result;
		MigrateDreamShaderSource(Fixture.GetSourceFilePath(), FDreamShaderMigrateOptions(), Result);
		TestTrue(FString::Printf(TEXT("1.x header: succeeds (%s)"), *DescribeResult(Result)), Result.bSucceeded);
		TestTrue(TEXT("1.x header: written in place"), Result.OutputFilePath.Equals(Result.SourceFilePath, ESearchCase::IgnoreCase));
		TestTrue(TEXT("1.x header: still there"), FileManager.FileExists(*Fixture.GetSourceFilePath()));

		FString Written;
		FFileHelper::LoadFileToString(Written, *Fixture.GetSourceFilePath());
		TestTrue(FString::Printf(TEXT("1.x header: the block is a `@custom` function now\n%s"), *Written), Written.Contains(TEXT("@custom")) && !Written.Contains(TEXT("Function Remap01")));
		TestTrue(TEXT("1.x header: the comment is kept"), Written.Contains(TEXT("// header comment")));
		if (!Result.BackupFilePath.IsEmpty())
		{
			FString Backup;
			FFileHelper::LoadFileToString(Backup, *Result.BackupFilePath);
			TestTrue(TEXT("1.x header: the backup holds the 1.x text"), Backup.Contains(TEXT("Function Remap01")));
			FileManager.Delete(*Result.BackupFilePath, false, true, true);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
