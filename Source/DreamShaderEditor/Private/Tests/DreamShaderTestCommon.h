// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Shared helpers for the DreamShader data-driven test corpus.
//
// The corpus lives on disk under <DreamShaderPlugin>/Tests/Corpus/<Layer>/ as a tree of
// .dsm / .dsf / .dsh fixtures, each optionally paired with a <name>.expected.json golden file.
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
		FString SourcePath;          // absolute path to the .dsm/.dsf/.dsh fixture
		FString ExpectedPath;        // absolute path to <name>.expected.json (may not exist)
		FString RelativeName;        // path relative to the layer dir, extension stripped (for the test name)
		FString Extension;           // "dsm" / "dsf" / "dsh"
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

	/** Enumerate every .dsm/.dsf/.dsh fixture under Corpus/<SubDir>. */
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
