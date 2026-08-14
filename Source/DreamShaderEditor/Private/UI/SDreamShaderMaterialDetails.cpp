#include "UI/SDreamShaderMaterialDetails.h"

#include "UI/DreamShaderInstanceFactory.h"

#include "AssetThumbnail.h"
#include "DreamShaderMaterialInstance.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "SceneTypes.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		void NotifyDetails(const FText& Message, bool bSuccess)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
			if (Item.IsValid())
			{
				Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
			}
		}

		FText EnumText(UEnum* Enum, int64 Value)
		{
			return Enum ? Enum->GetDisplayNameTextByValue(Value) : FText::GetEmpty();
		}

		TSharedRef<SWidget> MakeInfoRow(const FText& Label, const FText& Value)
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
					SNew(STextBlock).Text(Value).AutoWrapText(true)
				];
		}
	}

	void SDreamShaderMaterialDetails::Construct(const FArguments& InArgs)
	{
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(16);

		// The pool renders on Tick; drive it from a Slate active timer while this panel is alive.
		RegisterActiveTimer(0.05f, FWidgetActiveTimerDelegate::CreateLambda(
			[WeakPool = TWeakPtr<FAssetThumbnailPool>(ThumbnailPool)](double, float DeltaTime)
			{
				if (TSharedPtr<FAssetThumbnailPool> Pool = WeakPool.Pin())
				{
					Pool->Tick(DeltaTime);
				}
				return EActiveTimerReturnType::Continue;
			}));

		ChildSlot
		[
			SAssignNew(ContentContainer, SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(FMargin(12.0f))
		];
		Rebuild();
	}

	void SDreamShaderMaterialDetails::SetMaterial(UMaterialInterface* InMaterial)
	{
		CurrentMaterial = InMaterial;
		Rebuild();
	}

	void SDreamShaderMaterialDetails::Rebuild()
	{
		if (ContentContainer.IsValid())
		{
			ContentContainer->SetContent(BuildContent(CurrentMaterial.Get()));
		}
	}

	TSharedRef<SWidget> SDreamShaderMaterialDetails::BuildContent(UMaterialInterface* Material)
	{
		if (!Material)
		{
			return SNew(STextBlock)
				.Text(LOCTEXT("NoSelection", "Select a material to see its inheritance and settings."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

		UMaterial* Base = Material->GetBaseMaterial();
		const bool bInMemory = IsMemoryOnlyMaterial(Material);
		const TWeakObjectPtr<UMaterialInterface> WeakMaterial(Material);

		CurrentThumbnail = MakeShared<FAssetThumbnail>(Material, 96, 96, ThumbnailPool);

		// Source file, when this is a DreamShader material.
		FText SourceText = LOCTEXT("SourceNone", "-");
		if (UDreamShaderMaterialInstance* DreamInstance = Cast<UDreamShaderMaterialInstance>(Material))
		{
			if (!DreamInstance->SourceFilePath.IsEmpty())
			{
				SourceText = FText::FromString(DreamInstance->SourceFilePath);
			}
		}

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

		const auto MakeMaterialLink = [this](UMaterialInterface* Target, const FText& Prefix, bool bIsSelf) -> TSharedRef<SWidget>
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
					.ColorAndOpacity(bIsSelf ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(0.35f, 0.55f, 0.85f)))
				];
		};

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

		return SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// Header: thumbnail + name.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)
						[
							CurrentThumbnail->MakeThumbnailWidget()
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Material->GetName()))
						.TextStyle(FAppStyle::Get(), "LargeText")
						.AutoWrapText(true)
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.Text(LOCTEXT("CreateInstanceBtn", "Create instance"))
						.OnClicked_Lambda([WeakMaterial]()
						{
							if (UMaterialInterface* M = WeakMaterial.Get()) { OpenCreateInstanceDialog(M); }
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("OpenBtn", "Open"))
						.OnClicked_Lambda([WeakMaterial]()
						{
							if (UMaterialInterface* M = WeakMaterial.Get())
							{
								if (GEditor) { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(M); }
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("MaterializeBtn", "Materialize"))
						.ToolTipText(LOCTEXT("MaterializeTip", "Write this memory-only material (and its base) to disk."))
						.Visibility(bInMemory ? EVisibility::Visible : EVisibility::Collapsed)
						.OnClicked_Lambda([this, WeakMaterial]()
						{
							UMaterialInterface* M = WeakMaterial.Get();
							if (!M)
							{
								return FReply::Handled();
							}
							FString Error;
							UMaterialInterface* Persisted = MaterializeDreamShaderMaterial(M, Error);
							if (Persisted)
							{
								NotifyDetails(FText::Format(LOCTEXT("Materialized", "Materialized {0} to disk"), FText::FromString(M->GetName())), true);
								SetMaterial(Persisted);
							}
							else
							{
								NotifyDetails(FText::FromString(Error), false);
							}
							return FReply::Handled();
						})
					]
				]

				// Surface settings from the base material.
				+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("Base", "Base"), Base ? FText::FromString(Base->GetName()) : LOCTEXT("BaseNone", "-")) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("Domain", "Domain"), Base ? EnumText(StaticEnum<EMaterialDomain>(), static_cast<int64>(Base->MaterialDomain.GetValue())) : FText::GetEmpty()) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("Blend", "Blend mode"), Base ? EnumText(StaticEnum<EBlendMode>(), static_cast<int64>(Base->BlendMode.GetValue())) : FText::GetEmpty()) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("Storage", "Storage"), bInMemory ? LOCTEXT("InMemory", "memory-only (not saved)") : LOCTEXT("OnDisk", "on disk")) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("SourceRow", "Source"), SourceText) ]

				// Inheritance chain.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Inheritance", "Inheritance")).TextStyle(FAppStyle::Get(), "LargeText")
				]
				+ SVerticalBox::Slot().AutoHeight()[ ChainBox ]

				// Children.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("ChildrenHeader", "Child instances ({0})"), FText::AsNumber(Children.Num())))
					.TextStyle(FAppStyle::Get(), "LargeText")
				]
				+ SVerticalBox::Slot().AutoHeight()[ ChildrenBox ]
			];
	}
}

#undef LOCTEXT_NAMESPACE
