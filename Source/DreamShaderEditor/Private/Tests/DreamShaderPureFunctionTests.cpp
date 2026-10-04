// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Pure-function quick layer: zero editor/world/asset dependency, runs in milliseconds. Covers the
// header-declared pure helpers used by diagnostics, import resolution, and the compile commandlet.
// These gate PRs alongside the parse corpus (DreamShader.Lang.* / DreamShader.Commandlet.Args.*).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commandlet/DreamShaderCommandletRunner.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderDependencyGraphService.h"
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "DreamShaderTextWireUtils.h"
#include "Lang/LangDiagnostic.h"

#include "Commandlets/Commandlet.h"
#include "Internationalization/Culture.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "DreamShaderTests"

// ---------------------------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderParseErrorLocationTest,
	"DreamShader.Lang.Diagnostics.ParseErrorLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderParseErrorLocationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	// Standard "File(Line,Col): message" form.
	{
		FDreamShaderDiagnosticLocation Location;
		const bool bParsed = FDreamShaderDiagnosticsStore::TryParseErrorLocation(
			TEXT("C:/Proj/DShader/M_Foo.dsm(17,9): Unexpected token 'f' in Graph expression."), Location);
		TestTrue(TEXT("located error parses"), bParsed);
		TestEqual(TEXT("line"), Location.Line, 17);
		TestEqual(TEXT("column"), Location.Column, 9);
		TestTrue(TEXT("file path retained"), Location.FilePath.Contains(TEXT("M_Foo.dsm")));
		TestEqual(TEXT("message"), ToInvariantWireString(Location.Message), FString(TEXT("Unexpected token 'f' in Graph expression.")));
	}

	// Line/Column are clamped to a minimum of 1.
	{
		FDreamShaderDiagnosticLocation Location;
		const bool bParsed = FDreamShaderDiagnosticsStore::TryParseErrorLocation(TEXT("X.dsm(0,0): boom"), Location);
		TestTrue(TEXT("zero location parses"), bParsed);
		TestEqual(TEXT("line clamped"), Location.Line, 1);
		TestEqual(TEXT("column clamped"), Location.Column, 1);
	}

	// A line without the "): " marker, or with non-numeric coordinates, is not a located error.
	{
		FDreamShaderDiagnosticLocation Location;
		TestFalse(TEXT("plain text is not located"), FDreamShaderDiagnosticsStore::TryParseErrorLocation(TEXT("just a message"), Location));
		TestFalse(TEXT("non-numeric coords rejected"), FDreamShaderDiagnosticsStore::TryParseErrorLocation(TEXT("X.dsm(a,b): boom"), Location));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBuildGenerateDiagnosticsTest,
	"DreamShader.Lang.Diagnostics.BuildGenerateDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderBuildGenerateDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	const FString SourceFilePath = TEXT("C:/Proj/DShader/M_Foo.dsm");
	const FString Message =
		FString(TEXT("C:/Proj/DShader/M_Foo.dsm(12,5): Unsupported swizzle '.q'.")) + LINE_TERMINATOR
		+ TEXT("Generation aborted.");

	const TArray<FDreamShaderDiagnosticRecord> Records =
		FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics(SourceFilePath, Message);

	if (!TestEqual(TEXT("two diagnostic records"), Records.Num(), 2))
	{
		return false;
	}

	TestEqual(TEXT("located record line"), Records[0].Line, 12);
	TestEqual(TEXT("located record column"), Records[0].Column, 5);
	TestEqual(TEXT("located record message"), ToInvariantWireString(Records[0].Message), FString(TEXT("Unsupported swizzle '.q'.")));
	TestEqual(TEXT("located record stage"), Records[0].Stage, FString(TEXT("generate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextWireUtilsTest,
	"DreamShader.Lang.Diagnostics.TextWireUtils",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextWireUtilsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	// The diagnostics wire is parsed by VSCode/Rider tooling and must be identical under every
	// editor culture, so the whole suite runs once per culture; the editor culture is restored
	// when the test exits.
	//
	// Snapshot/restore rather than SetCurrentCulture(GetCurrentCulture()->GetName()): a culture is
	// three settings -- language, locale and asset group -- and reading one name back cannot
	// reconstruct them. This is the engine's own API for the job.
	//
	// It does NOT fix Parse.Lexical.L_UnterminatedBlock, which fails on any editor whose culture is
	// not English because the corpus asserts the English message text and the zh-Hans locres
	// translates that particular string. That failure reproduces with this test excluded entirely,
	// so do not read the two as related.
	FInternationalization::FCultureStateSnapshot CultureSnapshot;
	FInternationalization::Get().BackupCultureState(CultureSnapshot);
	ON_SCOPE_EXIT
	{
		FInternationalization::Get().RestoreCultureState(CultureSnapshot);
	};

	auto AssertWireInvariance = [this](const FString& CultureName)
	{
		const bool bSetCulture = FInternationalization::Get().SetCurrentCulture(CultureName);
		TestTrue(TEXT("culture set"), bSetCulture);

		// Plain LOCTEXT: the source string is the English literal regardless of display culture.
		TestEqual(
			TEXT("plain LOCTEXT source"),
			ToInvariantWireString(LOCTEXT("WireUtils.Plain", "Generation aborted.")),
			FString(TEXT("Generation aborted.")));

		// FText::FromString dynamic text: the raw input IS the wire string.
		TestEqual(
			TEXT("dynamic text source"),
			ToInvariantWireString(FText::FromString(TEXT("Unexpected token 'f' in Graph expression."))),
			FString(TEXT("Unexpected token 'f' in Graph expression.")));

		// Ordered FText::Format with a dynamic text argument and an int64: the English LOCTEXT
		// pattern survives, the dynamic argument is substituted, and the number is rendered with
		// the invariant culture (no thousands separator) even though the localized display of the
		// same FText is "12,345" under both en-US and zh-Hans.
		{
			const FText Formatted = FText::Format(
				LOCTEXT("WireUtils.Ordered", "Unsupported swizzle {0} at line {1}."),
				FText::FromString(TEXT(".q")),
				(int64)12345);
			const FString Wire = ToInvariantWireString(Formatted);
			TestEqual(
				TEXT("ordered format wire is invariant"),
				Wire,
				FString(TEXT("Unsupported swizzle .q at line 12345.")));
			TestFalse(TEXT("no thousands separator in wire"), Wire.Contains(TEXT("12,345")));
			// Sanity check that the test is discriminating: the localized display of the same
			// FText really does use the culture-grouped number.
			TestTrue(TEXT("localized display groups the number"), Formatted.ToString().Contains(TEXT("12,345")));
		}

		// Nested format: an FText::Format result used as an argument is recursively rebuilt.
		{
			const FText Inner = FText::Format(
				LOCTEXT("WireUtils.Inner", "inner {0}"),
				FText::FromString(TEXT("deep")));
			const FText Outer = FText::Format(
				LOCTEXT("WireUtils.Outer", "outer [{0}] end"),
				Inner);
			TestEqual(
				TEXT("nested format wire is invariant"),
				ToInvariantWireString(Outer),
				FString(TEXT("outer [inner deep] end")));
		}

		// Float argument keeps the invariant decimal point.
		{
			const FText Formatted = FText::Format(
				LOCTEXT("WireUtils.Float", "value {0}"),
				3.14);
			TestEqual(
				TEXT("float wire is invariant"),
				ToInvariantWireString(Formatted),
				FString(TEXT("value 3.14")));
		}
	};

	AssertWireInvariance(TEXT("en-US"));
	AssertWireInvariance(TEXT("zh-Hans"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDiagnosticsNoFileTest,
	"DreamShader.Lang.Diagnostics.NoFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDiagnosticsNoFileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;

	// A diagnostic of no file -- a commandlet's argument error -- with no path to fall back on either. The empty path
	// used to be made absolute, into the engine's Binaries directory, and printed as its location.
	UE::DreamShader::Lang::FLangDiagnosticSink Sink;
	Sink.Error(TEXT("DSH9041"), UE::DreamShader::Lang::FLangSpan(), FText::FromString(TEXT("probe")));

	TArray<FLang2DiagnosticRecord> Records;
	BuildLang2DiagnosticRecords(Sink, FString(), Records);
	TestEqual(TEXT("one record"), Records.Num(), 1);
	if (Records.Num() == 1)
	{
		TestTrue(TEXT("the record names no file, so the store files it under the compiled source"), Records[0].Record.FilePath.IsEmpty());
		TestEqual(TEXT("its detail is the diagnostic alone"), Records[0].Record.Detail.ToString(), FString(TEXT("DSH9041: probe")));
	}

	UE::DreamShader::FDreamShaderError Error;
	TestTrue(TEXT("the compile error is built"), BuildLang2CompileError(Sink, FString(), Error));
	TestEqual(TEXT("its message is the diagnostic alone"), Error.Message, FString(TEXT("DSH9041: probe")));

	// A path still locates it.
	BuildLang2DiagnosticRecords(Sink, TEXT("C:/Probe/M_Probe.dss"), Records);
	if (Records.Num() == 1)
	{
		const FString Detail = Records[0].Record.Detail.ToString();
		TestTrue(FString::Printf(TEXT("a path locates it (%s)"), *Detail), Detail.EndsWith(TEXT("M_Probe.dss(1,1): DSH9041: probe")));
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// Import resolution
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderExtractImportPathTest,
	"DreamShader.Lang.Import.ExtractImportPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderExtractImportPathTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	auto Extract = [](const TCHAR* Line, FString& OutPath)
	{
		return FDreamShaderDependencyGraphService::TryExtractImportPathFromLine(Line, OutPath);
	};

	FString Path;
	TestTrue(TEXT("double-quoted with semicolon"), Extract(TEXT("import \"Shared/Common.dsh\";"), Path));
	TestEqual(TEXT("path value"), Path, FString(TEXT("Shared/Common.dsh")));

	TestTrue(TEXT("no trailing semicolon"), Extract(TEXT("import \"Functions/F.dsf\""), Path));
	TestEqual(TEXT("dsf path"), Path, FString(TEXT("Functions/F.dsf")));

	TestTrue(TEXT("single-quoted"), Extract(TEXT("import 'X.dsh';"), Path));
	TestEqual(TEXT("single-quoted path"), Path, FString(TEXT("X.dsh")));

	TestTrue(TEXT("trailing comment allowed"), Extract(TEXT("import \"A.dsh\"; // note"), Path));

	TestFalse(TEXT("commented import ignored"), Extract(TEXT("// import \"X.dsh\";"), Path));
	TestFalse(TEXT("no space after keyword"), Extract(TEXT("importX \"Y.dsh\";"), Path));
	TestFalse(TEXT("unquoted path rejected"), Extract(TEXT("import Shared/Common.dsh;"), Path));
	TestFalse(TEXT("non-import line"), Extract(TEXT("Shader(Name=\"M\")"), Path));
	TestFalse(TEXT("trailing junk rejected"), Extract(TEXT("import \"X.dsh\" garbage"), Path));
	// In 1.x a `#include` is HLSL inside a `Function` body, never an import.
	TestFalse(TEXT("#include is not a 1.x import"), Extract(TEXT("#include \"/Plugin/DreamShader/DreamShaderBuiltins.ush\""), Path));
	TestFalse(TEXT("#include of a header is not a 1.x import either"), Extract(TEXT("#include \"Shared/Common.dsh\""), Path));

	auto ExtractInclude = [](const TCHAR* Line, FString& OutPath)
	{
		return FDreamShaderDependencyGraphService::TryExtractIncludePathFromLine(Line, OutPath);
	};

	TestTrue(TEXT("2.0 include of a header"), ExtractInclude(TEXT("#include \"Shared/Common.dsh\""), Path));
	TestEqual(TEXT("2.0 include path"), Path, FString(TEXT("Shared/Common.dsh")));
	TestTrue(TEXT("space after # and no extension"), ExtractInclude(TEXT("# include \"Noise\""), Path));
	TestEqual(TEXT("extensionless include path"), Path, FString(TEXT("Noise")));
	TestFalse(TEXT("HLSL include is not an edge"), ExtractInclude(TEXT("#include \"/Engine/Private/Common.ush\""), Path));
	TestFalse(TEXT("import is not an include"), ExtractInclude(TEXT("import \"X.dsh\";"), Path));
	TestFalse(TEXT("#pragma is not an include"), ExtractInclude(TEXT("#pragma material(BlendMode = Masked)"), Path));
	TestFalse(TEXT("commented include ignored"), ExtractInclude(TEXT("// #include \"X.dsh\""), Path));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderNormalizeImportSpecifierTest,
	"DreamShader.Lang.Import.NormalizeSpecifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderNormalizeImportSpecifierTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	auto Normalize = [](const TCHAR* Spec)
	{
		return FDreamShaderDependencyGraphService::NormalizeImportSpecifier(Spec);
	};

	TestEqual(TEXT("extensionless defaults to .dsh"), Normalize(TEXT("Shared/Common")), FString(TEXT("Shared/Common.dsh")));
	TestEqual(TEXT("explicit .dsf preserved"), Normalize(TEXT("Functions/F.dsf")), FString(TEXT("Functions/F.dsf")));
	TestEqual(TEXT("backslashes and ./ stripped"), Normalize(TEXT("./Shared\\Common.dsh")), FString(TEXT("Shared/Common.dsh")));
	TestEqual(TEXT("bare name gets .dsh"), Normalize(TEXT("Noise")), FString(TEXT("Noise.dsh")));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Commandlet argument parsing
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCommandletArgParsingTest,
	"DreamShader.Commandlet.Args.SplitAndGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCommandletArgParsingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	// NormalizeCommandletKey strips leading dashes; NormalizeCommandletValue strips quotes/whitespace.
	TestEqual(TEXT("key strips dashes"), NormalizeCommandletKey(TEXT("--Force")), FString(TEXT("Force")));
	TestEqual(TEXT("value strips quotes"), NormalizeCommandletValue(TEXT("  \"a value\"  ")), FString(TEXT("a value")));

	// TrySplitCommandletAssignment.
	{
		FString Key, Value;
		TestTrue(TEXT("assignment splits"), TrySplitCommandletAssignment(TEXT("-Source=C:/x.dsm"), Key, Value));
		TestEqual(TEXT("split key"), Key, FString(TEXT("Source")));
		TestEqual(TEXT("split value"), Value, FString(TEXT("C:/x.dsm")));

		TestFalse(TEXT("no equals -> no split"), TrySplitCommandletAssignment(TEXT("Force"), Key, Value));
	}

	// The lists come from the parse UDreamShaderCommandlet::Main makes, never by hand: a hand-built switch list can hold
	// `Force=true`, which the engine never leaves there, and a lookup that reads only that list passes against it.
	{
		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> Params;
		UCommandlet::ParseCommandLine(TEXT("compile Mode=compile -Source=\"C:/x.dsm\" -Force=true -All"), Tokens, Switches, Params);

		// What the engine does with them, and the reason every lookup takes Params.
		TestEqual(TEXT("the verb is the first token"), Tokens.IsEmpty() ? FString() : Tokens[0], FString(TEXT("compile")));
		TestTrue(TEXT("a bare switch stays in the switch list"), Switches.Contains(TEXT("All")));
		TestFalse(TEXT("an assigned switch leaves the switch list"), Switches.ContainsByPredicate([](const FString& Switch)
		{
			return Switch.StartsWith(TEXT("Force"), ESearchCase::IgnoreCase);
		}));
		TestEqual(TEXT("and lands in the map"), Params.FindRef(TEXT("Force")), FString(TEXT("true")));

		FString Value;
		TestTrue(TEXT("param from the map"), TryGetCommandletParam(Tokens, Switches, Params, TEXT("Source"), Value));
		TestEqual(TEXT("map value, quotes stripped"), Value, FString(TEXT("C:/x.dsm")));

		TestTrue(TEXT("param from a dashless token"), TryGetCommandletParam(Tokens, Switches, Params, TEXT("Mode"), Value));
		TestEqual(TEXT("token value"), Value, FString(TEXT("compile")));

		TestFalse(TEXT("missing param"), TryGetCommandletParam(Tokens, Switches, Params, TEXT("Nope"), Value));

		UE::DreamShader::Lang::FLangDiagnosticSink Sink;
		TestTrue(TEXT("an assigned flag is read from the map"), HasCommandletFlag(Tokens, Switches, Params, TEXT("Force"), Sink));
		TestTrue(TEXT("a bare flag is read from the switches"), HasCommandletFlag(Tokens, Switches, Params, TEXT("All"), Sink));
		TestFalse(TEXT("an absent flag is off"), HasCommandletFlag(Tokens, Switches, Params, TEXT("Check"), Sink));
		TestEqual(TEXT("and none of the three is an error"), Sink.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCommandletFlagValuesTest,
	"DreamShader.Commandlet.Args.FlagValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCommandletFlagValuesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	enum class EExpect : uint8 { Off, On, Error };
	struct FCase
	{
		const TCHAR* CommandLine;
		EExpect Expect;
	};

	// Docs/tools/commandlet.md "Boolean flags", row by row, as a command line reaches Main.
	const FCase Cases[] = {
		{ TEXT("compile -All"), EExpect::Off },
		{ TEXT("compile -Force"), EExpect::On },
		{ TEXT("compile --force"), EExpect::On },
		{ TEXT("compile -Force=1"), EExpect::On },
		{ TEXT("compile -Force=true"), EExpect::On },
		{ TEXT("compile -force=TRUE"), EExpect::On },
		{ TEXT("compile -Force=yes"), EExpect::On },
		{ TEXT("compile -Force=On"), EExpect::On },
		{ TEXT("compile -Force=\"true\""), EExpect::On },
		{ TEXT("compile Force=true"), EExpect::On },
		{ TEXT("compile -Force=0"), EExpect::Off },
		{ TEXT("compile -Force=false"), EExpect::Off },
		{ TEXT("compile -Force=No"), EExpect::Off },
		{ TEXT("compile -Force=OFF"), EExpect::Off },
		{ TEXT("compile --Force=0"), EExpect::Off },
		{ TEXT("compile Force=false"), EExpect::Off },
		{ TEXT("compile -Force=banana"), EExpect::Error },
		{ TEXT("compile -Force=disable"), EExpect::Error },
		{ TEXT("compile -Force=2"), EExpect::Error },
		{ TEXT("compile -Force="), EExpect::Error },
		{ TEXT("compile -Force=\"\""), EExpect::Error },
		{ TEXT("compile Force=never"), EExpect::Error },
		// Another switch whose name starts the same is not this flag, and its value is not looked at.
		{ TEXT("compile -Forced=banana"), EExpect::Off },
	};

	for (const FCase& Case : Cases)
	{
		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> Params;
		UCommandlet::ParseCommandLine(Case.CommandLine, Tokens, Switches, Params);

		UE::DreamShader::Lang::FLangDiagnosticSink Sink;
		const bool bOn = HasCommandletFlag(Tokens, Switches, Params, TEXT("Force"), Sink);
		TestTrue(FString::Printf(TEXT("'%s': -Force is %s"), Case.CommandLine, Case.Expect == EExpect::On ? TEXT("on") : TEXT("off")), bOn == (Case.Expect == EExpect::On));
		TestEqual(FString::Printf(TEXT("'%s': errors raised"), Case.CommandLine), Sink.NumErrors(), Case.Expect == EExpect::Error ? 1 : 0);

		if (Case.Expect == EExpect::Error && Sink.FirstError())
		{
			const FString Wire = UE::DreamShader::Lang::FLangDiagnosticSink::ToWireString(*Sink.FirstError());
			TestEqual(FString::Printf(TEXT("'%s': the code"), Case.CommandLine), Sink.FirstError()->Code, FString(TEXT("DSH9110")));
			TestTrue(FString::Printf(TEXT("'%s': the message quotes the flag as read (%s)"), Case.CommandLine, *Wire), Wire.StartsWith(TEXT("DSH9110: '-Force=")));
		}
	}

	// The value is quoted back as written, so the user sees which flag said what.
	{
		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> Params;
		UCommandlet::ParseCommandLine(TEXT("migrate -All -Check=banana -DryRun=0"), Tokens, Switches, Params);

		UE::DreamShader::Lang::FLangDiagnosticSink Sink;
		TestFalse(TEXT("-Check=banana is off"), HasCommandletFlag(Tokens, Switches, Params, TEXT("Check"), Sink));
		TestFalse(TEXT("-DryRun=0 is off"), HasCommandletFlag(Tokens, Switches, Params, TEXT("DryRun"), Sink));
		TestTrue(TEXT("-All is on"), HasCommandletFlag(Tokens, Switches, Params, TEXT("All"), Sink));
		TestEqual(TEXT("one error, for -Check"), Sink.NumErrors(), 1);
		if (Sink.FirstError())
		{
			const FString Wire = UE::DreamShader::Lang::FLangDiagnosticSink::ToWireString(*Sink.FirstError());
			TestTrue(FString::Printf(TEXT("it names -Check=banana (%s)"), *Wire), Wire.Contains(TEXT("'-Check=banana'")));
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_DEV_AUTOMATION_TESTS
