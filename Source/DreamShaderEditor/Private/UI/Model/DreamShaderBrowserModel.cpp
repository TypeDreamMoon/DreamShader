// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/Model/DreamShaderBrowserModel.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"
#include "UI/DreamShaderGeneratedAssetPath.h"

#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	// ---------------------------------------------------------------------------------------------
	// FBrowserEntry

	FString FBrowserEntry::GetDisplayName() const
	{
		if (Source.IsSet())
		{
			return Source->DisplayName;
		}
		if (Asset.IsSet())
		{
			return Asset->AssetData.AssetName.ToString();
		}
		return FString();
	}

	FString FBrowserEntry::GetObjectPath() const
	{
		if (Asset.IsSet() && !Asset->ObjectPath.IsEmpty())
		{
			return Asset->ObjectPath;
		}
		if (Source.IsSet())
		{
			return Source->ResolvedObjectPath;
		}
		return FString();
	}

	UMaterialInterface* FBrowserEntry::ResolveMaterial() const
	{
		if (IsLibrary())
		{
			return nullptr;
		}
		const FString ObjectPath = GetObjectPath();
		if (ObjectPath.IsEmpty())
		{
			return nullptr;
		}
		// Find first: a memory-only material has no package on disk, and LoadObject on it would log
		// a failed load before falling back to the in-memory object.
		if (UMaterialInterface* Found = FindObject<UMaterialInterface>(nullptr, *ObjectPath))
		{
			return Found;
		}
		return LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
	}

	// ---------------------------------------------------------------------------------------------
	// FBrowserFilter

	bool FBrowserFilter::Matches(const FBrowserEntry& Entry) const
	{
		if (bHideLibraries && Entry.IsLibrary())
		{
			return false;
		}
		if (bErrorsOnly)
		{
			const bool bHasError = Entry.Source.IsSet()
				&& (Entry.Source->Status == EBrowserSourceStatus::Error
					|| Entry.Source->Status == EBrowserSourceStatus::Unresolved);
			if (!bHasError)
			{
				return false;
			}
		}
		if (!SearchText.IsEmpty())
		{
			const bool bNameMatches = Entry.GetDisplayName().Contains(SearchText, ESearchCase::IgnoreCase);
			const bool bRootMatches = Entry.Source.IsSet()
				&& Entry.Source->RootDisplayName.Contains(SearchText, ESearchCase::IgnoreCase);
			if (!bNameMatches && !bRootMatches)
			{
				return false;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// FDreamShaderBrowserModel

	namespace
	{
		// The stamp is project-relative so a checkout elsewhere recognizes its own assets; only a
		// source outside the project directory is stored absolute. Mirrors the bridge's resolution.
		FString StampedSourcePathToAbsolute(const FString& StampedPath)
		{
			if (StampedPath.IsEmpty())
			{
				return FString();
			}
			FString AbsolutePath = StampedPath;
			if (FPaths::IsRelative(AbsolutePath))
			{
				AbsolutePath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), StampedPath);
			}
			return UE::DreamShader::NormalizeSourceFilePath(AbsolutePath);
		}
	}

	void FDreamShaderBrowserModel::RefreshAll()
	{
		// Creating <Plugin>/DShader is the first thing anyone does with plugin sources, and the root
		// list is cached, so without this the folder stays invisible until the next editor start.
		UE::DreamShader::RefreshSourceShaderRoots();

		Entries.Reset();
		EntriesBySourcePath.Reset();

		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);
		SourceFiles.Sort();

		DependentsByFile.Reset();
		FDreamShaderDependencyGraphService::RebuildMaterialDependencyGraph(DependentsByFile);

		for (const FString& SourceFile : SourceFiles)
		{
			TSharedPtr<FBrowserEntry> Entry = MakeShared<FBrowserEntry>();
			FBrowserSourceInfo& Source = Entry->Source.Emplace();
			Source.FilePath = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			Source.DisplayName = FPaths::GetCleanFilename(Source.FilePath);

			if (const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(Source.FilePath))
			{
				// Named only for plugin roots, so the project's own list reads as before.
				Source.RootDisplayName = Root->bIsProjectRoot ? FString() : Root->DisplayName;
				Source.bWritableRoot = Root->bWritable;
			}

			if (UE::DreamShader::IsDreamShaderHeaderFile(Source.FilePath))
			{
				Source.Kind = EBrowserSourceKind::Header;
			}
			else if (UE::DreamShader::IsDreamShaderFunctionFile(Source.FilePath))
			{
				Source.Kind = EBrowserSourceKind::Function;
			}
			else
			{
				Source.Kind = EBrowserSourceKind::Material;
			}

			if (Source.IsLibrary())
			{
				if (const TSet<FString>* Dependents = DependentsByFile.Find(Source.FilePath))
				{
					Source.Dependents = Dependents->Array();
					Source.Dependents.Sort();
				}
			}

			Entry->Key = Source.FilePath;
			ComputeSourceStatus(Source);
			OverlayDiagnostics(Source);
			AttachAssetHalf(*Entry);

			Entries.Add(Entry);
			EntriesBySourcePath.Add(Source.FilePath, Entry);
		}

		OnChanged.Broadcast();
	}

	void FDreamShaderBrowserModel::RefreshEntry(const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid() || !Entry->Source.IsSet())
		{
			return;
		}
		ComputeSourceStatus(*Entry->Source);
		OverlayDiagnostics(*Entry->Source);
		AttachAssetHalf(*Entry);
		OnChanged.Broadcast();
	}

	void FDreamShaderBrowserModel::MarkCompileFailed(const TSharedPtr<FBrowserEntry>& Entry, const FString& Message)
	{
		if (!Entry.IsValid() || !Entry->Source.IsSet())
		{
			return;
		}
		Entry->Source->Status = EBrowserSourceStatus::Error;
		Entry->Source->StatusDetail = FText::FromString(Message);
		OnChanged.Broadcast();
	}

	TSharedPtr<FBrowserEntry> FDreamShaderBrowserModel::FindBySourcePath(const FString& SourceFilePath) const
	{
		const TSharedPtr<FBrowserEntry>* Found = EntriesBySourcePath.Find(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
		return Found ? *Found : nullptr;
	}

	TSharedPtr<FBrowserEntry> FDreamShaderBrowserModel::FindByObjectPath(const FString& ObjectPath) const
	{
		for (const TSharedPtr<FBrowserEntry>& Entry : Entries)
		{
			if (Entry->GetObjectPath().Equals(ObjectPath, ESearchCase::IgnoreCase))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	TSharedPtr<FBrowserEntry> FDreamShaderBrowserModel::MakeEntryForAsset(UObject* Asset) const
	{
		if (!Asset)
		{
			return nullptr;
		}

		TSharedPtr<FBrowserEntry> Entry = MakeShared<FBrowserEntry>();
		FBrowserAssetInfo& Info = Entry->Asset.Emplace();
		DescribeAsset(Asset, Info);
		Entry->Key = Info.ObjectPath;

		// Join to the scanned source through the stamp. A scanned entry already carries status and
		// diagnostics, so the asset-centric view gets them for free.
		const FString SourcePath = StampedSourcePathToAbsolute(GetGeneratedAssetSourceFile(Asset));
		if (!SourcePath.IsEmpty())
		{
			if (const TSharedPtr<FBrowserEntry>* Scanned = EntriesBySourcePath.Find(SourcePath))
			{
				Entry->Source = (*Scanned)->Source;
				Entry->Key = SourcePath;
			}
		}
		return Entry;
	}

	void FDreamShaderBrowserModel::DescribeAsset(UObject* Asset, FBrowserAssetInfo& OutInfo)
	{
		OutInfo = FBrowserAssetInfo();
		if (!Asset)
		{
			return;
		}
		OutInfo.AssetData = FAssetData(Asset);
		OutInfo.ObjectPath = Asset->GetPathName();
		const UPackage* Package = Asset->GetPackage();
		OutInfo.Storage = (Package && Package->HasAnyPackageFlags(PKG_NewlyCreated))
			? EBrowserStorage::InMemory
			: EBrowserStorage::OnDisk;
		OutInfo.Provenance = ClassifyGeneratedAsset(Asset);
	}

	void FDreamShaderBrowserModel::ComputeSourceStatus(FBrowserSourceInfo& Source) const
	{
		Source.ResolvedObjectPath.Reset();

		if (Source.IsLibrary())
		{
			Source.Status = EBrowserSourceStatus::Library;
			Source.StatusDetail = LOCTEXT("FunctionDetail", "Function library / header. Recompiles the materials that import it.");
			return;
		}

		FString Error;
		if (!ResolveGeneratedAssetObjectPath(Source.FilePath, Source.ResolvedObjectPath, Error))
		{
			Source.Status = EBrowserSourceStatus::Unresolved;
			Source.StatusDetail = FText::FromString(Error);
			return;
		}

		// A non-loading lookup: a material that exists on disk but has not been loaded this session
		// reads as not compiled until something loads it.
		UObject* Asset = FindObject<UObject>(nullptr, *Source.ResolvedObjectPath);
		if (!Asset)
		{
			Source.Status = EBrowserSourceStatus::NotCompiled;
			Source.StatusDetail = FText::Format(LOCTEXT("GenPageNoGeneratedAsset", "No generated asset at {0}"), FText::FromString(Source.ResolvedObjectPath));
			return;
		}

		FString PreparedText;
		FDreamShaderError LoadError;
		if (!UE::DreamShader::Editor::LoadPreparedDreamShaderSource(Source.FilePath, PreparedText, LoadError))
		{
			Source.Status = EBrowserSourceStatus::Unresolved;
			Source.StatusDetail = FText::FromString(LoadError);
			return;
		}

		Source.StatusDetail = FText::FromString(Source.ResolvedObjectPath);

		const FString SourceHash = BuildSourceHash(PreparedText);
		if (IsGeneratedAssetSourceCurrent(Asset, Source.FilePath, SourceHash))
		{
			Source.Status = EBrowserSourceStatus::UpToDate;
			return;
		}

		// A memory-only build stamps the source path but -- deliberately -- not the hash, so the
		// regeneration skip can never fire on it. Without a hash there is nothing to compare, and
		// reporting that as "stale" would have every Graph-backend material in the editor's default
		// mode wearing an amber dot forever.
		if (!IsGeneratedAssetPersisted(Asset) && GetGeneratedAssetSourceHash(Asset).IsEmpty())
		{
			Source.Status = EBrowserSourceStatus::InMemoryUntracked;
			return;
		}

		Source.Status = EBrowserSourceStatus::Stale;
	}

	void FDreamShaderBrowserModel::AttachAssetHalf(FBrowserEntry& Entry) const
	{
		Entry.Asset.Reset();
		if (!Entry.Source.IsSet() || Entry.Source->ResolvedObjectPath.IsEmpty())
		{
			return;
		}
		if (UObject* Asset = FindObject<UObject>(nullptr, *Entry.Source->ResolvedObjectPath))
		{
			DescribeAsset(Asset, Entry.Asset.Emplace());
		}
	}

	void FDreamShaderBrowserModel::OverlayDiagnostics(FBrowserSourceInfo& Source) const
	{
		Source.Diagnostics.Reset();

		FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge();
		if (!Bridge)
		{
			return;
		}
		const TArray<FDreamShaderDiagnosticRecord>* Diagnostics = Bridge->GetDiagnosticsForSource(Source.FilePath);
		if (!Diagnostics)
		{
			return;
		}
		Source.Diagnostics = *Diagnostics;

		const FDreamShaderDiagnosticRecord* ErrorRecord = Diagnostics->FindByPredicate(
			[](const FDreamShaderDiagnosticRecord& Record)
			{
				return Record.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
			});
		if (!ErrorRecord)
		{
			return;
		}

		// Line/column are identifiers, not quantities: grouping has to be off or a file past a
		// thousand lines reads as "L1,234:1".
		FNumberFormattingOptions LineNumberOptions;
		LineNumberOptions.UseGrouping = false;

		Source.Status = EBrowserSourceStatus::Error;
		Source.StatusDetail = FText::Format(
			LOCTEXT("DiagnosticLineFmt", "L{0}:{1} {2}"),
			FText::AsNumber(ErrorRecord->Line, &LineNumberOptions),
			FText::AsNumber(ErrorRecord->Column, &LineNumberOptions),
			ErrorRecord->Message);
	}
}

#undef LOCTEXT_NAMESPACE
