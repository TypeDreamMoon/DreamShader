// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The four 2.0 commandlet verbs. See DreamShaderCompilerTools.h for the split between this file and
// DreamShaderCompilerPipeline.cpp, which implements the driver the verbs call.
//
// Every verb ends with ONE summary line of its own, and that is not decoration: this project's
// commandlet exits non-zero for reasons that have nothing to do with DreamShader (the DevTest
// project's Angelscript errors fold into the exit code), so a reader who only has the exit code
// cannot tell a DreamShader failure from that. The summary line is the verdict; the exit code is a
// hint. It always ends `RESULT=OK` or `RESULT=FAILED`, so a script can grep for it.
//
// Every failure of the verbs themselves is raised into an FLangDiagnosticSink with its DSHnnnn code
// -- never straight into UE_LOG -- so that .skill/gen-diagnostics.ps1 finds the raise sites by the
// `.Error(TEXT("DSH...` shape it scans for, exactly as it does for the front end's.

#include "Tools/DreamShaderCompilerTools.h"

#include "Tools/DreamShaderCatalogManifest.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilePipeline.h"
#include "Tools/DreamShaderShaderCheck.h"

#include "Commandlet/DreamShaderCommandletRunner.h"
#include "DreamShaderModule.h"
#include "IR/IRDump.h"
#include "IR/IRLayout.h"
#include "Lang/LangFormat.h"
#include "Semantic/LangBound.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "DreamShader.Tools"

namespace UE::DreamShader::Editor::Compiler
{
	using UE::DreamShader::Lang::ELangSeverity;
	using UE::DreamShader::Lang::FLangDiagnostic;
	using UE::DreamShader::Lang::FLangDiagnosticSink;
	using UE::DreamShader::Lang::FLangSpan;

	namespace
	{
		/**
		 * `-Flag` / `-Flag=true` / `-Flag=0`.
		 *
		 * A copy of the commandlet runner's HasCommandletFlag, which is file-static there. Copied
		 * rather than exported because exporting it would be a change to a shared file beyond "add
		 * the verbs", and the alternative -- a verb whose `-Shaders` parsed differently from the
		 * `-Force` next to it -- is worse than fifteen duplicated lines.
		 */
		bool HasFlag(const TArray<FString>& Tokens, const TArray<FString>& Switches, const FString& Name)
		{
			auto Matches = [&Name](const FString& Text, bool& bOutValue) -> bool
			{
				FString Key;
				FString Value;
				const bool bHasValue = Private::TrySplitCommandletAssignment(Text, Key, Value);
				if (!bHasValue)
				{
					Key = Private::NormalizeCommandletKey(Text);
				}

				if (!Key.Equals(Name, ESearchCase::IgnoreCase))
				{
					return false;
				}

				bOutValue = true;
				if (bHasValue)
				{
					const FString Normalized = Value.ToLower();
					if (Normalized == TEXT("0") || Normalized == TEXT("false") || Normalized == TEXT("no") || Normalized == TEXT("off"))
					{
						bOutValue = false;
					}
				}
				return true;
			};

			bool bValue = false;
			for (const FString& Switch : Switches)
			{
				if (Matches(Switch, bValue))
				{
					return bValue;
				}
			}
			for (const FString& Token : Tokens)
			{
				if (Matches(Token, bValue))
				{
					return bValue;
				}
			}
			return false;
		}

		FString GetParam(
			const TArray<FString>& Tokens,
			const TArray<FString>& Switches,
			const TMap<FString, FString>& Params,
			const FString& Name)
		{
			FString Value;
			Private::TryGetCommandletParam(Tokens, Switches, Params, Name, Value);
			return Value;
		}

		/** `-Out=` or `-Output=`, whichever was given. */
		FString GetOutParam(
			const TArray<FString>& Tokens,
			const TArray<FString>& Switches,
			const TMap<FString, FString>& Params)
		{
			FString Value = GetParam(Tokens, Switches, Params, TEXT("Out"));
			if (Value.IsEmpty())
			{
				Value = GetParam(Tokens, Switches, Params, TEXT("Output"));
			}
			return Value;
		}

