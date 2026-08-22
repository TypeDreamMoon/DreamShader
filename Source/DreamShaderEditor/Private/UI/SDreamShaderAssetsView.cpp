// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderAssetsView.h"

#include "UI/Model/DreamShaderBrowserModel.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	void SDreamShaderAssetsView::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		SharedState = InArgs._SharedState;
		OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
		OnEntryActivatedDelegate = InArgs._OnEntryActivated;
		OnGetContextMenuDelegate = InArgs._OnGetContextMenu;
		check(Model.IsValid() && SharedState.IsValid());

		// A status change on a scanned source can move an asset in or out of the status filters.
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderAssetsView::OnFilterChanged);
		FilterChangedHandle = SharedState->OnFilterChanged.AddSP(this, &SDreamShaderAssetsView::OnFilterChanged);
		ScopeChangedHandle = SharedState->OnScopeChanged.AddSP(this, &SDreamShaderAssetsView::OnScopeChanged);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		FAssetPickerConfig PickerConfig;
		PickerConfig.Filter = MakeScopeFilter();
		PickerConfig.InitialAssetViewType = EAssetViewType::Tile;
		PickerConfig.bAllowDragging = true;
		PickerConfig.bCanShowClasses = false;
		PickerConfig.bCanShowFolders = false;
		PickerConfig.bForceShowEngineContent = false;
		PickerConfig.bForceShowPluginContent = true;
		PickerConfig.bShowPathInColumnView = true;
		PickerConfig.bShowTypeInColumnView = false;
		PickerConfig.bShowBottomToolbar = true;
		// The shell's search box drives the filter below; the picker's own bar would be a second one.
		PickerConfig.bAutohideSearchBar = true;
		PickerConfig.bAddFilterUI = false;
		PickerConfig.SaveSettingsName = TEXT("DreamShaderMaterialBrowser.Assets");
		PickerConfig.SelectionMode = ESelectionMode::Multi;
		PickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SDreamShaderAssetsView::OnShouldFilterAsset);
		PickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SDreamShaderAssetsView::OnAssetSelected);
		PickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SDreamShaderAssetsView::OnAssetDoubleClicked);
		PickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SDreamShaderAssetsView::OnGetAssetContextMenu);
		PickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelection);
		PickerConfig.SyncToAssetsDelegates.Add(&SyncToAssets);
		PickerConfig.SetFilterDelegates.Add(&SetFilter);
		PickerConfig.RefreshAssetViewDelegates.Add(&RefreshAssetView);
		PickerConfig.AssetShowWarningText = LOCTEXT("EmptyContentScope", "No materials here.");

		ChildSlot
		[
			ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
		];
	}

	SDreamShaderAssetsView::~SDreamShaderAssetsView()
	{
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
		if (SharedState.IsValid())
		{
			SharedState->OnFilterChanged.Remove(FilterChangedHandle);
			SharedState->OnScopeChanged.Remove(ScopeChangedHandle);
		}
	}

	FARFilter SDreamShaderAssetsView::MakeScopeFilter() const
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
		// Recursive so UDreamShaderMaterialInstance (a UMaterialInstanceConstant subclass) is included.
		Filter.bRecursiveClasses = true;
		const FString& ContentPath = SharedState->Scope.ContentPath;
		Filter.PackagePaths.Add(*(ContentPath.IsEmpty() ? FString(TEXT("/Game")) : ContentPath));
		Filter.bRecursivePaths = true;
		return Filter;
	}

	TArray<TSharedPtr<FBrowserEntry>> SDreamShaderAssetsView::EntriesForAssets(const TArray<FAssetData>& Assets) const
	{
		TArray<TSharedPtr<FBrowserEntry>> Entries;
		for (const FAssetData& AssetData : Assets)
		{
			if (TSharedPtr<FBrowserEntry> Entry = Model->MakeEntryForAsset(AssetData.GetAsset()))
			{
				Entries.Add(Entry);
			}
		}
		return Entries;
	}

	TArray<TSharedPtr<FBrowserEntry>> SDreamShaderAssetsView::GetSelectedEntries() const
	{
		return GetCurrentSelection.IsBound() ? EntriesForAssets(GetCurrentSelection.Execute()) : TArray<TSharedPtr<FBrowserEntry>>();
	}

	void SDreamShaderAssetsView::SyncToObjectPath(const FString& ObjectPath)
	{
		if (!SyncToAssets.IsBound() || ObjectPath.IsEmpty())
		{
			return;
		}
		FAssetData AssetData;
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetData = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		}
		if (!AssetData.IsValid())
		{
			// A memory-only material is not in the registry unless the in-memory toggle is on.
			if (UObject* Object = FindObject<UObject>(nullptr, *ObjectPath))
			{
				AssetData = FAssetData(Object);
			}
		}
		if (AssetData.IsValid())
		{
			SyncToAssets.Execute({ AssetData });
		}
	}

	bool SDreamShaderAssetsView::OnShouldFilterAsset(const FAssetData& AssetData) const
	{
		// Returning true hides the asset. Everything here is answered without loading it: the scan
		// already knows the status of every generated asset by object path, and the registry carries
		// the package flags that say whether it lives in memory.
		const FBrowserFilter& Filter = SharedState->Filter;
		if (!Filter.SearchText.IsEmpty() && !AssetData.AssetName.ToString().Contains(Filter.SearchText, ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (!Filter.HasStatusFilter())
		{
			return false;
		}

		if (TSharedPtr<FBrowserEntry> Entry = Model->FindByObjectPath(AssetData.GetObjectPathString()))
		{
			return !Filter.MatchesStatus(*Entry);
		}
		// Not a scanned source's asset: only the in-memory toggle can say yes to it.
		const bool bInMemory = (AssetData.PackageFlags & PKG_NewlyCreated) != 0;
		return !(Filter.bInMemoryOnly && bInMemory);
	}

	void SDreamShaderAssetsView::OnAssetSelected(const FAssetData&)
	{
		OnSelectionChangedDelegate.ExecuteIfBound(GetSelectedEntries());
	}

	void SDreamShaderAssetsView::OnAssetDoubleClicked(const FAssetData& AssetData)
	{
		if (TSharedPtr<FBrowserEntry> Entry = Model->MakeEntryForAsset(AssetData.GetAsset()))
		{
			OnEntryActivatedDelegate.ExecuteIfBound(Entry);
		}
	}

	TSharedPtr<SWidget> SDreamShaderAssetsView::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
	{
		return OnGetContextMenuDelegate.IsBound() ? OnGetContextMenuDelegate.Execute(EntriesForAssets(SelectedAssets)) : nullptr;
	}

	void SDreamShaderAssetsView::OnScopeChanged()
	{
		if (SharedState->Scope.Mode == EDreamShaderBrowserViewMode::Assets)
		{
			SetFilter.ExecuteIfBound(MakeScopeFilter());
		}
	}

	void SDreamShaderAssetsView::OnFilterChanged()
	{
		RefreshAssetView.ExecuteIfBound(/*bUpdateSources*/ false);
	}
}

#undef LOCTEXT_NAMESPACE
