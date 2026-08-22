// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderBrowserShell.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "Provenance/DreamShaderProvenanceActions.h"
#include "UI/DreamShaderBrowserActions.h"
#include "UI/DreamShaderBrowserCommands.h"
#include "UI/DreamShaderBrowserStyle.h"
#include "UI/DreamShaderBrowserUserSettings.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "UI/SDreamShaderAssetsView.h"
#include "UI/SDreamShaderBrowserNavigation.h"
#include "UI/SDreamShaderInspector.h"
#include "UI/SDreamShaderSourcesView.h"

#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "ISettingsModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		TSharedRef<SWidget> MakeToolbarButton(const TSharedPtr<FUICommandList>& CommandList, const TSharedPtr<FUICommandInfo>& Command, const FName IconName)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(Command->GetDescription())
				.IsEnabled_Lambda([CommandList, Command]() { return CommandList->CanExecuteAction(Command.ToSharedRef()); })
				.OnClicked_Lambda([CommandList, Command]()
				{
					CommandList->ExecuteAction(Command.ToSharedRef());
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush(IconName)).ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Command->GetLabel())
					]
				];
		}

		TSharedRef<SWidget> MakeStatusCount(const FText& Label, TAttribute<int32> Count, const FLinearColor& Color, TFunction<void()> OnClicked)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4.0f, 0.0f))
				.ToolTipText(LOCTEXT("StatusCountTip", "Click to filter the list to these."))
				.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(STextBlock).Text(INVTEXT("●")).ColorAndOpacity(FSlateColor(Color)).TextStyle(FAppStyle::Get(), "SmallText")
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallText")
						.Text_Lambda([Label, Count]() { return FText::Format(LOCTEXT("StatusCountFmt", "{0} {1}"), FText::AsNumber(Count.Get()), Label); })
					]
				];
		}
	}

	void SDreamShaderBrowserShell::Construct(const FArguments& InArgs)
	{
		FDreamShaderBrowserCommands::Register();

		Model = MakeShared<FDreamShaderBrowserModel>();
		SharedState = MakeShared<FBrowserSharedState>();
		CommandList = MakeShared<FUICommandList>();

		// Restore the last session's layout and filters before the panes read them.
		const UDreamShaderBrowserUserSettings* Settings = UDreamShaderBrowserUserSettings::Get();
		SharedState->Scope = FBrowserScope::FromKey(Settings->NavigationScope);
		SharedState->Scope.Mode = Settings->ViewMode;
		SharedState->Filter.bErrorsOnly = Settings->bErrorsOnly;
		SharedState->Filter.bStaleOnly = Settings->bStaleOnly;
		SharedState->Filter.bDivergedOnly = Settings->bDivergedOnly;
		SharedState->Filter.bInMemoryOnly = Settings->bInMemoryOnly;
		SharedState->Filter.bHideLibraries = Settings->bHideLibraries;
		SharedState->Filter.SourceDirectoryScope = SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Sources ? SharedState->Scope.SourceDirectory : FString();

		BindCommands();
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderBrowserShell::OnModelChanged);
		ScopeChangedHandle = SharedState->OnScopeChanged.AddSP(this, &SDreamShaderBrowserShell::OnScopeChanged);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildToolbar()
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.PhysicalSplitterHandleSize(2.0f)
				.OnSplitterFinishedResizing(this, &SDreamShaderBrowserShell::SaveLayout)

				+ SSplitter::Slot()
				.Value(Settings->NavigationPaneFraction)
				[
					SAssignNew(Navigation, SDreamShaderBrowserNavigation)
					.Model(Model)
					.SharedState(SharedState)
				]

				+ SSplitter::Slot()
				.Value(1.0f - Settings->NavigationPaneFraction - Settings->InspectorPaneFraction)
				[
					SAssignNew(ViewSwitcher, SWidgetSwitcher)
					.WidgetIndex(SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Assets ? 1 : 0)

					+ SWidgetSwitcher::Slot()
					[
						SAssignNew(SourcesView, SDreamShaderSourcesView)
						.Model(Model)
						.SharedState(SharedState)
						.OnSelectionChanged(this, &SDreamShaderBrowserShell::OnSelectionChanged)
						.OnEntryActivated(this, &SDreamShaderBrowserShell::OnEntryActivated)
						.OnContextMenuOpening_Lambda([this]() { return MakeContextMenu(Selection); })
					]

					+ SWidgetSwitcher::Slot()
					[
						SAssignNew(AssetsView, SDreamShaderAssetsView)
						.Model(Model)
						.SharedState(SharedState)
						.OnSelectionChanged(this, &SDreamShaderBrowserShell::OnSelectionChanged)
						.OnEntryActivated(this, &SDreamShaderBrowserShell::OnEntryActivated)
						.OnGetContextMenu(this, &SDreamShaderBrowserShell::MakeContextMenu)
					]
				]

				+ SSplitter::Slot()
				.Value(Settings->InspectorPaneFraction)
				[
					SAssignNew(Inspector, SDreamShaderInspector)
					.Model(Model)
					.OnNavigateToSource(this, &SDreamShaderBrowserShell::ShowSource)
					.OnNavigateToAsset(this, &SDreamShaderBrowserShell::ShowAsset)
				]
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildStatusBar()
			]
		];

		SourcesView->SetSort(Settings->SortColumn, Settings->bSortAscending);
		SourcesView->SetTileView(Settings->bTileView);

		// After every pane exists, so each gets the first OnChanged; then follow the editor.
		Model->RefreshAll();
		Model->BindToEditorEvents();
		if (!Settings->LastSelectedKey.IsEmpty())
		{
			SourcesView->SelectByKey(Settings->LastSelectedKey);
		}
	}

	SDreamShaderBrowserShell::~SDreamShaderBrowserShell()
	{
		SaveLayout();
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
		if (SharedState.IsValid() && ScopeChangedHandle.IsValid())
		{
			SharedState->OnScopeChanged.Remove(ScopeChangedHandle);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Layout

	TSharedRef<SWidget> SDreamShaderBrowserShell::BuildToolbar()
	{
		const FDreamShaderBrowserCommands& Commands = FDreamShaderBrowserCommands::Get();
		TWeakPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> WeakBridge;
		if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
		{
			WeakBridge = Bridge->AsWeak();
		}

		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(6.0f, 4.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeToolbarButton(CommandList, Commands.Refresh, "Icons.Refresh")
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.ToolTipText(LOCTEXT("CompileMenuTip", "Compile the selection, every stale source, or everything."))
					.OnGetMenuContent(this, &SDreamShaderBrowserShell::MakeCompileMenu)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Play")).ColorAndOpacity(FSlateColor::UseForeground())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(LOCTEXT("CompileMenu", "Compile"))
						]
					]
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchHintV2", "Search name, path, root, asset, error…"))
					.OnTextChanged_Lambda([this](const FText& NewText)
					{
						SharedState->Filter.SearchText = NewText.ToString();
						SharedState->NotifyFilterChanged();
					})
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.ToolTipText(LOCTEXT("ViewMenuTip", "List or tiles, sorting, and what the Content Browser shows."))
					.OnGetMenuContent(this, &SDreamShaderBrowserShell::MakeViewMenu)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Visibility")).ColorAndOpacity(FSlateColor::UseForeground())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(LOCTEXT("ViewMenu", "View"))
						]
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("OpenWorkspaceTip", "Open the DreamShader source workspace in VSCode."))
					.IsEnabled(WeakBridge.IsValid())
					.OnClicked_Lambda([WeakBridge]()
					{
						if (TSharedPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> Bridge = WeakBridge.Pin()) { Bridge->OpenDreamShaderWorkspace(); }
						return FReply::Handled();
					})
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.OpenInExternalEditor")).ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("OpenSettingsTip", "Open the DreamShader project settings."))
					.OnClicked_Lambda([]()
					{
						if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
						{
							const UDreamShaderSettings* ProjectSettings = GetDefault<UDreamShaderSettings>();
							SettingsModule->ShowViewer(ProjectSettings->GetContainerName(), ProjectSettings->GetCategoryName(), ProjectSettings->GetSectionName());
						}
						return FReply::Handled();
					})
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Settings")).ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
	}

	TSharedRef<SWidget> SDreamShaderBrowserShell::MakeCompileMenu()
	{
		const FDreamShaderBrowserCommands& Commands = FDreamShaderBrowserCommands::Get();
		FMenuBuilder Menu(/*bInShouldCloseWindowAfterMenuSelection*/ true, CommandList);
		Menu.AddMenuEntry(Commands.CompileSelected);
		Menu.AddMenuEntry(Commands.CompileStale);
		Menu.AddMenuEntry(Commands.CompileAll);
		return Menu.MakeWidget();
	}

	TSharedRef<SWidget> SDreamShaderBrowserShell::MakeViewMenu()
	{
		const FDreamShaderBrowserCommands& Commands = FDreamShaderBrowserCommands::Get();
		FMenuBuilder Menu(true, CommandList);

		Menu.BeginSection(TEXT("Layout"), LOCTEXT("ViewSectionLayout", "Sources list"));
		Menu.AddMenuEntry(Commands.ToggleTileView);
		Menu.EndSection();

		Menu.BeginSection(TEXT("Sort"), LOCTEXT("ViewSectionSort", "Sort by"));
		const auto AddSort = [this, &Menu](EDreamShaderBrowserSortColumn Column, const FText& Label)
		{
			Menu.AddMenuEntry(
				Label,
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this, Column]()
					{
						UDreamShaderBrowserUserSettings* Settings = UDreamShaderBrowserUserSettings::Get();
						Settings->SortColumn = Column;
						Settings->Save();
						SourcesView->SetSort(Column, Settings->bSortAscending);
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([Column]() { return UDreamShaderBrowserUserSettings::Get()->SortColumn == Column; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton);
		};
		AddSort(EDreamShaderBrowserSortColumn::Name, LOCTEXT("SortName", "Name"));
		AddSort(EDreamShaderBrowserSortColumn::Status, LOCTEXT("SortStatus", "Status"));
		AddSort(EDreamShaderBrowserSortColumn::Root, LOCTEXT("SortRoot", "Root"));
		AddSort(EDreamShaderBrowserSortColumn::Asset, LOCTEXT("SortAsset", "Asset path"));
		Menu.AddMenuEntry(
			LOCTEXT("SortAscending", "Ascending"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UDreamShaderBrowserUserSettings* Settings = UDreamShaderBrowserUserSettings::Get();
					Settings->bSortAscending = !Settings->bSortAscending;
					Settings->Save();
					SourcesView->SetSort(Settings->SortColumn, Settings->bSortAscending);
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([]() { return UDreamShaderBrowserUserSettings::Get()->bSortAscending; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
		Menu.EndSection();

		Menu.BeginSection(TEXT("Global"), LOCTEXT("ViewSectionGlobal", "Content Browser"));
		Menu.AddMenuEntry(
			LOCTEXT("ShowInMemory", "Show in-memory materials"),
			LOCTEXT("ShowInMemoryTip", "Show DreamShader's memory-only materials here and in the Content Browser (global project setting)."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([]()
				{
					if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
					{
						Bridge->ToggleShowInMemoryMaterialsInContentBrowser();
					}
				}),
				FCanExecuteAction::CreateLambda([]() { return GetDreamShaderEditorBridge() != nullptr; }),
				FIsActionChecked::CreateLambda([]() { return GetDefault<UDreamShaderSettings>()->bShowInMemoryMaterialsInContentBrowser; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
		Menu.EndSection();

		return Menu.MakeWidget();
	}

	TSharedRef<SWidget> SDreamShaderBrowserShell::BuildStatusBar()
	{
		const TSharedPtr<FBrowserSharedState> State = SharedState;
		// A count is a one-click filter: the status alone, or -- clicking it again -- nothing.
		const auto ToggleOnly = [State](bool FBrowserFilter::*Member)
		{
			const bool bWasOn = State->Filter.*Member;
			State->Filter.bErrorsOnly = State->Filter.bStaleOnly = State->Filter.bDivergedOnly = State->Filter.bInMemoryOnly = false;
			State->Filter.*Member = !bWasOn;
			State->NotifyFilterChanged();
		};
		const auto ClearStatusFilters = [State]()
		{
			State->Filter.bErrorsOnly = State->Filter.bStaleOnly = State->Filter.bDivergedOnly = State->Filter.bInMemoryOnly = false;
			State->NotifyFilterChanged();
		};

		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
			.Padding(FMargin(8.0f, 3.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("StatusTotal", "{0} sources"), FText::AsNumber(CountTotal)); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeStatusCount(LOCTEXT("StatusOk", "ok"), TAttribute<int32>::CreateLambda([this]() { return CountOk; }), FLinearColor(0.10f, 0.62f, 0.20f), ClearStatusFilters)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeStatusCount(LOCTEXT("StatusStaleCount", "stale"), TAttribute<int32>::CreateLambda([this]() { return CountStale; }), FLinearColor(0.90f, 0.62f, 0.12f), [ToggleOnly]() { ToggleOnly(&FBrowserFilter::bStaleOnly); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeStatusCount(LOCTEXT("StatusErrorCount", "errors"), TAttribute<int32>::CreateLambda([this]() { return CountErrors; }), BrowserErrorColor, [ToggleOnly]() { ToggleOnly(&FBrowserFilter::bErrorsOnly); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeStatusCount(LOCTEXT("StatusDivergedCount", "edited by hand"), TAttribute<int32>::CreateLambda([this]() { return CountDiverged; }), FLinearColor(0.85f, 0.45f, 0.15f), [ToggleOnly]() { ToggleOnly(&FBrowserFilter::bDivergedOnly); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeStatusCount(LOCTEXT("StatusInMemoryCount", "in memory"), TAttribute<int32>::CreateLambda([this]() { return CountInMemory; }), FLinearColor(0.50f, 0.50f, 0.50f), [ToggleOnly]() { ToggleOnly(&FBrowserFilter::bInMemoryOnly); })
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText_Lambda([]()
					{
						const FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge();
						return Bridge ? FText::FromString(Bridge->GetLastResult()) : FText::GetEmpty();
					})
					.Text_Lambda([]()
					{
						const FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge();
						if (!Bridge)
						{
							return LOCTEXT("BridgeOff", "bridge: off");
						}
						if (Bridge->IsBusy())
						{
							return FText::Format(LOCTEXT("BridgeBusy", "bridge: {0}"), FText::FromString(Bridge->GetBusyAction()));
						}
						return Bridge->IsBridgeOwner() ? LOCTEXT("BridgeIdleOwner", "bridge: idle") : LOCTEXT("BridgeIdleGuest", "bridge: idle (another editor owns writes)");
					})
				]
			];
	}

	TSharedPtr<SWidget> SDreamShaderBrowserShell::MakeContextMenu(const TArray<TSharedPtr<FBrowserEntry>>& Entries)
	{
		Selection = Entries;
		if (Inspector.IsValid() && Entries.Num() > 0)
		{
			Inspector->SetEntry(Entries[0]);
		}

		const FDreamShaderBrowserCommands& Commands = FDreamShaderBrowserCommands::Get();
		FMenuBuilder Menu(true, CommandList);

		Menu.BeginSection(TEXT("Open"), LOCTEXT("MenuSectionOpen", "Open"));
		Menu.AddMenuEntry(Commands.OpenMaterial);
		Menu.AddMenuEntry(Commands.OpenSource);
		Menu.AddMenuEntry(Commands.RevealInContentBrowser);
		Menu.EndSection();

		Menu.BeginSection(TEXT("Build"), LOCTEXT("MenuSectionBuild", "Build"));
		Menu.AddMenuEntry(Commands.CompileSelected);
		Menu.AddMenuEntry(Commands.CreateInstance);
		Menu.AddMenuEntry(Commands.Materialize);
		Menu.EndSection();

		if (HasSelectionGenerated())
		{
			Menu.BeginSection(TEXT("Provenance"), LOCTEXT("MenuSectionProvenance", "Generated asset"));
			Menu.AddMenuEntry(Commands.RevertToSource);
			Menu.AddMenuEntry(Commands.AdoptIntoSource);
			Menu.AddMenuEntry(Commands.DetachFromDreamShader);
			Menu.EndSection();
		}

		Menu.BeginSection(TEXT("Copy"), LOCTEXT("MenuSectionCopy", "Copy"));
		Menu.AddMenuEntry(Commands.CopySourcePath);
		Menu.AddMenuEntry(Commands.CopyAssetPath);
		Menu.EndSection();

		return Menu.MakeWidget();
	}

	// ---------------------------------------------------------------------------------------------
	// State

	void SDreamShaderBrowserShell::OnModelChanged()
	{
		CountTotal = CountOk = CountStale = CountErrors = CountDiverged = CountInMemory = 0;
		for (const TSharedPtr<FBrowserEntry>& Entry : Model->GetEntries())
		{
			++CountTotal;
			if (Entry->Source.IsSet())
			{
				switch (Entry->Source->Status)
				{
				case EBrowserSourceStatus::UpToDate:
				case EBrowserSourceStatus::InMemoryUntracked:
					++CountOk;
					break;
				case EBrowserSourceStatus::Stale:
					++CountStale;
					break;
				case EBrowserSourceStatus::Error:
				case EBrowserSourceStatus::Unresolved:
					++CountErrors;
					break;
				default:
					break;
				}
			}
			if (Entry->Asset.IsSet())
			{
				if (Entry->Asset->Provenance == EDreamShaderDigestState::Diverged) { ++CountDiverged; }
				if (Entry->Asset->Storage == EBrowserStorage::InMemory) { ++CountInMemory; }
			}
		}
	}

	void SDreamShaderBrowserShell::OnScopeChanged()
	{
		if (ViewSwitcher.IsValid())
		{
			ViewSwitcher->SetActiveWidgetIndex(SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Assets ? 1 : 0);
		}
		// The other view's selection is not this view's selection.
		OnSelectionChanged(SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Assets
			? (AssetsView.IsValid() ? AssetsView->GetSelectedEntries() : TArray<TSharedPtr<FBrowserEntry>>())
			: (SourcesView.IsValid() ? SourcesView->GetSelectedEntries() : TArray<TSharedPtr<FBrowserEntry>>()));
		SaveLayout();
	}

	void SDreamShaderBrowserShell::OnSelectionChanged(const TArray<TSharedPtr<FBrowserEntry>>& Entries)
	{
		Selection = Entries;
		if (Inspector.IsValid())
		{
			Inspector->SetEntry(Entries.Num() > 0 ? Entries[0] : nullptr);
		}
		if (Entries.Num() > 0)
		{
			UDreamShaderBrowserUserSettings::Get()->LastSelectedKey = Entries[0]->Key;
		}
	}

	void SDreamShaderBrowserShell::OnEntryActivated(const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid())
		{
			return;
		}
		// Double-click opens the material when there is one, else the source.
		if (Entry->ResolveMaterial())
		{
			FDreamShaderBrowserActions::OpenMaterial(*Entry);
		}
		else
		{
			FDreamShaderBrowserActions::OpenSource(*Entry);
		}
	}

	void SDreamShaderBrowserShell::SaveLayout()
	{
		UDreamShaderBrowserUserSettings* Settings = UDreamShaderBrowserUserSettings::Get();
		Settings->ViewMode = SharedState->Scope.Mode;
		Settings->NavigationScope = SharedState->Scope.ToKey();
		Settings->bErrorsOnly = SharedState->Filter.bErrorsOnly;
		Settings->bStaleOnly = SharedState->Filter.bStaleOnly;
		Settings->bDivergedOnly = SharedState->Filter.bDivergedOnly;
		Settings->bInMemoryOnly = SharedState->Filter.bInMemoryOnly;
		Settings->bHideLibraries = SharedState->Filter.bHideLibraries;
		Settings->bTileView = SourcesView.IsValid() && SourcesView->IsTileView();
		// Splitter fractions: read back off the children's allotted widths.
		if (Navigation.IsValid() && Inspector.IsValid())
		{
			const float Total = GetCachedGeometry().GetLocalSize().X;
			if (Total > 1.0f)
			{
				Settings->NavigationPaneFraction = FMath::Clamp(Navigation->GetCachedGeometry().GetLocalSize().X / Total, 0.05f, 0.6f);
				Settings->InspectorPaneFraction = FMath::Clamp(Inspector->GetCachedGeometry().GetLocalSize().X / Total, 0.1f, 0.7f);
			}
		}
		Settings->Save();
	}

	void SDreamShaderBrowserShell::ShowSource(const FString& SourceFilePath)
	{
		TSharedPtr<FBrowserEntry> Entry = Model->FindBySourcePath(SourceFilePath);
		if (!Entry.IsValid())
		{
			return;
		}
		// Widen the scope if the file is outside it, then switch to Sources and select.
		FBrowserScope Scope;
		Scope.Mode = EDreamShaderBrowserViewMode::Sources;
		if (SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Sources
			&& (SharedState->Scope.SourceDirectory.IsEmpty()
				|| UE::DreamShader::IsPathUnderSourceDirectory(SourceFilePath, SharedState->Scope.SourceDirectory)))
		{
			Scope = SharedState->Scope;
		}
		SharedState->SetScope(Scope);
		if (Navigation.IsValid())
		{
			Navigation->SelectScope(Scope);
		}
		if (SourcesView.IsValid())
		{
			SourcesView->SelectByKey(Entry->Key);
		}
	}

	void SDreamShaderBrowserShell::ShowAsset(const FString& ObjectPath)
	{
		if (ObjectPath.IsEmpty())
		{
			return;
		}
		FBrowserScope Scope;
		Scope.Mode = EDreamShaderBrowserViewMode::Assets;
		// Keep the current content scope when it already contains the asset.
		const FString PackagePath = FPackageName::GetLongPackagePath(FPackageName::ObjectPathToPackageName(ObjectPath));
		if (SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Assets
			&& (PackagePath == SharedState->Scope.ContentPath || PackagePath.StartsWith(SharedState->Scope.ContentPath + TEXT("/"))))
		{
			Scope = SharedState->Scope;
		}
		else
		{
			// The asset's mount point: "/Game" or "/PluginName".
			FString Remainder;
			PackagePath.Mid(1).Split(TEXT("/"), &Scope.ContentPath, &Remainder);
			Scope.ContentPath = Scope.ContentPath.IsEmpty() ? PackagePath : (TEXT("/") + Scope.ContentPath);
		}
		SharedState->SetScope(Scope);
		if (Navigation.IsValid())
		{
			Navigation->SelectScope(Scope);
		}
		if (AssetsView.IsValid())
		{
			AssetsView->SyncToObjectPath(ObjectPath);
		}
	}

	FReply SDreamShaderBrowserShell::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (CommandList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	// ---------------------------------------------------------------------------------------------
	// Commands

	TSharedPtr<FBrowserEntry> SDreamShaderBrowserShell::FirstSelected() const
	{
		return Selection.Num() > 0 ? Selection[0] : nullptr;
	}

	bool SDreamShaderBrowserShell::HasSelectionWithSource() const
	{
		return Selection.ContainsByPredicate([](const TSharedPtr<FBrowserEntry>& E) { return E.IsValid() && E->Source.IsSet(); });
	}

	bool SDreamShaderBrowserShell::HasSelectionWithMaterial() const
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		return Entry.IsValid() && !Entry->IsLibrary() && !Entry->GetObjectPath().IsEmpty() && Entry->Asset.IsSet();
	}

	bool SDreamShaderBrowserShell::HasSelectionInMemory() const
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		return Entry.IsValid() && Entry->Asset.IsSet() && Entry->Asset->Storage == EBrowserStorage::InMemory;
	}

	bool SDreamShaderBrowserShell::HasSelectionGenerated() const
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		return Entry.IsValid() && Entry->Asset.IsSet() && Entry->Asset->Provenance != EDreamShaderDigestState::Foreign;
	}

	bool SDreamShaderBrowserShell::HasSelectionForeignMaterial() const
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		return Entry.IsValid() && Entry->Asset.IsSet() && Entry->Asset->Provenance == EDreamShaderDigestState::Foreign;
	}

	UObject* SDreamShaderBrowserShell::FirstSelectedAssetObject() const
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		return (Entry.IsValid() && Entry->Asset.IsSet()) ? FindObject<UObject>(nullptr, *Entry->Asset->ObjectPath) : nullptr;
	}

	void SDreamShaderBrowserShell::BindCommands()
	{
		const FDreamShaderBrowserCommands& Commands = FDreamShaderBrowserCommands::Get();

		CommandList->MapAction(Commands.Refresh,
			FExecuteAction::CreateLambda([this]() { Model->RefreshAll(); }));
		CommandList->MapAction(Commands.CompileSelected,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteCompileSelected),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionWithSource));
		CommandList->MapAction(Commands.CompileStale,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteCompileStale));
		CommandList->MapAction(Commands.CompileAll,
			FExecuteAction::CreateLambda([this]() { FDreamShaderBrowserActions::CompileAll(*Model); }));
		CommandList->MapAction(Commands.OpenMaterial,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteOpenMaterial),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionWithMaterial));
		CommandList->MapAction(Commands.OpenSource,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteOpenSource),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionWithSource));
		CommandList->MapAction(Commands.CreateInstance,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteCreateInstance),
			FCanExecuteAction::CreateLambda([this]() { const TSharedPtr<FBrowserEntry> E = FirstSelected(); return E.IsValid() && !E->IsLibrary(); }));
		CommandList->MapAction(Commands.Materialize,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteMaterialize),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionInMemory),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionInMemory));
		CommandList->MapAction(Commands.RevealInContentBrowser,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteRevealInContentBrowser),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionWithMaterial));
		CommandList->MapAction(Commands.CopySourcePath,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteCopySourcePath),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionWithSource));
		CommandList->MapAction(Commands.CopyAssetPath,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteCopyAssetPath),
			FCanExecuteAction::CreateLambda([this]() { const TSharedPtr<FBrowserEntry> E = FirstSelected(); return E.IsValid() && !E->GetObjectPath().IsEmpty(); }));
		CommandList->MapAction(Commands.RevertToSource,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteRevert),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionGenerated));
		CommandList->MapAction(Commands.AdoptIntoSource,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteAdopt),
			FCanExecuteAction::CreateLambda([this]()
			{
				// Adopt rewrites the source; a plugin's shipped sources are read-only.
				const TSharedPtr<FBrowserEntry> E = FirstSelected();
				return HasSelectionGenerated() && E.IsValid() && (!E->Source.IsSet() || E->Source->bWritableRoot);
			}));
		CommandList->MapAction(Commands.DetachFromDreamShader,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteDetach),
			FCanExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::HasSelectionGenerated));
		CommandList->MapAction(Commands.FocusSearch,
			FExecuteAction::CreateLambda([this]()
			{
				if (SearchBox.IsValid()) { FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly); }
			}));
		CommandList->MapAction(Commands.ToggleTileView,
			FExecuteAction::CreateSP(this, &SDreamShaderBrowserShell::ExecuteToggleTileView),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return SourcesView.IsValid() && SourcesView->IsTileView(); }));
	}

	void SDreamShaderBrowserShell::ExecuteCompileSelected()
	{
		for (const TSharedPtr<FBrowserEntry>& Entry : TArray<TSharedPtr<FBrowserEntry>>(Selection))
		{
			if (Entry.IsValid() && Entry->Source.IsSet())
			{
				const TSharedPtr<FBrowserEntry> Scanned = Model->FindBySourcePath(Entry->Key);
				FDreamShaderBrowserActions::Compile(*Model, Scanned.IsValid() ? Scanned : Entry);
			}
		}
	}

	void SDreamShaderBrowserShell::ExecuteCompileStale()
	{
		int32 Compiled = 0;
		for (const TSharedPtr<FBrowserEntry>& Entry : TArray<TSharedPtr<FBrowserEntry>>(Model->GetEntries()))
		{
			if (!Entry->Source.IsSet())
			{
				continue;
			}
			const EBrowserSourceStatus Status = Entry->Source->Status;
			if (Status == EBrowserSourceStatus::Stale || Status == EBrowserSourceStatus::NotCompiled || Status == EBrowserSourceStatus::Error)
			{
				FDreamShaderBrowserActions::Compile(*Model, Entry);
				++Compiled;
			}
		}
		if (Compiled == 0)
		{
			FDreamShaderBrowserActions::Notify(LOCTEXT("NothingStale", "Nothing is stale."), true);
		}
	}

	void SDreamShaderBrowserShell::ExecuteOpenMaterial()
	{
		if (const TSharedPtr<FBrowserEntry> Entry = FirstSelected())
		{
			FDreamShaderBrowserActions::OpenMaterial(*Entry);
		}
	}

	void SDreamShaderBrowserShell::ExecuteOpenSource()
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		if (!Entry.IsValid() || !Entry->Source.IsSet())
		{
			return;
		}
		// Land on the first error when there is one.
		int32 Line = 1, Column = 1;
		for (const FDreamShaderDiagnosticRecord& Record : Entry->Source->Diagnostics)
		{
			if (Record.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
			{
				Line = FMath::Max(1, Record.Line);
				Column = FMath::Max(1, Record.Column);
				break;
			}
		}
		FDreamShaderBrowserActions::OpenSource(*Entry, Line, Column);
	}

	void SDreamShaderBrowserShell::ExecuteCreateInstance()
	{
		if (const TSharedPtr<FBrowserEntry> Entry = FirstSelected())
		{
			FDreamShaderBrowserActions::CreateInstance(*Model, Entry);
		}
	}

	void SDreamShaderBrowserShell::ExecuteMaterialize()
	{
		if (const TSharedPtr<FBrowserEntry> Entry = FirstSelected())
		{
			FDreamShaderBrowserActions::Materialize(*Model, Entry);
		}
	}

	void SDreamShaderBrowserShell::ExecuteRevealInContentBrowser()
	{
		if (UObject* Asset = FirstSelectedAssetObject())
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(TArray<UObject*>{ Asset });
		}
	}

	void SDreamShaderBrowserShell::ExecuteCopySourcePath()
	{
		const TSharedPtr<FBrowserEntry> Entry = FirstSelected();
		if (Entry.IsValid() && Entry->Source.IsSet())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Entry->Source->FilePath);
		}
	}

	void SDreamShaderBrowserShell::ExecuteCopyAssetPath()
	{
		if (const TSharedPtr<FBrowserEntry> Entry = FirstSelected())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Entry->GetObjectPath());
		}
	}

	void SDreamShaderBrowserShell::ExecuteRevert()
	{
		if (UObject* Asset = FirstSelectedAssetObject())
		{
			RevertGeneratedAssetToSource(Asset);
		}
	}

	void SDreamShaderBrowserShell::ExecuteAdopt()
	{
		if (UObject* Asset = FirstSelectedAssetObject())
		{
			AdoptGeneratedAssetIntoSource(Asset);
		}
	}

	void SDreamShaderBrowserShell::ExecuteDetach()
	{
		if (UObject* Asset = FirstSelectedAssetObject())
		{
			DetachGeneratedAssetFromDreamShader(Asset);
			Model->RefreshStatuses();
		}
	}

	void SDreamShaderBrowserShell::ExecuteToggleTileView()
	{
		if (SourcesView.IsValid())
		{
			SourcesView->SetTileView(!SourcesView->IsTileView());
			SaveLayout();
		}
	}
}

#undef LOCTEXT_NAMESPACE
