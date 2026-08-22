// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderInspector.h"

#include "UI/DreamShaderBrowserActions.h"
#include "UI/DreamShaderBrowserStyle.h"
#include "UI/Model/DreamShaderBrowserModel.h"

#include "AssetThumbnail.h"
#include "DreamShaderMaterialInstance.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "SceneTypes.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FText EnumText(UEnum* Enum, int64 Value)
		{
			return Enum ? Enum->GetDisplayNameTextByValue(Value) : FText::GetEmpty();
		}

		TSharedRef<SWidget> MakeInfoRow(const FText& Label, const FText& Value, const FText& Tooltip = FText::GetEmpty())
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 2.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(STextBlock).Text(Label).ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock).Text(Value).ToolTipText(Tooltip).AutoWrapText(true)
				];
		}

		TSharedRef<SWidget> MakeSectionHeader(const FText& Title)
		{
			return SNew(STextBlock).Text(Title).TextStyle(FAppStyle::Get(), "LargeText");
		}

		TSharedRef<SWidget> MakeActionButton(const FText& Label, const FText& Tip, TFunction<void()> Action, bool bPrimary = false)
		{
			TSharedRef<SButton> Button = SNew(SButton)
				.Text(Label)
				.ToolTipText(Tip)
				.OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); });
			if (bPrimary)
			{
				Button->SetButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"));
			}
			return Button;
		}
	}

	void SDreamShaderInspector::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		OnNavigateToSource = InArgs._OnNavigateToSource;
		OnNavigateToAsset = InArgs._OnNavigateToAsset;
		if (Model.IsValid())
		{
			ModelChangedHandle = Model->OnChanged.AddSP(this, &SDreamShaderInspector::Rebuild);
		}

		ChildSlot
		[
			SAssignNew(ContentContainer, SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(FMargin(12.0f))
		];
		Rebuild();
	}

	SDreamShaderInspector::~SDreamShaderInspector()
	{
		if (Model.IsValid() && ModelChangedHandle.IsValid())
		{
			Model->OnChanged.Remove(ModelChangedHandle);
		}
	}

	void SDreamShaderInspector::SetEntry(TSharedPtr<FBrowserEntry> InEntry)
	{
		Entry = InEntry;
		Rebuild();
	}

	void SDreamShaderInspector::SetMaterial(UMaterialInterface* Material)
	{
		SetEntry(Model.IsValid() ? Model->MakeEntryForAsset(Material) : nullptr);
	}

	void SDreamShaderInspector::Rebuild()
	{
		// A scanned entry is owned by the model and refreshed in place. An asset-centric one is a
		// snapshot the model handed out, so re-describe it from the live object each time the model
		// moves -- a compile or materialize may have changed its storage, provenance or joined source.
		if (Entry.IsValid() && Model.IsValid() && Model->FindBySourcePath(Entry->Key) != Entry && Entry->Asset.IsSet())
		{
			if (UObject* LiveAsset = FindObject<UObject>(nullptr, *Entry->Asset->ObjectPath))
			{
				Entry = Model->MakeEntryForAsset(LiveAsset);
			}
		}
		if (ContentContainer.IsValid())
		{
			ContentContainer->SetContent(BuildContent());
		}
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildContent()
	{
		if (!Entry.IsValid())
		{
			Thumbnail.Reset();
			return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InspectorEmpty", "Select a source file or a material to inspect it."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}

		UMaterialInterface* Material = Entry->ResolveMaterial();

		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ BuildHeader(Material) ];
		Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[ BuildActions(Material) ];
		Body->AddSlot().AutoHeight()[ BuildInfoRows(Material) ];
		Body->AddSlot().AutoHeight()[ BuildDiagnostics() ];
		Body->AddSlot().AutoHeight()[ BuildDependents() ];
		if (Material)
		{
			Body->AddSlot().AutoHeight()[ BuildInheritance(Material) ];
		}

		return SNew(SScrollBox) + SScrollBox::Slot()[ Body ];
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildHeader(UMaterialInterface* Material)
	{
		// Thumbnail, or a placeholder tile when there is no compiled material to render.
		TSharedRef<SWidget> ThumbWidget = SNullWidget::NullWidget;
		if (Material)
		{
			Thumbnail = MakeShared<FAssetThumbnail>(Material, 96, 96, UThumbnailManager::Get().GetSharedThumbnailPool());
			ThumbWidget = Thumbnail->MakeThumbnailWidget();
		}
		else
		{
			Thumbnail.Reset();
			ThumbWidget = SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Entry->IsLibrary() ? LOCTEXT("PreviewFunction", "function library") : LOCTEXT("PreviewNoMaterial", "not compiled yet"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
				];
		}

		TSharedRef<SVerticalBox> Title = SNew(SVerticalBox);
		Title->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->GetDisplayName()))
			.TextStyle(FAppStyle::Get(), "LargeText")
			.AutoWrapText(true)
		];

		if (Entry->Source.IsSet())
		{
			const FBrowserStatusVisual Visual = GetBrowserStatusVisual(Entry->Source->Status);
			Title->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(Visual.Glyph).ColorAndOpacity(FSlateColor(Visual.Color))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Visual.Label).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			];
		}

		if (Entry->Asset.IsSet())
		{
			Title->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(GetBrowserStorageLabel(Entry->Asset->Storage))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)[ ThumbWidget ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Title
			];
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildActions(UMaterialInterface* Material)
	{
		TSharedRef<SWrapBox> ActionBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
		const TSharedPtr<FBrowserEntry> EntryRef = Entry;
		const TWeakPtr<FDreamShaderBrowserModel> WeakModel = Model;
		const bool bHasSource = Entry->Source.IsSet();
		const bool bInMemory = Entry->Asset.IsSet() && Entry->Asset->Storage == EBrowserStorage::InMemory;

		if (!Entry->IsLibrary())
		{
			ActionBox->AddSlot()[ MakeActionButton(
				LOCTEXT("CreateInstanceBtn", "Create instance"),
				LOCTEXT("PInstTip", "Create a material instance of this material."),
				[WeakModel, EntryRef]()
				{
					if (TSharedPtr<FDreamShaderBrowserModel> M = WeakModel.Pin()) { FDreamShaderBrowserActions::CreateInstance(*M, EntryRef); }
				},
				/*bPrimary*/ true) ];
		}
		if (bHasSource)
		{
			ActionBox->AddSlot()[ MakeActionButton(
				LOCTEXT("PComp", "Compile"),
				LOCTEXT("PCompTip", "Force-recompile this source (in memory)."),
				[WeakModel, EntryRef]()
				{
					if (TSharedPtr<FDreamShaderBrowserModel> M = WeakModel.Pin()) { FDreamShaderBrowserActions::Compile(*M, EntryRef); }
				}) ];
		}
		if (Material)
		{
			ActionBox->AddSlot()[ MakeActionButton(
				LOCTEXT("OpenBtn", "Open"),
				LOCTEXT("POpenMatTip", "Open the generated material asset."),
				[EntryRef]() { FDreamShaderBrowserActions::OpenMaterial(*EntryRef); }) ];
		}
		if (Material && bInMemory)
		{
			ActionBox->AddSlot()[ MakeActionButton(
				LOCTEXT("MaterializeBtn", "Materialize"),
				LOCTEXT("MaterializeTip", "Write this memory-only material (and its base) to disk."),
				[this, WeakModel, EntryRef]()
				{
					TSharedPtr<FDreamShaderBrowserModel> M = WeakModel.Pin();
					if (!M.IsValid())
					{
						return;
					}
					UMaterialInterface* Persisted = FDreamShaderBrowserActions::Materialize(*M, EntryRef);
					// An asset-centric entry is a snapshot of the old object; re-target the persisted one.
					if (Persisted && M->FindBySourcePath(EntryRef->Key) != EntryRef)
					{
						SetMaterial(Persisted);
					}
				}) ];
		}
		if (bHasSource)
		{
			ActionBox->AddSlot()[ MakeActionButton(
				LOCTEXT("POpenSrc", "Open source"),
				LOCTEXT("POpenSrcTip", "Open the .dsm/.dsf in your preferred editor."),
				[EntryRef]() { FDreamShaderBrowserActions::OpenSource(*EntryRef); }) ];
		}
		return ActionBox;
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildInfoRows(UMaterialInterface* Material)
	{
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		UMaterial* Base = Material ? Material->GetBaseMaterial() : nullptr;
		const FText SourceLabel = LOCTEXT("SourceRow", "Source");
		const FText AssetLabel = LOCTEXT("AssetRow", "Asset");

		if (Material)
		{
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(LOCTEXT("Base", "Base"), Base ? FText::FromString(Base->GetName()) : LOCTEXT("BaseNone", "-")) ];
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(LOCTEXT("Domain", "Domain"), Base ? EnumText(StaticEnum<EMaterialDomain>(), static_cast<int64>(Base->MaterialDomain.GetValue())) : FText::GetEmpty()) ];
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(LOCTEXT("Blend", "Blend mode"), Base ? EnumText(StaticEnum<EBlendMode>(), static_cast<int64>(Base->BlendMode.GetValue())) : FText::GetEmpty()) ];
		}

		if (Entry->Source.IsSet())
		{
			FText RootText = Entry->Source->RootDisplayName.IsEmpty()
				? LOCTEXT("RootProject", "Project")
				: FText::FromString(Entry->Source->RootDisplayName);
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(LOCTEXT("RootRow", "Root"), RootText) ];
			const FString SourcePath = Entry->Source->FilePath;
			Rows->AddSlot().AutoHeight()[ MakeLinkRow(
				SourceLabel,
				FText::FromString(SourcePath),
				LOCTEXT("SourceLinkTip", "Show this file in the Sources list."),
				[this, SourcePath]() { OnNavigateToSource.ExecuteIfBound(SourcePath); }) ];
		}
		else if (UDreamShaderMaterialInstance* DreamInstance = Cast<UDreamShaderMaterialInstance>(Material))
		{
			// No scanned source to join to, but the instance remembers where it came from.
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(
				SourceLabel,
				DreamInstance->SourceFilePath.IsEmpty() ? LOCTEXT("SourceNone", "-") : FText::FromString(DreamInstance->SourceFilePath)) ];
		}

		if (Entry->Asset.IsSet())
		{
			const FString ObjectPath = Entry->Asset->ObjectPath;
			Rows->AddSlot().AutoHeight()[ MakeLinkRow(
				AssetLabel,
				FText::FromString(ObjectPath),
				LOCTEXT("AssetLinkTip", "Show this asset in the Content list."),
				[this, ObjectPath]() { OnNavigateToAsset.ExecuteIfBound(ObjectPath); }) ];
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(LOCTEXT("Storage", "Storage"), GetBrowserStorageLabel(Entry->Asset->Storage)) ];
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(
				LOCTEXT("ProvenanceRow", "Provenance"),
				GetBrowserProvenanceLabel(Entry->Asset->Provenance),
				LOCTEXT("ProvenanceTip", "Whether the asset still holds what DreamShader last generated into it. A hand-edited asset refuses to rebuild until you choose Revert, Adopt or Detach from its Content Browser context menu.")) ];
		}
		else if (Entry->Source.IsSet() && !Entry->Source->ResolvedObjectPath.IsEmpty())
		{
			Rows->AddSlot().AutoHeight()[ MakeInfoRow(AssetLabel, FText::FromString(Entry->Source->ResolvedObjectPath)) ];
		}

		return Rows;
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildDiagnostics()
	{
		if (!Entry->Source.IsSet() || !IsBrowserErrorStatus(Entry->Source->Status))
		{
			return SNullWidget::NullWidget;
		}

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(Entry->Source->StatusDetail)
			.ColorAndOpacity(FSlateColor(BrowserErrorColor))
			.AutoWrapText(true)
		];
		return Box;
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildDependents()
	{
		if (!Entry->IsLibrary())
		{
			return SNullWidget::NullWidget;
		}

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("PreviewUsedBy", "used by {0} material(s)"), FText::AsNumber(Entry->Source->Dependents.Num())))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		return Box;
	}

	TSharedRef<SWidget> SDreamShaderInspector::MakeLinkRow(const FText& Label, const FText& Value, const FText& Tooltip, TFunction<void()> OnClicked)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 2.0f)
			[
				SNew(SBox).WidthOverride(90.0f)
				[
					SNew(STextBlock).Text(Label).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(0.0f, 2.0f))
				.HAlign(HAlign_Left)
				.ToolTipText(Tooltip)
				.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(Value).ColorAndOpacity(FSlateColor(BrowserLinkColor)).AutoWrapText(true)
				]
			];
	}

	TSharedRef<SWidget> SDreamShaderInspector::MakeMaterialLink(UMaterialInterface* Target, const FText& Prefix, bool bIsSelf)
	{
		const TWeakObjectPtr<UMaterialInterface> WeakTarget(Target);
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.HAlign(HAlign_Left)
			.ToolTipText(FText::FromString(Target->GetPathName()))
			.OnClicked_Lambda([this, WeakTarget]()
			{
				if (UMaterialInterface* T = WeakTarget.Get()) { SetMaterial(T); }
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ChainRowFmt", "{0}{1}"), Prefix, FText::FromString(Target->GetName())))
				.ColorAndOpacity(bIsSelf ? FSlateColor::UseForeground() : FSlateColor(BrowserLinkColor))
			];
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildInheritance(UMaterialInterface* Material)
	{
		// Inheritance chain: root base -> ... -> this material.
		TArray<UMaterialInterface*> Chain;
		for (UMaterialInterface* Cursor = Material; Cursor; )
		{
			Chain.Insert(Cursor, 0);
			UMaterialInstance* AsInstance = Cast<UMaterialInstance>(Cursor);
			Cursor = AsInstance ? AsInstance->Parent : nullptr;
		}

		// Direct children among loaded instances.
		TArray<UMaterialInstanceConstant*> Children;
		for (TObjectIterator<UMaterialInstanceConstant> It; It; ++It)
		{
			if (It->Parent == Material)
			{
				Children.Add(*It);
			}
		}

		TSharedRef<SVerticalBox> ChainBox = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Chain.Num(); ++Index)
		{
			const FString Indent = FString::ChrN(Index * 4, TEXT(' '));
			const FText Prefix = FText::FromString(Index == 0 ? FString() : (Indent + INVTEXT("└ ").ToString()));
			ChainBox->AddSlot().AutoHeight()[ MakeMaterialLink(Chain[Index], Prefix, Chain[Index] == Material) ];
		}

		TSharedRef<SVerticalBox> ChildrenBox = SNew(SVerticalBox);
		if (Children.Num() == 0)
		{
			ChildrenBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("NoChildren", "No loaded child instances.")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
		for (UMaterialInstanceConstant* Child : Children)
		{
			ChildrenBox->AddSlot().AutoHeight()[ MakeMaterialLink(Child, INVTEXT("└ "), false) ];
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)[ MakeSectionHeader(LOCTEXT("Inheritance", "Inheritance")) ]
			+ SVerticalBox::Slot().AutoHeight()[ ChainBox ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
			[
				MakeSectionHeader(FText::Format(LOCTEXT("ChildrenHeader", "Child instances ({0})"), FText::AsNumber(Children.Num())))
			]
			+ SVerticalBox::Slot().AutoHeight()[ ChildrenBox ];
	}
}

#undef LOCTEXT_NAMESPACE
