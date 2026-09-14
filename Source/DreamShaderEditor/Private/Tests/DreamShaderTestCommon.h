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

// Batch 1 (M2+M3): the IR and Compile corpus layers at the end of this file need the 2.0 front
// end's semantic and IR headers, plus the `dump-graph` builder that is the Compile layer's golden
// format. Additive: nothing above this line changed.
#include "IR/IR.h"
#include "IR/IRBuilder.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRDump.h"
#include "IR/IRPasses.h"
#include "IR/IRTypes.h"
#include "IR/IRValidator.h"
#include "Semantic/LangBound.h"

#include "Commandlet/DreamShaderGraphDump.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"

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
	// Allows Ephemeral so generation builds the graph in memory without writing /Game assets,
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
			bGenerated = FMaterialGenerator::GenerateMaterialFromFile(Case.SourcePath, Message, /*bForce*/ true, /*bAllowEphemeralThinCustom*/ true);
		}
		else if (Extension == TEXT("dsf"))
		{
			bGenerated = FMaterialGenerator::GenerateAssetsFromFile(Case.SourcePath, Message, /*bForce*/ true, /*bAllowEphemeralThinCustom*/ true);
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

	// =============================================================================================
	// IR layer (Tests/Corpus/IR/**) and Compile layer (Tests/Corpus/Compile/**) -- batch 1 (M2+M3).
	//
	// Two more data-driven layers, built the same way the Lang layer above is: drop a `.dss` under
	// Tests/Corpus/<Layer>/<Area>/ with an optional `<name>.expected.json` and the runner discovers
	// it on the next run. No new C++, no recompile.
	//
	//   IR/       ParseDreamShaderLang -> BindDreamShaderLang -> BuildDreamShaderIR ->
	//             RunDreamShaderIRPasses -> ValidateDreamShaderIR, golden = DumpDreamShaderIRText.
	//             Pure Core: no asset I/O, no reflection. The builtin catalog is the HAND-BUILT one
	//             below, not the engine's -- a golden that moved because the engine gained a pin
	//             would say nothing about the compiler, and the corpus exists to say something
	//             about the compiler. `Tests/Corpus/IR` fixtures may therefore only name the
	//             builtins MakeDreamShaderTestBuiltinCatalog() declares; everything else is what
	//             the Compile layer is for.
	//
	//   Compile/  FMaterialGenerator::GenerateAssetsFromFile on a `.dss` (the 1.x generator's hook
	//             routes it into the 2.0 pipeline), golden = the normalised `dump-graph` JSON of
	//             every asset it produced. Slow: editor, reflection, real /Game packages. Each
	//             fixture is copied into its OWN directory under the project's DShader root so the
	//             assets it makes have a package path nothing else writes to, and both the copy and
	//             the assets are deleted on the way out.
	//
	// Neither runner preprocesses: like the Lang runner, a fixture is the already-preprocessed text.
	// The Compile layer goes through the real generator, which does preprocess -- so a `#if` belongs
	// in a Compile fixture, never in an IR one.
	// =============================================================================================

	// ---------------------------------------------------------------------------------------------
	// The hand-built builtin catalog
	// ---------------------------------------------------------------------------------------------

	/** One reflected-pin descriptor, spelled out so the table below reads as a table. */
	inline UE::DreamShader::IR::FCatalogPin MakeDreamShaderTestPin(
		const TCHAR* Name,
		UE::DreamShader::IR::ECatalogValueType Type,
		bool bRequired = false,
		const TCHAR* ConstPropertyName = nullptr,
		const TArray<FString>& Aliases = TArray<FString>())
	{
		UE::DreamShader::IR::FCatalogPin Pin;
		Pin.Name = Name;
		Pin.Type = Type;
		Pin.bRequired = bRequired;
		if (ConstPropertyName)
		{
			Pin.ConstPropertyName = ConstPropertyName;
		}
		// The 1.x argument spellings the binder still answers to: `UE.TexCoord(Index = 0)` has to
		// keep working, and the alias lives on the pin or the property rather than in a rule
		// somewhere, so the catalog stays the only place engine facts are written down.
		Pin.Aliases = Aliases;
		return Pin;
	}

	inline UE::DreamShader::IR::FCatalogProperty MakeDreamShaderTestProperty(
		const TCHAR* Name,
		UE::DreamShader::IR::ECatalogValueType Type,
		const TArray<FString>& EnumValues = TArray<FString>(),
		const TArray<FString>& Aliases = TArray<FString>())
	{
		UE::DreamShader::IR::FCatalogProperty Property;
		Property.Name = Name;
		Property.Type = Type;
		Property.EnumValues = EnumValues;
		Property.Aliases = Aliases;
		return Property;
	}

	inline UE::DreamShader::IR::FCatalogMaterialAttribute MakeDreamShaderTestAttribute(
		const TCHAR* Name,
		const TCHAR* PropertyName,
		const UE::DreamShader::IR::FIRType& ValueType,
		const TArray<FString>& Aliases = TArray<FString>())
	{
		UE::DreamShader::IR::FCatalogMaterialAttribute Attribute;
		Attribute.Name = Name;
		Attribute.PropertyName = PropertyName;
		Attribute.ValueType = ValueType;
		Attribute.Aliases = Aliases;
		return Attribute;
	}

	/**
	 * A small, engine-free FBuiltinCatalog: enough reflected classes and material attributes for the
	 * binder and IR unit tests and for every Tests/Corpus/IR fixture, and nothing else.
	 *
	 * Written by hand ON PURPOSE. BuildBuiltinCatalogFromReflection is the production producer and
	 * the Compile layer exercises it end to end; a unit test that used it would be asserting on the
	 * engine's current pin set rather than on the binder, would need an editor to run at all, and
	 * would turn every engine upgrade into a corpus diff. The shapes here are faithful to the
	 * classes they name in the respects the front end cares about (pin names, output counts, which
	 * pins have a Const* twin, which class is a custom output) and deliberately trimmed everywhere
	 * else -- UE.VertexColor has one float4 output here where the engine class has five, because
	 * `.rgb` on it must be an ordinary Swizzle (CONTRACT 6.6) and one output is what makes that so.
	 */
	inline UE::DreamShader::IR::FBuiltinCatalog MakeDreamShaderTestBuiltinCatalog()
	{
		using namespace UE::DreamShader::IR;

		FBuiltinCatalog Catalog;
		Catalog.Source = TEXT("test");
		Catalog.EngineVersion = TEXT("test");

		// UE.TextureCoordinate / UE.TexCoord -- the alias case, the positional case and the
		// "argument is a property, not a pin" case in one entry.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("TextureCoordinate");
			Expression.ClassName = TEXT("MaterialExpressionTextureCoordinate");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionTextureCoordinate");
			Expression.Aliases.Add(TEXT("TexCoord"));
			// `Coordinates` takes `UV` and `CoordinateIndex` takes `Index`: the 1.x spellings, kept
			// alive as aliases so the three Lang examples still say what their authors wrote.
			Expression.Inputs.Add(MakeDreamShaderTestPin(
				TEXT("Coordinates"), ECatalogValueType::Float2, false, nullptr, { TEXT("UV") }));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Float2));
			Expression.Properties.Add(MakeDreamShaderTestProperty(
				TEXT("CoordinateIndex"), ECatalogValueType::Int, TArray<FString>(), { TEXT("Index") }));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("UTiling"), ECatalogValueType::Float1));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("VTiling"), ECatalogValueType::Float1));
			Expression.PositionalParameters.Add(TEXT("CoordinateIndex"));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.VertexColor -- one output, no inputs, no properties: the "named arguments only, and
		// there are none" shape, and the operand of every `.rgb` swizzle case.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("VertexColor");
			Expression.ClassName = TEXT("MaterialExpressionVertexColor");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionVertexColor");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Float4));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.Time -- one output, one optional property. The dedupe pass's favourite node.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("Time");
			Expression.ClassName = TEXT("MaterialExpressionTime");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionTime");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Float1));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Period"), ECatalogValueType::Float1));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.SceneTexture -- THREE outputs, so its result type is `Node` and an output has to be
		// selected by member access before the value can be used (decision 11 #1(b)).
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("SceneTexture");
			Expression.ClassName = TEXT("MaterialExpressionSceneTexture");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionSceneTexture");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("UV"), ECatalogValueType::Float2));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("Color"), ECatalogValueType::Float4));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("Size"), ECatalogValueType::Float2));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("InvSize"), ECatalogValueType::Float2));
			Expression.Properties.Add(MakeDreamShaderTestProperty(
				TEXT("SceneTextureId"),
				ECatalogValueType::Enum,
				{ TEXT("PostProcessInput0"), TEXT("SceneColor"), TEXT("SceneDepth") }));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.LinearInterpolate -- three pins, each with a Const* twin: the const-property
		// preference of CONTRACT 6.10 has to have somewhere to happen.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("LinearInterpolate");
			Expression.ClassName = TEXT("MaterialExpressionLinearInterpolate");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionLinearInterpolate");
			Expression.Aliases.Add(TEXT("Lerp"));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::Numeric, false, TEXT("ConstA")));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::Numeric, false, TEXT("ConstB")));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Alpha"), ECatalogValueType::Numeric, false, TEXT("ConstAlpha")));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Numeric));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("ConstA"), ECatalogValueType::Float1));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("ConstB"), ECatalogValueType::Float1));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("ConstAlpha"), ECatalogValueType::Float1));
			Expression.PositionalParameters.Add(TEXT("A"));
			Expression.PositionalParameters.Add(TEXT("B"));
			Expression.PositionalParameters.Add(TEXT("Alpha"));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// A custom-output class: a statement, never a value (CONTRACT 6.10, DSH4231).
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("ClearCoatNormalCustomOutput");
			Expression.ClassName = TEXT("MaterialExpressionClearCoatNormalCustomOutput");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionClearCoatNormalCustomOutput");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Input"), ECatalogValueType::Float3, true));
			Expression.bIsCustomOutput = true;
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// A reflected class with MaterialAttributes PINS: the one place CONTRACT 6.13 still wants a
		// MakeMaterialAttributes node made, now that a `material` may not cross into a @custom.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("BlendMaterialAttributes");
			Expression.ClassName = TEXT("MaterialExpressionBlendMaterialAttributes");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionBlendMaterialAttributes");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::MaterialAttributes));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::MaterialAttributes));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Alpha"), ECatalogValueType::Numeric));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::MaterialAttributes));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// The Substrate namespace: a second namespace root, and the only producer of a Substrate
		// value (decision 11 #11 -- `Substrate.Unlit` and `UE.Unlit` must resolve to the same entry
		// only through the namespace the catalog records, never by accident).
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("Substrate");
			Expression.ShortName = TEXT("Unlit");
			Expression.ClassName = TEXT("MaterialExpressionSubstrateUnlitBSDF");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionSubstrateUnlitBSDF");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("EmissiveColor"), ECatalogValueType::Float3));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("TransmittanceColor"), ECatalogValueType::Float3));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Substrate));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// A class whose outputs are all CHANNEL VIEWS of one float4 -- the engine VertexColor's pin set,
		// `RGB, R, G, B, A`, the way the reflection filler names it (FIX3-Binder). The VertexColor entry
		// above keeps its one float4 output because other tests read it; this one is where the binder's
		// "a node of channel views stands for its whole value" and the builder's widening and pin
		// selection have something to run against. Appended after every other class, so no index moves.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("VertexColorViews");
			Expression.ClassName = TEXT("MaterialExpressionVertexColorViews");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionVertexColorViews");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("RGB"), ECatalogValueType::Float3));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("R"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("G"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::Float1));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// The material attribute table -- the seven CONTRACT 6.1/6.2 name, so that an attribute outside it
		// (`m.Metallic`) is a negative fixture with somewhere to land -- plus the whole-set entry below.
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("BaseColor"), TEXT("MP_BaseColor"), FIRType::Float(3)));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("EmissiveColor"), TEXT("MP_EmissiveColor"), FIRType::Float(3), { TEXT("Emissive") }));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("Roughness"), TEXT("MP_Roughness"), FIRType::Float(1)));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("Normal"), TEXT("MP_Normal"), FIRType::Float(3)));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("Opacity"), TEXT("MP_Opacity"), FIRType::Float(1)));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("WorldPositionOffset"), TEXT("MP_WorldPositionOffset"), FIRType::Float(3)));
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("FrontMaterial"), TEXT("MP_FrontMaterial"), FIRType::Substrate()));
		// The whole-set entry reflection always adds (DreamShaderBuiltinCatalogReflection.cpp): a material
		// replaced as a whole reaches the sink's MaterialAttributes input by this name, and
		// `m.MaterialAttributes` names it. Last, so the seven above keep their indices.
		Catalog.MaterialAttributes.Add(MakeDreamShaderTestAttribute(TEXT("MaterialAttributes"), TEXT("MP_MaterialAttributes"), FIRType::Material(), { TEXT("Attributes") }));

		return Catalog;
	}

	/** One shared instance, so a hundred sub-tests do not rebuild the table a hundred times. */
	inline const UE::DreamShader::IR::FBuiltinCatalog& GetDreamShaderTestBuiltinCatalog()
	{
		static const UE::DreamShader::IR::FBuiltinCatalog Catalog = MakeDreamShaderTestBuiltinCatalog();
		return Catalog;
	}

	// ---------------------------------------------------------------------------------------------
	// One run of the front end, from text to validated IR
	// ---------------------------------------------------------------------------------------------

	/**
	 * Everything one IR run produced, with the ownership order the bound module needs.
	 *
	 * Member order is destruction order reversed: the IR module goes first, then the bound module
	 * (which holds pointers into the ASTs), then the included ASTs, then the main one. Reordering
	 * these is a dangling read, not a style change.
	 */
	struct FDreamShaderIRRun
	{
		UE::DreamShader::Lang::FLangParseResult Parse;
		/** Modules the include resolver handed to the binder; they must outlive the bound module. */
		TArray<TUniquePtr<UE::DreamShader::Lang::FLangParseResult>> Included;
		UE::DreamShader::Lang::FLangBindResult Bind;
		/** Build + passes + validate all report here. */
		UE::DreamShader::Lang::FLangDiagnosticSink Lowering;
		TUniquePtr<UE::DreamShader::IR::FIRModule> Module;

		bool bParsed = false;
		bool bBound = false;
		bool bBuilt = false;
		bool bValidated = false;

		/** DumpDreamShaderIRText of the module, or empty when there is none. */
		FString IRText;
		/** Every diagnostic of that severity across every stage, in `DSHnnnn: message` wire form. */
		TArray<FString> Errors;
		TArray<FString> Warnings;

		bool Succeeded() const { return bValidated && Errors.Num() == 0; }
		FString ErrorText() const { return FString::Join(Errors, TEXT(" | ")); }
		FString WarningText() const { return FString::Join(Warnings, TEXT(" | ")); }
	};

	/** Options the IR runner takes from a golden (or a unit test) rather than hard-coding. */
	struct FDreamShaderIRRunOptions
	{
		/** Null means GetDreamShaderTestBuiltinCatalog(). */
		const UE::DreamShader::IR::FBuiltinCatalog* Catalog = nullptr;
		/** False stops after BuildDreamShaderIR, so a fixture can show the graph BEFORE dedupe/fold. */
		bool bRunPasses = true;
		bool bValidate = true;
		/** Resolves `#include` against this directory first; empty disables includes entirely. */
		FString IncludeDirectory;
	};

	/**
	 * Parse -> bind -> lower -> passes -> validate, stopping at the first stage that errors.
	 *
	 * Every stage reports into its own sink and the wire strings are gathered across all of them,
	 * so a golden's `errorContains` names a code without caring which stage raised it -- which is
	 * the point: DSH4210 is the contract, "the binder raised it" is an implementation detail.
	 */
	inline void RunDreamShaderIRPipeline(
		const FString& SourcePath,
		const FString& SourceText,
		const FDreamShaderIRRunOptions& Options,
		FDreamShaderIRRun& Out)
	{
		using namespace UE::DreamShader;
		using namespace UE::DreamShader::Lang;
		using namespace UE::DreamShader::IR;

		const FBuiltinCatalog& Catalog = Options.Catalog ? *Options.Catalog : GetDreamShaderTestBuiltinCatalog();

		Out.Parse = ParseDreamShaderLang(FLangSourceText(SourcePath, SourceText), FLangParseOptions());
		Out.bParsed = Out.Parse.Succeeded();

		auto Gather = [&Out](const FLangDiagnosticSink& Sink)
		{
			for (const FString& Line : GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Error))
			{
				Out.Errors.Add(Line);
			}
			for (const FString& Line : GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Warning))
			{
				Out.Warnings.Add(Line);
			}
		};

		Gather(Out.Parse.Diagnostics);
		if (!Out.bParsed || !Out.Parse.Module.IsValid())
		{
			return;
		}

		FBindOptions BindOptions;
		BindOptions.Catalog = &Catalog;
		if (!Options.IncludeDirectory.IsEmpty())
		{
			const FString IncludeDirectory = Options.IncludeDirectory;
			FDreamShaderIRRun* Run = &Out;
			BindOptions.IncludeResolver =
				[Run, IncludeDirectory](const FString& IncludePath, const FString& FromFile, FLangDiagnosticSink& Diagnostics) -> const FModule*
			{
				// Fixture-local resolution only: the leaf of whatever was written, looked for next
				// to the including file and then in the layer's include directory. The corpus is a
				// flat set of small files on purpose -- the real resolver (unit P) is what knows
				// about /Game, @scope and the source roots, and it is the Compile layer that
				// exercises it.
				const FString Leaf = FPaths::GetCleanFilename(IncludePath);
				TArray<FString> Candidates;
				if (!FromFile.IsEmpty())
				{
					Candidates.Add(FPaths::Combine(FPaths::GetPath(FromFile), Leaf));
				}
				Candidates.Add(FPaths::Combine(IncludeDirectory, Leaf));

				for (const FString& Candidate : Candidates)
				{
					const FString Full = FPaths::ConvertRelativePathToFull(Candidate);
					for (const TUniquePtr<FLangParseResult>& Existing : Run->Included)
					{
						if (Existing.IsValid() && Existing->Module.IsValid()
							&& Existing->Module->FilePath.Equals(Full, ESearchCase::CaseSensitive))
						{
							return Existing->Module.Get();
						}
					}

					FString Text;
					if (!FFileHelper::LoadFileToString(Text, *Full))
					{
						continue;
					}

					TUniquePtr<FLangParseResult> Parsed = MakeUnique<FLangParseResult>(
						ParseDreamShaderLang(FLangSourceText(Full, Text), FLangParseOptions()));
					if (!Parsed->Module.IsValid())
					{
						return nullptr;
					}

					// The include's own diagnostics belong to this run too.
					Diagnostics.Append(MoveTemp(Parsed->Diagnostics));

					const FModule* Module = Parsed->Module.Get();
					Run->Included.Add(MoveTemp(Parsed));
					return Module;
				}

				return nullptr;
			};
		}

		Out.Bind = BindDreamShaderLang(*Out.Parse.Module, BindOptions);
		Gather(Out.Bind.Diagnostics);
		Out.bBound = Out.Bind.Succeeded();
		if (!Out.bBound || !Out.Bind.Bound.IsValid())
		{
			return;
		}

		FIRBuildOptions BuildOptions;
		Out.Module = BuildDreamShaderIR(*Out.Bind.Bound, BuildOptions, Out.Lowering);
		Out.bBuilt = Out.Module.IsValid() && !Out.Lowering.HasErrors();
		if (!Out.bBuilt)
		{
			Gather(Out.Lowering);
			return;
		}

		if (Options.bRunPasses)
		{
			FIRPassOptions PassOptions;
			RunDreamShaderIRPasses(*Out.Module, PassOptions, Out.Lowering);
		}

		if (Options.bValidate)
		{
			Out.bValidated = ValidateDreamShaderIR(*Out.Module, Catalog, Out.Lowering) && !Out.Lowering.HasErrors();
		}
		else
		{
			Out.bValidated = !Out.Lowering.HasErrors();
		}

		Gather(Out.Lowering);
		Out.IRText = DumpDreamShaderIRText(*Out.Module);
		// A `@custom` body names its source file in the Custom-code markers, and a corpus fixture's file is
		// an absolute path on this machine. Goldens are committed, so the corpus root reads <corpus>/ on both
		// sides of the compare. The product itself still embeds the absolute path; that is tracked for M4.
		{
			FString CorpusRoot = GetDreamShaderCorpusRoot();
			CorpusRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (!CorpusRoot.IsEmpty())
			{
				if (!CorpusRoot.EndsWith(TEXT("/")))
				{
					CorpusRoot += TEXT("/");
				}
				Out.IRText.ReplaceInline(*CorpusRoot, TEXT("<corpus>/"), ESearchCase::IgnoreCase);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// `"entryPoint": "ir"` goldens
	// ---------------------------------------------------------------------------------------------

	/**
	 * Golden schema (every field optional):
	 *
	 *   {
	 *     "entryPoint": "ir",
	 *     "outcome": "ok" | "error",
	 *     "errorContains":   ["DSH4210"],   // substrings of the ERROR diagnostics' wire strings
	 *     "warningsContain": ["DSH8210"],
	 *     "passes": true,                   // false stops before RunDreamShaderIRPasses
	 *     "irPending": true,                // skip the `ir` compare; outcome/codes/module still run
	 *     "ir": "<DumpDreamShaderIRText output, verbatim>",
	 *     "module": {
	 *       "products": 1,
	 *       "includes": 0,
	 *       "sinks": 1,                     // products whose Graph.Sink is set
	 *       "productKinds": ["Material"],   // LexToString(EIRProductKind), in product order
	 *       "productNames": ["M_X"],
	 *       "nodeCounts": [12]              // Graph.Nodes.Num() per product
	 *     }
	 *   }
	 *
	 * `irPending` is the corpus's way of being useful before unit I1 has settled the dump's exact
	 * line format: the outcome, the codes and the structural counts are asserted from day one, and
	 * the text golden is filled in by a `-DreamShaderUpdateGolden` run once the format is real.
	 * Dropping the flag is what turns a fixture into a byte-exact golden -- an update run always
	 * writes the `ir` field, so the flag is the only thing standing between the two.
	 */
	struct FIRCorpusExpectation
	{
		FString EntryPoint;
		bool bExpectError = false;
		TArray<FString> ErrorContains;
		TArray<FString> WarningsContain;

		bool bRunPasses = true;
		bool bIRPending = false;
		bool bCheckIR = false;              FString IR;

		bool bCheckProducts = false;        int32 Products = 0;
		bool bCheckIncludes = false;        int32 Includes = 0;
		bool bCheckSinks = false;           int32 Sinks = 0;
		bool bCheckProductKinds = false;    TArray<FString> ProductKinds;
		bool bCheckProductNames = false;    TArray<FString> ProductNames;
		bool bCheckNodeCounts = false;      TArray<int32> NodeCounts;
	};

	/** The structural facts an `"module"` golden of the IR layer can assert. */
	struct FIRModuleSummary
	{
		int32 Products = 0;
		int32 Includes = 0;
		int32 Sinks = 0;
		TArray<FString> ProductKinds;
		TArray<FString> ProductNames;
		TArray<int32> NodeCounts;
	};

	inline FIRModuleSummary SummariseDreamShaderIRModule(const UE::DreamShader::IR::FIRModule& Module)
	{
		using namespace UE::DreamShader::IR;

		FIRModuleSummary Summary;
		Summary.Products = Module.Products.Num();
		Summary.Includes = Module.Includes.Num();
		for (const FIRProduct& Product : Module.Products)
		{
			Summary.ProductKinds.Add(FString(LexToString(Product.Kind)));
			Summary.ProductNames.Add(Product.Name);
			Summary.NodeCounts.Add(Product.Graph.Nodes.Num());
			if (Product.Graph.Sink != INDEX_NONE)
			{
				++Summary.Sinks;
			}
		}
		return Summary;
	}

	inline bool ParseDreamShaderIRExpectation(const FString& JsonText, FIRCorpusExpectation& Out, FString& OutError)
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

		bool BoolValue = false;
		if (Root->TryGetBoolField(TEXT("passes"), BoolValue)) { Out.bRunPasses = BoolValue; }
		if (Root->TryGetBoolField(TEXT("irPending"), BoolValue)) { Out.bIRPending = BoolValue; }

		FString StringValue;
		if (Root->TryGetStringField(TEXT("ir"), StringValue)) { Out.bCheckIR = true; Out.IR = StringValue; }

		const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
		if (Root->TryGetObjectField(TEXT("module"), ModuleObject))
		{
			double NumberValue = 0.0;
			if ((*ModuleObject)->TryGetNumberField(TEXT("products"), NumberValue)) { Out.bCheckProducts = true; Out.Products = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("includes"), NumberValue)) { Out.bCheckIncludes = true; Out.Includes = static_cast<int32>(NumberValue); }
			if ((*ModuleObject)->TryGetNumberField(TEXT("sinks"), NumberValue)) { Out.bCheckSinks = true; Out.Sinks = static_cast<int32>(NumberValue); }

			if ((*ModuleObject)->TryGetArrayField(TEXT("productKinds"), Array))
			{
				Out.bCheckProductKinds = true;
				for (const TSharedPtr<FJsonValue>& Value : *Array) { Out.ProductKinds.Add(Value->AsString()); }
			}
			if ((*ModuleObject)->TryGetArrayField(TEXT("productNames"), Array))
			{
				Out.bCheckProductNames = true;
				for (const TSharedPtr<FJsonValue>& Value : *Array) { Out.ProductNames.Add(Value->AsString()); }
			}
			if ((*ModuleObject)->TryGetArrayField(TEXT("nodeCounts"), Array))
			{
				Out.bCheckNodeCounts = true;
				for (const TSharedPtr<FJsonValue>& Value : *Array) { Out.NodeCounts.Add(static_cast<int32>(Value->AsNumber())); }
			}
		}

		return true;
	}

	/**
	 * Serialize a baseline `ir` golden from an actual run (used by -DreamShaderUpdateGolden).
	 *
	 * bPending carries the fixture's existing `irPending` flag back into the rewritten file. The
	 * flag is the review gate: an update run always writes the `ir` text, and dropping the flag at
	 * the same time would silently promote an unreviewed dump to a byte-exact golden. Deleting it is
	 * a person's job, after reading the diff.
	 */
	inline FString BuildDreamShaderIRGoldenJson(const FDreamShaderIRRun& Run, bool bRunPasses, bool bPending = false)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("ir"));
		Root->SetStringField(TEXT("outcome"), Run.Succeeded() ? TEXT("ok") : TEXT("error"));

		// Codes only, never prose -- the same rule the lang layer settled on: the code is the
		// contract, the English is not.
		if (Run.Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Error : Run.Errors)
			{
				Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Error)));
			}
			Root->SetArrayField(TEXT("errorContains"), Values);
		}
		if (Run.Warnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Warning : Run.Warnings)
			{
				Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Warning)));
			}
			Root->SetArrayField(TEXT("warningsContain"), Values);
		}

		if (!bRunPasses)
		{
			Root->SetBoolField(TEXT("passes"), false);
		}
		if (bPending)
		{
			Root->SetBoolField(TEXT("irPending"), true);
		}

		if (Run.Module.IsValid())
		{
			Root->SetStringField(TEXT("ir"), Run.IRText);

			const FIRModuleSummary Summary = SummariseDreamShaderIRModule(*Run.Module);
			const TSharedRef<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
			ModuleObject->SetNumberField(TEXT("products"), Summary.Products);
			ModuleObject->SetNumberField(TEXT("includes"), Summary.Includes);
			ModuleObject->SetNumberField(TEXT("sinks"), Summary.Sinks);

			TArray<TSharedPtr<FJsonValue>> Kinds;
			for (const FString& Kind : Summary.ProductKinds) { Kinds.Add(MakeShared<FJsonValueString>(Kind)); }
			ModuleObject->SetArrayField(TEXT("productKinds"), Kinds);

			TArray<TSharedPtr<FJsonValue>> Names;
			for (const FString& Name : Summary.ProductNames) { Names.Add(MakeShared<FJsonValueString>(Name)); }
			ModuleObject->SetArrayField(TEXT("productNames"), Names);

			TArray<TSharedPtr<FJsonValue>> Counts;
			for (const int32 Count : Summary.NodeCounts) { Counts.Add(MakeShared<FJsonValueNumber>(Count)); }
			ModuleObject->SetArrayField(TEXT("nodeCounts"), Counts);

			Root->SetObjectField(TEXT("module"), ModuleObject);
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/** The offset of the first difference between two texts, or INDEX_NONE when they are equal. */
	inline int32 FindDreamShaderTextDifference(const FString& A, const FString& B)
	{
		const int32 Common = FMath::Min(A.Len(), B.Len());
		for (int32 Index = 0; Index < Common; ++Index)
		{
			if (A[Index] != B[Index])
			{
				return Index;
			}
		}
		return (A.Len() == B.Len()) ? INDEX_NONE : Common;
	}

	/** The 1-based line a character offset falls on, and that line's text -- for a golden failure. */
	inline FString DescribeDreamShaderTextDifference(const FString& Actual, const FString& Expected)
	{
		const int32 Offset = FindDreamShaderTextDifference(Actual, Expected);
		if (Offset == INDEX_NONE)
		{
			return FString();
		}

		TArray<FString> ActualLines;
		TArray<FString> ExpectedLines;
		Actual.ParseIntoArrayLines(ActualLines, false);
		Expected.ParseIntoArrayLines(ExpectedLines, false);

		for (int32 Line = 0; Line < FMath::Max(ActualLines.Num(), ExpectedLines.Num()); ++Line)
		{
			const FString& Left = ActualLines.IsValidIndex(Line) ? ActualLines[Line] : FString();
			const FString& Right = ExpectedLines.IsValidIndex(Line) ? ExpectedLines[Line] : FString();
			if (!Left.Equals(Right, ESearchCase::CaseSensitive))
			{
				return FString::Printf(
					TEXT("line %d: actual '%s' vs expected '%s'"), Line + 1, *Left, *Right);
			}
		}

		return FString::Printf(TEXT("texts differ in length only (%d vs %d)"), Actual.Len(), Expected.Len());
	}

	/**
	 * Run one Tests/Corpus/IR fixture and assert it against its golden.
	 * In -DreamShaderUpdateGolden mode it rewrites the golden instead of asserting.
	 * Returns false only on a hard I/O failure; mismatches are recorded on Test.
	 */
	inline bool RunDreamShaderIRCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		using namespace UE::DreamShader::Lang;

		// `.dsh` headers are included BY a fixture, never run as one -- the same rule the Generate
		// layer applies, for the same reason: a header produces nothing to assert on.
		if (!Case.Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a .dss compilation unit; the IR layer skips it."), *Case.SourcePath));
			return true;
		}

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FIRCorpusExpectation Expectation;
		Expectation.bExpectError = Case.bBadByName; // default; the json may override
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
				return false;
			}

			FIRCorpusExpectation Loaded;
			Loaded.bExpectError = Case.bBadByName;
			FString JsonError;
			if (!ParseDreamShaderIRExpectation(JsonText, Loaded, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
			Expectation = MoveTemp(Loaded);
		}

		if (!Expectation.EntryPoint.IsEmpty() && !Expectation.EntryPoint.Equals(TEXT("ir"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] golden declares entryPoint '%s'; the IR corpus only runs 'ir' goldens."),
				*Case.ExpectedPath, *Expectation.EntryPoint));
			return false;
		}

		FDreamShaderIRRunOptions Options;
		Options.bRunPasses = Expectation.bRunPasses;
		Options.IncludeDirectory = FPaths::GetPath(Case.SourcePath);

		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(Case.SourcePath, SourceString, Options, Run);

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderIRGoldenJson(Run, Expectation.bRunPasses, Expectation.bIRPending);
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

		if (Expectation.bExpectError)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] the pipeline should REFUSE this source"), *Case.SourcePath),
				Run.Errors.Num() > 0);
		}
		else if (Run.Errors.Num() > 0)
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] the pipeline should SUCCEED but reported: %s"), *Case.SourcePath, *Run.ErrorText()));
			return false;
		}

		for (const FString& Needle : Expectation.ErrorContains)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] an error contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *Run.ErrorText()),
				Run.Errors.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
		}
		for (const FString& Needle : Expectation.WarningsContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] a warning contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *Run.WarningText()),
				Run.Warnings.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
		}

		if (Expectation.bExpectError)
		{
			return true;
		}

		if (!Run.Module.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("[%s] the pipeline produced no IR module."), *Case.SourcePath));
			return false;
		}

		const FIRModuleSummary Summary = SummariseDreamShaderIRModule(*Run.Module);
		if (Expectation.bCheckProducts) { Test.TestEqual(FString::Printf(TEXT("[%s] products"), *Case.SourcePath), Summary.Products, Expectation.Products); }
		if (Expectation.bCheckIncludes) { Test.TestEqual(FString::Printf(TEXT("[%s] includes"), *Case.SourcePath), Summary.Includes, Expectation.Includes); }
		if (Expectation.bCheckSinks) { Test.TestEqual(FString::Printf(TEXT("[%s] sinks"), *Case.SourcePath), Summary.Sinks, Expectation.Sinks); }

		if (Expectation.bCheckProductKinds)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] productKinds count"), *Case.SourcePath), Summary.ProductKinds.Num(), Expectation.ProductKinds.Num());
			for (int32 Index = 0; Index < FMath::Min(Summary.ProductKinds.Num(), Expectation.ProductKinds.Num()); ++Index)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] productKinds[%d] == '%s' (actual '%s')"),
						*Case.SourcePath, Index, *Expectation.ProductKinds[Index], *Summary.ProductKinds[Index]),
					Summary.ProductKinds[Index].Equals(Expectation.ProductKinds[Index], ESearchCase::CaseSensitive));
			}
		}
		if (Expectation.bCheckProductNames)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] productNames count"), *Case.SourcePath), Summary.ProductNames.Num(), Expectation.ProductNames.Num());
			for (int32 Index = 0; Index < FMath::Min(Summary.ProductNames.Num(), Expectation.ProductNames.Num()); ++Index)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] productNames[%d] == '%s' (actual '%s')"),
						*Case.SourcePath, Index, *Expectation.ProductNames[Index], *Summary.ProductNames[Index]),
					Summary.ProductNames[Index].Equals(Expectation.ProductNames[Index], ESearchCase::CaseSensitive));
			}
		}
		if (Expectation.bCheckNodeCounts)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] nodeCounts count"), *Case.SourcePath), Summary.NodeCounts.Num(), Expectation.NodeCounts.Num());
			for (int32 Index = 0; Index < FMath::Min(Summary.NodeCounts.Num(), Expectation.NodeCounts.Num()); ++Index)
			{
				Test.TestEqual(
					FString::Printf(TEXT("[%s] nodeCounts[%d]"), *Case.SourcePath, Index),
					Summary.NodeCounts[Index],
					Expectation.NodeCounts[Index]);
			}
		}

		if (Expectation.bCheckIR && !Expectation.bIRPending)
		{
			const bool bEqual = Run.IRText.Equals(Expectation.IR, ESearchCase::CaseSensitive);
			Test.TestTrue(FString::Printf(TEXT("[%s] the IR dump matches its golden"), *Case.SourcePath), bEqual);
			if (!bEqual)
			{
				Test.AddInfo(FString::Printf(
					TEXT("[%s] %s"), *Case.SourcePath, *DescribeDreamShaderTextDifference(Run.IRText, Expectation.IR)));
			}
		}
		else if (Expectation.bIRPending)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[%s] irPending: the IR text compare is skipped. Fill the golden with -DreamShaderUpdateGolden, review the diff, then drop the flag."),
				*Case.SourcePath));
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Compile layer: `dump-graph` normalisation
	// ---------------------------------------------------------------------------------------------

	/**
	 * Drop the two root-level keys of a `dump-graph` JSON that name WHERE the asset came from
	 * rather than WHAT it is: `asset` (the object path) and the `source` object (root + relative
	 * path). Everything else the dump prints is already identity-free -- coordinates, colours,
	 * guids and the engine's uniquifying name suffix are excluded by the dumper itself
	 * (Commandlet/DreamShaderGraphDump.h), which is why this is six lines and not a rewrite.
	 *
	 * Line-based because the dump is a canonical text: two-space indents, keys sorted
	 * case-sensitively at every level, LF endings. Root-level keys sit at exactly two spaces, so an
	 * `"asset":` nested inside a node's props cannot be caught by accident.
	 */
	inline FString NormaliseDreamShaderGraphDumpJson(const FString& Json)
	{
		TArray<FString> Lines;
		Json.ParseIntoArrayLines(Lines, false);

		TArray<FString> Kept;
		Kept.Reserve(Lines.Num());

		bool bInSourceObject = false;
		for (const FString& Line : Lines)
		{
			if (bInSourceObject)
			{
				// The dump closes a root-level object with exactly two spaces of indent.
				if (Line.Equals(TEXT("  },"), ESearchCase::CaseSensitive) || Line.Equals(TEXT("  }"), ESearchCase::CaseSensitive))
				{
					bInSourceObject = false;
				}
				continue;
			}

			if (Line.StartsWith(TEXT("  \"source\": {"), ESearchCase::CaseSensitive))
			{
				bInSourceObject = true;
				continue;
			}
			if (Line.StartsWith(TEXT("  \"asset\": "), ESearchCase::CaseSensitive))
			{
				continue;
			}
			// The plugin version that wrote the dump: left in, every version bump would break every golden
			// with nothing about the graph having changed.
			if (Line.StartsWith(TEXT("  \"plugin\": "), ESearchCase::CaseSensitive))
			{
				continue;
			}

			Kept.Add(Line);
		}

		return FString::Join(Kept, TEXT("\n"));
	}

	/**
	 * Drop every line whose key is one of Keys, at any depth.
	 *
	 * Only the parity oracle uses this, and only for keys that differ because the two SOURCES
	 * differ rather than because the two COMPILERS do -- a `.dsm` that spells `SortPriority=10` and
	 * a `.dss` that does not is a difference of authorship. Every use names its keys inline with
	 * the reason; there is no shared default list, because a default list is how an oracle quietly
	 * stops looking at the thing it was built to watch.
	 */
	inline FString FilterDreamShaderGraphDumpKeys(const FString& Json, const TArray<FString>& Keys)
	{
		if (Keys.Num() == 0)
		{
			return Json;
		}

		TArray<FString> Lines;
		Json.ParseIntoArrayLines(Lines, false);

		TArray<FString> Kept;
		Kept.Reserve(Lines.Num());
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStart();
			bool bDropped = false;
			for (const FString& Key : Keys)
			{
				if (Trimmed.StartsWith(FString::Printf(TEXT("\"%s\": "), *Key), ESearchCase::CaseSensitive))
				{
					bDropped = true;
					break;
				}
			}
			if (!bDropped)
			{
				Kept.Add(Line);
			}
		}

		return FString::Join(Kept, TEXT("\n"));
	}

	// ---------------------------------------------------------------------------------------------
	// Compile layer: driving one `.dss` end to end
	// ---------------------------------------------------------------------------------------------

	/**
	 * Base for the Compile corpus runner and the Compiler2 end-to-end tests.
	 *
	 * A fixture that is meant to fail logs its failure, and generation logs warnings of its own on
	 * the way past; the assertion is on the generator's bool + the diagnostics, so an incidental
	 * log line must not fail the automation test.
	 */
	class FDreamShaderCompile2CorpusTestBase : public FAutomationTestBase
	{
	public:
		FDreamShaderCompile2CorpusTestBase(const FString& InName, bool bInComplexTask)
			: FAutomationTestBase(InName, bInComplexTask)
		{
		}

		virtual bool SuppressLogErrors() override { return true; }
		virtual bool SuppressLogWarnings() override { return true; }
	};

	/** One asset a compile produced, as the golden talks about it. */
	struct FDreamShaderCompiledAsset
	{
		/** The asset's leaf name -- the stable half of its identity. */
		FString Name;
		/** `Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` / `ThinCustomInstance`. */
		FString Kind;
		/** The full object path, for cleanup and for a golden that asks for a suffix match. */
		FString ObjectPath;
		int32 NodeCount = 0;
		/** The normalised dump-graph JSON. */
		FString Dump;
	};

	/**
	 * A `.dss` fixture copied under the project's DShader root, compiled, and removed again.
	 *
	 * Its own directory per case: the asset destination follows the source path, so a directory
	 * nothing else writes into is what makes "every asset under this package path is mine" true --
	 * which is how the produced assets are discovered at all (the generator returns a bool, not a
	 * list) and how they are cleaned up without guessing their names.
	 */
	class FDreamShaderCompile2Fixture
	{
	public:
		/** RelativeName is the corpus-relative, extension-free name; it becomes the scratch subtree. */
		explicit FDreamShaderCompile2Fixture(const FString& InRelativeName, const TCHAR* InScratchArea = TEXT("Compile2"))
			: RelativeName(InRelativeName)
			, ScratchArea(InScratchArea)
		{
			const FString Sanitised = RelativeName.Replace(TEXT("\\"), TEXT("/"));
			PackagePath = FString::Printf(TEXT("/Game/DreamShaderTests/%s/%s"), *ScratchArea, *Sanitised);
			SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
				UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("DreamShaderTests"),
				ScratchArea,
				Sanitised,
				FPaths::GetCleanFilename(Sanitised) + TEXT(".dss")));
		}

		~FDreamShaderCompile2Fixture()
		{
			TArray<UObject*> ObjectsToDelete;
			for (const FString& ObjectPath : ProducedObjectPaths)
			{
				if (UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath))
				{
					ObjectsToDelete.Add(Asset);
				}
			}
			if (ObjectsToDelete.Num() > 0)
			{
				ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
			}

			IFileManager::Get().Delete(*SourceFilePath, false, true);
			IFileManager::Get().DeleteDirectory(*FPaths::GetPath(SourceFilePath), false, true);
		}

		const FString& GetSourceFilePath() const { return SourceFilePath; }
		const FString& GetPackagePath() const { return PackagePath; }

		bool WriteSource(FAutomationTestBase& Test, const FString& SourceText) const
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourceFilePath), true);
			if (!FFileHelper::SaveStringToFile(SourceText, *SourceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(FString::Printf(TEXT("Failed to write the compile fixture source for '%s'."), *RelativeName));
				return false;
			}
			return true;
		}

		/** Everything the asset registry can see under this fixture's package path, dumped. */
		void CollectProducedAssets(TArray<FDreamShaderCompiledAsset>& OutAssets)
		{
			OutAssets.Reset();

			FAssetRegistryModule& AssetRegistryModule =
				FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			TArray<FString> ScanPaths;
			ScanPaths.Add(PackagePath);
			AssetRegistry.ScanPathsSynchronous(ScanPaths, /*bForceRescan*/ true);

			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*PackagePath));
			Filter.bRecursivePaths = true;

			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);

			for (const FAssetData& AssetData : Assets)
			{
				UObject* Asset = AssetData.GetAsset();
				if (!Asset)
				{
					continue;
				}

				FDreamShaderCompiledAsset Produced;
				Produced.Name = Asset->GetName();
				Produced.ObjectPath = Asset->GetPathName();
				Produced.Kind = Asset->GetClass()->GetName();

				int32 NodeCount = 0;
				const FString Dump = UE::DreamShader::Editor::Private::BuildDreamShaderGraphDumpJson(
					Asset, /*SourceFilePath*/ FString(), &NodeCount);
				Produced.NodeCount = NodeCount;
				Produced.Dump = NormaliseDreamShaderGraphDumpJson(Dump);
				// A call into a sibling product names this fixture's scratch package; the golden must not know
				// where the runner put the fixture.
				Produced.Dump.ReplaceInline(*(PackagePath + TEXT("/")), TEXT("<package>/"), ESearchCase::CaseSensitive);

				// The dump names the kind the way the golden does (`Material`,
				// `MaterialFunction`, `MaterialLayer`, `MaterialLayerBlend`, `ThinCustomInstance`);
				// the UClass name is only the fallback for an asset the dump does not cover.
				//
				// The ROOT kind only. The dump sorts keys at every level, so a ThinCustom instance's
				// `instance.parent.kind` -- the hidden base's UClass, "Material" -- is written before the
				// root `kind`, and the first match used to be that one. Root members are exactly the lines
				// indented by two spaces (FDumpJson::WriteObject).
				const FString KindNeedle = TEXT("\n  \"kind\": \"");
				int32 KindIndex = Dump.Find(KindNeedle, ESearchCase::CaseSensitive);
				if (KindIndex != INDEX_NONE)
				{
					const int32 Start = KindIndex + KindNeedle.Len();
					const int32 End = Dump.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
					if (End != INDEX_NONE)
					{
						Produced.Kind = Dump.Mid(Start, End - Start);
					}
				}

				ProducedObjectPaths.AddUnique(Produced.ObjectPath);
				OutAssets.Add(MoveTemp(Produced));
			}

			OutAssets.Sort([](const FDreamShaderCompiledAsset& A, const FDreamShaderCompiledAsset& B)
			{
				return A.Name.Compare(B.Name, ESearchCase::CaseSensitive) < 0;
			});
		}

		/** Remember an asset for cleanup even when the registry never saw it. */
		void TrackObjectPath(const FString& ObjectPath) { ProducedObjectPaths.AddUnique(ObjectPath); }

	private:
		FString RelativeName;
		FString ScratchArea;
		FString PackagePath;
		FString SourceFilePath;
		TArray<FString> ProducedObjectPaths;
	};

	/** The assets' dumps, concatenated under a stable header, so one string is one golden. */
	inline FString BuildDreamShaderCompiledGraphDumpText(const TArray<FDreamShaderCompiledAsset>& Assets)
	{
		TArray<FString> Parts;
		for (const FDreamShaderCompiledAsset& Asset : Assets)
		{
			Parts.Add(FString::Printf(TEXT("=== %s (%s) ==="), *Asset.Name, *Asset.Kind));
			Parts.Add(Asset.Dump);
		}
		return FString::Join(Parts, TEXT("\n"));
	}

	// ---------------------------------------------------------------------------------------------
	// `"entryPoint": "compile"` goldens
	// ---------------------------------------------------------------------------------------------

	/** One `assets[]` entry of a compile golden. */
	struct FCompileAssetExpectation
	{
		FString Name;
		bool bCheckKind = false;       FString Kind;
		bool bCheckNodeCount = false;  int32 NodeCount = 0;
		/** Optional: asserted as a SUFFIX of the real object path, so the scratch root stays free. */
		bool bCheckPath = false;       FString Path;
	};

	/**
	 * Golden schema (every field optional):
	 *
	 *   {
	 *     "entryPoint": "compile",
	 *     "outcome": "ok" | "error",
	 *     "errorContains": ["DSH8290"],
	 *     "graphPending": true,
	 *     "assets": [ { "name": "M_X", "kind": "Material", "nodeCount": 7, "path": "M_X.M_X" } ],
	 *     "graphDump": "<normalised dump-graph JSON of every asset, in name order>"
	 *   }
	 *
	 * `name` is the leaf, not the package path: the runner copies the fixture into a scratch tree
	 * whose location is an implementation detail of the runner, and a golden that spelled the whole
	 * object path would be asserting on that detail. `path`, when a fixture DOES care (a
	 * `/// @name /Game/...` override), is matched as a suffix.
	 */
	struct FCompileCorpusExpectation
	{
		FString EntryPoint;
		bool bExpectError = false;
		TArray<FString> ErrorContains;

		bool bGraphPending = false;
		bool bCheckGraphDump = false;   FString GraphDump;
		bool bCheckAssets = false;      TArray<FCompileAssetExpectation> Assets;
	};

	inline bool ParseDreamShaderCompileExpectation(const FString& JsonText, FCompileCorpusExpectation& Out, FString& OutError)
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

		bool BoolValue = false;
		if (Root->TryGetBoolField(TEXT("graphPending"), BoolValue)) { Out.bGraphPending = BoolValue; }

		FString StringValue;
		if (Root->TryGetStringField(TEXT("graphDump"), StringValue)) { Out.bCheckGraphDump = true; Out.GraphDump = StringValue; }

		if (Root->TryGetArrayField(TEXT("assets"), Array))
		{
			Out.bCheckAssets = true;
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object)
				{
					continue;
				}

				FCompileAssetExpectation Asset;
				(*Object)->TryGetStringField(TEXT("name"), Asset.Name);

				double NumberValue = 0.0;
				if ((*Object)->TryGetStringField(TEXT("kind"), StringValue)) { Asset.bCheckKind = true; Asset.Kind = StringValue; }
				if ((*Object)->TryGetNumberField(TEXT("nodeCount"), NumberValue)) { Asset.bCheckNodeCount = true; Asset.NodeCount = static_cast<int32>(NumberValue); }
				if ((*Object)->TryGetStringField(TEXT("path"), StringValue)) { Asset.bCheckPath = true; Asset.Path = StringValue; }

				Out.Assets.Add(MoveTemp(Asset));
			}
		}

		return true;
	}

	/** bPending carries `graphPending` back, for the same reason `irPending` is carried back. */
	inline FString BuildDreamShaderCompileGoldenJson(
		bool bCompiled,
		const UE::DreamShader::FDreamShaderError& Error,
		const TArray<FDreamShaderCompiledAsset>& Assets,
		bool bPending = false)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("compile"));
		Root->SetStringField(TEXT("outcome"), bCompiled ? TEXT("ok") : TEXT("error"));

		if (!bCompiled)
		{
			// The code, never the message -- the 1.x wire form carries both and only one of them
			// is a contract.
			TArray<TSharedPtr<FJsonValue>> Values;
			Values.Add(MakeShared<FJsonValueString>(Error.Code.IsEmpty() ? Error.Message : Error.Code));
			Root->SetArrayField(TEXT("errorContains"), Values);
		}

		if (bPending)
		{
			Root->SetBoolField(TEXT("graphPending"), true);
		}

		TArray<TSharedPtr<FJsonValue>> AssetValues;
		for (const FDreamShaderCompiledAsset& Asset : Assets)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("name"), Asset.Name);
			Object->SetStringField(TEXT("kind"), Asset.Kind);
			Object->SetNumberField(TEXT("nodeCount"), Asset.NodeCount);
			AssetValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("assets"), AssetValues);

		if (Assets.Num() > 0)
		{
			Root->SetStringField(TEXT("graphDump"), BuildDreamShaderCompiledGraphDumpText(Assets));
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Run one Tests/Corpus/Compile fixture end to end and assert it against its golden.
	 *
	 * The fixture is COPIED under the project's DShader root before compiling: the 2.0 pipeline has
	 * no transient request (the compiler pipeline header says so), the asset destination follows
	 * the source path, and a corpus directory is not a source root. The copy and every asset it
	 * produced are deleted by the fixture's destructor whatever happens in between.
	 */
	inline bool RunDreamShaderCompileCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		if (!Case.Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a .dss compilation unit; the Compile layer skips it."), *Case.SourcePath));
			return true;
		}

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		// MakeDreamShaderCorpusCase (the shared helper the complex runner's RunTest rebuilds a case
		// with) fills SourcePath, Extension and the expectation path but NOT RelativeName -- only the
		// enumerator LoadDreamShaderCorpusCases does that, and RunTest only ever gets the source path
		// back. Deriving it here is what keeps every fixture in a scratch directory of its own; an
		// empty name would put all of them in /Game/DreamShaderTests/Compile2 and write every source
		// to the same `.dss`.
		FString RelativeName = Case.RelativeName;
		if (RelativeName.IsEmpty())
		{
			RelativeName = Case.SourcePath;
			const FString LayerRoot = FPaths::Combine(GetDreamShaderCorpusRoot(), TEXT("Compile")) / TEXT("");
			FPaths::MakePathRelativeTo(RelativeName, *LayerRoot);
			RelativeName = FPaths::GetBaseFilename(RelativeName, /*bRemovePath*/ false);
		}

		FCompileCorpusExpectation Expectation;
		Expectation.bExpectError = Case.bBadByName;
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
				return false;
			}

			FCompileCorpusExpectation Loaded;
			Loaded.bExpectError = Case.bBadByName;
			FString JsonError;
			if (!ParseDreamShaderCompileExpectation(JsonText, Loaded, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
			Expectation = MoveTemp(Loaded);
		}

		if (!Expectation.EntryPoint.IsEmpty() && !Expectation.EntryPoint.Equals(TEXT("compile"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] golden declares entryPoint '%s'; the Compile corpus only runs 'compile' goldens."),
				*Case.ExpectedPath, *Expectation.EntryPoint));
			return false;
		}

		// The goldens describe GRAPH-backend output unless the fixture itself asks for ThinCustom,
		// exactly as the Generate layer's do.
		FScopedDreamShaderGraphBackendPin BackendPin;

		FDreamShaderCompile2Fixture Fixture(RelativeName);
		// Suppression, not a requirement: the new-asset probe fires for some engine builds and not
		// others. Never quote the package path in an assertion message below -- the harness would
		// swallow the failure along with the probe.
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		if (!Fixture.WriteSource(Test, SourceString))
		{
			return false;
		}

		UE::DreamShader::FDreamShaderError Error;
		const bool bCompiled = FMaterialGenerator::GenerateAssetsFromFile(
			Fixture.GetSourceFilePath(), Error, /*bForce*/ true, /*bAllowEphemeralThinCustom*/ false);

		TArray<FDreamShaderCompiledAsset> Assets;
		Fixture.CollectProducedAssets(Assets);

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderCompileGoldenJson(bCompiled, Error, Assets, Expectation.bGraphPending);
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

		const FString ErrorText = Error.Code.IsEmpty()
			? Error.Message
			: FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message);

		if (Expectation.bExpectError)
		{
			Test.TestFalse(FString::Printf(TEXT("[%s] the compile should FAIL"), *RelativeName), bCompiled);
		}
		else if (!bCompiled)
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] the compile should SUCCEED but failed: %s"), *RelativeName, *ErrorText));
			return false;
		}

		for (const FString& Needle : Expectation.ErrorContains)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] the error contains '%s' (actual: %s)"), *RelativeName, *Needle, *ErrorText),
				ErrorText.Contains(Needle, ESearchCase::CaseSensitive));
		}

		if (Expectation.bExpectError)
		{
			return true;
		}

		if (Expectation.bCheckAssets)
		{
			Test.TestEqual(
				FString::Printf(TEXT("[%s] produced asset count"), *RelativeName),
				Assets.Num(),
				Expectation.Assets.Num());

			for (const FCompileAssetExpectation& Expected : Expectation.Assets)
			{
				const FDreamShaderCompiledAsset* Actual = Assets.FindByPredicate(
					[&Expected](const FDreamShaderCompiledAsset& Candidate)
					{
						return Candidate.Name.Equals(Expected.Name, ESearchCase::CaseSensitive);
					});

				if (!Test.TestNotNull(
						FString::Printf(TEXT("[%s] produced an asset named '%s'"), *RelativeName, *Expected.Name),
						Actual))
				{
					continue;
				}

				if (Expected.bCheckKind)
				{
					Test.TestTrue(
						FString::Printf(TEXT("[%s] '%s' kind == '%s' (actual '%s')"),
							*RelativeName, *Expected.Name, *Expected.Kind, *Actual->Kind),
						Actual->Kind.Equals(Expected.Kind, ESearchCase::CaseSensitive));
				}
				if (Expected.bCheckNodeCount)
				{
					Test.TestEqual(
						FString::Printf(TEXT("[%s] '%s' nodeCount"), *RelativeName, *Expected.Name),
						Actual->NodeCount,
						Expected.NodeCount);
				}
				if (Expected.bCheckPath)
				{
					Test.TestTrue(
						FString::Printf(TEXT("[%s] '%s' object path ends with the expected suffix"), *RelativeName, *Expected.Name),
						Actual->ObjectPath.EndsWith(Expected.Path, ESearchCase::CaseSensitive));
				}
			}
		}

		if (Expectation.bCheckGraphDump && !Expectation.bGraphPending)
		{
			const FString Actual = BuildDreamShaderCompiledGraphDumpText(Assets);
			const bool bEqual = Actual.Equals(Expectation.GraphDump, ESearchCase::CaseSensitive);
			Test.TestTrue(FString::Printf(TEXT("[%s] the graph dump matches its golden"), *RelativeName), bEqual);
			if (!bEqual)
			{
				Test.AddInfo(FString::Printf(
					TEXT("[%s] %s"), *RelativeName, *DescribeDreamShaderTextDifference(Actual, Expectation.GraphDump)));
			}
		}
		else if (Expectation.bGraphPending)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[%s] graphPending: the graph-dump compare is skipped. Fill the golden with -DreamShaderUpdateGolden, review the diff, then drop the flag."),
				*RelativeName));
		}

		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
