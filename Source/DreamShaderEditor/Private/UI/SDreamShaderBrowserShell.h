// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DreamShaderBrowserState.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class SSearchBox;
class SWidgetSwitcher;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;
	class SDreamShaderAssetsView;
	class SDreamShaderBrowserNavigation;
	class SDreamShaderInspector;
	class SDreamShaderSourcesView;
	struct FBrowserEntry;

	// The Material Content Browser tab's root: a toolbar over [navigation | sources-or-assets list |
	// inspector] over a status bar. Owns the model, the shared filter/scope state, the command list
	// every pane's actions run through, and the persistence of all of it in the user settings.
	class SDreamShaderBrowserShell : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderBrowserShell) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderBrowserShell() override;

		// Deep links from elsewhere in the editor.
		void ShowSource(const FString& SourceFilePath);
		void ShowAsset(const FString& ObjectPath);

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<FBrowserSharedState> SharedState;
		TSharedPtr<FUICommandList> CommandList;
		FDelegateHandle ModelChangedHandle;
		FDelegateHandle ScopeChangedHandle;

		TSharedPtr<SDreamShaderBrowserNavigation> Navigation;
		TSharedPtr<SWidgetSwitcher> ViewSwitcher;
		TSharedPtr<SDreamShaderSourcesView> SourcesView;
		TSharedPtr<SDreamShaderAssetsView> AssetsView;
		TSharedPtr<SDreamShaderInspector> Inspector;
		TSharedPtr<SSearchBox> SearchBox;

		TArray<TSharedPtr<FBrowserEntry>> Selection;
		int32 CountTotal = 0, CountOk = 0, CountStale = 0, CountErrors = 0, CountDiverged = 0, CountInMemory = 0;

		TSharedRef<SWidget> BuildToolbar();
		TSharedRef<SWidget> BuildStatusBar();
		TSharedRef<SWidget> MakeCompileMenu();
		TSharedRef<SWidget> MakeNewMenu();
		TSharedRef<SWidget> MakeViewMenu();
		TSharedPtr<SWidget> MakeContextMenu(const TArray<TSharedPtr<FBrowserEntry>>& Entries);

		void BindCommands();
		void OnModelChanged();
		void OnScopeChanged();
		void OnSelectionChanged(const TArray<TSharedPtr<FBrowserEntry>>& Entries);
		void OnEntryActivated(const TSharedPtr<FBrowserEntry>& Entry);
		void SaveLayout();

		// Command predicates over the current selection.
		TSharedPtr<FBrowserEntry> FirstSelected() const;
		bool HasSelectionWithSource() const;
		bool HasSelectionWithMaterial() const;
		bool HasSelectionInMemory() const;
		bool HasSelectionGenerated() const;
		bool HasSelectionForeignMaterial() const;
		UObject* FirstSelectedAssetObject() const;

		void ExecuteCompileSelected();
		void ExecuteCompileStale();
		void ExecuteOpenMaterial();
		void ExecuteOpenSource();
		void ExecuteCreateInstance();
		void ExecuteMaterialize();
		void ExecuteRevealInContentBrowser();
		void ExecuteCopySourcePath();
		void ExecuteCopyAssetPath();
		void ExecuteRevert();
		void ExecuteAdopt();
		void ExecuteDetach();
		void ExecuteExportSource();
		void ExecuteToggleTileView();
	};
}
