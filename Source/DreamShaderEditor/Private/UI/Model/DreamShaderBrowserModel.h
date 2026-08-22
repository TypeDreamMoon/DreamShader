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
	// What the source list shows. Search matches the display name and the root name -- a plugin's
	// name is a usable filter for everything it ships.
	struct FBrowserFilter
	{
		FString SearchText;
		bool bErrorsOnly = false;
		bool bHideLibraries = false;

		bool Matches(const FBrowserEntry& Entry) const;
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

		FSimpleMulticastDelegate OnChanged;

	private:
		TArray<TSharedPtr<FBrowserEntry>> Entries;
		TMap<FString, TSharedPtr<FBrowserEntry>> EntriesBySourcePath;
		TMap<FString, TSet<FString>> DependentsByFile;

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
