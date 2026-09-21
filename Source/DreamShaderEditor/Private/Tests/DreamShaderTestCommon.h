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
//
// The 1.x runtime parser and the FMaterialGenerator facade are deleted. The Parse layer
// runs through the legacy front end, and every compile in the tests goes through the compile facade
// below (CompileDreamShaderTestAssets / CompileDreamShaderTestMaterial), which asks the registered
// compiler service.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderSettings.h"
#include "DreamShaderTypes.h"
#include "DreamShaderCompilerService.h"
// IDreamShaderCompiler, GetDreamShaderCompiler, EThinCustomPersistence: the compile facade.
#include "DreamShaderCompilerInterface.h"
// ToInvariantWireString: the facade's FString message, in the invariant English a golden can quote.
#include "DreamShaderTextWireUtils.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

// The IR and Compile corpus layers at the end of this file need the 2.0 front end's semantic and IR
// headers, plus the `dump-graph` builder that is the Compile layer's golden format.
#include "IR/IR.h"
#include "IR/IRBuilder.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRDump.h"
#include "IR/IRInstanceSchema.h"
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

	/** Enumerate every .dsm/.dsf/.dsh/.dss/.dsi fixture under Corpus/<SubDir>. Each runner skips the extensions it does not run. */
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
		FM.FindFilesRecursive(Files, *Dir, TEXT("*.dsi"), true, false, false);
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

	// ---------------------------------------------------------------------------------------------
	// The compile facade
	//
	// The 1.x FMaterialGenerator facade is deleted. Every production caller now asks the registered
	// compiler -- ::UE::DreamShader::GetDreamShaderCompiler(), implemented by the compiler module's
	// service -- with an explicit ThinCustom persistence, and the tests do the same through these
	// helpers. They keep the facade's argument order and its three message shapes, so a call site
	// changes its name and nothing else:
	//
	//   FMaterialGenerator::GenerateAssetsFromFile(Path, Message, bForce, bTransient)
	//     -> CompileDreamShaderTestAssets(Path, Message, bForce, bEphemeralThinCustom)
	//
	// bEphemeralThinCustom asks for EThinCustomPersistence::Ephemeral. Only a ThinCustom product has a
	// memory-only state: a Graph material and a material function always save, whatever is asked, so a
	// test that built a Graph material "transient" under 1.x now writes a real asset and needs a scratch
	// package root (FDreamShaderCompile2Fixture) that cleans it up.
	//
	// CompileMaterial and CompileAssets are one compile in the service (CO-report); both are kept so a
	// call site still says which one it meant.
	// ---------------------------------------------------------------------------------------------

	/** One compile through the registered compiler. A null compiler answers a failed result that says so. */
	inline ::UE::DreamShader::FDreamShaderCompileResult CompileDreamShaderTestSource(
		const FString& SourceFilePath,
		const bool bMaterialOnly,
		const bool bForce,
		const bool bEphemeralThinCustom)
	{
		::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
		if (!Compiler)
		{
			::UE::DreamShader::FDreamShaderCompileResult Unavailable;
			Unavailable.bSucceeded = false;
			Unavailable.Message = FText::FromString(TEXT("No DreamShader compiler is registered: GetDreamShaderCompiler() answered null."));
			return Unavailable;
		}

		::UE::DreamShader::FDreamShaderCompileRequest Request;
		Request.SourceFilePath = SourceFilePath;
		Request.bForce = bForce;
		Request.ThinCustomPersistence = bEphemeralThinCustom
			? ::UE::DreamShader::EThinCustomPersistence::Ephemeral
			: ::UE::DreamShader::EThinCustomPersistence::Materialized;
		return bMaterialOnly ? Compiler->CompileMaterial(Request) : Compiler->CompileAssets(Request);
	}

	/** Every product of the source; the report in its invariant wire form (what the facade's FString overload gave). */
	inline bool CompileDreamShaderTestAssets(
		const FString& SourceFilePath,
		FString& OutMessage,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ false, bForce, bEphemeralThinCustom);
		OutMessage = ::UE::DreamShader::Editor::Private::ToInvariantWireString(Result.Message);
		return Result.bSucceeded;
	}

	/** Every product of the source; the report as the service worded it. */
	inline bool CompileDreamShaderTestAssets(
		const FString& SourceFilePath,
		FText& OutMessage,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ false, bForce, bEphemeralThinCustom);
		OutMessage = Result.Message;
		return Result.bSucceeded;
	}

	/** Every product of the source; the first error's DSHnnnn code beside the report text. */
	inline bool CompileDreamShaderTestAssets(
		const FString& SourceFilePath,
		::UE::DreamShader::FDreamShaderError& OutError,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ false, bForce, bEphemeralThinCustom);
		OutError.Code = Result.Code;
		OutError.Message = ::UE::DreamShader::Editor::Private::ToInvariantWireString(Result.Message);
		return Result.bSucceeded;
	}

	/** The material route (IDreamShaderCompiler::CompileMaterial); the report in its invariant wire form. */
	inline bool CompileDreamShaderTestMaterial(
		const FString& SourceFilePath,
		FString& OutMessage,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ true, bForce, bEphemeralThinCustom);
		OutMessage = ::UE::DreamShader::Editor::Private::ToInvariantWireString(Result.Message);
		return Result.bSucceeded;
	}

	/** The material route; the report as the service worded it. */
	inline bool CompileDreamShaderTestMaterial(
		const FString& SourceFilePath,
		FText& OutMessage,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ true, bForce, bEphemeralThinCustom);
		OutMessage = Result.Message;
		return Result.bSucceeded;
	}

	/** The material route; the first error's DSHnnnn code beside the report text. */
	inline bool CompileDreamShaderTestMaterial(
		const FString& SourceFilePath,
		::UE::DreamShader::FDreamShaderError& OutError,
		const bool bForce = false,
		const bool bEphemeralThinCustom = false)
	{
		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			CompileDreamShaderTestSource(SourceFilePath, /*bMaterialOnly*/ true, bForce, bEphemeralThinCustom);
		OutError.Code = Result.Code;
		OutError.Message = ::UE::DreamShader::Editor::Private::ToInvariantWireString(Result.Message);
		return Result.bSucceeded;
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

	// ---------------------------------------------------------------------------------------------
	// The project's default backend, pinned
	//
	// (The Generate layer that lived here drove the 1.x generator. Its fixtures moved to
	// Tests/Corpus/Legacy/Compile in 2.0 and run through the Compile layer's runner below.)
	// ---------------------------------------------------------------------------------------------

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

	/**
	 * A compile a test needs to have SUCCEEDED. What the compiler said goes out as an Info line and the error names no
	 * path: a fixture registers its package path as an expected error (the new-asset probes quote it), and an error line
	 * that quoted the path too would be swallowed whole -- the test then fails with "no errors were logged".
	 */
	inline bool ExpectDreamShaderTestCompile(FAutomationTestBase& Test, const TCHAR* What, const bool bCompiled, const UE::DreamShader::FDreamShaderError& Error)
	{
		if (!bCompiled)
		{
			Test.AddInfo(FString::Printf(TEXT("%s -- the compiler said: %s: %s"), What, *Error.Code, *Error.Message));
			Test.AddError(FString::Printf(TEXT("%s: the compile failed; the info line above has what the compiler said."), What));
		}
		return bCompiled;
	}

	/** A compile a test needs to have FAILED with Code. Same reason for the Info line. */
	inline bool ExpectDreamShaderTestRefusal(FAutomationTestBase& Test, const TCHAR* What, const bool bCompiled, const UE::DreamShader::FDreamShaderError& Error, const TCHAR* Code)
	{
		const bool bRefused = !bCompiled && (Error.Code.Equals(Code, ESearchCase::CaseSensitive) || Error.Message.Contains(Code, ESearchCase::CaseSensitive));
		if (!bRefused)
		{
			Test.AddInfo(FString::Printf(TEXT("%s -- compiled=%d, the compiler said: %s: %s"), What, bCompiled ? 1 : 0, *Error.Code, *Error.Message));
			Test.AddError(FString::Printf(TEXT("%s: expected a refusal with %s; the info line above has what happened."), What, Code));
		}
		return bRefused;
	}

	/** The same pin for any backend: a corpus layer names the one its goldens were captured under. */
	struct FScopedDreamShaderBackendPin
	{
		EDreamShaderDefaultBackend SavedDefaultBackend;

		explicit FScopedDreamShaderBackendPin(const EDreamShaderDefaultBackend Backend)
			: SavedDefaultBackend(GetMutableDefault<UDreamShaderSettings>()->DefaultBackend)
		{
			GetMutableDefault<UDreamShaderSettings>()->DefaultBackend = Backend;
		}

		~FScopedDreamShaderBackendPin()
		{
			GetMutableDefault<UDreamShaderSettings>()->DefaultBackend = SavedDefaultBackend;
		}
	};

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

	// ---------------------------------------------------------------------------------------------
	// Parse layer (Tests/Corpus/Parse/**), retargeted in 2.0 to the legacy front end.
	//
	// The 1.x runtime parser (FTextShaderParser) is deleted, so these fixtures are the parse-equivalence
	// set of research-legacy.md section 7 item 2: the legacy front end has to accept or refuse each one
	// exactly as the 1.x parser did, and the goldens plus the `.bad.` names record which. The parse is
	// ParseDreamShaderLang with the Auto front end -- a `.dsm`/`.dsf` goes to the legacy parser, a `.dsh`
	// declaration by declaration -- which is the parse a compile makes. No preprocessing, as before.
	//
	// A `"parse"` golden keeps its schema. `errorContains` names the legacy front end's codes (matched
	// ignoring case, as the 1.x layer did), and every `definition` field is now read off the AST and
	// FLegacyMigrationInfo by SummariseDreamShaderLegacyParse; its comments say how each maps to the
	// FTextShaderDefinition field of the same name.
	// ---------------------------------------------------------------------------------------------

	/** What a `"parse"` golden's `definition` object talks about, derived from one legacy parse. */
	struct FLegacyParseSummary
	{
		/** FLegacyBlock::Name of the first product block (Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend). */
		FString Name;
		/** The arguments of the first `#pragma material` (the Shader's Settings), as written. */
		TArray<TPair<FString, FString>> Settings;
		/** The first Shader block's Outputs declarations: FLegacyBlock::OutputNames. */
		int32 OutputDeclarations = 0;
		/**
		 * The first Shader block's Outputs bindings, counted the way FTextShaderDefinition::Outputs counted them:
		 * one per `Base.X = ...` assignment and one per `Pin[i] = ...` argument of an `Expression(...)` target,
		 * among the statements of the entry body whose span lies inside the Outputs section.
		 */
		int32 Outputs = 0;
		/** ShaderFunction / ShaderLayer / ShaderLayerBlend blocks. */
		int32 MaterialFunctions = 0;
		/** The first of those blocks' FLegacyBlock::BlockWord. */
		FString MaterialFunction0Kind;
		/** VirtualFunction blocks. */
		int32 VirtualFunctions = 0;
		/** The first product block's Graph section holds anything besides whitespace and its braces. */
		bool bCodeNotEmpty = false;
	};

	/** Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend: the 1.x block words that produce an asset. */
	inline bool IsDreamShaderLegacyProductBlockWord(const FString& Word, bool& bOutFunctionBlock)
	{
		bOutFunctionBlock = Word.Equals(TEXT("ShaderFunction"), ESearchCase::IgnoreCase)
			|| Word.Equals(TEXT("ShaderLayer"), ESearchCase::IgnoreCase)
			|| Word.Equals(TEXT("ShaderLayerBlend"), ESearchCase::IgnoreCase)
			|| Word.Equals(TEXT("MaterialLayer"), ESearchCase::IgnoreCase)
			|| Word.Equals(TEXT("MaterialLayerBlend"), ESearchCase::IgnoreCase);
		return bOutFunctionBlock || Word.Equals(TEXT("Shader"), ESearchCase::IgnoreCase);
	}

	/** The section of one legacy block with this name, ignoring case (1.x section names are case-insensitive); null when absent. */
	inline const UE::DreamShader::Lang::FLegacySection* FindDreamShaderLegacySection(
		const UE::DreamShader::Lang::FLegacyMigrationInfo& Info,
		const UE::DreamShader::Lang::FDecl* Block,
		const TCHAR* SectionName)
	{
		for (const UE::DreamShader::Lang::FLegacySection& Section : Info.Sections)
		{
			if (Section.Block == Block && Section.Name.Equals(SectionName, ESearchCase::IgnoreCase))
			{
				return &Section;
			}
		}
		return nullptr;
	}

	/** Derive a FLegacyParseSummary from one parse and the text it parsed. */
	inline FLegacyParseSummary SummariseDreamShaderLegacyParse(
		const UE::DreamShader::Lang::FLangParseResult& Result,
		const FString& SourceText)
	{
		using namespace UE::DreamShader::Lang;

		FLegacyParseSummary Summary;
		if (!Result.Module.IsValid())
		{
			return Summary;
		}

		for (const FDeclPtr& Decl : Result.Module->Declarations)
		{
			const FPragmaDecl* Pragma = Decl.IsValid() ? Decl->As<FPragmaDecl>() : nullptr;
			if (Pragma && Pragma->PragmaKind == EPragmaKind::Material)
			{
				for (const FPragmaArgument& Argument : Pragma->Arguments)
				{
					Summary.Settings.Emplace(Argument.Key, Argument.Value);
				}
				break;
			}
		}

		if (!Result.Legacy.IsValid())
		{
			return Summary;
		}
		const FLegacyMigrationInfo& Info = *Result.Legacy;

		const FLegacyBlock* Product = nullptr;
		const FLegacyBlock* Shader = nullptr;
		for (const FLegacyBlock& Block : Info.Blocks)
		{
			bool bFunctionBlock = false;
			const bool bProductBlock = IsDreamShaderLegacyProductBlockWord(Block.BlockWord, bFunctionBlock);
			if (bFunctionBlock)
			{
				if (Summary.MaterialFunctions == 0)
				{
					Summary.MaterialFunction0Kind = Block.BlockWord;
				}
				++Summary.MaterialFunctions;
			}
			if (Block.BlockWord.Equals(TEXT("VirtualFunction"), ESearchCase::IgnoreCase))
			{
				++Summary.VirtualFunctions;
			}
			if (bProductBlock && !Product)
			{
				Product = &Block;
			}
			if (!bFunctionBlock && bProductBlock && !Shader)
			{
				Shader = &Block;
			}
		}

		if (Product)
		{
			Summary.Name = Product->Name;
			if (const FLegacySection* Graph = FindDreamShaderLegacySection(Info, Product->Decl, TEXT("Graph")))
			{
				const FString Body = SourceText.Mid(Graph->BodySpan.Offset, Graph->BodySpan.Length);
				for (const TCHAR Character : Body)
				{
					if (!FChar::IsWhitespace(Character) && Character != TEXT('{') && Character != TEXT('}'))
					{
						Summary.bCodeNotEmpty = true;
						break;
					}
				}
			}
		}

		if (Shader)
		{
			Summary.OutputDeclarations = Shader->OutputNames.Num();

			const FFunctionDecl* Entry = Shader->Decl ? Shader->Decl->As<FFunctionDecl>() : nullptr;
			const FLegacySection* OutputsSection = FindDreamShaderLegacySection(Info, Shader->Decl, TEXT("Outputs"));
			if (Entry && Entry->Body.IsValid() && OutputsSection)
			{
				for (const FStmtPtr& Statement : Entry->Body->Statements)
				{
					if (!Statement.IsValid()
						|| Statement->Span.Offset < OutputsSection->BodySpan.Offset
						|| Statement->Span.Offset >= OutputsSection->BodySpan.End())
					{
						continue;
					}

					const FExprStmt* ExpressionStatement = Statement->As<FExprStmt>();
					if (!ExpressionStatement || !ExpressionStatement->Expression.IsValid())
					{
						continue;
					}

					if (const FAssignExpr* Assign = ExpressionStatement->Expression->As<FAssignExpr>())
					{
						const FMemberExpr* Target = Assign->Target.IsValid() ? Assign->Target->As<FMemberExpr>() : nullptr;
						const FIdentifierExpr* Object = (Target && Target->Object.IsValid()) ? Target->Object->As<FIdentifierExpr>() : nullptr;
						if (Object && Object->Name.Equals(TEXT("Base"), ESearchCase::CaseSensitive))
						{
							++Summary.Outputs;
						}
					}
					else if (const FCallExpr* Call = ExpressionStatement->Expression->As<FCallExpr>())
					{
						for (const FArgument& Argument : Call->Arguments)
						{
							if (Argument.PinIndex != INDEX_NONE)
							{
								++Summary.Outputs;
							}
						}
					}
				}
			}
		}

		return Summary;
	}

	/** Serialize a baseline `parse` golden from an actual legacy parse (used by -DreamShaderUpdateGolden). Codes, never prose. */
	inline FString BuildDreamShaderGoldenJson(
		const bool bParsed,
		const FLegacyParseSummary& Summary,
		const TArray<FString>& Errors,
		const TArray<FString>& Warnings)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("parse"));
		Root->SetStringField(TEXT("outcome"), bParsed ? TEXT("ok") : TEXT("error"));

		if (!bParsed)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Error : Errors)
			{
				Values.AddUnique(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Error)));
			}
			Root->SetArrayField(TEXT("errorContains"), Values);
		}
		else
		{
			const TSharedRef<FJsonObject> Def = MakeShared<FJsonObject>();
			if (!Summary.Name.IsEmpty())
			{
				Def->SetStringField(TEXT("name"), Summary.Name);
			}
			Def->SetNumberField(TEXT("outputDeclarations"), Summary.OutputDeclarations);
			Def->SetNumberField(TEXT("outputs"), Summary.Outputs);
			Def->SetNumberField(TEXT("materialFunctions"), Summary.MaterialFunctions);
			if (Summary.MaterialFunctions > 0)
			{
				Def->SetStringField(TEXT("materialFunction0Kind"), Summary.MaterialFunction0Kind);
			}
			Def->SetNumberField(TEXT("virtualFunctions"), Summary.VirtualFunctions);
			Def->SetBoolField(TEXT("codeNotEmpty"), Summary.bCodeNotEmpty);

			if (Summary.Settings.Num() > 0)
			{
				const TSharedRef<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
				for (const TPair<FString, FString>& Pair : Summary.Settings)
				{
					SettingsObject->SetStringField(Pair.Key, Pair.Value);
				}
				Def->SetObjectField(TEXT("settings"), SettingsObject);
			}

			Root->SetObjectField(TEXT("definition"), Def);

			if (Warnings.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> Values;
				for (const FString& Warning : Warnings)
				{
					Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Warning)));
				}
				Root->SetArrayField(TEXT("warningsContain"), Values);
			}
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Run one Tests/Corpus/Parse fixture through the legacy front end and assert it against its golden.
	 * In -DreamShaderUpdateGolden mode it rewrites the golden instead of asserting.
	 * Returns false only on a hard I/O failure; semantic mismatches are recorded on Test.
	 */
	inline bool RunDreamShaderParseCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
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
		const bool bParsed = Result.Succeeded();
		const FLegacyParseSummary Summary = SummariseDreamShaderLegacyParse(Result, SourceString);

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderGoldenJson(bParsed, Summary, Errors, Warnings);
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
			Test.TestFalse(FString::Printf(TEXT("[%s] the legacy front end should REFUSE this source"), *Case.SourcePath), bParsed);
			for (const FString& Needle : Expectation.ErrorContains)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] an error contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *ErrorText),
					Errors.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::IgnoreCase); }));
			}
			return true;
		}

		if (!bParsed)
		{
			Test.AddError(FString::Printf(TEXT("[%s] the legacy front end should ACCEPT this source but reported: %s"), *Case.SourcePath, *ErrorText));
			return false;
		}

		if (Expectation.bCheckName)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] name == '%s' (actual '%s')"), *Case.SourcePath, *Expectation.Name, *Summary.Name),
				Summary.Name.Equals(Expectation.Name, ESearchCase::CaseSensitive));
		}
		if (Expectation.bCheckOutputDeclarations)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] outputDeclarations"), *Case.SourcePath), Summary.OutputDeclarations, Expectation.OutputDeclarations);
		}
		if (Expectation.bCheckOutputs)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] outputs"), *Case.SourcePath), Summary.Outputs, Expectation.Outputs);
		}
		if (Expectation.bCheckMaterialFunctions)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] materialFunctions"), *Case.SourcePath), Summary.MaterialFunctions, Expectation.MaterialFunctions);
		}
		if (Expectation.bCheckMaterialFunction0Kind && Summary.MaterialFunctions > 0)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] materialFunction0Kind"), *Case.SourcePath), Summary.MaterialFunction0Kind, Expectation.MaterialFunction0Kind);
		}
		if (Expectation.bCheckVirtualFunctions)
		{
			Test.TestEqual(FString::Printf(TEXT("[%s] virtualFunctions"), *Case.SourcePath), Summary.VirtualFunctions, Expectation.VirtualFunctions);
		}
		if (Expectation.bCheckCodeNotEmpty)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] codeNotEmpty == %s"), *Case.SourcePath, Expectation.bCodeNotEmpty ? TEXT("true") : TEXT("false")),
				Summary.bCodeNotEmpty == Expectation.bCodeNotEmpty);
		}
		for (const TPair<FString, FString>& Pair : Expectation.Settings)
		{
			// The key ignores case (1.x setting keys did), the value is exact.
			const TPair<FString, FString>* Found = Summary.Settings.FindByPredicate([&Pair](const TPair<FString, FString>& Candidate)
			{
				return Candidate.Key.Equals(Pair.Key, ESearchCase::IgnoreCase);
			});
			Test.TestNotNull(FString::Printf(TEXT("[%s] setting '%s' present"), *Case.SourcePath, *Pair.Key), Found);
			if (Found)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] setting '%s' == '%s' (actual '%s')"), *Case.SourcePath, *Pair.Key, *Pair.Value, *Found->Value),
					Found->Value.Equals(Pair.Value, ESearchCase::CaseSensitive));
			}
		}
		for (const FString& Needle : Expectation.WarningsContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] a warning contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *WarningText),
				Warnings.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::IgnoreCase); }));
		}

		return true;
	}

	// =============================================================================================
	// IR layer (Tests/Corpus/IR/**) and Compile layer (Tests/Corpus/Compile/**).
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
	//   Compile/  the compiler service's CompileAssets on a `.dss` (CompileDreamShaderTestAssets, the
	//             test compile facade), golden = the normalised `dump-graph` JSON of
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

		// The classes a 1.x source reaches through the legacy front end's rewrites. Appended, so no index of
		// the classes above moves and no golden written before them changes.

		// UE.StaticSwitchParameter -- a PARAMETER class with pins: what a 1.x `StaticSwitchParameter` property expands to
		// at every call (research-legacy.md section 3.6), and the target of rule L20.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("StaticSwitchParameter");
			Expression.ClassName = TEXT("MaterialExpressionStaticSwitchParameter");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionStaticSwitchParameter");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::Numeric, false, nullptr, { TEXT("True") }));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::Numeric, false, nullptr, { TEXT("False") }));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Numeric));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("ParameterName"), ECatalogValueType::Name, TArray<FString>(), { TEXT("Name") }));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("DefaultValue"), ECatalogValueType::Bool, TArray<FString>(), { TEXT("Default") }));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Group"), ECatalogValueType::Name));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("SortPriority"), ECatalogValueType::Int));
			Expression.bIsParameter = true;
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.Custom -- no pins of its own: a call names the inputs it wants (rule L4), and the validator lets a node of
		// this class carry inputs the catalog does not list.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("Custom");
			Expression.ClassName = TEXT("MaterialExpressionCustom");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionCustom");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Numeric));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Code"), ECatalogValueType::String));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Description"), ECatalogValueType::String));
			Expression.Properties.Add(MakeDreamShaderTestProperty(
				TEXT("OutputType"),
				ECatalogValueType::Enum,
				{ TEXT("Float1"), TEXT("Float2"), TEXT("Float3"), TEXT("Float4"), TEXT("MaterialAttributes") }));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.DotProduct -- two REQUIRED value pins and no Const twins: where a missing required pin is an error in a
		// `.dss` (DSH5219) and a warning in a 1.x body (rule L13, DSH5279).
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("DotProduct");
			Expression.ClassName = TEXT("MaterialExpressionDotProduct");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionDotProduct");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::Numeric, true));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::Numeric, true));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Float1));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.TextureSample -- what 1.x `SampleTexture2D(Tex, UV)` becomes, with the engine's five channel views plus RGBA
		// and an enum property for the lenient enumerator rule (L12).
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("TextureSample");
			Expression.ClassName = TEXT("MaterialExpressionTextureSample");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionTextureSample");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Coordinates"), ECatalogValueType::Float2, false, nullptr, { TEXT("UVs"), TEXT("UV") }));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("TextureObject"), ECatalogValueType::Texture, false, nullptr, { TEXT("Tex") }));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("RGB"), ECatalogValueType::Float3));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("R"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("G"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("B"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("A"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("RGBA"), ECatalogValueType::Float4));
			Expression.Properties.Add(MakeDreamShaderTestProperty(
				TEXT("SamplerType"),
				ECatalogValueType::Enum,
				{ TEXT("Color"), TEXT("Grayscale"), TEXT("Alpha"), TEXT("Normal"), TEXT("Masks"), TEXT("LinearColor") }));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.TextureObject -- what a constant texture is (`static const Texture2D T`, 1.x `const Texture2D T = Path(...)`):
		// one Object property for the asset, an enum for how it is sampled, and the texture as its value.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("TextureObject");
			Expression.ClassName = TEXT("MaterialExpressionTextureObject");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionTextureObject");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Texture));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Texture"), ECatalogValueType::Object));
			Expression.Properties.Add(MakeDreamShaderTestProperty(
				TEXT("SamplerType"),
				ECatalogValueType::Enum,
				{ TEXT("Color"), TEXT("Grayscale"), TEXT("Alpha"), TEXT("Normal"), TEXT("Masks"), TEXT("LinearColor") }));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.CameraPositionWS -- one output the engine types exactly (float3), for the 1.x `OutputType` rule: a call that
		// says `OutputType = "float"` is a float to a 1.x body, whatever the node is.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("CameraPositionWS");
			Expression.ClassName = TEXT("MaterialExpressionCameraPositionWS");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionCameraPositionWS");
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::Float3));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.VertexInterpolator -- a custom-output class that hands its value on through an output, so it is a value.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("VertexInterpolator");
			Expression.ClassName = TEXT("MaterialExpressionVertexInterpolator");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionVertexInterpolator");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Input"), ECatalogValueType::Float4, false, nullptr, { TEXT("VS") }));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("PS"), ECatalogValueType::Numeric));
			Expression.bIsCustomOutput = true;
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// Substrate.MoonToonModifier -- a class whose nodes name their pins after a property (the engine fork's modifier
		// node): the default node is a Matcap and shows `Color` and `Intensity`; set to OilFilm it shows `Thickness`,
		// which no catalog read off a default object can list. The pins' own names always resolve.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("Substrate");
			Expression.ShortName = TEXT("MoonToonModifier");
			Expression.ClassName = TEXT("MaterialExpressionMoonToonModifier");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionMoonToonModifier");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("ChannelRGB"), ECatalogValueType::Float3, false, nullptr, { TEXT("Color") }));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("ChannelX"), ECatalogValueType::Float1));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("ChannelW"), ECatalogValueType::Float1, false, nullptr, { TEXT("Intensity") }));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("Payload"), ECatalogValueType::Float4));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT("Id"), ECatalogValueType::Float1));
			Expression.Properties.Add(MakeDreamShaderTestProperty(TEXT("Modifier"), ECatalogValueType::Enum, { TEXT("Matcap"), TEXT("OilFilm") }));
			Expression.bHasInstanceDependentPins = true;
			Catalog.Expressions.Add(MoveTemp(Expression));
		}

		// UE.MakeMaterialAttributes -- what a 1.x `MaterialAttributes X;` without an initializer is, and what a 1.x layer's
		// output starts as when its input has another name: a node of its own, so an attribute read off it is a
		// GetMaterialAttributes against it and not DSH4370.
		{
			FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("MakeMaterialAttributes");
			Expression.ClassName = TEXT("MaterialExpressionMakeMaterialAttributes");
			Expression.ClassPathName = TEXT("/Script/Engine.MaterialExpressionMakeMaterialAttributes");
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("BaseColor"), ECatalogValueType::Float3));
			Expression.Inputs.Add(MakeDreamShaderTestPin(TEXT("Roughness"), ECatalogValueType::Float1));
			Expression.Outputs.Add(MakeDreamShaderTestPin(TEXT(""), ECatalogValueType::MaterialAttributes));
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
		TArray<FString> Infos;

		bool Succeeded() const { return bValidated && Errors.Num() == 0; }
		FString ErrorText() const { return FString::Join(Errors, TEXT(" | ")); }
		FString WarningText() const { return FString::Join(Warnings, TEXT(" | ")); }
		FString InfoText() const { return FString::Join(Infos, TEXT(" | ")); }
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
		/** Keep comments and blank lines (FModule::Trivia) on the MAIN module: what the printer and migrate runners read. */
		bool bKeepTrivia = false;
		/** FIRBuildOptions::StampSourcePath (debt B5); unset keeps the paths as given. */
		TFunction<FString(const FString& File)> StampSourcePath;
		/** `.dsi` only: what the binder checks the overrides against, and the path the instance product carries. */
		const UE::DreamShader::IR::FIRParameterSchema* ParentSchema = nullptr;
		FString ParentObjectPath;
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

		FLangParseOptions MainParseOptions;
		MainParseOptions.bKeepTrivia = Options.bKeepTrivia;
		Out.Parse = ParseDreamShaderLang(FLangSourceText(SourcePath, SourceText), MainParseOptions);
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
			for (const FString& Line : GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Info))
			{
				Out.Infos.Add(Line);
			}
		};

		Gather(Out.Parse.Diagnostics);
		if (!Out.bParsed || !Out.Parse.Module.IsValid())
		{
			return;
		}

		FBindOptions BindOptions;
		BindOptions.Catalog = &Catalog;
		BindOptions.ParentSchema = Options.ParentSchema;
		BindOptions.ParentObjectPath = Options.ParentObjectPath;
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
		// The catalog the bind ran against, said out loud as the pipeline says it (the bound module carries it too).
		BuildOptions.Catalog = &Catalog;
		BuildOptions.StampSourcePath = Options.StampSourcePath;
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
		// sides of the compare.
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
		TArray<FString> InfosContain;

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
		if (Root->TryGetArrayField(TEXT("infosContain"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.InfosContain.Add(Value->AsString());
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
	inline FString BuildDreamShaderIRGoldenJson(const FDreamShaderIRRun& Run, bool bRunPasses, bool bPending = false, const TCHAR* EntryPoint = TEXT("ir"))
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), EntryPoint);
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

		if (Run.Infos.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Info : Run.Infos)
			{
				Values.Add(MakeShared<FJsonValueString>(GetDreamShaderLangDiagnosticCode(Info)));
			}
			Root->SetArrayField(TEXT("infosContain"), Values);
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


	/** Which fixtures an IR-shaped corpus layer runs, and what its goldens call themselves. */
	struct FDreamShaderIRCorpusLayer
	{
		const TCHAR* EntryPoint = TEXT("ir");
		/** Lower case, without the dot. A `dsi` fixture is bound against the sibling its Parent names. */
		TArray<FString> Extensions = { TEXT("dss"), TEXT("dsi") };
	};

	/**
	 * The schema a `.dsi` fixture binds against: the sibling `<leaf of Parent>.dss` (or `.dsi`, for an instance of an
	 * instance) taken through the same runner, then BuildParameterSchemaFromIR on its first material or instance
	 * product. False when the fixture names no parent or no such sibling exists; the fixture then binds without a
	 * schema, which the binder says with DSH7263.
	 */
	inline bool BuildDreamShaderInstanceFixtureSchema(
		const FString& InstancePath,
		const FString& InstanceText,
		UE::DreamShader::IR::FIRParameterSchema& OutSchema,
		FString& OutParentObjectPath,
		const int32 Depth = 0)
	{
		using namespace UE::DreamShader::Lang;
		using namespace UE::DreamShader::IR;

		if (Depth > 4)
		{
			return false;
		}

		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(InstancePath, InstanceText), FLangParseOptions());
		if (!Parsed.Module.IsValid())
		{
			return false;
		}

		FString Leaf;
		for (const FDeclPtr& Decl : Parsed.Module->Declarations)
		{
			const FPragmaDecl* Pragma = Decl.IsValid() ? Decl->As<FPragmaDecl>() : nullptr;
			const FPragmaArgument* Parent = (Pragma && Pragma->PragmaKind == EPragmaKind::Instance) ? Pragma->Find(TEXT("Parent")) : nullptr;
			if (Parent)
			{
				Leaf = Parent->Value;
				break;
			}
		}

		// `/Game/X/M_Parent.M_Parent`, `/Game/X/M_Parent` and `M_Parent` all name the sibling `M_Parent`.
		int32 Separator = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('/'), Separator))
		{
			Leaf.RightChopInline(Separator + 1);
		}
		if (Leaf.FindChar(TEXT('.'), Separator))
		{
			Leaf.LeftInline(Separator);
		}
		if (Leaf.IsEmpty())
		{
			return false;
		}

		const FString Directory = FPaths::GetPath(InstancePath);
		for (const TCHAR* Extension : { TEXT("dss"), TEXT("dsi") })
		{
			const FString ParentPath = FPaths::Combine(Directory, Leaf + TEXT(".") + Extension);
			FString ParentText;
			if (ParentPath.Equals(InstancePath, ESearchCase::IgnoreCase) || !FFileHelper::LoadFileToString(ParentText, *ParentPath))
			{
				continue;
			}

			// Declared before the run that points into it.
			FIRParameterSchema GrandparentSchema;
			FDreamShaderIRRunOptions ParentOptions;
			ParentOptions.IncludeDirectory = Directory;
			if (FCString::Stricmp(Extension, TEXT("dsi")) == 0
				&& BuildDreamShaderInstanceFixtureSchema(ParentPath, ParentText, GrandparentSchema, ParentOptions.ParentObjectPath, Depth + 1))
			{
				ParentOptions.ParentSchema = &GrandparentSchema;
			}

			FDreamShaderIRRun ParentRun;
			RunDreamShaderIRPipeline(ParentPath, ParentText, ParentOptions, ParentRun);
			if (!ParentRun.Module.IsValid() || !ParentRun.Succeeded())
			{
				return false;
			}

			const int32 ProductIndex = ParentRun.Module->Products.IndexOfByPredicate([](const FIRProduct& Product)
			{
				return Product.Kind == EIRProductKind::Material || Product.Kind == EIRProductKind::MaterialInstance;
			});
			if (ProductIndex == INDEX_NONE)
			{
				return false;
			}

			// Made up and stable: a corpus directory is not a source root, so nothing resolves a real one.
			OutParentObjectPath = FString::Printf(TEXT("/Game/Corpus/%s.%s"), *Leaf, *Leaf);
			return BuildParameterSchemaFromIR(*ParentRun.Module, ProductIndex, ParentRun.Bind.Bound.Get(), OutSchema);
		}
		return false;
	}

	/**
	 * Run one fixture of an IR-shaped layer (Tests/Corpus/IR, Tests/Corpus/Legacy/IR) and assert it against its golden.
	 * In -DreamShaderUpdateGolden mode it rewrites the golden instead of asserting.
	 * Returns false only on a hard I/O failure; mismatches are recorded on Test.
	 */
	inline bool RunDreamShaderIRCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case, const FDreamShaderIRCorpusLayer& Layer)
	{
		using namespace UE::DreamShader::Lang;

		// `.dsh` headers are included BY a fixture, never run as one: a header produces nothing to assert on.
		if (!Layer.Extensions.Contains(Case.Extension.ToLower()))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a compilation unit of the '%s' layer; skipped."), *Case.SourcePath, Layer.EntryPoint));
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

		if (!Expectation.EntryPoint.IsEmpty() && !Expectation.EntryPoint.Equals(Layer.EntryPoint, ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] golden declares entryPoint '%s'; this corpus only runs '%s' goldens."),
				*Case.ExpectedPath, *Expectation.EntryPoint, Layer.EntryPoint));
			return false;
		}

		FDreamShaderIRRunOptions Options;
		Options.bRunPasses = Expectation.bRunPasses;
		Options.IncludeDirectory = FPaths::GetPath(Case.SourcePath);

		// A `.dsi` binds against its parent's parameters: the sibling its Parent names, lowered first.
		UE::DreamShader::IR::FIRParameterSchema ParentSchema;
		if (Case.Extension.Equals(TEXT("dsi"), ESearchCase::IgnoreCase)
			&& BuildDreamShaderInstanceFixtureSchema(Case.SourcePath, SourceString, ParentSchema, Options.ParentObjectPath))
		{
			Options.ParentSchema = &ParentSchema;
		}

		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(Case.SourcePath, SourceString, Options, Run);

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderIRGoldenJson(Run, Expectation.bRunPasses, Expectation.bIRPending, Layer.EntryPoint);
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
		for (const FString& Needle : Expectation.InfosContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] an info contains '%s' (actual: %s)"), *Case.SourcePath, *Needle, *Run.InfoText()),
				Run.Infos.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
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

	/** The IR layer proper: Tests/Corpus/IR, `.dss` and `.dsi`. */
	inline bool RunDreamShaderIRCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		return RunDreamShaderIRCorpusCase(Test, Case, FDreamShaderIRCorpusLayer());
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
		/**
		 * RelativeName is the corpus-relative, extension-free name; it becomes the scratch subtree.
		 *
		 * Extension is the main source's: `dss` for the Compile corpus, `dsm` / `dsf` for a legacy source. A legacy
		 * destination follows the block's `Name=` and never the source folder (research-legacy.md L10), so a legacy
		 * fixture writes `Name="<MakeLegacyAssetName(...)>"` to land under this fixture's package path.
		 */
		explicit FDreamShaderCompile2Fixture(
			const FString& InRelativeName,
			const TCHAR* InScratchArea = TEXT("Compile2"),
			const TCHAR* InExtension = TEXT("dss"))
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
				FPaths::GetCleanFilename(Sanitised) + TEXT(".") + InExtension));
		}

		~FDreamShaderCompile2Fixture()
		{
			// Everything under the fixture's package path is the fixture's, whether or not a test tracked it: a product
			// the compile made under a name the test never asked for would otherwise outlive the run (a Graph
			// material or function always saves, so nothing a fixture compiles is memory-only unless it is ThinCustom).
			{
				FAssetRegistryModule& AssetRegistryModule =
					FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				TArray<FString> ScanPaths;
				ScanPaths.Add(PackagePath);
				AssetRegistryModule.Get().ScanPathsSynchronous(ScanPaths, /*bForceRescan*/ true);

				FARFilter Filter;
				Filter.PackagePaths.Add(FName(*PackagePath));
				Filter.bRecursivePaths = true;
				TArray<FAssetData> Remaining;
				AssetRegistryModule.Get().GetAssets(Filter, Remaining);
				for (const FAssetData& AssetData : Remaining)
				{
					ProducedObjectPaths.AddUnique(AssetData.GetObjectPathString());
				}
			}

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

		/**
		 * `DreamShaderTests/<Area>/<RelativeName>/<AssetName>`: the `Name=` that lands a legacy block's asset under this
		 * fixture's package path (with no `Root=`, the project DShader root the source sits in answers `/Game`).
		 */
		FString MakeLegacyAssetName(const FString& AssetName) const
		{
			// PackagePath always starts with the six characters "/Game/": the constructor wrote it.
			return PackagePath.RightChop(6) + TEXT("/") + AssetName;
		}

		/** `<PackagePath>/<AssetName>.<AssetName>`. */
		FString MakeObjectPath(const FString& AssetName) const
		{
			return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		}

		/**
		 * Writes another file -- a header a source imports, a second source, a decompiled round trip -- into the main
		 * source's directory. The destructor deletes that directory as a tree, so nothing written here outlives the fixture.
		 */
		bool WriteSiblingSource(FAutomationTestBase& Test, const FString& FileName, const FString& SourceText, FString& OutFilePath) const
		{
			OutFilePath = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(FPaths::GetPath(SourceFilePath), FileName));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutFilePath), true);
			if (!FFileHelper::SaveStringToFile(SourceText, *OutFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(FString::Printf(TEXT("Failed to write the fixture file '%s' for '%s'."), *FileName, *RelativeName));
				return false;
			}
			return true;
		}

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

	/**
	 * Point a 1.x source's product block at another asset path: rewrite the `Name="..."` of its LAST block header and,
	 * when the header has one, set `Root="Game"`. False when there is no Shader / ShaderFunction / ShaderLayer /
	 * ShaderLayerBlend header with a quoted `Name`.
	 *
	 * A legacy destination follows `Name=` and never the source folder (research-legacy.md L10), so a test that
	 * compiles a real 1.x source from a scratch directory has to move its Name too, or it writes over the real asset.
	 * Only the last header is rewritten, which is the product block: a `.dsf` opens with `VirtualFunction(Name = "...")`
	 * prototypes whose names are call targets, not asset paths. Moved here from DreamShaderCompiler2Tests.cpp
	 * (RetargetLegacySource) when the parity tests stopped compiling the 1.x twin.
	 */
	inline bool RetargetDreamShaderLegacyBlockName(const FString& Source, const FString& AssetPath, FString& OutSource)
	{
		const TCHAR* BlockKeywords[] = { TEXT("ShaderLayerBlend("), TEXT("ShaderLayer("), TEXT("ShaderFunction("), TEXT("Shader(") };

		int32 BlockStart = INDEX_NONE;
		for (const TCHAR* Keyword : BlockKeywords)
		{
			const int32 Index = Source.Find(Keyword, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Index != INDEX_NONE && Index > BlockStart)
			{
				BlockStart = Index;
			}
		}
		if (BlockStart == INDEX_NONE)
		{
			return false;
		}

		const int32 HeaderEnd = Source.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BlockStart);
		if (HeaderEnd == INDEX_NONE)
		{
			return false;
		}

		FString Header = Source.Mid(BlockStart, HeaderEnd - BlockStart + 1);

		auto ReplaceKey = [&Header](const TCHAR* Key, const FString& Value) -> bool
		{
			const FString Needle = FString::Printf(TEXT("%s=\""), Key);
			const int32 Start = Header.Find(Needle, ESearchCase::CaseSensitive);
			if (Start == INDEX_NONE)
			{
				return false;
			}
			const int32 ValueStart = Start + Needle.Len();
			const int32 ValueEnd = Header.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				return false;
			}
			Header = Header.Left(ValueStart) + Value + Header.Mid(ValueEnd);
			return true;
		};

		if (!ReplaceKey(TEXT("Name"), AssetPath))
		{
			return false;
		}
		// Root is optional in 1.x; when it is there it has to come back to Game so the retargeted path means what it says.
		ReplaceKey(TEXT("Root"), TEXT("Game"));

		OutSource = Source.Left(BlockStart) + Header + Source.Mid(HeaderEnd + 1);
		return true;
	}

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
		/** `"siblings": ["Shared.dsh"]`: file names beside the fixture that are copied with it. Absent: every `.dsh` there. */
		TArray<FString> Siblings;
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

		if (Root->TryGetArrayField(TEXT("siblings"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				Out.Siblings.Add(Value->AsString());
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
		bool bPending = false,
		const TArray<FString>* Siblings = nullptr)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("compile"));
		Root->SetStringField(TEXT("outcome"), bCompiled ? TEXT("ok") : TEXT("error"));

		// What the author wrote, carried back: an update run must not forget which files the fixture needs.
		if (Siblings && Siblings->Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Sibling : *Siblings)
			{
				Values.Add(MakeShared<FJsonValueString>(Sibling));
			}
			Root->SetArrayField(TEXT("siblings"), Values);
		}

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

	/** Which fixtures a compile-shaped corpus layer runs, where it puts them, and under which default backend. */
	struct FDreamShaderCompileCorpusLayer
	{
		/** Directory under Tests/Corpus. */
		FString LayerDir = TEXT("Compile");
		/** Subtree under `DShader/DreamShaderTests` and `/Game/DreamShaderTests` the fixtures are copied into. */
		FString ScratchArea = TEXT("Compile2");
		/** Lower case, without the dot. */
		TArray<FString> Extensions = { TEXT("dss") };
		/** The backend a fixture gets when it does not name one: what the layer's goldens were captured under. */
		EDreamShaderDefaultBackend PinnedBackend = EDreamShaderDefaultBackend::Graph;
	};

	/**
	 * The files a fixture needs beside it: the golden's `siblings` when it lists them, else every `.dsh` of the fixture's
	 * directory -- and, for a `.dsi`, the `.dss` / `.dsi` its Parent names, which the compile builds first by itself.
	 */
	inline void CollectDreamShaderFixtureSiblings(const FCorpusCase& Case, const FString& SourceText, const TArray<FString>& Listed, TArray<FString>& OutFiles)
	{
		const FString Directory = FPaths::GetPath(Case.SourcePath);
		if (Listed.Num() > 0)
		{
			for (const FString& Name : Listed)
			{
				OutFiles.AddUnique(FPaths::Combine(Directory, Name));
			}
			return;
		}

		TArray<FString> Headers;
		IFileManager::Get().FindFiles(Headers, *FPaths::Combine(Directory, TEXT("*.dsh")), true, false);
		for (const FString& Header : Headers)
		{
			OutFiles.AddUnique(FPaths::Combine(Directory, Header));
		}

		if (!Case.Extension.Equals(TEXT("dsi"), ESearchCase::IgnoreCase))
		{
			return;
		}

		// The parent chain, by the same reading of Parent the IR layer uses.
		FString ChildPath = Case.SourcePath;
		FString ChildText = SourceText;
		for (int32 Depth = 0; Depth < 4; ++Depth)
		{
			using namespace UE::DreamShader::Lang;
			const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(ChildPath, ChildText), FLangParseOptions());
			FString Leaf;
			if (Parsed.Module.IsValid())
			{
				for (const FDeclPtr& Decl : Parsed.Module->Declarations)
				{
					const FPragmaDecl* Pragma = Decl.IsValid() ? Decl->As<FPragmaDecl>() : nullptr;
					const FPragmaArgument* Parent = (Pragma && Pragma->PragmaKind == EPragmaKind::Instance) ? Pragma->Find(TEXT("Parent")) : nullptr;
					if (Parent)
					{
						Leaf = Parent->Value;
						break;
					}
				}
			}
			int32 Separator = INDEX_NONE;
			if (Leaf.FindLastChar(TEXT('/'), Separator))
			{
				Leaf.RightChopInline(Separator + 1);
			}
			if (Leaf.FindChar(TEXT('.'), Separator))
			{
				Leaf.LeftInline(Separator);
			}

			FString ParentPath;
			for (const TCHAR* Extension : { TEXT("dss"), TEXT("dsi") })
			{
				const FString Candidate = FPaths::Combine(Directory, Leaf + TEXT(".") + Extension);
				if (!Leaf.IsEmpty() && IFileManager::Get().FileExists(*Candidate))
				{
					ParentPath = Candidate;
					break;
				}
			}
			if (ParentPath.IsEmpty() || OutFiles.Contains(ParentPath))
			{
				return;
			}
			OutFiles.Add(ParentPath);
			if (!ParentPath.EndsWith(TEXT(".dsi"), ESearchCase::IgnoreCase) || !FFileHelper::LoadFileToString(ChildText, *ParentPath))
			{
				return;
			}
			ChildPath = ParentPath;
		}
	}

	/**
	 * Run one fixture of a compile-shaped layer end to end and assert it against its golden.
	 *
	 * The fixture is COPIED under the project's DShader root before compiling: the 2.0 pipeline has
	 * no transient request (the compiler pipeline header says so), the asset destination follows
	 * the source path, and a corpus directory is not a source root. The copy and every asset it
	 * produced are deleted by the fixture's destructor whatever happens in between.
	 *
	 * A legacy source (`.dsm` / `.dsf`) is the exception to "the destination follows the source path": its block's
	 * `Name=` decides (research-legacy.md L10), so the copy's last product block is renamed to land under the fixture's
	 * package path, keeping the fixture's own stem as the asset name.
	 */
	inline bool RunDreamShaderCompileCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case, const FDreamShaderCompileCorpusLayer& Layer)
	{
		if (!Layer.Extensions.Contains(Case.Extension.ToLower()))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a compilation unit of the '%s' layer; skipped."), *Case.SourcePath, *Layer.LayerDir));
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
		// empty name would put all of them in one directory and write every source to the same file.
		FString RelativeName = Case.RelativeName;
		if (RelativeName.IsEmpty())
		{
			RelativeName = Case.SourcePath;
			const FString LayerRoot = FPaths::Combine(GetDreamShaderCorpusRoot(), Layer.LayerDir) / TEXT("");
			FPaths::MakePathRelativeTo(RelativeName, *LayerRoot);
			RelativeName = FPaths::GetBaseFilename(RelativeName, /*bRemovePath*/ false);
		}
		// `X.bad` is a file name, not a package name.
		RelativeName.ReplaceInline(TEXT(".bad"), TEXT("_bad"), ESearchCase::IgnoreCase);

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
				TEXT("[%s] golden declares entryPoint '%s'; the compile-shaped corpora only run 'compile' goldens."),
				*Case.ExpectedPath, *Expectation.EntryPoint));
			return false;
		}

		// The goldens describe the layer's backend unless the fixture itself names one.
		FScopedDreamShaderBackendPin BackendPin(Layer.PinnedBackend);

		FDreamShaderCompile2Fixture Fixture(RelativeName, *Layer.ScratchArea, *Case.Extension.ToLower());
		// Suppression, not a requirement: the new-asset probe fires for some engine builds and not
		// others. Never quote the package path in an assertion message below -- the harness would
		// swallow the failure along with the probe.
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		const bool bLegacySource = Case.Extension.Equals(TEXT("dsm"), ESearchCase::IgnoreCase) || Case.Extension.Equals(TEXT("dsf"), ESearchCase::IgnoreCase);
		if (bLegacySource)
		{
			FString AssetLeaf = FPaths::GetBaseFilename(Case.SourcePath);
			AssetLeaf.ReplaceInline(TEXT(".bad"), TEXT(""), ESearchCase::IgnoreCase);
			FString Retargeted;
			if (RetargetDreamShaderLegacyBlockName(SourceString, Fixture.MakeLegacyAssetName(AssetLeaf), Retargeted))
			{
				SourceString = MoveTemp(Retargeted);
			}
			// No product block with a quoted Name: a fixture about exactly that. It fails before it writes anything.
		}

		if (!Fixture.WriteSource(Test, SourceString))
		{
			return false;
		}

		TArray<FString> Siblings;
		CollectDreamShaderFixtureSiblings(Case, SourceString, Expectation.Siblings, Siblings);
		for (const FString& Sibling : Siblings)
		{
			FString SiblingText;
			FString WrittenPath;
			if (!FFileHelper::LoadFileToString(SiblingText, *Sibling))
			{
				Test.AddError(FString::Printf(TEXT("[%s] cannot read the sibling '%s'."), *RelativeName, *Sibling));
				return false;
			}
			if (!Fixture.WriteSiblingSource(Test, FPaths::GetCleanFilename(Sibling), SiblingText, WrittenPath))
			{
				return false;
			}
		}

		UE::DreamShader::FDreamShaderError Error;
		const bool bCompiled = ::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(
			Fixture.GetSourceFilePath(), Error, /*bForce*/ true, /*bEphemeralThinCustom*/ false);

		TArray<FDreamShaderCompiledAsset> Assets;
		Fixture.CollectProducedAssets(Assets);

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderCompileGoldenJson(bCompiled, Error, Assets, Expectation.bGraphPending, &Expectation.Siblings);
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
			// As an Info line: the text quotes the fixture's package path, which is a registered expected error.
			Test.AddInfo(FString::Printf(TEXT("[%s] the compiler said: %s"), *RelativeName, *ErrorText));
			Test.AddError(FString::Printf(
				TEXT("[%s] the compile should SUCCEED but failed; the info line above has what the compiler said."), *RelativeName));
			return false;
		}

		for (const FString& Needle : Expectation.ErrorContains)
		{
			const bool bContains = ErrorText.Contains(Needle, ESearchCase::CaseSensitive);
			if (!bContains)
			{
				Test.AddInfo(FString::Printf(TEXT("[%s] the compiler said: %s"), *RelativeName, *ErrorText));
			}
			Test.TestTrue(FString::Printf(TEXT("[%s] the error contains '%s' (the info line above has the actual text)"), *RelativeName, *Needle), bContains);
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

	/** The Compile layer proper: Tests/Corpus/Compile, `.dss`, Graph pinned. */
	inline bool RunDreamShaderCompileCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		return RunDreamShaderCompileCorpusCase(Test, Case, FDreamShaderCompileCorpusLayer());
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
