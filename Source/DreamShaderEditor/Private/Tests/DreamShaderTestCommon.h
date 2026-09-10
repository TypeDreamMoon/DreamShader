// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Shared helpers for the DreamShader data-driven test corpus.
//
// The corpus lives on disk under <DreamShaderPlugin>/Tests/Corpus/<Layer>/ as a tree of
// .dsm / .dsf / .dsh / .dss fixtures, each optionally paired with a <name>.expected.json golden
// file.
// A small set of generic runners (one per pipeline entry point) enumerate the tree at runtime
// and assert each fixture against its golden — so adding coverage for a new keyword is just
// "drop a source file (+ optional json)", with no new C++ and no recompile.
//
// Run with -DreamShaderUpdateGolden to (re)write each golden from the actual parse result;
// review the resulting json/diff by hand before committing.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderParser.h"
#include "DreamShaderSettings.h"
#include "DreamShaderTypes.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	/** One discovered fixture: a source file plus its (possibly absent) golden. */
	struct FCorpusCase
	{
		FString SourcePath;          // absolute path to the .dsm/.dsf/.dsh/.dss fixture
		FString ExpectedPath;        // absolute path to <name>.expected.json (may not exist)
		FString RelativeName;        // path relative to the layer dir, extension stripped (for the test name)
		FString Extension;           // "dsm" / "dsf" / "dsh" / "dss"
		bool bBadByName = false;     // filename contains ".bad." -> default expectation is "parse fails"
		bool bHasExpectationFile = false;
	};

	/** Declarative expectation decoded from a <name>.expected.json. Every field is opt-in. */
	struct FCorpusExpectation
	{
		bool bExpectError = false;                 // outcome: "error" (or .bad. filename)
		TArray<FString> ErrorContains;             // substrings that must appear in the parse error
		TArray<FString> WarningsContain;           // substrings that must appear among Definition.Warnings

		bool bCheckName = false;                   FString Name;
		TMap<FString, FString> Settings;           // key (case-insensitive) -> exact value, asserted via TryGetSetting
		bool bCheckOutputDeclarations = false;     int32 OutputDeclarations = 0;
		bool bCheckOutputs = false;                int32 Outputs = 0;
		bool bCheckMaterialFunctions = false;      int32 MaterialFunctions = 0;
		bool bCheckMaterialFunction0Kind = false;  FString MaterialFunction0Kind; // "ShaderFunction"/"ShaderLayer"/"ShaderLayerBlend"
		bool bCheckVirtualFunctions = false;       int32 VirtualFunctions = 0;
		bool bCheckCodeNotEmpty = false;           bool bCodeNotEmpty = true;
	};

	/** True when the run was launched with -DreamShaderUpdateGolden. */
	inline bool ShouldUpdateDreamShaderGolden()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("DreamShaderUpdateGolden"));
	}

	/** Plugin-relative root of the test corpus (outside the project's DShader source tree). */
	inline FString GetDreamShaderCorpusRoot()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Tests"), TEXT("Corpus")));
		}
		return FString();
	}

	/** Reconstruct a case from just its source path (the only thing threaded through RunTest). */
	inline FCorpusCase MakeDreamShaderCorpusCase(const FString& SourcePath)
	{
		FCorpusCase Case;
		Case.SourcePath = FPaths::ConvertRelativePathToFull(SourcePath);
		Case.Extension = FPaths::GetExtension(Case.SourcePath);
		Case.bBadByName = FPaths::GetCleanFilename(Case.SourcePath).Contains(TEXT(".bad."), ESearchCase::IgnoreCase);
		Case.ExpectedPath = FPaths::GetBaseFilename(Case.SourcePath, false) + TEXT(".expected.json");
		Case.bHasExpectationFile = IFileManager::Get().FileExists(*Case.ExpectedPath);
		return Case;
	}

	/** Enumerate every .dsm/.dsf/.dsh/.dss fixture under Corpus/<SubDir>. */
	inline bool LoadDreamShaderCorpusCases(const FString& SubDir, TArray<FCorpusCase>& OutCases)
	{
		const FString Root = GetDreamShaderCorpusRoot();
		if (Root.IsEmpty())
		{
			return false;
		}

		const FString Dir = FPaths::Combine(Root, SubDir);
		IFileManager& FM = IFileManager::Get();

		TArray<FString> Files;
		FM.FindFilesRecursive(Files, *Dir, TEXT("*.dsm"), true, false, false);
		FM.FindFilesRecursive(Files, *Dir, TEXT("*.dsf"), true, false, false);
		FM.FindFilesRecursive(Files, *Dir, TEXT("*.dsh"), true, false, false);
		FM.FindFilesRecursive(Files, *Dir, TEXT("*.dss"), true, false, false);
		Files.Sort();

		const FString RelativeBase = Dir / TEXT("");
		for (const FString& File : Files)
		{
			FCorpusCase Case = MakeDreamShaderCorpusCase(File);

			FString Relative = Case.SourcePath;
			FPaths::MakePathRelativeTo(Relative, *RelativeBase);
			Case.RelativeName = FPaths::GetBaseFilename(Relative, false);

			OutCases.Add(MoveTemp(Case));
		}
		return true;
	}

	/** Decode a golden json into an FCorpusExpectation. Returns false only on malformed json. */
	inline bool ParseDreamShaderExpectation(const FString& JsonText, FCorpusExpectation& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("invalid JSON");
			return false;
		}

		FString Outcome;
		if (Root->TryGetStringField(TEXT("outcome"), Outcome))
		{
			Out.bExpectError = Outcome.Equals(TEXT("error"), ESearchCase::IgnoreCase);
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Root->TryGetArrayField(TEXT("errorContains"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.ErrorContains.Add(Value->AsString());
			}
		}
		if (Root->TryGetArrayField(TEXT("warningsContain"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.WarningsContain.Add(Value->AsString());
			}
		}
		// "messageContains" is the Generate-layer alias: substrings asserted against the
		// generator's OutMessage (whether the outcome is ok or error). Folded into ErrorContains.
		if (Root->TryGetArrayField(TEXT("messageContains"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.ErrorContains.Add(Value->AsString());
			}
		}

		const TSharedPtr<FJsonObject>* Def = nullptr;
		if (Root->TryGetObjectField(TEXT("definition"), Def))
		{
			FString StringValue;
			double NumberValue = 0.0;
			bool BoolValue = false;

			if ((*Def)->TryGetStringField(TEXT("name"), StringValue)) { Out.bCheckName = true; Out.Name = StringValue; }
			if ((*Def)->TryGetNumberField(TEXT("outputDeclarations"), NumberValue)) { Out.bCheckOutputDeclarations = true; Out.OutputDeclarations = static_cast<int32>(NumberValue); }
			if ((*Def)->TryGetNumberField(TEXT("outputs"), NumberValue)) { Out.bCheckOutputs = true; Out.Outputs = static_cast<int32>(NumberValue); }
			if ((*Def)->TryGetNumberField(TEXT("materialFunctions"), NumberValue)) { Out.bCheckMaterialFunctions = true; Out.MaterialFunctions = static_cast<int32>(NumberValue); }
			if ((*Def)->TryGetStringField(TEXT("materialFunction0Kind"), StringValue)) { Out.bCheckMaterialFunction0Kind = true; Out.MaterialFunction0Kind = StringValue; }
			if ((*Def)->TryGetNumberField(TEXT("virtualFunctions"), NumberValue)) { Out.bCheckVirtualFunctions = true; Out.VirtualFunctions = static_cast<int32>(NumberValue); }
			if ((*Def)->TryGetBoolField(TEXT("codeNotEmpty"), BoolValue)) { Out.bCheckCodeNotEmpty = true; Out.bCodeNotEmpty = BoolValue; }

			const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
			if ((*Def)->TryGetObjectField(TEXT("settings"), SettingsObject))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*SettingsObject)->Values)
				{
					FString Value;
					if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
					{
						Out.Settings.Add(Pair.Key, Value);
					}
				}
			}
		}

		return true;
	}

	/** Serialize a baseline golden from an actual parse result (used by -DreamShaderUpdateGolden). */
	inline FString BuildDreamShaderGoldenJson(bool bParsed, const FTextShaderDefinition& Definition, const FString& Error)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("parse"));
		Root->SetStringField(TEXT("outcome"), bParsed ? TEXT("ok") : TEXT("error"));

		if (!bParsed)
		{
			TArray<TSharedPtr<FJsonValue>> Errors;
			Errors.Add(MakeShared<FJsonValueString>(Error));
			Root->SetArrayField(TEXT("errorContains"), Errors);
		}
		else
		{
			const TSharedRef<FJsonObject> Def = MakeShared<FJsonObject>();
			if (!Definition.Name.IsEmpty())
			{
				Def->SetStringField(TEXT("name"), Definition.Name);
			}
			Def->SetNumberField(TEXT("outputDeclarations"), Definition.OutputDeclarations.Num());
			Def->SetNumberField(TEXT("outputs"), Definition.Outputs.Num());
			Def->SetNumberField(TEXT("materialFunctions"), Definition.MaterialFunctions.Num());
			if (Definition.MaterialFunctions.Num() > 0)
			{
				Def->SetStringField(TEXT("materialFunction0Kind"), LexToString(Definition.MaterialFunctions[0].Kind));
			}
			Def->SetNumberField(TEXT("virtualFunctions"), Definition.VirtualFunctions.Num());
			Def->SetBoolField(TEXT("codeNotEmpty"), !Definition.Code.IsEmpty());

			if (Definition.Settings.Num() > 0)
			{
				const TSharedRef<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
				for (const TPair<FString, FString>& Pair : Definition.Settings)
				{
					SettingsObject->SetStringField(Pair.Key, Pair.Value);
				}
				Def->SetObjectField(TEXT("settings"), SettingsObject);
			}

			Root->SetObjectField(TEXT("definition"), Def);

			if (Definition.Warnings.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> Warnings;
				for (const FString& Warning : Definition.Warnings)
				{
					Warnings.Add(MakeShared<FJsonValueString>(Warning));
				}
				Root->SetArrayField(TEXT("warningsContain"), Warnings);
			}
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Run one corpus case through FTextShaderParser::Parse and assert it against its golden.
	 * In -DreamShaderUpdateGolden mode it rewrites the golden instead of asserting.
	 * Returns false only on a hard I/O failure; semantic mismatches are recorded on Test.
	 */
	inline bool RunDreamShaderParseCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FTextShaderDefinition Definition;
		// Parse through the code-carrying overload and fold the DSHnnnn code into the string the
		// golden's errorContains is matched against. The code is the stable half of a diagnostic --
		// the message is free to be reworded and, eventually, translated -- so a negative fixture
		// should be able to name the code instead of English prose. Purely additive: a golden that
		// still names a message substring keeps matching.
		FDreamShaderTextError ParseError;
		const bool bParsed = FTextShaderParser::Parse(Source, Definition, ParseError);
		const FString Error = ParseError.HasCode()
			? FString::Printf(TEXT("%s: %s"), *ParseError.Code, *ParseError.Message.ToString())
			: ParseError.Message.ToString();

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderGoldenJson(bParsed, Definition, Error);
			if (FFileHelper::SaveStringToFile(Json, *Case.ExpectedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddInfo(FString::Printf(TEXT("Updated golden '%s'."), *Case.ExpectedPath));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to write golden '%s'."), *Case.ExpectedPath));
			}
			return true;
		}

		FCorpusExpectation Expectation;
		Expectation.bExpectError = Case.bBadByName; // default; json may override
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
				return false;
			}

			FCorpusExpectation Loaded;
			Loaded.bExpectError = Case.bBadByName;
			FString JsonError;
			if (!ParseDreamShaderExpectation(JsonText, Loaded, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
			Expectation = MoveTemp(Loaded);
		}

		if (Expectation.bExpectError)
		{
			Test.TestFalse(FString::Printf(TEXT("[%s] parse should FAIL"), *Case.SourcePath), bParsed);
			for (const FString& Needle : Expectation.ErrorContains)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] error contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *Error),
					Error.Contains(Needle, ESearchCase::IgnoreCase));
			}
			return true;
		}

		if (!bParsed)
		{
			Test.AddError(FString::Printf(TEXT("[%s] parse should SUCCEED but failed: %s"), *Case.SourcePath, *Error));
			return false;
		}

		if (Expectation.bCheckName)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] name"), *Case.SourcePath), Definition.Name, Expectation.Name);
		}
		if (Expectation.bCheckOutputDeclarations)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] outputDeclarations"), *Case.SourcePath), Definition.OutputDeclarations.Num(), Expectation.OutputDeclarations);
		}
		if (Expectation.bCheckOutputs)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] outputs"), *Case.SourcePath), Definition.Outputs.Num(), Expectation.Outputs);
		}
		if (Expectation.bCheckMaterialFunctions)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] materialFunctions"), *Case.SourcePath), Definition.MaterialFunctions.Num(), Expectation.MaterialFunctions);
		}
		if (Expectation.bCheckMaterialFunction0Kind && Definition.MaterialFunctions.Num() > 0)
		{
			Test.TestEqual(
				FString::Printf(TEXT("[%s] materialFunction0Kind"), *Case.SourcePath),
				FString(LexToString(Definition.MaterialFunctions[0].Kind)),
				Expectation.MaterialFunction0Kind);
		}
		if (Expectation.bCheckVirtualFunctions)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] virtualFunctions"), *Case.SourcePath), Definition.VirtualFunctions.Num(), Expectation.VirtualFunctions);
		}
		if (Expectation.bCheckCodeNotEmpty)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] codeNotEmpty == %s"), *Case.SourcePath, Expectation.bCodeNotEmpty ? TEXT("true") : TEXT("false")),
				(!Definition.Code.IsEmpty()) == Expectation.bCodeNotEmpty);
		}
		for (const TPair<FString, FString>& Pair : Expectation.Settings)
		{
			FString Value;
			const bool bHas = Definition.TryGetSetting(*Pair.Key, Value);
			Test.TestTrue(FString::Printf(TEXT("[%s] setting '%s' present"), *Case.SourcePath, *Pair.Key), bHas);
			if (bHas)
			{
				Test.TestEqual(FString::Printf(TEXT("[%s] setting '%s'"), *Case.SourcePath, *Pair.Key), Value, Pair.Value);
			}
		}
		for (const FString& Needle : Expectation.WarningsContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] warnings contain '%s'"), *Case.SourcePath, *Needle),
				Definition.Warnings.ContainsByPredicate([&Needle](const FString& Warning) { return Warning.Contains(Needle, ESearchCase::IgnoreCase); }));
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Generate layer: drives the actual material/asset generator (slow; needs editor + asset registry).
	// Uses bTransient=true so generation builds the graph in memory without writing /Game assets,
	// which means no asset cleanup is required and no save/metadata side effects occur.
	// ---------------------------------------------------------------------------------------------

	/**
	 * Base for the Generate corpus runner. Generation legitimately logs warnings/errors (e.g. a
	 * fixture that is supposed to fail will log its parse/generation error); we assert on the
	 * generator's bool return + OutMessage, so incidental logs must not fail the automation test.
	 */
	class FDreamShaderGenerateCorpusTestBase : public FAutomationTestBase
	{
	public:
		FDreamShaderGenerateCorpusTestBase(const FString& InName, bool bInComplexTask)
			: FAutomationTestBase(InName, bInComplexTask)
		{
		}

		virtual bool SuppressLogErrors() override { return true; }
		virtual bool SuppressLogWarnings() override { return true; }
	};

	inline FString BuildDreamShaderGenerateGoldenJson(bool bGenerated, const FString& Message)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("generate"));
		Root->SetStringField(TEXT("outcome"), bGenerated ? TEXT("ok") : TEXT("error"));
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueString>(Message));
		Root->SetArrayField(TEXT("messageContains"), Messages);

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Run one corpus case through the material generator (transient) and assert outcome + message.
	 * .dsm -> GenerateMaterialFromFile, .dsf -> GenerateAssetsFromFile, .dsh -> skipped (no asset).
	 */
	// Pins the project DefaultBackend to Graph for a test's duration: tests that assert
	// graph-backend generation shapes must not be rerouted by a project-level
	// DefaultBackend=Instance when their fixtures don't declare Backend themselves.
	struct FScopedDreamShaderGraphBackendPin
	{
		EDreamShaderDefaultBackend SavedDefaultBackend;

		FScopedDreamShaderGraphBackendPin()
			: SavedDefaultBackend(GetMutableDefault<UDreamShaderSettings>()->DefaultBackend)
		{
			GetMutableDefault<UDreamShaderSettings>()->DefaultBackend = EDreamShaderDefaultBackend::Graph;
		}

		~FScopedDreamShaderGraphBackendPin()
		{
			GetMutableDefault<UDreamShaderSettings>()->DefaultBackend = SavedDefaultBackend;
		}
	};

	inline bool RunDreamShaderGenerateCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		const FString Extension = Case.Extension.ToLower();

		// The corpus goldens encode GRAPH-backend generation semantics (node shapes, graph-level
		// type merging).
		FScopedDreamShaderGraphBackendPin BackendPin;

		FString Message;
		bool bGenerated = false;
		if (Extension == TEXT("dsm"))
		{
			bGenerated = FMaterialGenerator::GenerateMaterialFromFile(Case.SourcePath, Message, /*bForce*/ true, /*bTransient*/ true);
		}
		else if (Extension == TEXT("dsf"))
		{
			bGenerated = FMaterialGenerator::GenerateAssetsFromFile(Case.SourcePath, Message, /*bForce*/ true, /*bTransient*/ true);
		}
		else
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is a .dsh header; nothing to generate (skipped)."), *Case.SourcePath));
			return true;
		}

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderGenerateGoldenJson(bGenerated, Message);
			if (FFileHelper::SaveStringToFile(Json, *Case.ExpectedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddInfo(FString::Printf(TEXT("Updated golden '%s'."), *Case.ExpectedPath));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to write golden '%s'."), *Case.ExpectedPath));
			}
			return true;
		}

		FCorpusExpectation Expectation;
		Expectation.bExpectError = Case.bBadByName;
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
				return false;
			}

			FCorpusExpectation Loaded;
			Loaded.bExpectError = Case.bBadByName;
			FString JsonError;
			if (!ParseDreamShaderExpectation(JsonText, Loaded, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
			Expectation = MoveTemp(Loaded);
		}

		if (Expectation.bExpectError)
		{
			Test.TestFalse(FString::Printf(TEXT("[%s] generation should FAIL (msg: %s)"), *Case.SourcePath, *Message), bGenerated);
		}
		else if (!bGenerated)
		{
			Test.AddError(FString::Printf(TEXT("[%s] generation should SUCCEED but failed: %s"), *Case.SourcePath, *Message));
			return false;
		}

		for (const FString& Needle : Expectation.ErrorContains)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] message contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *Message),
				Message.Contains(Needle, ESearchCase::IgnoreCase));
		}

		return true;
	}
	// ---------------------------------------------------------------------------------------------
	// Lang layer: the DreamShaderLang 2.0 front end (Tests/Corpus/Lang/**).
	//
	// A `"entryPoint": "lang"` golden describes ONE parse of ONE .dss/.dsh fixture through
	// ParseDreamShaderLang: the outcome, DSHnnnn substrings matched against the wire form of the
	// diagnostics of that severity, a handful of structural counts over the module, and the
	// print -> parse -> print round trip. Everything is opt-in: a golden names only what it cares
	// about, and a fixture with no golden falls back to "positive parses, `.bad.` fails".
	//
	// Golden schema (all fields optional):
	//   {
	//     "entryPoint": "lang",
	//     "outcome": "ok" | "error",
	//     "errorContains":   ["DSH2105"],      // substrings of the ERROR diagnostics' wire strings
	//     "warningsContain": ["DSH3220"],      // substrings of the WARNING diagnostics' wire strings
	//     "module": {
	//       "declarations": 6,                 // FModule::Declarations.Num()  (pragmas/includes too)
	//       "pragmas": 1, "includes": 0, "structs": 0,
	//       "uniforms": 3,                     // FVariableDecl with EStorageClass::Uniform
	//       "constants": 0,                    // FVariableDecl with EStorageClass::StaticConst
	//       "functions": 2, "exports": 1, "externs": 0, "opaqueBodies": 0,
	//       "entry": "M_TeleportGlow",         // first IsMaterialEntry() function, "" when none
	//       "roundtrip": true                  // print/parse/print is byte-identical
	//     }
	//   }
	// ---------------------------------------------------------------------------------------------

	/** The structural facts a `"module"` golden can assert, derived from one FModule. */
	struct FLangModuleSummary
	{
		int32 Declarations = 0;
		int32 Pragmas = 0;
		int32 Includes = 0;
		int32 Structs = 0;
		int32 Uniforms = 0;
		int32 Constants = 0;
		int32 Functions = 0;
		int32 Exports = 0;
		int32 Externs = 0;
		int32 OpaqueBodies = 0;
		/** Name of the first function whose signature is `void (inout material)`; empty when none. */
		FString Entry;
	};

	/** Declarative `"entryPoint": "lang"` expectation. Every field is opt-in. */
	struct FLangCorpusExpectation
	{
		/** As written in the golden; the runner rejects a fixture whose golden is for another layer. */
		FString EntryPoint;
		bool bExpectError = false;                 // outcome: "error" (or a `.bad.` filename)
		TArray<FString> ErrorContains;             // substrings of the error diagnostics
		TArray<FString> WarningsContain;           // substrings of the warning diagnostics

		bool bCheckDeclarations = false;  int32 Declarations = 0;
		bool bCheckPragmas = false;       int32 Pragmas = 0;
		bool bCheckIncludes = false;      int32 Includes = 0;
		bool bCheckStructs = false;       int32 Structs = 0;
		bool bCheckUniforms = false;      int32 Uniforms = 0;
		bool bCheckConstants = false;     int32 Constants = 0;
		bool bCheckFunctions = false;     int32 Functions = 0;
		bool bCheckExports = false;       int32 Exports = 0;
		bool bCheckExterns = false;       int32 Externs = 0;
		bool bCheckOpaqueBodies = false;  int32 OpaqueBodies = 0;
		bool bCheckEntry = false;         FString Entry;
		bool bCheckRoundtrip = false;     bool bRoundtrip = true;
	};

	/** Count the declaration shapes a `"module"` golden talks about. */
	inline FLangModuleSummary SummariseDreamShaderLangModule(const UE::DreamShader::Lang::FModule& Module)
	{
		using namespace UE::DreamShader::Lang;

		FLangModuleSummary Summary;
		Summary.Declarations = Module.Declarations.Num();

		for (const FDeclPtr& DeclPtr : Module.Declarations)
		{
			const FDecl* Decl = DeclPtr.Get();
			if (Decl == nullptr)
			{
				continue;
			}

			if (const FVariableDecl* Variable = Decl->As<FVariableDecl>())
			{
				if (Variable->Storage == EStorageClass::Uniform)
				{
					++Summary.Uniforms;
				}
				else if (Variable->Storage == EStorageClass::StaticConst)
				{
					++Summary.Constants;
				}
			}
			else if (const FFunctionDecl* Function = Decl->As<FFunctionDecl>())
			{
				++Summary.Functions;
				if (Function->Linkage == EFunctionLinkage::Export)
				{
					++Summary.Exports;
				}
				else if (Function->Linkage == EFunctionLinkage::Extern)
				{
					++Summary.Externs;
				}
				if (Function->bOpaqueBody)
				{
					++Summary.OpaqueBodies;
				}
				if (Summary.Entry.IsEmpty() && Function->IsMaterialEntry())
				{
					Summary.Entry = Function->Name;
				}
			}
			else if (Decl->Is<FStructDecl>())
			{
				++Summary.Structs;
			}
			else if (Decl->Is<FIncludeDecl>())
			{
				++Summary.Includes;
			}
			else if (Decl->Is<FPragmaDecl>())
			{
				++Summary.Pragmas;
			}
		}

		return Summary;
	}

	/** Every diagnostic of one severity, in its `DSHnnnn: message` wire form. */
	inline TArray<FString> GatherDreamShaderLangDiagnostics(
		const UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
		UE::DreamShader::Lang::ELangSeverity Severity)
	{
		using namespace UE::DreamShader::Lang;

		TArray<FString> Lines;
		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			if (Diagnostic.Severity == Severity)
			{
				Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic));
			}
		}
		return Lines;
	}

	/**
	 * print -> parse -> print, byte-identical. The reprinted text keeps the fixture's path so the
	 * `Auto` front-end selection still sees the same extension.
	 */
	inline bool CheckDreamShaderLangRoundtrip(
		const UE::DreamShader::Lang::FModule& Module,
		const FString& SourcePath,
		FString& OutFailure)
	{
		using namespace UE::DreamShader::Lang;

		const FString First = PrintDreamShaderLang(Module);

		const FLangSourceText Reprinted(SourcePath, First);
		const FLangParseResult Second = ParseDreamShaderLang(Reprinted);
		if (!Second.Module.IsValid())
		{
			OutFailure = TEXT("re-parsing the printed text produced no module");
			return false;
		}
		if (Second.Diagnostics.HasErrors())
		{
			OutFailure = FString::Printf(
				TEXT("re-parsing the printed text failed: %s"),
				*FString::Join(GatherDreamShaderLangDiagnostics(Second.Diagnostics, ELangSeverity::Error), TEXT(" | ")));
			return false;
		}

		const FString Reprint = PrintDreamShaderLang(*Second.Module);
		if (!First.Equals(Reprint, ESearchCase::CaseSensitive))
		{
			int32 Index = 0;
			const int32 Common = FMath::Min(First.Len(), Reprint.Len());
			while (Index < Common && First[Index] == Reprint[Index])
			{
				++Index;
			}
			OutFailure = FString::Printf(
				TEXT("print/parse/print differs at offset %d (%d vs %d characters)"),
				Index, First.Len(), Reprint.Len());
			return false;
		}

		return true;
	}

	/** Decode a `"entryPoint": "lang"` golden. Returns false only on malformed json. */
	inline bool ParseDreamShaderLangExpectation(const FString& JsonText, FLangCorpusExpectation& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("invalid JSON");
			return false;
		}

		Root->TryGetStringField(TEXT("entryPoint"), Out.EntryPoint);

		FString Outcome;
		if (Root->TryGetStringField(TEXT("outcome"), Outcome))
		{
			Out.bExpectError = Outcome.Equals(TEXT("error"), ESearchCase::IgnoreCase);
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Root->TryGetArrayField(TEXT("errorContains"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.ErrorContains.Add(Value->AsString());
			}
		}
		if (Root->TryGetArrayField(TEXT("warningsContain"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.WarningsContain.Add(Value->AsString());
			}
		}

		const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
		if (Root->TryGetObjectField(TEXT("module"), ModuleObject))
		{
			double NumberValue = 0.0;
			bool BoolValue = false;
			FString StringValue;

			if ((*ModuleObject)->TryGetNumberField(TEXT("declarations"), NumberValue)) { Out.bCheckDeclarations = true; Out.Declarations = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("pragmas"), NumberValue)) { Out.bCheckPragmas = true; Out.Pragmas = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("includes"), NumberValue)) { Out.bCheckIncludes = true; Out.Includes = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("structs"), NumberValue)) { Out.bCheckStructs = true; Out.Structs = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("uniforms"), NumberValue)) { Out.bCheckUniforms = true; Out.Uniforms = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("constants"), NumberValue)) { Out.bCheckConstants = true; Out.Constants = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("functions"), NumberValue)) { Out.bCheckFunctions = true; Out.Functions = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("exports"), NumberValue)) { Out.bCheckExports = true; Out.Exports = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("externs"), NumberValue)) { Out.bCheckExterns = true; Out.Externs = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("opaqueBodies"), NumberValue)) { Out.bCheckOpaqueBodies = true; Out.OpaqueBodies = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetStringField(TEXT("entry"), StringValue)) { Out.bCheckEntry = true; Out.Entry = StringValue; }
			if ((*ModuleObject)->TryGetBoolField(TEXT("roundtrip"), BoolValue)) { Out.bCheckRoundtrip = true; Out.bRoundtrip = BoolValue; }
		}

		return true;
	}

	/**
	 * The `DSHnnnn` prefix of a wire string, or the whole string when it has none.
	 *
	 * A golden written by -DreamShaderUpdateGolden must name the CODE, never the message. The code
	 * is the stable half of a diagnostic; the English half is free to be reworded and is localised
	 * in the editor, so a golden quoting it would start failing for reasons that have nothing to do
	 * with the parser. Every checked-in `lang` golden already names bare codes, and regenerating
	 * one must not quietly convert the corpus to prose. Matching stays a substring test, so a
	 * hand-written golden that names a message fragment on purpose keeps working; this decides only
	 * what gets WRITTEN.
	 */
	inline FString GetDreamShaderLangDiagnosticCode(const FString& WireString)
	{
		int32 ColonIndex = INDEX_NONE;
		if (!WireString.FindChar(TEXT(':'), ColonIndex))
		{
			return WireString;
		}

		const FString Prefix = WireString.Left(ColonIndex);
		return Prefix.StartsWith(TEXT("DSH"), ESearchCase::CaseSensitive) ? Prefix : WireString;
	}

	/** Serialize a baseline `lang` golden from an actual parse (used by -DreamShaderUpdateGolden). */
	inline FString BuildDreamShaderLangGoldenJson(
		bool bOk,
		const FLangModuleSummary& Summary,
		bool bRoundtrip,
		const TArray<FString>& Errors,
		const TArray<FString>& Warnings)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("lang"));
		Root->SetStringField(TEXT("outcome"), bOk ? TEXT("ok") : TEXT("error"));

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Error : Errors)
			{
				Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Error)));
			}
			Root->SetArrayField(TEXT("errorContains"), Values);
		}
		if (Warnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Warning : Warnings)
			{
				Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Warning)));
			}
			Root->SetArrayField(TEXT("warningsContain"), Values);
		}

		if (bOk)
		{
			const TSharedRef<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
			ModuleObject->SetNumberField(TEXT("declarations"), Summary.Declarations);
			ModuleObject->SetNumberField(TEXT("pragmas"), Summary.Pragmas);
			ModuleObject->SetNumberField(TEXT("includes"), Summary.Includes);
			ModuleObject->SetNumberField(TEXT("structs"), Summary.Structs);
			ModuleObject->SetNumberField(TEXT("uniforms"), Summary.Uniforms);
			ModuleObject->SetNumberField(TEXT("constants"), Summary.Constants);
			ModuleObject->SetNumberField(TEXT("functions"), Summary.Functions);
			ModuleObject->SetNumberField(TEXT("exports"), Summary.Exports);
			ModuleObject->SetNumberField(TEXT("externs"), Summary.Externs);
			ModuleObject->SetNumberField(TEXT("opaqueBodies"), Summary.OpaqueBodies);
			ModuleObject->SetStringField(TEXT("entry"), Summary.Entry);
			ModuleObject->SetBoolField(TEXT("roundtrip"), bRoundtrip);
			Root->SetObjectField(TEXT("module"), ModuleObject);
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Run one corpus case through ParseDreamShaderLang and assert it against its golden.
	 * In -DreamShaderUpdateGolden mode it rewrites the golden instead of asserting.
	 * Returns false only on a hard I/O failure; semantic mismatches are recorded on Test.
	 */
	inline bool RunDreamShaderLangCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		using namespace UE::DreamShader::Lang;

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		const FLangSourceText Source(Case.SourcePath, SourceString);
		const FLangParseResult Result = ParseDreamShaderLang(Source, FLangParseOptions());

		const TArray<FString> Errors = GatherDreamShaderLangDiagnostics(Result.Diagnostics, ELangSeverity::Error);
		const TArray<FString> Warnings = GatherDreamShaderLangDiagnostics(Result.Diagnostics, ELangSeverity::Warning);
		const FString ErrorText = FString::Join(Errors, TEXT(" | "));
		const FString WarningText = FString::Join(Warnings, TEXT(" | "));
		const bool bOk = Result.Succeeded();

		FLangModuleSummary Summary;
		bool bRoundtrip = false;
		FString RoundtripFailure = TEXT("the parse reported errors");
		if (Result.Module.IsValid())
		{
			Summary = SummariseDreamShaderLangModule(*Result.Module);
			if (bOk)
			{
				bRoundtrip = CheckDreamShaderLangRoundtrip(*Result.Module, Case.SourcePath, RoundtripFailure);
			}
		}

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderLangGoldenJson(bOk, Summary, bRoundtrip, Errors, Warnings);
			if (FFileHelper::SaveStringToFile(Json, *Case.ExpectedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddInfo(FString::Printf(TEXT("Updated golden '%s'."), *Case.ExpectedPath));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to write golden '%s'."), *Case.ExpectedPath));
			}
			return true;
		}

		FLangCorpusExpectation Expectation;
		Expectation.bExpectError = Case.bBadByName; // default; the json may override
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
				return false;
			}

			FLangCorpusExpectation Loaded;
			Loaded.bExpectError = Case.bBadByName;
			FString JsonError;
			if (!ParseDreamShaderLangExpectation(JsonText, Loaded, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
			Expectation = MoveTemp(Loaded);
		}

		if (!Expectation.EntryPoint.IsEmpty() && !Expectation.EntryPoint.Equals(TEXT("lang"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] golden declares entryPoint '%s'; the Lang corpus only runs 'lang' goldens."),
				*Case.ExpectedPath, *Expectation.EntryPoint));
			return false;
		}

		if (Expectation.bExpectError)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] parse should FAIL"), *Case.SourcePath),
				Result.Diagnostics.HasErrors());
		}
		else if (Result.Diagnostics.HasErrors())
		{
			Test.AddError(FString::Printf(TEXT("[%s] parse should SUCCEED but failed: %s"), *Case.SourcePath, *ErrorText));
			return false;
		}

		for (const FString& Needle : Expectation.ErrorContains)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] an error contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *ErrorText),
				Errors.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
		}
		for (const FString& Needle : Expectation.WarningsContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] a warning contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *WarningText),
				Warnings.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
		}

		if (Expectation.bExpectError)
		{
			return true;
		}

		if (!Result.Module.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("[%s] parse produced no module."), *Case.SourcePath));
			return false;
		}

		if (Expectation.bCheckDeclarations) { Test.TestEqual(FString::Printf(TEXT("[%s] declarations"), *Case.SourcePath), Summary.Declarations, Expectation.Declarations); }
		if (Expectation.bCheckPragmas) { Test.TestEqual(FString::Printf(TEXT("[%s] pragmas"), *Case.SourcePath), Summary.Pragmas, Expectation.Pragmas); }
		if (Expectation.bCheckIncludes) { Test.TestEqual(FString::Printf(TEXT("[%s] includes"), *Case.SourcePath), Summary.Includes, Expectation.Includes); }
		if (Expectation.bCheckStructs) { Test.TestEqual(FString::Printf(TEXT("[%s] structs"), *Case.SourcePath), Summary.Structs, Expectation.Structs); }
		if (Expectation.bCheckUniforms) { Test.TestEqual(FString::Printf(TEXT("[%s] uniforms"), *Case.SourcePath), Summary.Uniforms, Expectation.Uniforms); }
		if (Expectation.bCheckConstants) { Test.TestEqual(FString::Printf(TEXT("[%s] constants"), *Case.SourcePath), Summary.Constants, Expectation.Constants); }
		if (Expectation.bCheckFunctions) { Test.TestEqual(FString::Printf(TEXT("[%s] functions"), *Case.SourcePath), Summary.Functions, Expectation.Functions); }
		if (Expectation.bCheckExports) { Test.TestEqual(FString::Printf(TEXT("[%s] exports"), *Case.SourcePath), Summary.Exports, Expectation.Exports); }
		if (Expectation.bCheckExterns) { Test.TestEqual(FString::Printf(TEXT("[%s] externs"), *Case.SourcePath), Summary.Externs, Expectation.Externs); }
		if (Expectation.bCheckOpaqueBodies) { Test.TestEqual(FString::Printf(TEXT("[%s] opaqueBodies"), *Case.SourcePath), Summary.OpaqueBodies, Expectation.OpaqueBodies); }

		if (Expectation.bCheckEntry)
		{
			// FString::operator== is case-insensitive; a material entry's name is the asset name.
			Test.TestTrue(
				FString::Printf(TEXT("[%s] entry == '%s' (actual: '%s')"), *Case.SourcePath, *Expectation.Entry, *Summary.Entry),
				Summary.Entry.Equals(Expectation.Entry, ESearchCase::CaseSensitive));
		}

		if (Expectation.bCheckRoundtrip)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] roundtrip == %s%s%s"),
					*Case.SourcePath,
					Expectation.bRoundtrip ? TEXT("true") : TEXT("false"),
					bRoundtrip ? TEXT("") : TEXT(" -- "),
					bRoundtrip ? TEXT("") : *RoundtripFailure),
				bRoundtrip == Expectation.bRoundtrip);
		}

		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