		/** A relative `-Source` is tried under the project's DShader tree first, then the project. */
		FString ResolveSourcePath(const FString& InPath)
		{
			const FString Path = Private::NormalizeCommandletValue(InPath);
			if (Path.IsEmpty() || !FPaths::IsRelative(Path))
			{
				return UE::DreamShader::NormalizeSourceFilePath(Path);
			}

			const FString UnderSourceDirectory = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), Path));
			if (IFileManager::Get().FileExists(*UnderSourceDirectory))
			{
				return UnderSourceDirectory;
			}

			const FString UnderProject = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(FPaths::ProjectDir(), Path));
			if (IFileManager::Get().FileExists(*UnderProject))
			{
				return UnderProject;
			}

			return UE::DreamShader::NormalizeSourceFilePath(Path);
		}

		FString DefaultOutputDirectory(const TCHAR* Leaf)
		{
			return FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), Leaf));
		}

		/**
		 * `<OutputDirectory>/<root>/<source path relative to that root><Suffix>`.
		 *
		 * The same shape MakeDreamShaderGraphDumpFilePath produces, for the same reason: two roots
		 * may hold a `Materials/M_Foo.dss` each, and a flat output directory would have the second
		 * overwrite the first with nothing to notice it by. The suffix goes after the whole file
		 * name, extension included, so a `.dss` and a `.dsh` of the same stem cannot collide either.
		 */
		FString MakeOutputFilePath(const FString& OutputDirectory, const FString& SourceFilePath, const TCHAR* Suffix)
		{
			FString RootName = TEXT("External");
			FString Relative = FPaths::GetCleanFilename(SourceFilePath);

			if (const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(SourceFilePath))
			{
				RootName = Root->DisplayName.IsEmpty() ? FString(TEXT("Project")) : Root->DisplayName;
				FString Candidate = SourceFilePath;
				// The trailing slash is what MakePathRelativeTo needs to read the base as a
				// DIRECTORY rather than as a file whose parent is the base.
				const FString RootWithSlash = Root->Directory + TEXT("/");
				if (FPaths::MakePathRelativeTo(Candidate, *RootWithSlash))
				{
					Relative = Candidate;
				}
			}

			return FPaths::Combine(OutputDirectory, RootName, Relative + Suffix);
		}

		bool WriteToolFile(const FString& FilePath, const FString& Contents, FString& OutError)
		{
			const FString Directory = FPaths::GetPath(FilePath);
			if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
			{
				OutError = FString::Printf(TEXT("could not create directory '%s'"), *Directory); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				return false;
			}

			if (!FFileHelper::SaveStringToFile(Contents, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = FString::Printf(TEXT("could not write '%s'"), *FilePath); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				return false;
			}

			return true;
		}

		/** Counts what a sink holds, for the summary line. */
		void CountDiagnostics(const FLangDiagnosticSink& Sink, int32& InOutErrors, int32& InOutWarnings)
		{
			for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Error)
				{
					++InOutErrors;
				}
				else if (Diagnostic.Severity == ELangSeverity::Warning)
				{
					++InOutWarnings;
				}
			}
		}

		/** One summary line, always ending RESULT=OK or RESULT=FAILED. */
		void LogSummary(const bool bSucceeded, const FString& Line)
		{
			const FString Full = FString::Printf(TEXT("%s RESULT=%s"), *Line, bSucceeded ? TEXT("OK") : TEXT("FAILED")); /* I18N-EXEMPT: machine-readable verdict line */
			if (bSucceeded)
			{
				UE_LOG(LogDreamShader, Display, TEXT("%s"), *Full);
			}
			else
			{
				// Error, not Display: a failed verb has to colour red in dsc.ps1, which decides by
				// the `LogDreamShader: Error:` prefix and not by the exit code.
				UE_LOG(LogDreamShader, Error, TEXT("%s"), *Full);
			}
		}

		/**
		 * True when the path is a compilable source (`.dss`, `.dsi`, `.dsm`, `.dsf`); raises DSH9035 into the sink otherwise.
		 *
		 * NOT DSH9021: that code is already live and documented in
		 * SourceFiles/DreamShaderAssetRenameSyncService.cpp ("Could not write ... after renaming an
		 * asset it references"), alongside DSH9020. CONTRACT 7 hands DSH9020-9049 to this unit
		 * without noticing the two the rename-sync service had already taken.
		 */
		bool RequireLang2Source(const FString& SourceFilePath, const FText& VerbName, FLangDiagnosticSink& Diagnostics)
		{
			if (IsDreamShaderLang2Source(SourceFilePath))
			{
				return true;
			}

			const FLangSpan NoSpan;
			return Diagnostics.Error(TEXT("DSH9035"), NoSpan, FText::Format(
				LOCTEXT("NotALang2SourceForVerb", "'{0}' is not a compilable DreamShader source (.dss, .dsi, .dsm or .dsf), so '{1}' has nothing to do with it; a .dsh header is checked through a source that includes it."),
				FText::FromString(SourceFilePath),
				VerbName));
		}
	}

	void FindProjectDreamShaderLang2Sources(TArray<FString>& OutSourceFiles)
	{
		OutSourceFiles.Reset();

		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			if (Root.Directory.IsEmpty())
			{
				continue;
			}

			// Every extension IsDreamShaderLang2Source accepts; a header is compiled only through a source that includes it.
			TArray<FString> Found;
			for (const TCHAR* const Pattern : { TEXT("*.dss"), TEXT("*.dsi"), TEXT("*.dsm"), TEXT("*.dsf") })
			{
				IFileManager::Get().FindFilesRecursive(
					Found,
					*Root.Directory,
					Pattern,
					/*Files*/ true,
					/*Directories*/ false,
					/*bClearFileNames*/ false);
			}

			for (const FString& File : Found)
			{
				const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(File);
				// The Packages tree is a third-party drop, compiled only when something imports it --
				// the same exclusion FindProjectDreamShaderSourceFiles makes for the 1.x extensions.
				if (UE::DreamShader::IsPathUnderSourceDirectory(Normalized, Root.PackagesDirectory))
				{
					continue;
				}
				OutSourceFiles.AddUnique(Normalized);
			}
		}

		OutSourceFiles.Sort([](const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
		});
	}

	namespace
	{
		/** `-Source=` / `-File=`, or a bare positional argument after the verb; resolved the way `compile` resolves it. */
		bool TryGetExplicitSourceFile(
			const TArray<FString>& Tokens,
			const TArray<FString>& Switches,
			const TMap<FString, FString>& Params,
			FString& OutSourceFile)
		{
			FString SourceFilePath = GetParam(Tokens, Switches, Params, TEXT("Source"));
			if (SourceFilePath.IsEmpty())
			{
				SourceFilePath = GetParam(Tokens, Switches, Params, TEXT("File"));
			}

			if (!SourceFilePath.IsEmpty())
			{
				OutSourceFile = ResolveSourcePath(SourceFilePath);
				return true;
			}

			// A bare positional argument, so `dsc check DShader/Materials/M_Foo.dss` works. Tokens[0] is
			// the verb itself, so the scan starts at 1.
			for (int32 Index = 1; Index < Tokens.Num(); ++Index)
			{
				FString Key;
				FString Value;
				if (Private::TrySplitCommandletAssignment(Tokens[Index], Key, Value) || Tokens[Index].StartsWith(TEXT("-")))
				{
					continue;
				}

				OutSourceFile = ResolveSourcePath(Tokens[Index]);
				return true;
			}
			return false;
		}
	}

	bool ResolveDreamShaderLang2CommandletSourceFiles(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		TArray<FString>& OutSourceFiles)
	{
		FString Explicit;
		if (TryGetExplicitSourceFile(Tokens, Switches, Params, Explicit))
		{
			OutSourceFiles.Add(MoveTemp(Explicit));
			return true;
		}

		if (!HasFlag(Tokens, Switches, TEXT("All")))
		{
			return false;
		}

		FindProjectDreamShaderLang2Sources(OutSourceFiles);
		return true;
	}

	const TCHAR* GetDreamShaderLang2CommandletUsage()
	{
		return TEXT(
			"  -run=DreamShader check { -Source=\"C:/Project/DShader/File.dss\" | -All } [-Shaders]\n"
			"                         [-Platform=SM6,SM5] [-Quality=High] [-Timeout=120] [-DiagnosticsOut=<file>]\n"
			"  -run=DreamShader dump-ir { -Source=... | -All } [-Out=<dir>] [-Json]\n"
			"  -run=DreamShader index { -Source=... | -All } [-Out=<dir>]\n"
			"  -run=DreamShader export-catalog [-Out=<file>]\n"
			"check runs the 2.0 pipeline to IR validation and writes no asset. -Shaders is the one\n"
			"exception: a shader compile needs a real material and 2.0 has no transient asset, so\n"
			"it builds and saves the products as compile does, then reports HLSL errors as\n"
			"stage: shader.\n"
			"dump-ir, index and export-catalog are language-service and debugging tools.\n"
			"check, dump-ir and index take any compilable source: .dss, .dsi, .dsm or .dsf.");
	}

	// -------------------------------------------------------------------------------------- check

	bool RunDreamShaderCheckCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		TArray<FString> SourceFiles;
		if (!ResolveDreamShaderLang2CommandletSourceFiles(Tokens, Switches, Params, SourceFiles))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderLang2CommandletUsage());
			return false;
		}

		const bool bShaders = HasFlag(Tokens, Switches, TEXT("Shaders"));
		const bool bForce = HasFlag(Tokens, Switches, TEXT("Force"));
		const FString DiagnosticsOut = GetParam(Tokens, Switches, Params, TEXT("DiagnosticsOut"));

		FLangDiagnosticSink ToolSink;
		FDreamShaderShaderCheckOptions ShaderOptions;
		if (bShaders && !ParseDreamShaderShaderCheckOptions(
				GetParam(Tokens, Switches, Params, TEXT("Platform")),
				GetParam(Tokens, Switches, Params, TEXT("Quality")),
				GetParam(Tokens, Switches, Params, TEXT("Timeout")),
				ShaderOptions,
				ToolSink))
		{
			LogLang2Diagnostics(ToolSink, FString());
			LogSummary(false, TEXT("DreamShader check: the -Platform / -Quality selection could not be resolved."));
			return false;
		}

		if (SourceFiles.IsEmpty())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader check found no compilable source files (.dss, .dsi, .dsm, .dsf)."));
			LogSummary(true, TEXT("DreamShader check: 0 source(s), 0 error(s), 0 warning(s)."));
			return true;
		}

		if (bShaders)
		{
			// Said once, up front, because it is the one thing about this verb that surprises
			// people: plain `check` writes nothing, but `-Shaders` has to BUILD the assets before
			// their shaders can be compiled, and 2.0 has no transient asset to build them into
			// (plan §5 -- every product saves). Holding the write guard instead was tried and is
			// wrong: it makes the emitter SKIP every product rather than build it off disk, so the
			// run would compile the shaders of whatever happened to be on disk already and report
			// a clean gate on a source it never looked at.
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader check -Shaders builds the assets each source produces before compiling their shaders; 2.0 has no transient asset, so those builds are saved exactly as `compile` saves them."));
		}

		int32 ErrorCount = 0;
		int32 WarningCount = 0;
		int32 ShaderErrorCount = 0;
		int32 CheckedCount = 0;
		bool bAllSucceeded = true;

		for (const FString& SourceFile : SourceFiles)
		{
			FLangDiagnosticSink FileSink(SourceFile);
			if (!RequireLang2Source(SourceFile, LOCTEXT("VerbCheck", "check"), FileSink))
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				CountDiagnostics(FileSink, ErrorCount, WarningCount);
				bAllSucceeded = false;
				continue;
			}

			FDreamShaderLang2PipelineOptions Options;
			// -Shaders needs a real material to compile, so it emits; without it nothing is built
			// at all and the run stops at IR validation, which is what makes plain `check` safe to
			// run against a tree you do not want touched.
			Options.bEmitAssets = bShaders;
			Options.bForce = bForce;

			FDreamShaderLang2PipelineResult Result;
			const bool bPipelineOk = RunDreamShaderLang2Pipeline(SourceFile, Options, Result);
			++CheckedCount;

			if (!bPipelineOk)
			{
				bAllSucceeded = false;
			}
			else if (bShaders)
			{
				FDreamShaderShaderCheckStats Stats;
				if (!CheckDreamShaderShaders(Result, ShaderOptions, Result.Diagnostics, Stats))
				{
					bAllSucceeded = false;
				}
				ShaderErrorCount += Stats.ShaderErrorsReported;
			}

			LogLang2Diagnostics(Result.Diagnostics, SourceFile);
			CountDiagnostics(Result.Diagnostics, ErrorCount, WarningCount);

			if (!DiagnosticsOut.IsEmpty())
			{
				TArray<FLang2DiagnosticRecord> Records;
				BuildLang2DiagnosticRecords(Result.Diagnostics, SourceFile, Records);

				// One file per source when several were checked, so a `-All` run does not have each
				// file overwrite the last.
				const FString OutputRoot = FPaths::ConvertRelativePathToFull(DiagnosticsOut);
				const FString OutputPath = SourceFiles.Num() == 1
					? OutputRoot
					: MakeOutputFilePath(OutputRoot, SourceFile, TEXT(".diagnostics.json"));

				FString WriteError;
				if (!WriteLang2DiagnosticsWireJson(OutputPath, SourceFile, Records, WriteError))
				{
					FLangDiagnosticSink WriteSink(SourceFile);
					const FLangSpan NoSpan;
					WriteSink.Error(TEXT("DSH9036"), NoSpan, FText::Format(
						LOCTEXT("DiagnosticsOutFailed", "The diagnostics JSON could not be written: {0}."),
						FText::FromString(WriteError)));
					LogLang2Diagnostics(WriteSink, SourceFile);
					++ErrorCount;
					bAllSucceeded = false;
				}
			}
		}

		const FString ShaderSuffix = bShaders
			? FString::Printf(TEXT(", %d shader error(s)"), ShaderErrorCount) /* I18N-EXEMPT: machine-readable verdict line */
			: FString();
		LogSummary(bAllSucceeded, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader check: %d source(s), %d error(s), %d warning(s)%s."),
			CheckedCount,
			ErrorCount,
			WarningCount,
			*ShaderSuffix));

		return bAllSucceeded;
	}

	// ------------------------------------------------------------------------------------ dump-ir

	bool RunDreamShaderDumpIRCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		TArray<FString> SourceFiles;
		if (!ResolveDreamShaderLang2CommandletSourceFiles(Tokens, Switches, Params, SourceFiles))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderLang2CommandletUsage());
			return false;
		}

		const FString OutParam = GetOutParam(Tokens, Switches, Params);
		const FString OutputDirectory = OutParam.IsEmpty()
			? DefaultOutputDirectory(TEXT("IR"))
			: FPaths::ConvertRelativePathToFull(OutParam);
		const bool bJson = HasFlag(Tokens, Switches, TEXT("Json"));

		int32 WrittenCount = 0;
		bool bAllSucceeded = true;

		for (const FString& SourceFile : SourceFiles)
		{
			FLangDiagnosticSink FileSink(SourceFile);
			const FLangSpan NoSpan;

			if (!RequireLang2Source(SourceFile, LOCTEXT("VerbDumpIR", "dump-ir"), FileSink))
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
				continue;
			}

			FDreamShaderLang2PipelineOptions Options;
			Options.bEmitAssets = false;

			FDreamShaderLang2PipelineResult Result;
			const bool bPipelineOk = RunDreamShaderLang2Pipeline(SourceFile, Options, Result);
			LogLang2Diagnostics(Result.Diagnostics, SourceFile);

			// Dumped whenever there IS an IR module, whether or not the run succeeded: the partial
			// graph of a file that failed validation is exactly what somebody running dump-ir wants.
			if (!Result.IR.IsValid())
			{
				bAllSucceeded = false;
				continue;
			}
			bAllSucceeded = bAllSucceeded && bPipelineOk;

			auto WriteOne = [&](const TCHAR* Suffix, const FString& Contents) -> bool
			{
				const FString Path = MakeOutputFilePath(OutputDirectory, SourceFile, Suffix);
				FString WriteError;
				if (!WriteToolFile(Path, Contents, WriteError))
				{
					FileSink.Error(TEXT("DSH9022"), NoSpan, FText::Format(
						LOCTEXT("DumpIRWriteFailed", "The IR dump could not be written: {0}."),
						FText::FromString(WriteError)));
					return false;
				}

				++WrittenCount;
				UE_LOG(LogDreamShader, Display, TEXT("Dumped the IR of %s to %s."), *SourceFile, *Path);
				return true;
			};

			bool bWrote = WriteOne(TEXT(".ir.txt"), UE::DreamShader::IR::DumpDreamShaderIRText(*Result.IR));
			if (bWrote && bJson)
			{
				bWrote = WriteOne(TEXT(".ir.json"), UE::DreamShader::IR::DumpDreamShaderIRJson(*Result.IR));
			}

			if (!bWrote)
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
			}
		}

		LogSummary(bAllSucceeded, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader dump-ir: %d file(s) from %d source(s) to %s."),
			WrittenCount,
			SourceFiles.Num(),
			*OutputDirectory));

		return bAllSucceeded;
	}

	// ---------------------------------------------------------------------------------- dump-layout

	bool RunDreamShaderDumpLayoutCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		namespace IR = UE::DreamShader::IR;

		TArray<FString> SourceFiles;
		if (!ResolveDreamShaderLang2CommandletSourceFiles(Tokens, Switches, Params, SourceFiles))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderLang2CommandletUsage());
			return false;
		}

		// Every style unless one is named: the point of the verb is to look at them side by side.
		TArray<IR::EIRLayoutStyle> Styles;
		const FString StyleParam = GetParam(Tokens, Switches, Params, TEXT("Style"));
		if (StyleParam.IsEmpty() || StyleParam.Equals(TEXT("All"), ESearchCase::IgnoreCase))
		{
			Styles.Add(IR::EIRLayoutStyle::Blocks);
			Styles.Add(IR::EIRLayoutStyle::SourceBands);
			Styles.Add(IR::EIRLayoutStyle::Layered);
		}
		else
		{
			IR::EIRLayoutStyle Style = IR::EIRLayoutStyle::Blocks;
			if (!IR::TryParseIRLayoutStyle(StyleParam, Style))
			{
				FLangDiagnosticSink ToolSink;
				const FLangSpan NoSpan;
				ToolSink.Error(TEXT("DSH9041"), NoSpan, FText::Format(
					LOCTEXT("DumpLayoutBadStyle", "'{0}' is not a layout style; -Style takes Blocks, SourceBands, Layered or All."),
					FText::FromString(StyleParam)));
				LogLang2Diagnostics(ToolSink, FString());
				LogSummary(false, TEXT("DreamShader dump-layout: the -Style selection could not be resolved."));
				return false;
			}
			Styles.Add(Style);
		}

		const FString OutParam = GetOutParam(Tokens, Switches, Params);
		const FString OutputDirectory = OutParam.IsEmpty()
			? DefaultOutputDirectory(TEXT("Layout"))
			: FPaths::ConvertRelativePathToFull(OutParam);
		const bool bJson = HasFlag(Tokens, Switches, TEXT("Json"));

		int32 WrittenCount = 0;
		bool bAllSucceeded = true;

		for (const FString& SourceFile : SourceFiles)
		{
			FLangDiagnosticSink FileSink(SourceFile);
			const FLangSpan NoSpan;

			if (!RequireLang2Source(SourceFile, LOCTEXT("VerbDumpLayout", "dump-layout"), FileSink))
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
				continue;
			}

			FDreamShaderLang2PipelineOptions Options;
			Options.bEmitAssets = false;

			FDreamShaderLang2PipelineResult Result;
			const bool bPipelineOk = RunDreamShaderLang2Pipeline(SourceFile, Options, Result);
			LogLang2Diagnostics(Result.Diagnostics, SourceFile);
			if (!Result.IR.IsValid())
			{
				bAllSucceeded = false;
				continue;
			}
			bAllSucceeded = bAllSucceeded && bPipelineOk;

			bool bWroteAll = true;
			for (const IR::FIRProduct& Product : Result.IR->Products)
			{
				// An instance assigns its parent's parameters and has no graph to draw.
				if (Product.Kind == IR::EIRProductKind::MaterialInstance || Product.Graph.Nodes.Num() == 0)
				{
					continue;
				}

				for (const IR::EIRLayoutStyle Style : Styles)
				{
					IR::FIRLayoutOptions LayoutOptions;
					LayoutOptions.Style = Style;
					IR::FIRLayoutResult Layout;
					IR::LayoutDreamShaderIRGraph(Product.Graph, LayoutOptions, Layout);

					// A 1.x product is named with its folder (`Name="MaterialFunctions/MF_X"`), and a slash in a file name is a
					// directory: the leaf is the name here, as it is the asset's.
					const FString ProductLeaf = FPaths::MakeValidFileName(FPaths::GetCleanFilename(Product.Name), TEXT('_'));
					const FString Stem = FString::Printf(TEXT(".%s.%s.layout"), *ProductLeaf, IR::LexToString(Style)); /* I18N-EXEMPT: a file name */
					const FString Title = FString::Printf( /* I18N-EXEMPT: the caption inside a debugging picture */
						TEXT("%s -- %s -- %d nodes, %d columns, %d bands"),
						*Product.Name,
						IR::LexToString(Style),
						Product.Graph.Nodes.Num(),
						Layout.ColumnCount,
						Layout.BandCount);

					auto WriteOne = [&](const FString& Suffix, const FString& Contents) -> bool
					{
						const FString Path = MakeOutputFilePath(OutputDirectory, SourceFile, *Suffix);
						FString WriteError;
						if (!WriteToolFile(Path, Contents, WriteError))
						{
							FileSink.Error(TEXT("DSH9040"), NoSpan, FText::Format(
								LOCTEXT("DumpLayoutWriteFailed", "The layout dump could not be written: {0}."),
								FText::FromString(WriteError)));
							return false;
						}

						++WrittenCount;
						UE_LOG(LogDreamShader, Display, TEXT("Dumped the %s layout of %s (%s) to %s."), IR::LexToString(Style), *Product.Name, *SourceFile, *Path);
						return true;
					};

					bWroteAll = WriteOne(Stem + TEXT(".svg"), IR::DumpDreamShaderIRLayoutSvg(Product.Graph, Layout, Title)) && bWroteAll;
					if (bJson)
					{
						bWroteAll = WriteOne(Stem + TEXT(".json"), IR::DumpDreamShaderIRLayoutJson(Product.Graph, Layout, Style)) && bWroteAll;
					}
				}
			}

			if (!bWroteAll)
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
			}
		}

		LogSummary(bAllSucceeded, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader dump-layout: %d file(s) from %d source(s) to %s."),
			WrittenCount,
			SourceFiles.Num(),
			*OutputDirectory));

		return bAllSucceeded;
	}

	// ---------------------------------------------------------------------------------------- index

	bool RunDreamShaderIndexCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		TArray<FString> SourceFiles;
		if (!ResolveDreamShaderLang2CommandletSourceFiles(Tokens, Switches, Params, SourceFiles))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderLang2CommandletUsage());
			return false;
		}

		const FString OutParam = GetOutParam(Tokens, Switches, Params);
		const FString OutputDirectory = OutParam.IsEmpty()
			? DefaultOutputDirectory(TEXT("Index"))
			: FPaths::ConvertRelativePathToFull(OutParam);

		int32 WrittenCount = 0;
		bool bAllSucceeded = true;

		for (const FString& SourceFile : SourceFiles)
		{
			FLangDiagnosticSink FileSink(SourceFile);
			const FLangSpan NoSpan;

			if (!RequireLang2Source(SourceFile, LOCTEXT("VerbIndex", "index"), FileSink))
			{
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
				continue;
			}

			FDreamShaderLang2PipelineOptions Options;
			Options.bEmitAssets = false;

			FDreamShaderLang2PipelineResult Result;
			RunDreamShaderLang2Pipeline(SourceFile, Options, Result);

			// Indexed even when the compile failed. An editor wants go-to-definition on a broken file
			// most of all, and BuildDreamShaderSymbolIndexJson is documented to work on a bound
			// module with errors -- so a failed bind is a reason to write the index, not to skip it.
			// The diagnostics are logged either way, at Display for a run that only wanted an index.
			if (!Result.Bound.IsValid())
			{
				LogLang2Diagnostics(Result.Diagnostics, SourceFile);
				bAllSucceeded = false;
				continue;
			}

			// The one rule the compiler service writes by too, so a language service reads one place.
			FString IndexPath;
			FString WriteError;
			if (!WriteDreamShaderSymbolIndex(SourceFile, *Result.Bound, OutputDirectory, IndexPath, WriteError))
			{
				FileSink.Error(TEXT("DSH9023"), NoSpan, FText::Format(
					LOCTEXT("IndexWriteFailed", "The symbol index could not be written: {0}."),
					FText::FromString(WriteError)));
				LogLang2Diagnostics(FileSink, SourceFile);
				bAllSucceeded = false;
				continue;
			}

			++WrittenCount;
			UE_LOG(LogDreamShader, Display, TEXT("Indexed %s to %s."), *SourceFile, *IndexPath);
		}

		LogSummary(bAllSucceeded, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader index: %d index file(s) from %d source(s) to %s."),
			WrittenCount,
			SourceFiles.Num(),
			*OutputDirectory));

		return bAllSucceeded;
	}

	// ------------------------------------------------------------------------------------------ fmt

	namespace
	{
		/** Every 2.0-syntax file under the WRITABLE source roots, minus the `Packages` trees, sorted. */
		void FindProjectDreamShaderFormatSources(TArray<FString>& OutSourceFiles)
		{
			OutSourceFiles.Reset();

			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				// A plugin ships its sources as they are (FDreamShaderSourceRoot::bWritable); naming one of its files is
				// how to format it anyway.
				if (Root.Directory.IsEmpty() || !Root.bWritable)
				{
					continue;
				}

				TArray<FString> Found;
				for (const TCHAR* const Pattern : { TEXT("*.dss"), TEXT("*.dsi"), TEXT("*.dsh") })
				{
					IFileManager::Get().FindFilesRecursive(Found, *Root.Directory, Pattern, /*Files*/ true, /*Directories*/ false, /*bClearFileNames*/ false);
				}

				for (const FString& File : Found)
				{
					const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(File);
					if (UE::DreamShader::IsPathUnderSourceDirectory(Normalized, Root.PackagesDirectory))
					{
						continue;
					}
					OutSourceFiles.AddUnique(Normalized);
				}
			}

			OutSourceFiles.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
			});
		}
	}

	bool RunDreamShaderFormatCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		TArray<FString> SourceFiles;
		FString Explicit;
		if (TryGetExplicitSourceFile(Tokens, Switches, Params, Explicit))
		{
			SourceFiles.Add(MoveTemp(Explicit));
		}
		else if (HasFlag(Tokens, Switches, TEXT("All")))
		{
			FindProjectDreamShaderFormatSources(SourceFiles);
		}
		else
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderLang2CommandletUsage());
			return false;
		}

		const bool bCheck = HasFlag(Tokens, Switches, TEXT("Check"));
		const FString OutParam = GetOutParam(Tokens, Switches, Params);
		const FString OutputDirectory = OutParam.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(OutParam);

		int32 ChangedCount = 0;
		int32 UnchangedCount = 0;
		int32 SkippedCount = 0;
		int32 FailedCount = 0;

		for (const FString& SourceFile : SourceFiles)
		{
			FLangDiagnosticSink FileSink(SourceFile);
			const FLangSpan NoSpan;

			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *SourceFile))
			{
				FileSink.Error(TEXT("DSH9045"), NoSpan, FText::Format(
					LOCTEXT("FormatReadFailed", "'{0}' could not be read, so it was not formatted."),
					FText::FromString(SourceFile)));
				LogLang2Diagnostics(FileSink, SourceFile);
				++FailedCount;
				continue;
			}

			FString Formatted;
			const UE::DreamShader::Lang::ELangFormatOutcome Outcome = UE::DreamShader::Lang::FormatDreamShaderLangSource(
				UE::DreamShader::Lang::FLangSourceText(SourceFile, Text),
				UE::DreamShader::Lang::FLangFormatOptions(),
				Formatted,
				FileSink);

			switch (Outcome)
			{
			case UE::DreamShader::Lang::ELangFormatOutcome::Unchanged:
				++UnchangedCount;
				break;

			case UE::DreamShader::Lang::ELangFormatOutcome::Skipped:
				++SkippedCount;
				break;

			case UE::DreamShader::Lang::ELangFormatOutcome::Failed:
				++FailedCount;
				break;

			case UE::DreamShader::Lang::ELangFormatOutcome::Changed:
			{
				++ChangedCount;
				if (bCheck)
				{
					FileSink.Error(TEXT("DSH9046"), NoSpan, FText::Format(
						LOCTEXT("FormatCheckWouldChange", "'{0}' is not in the formatter's layout; 'dsc fmt' would rewrite it."),
						FText::FromString(SourceFile)));
					break;
				}

				const FString TargetPath = OutputDirectory.IsEmpty() ? SourceFile : MakeOutputFilePath(OutputDirectory, SourceFile, TEXT(""));
				FString WriteError;
				if (!WriteToolFile(TargetPath, Formatted, WriteError))
				{
					FileSink.Error(TEXT("DSH9045"), NoSpan, FText::Format(
						LOCTEXT("FormatWriteFailed", "The formatted text of '{0}' could not be written: {1}. A file that is read-only -- checked in, not checked out -- is the usual reason."),
						FText::FromString(SourceFile),
						FText::FromString(WriteError)));
					--ChangedCount;
					++FailedCount;
					break;
				}
				UE_LOG(LogDreamShader, Display, TEXT("Formatted %s%s."), *SourceFile, OutputDirectory.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" to %s"), *TargetPath));
				break;
			}
			}

			LogLang2Diagnostics(FileSink, SourceFile);
		}

		const bool bSucceeded = FailedCount == 0 && (!bCheck || ChangedCount == 0);
		LogSummary(bSucceeded, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader fmt: %d %s, %d already formatted, %d left alone, %d failed of %d file(s)."),
			ChangedCount,
			bCheck ? TEXT("would be rewritten") : TEXT("rewritten"),
			UnchangedCount,
			SkippedCount,
			FailedCount,
			SourceFiles.Num()));
		return bSucceeded;
	}

	// ----------------------------------------------------------------------------- export-catalog

	bool RunDreamShaderExportCatalogCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		FLangDiagnosticSink ToolSink;
		const FLangSpan NoSpan;

		FString WrittenPath;
		FString WriteError;
		if (!ExportDreamShaderBuiltinCatalogManifest(GetOutParam(Tokens, Switches, Params), WrittenPath, WriteError))
		{
			ToolSink.Error(TEXT("DSH9024"), NoSpan, FText::Format(
				LOCTEXT("ExportCatalogWriteFailed", "The builtin catalog manifest could not be written: {0}."),
				FText::FromString(WriteError)));
			LogLang2Diagnostics(ToolSink, FString());
			LogSummary(false, TEXT("DreamShader export-catalog: nothing was written."));
			return false;
		}

		const UE::DreamShader::IR::FBuiltinCatalog& Catalog = GetDreamShaderBuiltinCatalog();
		if (Catalog.IsEmpty())
		{
			// The file was written, and it is empty. Worth failing on: a language service that binds
			// against an empty catalog reports every `UE.*` name in the project as unknown, which
			// reads as a language bug rather than as a missing export.
			ToolSink.Error(TEXT("DSH9025"), NoSpan, FText::Format(
				LOCTEXT("ExportCatalogEmpty", "The builtin catalog came back empty, so '{0}' describes no expression at all. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded."),
				FText::FromString(WrittenPath)));
			LogLang2Diagnostics(ToolSink, FString());
			LogSummary(false, TEXT("DreamShader export-catalog: the catalog is empty."));
			return false;
		}

		LogSummary(true, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
			TEXT("DreamShader export-catalog: %d expression(s), %d material attribute(s) to %s."),
			Catalog.Expressions.Num(),
			Catalog.MaterialAttributes.Num(),
			*WrittenPath));
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
