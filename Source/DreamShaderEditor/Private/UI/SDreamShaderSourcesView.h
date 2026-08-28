// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DreamShaderBrowserState.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STileView.h"

class ITableRow;
class SHeaderRow;
class STableViewBase;
class SWidgetSwitcher;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;

	DECLARE_DELEGATE_OneParam(FOnBrowserEntriesSelected, const TArray<TSharedPtr<FBrowserEntry>>& /*Selected*/);
	DECLARE_DELEGATE_OneParam(FOnBrowserEntryActivated, const TSharedPtr<FBrowserEntry>& /*Entry*/);

	// The Sources mode list: every scanned source that passes the shared filter and scope, as a sortable
	// multi-column list or a thumbnail tile grid, with multi-selection and a context menu the shell
	// supplies. A view over the model; it owns no state beyond the visible subset and the selection.
	class SDreamShaderSourcesView : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderSourcesView) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
			SLATE_ARGUMENT(TSharedPtr<FBrowserSharedState>, SharedState)
			SLATE_EVENT(FOnBrowserEntriesSelected, OnSelectionChanged)
			SLATE_EVENT(FOnBrowserEntryActivated, OnEntryActivated)
			SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderSourcesView() override;

		void SetTileView(bool bTiles);
		bool IsTileView() const { return bTileView; }
		void SetSort(EDreamShaderBrowserSortColumn Column, bool bAscending);

		TArray<TSharedPtr<FBrowserEntry>> GetSelectedEntries() const;
		// Select by key (entries are replaced on every rescan); scrolls it into view.
		void SelectByKey(const FString& Key);
		int32 GetVisibleCount() const { return VisibleItems.Num(); }

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<FBrowserSharedState> SharedState;
		FOnBrowserEntriesSelected OnSelectionChangedDelegate;
		FOnBrowserEntryActivated OnEntryActivatedDelegate;
		FOnContextMenuOpening OnContextMenuOpeningDelegate;
		FDelegateHandle ModelChangedHandle;
		FDelegateHandle FilterChangedHandle;

		TArray<TSharedPtr<FBrowserEntry>> VisibleItems;
		TSharedPtr<SListView<TSharedPtr<FBrowserEntry>>> ListView;
		TSharedPtr<STileView<TSharedPtr<FBrowserEntry>>> TileView;
		TSharedPtr<SWidgetSwitcher> ViewSwitcher;
		TSharedPtr<SHeaderRow> HeaderRow;
		bool bTileView = false;
		EDreamShaderBrowserSortColumn SortColumn = EDreamShaderBrowserSortColumn::Name;
		bool bSortAscending = true;
		TSet<FString> SelectedKeys; // survives a rescan
		bool bSuppressSelectionEvents = false;

		void Rebuild();
		void SortVisibleItems();
		TSharedRef<ITableRow> OnGenerateListRow(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedRef<ITableRow> OnGenerateTile(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void OnSelectionChangedInternal(TSharedPtr<FBrowserEntry> Item, ESelectInfo::Type SelectInfo);
		void OnItemDoubleClicked(TSharedPtr<FBrowserEntry> Item);
		EColumnSortMode::Type GetColumnSortMode(FName ColumnId) const;
		void OnColumnSort(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type Mode);
		TSharedPtr<SWidget> OnContextMenuOpeningInternal();
	};
}
