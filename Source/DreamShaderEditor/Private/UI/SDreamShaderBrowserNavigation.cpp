// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderBrowserNavigation.h"

#include "DreamShaderModule.h"
#include "UI/Model/DreamShaderBrowserModel.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		const FSlateBrush* GetNodeIcon(FBrowserNavNode::EKind Kind)
		{
			switch (Kind)
			{
			case FBrowserNavNode::EKind::SourcesHeader:
			case FBrowserNavNode::EKind::ContentHeader:
				return FAppStyle::Get().GetBrush("Icons.FolderClosed");
			case FBrowserNavNode::EKind::SourceRoot:
				return FAppStyle::Get().GetBrush("Icons.Package");
			case FBrowserNavNode::EKind::ContentRoot:
				return FAppStyle::Get().GetBrush("ContentBrowser.AssetTreeFolderOpen");
			default:
				return FAppStyle::Get().GetBrush("ContentBrowser.AssetTreeFolderClosed");
			}
		}

		// Finds or creates the folder node for one relative directory ("Lib/Noise") under a root.
		TSharedPtr<FBrowserNavNode> FindOrAddFolder(const TSharedPtr<FBrowserNavNode>& Root, const FString& RelativeDirectory)
		{
			TSharedPtr<FBrowserNavNode> Cursor = Root;
			TArray<FString> Parts;
			RelativeDirectory.ParseIntoArray(Parts, TEXT("/"), true);
			FString Accumulated = Root->Scope.SourceDirectory;
			for (const FString& Part : Parts)
			{
				Accumulated /= Part;
				TSharedPtr<FBrowserNavNode>* Existing = Cursor->Children.FindByPredicate(
					[&Part](const TSharedPtr<FBrowserNavNode>& Child) { return Child->DisplayName == Part; });
				if (Existing)
				{
					Cursor = *Existing;
				}
				else
				{
					TSharedPtr<FBrowserNavNode> Folder = MakeShared<FBrowserNavNode>();
					Folder->Kind = FBrowserNavNode::EKind::SourceFolder;
					Folder->DisplayName = Part;
					Folder->Scope.Mode = EDreamShaderBrowserViewMode::Sources;
					Folder->Scope.SourceDirectory = Accumulated;
					Folder->bChildrenPopulated = true;
					Cursor->Children.Add(Folder);
					Cursor = Folder;
				}
			}
			return Cursor;
		}

		void SortChildrenRecursive(const TSharedPtr<FBrowserNavNode>& Node)
		{
			Node->Children.Sort([](const TSharedPtr<FBrowserNavNode>& A, const TSharedPtr<FBrowserNavNode>& B)
			{
				return A->DisplayName < B->DisplayName;
			});
			for (const TSharedPtr<FBrowserNavNode>& Child : Node->Children)
			{
				SortChildrenRecursive(Child);
			}
		}

		int32 CountSourcesRecursive(const TSharedPtr<FBrowserNavNode>& Node)
		{
			int32 Total = Node->SourceCount;
			for (const TSharedPtr<FBrowserNavNode>& Child : Node->Children)
			{
				Total += CountSourcesRecursive(Child);
			}
			Node->SourceCount = Total;
			return Total;
		}
	}

	void SDreamShaderBrowserNavigation::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		SharedState = InArgs._SharedState;
		check(Model.IsValid() && SharedState.IsValid());
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderBrowserNavigation::RebuildTree);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FBrowserNavNode>>)
				.TreeItemsSource(&RootNodes)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SDreamShaderBrowserNavigation::OnGenerateRow)
				.OnGetChildren(this, &SDreamShaderBrowserNavigation::OnGetChildren)
				.OnSelectionChanged(this, &SDreamShaderBrowserNavigation::OnSelectionChanged)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(8.0f, 6.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("QuickFilters", "Quick filters"))
						.TextStyle(FAppStyle::Get(), "SmallText")
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFErrors", "Errors"), LOCTEXT("QFErrorsTip", "Sources whose last compile failed, or that could not be read."), &FBrowserFilter::bErrorsOnly) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFStale", "Stale"), LOCTEXT("QFStaleTip", "Sources that changed since their asset was last generated."), &FBrowserFilter::bStaleOnly) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFDiverged", "Edited by hand"), LOCTEXT("QFDivergedTip", "Generated assets that no longer match what DreamShader last wrote into them."), &FBrowserFilter::bDivergedOnly) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFInMemory", "In memory"), LOCTEXT("QFInMemoryTip", "Materials that exist only in memory and have not been written to disk."), &FBrowserFilter::bInMemoryOnly) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFHideLibraries", "Hide functions"), LOCTEXT("QFHideLibrariesTip", "Drop every .dsf and .dsh from the list."), &FBrowserFilter::bHideLibraries) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeQuickFilter(LOCTEXT("QFHideUnmanaged", "Hide unmanaged"), LOCTEXT("QFHideUnmanagedTip", "Drop the materials DreamShader does not manage from the list."), &FBrowserFilter::bHideUnmanaged) ]
				]
			]
		];

		RebuildTree();
	}

	SDreamShaderBrowserNavigation::~SDreamShaderBrowserNavigation()
	{
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
	}

	TSharedRef<SWidget> SDreamShaderBrowserNavigation::MakeQuickFilter(const FText& Label, const FText& Tooltip, bool FBrowserFilter::*Member)
	{
		const TSharedPtr<FBrowserSharedState> State = SharedState;
		return SNew(SCheckBox)
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([State, Member]() { return (State->Filter.*Member) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([State, Member](ECheckBoxState NewState)
			{
				State->Filter.*Member = (NewState == ECheckBoxState::Checked);
				State->NotifyFilterChanged();
			})
			[
				SNew(STextBlock).Text(Label)
			];
	}

	void SDreamShaderBrowserNavigation::RebuildTree()
	{
		const FBrowserScope CurrentScope = SharedState->Scope;

		// The model changes on every compile, and every rebuild makes new node objects, so the tree
		// view's expansion (keyed by object) would collapse under the user each time. Carry it across
		// by scope key; the first build expands the headers and the roots.
		TSet<FString> ExpandedKeys;
		if (TreeView.IsValid() && bBuiltOnce)
		{
			TSet<TSharedPtr<FBrowserNavNode>> Expanded;
			TreeView->GetExpandedItems(Expanded);
			for (const TSharedPtr<FBrowserNavNode>& Node : Expanded)
			{
				ExpandedKeys.Add(Node->Scope.ToKey() + (Node->Kind == FBrowserNavNode::EKind::SourcesHeader || Node->Kind == FBrowserNavNode::EKind::ContentHeader ? TEXT("#header") : TEXT("")));
			}
		}

		RootNodes.Reset();
		RootNodes.Add(BuildSourcesTree());
		RootNodes.Add(BuildContentTree());

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
			if (!bBuiltOnce)
			{
				for (const TSharedPtr<FBrowserNavNode>& Root : RootNodes)
				{
					TreeView->SetItemExpansion(Root, true);
					for (const TSharedPtr<FBrowserNavNode>& Child : Root->Children)
					{
						TreeView->SetItemExpansion(Child, true);
					}
				}
			}
			else
			{
				for (const FString& Key : ExpandedKeys)
				{
					const bool bHeader = Key.EndsWith(TEXT("#header"));
					const FBrowserScope Scope = FBrowserScope::FromKey(bHeader ? Key.LeftChop(7) : Key);
					TArray<TSharedPtr<FBrowserNavNode>> Ancestors;
					TSharedPtr<FBrowserNavNode> Node = bHeader
						? RootNodes[Scope.Mode == EDreamShaderBrowserViewMode::Sources ? 0 : 1]
						: FindNodeForScope(RootNodes, Scope, Ancestors);
					if (Node.IsValid())
					{
						TreeView->SetItemExpansion(Node, true);
					}
				}
			}
			bBuiltOnce = true;
		}
		SelectScope(CurrentScope);
	}

	TSharedPtr<FBrowserNavNode> SDreamShaderBrowserNavigation::BuildSourcesTree() const
	{
		TSharedPtr<FBrowserNavNode> Header = MakeShared<FBrowserNavNode>();
		Header->Kind = FBrowserNavNode::EKind::SourcesHeader;
		Header->DisplayName = LOCTEXT("NavSources", "Sources").ToString();
		Header->Scope.Mode = EDreamShaderBrowserViewMode::Sources;
		Header->bChildrenPopulated = true;

		TMap<FString, TSharedPtr<FBrowserNavNode>> RootNodesByDirectory;
		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			TSharedPtr<FBrowserNavNode> RootNode = MakeShared<FBrowserNavNode>();
			RootNode->Kind = FBrowserNavNode::EKind::SourceRoot;
			RootNode->DisplayName = Root.DisplayName;
			RootNode->Scope.Mode = EDreamShaderBrowserViewMode::Sources;
			RootNode->Scope.SourceDirectory = Root.Directory;
			RootNode->bChildrenPopulated = true;
			Header->Children.Add(RootNode);
			RootNodesByDirectory.Add(Root.Directory, RootNode);
		}

		for (const TSharedPtr<FBrowserEntry>& Entry : Model->GetEntries())
		{
			if (!Entry->Source.IsSet())
			{
				continue;
			}
			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(Entry->Source->FilePath);
			TSharedPtr<FBrowserNavNode>* RootNode = Root ? RootNodesByDirectory.Find(Root->Directory) : nullptr;
			if (!RootNode)
			{
				continue;
			}
			// Both are normalized, and the file is under the root by construction.
			const FString Directory = FPaths::GetPath(Entry->Source->FilePath);
			const FString Relative = Directory.Len() > Root->Directory.Len() ? Directory.Mid(Root->Directory.Len() + 1) : FString();
			TSharedPtr<FBrowserNavNode> Folder = Relative.IsEmpty() ? *RootNode : FindOrAddFolder(*RootNode, Relative);
			++Folder->SourceCount;
		}

		SortChildrenRecursive(Header);
		CountSourcesRecursive(Header);

		// After the roots, unsorted on purpose: the materials DreamShader does not manage, in one place.
		TSharedPtr<FBrowserNavNode> Unmanaged = MakeShared<FBrowserNavNode>();
		Unmanaged->Kind = FBrowserNavNode::EKind::SourceFolder;
		Unmanaged->DisplayName = LOCTEXT("NavUnmanaged", "Not managed by DreamShader").ToString();
		Unmanaged->Scope.Mode = EDreamShaderBrowserViewMode::Sources;
		Unmanaged->Scope.SourceDirectory = FBrowserFilter::UnmanagedScope();
		Unmanaged->bChildrenPopulated = true;
		Unmanaged->SourceCount = Model->GetUnmanagedCount();
		Header->Children.Add(Unmanaged);
		Header->SourceCount += Unmanaged->SourceCount;
		return Header;
	}

	TSharedPtr<FBrowserNavNode> SDreamShaderBrowserNavigation::BuildContentTree() const
	{
		TSharedPtr<FBrowserNavNode> Header = MakeShared<FBrowserNavNode>();
		Header->Kind = FBrowserNavNode::EKind::ContentHeader;
		Header->DisplayName = LOCTEXT("NavContent", "Content").ToString();
		Header->Scope.Mode = EDreamShaderBrowserViewMode::Assets;
		Header->Scope.ContentPath = TEXT("/Game");
		Header->bChildrenPopulated = true;

		// /Game, then the content root of every plugin that ships DreamShader sources -- that is where
		// its generated assets land. Other plugins' content is reachable through the main Content
		// Browser; listing every mounted root here would bury the ones that matter.
		for (const FString& ContentRoot : FDreamShaderBrowserModel::GetContentRoots())
		{
			TSharedPtr<FBrowserNavNode> RootNode = MakeShared<FBrowserNavNode>();
			RootNode->Kind = FBrowserNavNode::EKind::ContentRoot;
			RootNode->DisplayName = ContentRoot;
			RootNode->Scope.Mode = EDreamShaderBrowserViewMode::Assets;
			RootNode->Scope.ContentPath = ContentRoot;
			Header->Children.Add(RootNode);
		}
		return Header;
	}

	void SDreamShaderBrowserNavigation::PopulateContentChildren(const TSharedPtr<FBrowserNavNode>& Node) const
	{
		if (Node->bChildrenPopulated)
		{
			return;
		}
		Node->bChildrenPopulated = true;

		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return;
		}
		TArray<FString> SubPaths;
		AssetRegistry->GetSubPaths(Node->Scope.ContentPath, SubPaths, /*bInRecurse*/ false);
		SubPaths.Sort();
		for (const FString& SubPath : SubPaths)
		{
			TSharedPtr<FBrowserNavNode> Child = MakeShared<FBrowserNavNode>();
			Child->Kind = FBrowserNavNode::EKind::ContentFolder;
			Child->DisplayName = FPaths::GetCleanFilename(SubPath);
			Child->Scope.Mode = EDreamShaderBrowserViewMode::Assets;
			Child->Scope.ContentPath = SubPath;
			Node->Children.Add(Child);
		}
	}

	TSharedPtr<FBrowserNavNode> SDreamShaderBrowserNavigation::FindNodeForScope(
		const TArray<TSharedPtr<FBrowserNavNode>>& Nodes, const FBrowserScope& Scope, TArray<TSharedPtr<FBrowserNavNode>>& OutAncestors) const
	{
		for (const TSharedPtr<FBrowserNavNode>& Node : Nodes)
		{
			if (Node->Scope == Scope)
			{
				return Node;
			}
			// Only descend into branches that can contain the scope, populating content folders on
			// the way so a remembered deep content path can be re-selected.
			const bool bMayContain = Node->Kind == FBrowserNavNode::EKind::SourcesHeader
				|| Node->Kind == FBrowserNavNode::EKind::ContentHeader
				|| (Scope.Mode == EDreamShaderBrowserViewMode::Sources
					&& Node->Scope.Mode == EDreamShaderBrowserViewMode::Sources
					&& UE::DreamShader::IsPathUnderSourceDirectory(Scope.SourceDirectory, Node->Scope.SourceDirectory))
				|| (Scope.Mode == EDreamShaderBrowserViewMode::Assets
					&& Node->Scope.Mode == EDreamShaderBrowserViewMode::Assets
					&& Scope.ContentPath.StartsWith(Node->Scope.ContentPath + TEXT("/")));
			if (!bMayContain)
			{
				continue;
			}
			PopulateContentChildren(Node);
			OutAncestors.Add(Node);
			if (TSharedPtr<FBrowserNavNode> Found = FindNodeForScope(Node->Children, Scope, OutAncestors))
			{
				return Found;
			}
			OutAncestors.Pop();
		}
		return nullptr;
	}

	void SDreamShaderBrowserNavigation::SelectScope(const FBrowserScope& Scope)
	{
		if (!TreeView.IsValid())
		{
			return;
		}
		TArray<TSharedPtr<FBrowserNavNode>> Ancestors;
		TSharedPtr<FBrowserNavNode> Node = FindNodeForScope(RootNodes, Scope, Ancestors);
		if (!Node.IsValid())
		{
			return;
		}
		for (const TSharedPtr<FBrowserNavNode>& Ancestor : Ancestors)
		{
			TreeView->SetItemExpansion(Ancestor, true);
		}
		TGuardValue<bool> Suppress(bSuppressSelectionEvents, true);
		TreeView->SetSelection(Node, ESelectInfo::Direct);
		TreeView->RequestScrollIntoView(Node);
	}

	TSharedRef<ITableRow> SDreamShaderBrowserNavigation::OnGenerateRow(TSharedPtr<FBrowserNavNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const bool bHeader = Node->Kind == FBrowserNavNode::EKind::SourcesHeader || Node->Kind == FBrowserNavNode::EKind::ContentHeader;
		const bool bShowCount = Node->Scope.Mode == EDreamShaderBrowserViewMode::Sources && !bHeader;

		return SNew(STableRow<TSharedPtr<FBrowserNavNode>>, OwnerTable)
			.Padding(FMargin(2.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage).Image(GetNodeIcon(Node->Kind)).ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Node->DisplayName))
					.TextStyle(FAppStyle::Get(), bHeader ? "ButtonText" : "NormalText")
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility(bShowCount ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(FText::AsNumber(Node->SourceCount))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			];
	}

	void SDreamShaderBrowserNavigation::OnGetChildren(TSharedPtr<FBrowserNavNode> Node, TArray<TSharedPtr<FBrowserNavNode>>& OutChildren)
	{
		PopulateContentChildren(Node);
		OutChildren = Node->Children;
	}

	void SDreamShaderBrowserNavigation::OnSelectionChanged(TSharedPtr<FBrowserNavNode> Node, ESelectInfo::Type)
	{
		if (bSuppressSelectionEvents || !Node.IsValid())
		{
			return;
		}
		SharedState->SetScope(Node->Scope);
	}
}

#undef LOCTEXT_NAMESPACE
