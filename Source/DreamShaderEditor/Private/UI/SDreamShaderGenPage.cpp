// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderGenPage.h"

#include "UI/DreamShaderBrowserActions.h"
#include "UI/DreamShaderBrowserStyle.h"
#include "UI/SDreamShaderInspector.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	void SDreamShaderGenPage::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		check(Model.IsValid());
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderGenPage::OnModelChanged);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(8.0f, 5.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.ToolTipText(LOCTEXT("RefreshTip", "Rescan the source directory and recompute status."))
						.OnClicked_Lambda([this]() { Model->RefreshAll(); return FReply::Handled(); })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CompileAll", "Compile all"))
						.ToolTipText(LOCTEXT("CompileAllTip", "Force-recompile every .dsm/.dsf source (in memory)."))
						.OnClicked_Lambda([this]() { FDreamShaderBrowserActions::CompileAll(*Model); return FReply::Handled(); })
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SSearchBox)
						.HintText(LOCTEXT("SearchHint", "Search sources"))
						.OnTextChanged_Lambda([this](const FText& NewText) { Filter.SearchText = NewText.ToString(); ApplyFilter(); })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return Filter.bErrorsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Filter.bErrorsOnly = (State == ECheckBoxState::Checked); ApplyFilter(); })
						[
							SNew(STextBlock).Text(LOCTEXT("ErrorsOnly", "Errors only"))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return Filter.bHideLibraries ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Filter.bHideLibraries = (State == ECheckBoxState::Checked); ApplyFilter(); })
						[
							SNew(STextBlock).Text(LOCTEXT("HideFunctions", "Hide functions"))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("SourceCount", "{0} / {1}"), FText::AsNumber(VisibleItems.Num()), FText::AsNumber(Model->GetEntries().Num()));
						})
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FBrowserEntry>>)
					.ListItemsSource(&VisibleItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SDreamShaderGenPage::OnGenerateRow)
					.OnSelectionChanged(this, &SDreamShaderGenPage::OnSelectionChanged)
				]

				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SAssignNew(Inspector, SDreamShaderInspector)
					.Model(Model)
				]
			]
		];

		ApplyFilter();
	}

	SDreamShaderGenPage::~SDreamShaderGenPage()
	{
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
	}

	void SDreamShaderGenPage::OnModelChanged()
	{
		ApplyFilter();
	}

	void SDreamShaderGenPage::ApplyFilter()
	{
		VisibleItems.Reset();
		TSharedPtr<FBrowserEntry> Reselect;
		for (const TSharedPtr<FBrowserEntry>& Entry : Model->GetEntries())
		{
			if (!Filter.Matches(*Entry))
			{
				continue;
			}
			VisibleItems.Add(Entry);
			if (!SelectedKey.IsEmpty() && Entry->Key == SelectedKey)
			{
				Reselect = Entry;
			}
		}

		if (!ListView.IsValid())
		{
			return;
		}
		ListView->RequestListRefresh();

		// A rescan replaces every entry object; re-select by key so the inspector follows the same
		// file across a Refresh instead of going blank.
		if (Reselect.IsValid())
		{
			if (!ListView->IsItemSelected(Reselect))
			{
				ListView->SetSelection(Reselect, ESelectInfo::Direct);
			}
		}
		else if (!SelectedKey.IsEmpty())
		{
			SelectedKey.Reset();
			ListView->ClearSelection();
			if (Inspector.IsValid())
			{
				Inspector->SetEntry(nullptr);
			}
		}
	}

	void SDreamShaderGenPage::OnSelectionChanged(TSharedPtr<FBrowserEntry> Item, ESelectInfo::Type)
	{
		SelectedKey = Item.IsValid() ? Item->Key : FString();
		if (Inspector.IsValid())
		{
			Inspector->SetEntry(Item);
		}
	}

	TSharedRef<ITableRow> SDreamShaderGenPage::OnGenerateRow(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const FBrowserSourceInfo& Source = *Item->Source;
		const FBrowserStatusVisual Visual = GetBrowserStatusVisual(Source.Status);
		FText SubLabel = Source.IsLibrary()
			? FText::Format(LOCTEXT("FunctionUsedBy", "function · used by {0} material(s)"), FText::AsNumber(Source.Dependents.Num()))
			: Visual.Label;
		if (!Source.RootDisplayName.IsEmpty())
		{
			SubLabel = FText::Format(
				LOCTEXT("SubLabelWithRoot", "{0} · {1}"),
				FText::FromString(Source.RootDisplayName),
				SubLabel);
		}

		return SNew(STableRow<TSharedPtr<FBrowserEntry>>, OwnerTable)
			.Padding(FMargin(4.0f, 3.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Visual.Glyph)
					.ColorAndOpacity(FSlateColor(Visual.Color))
					.ToolTipText(Source.StatusDetail)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Source.DisplayName))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.TextStyle(FAppStyle::Get(), "SmallText")
						.Text(SubLabel)
					]
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE
