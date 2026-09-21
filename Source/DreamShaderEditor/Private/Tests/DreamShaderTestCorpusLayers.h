// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The corpus layers of the legacy front end, the decompiler and the migrator, beside the ones in DreamShaderTestCommon.h:
//
//   Tests/Corpus/Legacy/Parse    `"entryPoint": "legacy"`     1.x text -> legacy front end -> printed 2.0 text
//   Tests/Corpus/Decompile       `"entryPoint": "decompile"`  .dss -> IR -> AST -> printed text -> IR again
//   Tests/Corpus/Migrate         `"entryPoint": "migrate"`    1.x text -> migrated 2.0 text -> the same IR
//   Tests/Corpus/Roundtrip       `"entryPoint": "roundtrip"`  .dss -> assets -> decompiled text -> the same assets
//
// The first three are Core only: no asset, no reflection, the hand-built test catalog. They share one golden shape,
// because all three are "a text comes out, and it has to mean what went in":
//
//   {
//     "entryPoint": "legacy" | "decompile" | "migrate",
//     "outcome": "ok" | "error",
//     "errorContains": ["DSH2241"], "warningsContain": [...], "infosContain": [...],
//     "textPending": true,            // skip the `text` compare; everything else still runs
//     "text": "<the printed text, verbatim>",
//     "roundtrip": false,             // opt out of the layer's meaning check (say why in the fixture's first comment)
//     "comments": false,              // migrate: opt out of the comment check
//     "compareDestinations": false,   // decompile: product names may differ (a fixture about `@name`)
//     "legacy": { "blocks": 1, ... }, // legacy: counts over FLegacyMigrationInfo
//     "productNames": { "M_X": "" }   // migrate: what the host would answer per product function ("" = no `@name`)
//   }
//
// `textPending` is `irPending`'s twin: an update run (-DreamShaderUpdateGolden) always writes `text` and carries the
// flag back, and dropping the flag is a person's job, after reading the text.

#pragma once

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompile/IRToAst.h"
#include "Decompiler/DreamShaderDecompileService.h"
// ImportDreamShaderGraphToIR: the Roundtrip layer reads both builds back and compares them as graphs, not as dump text.
#include "Decompiler/DreamShaderGraphImport.h"
#include "DreamShaderMaterialInstance.h"
#include "IR/IRCompare.h"
#include "Migrate/LangMigrate.h"
#include "Tools/DreamShaderDecompileTools.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	/**
	 * GetTests for one corpus directory: a test per fixture, named by its corpus-relative path with dots, commanded by
	 * its source path. NamePrefix keeps the names of a test that sweeps several directories apart.
	 */
	inline void AddDreamShaderCorpusTests(const TCHAR* SubDir, const TCHAR* NamePrefix, TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands)
	{
		TArray<FCorpusCase> Cases;
		LoadDreamShaderCorpusCases(SubDir, Cases);
		for (const FCorpusCase& Case : Cases)
		{
			OutBeautifiedNames.Add(NamePrefix + Case.RelativeName.Replace(TEXT("/"), TEXT(".")).Replace(TEXT("\\"), TEXT(".")));
			OutTestCommands.Add(Case.SourcePath);
		}
	}

	/** `IR/Branches/B_StaticIf` for a source path under the corpus root: the key of a skip list. */
	inline FString MakeDreamShaderCorpusRelativeName(const FString& SourcePath)
	{
		FString Relative = FPaths::ConvertRelativePathToFull(SourcePath);
		const FString Root = GetDreamShaderCorpusRoot() / TEXT("");
		FPaths::MakePathRelativeTo(Relative, *Root);
		Relative = FPaths::GetBaseFilename(Relative, /*bRemovePath*/ false);
		Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Relative;
	}

	// ---------------------------------------------------------------------------------------------
	// The shared golden of the three text layers
	// ---------------------------------------------------------------------------------------------

	struct FTextCorpusExpectation
	{
		FString EntryPoint;
		bool bExpectError = false;
		TArray<FString> ErrorContains;
		TArray<FString> WarningsContain;
		TArray<FString> InfosContain;

		bool bTextPending = false;
		bool bCheckText = false;          FString Text;
		bool bCheckRoundtrip = true;
		bool bCheckComments = true;
		bool bCompareDestinations = true;

		/** `"legacy": { ... }`: only the keys the golden names are asserted. */
		TMap<FString, int32> LegacyCounts;
		/** `"productNames": { "<function>": "<name>" }`, in the golden's order. */
		TArray<TPair<FString, FString>> ProductNames;
	};

	/** What one run of a text layer produced, in the shape the golden is written from. */
	struct FTextCorpusOutcome
	{
		bool bSucceeded = false;
		FString Text;
		TArray<FString> Errors;
		TArray<FString> Warnings;
		TArray<FString> Infos;
		TMap<FString, int32> LegacyCounts;

		void Gather(const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
		{
			using namespace UE::DreamShader::Lang;
			Errors.Append(GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Error));
			Warnings.Append(GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Warning));
			Infos.Append(GatherDreamShaderLangDiagnostics(Sink, ELangSeverity::Info));
		}

		void Gather(const FDreamShaderIRRun& Run)
		{
			Errors.Append(Run.Errors);
			Warnings.Append(Run.Warnings);
			Infos.Append(Run.Infos);
		}
	};

	inline bool ParseDreamShaderTextExpectation(const FString& JsonText, FTextCorpusExpectation& Out, FString& OutError)
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

		const auto ReadStrings = [&Root](const TCHAR* Key, TArray<FString>& OutValues)
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Root->TryGetArrayField(Key, Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					OutValues.Add(Value->AsString());
				}
			}
		};
		ReadStrings(TEXT("errorContains"), Out.ErrorContains);
		ReadStrings(TEXT("warningsContain"), Out.WarningsContain);
		ReadStrings(TEXT("infosContain"), Out.InfosContain);

		bool BoolValue = false;
		if (Root->TryGetBoolField(TEXT("textPending"), BoolValue)) { Out.bTextPending = BoolValue; }
		if (Root->TryGetBoolField(TEXT("roundtrip"), BoolValue)) { Out.bCheckRoundtrip = BoolValue; }
		if (Root->TryGetBoolField(TEXT("comments"), BoolValue)) { Out.bCheckComments = BoolValue; }
		if (Root->TryGetBoolField(TEXT("compareDestinations"), BoolValue)) { Out.bCompareDestinations = BoolValue; }

		FString StringValue;
		if (Root->TryGetStringField(TEXT("text"), StringValue)) { Out.bCheckText = true; Out.Text = StringValue; }

		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Root->TryGetObjectField(TEXT("legacy"), Object))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
			{
				Out.LegacyCounts.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
			}
		}
		if (Root->TryGetObjectField(TEXT("productNames"), Object))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
			{
				Out.ProductNames.Emplace(Pair.Key, Pair.Value->AsString());
			}
		}
		return true;
	}

	/** Everything the author wrote that a run cannot know is carried back from Expectation; everything a run knows is written from Outcome. */
	inline FString BuildDreamShaderTextGoldenJson(const TCHAR* EntryPoint, const FTextCorpusOutcome& Outcome, const FTextCorpusExpectation& Expectation)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), EntryPoint);
		Root->SetStringField(TEXT("outcome"), Outcome.bSucceeded ? TEXT("ok") : TEXT("error"));

		// Codes only, never prose: the code is the contract, the English is not.
		const auto WriteCodes = [&Root](const TCHAR* Key, const TArray<FString>& Lines)
		{
			if (Lines.Num() == 0)
			{
				return;
			}
			TArray<FString> Codes;
			for (const FString& Line : Lines)
			{
				Codes.AddUnique(GetDreamShaderLangDiagnosticCode(Line));
			}
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Code : Codes)
			{
				Values.Add(MakeShared<FJsonValueString>(Code));
			}
			Root->SetArrayField(Key, Values);
		};
		WriteCodes(TEXT("errorContains"), Outcome.Errors);
		WriteCodes(TEXT("warningsContain"), Outcome.Warnings);
		WriteCodes(TEXT("infosContain"), Outcome.Infos);

		if (Expectation.bTextPending) { Root->SetBoolField(TEXT("textPending"), true); }
		if (!Expectation.bCheckRoundtrip) { Root->SetBoolField(TEXT("roundtrip"), false); }
		if (!Expectation.bCheckComments) { Root->SetBoolField(TEXT("comments"), false); }
		if (!Expectation.bCompareDestinations) { Root->SetBoolField(TEXT("compareDestinations"), false); }

		if (Expectation.ProductNames.Num() > 0)
		{
			const TSharedRef<FJsonObject> Names = MakeShared<FJsonObject>();
			for (const TPair<FString, FString>& Pair : Expectation.ProductNames)
			{
				Names->SetStringField(Pair.Key, Pair.Value);
			}
			Root->SetObjectField(TEXT("productNames"), Names);
		}

		if (Outcome.LegacyCounts.Num() > 0)
		{
			const TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
			for (const TPair<FString, int32>& Pair : Outcome.LegacyCounts)
			{
				Counts->SetNumberField(Pair.Key, Pair.Value);
			}
			Root->SetObjectField(TEXT("legacy"), Counts);
		}

		if (!Outcome.Text.IsEmpty())
		{
			Root->SetStringField(TEXT("text"), Outcome.Text);
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/** Loads the golden of a text layer; false on a hard failure, which is already on Test. */
	inline bool LoadDreamShaderTextExpectation(FAutomationTestBase& Test, const FCorpusCase& Case, const TCHAR* EntryPoint, FTextCorpusExpectation& Out)
	{
		Out = FTextCorpusExpectation();
		Out.bExpectError = Case.bBadByName;
		if (!Case.bHasExpectationFile)
		{
			return true;
		}

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read golden '%s'."), *Case.ExpectedPath));
			return false;
		}
		FString JsonError;
		if (!ParseDreamShaderTextExpectation(JsonText, Out, JsonError))
		{
			Test.AddError(FString::Printf(TEXT("Malformed golden '%s': %s"), *Case.ExpectedPath, *JsonError));
			return false;
		}
		if (!Out.EntryPoint.IsEmpty() && !Out.EntryPoint.Equals(EntryPoint, ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] golden declares entryPoint '%s'; this corpus only runs '%s' goldens."), *Case.ExpectedPath, *Out.EntryPoint, EntryPoint));
			return false;
		}
		return true;
	}

	/**
	 * The half every text layer asserts the same way: update mode, outcome, codes, the text golden.
	 * True when the caller should go on to its own checks (the run succeeded and was expected to).
	 */
	inline bool AssertDreamShaderTextOutcome(
		FAutomationTestBase& Test,
		const FCorpusCase& Case,
		const TCHAR* EntryPoint,
		const FTextCorpusOutcome& Outcome,
		const FTextCorpusExpectation& Expectation,
		bool& bOutUpdated)
	{
		bOutUpdated = false;
		if (ShouldUpdateDreamShaderGolden())
		{
			bOutUpdated = true;
			const FString Json = BuildDreamShaderTextGoldenJson(EntryPoint, Outcome, Expectation);
			if (FFileHelper::SaveStringToFile(Json, *Case.ExpectedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddInfo(FString::Printf(TEXT("Updated golden '%s'."), *Case.ExpectedPath));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to write golden '%s'."), *Case.ExpectedPath));
			}
			return false;
		}

		const FString ErrorText = FString::Join(Outcome.Errors, TEXT(" | "));
		if (Expectation.bExpectError)
		{
			Test.TestTrue(FString::Printf(TEXT("[%s] the '%s' run should REFUSE this source"), *Case.SourcePath, EntryPoint), Outcome.Errors.Num() > 0 || !Outcome.bSucceeded);
		}
		else if (!Outcome.bSucceeded || Outcome.Errors.Num() > 0)
		{
			Test.AddError(FString::Printf(TEXT("[%s] the '%s' run should SUCCEED but reported: %s"), *Case.SourcePath, EntryPoint, *ErrorText));
			return false;
		}

		const auto AssertContains = [&Test, &Case](const TCHAR* What, const TArray<FString>& Needles, const TArray<FString>& Lines)
		{
			for (const FString& Needle : Needles)
			{
				Test.TestTrue(
					FString::Printf(TEXT("[%s] %s contains '%s' (actual: %s)"), *Case.SourcePath, What, *Needle, *FString::Join(Lines, TEXT(" | "))),
					Lines.ContainsByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle, ESearchCase::CaseSensitive); }));
			}
		};
		AssertContains(TEXT("an error"), Expectation.ErrorContains, Outcome.Errors);
		AssertContains(TEXT("a warning"), Expectation.WarningsContain, Outcome.Warnings);
		AssertContains(TEXT("an info"), Expectation.InfosContain, Outcome.Infos);

		if (Expectation.bExpectError)
		{
			return false;
		}

		for (const TPair<FString, int32>& Expected : Expectation.LegacyCounts)
		{
			const int32* Actual = Outcome.LegacyCounts.Find(Expected.Key);
			Test.TestEqual(FString::Printf(TEXT("[%s] legacy.%s"), *Case.SourcePath, *Expected.Key), Actual ? *Actual : -1, Expected.Value);
		}

		if (Expectation.bCheckText && !Expectation.bTextPending)
		{
			const bool bEqual = Outcome.Text.Equals(Expectation.Text, ESearchCase::CaseSensitive);
			Test.TestTrue(FString::Printf(TEXT("[%s] the printed text matches its golden"), *Case.SourcePath), bEqual);
			if (!bEqual)
			{
				Test.AddInfo(FString::Printf(TEXT("[%s] %s"), *Case.SourcePath, *DescribeDreamShaderTextDifference(Outcome.Text, Expectation.Text)));
			}
		}
		else if (Expectation.bTextPending)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[%s] textPending: the text compare is skipped. Fill the golden with -DreamShaderUpdateGolden, review it, then drop the flag."),
				*Case.SourcePath));
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Legacy/Parse: the legacy front end, and the text its tree prints as
	// ---------------------------------------------------------------------------------------------

	inline TMap<FString, int32> SummariseDreamShaderLegacyInfo(const UE::DreamShader::Lang::FModule& Module, const UE::DreamShader::Lang::FLegacyMigrationInfo* Legacy)
	{
		using namespace UE::DreamShader::Lang;

		TMap<FString, int32> Counts;
		Counts.Add(TEXT("hasLegacyInfo"), Legacy ? 1 : 0);
		Counts.Add(TEXT("blocks"), Legacy ? Legacy->Blocks.Num() : 0);
		Counts.Add(TEXT("sections"), Legacy ? Legacy->Sections.Num() : 0);
		Counts.Add(TEXT("parameterDeclarations"), Legacy ? Legacy->ParameterDeclarations.Num() : 0);
		Counts.Add(TEXT("assetReferences"), Legacy ? Legacy->AssetReferences.Num() : 0);
		Counts.Add(TEXT("outputSelections"), Legacy ? Legacy->OutputSelections.Num() : 0);
		Counts.Add(TEXT("synthesizedInitializers"), Legacy ? Legacy->SynthesizedInitializers.Num() : 0);
		Counts.Add(TEXT("synthesizedDirectives"), Legacy ? Legacy->SynthesizedDirectives.Num() : 0);
		Counts.Add(TEXT("renames"), Legacy ? Legacy->BodyRenames.Num() : 0);
		Counts.Add(TEXT("namespaceFlatten"), Legacy ? Legacy->NamespaceFlatten.Num() : 0);

		int32 HoistedCalls = 0;
		int32 LegacyDeclarations = 0;
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			if (!Decl.IsValid())
			{
				continue;
			}
			LegacyDeclarations += Decl->bLegacy ? 1 : 0;
			if (const FFunctionDecl* Function = Decl->As<FFunctionDecl>())
			{
				HoistedCalls += Function->HoistedCalls.Num();
			}
		}
		Counts.Add(TEXT("hoistedCalls"), HoistedCalls);
		Counts.Add(TEXT("legacyDeclarations"), LegacyDeclarations);

		// Output selections that share a call: the number of distinct groups.
		TSet<int32> Groups;
		for (int32 Index = 0; Legacy && Index < Legacy->OutputSelections.Num(); ++Index)
		{
			Groups.Add(Legacy->OutputSelections[Index].Group);
		}
		Counts.Add(TEXT("outputSelectionGroups"), Groups.Num());
		return Counts;
	}

	/** `X.dsm` -> `X.dss`; a header keeps its extension, because a migrated or printed header is still a header. */
	inline FString MakeDreamShaderPrintedPath(const FString& SourcePath, const TCHAR* Infix)
	{
		const bool bHeader = FPaths::GetExtension(SourcePath).Equals(TEXT("dsh"), ESearchCase::IgnoreCase);
		return FPaths::Combine(
			FPaths::GetPath(SourcePath),
			FPaths::GetBaseFilename(SourcePath) + Infix + (bHeader ? TEXT(".dsh") : TEXT(".dss")));
	}

	inline bool RunDreamShaderLegacyParseCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		using namespace UE::DreamShader::Lang;
		const TCHAR* EntryPoint = TEXT("legacy");

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FTextCorpusExpectation Expectation;
		if (!LoadDreamShaderTextExpectation(Test, Case, EntryPoint, Expectation))
		{
			return false;
		}

		FLangParseOptions ParseOptions;
		ParseOptions.bKeepTrivia = true;
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(Case.SourcePath, SourceString), ParseOptions);

		FTextCorpusOutcome Outcome;
		Outcome.Gather(Parsed.Diagnostics);
		Outcome.bSucceeded = Parsed.Succeeded();
		if (Parsed.Module.IsValid())
		{
			Outcome.LegacyCounts = SummariseDreamShaderLegacyInfo(*Parsed.Module, Parsed.Legacy.Get());
			if (Outcome.bSucceeded)
			{
				Outcome.Text = PrintDreamShaderLang(*Parsed.Module);
			}
		}

		bool bUpdated = false;
		if (!AssertDreamShaderTextOutcome(Test, Case, EntryPoint, Outcome, Expectation, bUpdated) || !Expectation.bCheckRoundtrip)
		{
			return true;
		}

		// What the legacy tree prints is 2.0 SYNTAX (its meaning still needs the legacy rules until it is migrated): it
		// parses with the 2.0 front end, and printing that parse gives the same text back.
		const FString PrintedPath = MakeDreamShaderPrintedPath(Case.SourcePath, TEXT(".printed"));
		const FLangParseResult Reparsed = ParseDreamShaderLang(FLangSourceText(PrintedPath, Outcome.Text), ParseOptions);
		const TArray<FString> ReparseErrors = GatherDreamShaderLangDiagnostics(Reparsed.Diagnostics, ELangSeverity::Error);
		Test.TestTrue(
			FString::Printf(TEXT("[%s] the printed text parses as 2.0 source (errors: %s)"), *Case.SourcePath, *FString::Join(ReparseErrors, TEXT(" | "))),
			Reparsed.Succeeded());
		if (Reparsed.Succeeded() && Reparsed.Module.IsValid())
		{
			const FString PrintedAgain = PrintDreamShaderLang(*Reparsed.Module);
			const bool bStable = PrintedAgain.Equals(Outcome.Text, ESearchCase::CaseSensitive);
			Test.TestTrue(FString::Printf(TEXT("[%s] print(parse(printed)) == printed"), *Case.SourcePath), bStable);
			if (!bStable)
			{
				Test.AddInfo(FString::Printf(TEXT("[%s] %s"), *Case.SourcePath, *DescribeDreamShaderTextDifference(PrintedAgain, Outcome.Text)));
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Decompile: IR -> AST -> text, and the text back to the same IR
	// ---------------------------------------------------------------------------------------------

	/**
	 * The engine-free half of a decompile, over the IR a SOURCE lowered to. That IR is what the importer aims to
	 * reproduce from an asset (names, regions and layout hints included), so everything downstream of the importer is
	 * exercised here without an asset in sight.
	 */
	inline bool DecompileDreamShaderIRToText(
		const UE::DreamShader::IR::FIRModule& Module,
		const UE::DreamShader::Lang::FIRToAstOptions& Options,
		FString& OutText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace UE::DreamShader::Lang;

		// Raise works in place, and the caller still needs what it lowered.
		UE::DreamShader::IR::FIRModule Working = Module;
		RaiseDreamShaderIR(Working, Diagnostics, &GetDreamShaderTestBuiltinCatalog());

		const TUniquePtr<FModule> Ast = BuildDreamShaderAstFromIR(Working, GetDreamShaderTestBuiltinCatalog(), Options, Diagnostics);
		if (!Ast.IsValid() || Diagnostics.HasErrors())
		{
			return false;
		}
		OutText = PrintDreamShaderLang(*Ast);
		return true;
	}

	/**
	 * One `.dss` through the decompile layer. bTextGolden false is the RoundtripIR sweep over the other corpora: no
	 * golden of its own, only "the decompiled text lowers to an equivalent IR", and a fixture that does not build in the
	 * first place is none of this layer's business.
	 */
	inline bool RunDreamShaderDecompileCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case, const bool bTextGolden = true)
	{
		using namespace UE::DreamShader::Lang;
		const TCHAR* EntryPoint = TEXT("decompile");

		if (!Case.Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a .dss compilation unit; the decompile layer skips it."), *Case.SourcePath));
			return true;
		}

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FTextCorpusExpectation Expectation;
		if (bTextGolden && !LoadDreamShaderTextExpectation(Test, Case, EntryPoint, Expectation))
		{
			return false;
		}

		FDreamShaderIRRunOptions RunOptions;
		RunOptions.IncludeDirectory = FPaths::GetPath(Case.SourcePath);

		FDreamShaderIRRun Source;
		RunDreamShaderIRPipeline(Case.SourcePath, SourceString, RunOptions, Source);
		if (!Source.Module.IsValid() || !Source.Succeeded())
		{
			if (bTextGolden)
			{
				Test.AddError(FString::Printf(TEXT("[%s] a decompile fixture has to build first: %s"), *Case.SourcePath, *Source.ErrorText()));
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("[%s] does not build; nothing to round-trip."), *Case.SourcePath));
			return true;
		}

		FIRToAstOptions AstOptions;
		AstOptions.bEmitLayout = true;

		FTextCorpusOutcome Outcome;
		FLangDiagnosticSink DecompileSink(Case.SourcePath);
		Outcome.bSucceeded = DecompileDreamShaderIRToText(*Source.Module, AstOptions, Outcome.Text, DecompileSink);
		Outcome.Gather(DecompileSink);

		if (bTextGolden)
		{
			bool bUpdated = false;
			if (!AssertDreamShaderTextOutcome(Test, Case, EntryPoint, Outcome, Expectation, bUpdated) || !Expectation.bCheckRoundtrip)
			{
				return true;
			}
		}
		else if (!Outcome.bSucceeded)
		{
			Test.AddError(FString::Printf(TEXT("[%s] builds, and its IR does not decompile: %s"), *Case.SourcePath, *FString::Join(Outcome.Errors, TEXT(" | "))));
			return false;
		}

		// The fixed point that matters: not the same text, the same graph.
		FDreamShaderIRRun Again;
		RunDreamShaderIRPipeline(MakeDreamShaderPrintedPath(Case.SourcePath, TEXT(".decompiled")), Outcome.Text, RunOptions, Again);
		if (!Again.Module.IsValid() || !Again.Succeeded())
		{
			Test.AddError(FString::Printf(TEXT("[%s] the decompiled text does not build: %s\n%s"), *Case.SourcePath, *Again.ErrorText(), *Outcome.Text));
			return false;
		}

		UE::DreamShader::IR::FIRCompareOptions CompareOptions;
		CompareOptions.bCompareDestinations = Expectation.bCompareDestinations;
		FString Difference;
		const bool bEquivalent = UE::DreamShader::IR::AreDreamShaderIRModulesEquivalent(*Source.Module, *Again.Module, CompareOptions, Difference);
		Test.TestTrue(FString::Printf(TEXT("[%s] the decompiled text lowers to an equivalent IR (%s)"), *Case.SourcePath, *Difference), bEquivalent);
		if (!bEquivalent)
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] decompiled text:\n%s"), *Case.SourcePath, *Outcome.Text));
		}
		return true;
	}

	/** `Tests/Corpus/Decompile/roundtrip-skips.json`: `{ "IR/Loops/L_Unroll": "why" }`, keyed by corpus-relative name. */
	inline const TMap<FString, FString>& GetDreamShaderRoundtripSkips()
	{
		static TMap<FString, FString> Skips;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString JsonText;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(JsonText, *FPaths::Combine(GetDreamShaderCorpusRoot(), TEXT("Decompile"), TEXT("roundtrip-skips.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(JsonText), Root) && Root.IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
				{
					Skips.Add(Pair.Key.Replace(TEXT("\\"), TEXT("/")), Pair.Value->AsString());
				}
			}
		}
		return Skips;
	}

	// ---------------------------------------------------------------------------------------------
	// Migrate: 1.x text -> 2.0 text that lowers to the same IR and keeps every comment
	// ---------------------------------------------------------------------------------------------

	/** Every comment of Before that After does not have, counted: the engine-free half of DSH9092. */
	inline TArray<FString> FindDreamShaderLostComments(const FString& BeforePath, const FString& BeforeText, const FString& AfterPath, const FString& AfterText)
	{
		using namespace UE::DreamShader::Lang;

		TArray<FString> Before;
		TArray<FString> After;
		CollectDreamShaderComments(FLangSourceText(BeforePath, BeforeText), /*bIncludeDocComments*/ true, Before);
		CollectDreamShaderComments(FLangSourceText(AfterPath, AfterText), /*bIncludeDocComments*/ true, After);

		TArray<FString> Lost;
		for (const FString& Comment : Before)
		{
			const int32 Found = After.IndexOfByPredicate([&Comment](const FString& Candidate) { return Candidate.Equals(Comment, ESearchCase::CaseSensitive); });
			if (Found == INDEX_NONE)
			{
				Lost.Add(Comment);
			}
			else
			{
				After.RemoveAtSwap(Found);
			}
		}
		return Lost;
	}

	inline bool RunDreamShaderMigrateCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		using namespace UE::DreamShader::Lang;
		const TCHAR* EntryPoint = TEXT("migrate");

		const FString Extension = Case.Extension.ToLower();
		if (Extension != TEXT("dsm") && Extension != TEXT("dsf") && Extension != TEXT("dsh"))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a 1.x source; the migrate layer skips it."), *Case.SourcePath));
			return true;
		}
		// A header beside a fixture is what the fixture imports; one that is a fixture itself says so in its name.
		if (Extension == TEXT("dsh") && !FPaths::GetBaseFilename(Case.SourcePath).StartsWith(TEXT("H_"), ESearchCase::CaseSensitive))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is a header a fixture imports (a header fixture is named H_*); skipped."), *Case.SourcePath));
			return true;
		}

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FTextCorpusExpectation Expectation;
		if (!LoadDreamShaderTextExpectation(Test, Case, EntryPoint, Expectation))
		{
			return false;
		}

		FDreamShaderIRRunOptions RunOptions;
		RunOptions.IncludeDirectory = FPaths::GetPath(Case.SourcePath);
		RunOptions.bKeepTrivia = true;

		FDreamShaderIRRun Legacy;
		RunDreamShaderIRPipeline(Case.SourcePath, SourceString, RunOptions, Legacy);

		FTextCorpusOutcome Outcome;
		Outcome.Gather(Legacy);
		const bool bLegacyBuilt = Legacy.Parse.Module.IsValid() && Legacy.Bind.Bound.IsValid() && Legacy.Errors.Num() == 0;
		if (!bLegacyBuilt && !Expectation.bExpectError)
		{
			Test.AddError(FString::Printf(TEXT("[%s] a migrate fixture has to build as 1.x first: %s"), *Case.SourcePath, *Legacy.ErrorText()));
			return false;
		}

		if (bLegacyBuilt)
		{
			FLangMigrateOptions MigrateOptions;
			for (const TPair<FString, FString>& Named : Expectation.ProductNames)
			{
				for (const FDeclPtr& Decl : Legacy.Parse.Module->Declarations)
				{
					const FFunctionDecl* Function = Decl.IsValid() ? Decl->As<FFunctionDecl>() : nullptr;
					if (Function && Function->Name.Equals(Named.Key, ESearchCase::CaseSensitive))
					{
						FLangMigrateProductName Answer;
						Answer.Decl = Function;
						Answer.Name = Named.Value;
						MigrateOptions.ProductNames.Add(Answer);
					}
				}
			}

			const FLegacyMigrationInfo NoInfo;
			FLangDiagnosticSink MigrateSink(Case.SourcePath);
			const bool bMigrated = MigrateDreamShaderLegacyModule(
				*Legacy.Parse.Module,
				Legacy.Parse.Legacy.IsValid() ? *Legacy.Parse.Legacy : NoInfo,
				*Legacy.Bind.Bound,
				MigrateOptions,
				MigrateSink);
			Outcome.Gather(MigrateSink);
			Outcome.bSucceeded = bMigrated && !MigrateSink.HasErrors();
			if (Outcome.bSucceeded)
			{
				Outcome.Text = PrintDreamShaderLang(*Legacy.Parse.Module);
			}
		}

		bool bUpdated = false;
		if (!AssertDreamShaderTextOutcome(Test, Case, EntryPoint, Outcome, Expectation, bUpdated))
		{
			return true;
		}

		const FString MigratedPath = MakeDreamShaderPrintedPath(Case.SourcePath, TEXT(".migrated"));

		if (Expectation.bCheckComments)
		{
			const TArray<FString> Lost = FindDreamShaderLostComments(Case.SourcePath, SourceString, MigratedPath, Outcome.Text);
			Test.TestEqual(
				FString::Printf(TEXT("[%s] comments the migrated text lost (%s)"), *Case.SourcePath, *FString::Join(Lost, TEXT(" | "))),
				Lost.Num(),
				0);
		}

		if (!Expectation.bCheckRoundtrip)
		{
			return true;
		}

		FDreamShaderIRRunOptions AgainOptions;
		AgainOptions.IncludeDirectory = RunOptions.IncludeDirectory;
		FDreamShaderIRRun Again;
		RunDreamShaderIRPipeline(MigratedPath, Outcome.Text, AgainOptions, Again);
		const bool bHeader = Extension == TEXT("dsh");
		if (Again.Errors.Num() > 0 || (!bHeader && (!Again.Module.IsValid() || !Again.Succeeded())))
		{
			Test.AddError(FString::Printf(TEXT("[%s] the migrated text does not build as 2.0 source: %s\n%s"), *Case.SourcePath, *Again.ErrorText(), *Outcome.Text));
			return false;
		}
		if (bHeader || !Legacy.Module.IsValid())
		{
			// A header lowers to nothing on its own: that it binds is what there is to say.
			return true;
		}

		// `@root` against `@name`: the two sides spell where the asset goes differently, and only the host can resolve either.
		// A verbatim body is written back without its Namespace block's indentation and with its lifted calls respelled.
		UE::DreamShader::IR::FIRCompareOptions CompareOptions;
		// And a literal on a pin that has a `Const*` twin is a Constant node on the 1.x side and the twin on the other.
		CompareOptions.bCompareDestinations = false;
		CompareOptions.bCompareCodeSpacing = false;
		CompareOptions.ConstTwinCatalog = &GetDreamShaderTestBuiltinCatalog();
		// And `.rgb` of a float3 is a mask node in a 1.x product, as 1.x built one, and nothing in a `.dss`.
		CompareOptions.bCompareIdentitySwizzles = false;
		FString Difference;
		const bool bEquivalent = UE::DreamShader::IR::AreDreamShaderIRModulesEquivalent(*Legacy.Module, *Again.Module, CompareOptions, Difference);
		Test.TestTrue(FString::Printf(TEXT("[%s] the migrated text lowers to an equivalent IR (%s)"), *Case.SourcePath, *Difference), bEquivalent);
		if (!bEquivalent)
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] migrated text:\n%s"), *Case.SourcePath, *Outcome.Text));
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Roundtrip (engine): source -> assets -> decompiled text -> assets, and the two sets of graphs are one
	// ---------------------------------------------------------------------------------------------

	/**
	 * Golden schema (every field optional):
	 *
	 *   {
	 *     "entryPoint": "roundtrip",
	 *     "readable": true,                 // decompile with bReadable
	 *     "siblings": ["Shared.dsh"],
	 *     "warningsContain": ["DSH9073"],   // codes the DECOMPILE reports
	 *     "ignoreKeys": ["Desc"],           // dump keys dropped on both sides before the compare, each with its reason in the fixture
	 *     "textPending": true, "text": "<the decompiled text>"
	 *   }
	 *
	 * The assertion that needs no golden is the point of the layer: the graph dump of what the decompiled text builds
	 * equals the graph dump of what the source built.
	 */
	struct FRoundtripCorpusExpectation
	{
		FString EntryPoint;
		bool bReadable = false;
		TArray<FString> Siblings;
		TArray<FString> WarningsContain;
		TArray<FString> IgnoreKeys;
		bool bTextPending = false;
		bool bCheckText = false;          FString Text;
	};

	inline bool ParseDreamShaderRoundtripExpectation(const FString& JsonText, FRoundtripCorpusExpectation& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(JsonText), Root) || !Root.IsValid())
		{
			OutError = TEXT("invalid JSON");
			return false;
		}
		Root->TryGetStringField(TEXT("entryPoint"), Out.EntryPoint);
		Root->TryGetBoolField(TEXT("readable"), Out.bReadable);
		Root->TryGetBoolField(TEXT("textPending"), Out.bTextPending);
		Out.bCheckText = Root->TryGetStringField(TEXT("text"), Out.Text);

		const auto ReadStrings = [&Root](const TCHAR* Key, TArray<FString>& OutValues)
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Root->TryGetArrayField(Key, Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					OutValues.Add(Value->AsString());
				}
			}
		};
		ReadStrings(TEXT("siblings"), Out.Siblings);
		ReadStrings(TEXT("warningsContain"), Out.WarningsContain);
		ReadStrings(TEXT("ignoreKeys"), Out.IgnoreKeys);
		return true;
	}

	inline FString BuildDreamShaderRoundtripGoldenJson(const FRoundtripCorpusExpectation& Expectation, const FString& DecompiledText, const TArray<FString>& WarningCodes)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("entryPoint"), TEXT("roundtrip"));
		if (Expectation.bReadable) { Root->SetBoolField(TEXT("readable"), true); }

		const auto WriteStrings = [&Root](const TCHAR* Key, const TArray<FString>& InValues)
		{
			if (InValues.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> Values;
				for (const FString& Value : InValues)
				{
					Values.Add(MakeShared<FJsonValueString>(Value));
				}
				Root->SetArrayField(Key, Values);
			}
		};
		WriteStrings(TEXT("siblings"), Expectation.Siblings);
		WriteStrings(TEXT("warningsContain"), WarningCodes);
		WriteStrings(TEXT("ignoreKeys"), Expectation.IgnoreKeys);

		if (Expectation.bTextPending) { Root->SetBoolField(TEXT("textPending"), true); }
		Root->SetStringField(TEXT("text"), DecompiledText);

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	/**
	 * Whether two builds are one graph. Both sets of assets are read back into IR (the decompiler's importer) and
	 * compared structurally, because the dump lists nodes in the order a build created them, and the decompiled text
	 * need not create them in the order the source did: a uniform declared earlier, two statements swapped. What a
	 * graph IS -- every node, property, wire and setting -- is what AreDreamShaderIRModulesEquivalent compares.
	 *
	 * The second build lives under another scratch root, so what names a package there is rewritten to the first
	 * root before comparing (a call to a function of the same file), and the products' own destinations are skipped.
	 */
	inline bool AreDreamShaderRoundtripBuildsOneGraph(
		const TArray<FDreamShaderCompiledAsset>& FirstAssets,
		const FString& FirstPackagePath,
		const TArray<FDreamShaderCompiledAsset>& SecondAssets,
		const FString& SecondPackagePath,
		FString& OutDifference)
	{
		using namespace UE::DreamShader;

		const auto Import = [](const TArray<FDreamShaderCompiledAsset>& Assets, IR::FIRModule& OutModule, FString& OutError) -> bool
		{
			TArray<const FDreamShaderCompiledAsset*> Ordered;
			for (const FDreamShaderCompiledAsset& Asset : Assets)
			{
				Ordered.Add(&Asset);
			}
			Ordered.Sort([](const FDreamShaderCompiledAsset& Left, const FDreamShaderCompiledAsset& Right) { return Left.Name < Right.Name; });

			UE::DreamShader::Editor::Private::FGraphImportOptions Options;
			Options.bImportLayout = false;
			UE::DreamShader::Editor::Private::FGraphImportContext Context;
			Lang::FLangDiagnosticSink Sink(TEXT("roundtrip"));
			for (const FDreamShaderCompiledAsset* Asset : Ordered)
			{
				UObject* Object = LoadObject<UObject>(nullptr, *Asset->ObjectPath);
				// A ThinCustom pair is read through its hidden base, which is where its graph is (the importer leaves that
				// step to its caller).
				if (const UDreamShaderMaterialInstance* Pair = Cast<UDreamShaderMaterialInstance>(Object))
				{
					Object = Pair->Parent.Get();
				}
				if (!Object || !UE::DreamShader::Editor::Private::ImportDreamShaderGraphToIR(Object, Options, OutModule, Context, Sink))
				{
					OutError = FString::Printf(TEXT("'%s' could not be read back into IR (%s)"), *Asset->Name, *FString::Join(GatherDreamShaderLangDiagnostics(Sink, Lang::ELangSeverity::Error), TEXT(" | ")));
					return false;
				}
			}
			return true;
		};

		IR::FIRModule First;
		IR::FIRModule Second;
		if (!Import(FirstAssets, First, OutDifference) || !Import(SecondAssets, Second, OutDifference))
		{
			return false;
		}

		// One scratch root for both: `/Game/DreamShaderTests/RoundtripBack/X` names what `/Game/DreamShaderTests/Roundtrip/X` names.
		const auto Rehome = [&FirstPackagePath, &SecondPackagePath](FString& Text)
		{
			if (!SecondPackagePath.IsEmpty() && Text.Contains(SecondPackagePath, ESearchCase::IgnoreCase))
			{
				Text.ReplaceInline(*SecondPackagePath, *FirstPackagePath, ESearchCase::IgnoreCase);
			}
		};
		for (IR::FIRProduct& Product : Second.Products)
		{
			for (IR::FIRNode& Node : Product.Graph.Nodes)
			{
				Rehome(Node.ClassName);
				for (IR::FIRProperty& Property : Node.Properties)
				{
					Rehome(Property.Value.S);
				}
			}
		}

		IR::FIRCompareOptions CompareOptions;
		CompareOptions.bCompareDestinations = false;
		return IR::AreDreamShaderIRModulesEquivalent(First, Second, CompareOptions, OutDifference);
	}

	/** Compile one fixture file set into Fixture and dump what it made; false with the error on Test. */
	inline bool CompileDreamShaderRoundtripSide(
		FAutomationTestBase& Test,
		FDreamShaderCompile2Fixture& Fixture,
		const FString& What,
		TArray<FDreamShaderCompiledAsset>& OutAssets)
	{
		UE::DreamShader::FDreamShaderError Error;
		if (!::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true, /*bEphemeralThinCustom*/ false))
		{
			Test.AddError(FString::Printf(TEXT("%s does not compile: %s: %s"), *What, *Error.Code, *Error.Message));
			return false;
		}
		Fixture.CollectProducedAssets(OutAssets);
		return true;
	}

	inline bool RunDreamShaderRoundtripCorpusCase(FAutomationTestBase& Test, const FCorpusCase& Case)
	{
		if (!Case.Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] is not a .dss compilation unit; the Roundtrip layer skips it."), *Case.SourcePath));
			return true;
		}

		FString SourceString;
		if (!FFileHelper::LoadFileToString(SourceString, *Case.SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read corpus source '%s'."), *Case.SourcePath));
			return false;
		}

		FRoundtripCorpusExpectation Expectation;
		if (Case.bHasExpectationFile)
		{
			FString JsonText;
			FString JsonError;
			if (!FFileHelper::LoadFileToString(JsonText, *Case.ExpectedPath) || !ParseDreamShaderRoundtripExpectation(JsonText, Expectation, JsonError))
			{
				Test.AddError(FString::Printf(TEXT("Cannot read golden '%s': %s"), *Case.ExpectedPath, *JsonError));
				return false;
			}
		}

		FString RelativeName = Case.SourcePath;
		const FString LayerRoot = FPaths::Combine(GetDreamShaderCorpusRoot(), TEXT("Roundtrip")) / TEXT("");
		FPaths::MakePathRelativeTo(RelativeName, *LayerRoot);
		RelativeName = FPaths::GetBaseFilename(RelativeName, /*bRemovePath*/ false);

		FScopedDreamShaderGraphBackendPin BackendPin;

		// Two scratch roots: the source's, and the decompiled text's. The same asset names in both, because a function
		// product is named after its declaration; the package paths differ, and the dumps do not say them.
		FDreamShaderCompile2Fixture First(RelativeName, TEXT("Roundtrip"));
		FDreamShaderCompile2Fixture Second(RelativeName, TEXT("RoundtripBack"));
		Test.AddExpectedError(First.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(Second.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		if (!First.WriteSource(Test, SourceString))
		{
			return false;
		}

		FCorpusCase SiblingCase = Case;
		TArray<FString> Siblings;
		CollectDreamShaderFixtureSiblings(SiblingCase, SourceString, Expectation.Siblings, Siblings);
		for (const FString& Sibling : Siblings)
		{
			FString SiblingText;
			FString Written;
			if (!FFileHelper::LoadFileToString(SiblingText, *Sibling)
				|| !First.WriteSiblingSource(Test, FPaths::GetCleanFilename(Sibling), SiblingText, Written)
				|| !Second.WriteSiblingSource(Test, FPaths::GetCleanFilename(Sibling), SiblingText, Written))
			{
				Test.AddError(FString::Printf(TEXT("[%s] cannot copy the sibling '%s'."), *RelativeName, *Sibling));
				return false;
			}
		}

		TArray<FDreamShaderCompiledAsset> FirstAssets;
		if (!CompileDreamShaderRoundtripSide(Test, First, FString::Printf(TEXT("[%s] the source"), *RelativeName), FirstAssets))
		{
			return false;
		}

		// Every product of the source into ONE text: the request the Adopt of a multi-product source makes.
		FString LoadError;
		UObject* AnyProduct = LoadDreamShaderDecompileSourceProduct(First.GetSourceFilePath(), LoadError);
		if (!AnyProduct)
		{
			Test.AddError(FString::Printf(TEXT("[%s] no product of the source could be loaded: %s"), *RelativeName, *LoadError));
			return false;
		}

		::UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = AnyProduct;
		Request.Format = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
		Request.SourceFilePath = First.GetSourceFilePath();
		Request.OutputFilePath = Second.GetSourceFilePath();
		Request.bReadable = Expectation.bReadable;
		const ::UE::DreamShader::Editor::FDreamShaderDecompileResult Decompiled = RunDreamShaderDecompileRequest(Request);
		if (!Decompiled.bSucceeded)
		{
			Test.AddError(FString::Printf(TEXT("[%s] the decompile failed: %s"), *RelativeName, *DescribeDreamShaderDecompileFailure(Decompiled)));
			return false;
		}

		TArray<FString> WarningCodes;
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Decompiled.Diagnostics)
		{
			if (Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Warning)
			{
				WarningCodes.AddUnique(Diagnostic.Code);
			}
		}

		if (ShouldUpdateDreamShaderGolden())
		{
			const FString Json = BuildDreamShaderRoundtripGoldenJson(Expectation, Decompiled.SourceText, WarningCodes);
			if (!FFileHelper::SaveStringToFile(Json, *Case.ExpectedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(FString::Printf(TEXT("Failed to write golden '%s'."), *Case.ExpectedPath));
			}
			return true;
		}

		for (const FString& Needle : Expectation.WarningsContain)
		{
			Test.TestTrue(
				FString::Printf(TEXT("[%s] the decompile warns '%s' (actual: %s)"), *RelativeName, *Needle, *FString::Join(WarningCodes, TEXT(", "))),
				WarningCodes.Contains(Needle));
		}
		if (Expectation.bCheckText && !Expectation.bTextPending)
		{
			const bool bEqual = Decompiled.SourceText.Equals(Expectation.Text, ESearchCase::CaseSensitive);
			Test.TestTrue(FString::Printf(TEXT("[%s] the decompiled text matches its golden"), *RelativeName), bEqual);
			if (!bEqual)
			{
				Test.AddInfo(FString::Printf(TEXT("[%s] %s"), *RelativeName, *DescribeDreamShaderTextDifference(Decompiled.SourceText, Expectation.Text)));
			}
		}

		// `/// @name` would send the second build to the first build's packages; this request did not ask to keep them.
		if (!Second.WriteSource(Test, Decompiled.SourceText))
		{
			return false;
		}
		TArray<FDreamShaderCompiledAsset> SecondAssets;
		if (!CompileDreamShaderRoundtripSide(Test, Second, FString::Printf(TEXT("[%s] the decompiled text"), *RelativeName), SecondAssets))
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] decompiled text:\n%s"), *RelativeName, *Decompiled.SourceText));
			return false;
		}

		// Graph for graph, whatever order each build made its nodes in. The dump texts say where they part when they do.
		FString Difference;
		const bool bSame = AreDreamShaderRoundtripBuildsOneGraph(FirstAssets, First.GetPackagePath(), SecondAssets, Second.GetPackagePath(), Difference);
		Test.TestTrue(FString::Printf(TEXT("[%s] the decompiled text builds the graphs the source built"), *RelativeName), bSame);
		if (!bSame)
		{
			const FString FirstDump = FilterDreamShaderGraphDumpKeys(BuildDreamShaderCompiledGraphDumpText(FirstAssets), Expectation.IgnoreKeys);
			const FString SecondDump = FilterDreamShaderGraphDumpKeys(BuildDreamShaderCompiledGraphDumpText(SecondAssets), Expectation.IgnoreKeys);
			Test.AddInfo(FString::Printf(TEXT("[%s] the graphs differ: %s"), *RelativeName, *Difference));
			Test.AddInfo(FString::Printf(TEXT("[%s] the dumps part at: %s"), *RelativeName, *DescribeDreamShaderTextDifference(SecondDump, FirstDump)));
			Test.AddInfo(FString::Printf(TEXT("[%s] decompiled text:\n%s"), *RelativeName, *Decompiled.SourceText));
		}
		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
