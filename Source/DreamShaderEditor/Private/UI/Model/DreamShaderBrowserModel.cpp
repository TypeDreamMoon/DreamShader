// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/Model/DreamShaderBrowserModel.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"
#include "UI/DreamShaderGeneratedAssetPath.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
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

	bool FBrowserFilter::MatchesStatus(const FBrowserEntry& Entry) const
	{
		if (!HasStatusFilter())
		{
			return true;
		}
		const EBrowserSourceStatus Status = Entry.Source.IsSet() ? Entry.Source->Status : EBrowserSourceStatus::NotCompiled;
		if (bErrorsOnly && Entry.Source.IsSet()
			&& (Status == EBrowserSourceStatus::Error || Status == EBrowserSourceStatus::Unresolved))
		{
			return true;
		}
		if (bStaleOnly && Status == EBrowserSourceStatus::Stale)
		{
			return true;
		}
		if (bDivergedOnly && Entry.Asset.IsSet() && Entry.Asset->Provenance == EDreamShaderDigestState::Diverged)
		{
			return true;
		}
		if (bInMemoryOnly && Entry.Asset.IsSet() && Entry.Asset->Storage == EBrowserStorage::InMemory)
		{
			return true;
		}
		return false;
	}

	bool FBrowserFilter::Matches(const FBrowserEntry& Entry) const
	{
		if (bHideLibraries && Entry.IsLibrary())
		{
			return false;
		}
		const bool bUnmanagedScope = SourceDirectoryScope == UnmanagedScope();
		if (Entry.IsUnmanaged())
		{
			// Listed under "everything" and under the unmanaged node; never under a source folder.
			if (bHideUnmanaged || (!SourceDirectoryScope.IsEmpty() && !bUnmanagedScope))
			{
				return false;
			}
		}
		else if (bUnmanagedScope)
		{
			return false;
		}
		else if (!SourceDirectoryScope.IsEmpty()
			&& !(Entry.Source.IsSet() && UE::DreamShader::IsPathUnderSourceDirectory(Entry.Source->FilePath, SourceDirectoryScope)))
		{
			return false;
		}
		if (!MatchesStatus(Entry))
		{
			return false;
		}
		if (!SearchText.IsEmpty())
		{
			const auto Has = [this](const FString& Text) { return Text.Contains(SearchText, ESearchCase::IgnoreCase); };
			bool bMatches = Has(Entry.GetDisplayName()) || Has(Entry.GetObjectPath());
			if (!bMatches && Entry.Source.IsSet())
			{
				bMatches = Has(Entry.Source->RootDisplayName)
					|| Has(Entry.Source->FilePath)
					|| Has(Entry.Source->StatusDetail.ToString());
			}
			if (!bMatches)
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

	FDreamShaderBrowserModel::~FDreamShaderBrowserModel()
	{
		UnbindFromEditorEvents();
	}

	void FDreamShaderBrowserModel::BindToEditorEvents()
	{
		if (bBoundToEditorEvents)
		{
			return;
		}
		bBoundToEditorEvents = true;

		// Every compile route ends in the generator, so this one signal covers the watcher, the
		// browser's own buttons, the provenance actions and anything external driving the bridge.
		SourceGeneratedHandle = UE::DreamShader::Editor::OnDreamShaderSourceGenerated().AddSP(
			this, &FDreamShaderBrowserModel::OnSourceGenerated);

		if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
		{
			DiagnosticsChangedHandle = Bridge->OnDiagnosticsChanged().AddSP(this, &FDreamShaderBrowserModel::OnDiagnosticsChanged);
			SourceTreeChangedHandle = Bridge->OnSourceTreeChanged().AddSP(this, &FDreamShaderBrowserModel::OnSourceTreeChanged);
			SourceFileModifiedHandle = Bridge->OnSourceFileModified().AddSP(this, &FDreamShaderBrowserModel::MarkSourceDirty);
		}

		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetsAddedHandle = AssetRegistry->OnAssetsAdded().AddSP(this, &FDreamShaderBrowserModel::OnAssetsAddedOrRemoved);
			AssetsRemovedHandle = AssetRegistry->OnAssetsRemoved().AddSP(this, &FDreamShaderBrowserModel::OnAssetsAddedOrRemoved);
			AssetRenamedHandle = AssetRegistry->OnAssetRenamed().AddSP(this, &FDreamShaderBrowserModel::OnAssetRenamed);
		}
	}

	void FDreamShaderBrowserModel::UnbindFromEditorEvents()
	{
		if (FlushTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
			FlushTickerHandle.Reset();
		}
		if (!bBoundToEditorEvents)
		{
			return;
		}
		bBoundToEditorEvents = false;

		UE::DreamShader::Editor::OnDreamShaderSourceGenerated().Remove(SourceGeneratedHandle);
		if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
		{
			Bridge->OnDiagnosticsChanged().Remove(DiagnosticsChangedHandle);
			Bridge->OnSourceTreeChanged().Remove(SourceTreeChangedHandle);
			Bridge->OnSourceFileModified().Remove(SourceFileModifiedHandle);
		}
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnAssetsAdded().Remove(AssetsAddedHandle);
			AssetRegistry->OnAssetsRemoved().Remove(AssetsRemovedHandle);
			AssetRegistry->OnAssetRenamed().Remove(AssetRenamedHandle);
		}
	}

	void FDreamShaderBrowserModel::OnSourceGenerated(const FString& SourceFilePath, bool /*bSucceeded*/)
	{
		// A file the scan has never seen is a new file: only a rescan can list it.
		if (EntriesBySourcePath.Contains(SourceFilePath))
		{
			MarkSourceDirty(SourceFilePath);
		}
		else if (IFileManager::Get().FileExists(*SourceFilePath))
		{
			bRescanPending = true;
			ScheduleFlush();
		}
	}

	void FDreamShaderBrowserModel::OnDiagnosticsChanged()
	{
		bDiagnosticsPending = true;
		ScheduleFlush();
	}

	void FDreamShaderBrowserModel::OnSourceTreeChanged()
	{
		bRescanPending = true;
		ScheduleFlush();
	}

	void FDreamShaderBrowserModel::OnAssetsAddedOrRemoved(TConstArrayView<FAssetData> Assets)
	{
		for (const FAssetData& AssetData : Assets)
		{
			MarkAssetDirty(AssetData);
		}
	}

	void FDreamShaderBrowserModel::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
	{
		MarkAssetDirty(AssetData);
		if (TSharedPtr<FBrowserEntry> Entry = FindByObjectPath(OldObjectPath))
		{
			MarkSourceDirty(Entry->Key);
		}
	}

	void FDreamShaderBrowserModel::MarkSourceDirty(const FString& SourceFilePath)
	{
		DirtySourcePaths.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
		ScheduleFlush();
	}

	void FDreamShaderBrowserModel::MarkAssetDirty(const FAssetData& AssetData)
	{
		// The registry announces every asset in the project; only materials and functions can be ours,
		// and only one the scan resolved to matters.
		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass
			|| !(AssetClass->IsChildOf(UMaterialInterface::StaticClass()) || AssetClass->IsChildOf(UMaterialFunctionInterface::StaticClass())))
		{
			return;
		}
		if (TSharedPtr<FBrowserEntry> Entry = FindByObjectPath(AssetData.GetObjectPathString()))
		{
			if (Entry->Source.IsSet())
			{
				MarkSourceDirty(Entry->Key);
				return;
			}
		}
		// An unmanaged material appeared or went away: only a rescan adds or drops its row.
		bRescanPending = true;
		ScheduleFlush();
	}

	void FDreamShaderBrowserModel::ScheduleFlush()
	{
		if (FlushTickerHandle.IsValid())
		{
			return;
		}
		// One flush per burst: a save that recompiles a header and its twelve dependents raises a
		// dozen generation notices and a diagnostics commit, and the pages should rebuild once.
		FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(this, &FDreamShaderBrowserModel::Flush), 0.2f);
	}

	bool FDreamShaderBrowserModel::Flush(float /*DeltaTime*/)
	{
		FlushTickerHandle.Reset();

		if (bRescanPending)
		{
			bRescanPending = false;
			bDiagnosticsPending = false;
			DirtySourcePaths.Reset();
			RefreshAll();
			return false;
		}

		bool bAnyChange = false;
		for (const FString& SourcePath : DirtySourcePaths)
		{
			if (const TSharedPtr<FBrowserEntry>* Entry = EntriesBySourcePath.Find(SourcePath))
			{
				RefreshEntryInPlace(**Entry);
				bAnyChange = true;
			}
		}
		DirtySourcePaths.Reset();

		if (bDiagnosticsPending)
		{
			bDiagnosticsPending = false;
			for (const TSharedPtr<FBrowserEntry>& Entry : Entries)
			{
				if (Entry->Source.IsSet())
				{
					// The error overlay sits on top of the computed status, so recompute first or a
					// cleared error would leave the stale "compile error" behind.
					RefreshEntryInPlace(*Entry);
				}
			}
			bAnyChange = true;
		}

		if (bAnyChange)
		{
			OnChanged.Broadcast();
		}
		return false; // one-shot
	}

	void FDreamShaderBrowserModel::RefreshAll()
	{
		// Creating <Plugin>/DShader is the first thing anyone does with plugin sources, and the root
		// list is cached, so without this the folder stays invisible until the next editor start.
		UE::DreamShader::RefreshSourceShaderRoots();

		Entries.Reset();
		EntriesBySourcePath.Reset();
		EntriesByObjectPath.Reset();
		UnmanagedCount = 0;

		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);
		SourceFiles.Sort();

		DependentsByFile.Reset();
		FDreamShaderDependencyGraphService::RebuildMaterialDependencyGraph(DependentsByFile);

		// The graph is keyed by the imported file; the importer's side is its inversion.
		TMap<FString, TArray<FString>> ImportsByFile;
		for (const TPair<FString, TSet<FString>>& Pair : DependentsByFile)
		{
			for (const FString& Dependent : Pair.Value)
			{
				ImportsByFile.FindOrAdd(Dependent).Add(Pair.Key);
			}
		}

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

			if (const TSet<FString>* Dependents = DependentsByFile.Find(Source.FilePath))
			{
				Source.Dependents = Dependents->Array();
				Source.Dependents.Sort();
			}
			if (const TArray<FString>* Imports = ImportsByFile.Find(Source.FilePath))
			{
				Source.Imports = *Imports;
				Source.Imports.Sort();
			}

			Entry->Key = Source.FilePath;
			ComputeSourceStatus(Source);
			OverlayDiagnostics(Source);
			AttachAssetHalf(*Entry);

			Entries.Add(Entry);
			EntriesBySourcePath.Add(Source.FilePath, Entry);
			IndexEntry(Entry);
		}

		ScanUnmanagedAssets();
		OnChanged.Broadcast();
	}

	void FDreamShaderBrowserModel::IndexEntry(const TSharedPtr<FBrowserEntry>& Entry)
	{
		const FString ObjectPath = Entry->GetObjectPath();
		if (!ObjectPath.IsEmpty())
		{
			EntriesByObjectPath.Add(ObjectPath.ToLower(), Entry);
		}
	}

	TArray<FString> FDreamShaderBrowserModel::GetContentRoots()
	{
		TArray<FString> ContentRoots;
		ContentRoots.Add(TEXT("/Game"));
		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			if (!Root.bIsProjectRoot && !Root.PluginName.IsEmpty())
			{
				const FString MountPoint = TEXT("/") + Root.PluginName;
				if (FPackageName::MountPointExists(MountPoint + TEXT("/")))
				{
					ContentRoots.AddUnique(MountPoint);
				}
			}
		}
		return ContentRoots;
	}

	void FDreamShaderBrowserModel::DescribeAssetFromRegistry(const FAssetData& AssetData, FBrowserAssetInfo& OutInfo)
	{
		OutInfo = FBrowserAssetInfo();
		OutInfo.AssetData = AssetData;
		OutInfo.ObjectPath = AssetData.GetObjectPathString();
		OutInfo.Storage = (AssetData.PackageFlags & PKG_NewlyCreated) != 0 ? EBrowserStorage::InMemory : EBrowserStorage::OnDisk;
		OutInfo.Provenance = EDreamShaderDigestState::Foreign;
		OutInfo.bFromRegistryOnly = true;
		const UClass* AssetClass = AssetData.GetClass();
		OutInfo.bIsInstance = AssetClass && AssetClass->IsChildOf(UMaterialInstance::StaticClass());
		FString Remainder;
		FString Mount;
		if (OutInfo.ObjectPath.Mid(1).Split(TEXT("/"), &Mount, &Remainder))
		{
			OutInfo.MountPoint = TEXT("/") + Mount;
		}
	}

	void FDreamShaderBrowserModel::ScanUnmanagedAssets()
	{
		// Every material and instance under the content roots that no scanned source resolves to.
		// Answered from the registry alone: a project can hold thousands of materials, and listing
		// them must not load them. A loaded one is described fully, which costs nothing extra.
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return;
		}
		FARFilter Filter;
		Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		for (const FString& ContentRoot : GetContentRoots())
		{
			Filter.PackagePaths.Add(*ContentRoot);
		}
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		AssetRegistry->GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetObjectPathString() < B.GetObjectPathString(); });

		for (const FAssetData& AssetData : Assets)
		{
			const FString ObjectPath = AssetData.GetObjectPathString();
			if (EntriesByObjectPath.Contains(ObjectPath.ToLower()))
			{
				continue; // a scanned source's asset
			}
			// The thin backend's hidden base is an export of its instance's package, never an asset
			// in its own right; the registry can still list it when the package was saved.
			if (AssetData.AssetName.ToString().StartsWith(TEXT("MB_DreamThinBase"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			TSharedPtr<FBrowserEntry> Entry = MakeShared<FBrowserEntry>();
			Entry->Key = ObjectPath;
			FBrowserAssetInfo& Info = Entry->Asset.Emplace();
			if (UObject* Loaded = AssetData.IsAssetLoaded() ? AssetData.GetAsset() : nullptr)
			{
				DescribeAsset(Loaded, Info);
			}
			else
			{
				DescribeAssetFromRegistry(AssetData, Info);
			}
			Entries.Add(Entry);
			IndexEntry(Entry);
			++UnmanagedCount;
		}
	}

	void FDreamShaderBrowserModel::RefreshStatuses()
	{
		for (const TSharedPtr<FBrowserEntry>& Entry : Entries)
		{
			RefreshEntryInPlace(*Entry);
		}
		OnChanged.Broadcast();
	}

	void FDreamShaderBrowserModel::RefreshEntry(const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid())
		{
			return;
		}
		RefreshEntryInPlace(*Entry);
		OnChanged.Broadcast();
	}

	void FDreamShaderBrowserModel::RefreshEntryInPlace(FBrowserEntry& Entry)
	{
		if (!Entry.Source.IsSet())
		{
			// An unmanaged asset: re-describe from the object once it is loaded, else leave the
			// registry description alone.
			if (Entry.Asset.IsSet())
			{
				if (UObject* Loaded = FindObject<UObject>(nullptr, *Entry.Asset->ObjectPath))
				{
					DescribeAsset(Loaded, *Entry.Asset);
				}
			}
			return;
		}
		ComputeSourceStatus(*Entry.Source);
		OverlayDiagnostics(*Entry.Source);
		AttachAssetHalf(Entry);
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
		const TSharedPtr<FBrowserEntry>* Found = EntriesByObjectPath.Find(ObjectPath.ToLower());
		return Found ? *Found : nullptr;
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
		OutInfo.bOpenInEditor = IsGeneratedAssetOpenInEditor(Asset);
		OutInfo.bIsInstance = Asset->IsA<UMaterialInstance>();
		FString Remainder;
		FString Mount;
		if (OutInfo.ObjectPath.Mid(1).Split(TEXT("/"), &Mount, &Remainder))
		{
			OutInfo.MountPoint = TEXT("/") + Mount;
		}
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

		// A memory-only asset with no hash: a build stamped before memory-only builds carried one.
		// Nothing to compare, so "stale" would be a guess; say what is known.
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
