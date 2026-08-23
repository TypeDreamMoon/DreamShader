// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Material Content Browser's data model: the scan of every DreamShader source under the
// configured roots, each one's compile status, the bridge's diagnostics for it, and the join from a
// project asset back to the source it was generated from. No Slate in here -- the pages and the
// inspector are views over this, and the automation tests drive it directly.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "UI/Model/DreamShaderBrowserEntry.h"

class UObject;

namespace UE::DreamShader::Editor::Private
{
	// What the list shows. Search matches the display name, the root name (a plugin's name is a
	// usable filter for everything it ships), the source and asset paths, and the status detail (so
	// an error message is searchable). The status toggles are OR-ed together: ticking two of them
	// shows entries matching either.
	struct FBrowserFilter
	{
		FString SearchText;
		bool bErrorsOnly = false;
		bool bStaleOnly = false;
		bool bDivergedOnly = false;
		bool bInMemoryOnly = false;
		bool bHideLibraries = false;
		bool bHideUnmanaged = false;
		// Absolute, normalized source directory; only sources under it pass. Empty = everything,
		// unmanaged assets included. UnmanagedScope = only the unmanaged assets.
		FString SourceDirectoryScope;
		static const TCHAR* UnmanagedScope() { return TEXT("#unmanaged"); }

		bool HasStatusFilter() const { return bErrorsOnly || bStaleOnly || bDivergedOnly || bInMemoryOnly; }
		bool Matches(const FBrowserEntry& Entry) const;
		// The status toggles alone, for a view that does its own search and scoping (the asset picker).
		bool MatchesStatus(const FBrowserEntry& Entry) const;
	};

	class FDreamShaderBrowserModel : public TSharedFromThis<FDreamShaderBrowserModel>
	{
	public:
		~FDreamShaderBrowserModel();

		// Follow the editor without a manual Refresh: a generation finishing (any route), the bridge's
		// diagnostics commit, the source watcher, and the asset registry all mark what they touched
		// and the model catches up on the next tick, once, however many of them fired. Only the shell
		// calls this; tests drive the model directly. Unbound on destruction.
		void BindToEditorEvents();
		void UnbindFromEditorEvents();

		// Rescan the source roots, rebuild the dependency graph, recompute every status, overlay the
		// bridge's diagnostics. Broadcasts OnChanged.
		void RefreshAll();

		// Recompute every entry's status and diagnostics without rescanning the tree. Broadcasts
		// OnChanged.
		void RefreshStatuses();

		// Recompute one entry's source status (after a compile) and re-overlay its diagnostics.
		// Broadcasts OnChanged.
		void RefreshEntry(const TSharedPtr<FBrowserEntry>& Entry);

		// Pin a compile failure onto an entry so the inspector can say why, instead of the status
		// falling back to a bare "not compiled". Broadcasts OnChanged.
		void MarkCompileFailed(const TSharedPtr<FBrowserEntry>& Entry, const FString& Message);

		const TArray<TSharedPtr<FBrowserEntry>>& GetEntries() const { return Entries; }
		TSharedPtr<FBrowserEntry> FindBySourcePath(const FString& SourceFilePath) const;
		TSharedPtr<FBrowserEntry> FindByObjectPath(const FString& ObjectPath) const;

		// An asset-centric entry for something picked out of the project: its asset half is filled
		// from the object, and its source half is the scanned entry the asset's DreamShader.SourceFile
		// stamp points at (when there is one). Not added to the scan list.
		TSharedPtr<FBrowserEntry> MakeEntryForAsset(UObject* Asset) const;

		// Fills the asset half from a live object: storage, provenance, registry data.
		static void DescribeAsset(UObject* Asset, FBrowserAssetInfo& OutInfo);
		// Fills the asset half from the registry alone (no load): storage from the package flags,
		// provenance provisional. Used for the unmanaged materials, which may number in the thousands.
		static void DescribeAssetFromRegistry(const FAssetData& AssetData, FBrowserAssetInfo& OutInfo);

		// The content roots the browser covers: /Game and the mount point of every plugin that ships
		// DreamShader sources (where its generated assets land).
		static TArray<FString> GetContentRoots();
		int32 GetUnmanagedCount() const { return UnmanagedCount; }

		FSimpleMulticastDelegate OnChanged;

	private:
		TArray<TSharedPtr<FBrowserEntry>> Entries;
		TMap<FString, TSharedPtr<FBrowserEntry>> EntriesBySourcePath;
		TMap<FString, TSharedPtr<FBrowserEntry>> EntriesByObjectPath; // lower-cased keys
		TMap<FString, TSet<FString>> DependentsByFile;
		int32 UnmanagedCount = 0;

		void ScanUnmanagedAssets();
		void IndexEntry(const TSharedPtr<FBrowserEntry>& Entry);

		// Pending editor-driven work, coalesced into one Flush per tick.
		TSet<FString> DirtySourcePaths;
		bool bRescanPending = false;
		bool bDiagnosticsPending = false;
		FTSTicker::FDelegateHandle FlushTickerHandle;

		bool bBoundToEditorEvents = false;
		FDelegateHandle SourceGeneratedHandle;
		FDelegateHandle DiagnosticsChangedHandle;
		FDelegateHandle SourceTreeChangedHandle;
		FDelegateHandle SourceFileModifiedHandle;
		FDelegateHandle AssetsAddedHandle;
		FDelegateHandle AssetsRemovedHandle;
		FDelegateHandle AssetRenamedHandle;

		void ComputeSourceStatus(FBrowserSourceInfo& Source) const;
		void AttachAssetHalf(FBrowserEntry& Entry) const;
		void OverlayDiagnostics(FBrowserSourceInfo& Source) const;
		void RefreshEntryInPlace(FBrowserEntry& Entry);

		void OnSourceGenerated(const FString& SourceFilePath, bool bSucceeded);
		void OnDiagnosticsChanged();
		void OnSourceTreeChanged();
		void OnAssetsAddedOrRemoved(TConstArrayView<FAssetData> Assets);
		void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
		void MarkSourceDirty(const FString& SourceFilePath);
		void MarkAssetDirty(const FAssetData& AssetData);
		void ScheduleFlush();
		bool Flush(float DeltaTime);
	};
}
