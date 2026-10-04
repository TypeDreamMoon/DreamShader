// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Commandlet/DreamShaderMigrate.h"

#include "Commandlet/DreamShaderCommandletRunner.h"
#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilerIncludes.h"
#include "DreamShaderDefineResolution.h"
#include "DreamShaderDefineTable.h"
#include "DreamShaderDependencyGraphService.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "DreamShaderSettings.h"
#include "DreamShaderSourceFileUtils.h"
#include "IR/IR.h"
#include "IR/IRBuilder.h"
#include "IR/IRCompare.h"
#include "IR/IRPasses.h"
#include "IR/IRValidator.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Migrate/LangMigrate.h"
#include "Semantic/LangBound.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DreamShader.Migrate"

namespace UE::DreamShader::Editor::Private
{
	namespace MigratePrivate
	{
		namespace Lang = ::UE::DreamShader::Lang;
		namespace IR = ::UE::DreamShader::IR;

		/**
		 * One file through the front end, owned. The member order is the pipeline result's, for its reason: the bound
		 * module points into the parsed module and into what the include resolver parsed, and members die in reverse.
		 */
		struct FFrontEndRun
		{
			Lang::FLangDiagnosticSink Diagnostics;
			bool bHadDirectives = false;

			TUniquePtr<::UE::DreamShader::FDreamShaderDefineTable> Defines;
			TUniquePtr<Lang::FLangSourceText> Source;
			TUniquePtr<Lang::FModule> Module;
			TUniquePtr<Lang::FLegacyMigrationInfo> Legacy;
			TUniquePtr<Compiler::FDreamShaderIncludeResolver> Includes;
			TUniquePtr<Lang::FBoundModule> Bound;
			TUniquePtr<IR::FIRModule> IR;
		};

		static ::UE::DreamShader::EDreamShaderPreprocessDialect DialectOf(const Lang::ELangFileKind Kind)
		{
			// The pipeline's GetDreamShaderPreprocessDialectForFile, which is private to it.
			switch (Kind)
			{
			case Lang::ELangFileKind::Dsm:
			case Lang::ELangFileKind::Dsf:
				return ::UE::DreamShader::EDreamShaderPreprocessDialect::Legacy;
			case Lang::ELangFileKind::Dsh:
				return ::UE::DreamShader::EDreamShaderPreprocessDialect::Mixed;
			case Lang::ELangFileKind::Dsp:
				return ::UE::DreamShader::EDreamShaderPreprocessDialect::Pipeline;
			default:
				return ::UE::DreamShader::EDreamShaderPreprocessDialect::Lang2;
			}
		}

		static IR::EIRBackend GetProjectDefaultBackend()
		{
			// The pipeline's ResolveDefaultBackend: Instance is the old spelling of ThinCustom.
			if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>())
			{
				return Settings->DefaultBackend == EDreamShaderDefaultBackend::Graph ? IR::EIRBackend::Graph : IR::EIRBackend::ThinCustom;
			}
			return IR::EIRBackend::Graph;
		}

