// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderSourcesView.h"

#include "UI/DreamShaderBrowserStyle.h"
#include "UI/Model/DreamShaderBrowserModel.h"

#include "AssetThumbnail.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		const FName ColumnStatus(TEXT("Status"));
		const FName ColumnName(TEXT("Name"));
		const FName ColumnRoot(TEXT("Root"));
		const FName ColumnState(TEXT("State"));
		const FName ColumnAsset(TEXT("Asset"));

		FName ColumnIdForSort(EDreamShaderBrowserSortColumn Column)
		{
			switch (Column)
			{
			case EDreamShaderBrowserSortColumn::Status: return ColumnState;
			case EDreamShaderBrowserSortColumn::Root: return ColumnRoot;
			case EDreamShaderBrowserSortColumn::Asset: return ColumnAsset;
			default: return ColumnName;
			}
		}

		EDreamShaderBrowserSortColumn SortForColumnId(FName ColumnId)
		{
			if (ColumnId == ColumnState || ColumnId == ColumnStatus) return EDreamShaderBrowserSortColumn::Status;
			if (ColumnId == ColumnRoot) return EDreamShaderBrowserSortColumn::Root;
			if (ColumnId == ColumnAsset) return EDreamShaderBrowserSortColumn::Asset;
			return EDreamShaderBrowserSortColumn::Name;
		}

		FText RootLabel(const FBrowserEntry& Entry)
		{
			if (!Entry.Source.IsSet())
			{
				return Entry.Asset.IsSet() ? FText::FromString(Entry.Asset->MountPoint) : FText::GetEmpty();
			}
			return Entry.Source->RootDisplayName.IsEmpty() ? LOCTEXT("RootProject", "Project") : FText::FromString(Entry.Source->RootDisplayName);
		}

		FText StateLabel(const FBrowserEntry& Entry)
		{
			if (!Entry.Source.IsSet())
			{
				return GetBrowserEntryVisual(Entry).Label;
			}
			if (Entry.Source->IsLibrary())
			{
				return FText::Format(LOCTEXT("FunctionUsedBy", "function · used by {0} material(s)"), FText::AsNumber(Entry.Source->Dependents.Num()));
			}
			return GetBrowserStatusVisual(Entry.Source->Status).Label;
		}

		FText AssetLabel(const FBrowserEntry& Entry)
		{
			const FString ObjectPath = Entry.GetObjectPath();
			if (ObjectPath.IsEmpty())
			{
				return FText::GetEmpty();
			}
			// The package path is enough to place it; the leaf repeats the name column.
			return FText::FromString(FPackageName::GetLongPackagePath(FPackageName::ObjectPathToPackageName(ObjectPath)));
		}

		// A multi-column row: one widget per column id.
		class SSourceRow : public SMultiColumnTableRow<TSharedPtr<FBrowserEntry>>
		{
		public:
			SLATE_BEGIN_ARGS(SSourceRow) {}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FBrowserEntry> InEntry)
			{
				Entry = InEntry;
				SMultiColumnTableRow<TSharedPtr<FBrowserEntry>>::Construct(FSuperRowType::FArguments().Padding(FMargin(0.0f, 2.0f)), OwnerTable);
			}

			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override
			{
				if (!Entry.IsValid())
				{
					return SNullWidget::NullWidget;
				}
				const FBrowserStatusVisual Visual = GetBrowserEntryVisual(*Entry);
				const FText Detail = Entry->Source.IsSet() ? Entry->Source->StatusDetail : Visual.Label;
				const FText NameTip = Entry->Source.IsSet() ? FText::FromString(Entry->Source->FilePath) : FText::FromString(Entry->GetObjectPath());

				if (ColumnId == ColumnStatus)
				{
					return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Visual.Glyph).ColorAndOpacity(FSlateColor(Visual.Color)).ToolTipText(Detail)
					];
				}
				if (ColumnId == ColumnName)
				{
					return SNew(SBox).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Entry->GetDisplayName())).ToolTipText(NameTip)
					];
				}
				if (ColumnId == ColumnRoot)
				{
					return SNew(SBox).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
					[
						SNew(STextBlock).Text(RootLabel(*Entry)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
				}
				if (ColumnId == ColumnState)
				{
					return SNew(SBox).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
					[
						SNew(STextBlock).Text(StateLabel(*Entry)).ColorAndOpacity(FSlateColor::UseSubduedForeground()).ToolTipText(Detail)
					];
				}
				if (ColumnId == ColumnAsset)
				{
					return SNew(SBox).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
					[
						SNew(STextBlock).Text(AssetLabel(*Entry)).ColorAndOpacity(FSlateColor::UseSubduedForeground()).ToolTipText(FText::FromString(Entry->GetObjectPath()))
					];
				}
				return SNullWidget::NullWidget;
			}

		private:
			TSharedPtr<FBrowserEntry> Entry;
		};
	}

	void SDreamShaderSourcesView::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		SharedState = InArgs._SharedState;
		OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
		OnEntryActivatedDelegate = InArgs._OnEntryActivated;
		OnContextMenuOpeningDelegate = InArgs._OnContextMenuOpening;
		check(Model.IsValid() && SharedState.IsValid());

		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderSourcesView::Rebuild);
		FilterChangedHandle = SharedState->OnFilterChanged.AddSP(this, &SDreamShaderSourcesView::Rebuild);

		HeaderRow = SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnStatus)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(28.0f)
				.HAlignHeader(HAlign_Center)
				.SortMode(this, &SDreamShaderSourcesView::GetColumnSortMode, ColumnStatus)
				.OnSort(this, &SDreamShaderSourcesView::OnColumnSort)
			+ SHeaderRow::Column(ColumnName)
				.DefaultLabel(LOCTEXT("ColumnName", "Name"))
				.FillWidth(0.34f)
				.SortMode(this, &SDreamShaderSourcesView::GetColumnSortMode, ColumnName)
				.OnSort(this, &SDreamShaderSourcesView::OnColumnSort)
			+ SHeaderRow::Column(ColumnRoot)
				.DefaultLabel(LOCTEXT("ColumnRoot", "Root"))
				.FillWidth(0.14f)
				.SortMode(this, &SDreamShaderSourcesView::GetColumnSortMode, ColumnRoot)
				.OnSort(this, &SDreamShaderSourcesView::OnColumnSort)
			+ SHeaderRow::Column(ColumnState)
				.DefaultLabel(LOCTEXT("ColumnState", "Status"))
				.FillWidth(0.22f)
				.SortMode(this, &SDreamShaderSourcesView::GetColumnSortMode, ColumnState)
				.OnSort(this, &SDreamShaderSourcesView::OnColumnSort)
			+ SHeaderRow::Column(ColumnAsset)
				.DefaultLabel(LOCTEXT("ColumnAsset", "Asset"))
				.FillWidth(0.30f)
				.SortMode(this, &SDreamShaderSourcesView::GetColumnSortMode, ColumnAsset)
				.OnSort(this, &SDreamShaderSourcesView::OnColumnSort);

		ChildSlot
		[
			SAssignNew(ViewSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)

			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ListView, SListView<TSharedPtr<FBrowserEntry>>)
				.ListItemsSource(&VisibleItems)
				.SelectionMode(ESelectionMode::Multi)
				.HeaderRow(HeaderRow)
				.OnGenerateRow(this, &SDreamShaderSourcesView::OnGenerateListRow)
				.OnSelectionChanged(this, &SDreamShaderSourcesView::OnSelectionChangedInternal)
				.OnMouseButtonDoubleClick(this, &SDreamShaderSourcesView::OnItemDoubleClicked)
				.OnContextMenuOpening(this, &SDreamShaderSourcesView::OnContextMenuOpeningInternal)
			]

			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(TileView, STileView<TSharedPtr<FBrowserEntry>>)
				.ListItemsSource(&VisibleItems)
				.SelectionMode(ESelectionMode::Multi)
				.ItemWidth(112.0f)
				.ItemHeight(132.0f)
				.OnGenerateTile(this, &SDreamShaderSourcesView::OnGenerateTile)
				.OnSelectionChanged(this, &SDreamShaderSourcesView::OnSelectionChangedInternal)
				.OnMouseButtonDoubleClick(this, &SDreamShaderSourcesView::OnItemDoubleClicked)
				.OnContextMenuOpening(this, &SDreamShaderSourcesView::OnContextMenuOpeningInternal)
			]
		];

		Rebuild();
	}

	SDreamShaderSourcesView::~SDreamShaderSourcesView()
	{
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
		if (SharedState.IsValid() && FilterChangedHandle.IsValid())
		{
			SharedState->OnFilterChanged.Remove(FilterChangedHandle);
		}
	}

	void SDreamShaderSourcesView::SetTileView(bool bTiles)
	{
		bTileView = bTiles;
		if (ViewSwitcher.IsValid())
		{
			ViewSwitcher->SetActiveWidgetIndex(bTileView ? 1 : 0);
		}
		// The two views keep separate selections; carry the keys across.
		TArray<FString> Keys = SelectedKeys.Array();
		for (const FString& Key : Keys)
		{
			SelectByKey(Key);
		}
	}

	void SDreamShaderSourcesView::SetSort(EDreamShaderBrowserSortColumn Column, bool bAscending)
	{
		SortColumn = Column;
		bSortAscending = bAscending;
		Rebuild();
	}

	TArray<TSharedPtr<FBrowserEntry>> SDreamShaderSourcesView::GetSelectedEntries() const
	{
		if (bTileView && TileView.IsValid())
		{
			return TileView->GetSelectedItems();
		}
		return ListView.IsValid() ? ListView->GetSelectedItems() : TArray<TSharedPtr<FBrowserEntry>>();
	}

	void SDreamShaderSourcesView::SelectByKey(const FString& Key)
	{
		const TSharedPtr<FBrowserEntry>* Found = VisibleItems.FindByPredicate(
			[&Key](const TSharedPtr<FBrowserEntry>& Entry) { return Entry->Key == Key; });
		if (!Found)
		{
			return;
		}
		if (bTileView && TileView.IsValid())
		{
			TileView->SetSelection(*Found, ESelectInfo::Direct);
			TileView->RequestScrollIntoView(*Found);
		}
		else if (ListView.IsValid())
		{
			ListView->SetSelection(*Found, ESelectInfo::Direct);
			ListView->RequestScrollIntoView(*Found);
		}
	}

	void SDreamShaderSourcesView::Rebuild()
	{
		VisibleItems.Reset();
		for (const TSharedPtr<FBrowserEntry>& Entry : Model->GetEntries())
		{
			if (SharedState->Filter.Matches(*Entry))
			{
				VisibleItems.Add(Entry);
			}
		}
		SortVisibleItems();

		if (ListView.IsValid())
		{
			ListView->RequestListRefresh();
		}
		if (TileView.IsValid())
		{
			TileView->RequestListRefresh();
		}

		// A rescan replaces every entry object; re-select by key so the inspector follows the same
		// files across a refresh instead of going blank.
		TArray<TSharedPtr<FBrowserEntry>> Reselect;
		for (const TSharedPtr<FBrowserEntry>& Entry : VisibleItems)
		{
			if (SelectedKeys.Contains(Entry->Key))
			{
				Reselect.Add(Entry);
			}
		}
		{
			TGuardValue<bool> Suppress(bSuppressSelectionEvents, true);
			if (ListView.IsValid())
			{
				ListView->ClearSelection();
				ListView->SetItemSelection(Reselect, true, ESelectInfo::Direct);
			}
			if (TileView.IsValid())
			{
				TileView->ClearSelection();
				TileView->SetItemSelection(Reselect, true, ESelectInfo::Direct);
			}
		}
		OnSelectionChangedDelegate.ExecuteIfBound(Reselect);
	}

	void SDreamShaderSourcesView::SortVisibleItems()
	{
		const EDreamShaderBrowserSortColumn Column = SortColumn;
		const bool bAscending = bSortAscending;
		VisibleItems.Sort([Column, bAscending](const TSharedPtr<FBrowserEntry>& A, const TSharedPtr<FBrowserEntry>& B)
		{
			int32 Order = 0;
			switch (Column)
			{
			case EDreamShaderBrowserSortColumn::Status:
				// Unmanaged assets sort after every source status.
				Order = (A->Source.IsSet() ? static_cast<int32>(A->Source->Status) : 100)
					- (B->Source.IsSet() ? static_cast<int32>(B->Source->Status) : 100);
				break;
			case EDreamShaderBrowserSortColumn::Root:
				Order = RootLabel(*A).ToString().Compare(RootLabel(*B).ToString(), ESearchCase::IgnoreCase);
				break;
			case EDreamShaderBrowserSortColumn::Asset:
				Order = A->GetObjectPath().Compare(B->GetObjectPath(), ESearchCase::IgnoreCase);
				break;
			default:
				break;
			}
			if (Order == 0)
			{
				Order = A->GetDisplayName().Compare(B->GetDisplayName(), ESearchCase::IgnoreCase);
			}
			return bAscending ? Order < 0 : Order > 0;
		});
	}

	TSharedRef<ITableRow> SDreamShaderSourcesView::OnGenerateListRow(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SSourceRow, OwnerTable, Item);
	}

	TSharedRef<ITableRow> SDreamShaderSourcesView::OnGenerateTile(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const FBrowserStatusVisual Visual = GetBrowserEntryVisual(*Item);

		TSharedRef<SWidget> ThumbWidget = SNullWidget::NullWidget;
		if (Item->Asset.IsSet() && Item->Asset->AssetData.IsValid())
		{
			// From the registry data, so a grid of a thousand unmanaged materials loads none of them;
			// the shared pool renders (and caches) the thumbnail on demand.
			TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(Item->Asset->AssetData, 96, 96, UThumbnailManager::Get().GetSharedThumbnailPool());
			ThumbWidget = Thumbnail->MakeThumbnailWidget();
		}
		else
		{
			ThumbWidget = SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Visual.Glyph).ColorAndOpacity(FSlateColor(Visual.Color)).TextStyle(FAppStyle::Get(), "LargeText")
				];
		}

		return SNew(STableRow<TSharedPtr<FBrowserEntry>>, OwnerTable)
			.Style(FAppStyle::Get(), "ContentBrowser.AssetListView.TileTableRow")
			.Padding(FMargin(4.0f))
			.ToolTipText(Item->Source.IsSet() ? Item->Source->StatusDetail : FText::FromString(Item->GetObjectPath()))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)[ ThumbWidget ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(STextBlock).Text(Visual.Glyph).ColorAndOpacity(FSlateColor(Visual.Color)).TextStyle(FAppStyle::Get(), "SmallText")
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->GetDisplayName()))
						.TextStyle(FAppStyle::Get(), "SmallText")
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
			];
	}

	void SDreamShaderSourcesView::OnSelectionChangedInternal(TSharedPtr<FBrowserEntry>, ESelectInfo::Type)
	{
		if (bSuppressSelectionEvents)
		{
			return;
		}
		const TArray<TSharedPtr<FBrowserEntry>> Selected = GetSelectedEntries();
		SelectedKeys.Reset();
		for (const TSharedPtr<FBrowserEntry>& Entry : Selected)
		{
			SelectedKeys.Add(Entry->Key);
		}
		OnSelectionChangedDelegate.ExecuteIfBound(Selected);
	}

	void SDreamShaderSourcesView::OnItemDoubleClicked(TSharedPtr<FBrowserEntry> Item)
	{
		OnEntryActivatedDelegate.ExecuteIfBound(Item);
	}

	EColumnSortMode::Type SDreamShaderSourcesView::GetColumnSortMode(FName ColumnId) const
	{
		if (ColumnIdForSort(SortColumn) != ColumnId && !(ColumnId == ColumnStatus && SortColumn == EDreamShaderBrowserSortColumn::Status))
		{
			return EColumnSortMode::None;
		}
		return bSortAscending ? EColumnSortMode::Ascending : EColumnSortMode::Descending;
	}

	void SDreamShaderSourcesView::OnColumnSort(EColumnSortPriority::Type, const FName& ColumnId, EColumnSortMode::Type Mode)
	{
		SortColumn = SortForColumnId(ColumnId);
		bSortAscending = Mode != EColumnSortMode::Descending;
		UDreamShaderBrowserUserSettings* Settings = UDreamShaderBrowserUserSettings::Get();
		Settings->SortColumn = SortColumn;
		Settings->bSortAscending = bSortAscending;
		Settings->Save();
		Rebuild();
	}

	TSharedPtr<SWidget> SDreamShaderSourcesView::OnContextMenuOpeningInternal()
	{
		return OnContextMenuOpeningDelegate.IsBound() ? OnContextMenuOpeningDelegate.Execute() : nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
