// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;

namespace UE::DreamShader::Editor::Private
{
	class SDreamShaderInspector;

	// Source-file-centric view of the DreamShader generation pipeline: every .dsm/.dsf/.dsh under the
	// source roots with its compile status, over the shared inspector. A view over the browser model;
	// the scan, the status computation and the actions live there.
	class SDreamShaderGenPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderGenPage) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderGenPage() override;

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		FDelegateHandle ModelChangedHandle;

		FBrowserFilter Filter;
		TArray<TSharedPtr<FBrowserEntry>> VisibleItems; // after search + filters
		TSharedPtr<SListView<TSharedPtr<FBrowserEntry>>> ListView;
		TSharedPtr<SDreamShaderInspector> Inspector;
		FString SelectedKey; // survives a rescan, which replaces every entry object

		void OnModelChanged();
		void ApplyFilter();
		TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FBrowserEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void OnSelectionChanged(TSharedPtr<FBrowserEntry> Item, ESelectInfo::Type SelectInfo);
	};
}