		/**
		 * Text as the file at FilePath would be compiled, as far as the validator: the pipeline's front half, from a text
		 * that does not have to be on disk. A header stops after the bind, because it lowers to nothing on its own.
		 */
		static bool RunFrontEnd(const FString& FilePath, const FString& RawText, const bool bKeepTrivia, const bool bRefuseConditionals, FFrontEndRun& Out)
		{
			const Lang::FLangSpan FileSpan;
			const Lang::ELangFileKind Kind = Lang::GetLangFileKindFromPath(FilePath);
			Out.Diagnostics = Lang::FLangDiagnosticSink(FilePath);
			Out.Defines = MakeUnique<::UE::DreamShader::FDreamShaderDefineTable>(::UE::DreamShader::ResolveDreamShaderDefines());

			::UE::DreamShader::FDreamShaderPreprocessResult Preprocessed;
			::UE::DreamShader::FDreamShaderTextError PreprocessError;
			if (!::UE::DreamShader::PreprocessDreamShaderSource(RawText, FilePath, *Out.Defines, Preprocessed, PreprocessError, DialectOf(Kind)))
			{
				Out.bHadDirectives = true;
				Out.Diagnostics.Error(TEXT("DSH9090"), FileSpan, FText::Format(
					LOCTEXT("PreprocessFailed", "'{0}' fails conditional compilation ({1}: {2}), and a source with '#if' lines is not migrated in any case."),
					FText::FromString(FilePath),
					FText::FromString(PreprocessError.Code),
					PreprocessError.Message));
				return false;
			}
			Out.bHadDirectives = Preprocessed.bHadDirectives;
			if (Out.bHadDirectives && bRefuseConditionals)
			{
				// Before anything else is said about the file: whatever follows would be about one branch of it.
				Out.Diagnostics.Error(TEXT("DSH9090"), FileSpan, FText::Format(
					LOCTEXT("ConditionalSource", "'{0}' uses '#if' conditional compilation, and only the branch taken with today's defines would reach the migrated file; migrate it by hand, or remove the conditionals first."),
					FText::FromString(FilePath)));
				return false;
			}

			Out.Source = MakeUnique<Lang::FLangSourceText>(FilePath, Preprocessed.Text);

			Lang::FLangParseOptions ParseOptions;
			ParseOptions.Frontend = Lang::ELangFrontend::Auto;
			ParseOptions.bKeepTrivia = bKeepTrivia;
			Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(*Out.Source, ParseOptions);
			const bool bParsed = Parsed.Succeeded();
			Out.Module = MoveTemp(Parsed.Module);
			Out.Legacy = MoveTemp(Parsed.Legacy);
			Out.Diagnostics.Append(MoveTemp(Parsed.Diagnostics));
			if (!bParsed)
			{
				return false;
			}

			// As the compile pipeline does between its parse and its bind: a 1.x `TextureObjectParameter` has the dimension
			// of its asset, and the declaration is retyped to it -- which is also the type the migrated text then declares.
			if (Out.Legacy.IsValid() && Out.Module.IsValid())
			{
				Compiler::ResolveDreamShaderLegacyTextureTypes(*Out.Module, *Out.Legacy);
			}

			const IR::FBuiltinCatalog& Catalog = Compiler::GetDreamShaderBuiltinCatalog();
			if (Catalog.IsEmpty())
			{
				Out.Diagnostics.Error(TEXT("DSH9099"), FileSpan, LOCTEXT("CatalogEmpty",
					"The builtin expression catalog came back empty, so nothing that names a 'UE.*' node can be bound and nothing can be migrated."));
				return false;
			}

			Out.Includes = MakeUnique<Compiler::FDreamShaderIncludeResolver>(*Out.Defines);

			Lang::FBindOptions BindOptions;
			BindOptions.Catalog = &Catalog;
			BindOptions.IncludeResolver = Out.Includes->MakeBinderResolver();
			BindOptions.DefaultBackend = GetProjectDefaultBackend();
			Lang::FLangBindResult BoundResult = Lang::BindDreamShaderLang(*Out.Module, BindOptions);
			const bool bBound = BoundResult.Succeeded();
			Out.Bound = MoveTemp(BoundResult.Bound);
			Out.Diagnostics.Append(MoveTemp(BoundResult.Diagnostics));
			if (!bBound)
			{
				return false;
			}
			if (Kind == Lang::ELangFileKind::Dsh)
			{
				return true;
			}

			IR::FIRBuildOptions BuildOptions;
			BuildOptions.Catalog = &Catalog;
			BuildOptions.StampSourcePath = [](const FString& File)
			{
				return MakeProjectRelativeSourcePath(File);
			};
			Out.IR = IR::BuildDreamShaderIR(*Out.Bound, BuildOptions, Out.Diagnostics);
			if (!Out.IR.IsValid() || Out.Diagnostics.HasErrors())
			{
				return false;
			}

			IR::RunDreamShaderIRPasses(*Out.IR, IR::FIRPassOptions(), Out.Diagnostics);
			if (Out.Diagnostics.HasErrors())
			{
				return false;
			}
			return IR::ValidateDreamShaderIR(*Out.IR, Catalog, Out.Diagnostics);
		}

		/** A header has something to migrate when any of its declarations is 1.x, an `import` included. */
		static bool HasLegacyDeclarations(const Lang::FModule& Module)
		{
			for (const Lang::FDeclPtr& Decl : Module.Declarations)
			{
				if (!Decl)
				{
					continue;
				}
				const Lang::FIncludeDecl* Include = Decl->As<Lang::FIncludeDecl>();
				if (Decl->bLegacy || (Include && Include->bImportSpelling))
				{
					return true;
				}
			}
			return false;
		}

		/** `<root name>/<path under the root>`, or `External/<file name>` for a file outside every root. */
		static FString MakeRootRelativePath(const FString& FilePath, const bool bWithRootName)
		{
			const ::UE::DreamShader::FDreamShaderSourceRoot* Root = ::UE::DreamShader::FindSourceRootForFile(FilePath);
			if (!Root || FilePath.Len() <= Root->Directory.Len() + 1)
			{
				return FPaths::Combine(TEXT("External"), FPaths::GetCleanFilename(FilePath));
			}
			const FString UnderRoot = FilePath.RightChop(Root->Directory.Len() + 1);
			return (bWithRootName || !Root->bIsProjectRoot) ? FPaths::Combine(Root->DisplayName, UnderRoot) : UnderRoot;
		}

		static FString MakeSavedPath(const TCHAR* Bucket, const FString& FilePath)
		{
			return ::UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Migrated"), Bucket, MakeRootRelativePath(FilePath, /* bWithRootName */ true)));
		}

		/** FilePath, or FilePath.1, FilePath.2...: an older backup is never written over. */
		static FString MakeFreeFilePath(const FString& FilePath)
		{
			FString Candidate = FilePath;
			for (int32 Suffix = 1; IFileManager::Get().FileExists(*Candidate); ++Suffix)
			{
				Candidate = FString::Printf(TEXT("%s.%d"), *FilePath, Suffix);
			}
			return Candidate;
		}

