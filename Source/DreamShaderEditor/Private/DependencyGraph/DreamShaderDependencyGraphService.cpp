#include "DreamShaderDependencyGraphService.h"

#include "DreamShaderModule.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		bool IsPathUnderDirectory(const FString& InPath, const FString& InDirectory)
		{
			return UE::DreamShader::IsPathUnderSourceDirectory(InPath, InDirectory);
		}

		/**
		 * The directory a file's own-directory-relative import is confined to: the longest of every
		 * root's source and Packages directories that contains the file. A `../` chain may walk up to
		 * that boundary and no further. Files outside every root are confined to their own directory.
		 */
		FString GetImportBaseDirectoryForFile(const FString& CurrentFilePath)
		{
			FString BestBaseDirectory;
			auto ConsiderBaseDirectory = [&CurrentFilePath, &BestBaseDirectory](const FString& BaseDirectory)
			{
				if (IsPathUnderDirectory(CurrentFilePath, BaseDirectory)
					&& BaseDirectory.Len() > BestBaseDirectory.Len())
				{
					BestBaseDirectory = BaseDirectory;
				}
			};

			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				ConsiderBaseDirectory(Root.Directory);
				ConsiderBaseDirectory(Root.PackagesDirectory);
			}

			return BestBaseDirectory.IsEmpty() ? FPaths::GetPath(CurrentFilePath) : BestBaseDirectory;
		}

		struct FImportCandidate
		{
			FString Path;
			FString RootDirectory;
		};

		bool TryResolveImportCandidates(const TArray<FImportCandidate>& Candidates, FString& OutResolvedPath)
		{
			for (const FImportCandidate& Candidate : Candidates)
			{
				const FString NormalizedCandidate = UE::DreamShader::NormalizeSourceFilePath(Candidate.Path);
				if (!IsPathUnderDirectory(NormalizedCandidate, Candidate.RootDirectory))
				{
					continue;
				}

				if (IFileManager::Get().FileExists(*NormalizedCandidate))
				{
					OutResolvedPath = NormalizedCandidate;
					return true;
				}
			}

			return false;
		}

		/**
		 * Recognizes the text before a `:` in an import specifier as a source-root qualifier. The
		 * vocabulary is deliberately the one `Root=` already uses, minus the package-only spellings:
		 * `Project`, `Plugin.<Name>`, `Plugins.<Name>`, `Plugin/<Name>`, `Plugins/<Name>`.
		 *
		 * Text that does not match one of those shapes is *not* a qualifier — `C:/Shared/Common.dsh`
		 * has to keep failing the way it always did, not turn into an unknown-root error.
		 */
		bool TryParseRootQualifier(const FString& Text, bool& bOutIsProjectRoot, FString& OutPluginName)
		{
			FString Normalized = Text.TrimStartAndEnd();
			Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

			if (Normalized.Equals(TEXT("Project"), ESearchCase::IgnoreCase))
			{
				bOutIsProjectRoot = true;
				OutPluginName.Reset();
				return true;
			}

			// Longest first: "Plugins." must win over "Plugin" before the shorter spellings are tried.
			static const TCHAR* PluginPrefixes[] = { TEXT("Plugins."), TEXT("Plugins/"), TEXT("Plugin."), TEXT("Plugin/") };
			for (const TCHAR* Prefix : PluginPrefixes)
			{
				if (!Normalized.StartsWith(Prefix, ESearchCase::IgnoreCase))
				{
					continue;
				}

				FString PluginName = Normalized.Mid(FCString::Strlen(Prefix)).TrimStartAndEnd();
				if (PluginName.IsEmpty() || PluginName.Contains(TEXT("/")))
				{
					return false;
				}

				bOutIsProjectRoot = false;
				OutPluginName = MoveTemp(PluginName);
				return true;
			}

			return false;
		}

		const UE::DreamShader::FDreamShaderSourceRoot* FindSourceRootByQualifier(
			const bool bIsProjectRoot,
			const FString& PluginName)
		{
			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				const bool bMatches = bIsProjectRoot
					? Root.bIsProjectRoot
					: Root.PluginName.Equals(PluginName, ESearchCase::IgnoreCase);
				if (bMatches)
				{
					return &Root;
				}
			}

			return nullptr;
		}
	}

	// Recognises an `import "..."` line. Deliberately blind to `#if`.
	//
	// This scan runs over the RAW file. It never goes through the preprocessor, and the callers below
	// never hand it preprocessed text, so an `import` inside a branch that generation would cut is
	// still reported as a dependency. The dependency set is therefore the UNION over all `#if`
	// branches: editing a .dsh that only a dead branch imports still marks its dependents dirty and
	// still triggers a rebuild.
	//
	// That is the intended asymmetry, not an oversight. The two directions of being wrong are not
	// comparable: rebuilding an asset that did not need it costs seconds, while failing to rebuild one
	// that did leaves a generated asset that no longer matches its source and no signal that it does
	// not -- the kind of divergence that is found much later, by hand. So the graph is deliberately
	// over-inclusive and generation is exact, and the two are allowed to disagree.
	//
	// This is also why the two paths must stay separate. Routing this scan through the prepare stage
	// to "share one code path" with LoadPreparedDreamShaderSourceRecursive would make the dependency
	// set follow the live define values, and every source reachable only from a currently-false branch
	// would silently drop out of the graph -- a missed rebuild with nothing at all to notice it by.
	bool FDreamShaderDependencyGraphService::TryExtractImportPathFromLine(const FString& Line, FString& OutPath)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();
		if (TrimmedLine.StartsWith(TEXT("//"))
			|| !TrimmedLine.StartsWith(TEXT("import"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		const int32 ImportKeywordLength = 6;
		if (TrimmedLine.Len() > ImportKeywordLength
			&& !FChar::IsWhitespace(TrimmedLine[ImportKeywordLength]))
		{
			return false;
		}

		TrimmedLine.RightChopInline(ImportKeywordLength, DREAMSHADER_ALLOW_SHRINKING_NO);
		TrimmedLine.TrimStartAndEndInline();
		if (TrimmedLine.Len() < 2 || (TrimmedLine[0] != TCHAR('"') && TrimmedLine[0] != TCHAR('\'')))
		{
			return false;
		}

		const TCHAR Quote = TrimmedLine[0];
		int32 ClosingQuoteIndex = INDEX_NONE;
		bool bEscaped = false;
		for (int32 Index = 1; Index < TrimmedLine.Len(); ++Index)
		{
			const TCHAR Character = TrimmedLine[Index];
			if (bEscaped)
			{
				bEscaped = false;
				continue;
			}
			if (Character == TCHAR('\\'))
			{
				bEscaped = true;
				continue;
			}
			if (Character == Quote)
			{
				ClosingQuoteIndex = Index;
				break;
			}
		}

		if (ClosingQuoteIndex == INDEX_NONE)
		{
			return false;
		}

		FString TrailingText = TrimmedLine.Mid(ClosingQuoteIndex + 1).TrimStartAndEnd();
		if (TrailingText.StartsWith(TEXT(";")))
		{
			TrailingText.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			TrailingText.TrimStartAndEndInline();
		}
		if (!TrailingText.IsEmpty() && !TrailingText.StartsWith(TEXT("//")))
		{
			return false;
		}

		OutPath = TrimmedLine.Mid(1, ClosingQuoteIndex - 1).TrimStartAndEnd();
		return !OutPath.IsEmpty();
	}

	FString FDreamShaderDependencyGraphService::NormalizeImportSpecifier(const FString& ImportSpecifier)
	{
		FString Normalized = ImportSpecifier.TrimStartAndEnd();
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Normalized.StartsWith(TEXT("./")))
		{
			Normalized.RightChopInline(2, DREAMSHADER_ALLOW_SHRINKING_NO);
		}

		if (FPaths::GetExtension(Normalized, true).IsEmpty())
		{
			Normalized += TEXT(".dsh");
		}

		return Normalized;
	}

	bool FDreamShaderDependencyGraphService::ResolveImportPath(
		const FString& CurrentFilePath,
		const FString& ImportSpecifier,
		FString& OutResolvedPath,
		FString* OutError)
	{
		if (OutError != nullptr)
		{
			OutError->Reset();
		}

		// A qualified specifier -- `Plugin.MoonToon:Shared/Common.dsh` -- is the one way to leave the
		// importing file's own root, and it says so at the call site. The `:` is what makes it
		// unambiguous: without it, `Plugin.MoonToon/Shared/Common.dsh` would be indistinguishable from
		// a real folder of that name, and the resolver would be back to guessing by scan order.
		FString RootQualifier;
		FString QualifiedPath;
		bool bIsProjectRoot = false;
		FString PluginName;
		if (ImportSpecifier.Split(TEXT(":"), &RootQualifier, &QualifiedPath)
			&& !QualifiedPath.TrimStartAndEnd().IsEmpty()
			&& TryParseRootQualifier(RootQualifier, bIsProjectRoot, PluginName))
		{
			const UE::DreamShader::FDreamShaderSourceRoot* TargetRoot =
				FindSourceRootByQualifier(bIsProjectRoot, PluginName);
			if (TargetRoot == nullptr)
			{
				if (OutError != nullptr)
				{
					*OutError = FString::Printf(
						TEXT("DreamShader import '%s' referenced from '%s' names source root '%s', which is not a DreamShader source root."),
						*ImportSpecifier,
						*CurrentFilePath,
						*RootQualifier.TrimStartAndEnd());
				}
				return false;
			}

			const FString NormalizedQualifiedImport = NormalizeImportSpecifier(QualifiedPath);
			if (NormalizedQualifiedImport.IsEmpty())
			{
				return false;
			}

			// Rooted by construction, so there is no relative-to-the-importing-file candidate.
			const TArray<FImportCandidate> QualifiedCandidates =
			{
				{ FPaths::Combine(TargetRoot->Directory, NormalizedQualifiedImport), TargetRoot->Directory },
				{ FPaths::Combine(TargetRoot->PackagesDirectory, NormalizedQualifiedImport), TargetRoot->PackagesDirectory }
			};

			return TryResolveImportCandidates(QualifiedCandidates, OutResolvedPath);
		}

		const FString NormalizedImport = NormalizeImportSpecifier(ImportSpecifier);
		if (NormalizedImport.IsEmpty())
		{
			return false;
		}

		// An unqualified import never crosses roots. A plugin's sources resolve against that plugin's
		// own tree, so two plugins shipping the same relative path cannot shadow one another and a
		// disabled plugin cannot silently change what another root's import means. A file that belongs
		// to no root at all -- a test fixture, a commandlet -Source outside the tree -- still resolves
		// against the project root, which is what it did before roots existed.
		const UE::DreamShader::FDreamShaderSourceRoot* OwningRoot =
			UE::DreamShader::FindSourceRootForFile(CurrentFilePath);
		const FString RootDirectory = OwningRoot != nullptr
			? OwningRoot->Directory
			: UE::DreamShader::GetSourceShaderDirectory();
		const FString PackagesDirectory = OwningRoot != nullptr
			? OwningRoot->PackagesDirectory
			: UE::DreamShader::GetPackageShaderDirectory();

		const TArray<FImportCandidate> Candidates =
		{
			{ FPaths::Combine(FPaths::GetPath(CurrentFilePath), NormalizedImport), GetImportBaseDirectoryForFile(CurrentFilePath) },
			{ FPaths::Combine(RootDirectory, NormalizedImport), RootDirectory },
			{ FPaths::Combine(PackagesDirectory, NormalizedImport), PackagesDirectory }
		};

		return TryResolveImportCandidates(Candidates, OutResolvedPath);
	}

	void FDreamShaderDependencyGraphService::CollectHeaderDependenciesRecursive(
		const FString& SourceFilePath,
		TSet<FString>& OutHeaders,
		TSet<FString>& InOutVisitedFiles)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		if (InOutVisitedFiles.Contains(NormalizedPath))
		{
			return;
		}
		InOutVisitedFiles.Add(NormalizedPath);

		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *NormalizedPath))
		{
			return;
		}

		TArray<FString> Lines;
		// Raw file text, straight from disk, with no preprocessing between the read and this scan --
		// intentionally, and load-bearingly so. Preprocessing here would resolve `#if` against the
		// current define set and quietly shrink the dependency graph to whatever is live right now;
		// a header imported only from a false branch would stop being a dependency, and editing it
		// would stop rebuilding anything. See TryExtractImportPathFromLine for the full reasoning.
		SourceText.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			FString ImportPath;
			if (!TryExtractImportPathFromLine(Line, ImportPath))
			{
				continue;
			}

			FString ResolvedImportPath;
			if (!ResolveImportPath(NormalizedPath, ImportPath, ResolvedImportPath))
			{
				continue;
			}

			if (UE::DreamShader::IsDreamShaderHeaderFile(ResolvedImportPath) || UE::DreamShader::IsDreamShaderFunctionFile(ResolvedImportPath))
			{
				OutHeaders.Add(ResolvedImportPath);
			}

			CollectHeaderDependenciesRecursive(ResolvedImportPath, OutHeaders, InOutVisitedFiles);
		}
	}

	void FDreamShaderDependencyGraphService::RebuildMaterialDependencyGraph(
		TMap<FString, TSet<FString>>& OutHeaderDependentsByFile)
	{
		OutHeaderDependentsByFile.Reset();

		TArray<FString> MaterialFiles;
		FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(MaterialFiles);
		for (const FString& MaterialFile : MaterialFiles)
		{
			TSet<FString> Dependencies;
			TSet<FString> VisitedFiles;
			CollectHeaderDependenciesRecursive(MaterialFile, Dependencies, VisitedFiles);
			for (const FString& HeaderFile : Dependencies)
			{
				OutHeaderDependentsByFile.FindOrAdd(HeaderFile).Add(MaterialFile);
			}
		}
	}

	void FDreamShaderDependencyGraphService::SortByDependencyOrder(TArray<FString>& InOutSourceFiles)
	{
		if (InOutSourceFiles.Num() < 2)
		{
			return;
		}

		// Only edges WITHIN the batch matter. A dependency that is not itself being compiled is already
		// whatever it is going to be, and waiting for it would mean waiting forever.
		TSet<FString> Batch;
		Batch.Reserve(InOutSourceFiles.Num());
		for (FString& SourceFile : InOutSourceFiles)
		{
			SourceFile = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			Batch.Add(SourceFile);
		}

		TMap<FString, TArray<FString>> DependenciesInBatch;
		DependenciesInBatch.Reserve(InOutSourceFiles.Num());
		for (const FString& SourceFile : InOutSourceFiles)
		{
			TSet<FString> Dependencies;
			TSet<FString> VisitedFiles;
			CollectHeaderDependenciesRecursive(SourceFile, Dependencies, VisitedFiles);

			TArray<FString>& Edges = DependenciesInBatch.Add(SourceFile);
			for (const FString& Dependency : Dependencies)
			{
				if (Dependency != SourceFile && Batch.Contains(Dependency))
				{
					Edges.Add(Dependency);
				}
			}
		}

		// Depth-first post-order: a file is emitted only once everything it imports has been. Files in a
		// cycle come out in the order the walk reached them -- the import loader rejects the cycle with
		// its own diagnostic anyway, and inventing an order for it here would only hide that.
		TArray<FString> Ordered;
		Ordered.Reserve(InOutSourceFiles.Num());
		TSet<FString> Emitted;
		TSet<FString> InProgress;

		TFunction<void(const FString&)> Visit = [&](const FString& SourceFile)
		{
			if (Emitted.Contains(SourceFile) || InProgress.Contains(SourceFile))
			{
				return;
			}
			InProgress.Add(SourceFile);

			if (const TArray<FString>* Edges = DependenciesInBatch.Find(SourceFile))
			{
				for (const FString& Dependency : *Edges)
				{
					Visit(Dependency);
				}
			}

			InProgress.Remove(SourceFile);
			Emitted.Add(SourceFile);
			Ordered.Add(SourceFile);
		};

		// Seeded from the incoming order, so two files with no dependency between them keep it.
		for (const FString& SourceFile : InOutSourceFiles)
		{
			Visit(SourceFile);
		}

		InOutSourceFiles = MoveTemp(Ordered);
	}

	TSet<FString> FDreamShaderDependencyGraphService::RebuildAndCollectDependentsForImport(
		const FString& ImportFilePath,
		TMap<FString, TSet<FString>>& InOutHeaderDependentsByFile)
	{
		const FString NormalizedImportPath = UE::DreamShader::NormalizeSourceFilePath(ImportFilePath);
		TSet<FString> SourcesToQueue;

		if (const TSet<FString>* ExistingDependents = InOutHeaderDependentsByFile.Find(NormalizedImportPath))
		{
			for (const FString& Dependent : *ExistingDependents)
			{
				SourcesToQueue.Add(Dependent);
			}
		}

		RebuildMaterialDependencyGraph(InOutHeaderDependentsByFile);

		if (const TSet<FString>* RebuiltDependents = InOutHeaderDependentsByFile.Find(NormalizedImportPath))
		{
			for (const FString& Dependent : *RebuiltDependents)
			{
				SourcesToQueue.Add(Dependent);
			}
		}

		return SourcesToQueue;
	}
}
