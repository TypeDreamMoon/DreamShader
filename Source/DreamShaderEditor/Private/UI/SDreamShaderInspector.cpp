// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderInspector.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "Provenance/DreamShaderProvenanceActions.h"
#include "UI/DreamShaderBrowserActions.h"
#include "UI/DreamShaderBrowserStyle.h"
#include "UI/DreamShaderBrowserUserSettings.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "UI/SDreamShaderLivePreview.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetThumbnail.h"
#include "DreamShaderMaterialInstance.h"
#include "Materials/MaterialFunction.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
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
#include "Widgets/Input/SComboBox.h"
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

		for (const TCHAR* MeshName : { TEXT("sphere"), TEXT("plane"), TEXT("cube"), TEXT("cylinder"), TEXT("shaderball") })
		{
			MeshOptions.Add(MakeShared<FString>(MeshName));
		}
		TSharedPtr<FString> InitialMesh = MeshOptions[0];
		for (const TSharedPtr<FString>& Option : MeshOptions)
		{
			if (*Option == UDreamShaderBrowserUserSettings::Get()->PreviewMesh)
			{
				InitialMesh = Option;
			}
		}

		LivePreview = SNew(SDreamShaderLivePreview).Size(256);
		LivePreview->SetMesh(*InitialMesh);

		MeshPicker = SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&MeshOptions)
			.InitiallySelectedItem(InitialMesh)
			.ToolTipText(LOCTEXT("PreviewMeshTip", "The shape the preview renders the material on."))
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option) { return SNew(STextBlock).Text(FText::FromString(*Option)); })
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
			{
				if (Option.IsValid())
				{
					UDreamShaderBrowserUserSettings::Get()->PreviewMesh = *Option;
					UDreamShaderBrowserUserSettings::Get()->Save();
					LivePreview->SetMesh(*Option);
				}
			})
			[
				SNew(STextBlock).Text_Lambda([this]()
				{
					const TSharedPtr<FString> Selected = MeshPicker.IsValid() ? MeshPicker->GetSelectedItem() : nullptr;
					return Selected.IsValid() ? FText::FromString(*Selected) : FText::GetEmpty();
				})
			];

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
			// Release the persistent preview widget from the old tree before the new one claims it.
			ContentContainer->SetContent(SNullWidget::NullWidget);
			ContentContainer->SetContent(BuildContent());
		}
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildContent()
	{
		if (!Entry.IsValid())
		{
			Thumbnail.Reset();
			PreviewedObjectPath.Reset();
			return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InspectorEmpty", "Select a source file or a material to inspect it."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}

		UMaterialInterface* Material = Entry->ResolveMaterial();

		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		if (Material)
		{
			Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ BuildPreview(Material) ];
		}
		Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ BuildHeader(Material) ];
		Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[ BuildActions(Material) ];
		Body->AddSlot().AutoHeight()[ BuildInfoRows(Material) ];
		Body->AddSlot().AutoHeight()[ BuildDiagnostics() ];
		Body->AddSlot().AutoHeight()[ BuildProvenance() ];
		Body->AddSlot().AutoHeight()[ BuildDependencies() ];
		if (Material)
		{
			Body->AddSlot().AutoHeight()[ BuildInheritance(Material) ];
		}

		return SNew(SScrollBox) + SScrollBox::Slot()[ Body ];
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildHeader(UMaterialInterface* Material)
	{
		// A placeholder tile when there is no compiled material to render; with one, the live preview
		// above stands in for the thumbnail.
		Thumbnail.Reset();
		TSharedRef<SWidget> ThumbWidget = SNullWidget::NullWidget;
		if (!Material)
		{
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
			if (Entry->Asset->bOpenInEditor)
			{
				Title->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OpenInEditorBadge", "open in an asset editor — a rebuild will refuse until it is closed"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.62f, 0.12f)))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.AutoWrapText(true)
				];
			}
		}

		TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
		if (!Material)
		{
			Header->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)[ ThumbWidget ]
			];
		}
		Header->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)[ Title ];
		return Header;
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildPreview(UMaterialInterface* Material)
	{
		// Only re-render when the material changed or the model moved under it; re-slotting alone
		// keeps the last frame.
		const FString ObjectPath = Material->GetPathName();
		if (PreviewedObjectPath != ObjectPath)
		{
			PreviewedObjectPath = ObjectPath;
			LivePreview->SetMaterial(Material);
		}
		else
		{
			LivePreview->Refresh();
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(FMargin(0.0f))
				[
					SNew(SBox).WidthOverride(224.0f).HeightOverride(224.0f)[ LivePreview.ToSharedRef() ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("PreviewMeshLabel", "Mesh")).ColorAndOpacity(FSlateColor::UseSubduedForeground()).TextStyle(FAppStyle::Get(), "SmallText")
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MeshPicker.ToSharedRef()
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("PreviewDragHint", "drag to orbit")).ColorAndOpacity(FSlateColor::UseSubduedForeground()).TextStyle(FAppStyle::Get(), "SmallText")
				]
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
		const FBrowserSourceInfo& Source = *Entry->Source;
		const TSharedPtr<FBrowserEntry> EntryRef = Entry;

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
		[
			MakeSectionHeader(FText::Format(LOCTEXT("DiagnosticsHeader", "Diagnostics ({0})"), FText::AsNumber(FMath::Max(1, Source.Diagnostics.Num()))))
		];

		if (Source.Diagnostics.Num() == 0)
		{
			// A failure this browser pinned itself (no bridge record): the message, and nothing to jump to.
			Box->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(Source.StatusDetail).ColorAndOpacity(FSlateColor(BrowserErrorColor)).AutoWrapText(true)
			];
			return Box;
		}

		// Line/column are identifiers, not quantities: no digit grouping.
		FNumberFormattingOptions LineNumberOptions;
		LineNumberOptions.UseGrouping = false;

		for (const FDreamShaderDiagnosticRecord& Record : Source.Diagnostics)
		{
			const int32 Line = FMath::Max(1, Record.Line);
			const int32 Column = FMath::Max(1, Record.Column);
			const FText Location = Record.Code.IsEmpty()
				? FText::Format(LOCTEXT("DiagLocationFmt", "L{0}:{1}"), FText::AsNumber(Line, &LineNumberOptions), FText::AsNumber(Column, &LineNumberOptions))
				: FText::Format(LOCTEXT("DiagLocationCodeFmt", "[{2}] L{0}:{1}"), FText::AsNumber(Line, &LineNumberOptions), FText::AsNumber(Column, &LineNumberOptions), FText::FromString(Record.Code));
			const bool bIsError = Record.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
			// A diagnostic can point into an imported header rather than the file itself.
			const FString TargetFile = Record.FilePath.IsEmpty() ? Source.FilePath : Record.FilePath;

			Box->AddSlot().AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(0.0f, 2.0f))
				.HAlign(HAlign_Left)
				.ToolTipText(FText::Format(LOCTEXT("DiagJumpTip", "Open {0} at this line in your editor."), FText::FromString(FPaths::GetCleanFilename(TargetFile))))
				.OnClicked_Lambda([TargetFile, Line, Column]()
				{
					FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(TargetFile, Line, Column);
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(Location).ColorAndOpacity(FSlateColor(BrowserLinkColor)).TextStyle(FAppStyle::Get(), "SmallText")
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(Record.Message)
						.ColorAndOpacity(bIsError ? FSlateColor(BrowserErrorColor) : FSlateColor::UseForeground())
						.AutoWrapText(true)
					]
				]
			];
		}
		return Box;
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildProvenance()
	{
		if (!Entry->Asset.IsSet())
		{
			return SNullWidget::NullWidget;
		}
		const FBrowserAssetInfo& Asset = *Entry->Asset;
		const FString ObjectPath = Asset.ObjectPath;
		const bool bWritableSource = !Entry->Source.IsSet() || Entry->Source->bWritableRoot;
		const bool bDiverged = Asset.Provenance == EDreamShaderDigestState::Diverged;

		FText Explanation;
		switch (Asset.Provenance)
		{
		case EDreamShaderDigestState::Generated:
			Explanation = LOCTEXT("ProvenanceExplainGenerated", "The asset holds exactly what DreamShader last generated into it. A source change rebuilds it freely.");
			break;
		case EDreamShaderDigestState::Diverged:
			Explanation = LOCTEXT("ProvenanceExplainDiverged", "The asset was edited by hand since it was generated, so a rebuild is refused to protect those edits. Decide which copy is the truth.");
			break;
		case EDreamShaderDigestState::Unstamped:
			Explanation = LOCTEXT("ProvenanceExplainUnstamped", "Generated by DreamShader, but carrying no digest this version can compare. The next rebuild restamps it.");
			break;
		default:
			Explanation = LOCTEXT("ProvenanceExplainForeign", "Not generated by DreamShader. Export it to a source file to bring it under DreamShader's management.");
			break;
		}

		const auto FindAsset = [ObjectPath]() -> UObject* { return FindObject<UObject>(nullptr, *ObjectPath); };
		TSharedRef<SWrapBox> Buttons = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
		if (Asset.Provenance == EDreamShaderDigestState::Foreign)
		{
			UObject* Object = FindAsset();
			if (UMaterial* Material = Cast<UMaterial>(Object))
			{
				const TWeakObjectPtr<UMaterial> WeakMaterial(Material);
				Buttons->AddSlot()[ MakeActionButton(
					LOCTEXT("ExportDsmBtn", "Export DSM"),
					LOCTEXT("ExportDsmTip", "Decompile this material into a .dsm source file under the project's DShader root."),
					[WeakMaterial]()
					{
						if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge()) { Bridge->ExportMaterialToDreamShaderFile(WeakMaterial); }
					}) ];
			}
			else if (UMaterialFunction* Function = Cast<UMaterialFunction>(Object))
			{
				const TWeakObjectPtr<UMaterialFunction> WeakFunction(Function);
				Buttons->AddSlot()[ MakeActionButton(
					LOCTEXT("ExportDsfBtn", "Export DSF"),
					LOCTEXT("ExportDsfTip", "Decompile this material function into a .dsf source file under the project's DShader root."),
					[WeakFunction]()
					{
						if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge()) { Bridge->ExportMaterialFunctionToDreamShaderFile(WeakFunction); }
					}) ];
			}
		}
		else
		{
			Buttons->AddSlot()[ MakeActionButton(
				bDiverged ? LOCTEXT("RevertDivergedBtn", "Revert to Source (discards edits)") : LOCTEXT("RevertBtn", "Revert to Source"),
				LOCTEXT("RevertTip", "Rebuild this asset from its DreamShader source, discarding every hand edit in it. The source file is not modified."),
				[FindAsset]() { if (UObject* Object = FindAsset()) { RevertGeneratedAssetToSource(Object); } },
				/*bPrimary*/ bDiverged) ];
			TSharedRef<SWidget> AdoptButton = MakeActionButton(
				LOCTEXT("AdoptBtn", "Adopt Into Source"),
				bWritableSource
					? LOCTEXT("AdoptTip", "Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it.")
					: LOCTEXT("AdoptReadOnlyTip", "This asset's source ships with a plugin and is read-only; adopt is not available."),
				[FindAsset]() { if (UObject* Object = FindAsset()) { AdoptGeneratedAssetIntoSource(Object); } });
			AdoptButton->SetEnabled(bWritableSource);
			Buttons->AddSlot()[ AdoptButton ];
			const TWeakPtr<FDreamShaderBrowserModel> WeakModel = Model;
			Buttons->AddSlot()[ MakeActionButton(
				LOCTEXT("DetachBtn", "Detach"),
				LOCTEXT("DetachTip", "Keep this asset exactly as it is and stop DreamShader from ever rebuilding it."),
				[FindAsset, WeakModel]()
				{
					if (UObject* Object = FindAsset())
					{
						DetachGeneratedAssetFromDreamShader(Object);
						if (TSharedPtr<FDreamShaderBrowserModel> M = WeakModel.Pin()) { M->RefreshStatuses(); }
					}
				}) ];
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)[ MakeSectionHeader(LOCTEXT("ProvenanceHeader", "Provenance")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(GetBrowserProvenanceLabel(Asset.Provenance))
				.ColorAndOpacity(bDiverged ? FSlateColor(FLinearColor(0.85f, 0.45f, 0.15f)) : FSlateColor::UseForeground())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock).Text(Explanation).ColorAndOpacity(FSlateColor::UseSubduedForeground()).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight()[ Buttons ];
	}

	TSharedRef<SWidget> SDreamShaderInspector::MakeSourceLink(const FString& SourceFilePath)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(0.0f, 1.0f))
			.HAlign(HAlign_Left)
			.ToolTipText(FText::FromString(SourceFilePath))
			.OnClicked_Lambda([this, SourceFilePath]()
			{
				OnNavigateToSource.ExecuteIfBound(SourceFilePath);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::Format(INVTEXT("└ {0}"), FText::FromString(FPaths::GetCleanFilename(SourceFilePath))))
				.ColorAndOpacity(FSlateColor(BrowserLinkColor))
			];
	}

	TSharedRef<SWidget> SDreamShaderInspector::BuildDependencies()
	{
		if (!Entry->Source.IsSet())
		{
			return SNullWidget::NullWidget;
		}
		const FBrowserSourceInfo& Source = *Entry->Source;
		if (Source.Imports.Num() == 0 && Source.Dependents.Num() == 0)
		{
			return SNullWidget::NullWidget;
		}

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		if (Source.Imports.Num() > 0)
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
			[
				MakeSectionHeader(FText::Format(LOCTEXT("ImportsHeader", "Imports ({0})"), FText::AsNumber(Source.Imports.Num())))
			];
			for (const FString& Import : Source.Imports)
			{
				Box->AddSlot().AutoHeight()[ MakeSourceLink(Import) ];
			}
		}
		if (Source.Dependents.Num() > 0)
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
			[
				MakeSectionHeader(FText::Format(LOCTEXT("DependentsHeader", "Used by ({0})"), FText::AsNumber(Source.Dependents.Num())))
			];
			for (const FString& Dependent : Source.Dependents)
			{
				Box->AddSlot().AutoHeight()[ MakeSourceLink(Dependent) ];
			}
		}
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

		// Direct children: every loaded instance whose parent this is, plus -- from the asset registry
		// -- every saved instance that references this package, which the loaded set misses when
		// the project has not loaded them this session. A material instance's only material reference
		// is its parent, so a referencing instance is a child.
		TArray<UMaterialInstanceConstant*> Children;
		for (TObjectIterator<UMaterialInstanceConstant> It; It; ++It)
		{
			if (It->Parent == Material)
			{
				Children.Add(*It);
			}
		}
		TArray<FAssetData> UnloadedChildren;
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			TArray<FName> Referencers;
			AssetRegistry->GetReferencers(Material->GetPackage()->GetFName(), Referencers);
			for (const FName& Referencer : Referencers)
			{
				TArray<FAssetData> PackageAssets;
				AssetRegistry->GetAssetsByPackageName(Referencer, PackageAssets);
				for (const FAssetData& AssetData : PackageAssets)
				{
					const UClass* AssetClass = AssetData.GetClass();
					if (AssetClass && AssetClass->IsChildOf(UMaterialInstanceConstant::StaticClass()) && !AssetData.IsAssetLoaded())
					{
						UnloadedChildren.Add(AssetData);
					}
				}
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
		if (Children.Num() == 0 && UnloadedChildren.Num() == 0)
		{
			ChildrenBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("NoChildrenAnywhere", "No child instances.")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
		for (UMaterialInstanceConstant* Child : Children)
		{
			ChildrenBox->AddSlot().AutoHeight()[ MakeMaterialLink(Child, INVTEXT("└ "), false) ];
		}
		for (const FAssetData& ChildData : UnloadedChildren)
		{
			// Clicking loads it, and the panel re-targets to the now-loaded object.
			const FString ChildPath = ChildData.GetObjectPathString();
			ChildrenBox->AddSlot().AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.HAlign(HAlign_Left)
				.ToolTipText(FText::Format(LOCTEXT("UnloadedChildTip", "{0} (not loaded — click to load)"), FText::FromString(ChildPath)))
				.OnClicked_Lambda([this, ChildPath]()
				{
					if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, *ChildPath)) { SetMaterial(Loaded); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("UnloadedChildFmt", "└ {0} (not loaded)"), FText::FromName(ChildData.AssetName)))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			];
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)[ MakeSectionHeader(LOCTEXT("Inheritance", "Inheritance")) ]
			+ SVerticalBox::Slot().AutoHeight()[ ChainBox ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
			[
				MakeSectionHeader(FText::Format(LOCTEXT("ChildrenHeader", "Child instances ({0})"), FText::AsNumber(Children.Num() + UnloadedChildren.Num())))
			]
			+ SVerticalBox::Slot().AutoHeight()[ ChildrenBox ];
	}
}

#undef LOCTEXT_NAMESPACE