		// ------------------------------------------------------------------------------ comments

		/** The spellings the legacy front end changed inside bodies, applied to a comment: it rewrote those too. */
		static FString ApplyBodyRenames(const FString& Text, const Lang::FLegacyMigrationInfo& Legacy)
		{
			FString Result = Text;
			for (const TPair<FString, FString>& Flattened : Legacy.NamespaceFlatten)
			{
				Result.ReplaceInline(*Flattened.Key, *Flattened.Value, ESearchCase::CaseSensitive);
			}

			FString Renamed;
			Renamed.Reserve(Result.Len());
			for (int32 Index = 0; Index < Result.Len();)
			{
				const TCHAR Char = Result[Index];
				if (!FChar::IsAlpha(Char) && Char != TCHAR('_'))
				{
					Renamed.AppendChar(Char);
					++Index;
					continue;
				}
				int32 End = Index;
				while (End < Result.Len() && (FChar::IsAlnum(Result[End]) || Result[End] == TCHAR('_')))
				{
					++End;
				}
				const FString Word = Result.Mid(Index, End - Index);
				const Lang::FLegacyRename* Rename = Legacy.BodyRenames.FindByPredicate([&Word](const Lang::FLegacyRename& Candidate)
				{
					return Candidate.From.Equals(Word, ESearchCase::IgnoreCase);
				});
				Renamed += Rename ? Rename->To : Word;
				Index = End;
			}
			return Renamed;
		}

		/** Every comment of Before that After does not have, counted: two equal comments need two. */
		static void FindLostComments(TArray<FString> Before, TArray<FString> After, const Lang::FLegacyMigrationInfo* Legacy, TArray<FString>& OutLost)
		{
			const auto TakeFrom = [](TArray<FString>& Pool, const FString& Text)
			{
				const int32 Found = Pool.IndexOfByPredicate([&Text](const FString& Candidate) { return Candidate.Equals(Text, ESearchCase::CaseSensitive); });
				if (Found == INDEX_NONE)
				{
					return false;
				}
				Pool.RemoveAtSwap(Found);
				return true;
			};

			TArray<FString> Unmatched;
			for (const FString& Comment : Before)
			{
				if (!TakeFrom(After, Comment))
				{
					Unmatched.Add(Comment);
				}
			}
			for (const FString& Comment : Unmatched)
			{
				if (!Legacy || !TakeFrom(After, ApplyBodyRenames(Comment, *Legacy)))
				{
					OutLost.Add(Comment);
				}
			}
		}

		// -------------------------------------------------------------------------- destinations

		static bool ResolvePackage(const IR::FIRProduct& Product, const FString& FilePath, FString& OutPackageName)
		{
			FString ObjectPath;
			FString Error;
			return Compiler::ResolveDreamShaderProductDestination(Product, FilePath, OutPackageName, ObjectPath, Error);
		}

		/**
		 * What each product has to be called in the new file to stay the asset it is: nothing when the file's own place
		 * and the function's name already say it, the leaf when only the name differs, the whole path otherwise.
		 */
		static void DecideProductNames(const FFrontEndRun& LegacyRun, const FString& SourceFilePath, const FString& NominalOutputPath, Lang::FLangMigrateOptions& OutOptions)
		{
			if (!LegacyRun.IR.IsValid() || !LegacyRun.Bound.IsValid())
			{
				return;
			}
			for (const IR::FIRProduct& Product : LegacyRun.IR->Products)
			{
				if (!LegacyRun.Bound->Functions.IsValidIndex(Product.BoundFunctionIndex))
				{
					continue;
				}
				const Lang::FBoundFunction& Function = LegacyRun.Bound->Functions[Product.BoundFunctionIndex];
				FString LegacyPackage;
				if (!Function.Decl || !ResolvePackage(Product, SourceFilePath, LegacyPackage))
				{
					// No answer: the declaration keeps what it has, and the rewrite says so (DSH9094).
					continue;
				}

				int32 LeafStart = INDEX_NONE;
				const FString Leaf = LegacyPackage.FindLastChar(TCHAR('/'), LeafStart) ? LegacyPackage.RightChop(LeafStart + 1) : LegacyPackage;

				IR::FIRProduct Probe;
				Probe.Kind = Product.Kind;
				FString ProbePackage;

				Lang::FLangMigrateProductName Answer;
				Answer.Decl = Function.Decl;

				Probe.Name = Function.Name;
				if (ResolvePackage(Probe, NominalOutputPath, ProbePackage) && ProbePackage.Equals(LegacyPackage, ESearchCase::IgnoreCase))
				{
					OutOptions.ProductNames.Add(Answer);
					continue;
				}

				Probe.Name = Leaf;
				if (ResolvePackage(Probe, NominalOutputPath, ProbePackage) && ProbePackage.Equals(LegacyPackage, ESearchCase::IgnoreCase))
				{
					Answer.Name = Leaf;
					OutOptions.ProductNames.Add(Answer);
					continue;
				}

				Probe.AssetPathOverride = LegacyPackage;
				if (ResolvePackage(Probe, NominalOutputPath, ProbePackage) && ProbePackage.Equals(LegacyPackage, ESearchCase::IgnoreCase))
				{
					Answer.Name = LegacyPackage;
					OutOptions.ProductNames.Add(Answer);
				}
			}
		}

