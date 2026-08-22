// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDelegates.h"
#include "CoreMinimal.h"
#include "UI/DreamShaderBrowserState.h"
#include "UI/SDreamShaderSourcesView.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;

	DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnBrowserEntriesContextMenu, const TArray<TSharedPtr<FBrowserEntry>>& /*Entries*/);

	// The Assets mode list: the engine's asset picker over the project's materials, scoped to the
	// navigation tree's content path and filtered by the shared status toggles and search text. Kept
	// as the engine widget on purpose -- thumbnails, drag-to-viewport and the column view come for
	// free -- and driven through the picker's delegate hooks rather than rebuilt.
	class SDreamShaderAssetsView : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderAssetsView) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
			SLATE_ARGUMENT(TSharedPtr<FBrowserSharedState>, SharedState)
			SLATE_EVENT(FOnBrowserEntriesSelected, OnSelectionChanged)
			SLATE_EVENT(FOnBrowserEntryActivated, OnEntryActivated)
			SLATE_EVENT(FOnBrowserEntriesContextMenu, OnGetContextMenu)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderAssetsView() override;

		TArray<TSharedPtr<FBrowserEntry>> GetSelectedEntries() const;
		// Scroll to and select the asset at an object path, if the picker can see it.
		void SyncToObjectPath(const FString& ObjectPath);

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<FBrowserSharedState> SharedState;
		FOnBrowserEntriesSelected OnSelectionChangedDelegate;
		FOnBrowserEntryActivated OnEntryActivatedDelegate;
		FOnBrowserEntriesContextMenu OnGetContextMenuDelegate;
		FDelegateHandle ModelChangedHandle;
		FDelegateHandle FilterChangedHandle;
		FDelegateHandle ScopeChangedHandle;

		FGetCurrentSelectionDelegate GetCurrentSelection;
		FSyncToAssetsDelegate SyncToAssets;
		FSetARFilterDelegate SetFilter;
		FRefreshAssetViewDelegate RefreshAssetView;

		FARFilter MakeScopeFilter() const;
		TArray<TSharedPtr<FBrowserEntry>> EntriesForAssets(const TArray<FAssetData>& Assets) const;
		bool OnShouldFilterAsset(const FAssetData& AssetData) const;
		void OnAssetSelected(const FAssetData& AssetData);
		void OnAssetDoubleClicked(const FAssetData& AssetData);
		TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);
		void OnScopeChanged();
		void OnFilterChanged();
	};
}
