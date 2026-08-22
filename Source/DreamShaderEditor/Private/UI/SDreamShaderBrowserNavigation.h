// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DreamShaderBrowserState.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class ITableRow;
class STableViewBase;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;

	// One node of the navigation tree: the Sources / Content headers, a source root, a source folder,
	// a content root, or a content folder. Selecting a node sets the shared scope.
	struct FBrowserNavNode
	{
		enum class EKind : uint8 { SourcesHeader, SourceRoot, SourceFolder, ContentHeader, ContentRoot, ContentFolder };

		EKind Kind = EKind::SourceFolder;
		FString DisplayName;
		FBrowserScope Scope;
		TArray<TSharedPtr<FBrowserNavNode>> Children;
		bool bChildrenPopulated = false; // content folders are expanded lazily from the asset registry
		int32 SourceCount = 0;           // sources under this node (recursive), for the label
	};

	// The browser's left pane: a tree of source roots and folders over a tree of content paths, with
	// the quick status filters underneath. Rebuilds its source half from the model on every change.
	class SDreamShaderBrowserNavigation : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderBrowserNavigation) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
			SLATE_ARGUMENT(TSharedPtr<FBrowserSharedState>, SharedState)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderBrowserNavigation() override;

		// Select the node for a scope (expanding its ancestors); no-op when none matches.
		void SelectScope(const FBrowserScope& Scope);

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<FBrowserSharedState> SharedState;
		FDelegateHandle ModelChangedHandle;

		TArray<TSharedPtr<FBrowserNavNode>> RootNodes;
		TSharedPtr<STreeView<TSharedPtr<FBrowserNavNode>>> TreeView;
		bool bSuppressSelectionEvents = false;

		void RebuildTree();
		TSharedPtr<FBrowserNavNode> BuildSourcesTree() const;
		TSharedPtr<FBrowserNavNode> BuildContentTree() const;
		void PopulateContentChildren(const TSharedPtr<FBrowserNavNode>& Node) const;
		TSharedPtr<FBrowserNavNode> FindNodeForScope(const TArray<TSharedPtr<FBrowserNavNode>>& Nodes, const FBrowserScope& Scope, TArray<TSharedPtr<FBrowserNavNode>>& OutAncestors) const;

		TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FBrowserNavNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
		void OnGetChildren(TSharedPtr<FBrowserNavNode> Node, TArray<TSharedPtr<FBrowserNavNode>>& OutChildren);
		void OnSelectionChanged(TSharedPtr<FBrowserNavNode> Node, ESelectInfo::Type SelectInfo);

		TSharedRef<SWidget> MakeQuickFilter(const FText& Label, const FText& Tooltip, bool FBrowserFilter::*Member);
	};
}