		// ------------------------------------------------------------------------------- results

		static void AppendDiagnostics(FDreamShaderMigrateResult& Result, const Lang::FLangDiagnosticSink& Sink, const FString& FallbackFilePath)
		{
			for (const Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				Lang::FLangDiagnostic Copy = Diagnostic;
				if (Copy.FilePath.IsEmpty())
				{
					Copy.FilePath = FallbackFilePath;
				}
				Result.Diagnostics.Add(MoveTemp(Copy));
			}
		}

		static bool Fail(FDreamShaderMigrateResult& Result)
		{
			Result.bSucceeded = false;
			const Lang::FLangDiagnostic* FirstError = Result.Diagnostics.FindByPredicate([](const Lang::FLangDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == Lang::ELangSeverity::Error;
			});
			Result.Error = FirstError
				? Compiler::FormatLang2DiagnosticWireLine(*FirstError, Result.SourceFilePath)
				: FString::Printf(TEXT("'%s' was not migrated."), *Result.SourceFilePath);
			return false;
		}

		static bool FailWith(FDreamShaderMigrateResult& Result, const TCHAR* Code, const FText& Message)
		{
			Lang::FLangDiagnosticSink Sink(Result.SourceFilePath);
			Sink.Error(Code, Lang::FLangSpan(), Message);
			AppendDiagnostics(Result, Sink, Result.SourceFilePath);
			return Fail(Result);
		}

		static void Warn(FDreamShaderMigrateResult& Result, const TCHAR* Code, const FText& Message)
		{
			Lang::FLangDiagnosticSink Sink(Result.SourceFilePath);
			Sink.Warning(Code, Lang::FLangSpan(), Message);
			AppendDiagnostics(Result, Sink, Result.SourceFilePath);
		}

		static bool SaveText(const FString& Text, const FString& FilePath)
		{
			return FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		// ---------------------------------------------------------------------------- commandlet

		/** A relative `-Source`: under the project's source folder first, then under the project, as `compile` reads it. */
		static FString ResolveSourceArgument(const FString& Argument)
		{
			const FString Value = NormalizeCommandletValue(Argument);
			if (Value.IsEmpty() || !FPaths::IsRelative(Value))
			{
				return ::UE::DreamShader::NormalizeSourceFilePath(Value);
			}
			for (const FString& Base : { ::UE::DreamShader::GetSourceShaderDirectory(), FPaths::ProjectDir() })
			{
				const FString Candidate = ::UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Base, Value));
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
			}
			return ::UE::DreamShader::NormalizeSourceFilePath(Value);
		}

		/** Under a root's `Packages` tree: somebody else's source, which `-All` and the header set leave alone. */
		static bool IsPackageFile(const FString& FilePath)
		{
			const ::UE::DreamShader::FDreamShaderSourceRoot* Root = ::UE::DreamShader::FindSourceRootForFile(FilePath);
			return Root && ::UE::DreamShader::IsPathUnderSourceDirectory(FilePath, Root->PackagesDirectory);
		}

		static bool IsLegacySourceFile(const FString& FilePath)
		{
			switch (Lang::GetLangFileKindFromPath(FilePath))
			{
			case Lang::ELangFileKind::Dsm:
			case Lang::ELangFileKind::Dsf:
			case Lang::ELangFileKind::Dsh:
				return true;
			default:
				return false;
			}
		}

		/** Headers, then functions, then materials: what is included before what includes it. */
		static int32 RankOf(const FString& FilePath)
		{
			switch (Lang::GetLangFileKindFromPath(FilePath))
			{
			case Lang::ELangFileKind::Dsh: return 0;
			case Lang::ELangFileKind::Dsf: return 1;
			default:                       return 2;
			}
		}

		static void LogDiagnostics(const FDreamShaderMigrateResult& Result)
		{
			for (const Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics)
			{
				const FString WireLine = Compiler::FormatLang2DiagnosticWireLine(Diagnostic, Result.SourceFilePath);
				switch (Diagnostic.Severity)
				{
				case Lang::ELangSeverity::Error:
					UE_LOG(LogDreamShader, Error, TEXT("%s"), *WireLine);
					break;
				case Lang::ELangSeverity::Warning:
					UE_LOG(LogDreamShader, Warning, TEXT("%s"), *WireLine);
					break;
				case Lang::ELangSeverity::Info:
					UE_LOG(LogDreamShader, Display, TEXT("%s"), *WireLine);
					break;
				}
			}
		}
	}

	bool MigrateDreamShaderSource(const FString& InSourceFilePath, const FDreamShaderMigrateOptions& Options, FDreamShaderMigrateResult& OutResult)
	{
		using namespace MigratePrivate;

		OutResult = FDreamShaderMigrateResult();
		const FString SourceFilePath = ::UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		OutResult.SourceFilePath = SourceFilePath;

		const Lang::ELangFileKind Kind = Lang::GetLangFileKindFromPath(SourceFilePath);
		const bool bIsHeader = Kind == Lang::ELangFileKind::Dsh;
		if (!IsLegacySourceFile(SourceFilePath))
		{
			return FailWith(OutResult, TEXT("DSH9095"), FText::Format(
				LOCTEXT("NotLegacySource", "'{0}' is not a 1.x source; migrate takes '.dsm', '.dsf' and '.dsh' files."),
				FText::FromString(SourceFilePath)));
		}

		FString RawText;
		if (!FFileHelper::LoadFileToString(RawText, *SourceFilePath))
		{
			return FailWith(OutResult, TEXT("DSH9095"), FText::Format(
				LOCTEXT("SourceUnreadable", "'{0}' could not be read."),
				FText::FromString(SourceFilePath)));
		}

		// Where the result belongs, and where it is written: the two differ under OutputDirectory only. Everything that
		// asks "as which file" -- includes, asset destinations -- asks about the place it belongs.
		const FString NominalOutputPath = bIsHeader ? SourceFilePath : FPaths::ChangeExtension(SourceFilePath, TEXT("dss"));
		const bool bWritesElsewhere = !Options.OutputDirectory.IsEmpty();
		OutResult.OutputFilePath = bWritesElsewhere
			? ::UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Options.OutputDirectory, MakeRootRelativePath(NominalOutputPath, /* bWithRootName */ false)))
			: NominalOutputPath;

		// ------------------------------------------------------------------- the 1.x file, read

		FFrontEndRun LegacyRun;
		const bool bLegacyBuilt = RunFrontEnd(SourceFilePath, RawText, /* bKeepTrivia */ true, /* bRefuseConditionals */ true, LegacyRun);
		AppendDiagnostics(OutResult, LegacyRun.Diagnostics, SourceFilePath);
		if (!bLegacyBuilt || LegacyRun.Diagnostics.HasErrors() || !LegacyRun.Module.IsValid() || !LegacyRun.Bound.IsValid())
		{
			// As it does not build as 1.x, there is nothing to compare a migration with.
			return Fail(OutResult);
		}

		if (bIsHeader && !HasLegacyDeclarations(*LegacyRun.Module))
		{
			return FailWith(OutResult, TEXT("DSH9093"), FText::Format(
				LOCTEXT("NothingToMigrate", "'{0}' has no 1.x declaration left; there is nothing to migrate."),
				FText::FromString(SourceFilePath)));
		}

		if ((!bIsHeader || bWritesElsewhere) && IFileManager::Get().FileExists(*OutResult.OutputFilePath))
		{
			return FailWith(OutResult, TEXT("DSH9099"), FText::Format(
				LOCTEXT("OutputExists", "'{0}' already exists and is not written over; move it away, or migrate into another folder with -Out."),
				FText::FromString(OutResult.OutputFilePath)));
		}

		// ------------------------------------------------------------------------- the rewrite

		Lang::FLangMigrateOptions MigrateOptions;
		DecideProductNames(LegacyRun, SourceFilePath, NominalOutputPath, MigrateOptions);

		const Lang::FLegacyMigrationInfo NoLegacyInfo;
		Lang::FLangDiagnosticSink MigrateSink(SourceFilePath);
		const bool bRewritten = Lang::MigrateDreamShaderLegacyModule(
			*LegacyRun.Module,
			LegacyRun.Legacy.IsValid() ? *LegacyRun.Legacy : NoLegacyInfo,
			*LegacyRun.Bound,
			MigrateOptions,
			MigrateSink);
		// The bound module describes a tree that is no longer there.
		LegacyRun.Bound.Reset();
		AppendDiagnostics(OutResult, MigrateSink, SourceFilePath);
		if (!bRewritten)
		{
			return Fail(OutResult);
		}

		// One line ending, the source's own: opaque bodies are printed as they were read.
		FString Text = Lang::PrintDreamShaderLang(*LegacyRun.Module);
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		Text.ReplaceInline(TEXT("\r"), TEXT("\n"), ESearchCase::CaseSensitive);
		if (RawText.Contains(TEXT("\r\n"), ESearchCase::CaseSensitive))
		{
			Text.ReplaceInline(TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);
		}
		OutResult.MigratedText = Text;

		// -------------------------------------------------------------------------- the proofs

		{
			TArray<FString> Before;
			TArray<FString> After;
			Lang::CollectDreamShaderComments(Lang::FLangSourceText(SourceFilePath, RawText), /* bIncludeDocComments */ true, Before);
			Lang::CollectDreamShaderComments(Lang::FLangSourceText(NominalOutputPath, Text), /* bIncludeDocComments */ true, After);

			TArray<FString> Lost;
			FindLostComments(MoveTemp(Before), MoveTemp(After), LegacyRun.Legacy.Get(), Lost);
			if (Lost.Num() > 0)
			{
				const int32 Shown = FMath::Min(Lost.Num(), 3);
				FString Examples;
				for (int32 Index = 0; Index < Shown; ++Index)
				{
					Examples += FString::Printf(TEXT("%s'%s'"), Index > 0 ? TEXT(", ") : TEXT(""), *Lost[Index].Left(60));
				}
				return FailWith(OutResult, TEXT("DSH9092"), FText::Format(
					LOCTEXT("CommentsLost", "{0} comment(s) of '{1}' would not be in the migrated file ({2}), so nothing was written; this is a fault of the migration, not of the source."),
					FText::AsNumber(Lost.Num()),
					FText::FromString(SourceFilePath),
					FText::FromString(Examples)));
			}
		}

		FFrontEndRun MigratedRun;
		if (!RunFrontEnd(NominalOutputPath, Text, /* bKeepTrivia */ false, /* bRefuseConditionals */ false, MigratedRun) || MigratedRun.Diagnostics.HasErrors())
		{
			// The text goes where it can be opened, and its diagnostics point into it.
			const FString RejectedPath = MakeSavedPath(TEXT("Rejected"), NominalOutputPath);
			const bool bSaved = SaveText(Text, RejectedPath);

			const Lang::FLangDiagnostic* FirstError = MigratedRun.Diagnostics.FirstError();
			Lang::FLangDiagnosticSink Sink(SourceFilePath);
			Sink.Error(TEXT("DSH9097"), Lang::FLangSpan(), FText::Format(
				LOCTEXT("MigratedTextFails", "The migrated text of '{0}' does not build as 2.0 source ({1}), so nothing was written; the text is in '{2}'."),
				FText::FromString(SourceFilePath),
				FText::FromString(FirstError ? Lang::FLangDiagnosticSink::ToWireString(*FirstError) : FString(TEXT("no error was reported"))),
				FText::FromString(bSaved ? RejectedPath : FString(TEXT("(it could not be saved)")))));
			AppendDiagnostics(OutResult, Sink, SourceFilePath);

			for (const Lang::FLangDiagnostic& Diagnostic : MigratedRun.Diagnostics.GetDiagnostics())
			{
				Lang::FLangDiagnostic Copy = Diagnostic;
				if (Copy.FilePath.IsEmpty() || Copy.FilePath.Equals(NominalOutputPath, ESearchCase::IgnoreCase))
				{
					Copy.FilePath = RejectedPath;
				}
				OutResult.Diagnostics.Add(MoveTemp(Copy));
			}
			return Fail(OutResult);
		}

		if (LegacyRun.IR.IsValid() && MigratedRun.IR.IsValid())
		{
			// Where an asset goes is said differently on the two sides (`@root` against `@name`), so it is compared
			// resolved, below, and not as spelled.
			// A verbatim body is written back without its Namespace block's indentation and with its lifted calls respelled.
			IR::FIRCompareOptions CompareOptions;
			// And a literal on a pin that has a `Const*` twin is a Constant node on the 1.x side and the twin on the other.
			CompareOptions.bCompareDestinations = false;
			CompareOptions.bCompareCodeSpacing = false;
			CompareOptions.ConstTwinCatalog = &Compiler::GetDreamShaderBuiltinCatalog();
			// And `.rgb` of a float3 is a mask node in a 1.x product, as 1.x built one, and nothing in a `.dss`.
			CompareOptions.bCompareIdentitySwizzles = false;
			FString Difference;
			if (!IR::AreDreamShaderIRModulesEquivalent(*LegacyRun.IR, *MigratedRun.IR, CompareOptions, Difference))
			{
				Warn(OutResult, TEXT("DSH9098"), FText::Format(
					LOCTEXT("GraphDiffers", "The migrated text of '{0}' does not build the graph the 1.x file builds: {1}"),
					FText::FromString(SourceFilePath),
					FText::FromString(Difference)));
			}
			else
			{
				for (int32 ProductIndex = 0; ProductIndex < LegacyRun.IR->Products.Num(); ++ProductIndex)
				{
					FString LegacyPackage;
					FString MigratedPackage;
					if (ResolvePackage(LegacyRun.IR->Products[ProductIndex], SourceFilePath, LegacyPackage)
						&& ResolvePackage(MigratedRun.IR->Products[ProductIndex], NominalOutputPath, MigratedPackage)
						&& !LegacyPackage.Equals(MigratedPackage, ESearchCase::IgnoreCase))
					{
						Warn(OutResult, TEXT("DSH9098"), FText::Format(
							LOCTEXT("AssetMoves", "'{0}' builds '{1}', and its migrated text would build '{2}': a new asset, with the old one left behind. Give the declaration a '/// @name {1}'."),
							FText::FromString(SourceFilePath),
							FText::FromString(LegacyPackage),
							FText::FromString(MigratedPackage)));
					}
				}
			}
		}

		if (Options.bCheck || Options.bDryRun)
		{
			OutResult.bSucceeded = true;
			return true;
		}

		// --------------------------------------------------------------------------- the write

		if (!bWritesElsewhere && !::UE::DreamShader::IsWritableSourceFilePath(SourceFilePath))
		{
			// Said, not refused: the plugin's author is who runs this on a plugin's sources.
			UE_LOG(LogDreamShader, Display, TEXT("DreamShader migrate: '%s' is under a source root the editor itself never rewrites."), *SourceFilePath);
		}

		// The new text first and under another name: whatever fails after this, the source is still whole.
		const FString TemporaryPath = OutResult.OutputFilePath + TEXT(".migrating");
		if (!SaveText(Text, TemporaryPath))
		{
			return FailWith(OutResult, TEXT("DSH9099"), FText::Format(
				LOCTEXT("OutputUnwritable", "'{0}' could not be written."),
				FText::FromString(OutResult.OutputFilePath)));
		}

		IFileManager& FileManager = IFileManager::Get();
		if (bWritesElsewhere)
		{
			// The source stays where it is, and is its own backup.
			OutResult.BackupFilePath = SourceFilePath;
		}
		else if (Options.bNoBackup)
		{
			if (!bIsHeader && !FileManager.Delete(*SourceFilePath, /* RequireExists */ false, /* EvenReadOnly */ false, /* Quiet */ true))
			{
				FileManager.Delete(*TemporaryPath, false, false, true);
				return FailWith(OutResult, TEXT("DSH9099"), FText::Format(
					LOCTEXT("SourceUndeletable", "'{0}' could not be deleted, so '{1}' was not written: the two would declare the same assets."),
					FText::FromString(SourceFilePath),
					FText::FromString(OutResult.OutputFilePath)));
			}
		}
		else
		{
			const FString BackupPath = MakeFreeFilePath(MakeSavedPath(TEXT("Sources"), SourceFilePath));
			if (!FileManager.Move(*BackupPath, *SourceFilePath, /* Replace */ false, /* EvenIfReadOnly */ false))
			{
				FileManager.Delete(*TemporaryPath, false, false, true);
				return FailWith(OutResult, TEXT("DSH9099"), FText::Format(
					LOCTEXT("SourceUnmovable", "'{0}' could not be moved to '{1}', so '{2}' was not written: the two would declare the same assets."),
					FText::FromString(SourceFilePath),
					FText::FromString(BackupPath),
					FText::FromString(OutResult.OutputFilePath)));
			}
			OutResult.BackupFilePath = BackupPath;
		}

		if (!FileManager.Move(*OutResult.OutputFilePath, *TemporaryPath, /* Replace */ true, /* EvenIfReadOnly */ false))
		{
			// The one failure that leaves the tree without the source: put it back before saying so.
			if (!OutResult.BackupFilePath.IsEmpty() && !bWritesElsewhere)
			{
				FileManager.Move(*SourceFilePath, *OutResult.BackupFilePath, /* Replace */ false, /* EvenIfReadOnly */ false);
				OutResult.BackupFilePath.Reset();
			}
			FileManager.Delete(*TemporaryPath, false, false, true);
			return FailWith(OutResult, TEXT("DSH9099"), FText::Format(
				LOCTEXT("OutputUnmovable", "'{0}' could not be written."),
				FText::FromString(OutResult.OutputFilePath)));
		}

		OutResult.bSucceeded = true;
		return true;
	}

	bool RunDreamShaderMigrateCommandlet(const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& Params)
	{
		using namespace MigratePrivate;

		// Every flag before any file: a `-Check` that is not read is a migration that writes.
		Lang::FLangDiagnosticSink FlagSink;
		FDreamShaderMigrateOptions Options;
		Options.bCheck = HasCommandletFlag(Tokens, Switches, Params, TEXT("Check"), FlagSink);
		Options.bDryRun = HasCommandletFlag(Tokens, Switches, Params, TEXT("DryRun"), FlagSink);
		Options.bNoBackup = HasCommandletFlag(Tokens, Switches, Params, TEXT("NoBackup"), FlagSink);
		const bool bAll = HasCommandletFlag(Tokens, Switches, Params, TEXT("All"), FlagSink);
		if (LogCommandletFlagErrors(FlagSink))
		{
			return false;
		}

		FString OutputDirectory;
		if (TryGetCommandletParam(Tokens, Switches, Params, TEXT("Out"), OutputDirectory)
			|| TryGetCommandletParam(Tokens, Switches, Params, TEXT("Output"), OutputDirectory))
		{
			Options.OutputDirectory = FPaths::ConvertRelativePathToFull(NormalizeCommandletValue(OutputDirectory));
		}

		// The set, and which members of it were asked for by name: only those fail the run by having nothing to migrate.
		TArray<FString> Files;
		TSet<FString> Named;

		FString SourceArgument;
		FString RootName;
		const bool bHasRoot = TryGetCommandletParam(Tokens, Switches, Params, TEXT("Root"), RootName);
		if (TryGetCommandletParam(Tokens, Switches, Params, TEXT("Source"), SourceArgument)
			|| TryGetCommandletParam(Tokens, Switches, Params, TEXT("File"), SourceArgument))
		{
			const FString SourceFilePath = ResolveSourceArgument(SourceArgument);
			Files.Add(SourceFilePath);
			Named.Add(SourceFilePath);

			// A header comes along when nothing outside the set includes it; otherwise it stays 1.x, which a `.dss`
			// includes just as well.
			TSet<FString> Headers;
			TSet<FString> Visited;
			FDreamShaderDependencyGraphService::CollectHeaderDependenciesRecursive(SourceFilePath, Headers, Visited);
			if (Headers.Num() > 0)
			{
				TMap<FString, TSet<FString>> DependentsByHeader;
				FDreamShaderDependencyGraphService::RebuildMaterialDependencyGraph(DependentsByHeader);
				for (const FString& Header : Headers)
				{
					const FString HeaderPath = ::UE::DreamShader::NormalizeSourceFilePath(Header);
					if (Lang::GetLangFileKindFromPath(HeaderPath) != Lang::ELangFileKind::Dsh || IsPackageFile(HeaderPath))
					{
						continue;
					}

					bool bOnlyThisSource = true;
					if (const TSet<FString>* Dependents = DependentsByHeader.Find(Header))
					{
						for (const FString& Dependent : *Dependents)
						{
							bOnlyThisSource = bOnlyThisSource && ::UE::DreamShader::NormalizeSourceFilePath(Dependent).Equals(SourceFilePath, ESearchCase::IgnoreCase);
						}
					}
					if (bOnlyThisSource)
					{
						Files.Add(HeaderPath);
					}
					else
					{
						UE_LOG(LogDreamShader, Display, TEXT("DreamShader migrate: '%s' is included by other sources too and stays as it is."), *HeaderPath);
					}
				}
			}
		}
		else if (bAll || bHasRoot)
		{
			RootName = NormalizeCommandletValue(RootName);
			TArray<FString> ProjectFiles;
			FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(ProjectFiles);
			for (const FString& FilePath : ProjectFiles)
			{
				const ::UE::DreamShader::FDreamShaderSourceRoot* Root = ::UE::DreamShader::FindSourceRootForFile(FilePath);
				const bool bInScope = bHasRoot
					? (Root && (Root->DisplayName.Equals(RootName, ESearchCase::IgnoreCase) || Root->PluginName.Equals(RootName, ESearchCase::IgnoreCase)))
					: (Root && Root->bWritable);
				if (bInScope && IsLegacySourceFile(FilePath))
				{
					Files.Add(FilePath);
				}
			}
		}
		else
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderCommandletUsage());
			return false;
		}

		Files.StableSort([](const FString& Left, const FString& Right)
		{
			return RankOf(Left) < RankOf(Right);
		});

		int32 NumMigrated = 0;
		int32 NumChecked = 0;
		int32 NumSkipped = 0;
		int32 NumFailed = 0;
		for (const FString& FilePath : Files)
		{
			FDreamShaderMigrateResult Result;
			const bool bMigrated = MigrateDreamShaderSource(FilePath, Options, Result) && Result.bSucceeded;

			// A header of the set that is 2.0 already is not a failure; one the user named is.
			const bool bNothingToMigrate = !bMigrated && Result.Diagnostics.ContainsByPredicate([](const Lang::FLangDiagnostic& Diagnostic)
			{
				return Diagnostic.Code.Equals(TEXT("DSH9093"), ESearchCase::CaseSensitive);
			});
			if (bNothingToMigrate && !Named.Contains(FilePath))
			{
				UE_LOG(LogDreamShader, Display, TEXT("DreamShader migrate: '%s' has no 1.x declaration left."), *FilePath);
				++NumSkipped;
				continue;
			}

			LogDiagnostics(Result);
			if (!bMigrated)
			{
				++NumFailed;
				continue;
			}

			// The three shapes dsc.ps1 and the bridge read back; keep them to the letter.
			if (Options.bCheck || Options.bDryRun)
			{
				UE_LOG(LogDreamShader, Display, TEXT("Checked '%s': would write '%s'."), *Result.SourceFilePath, *Result.OutputFilePath);
				++NumChecked;
			}
			else if (Result.BackupFilePath.IsEmpty())
			{
				UE_LOG(LogDreamShader, Display, TEXT("Migrated '%s' to '%s' (no backup)."), *Result.SourceFilePath, *Result.OutputFilePath);
				++NumMigrated;
			}
			else
			{
				UE_LOG(LogDreamShader, Display, TEXT("Migrated '%s' to '%s' (backup '%s')."), *Result.SourceFilePath, *Result.OutputFilePath, *Result.BackupFilePath);
				++NumMigrated;
			}
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader migrate: %d migrated, %d checked, %d skipped, %d failed."), NumMigrated, NumChecked, NumSkipped, NumFailed);
		return NumFailed == 0;
	}
}

#undef LOCTEXT_NAMESPACE
