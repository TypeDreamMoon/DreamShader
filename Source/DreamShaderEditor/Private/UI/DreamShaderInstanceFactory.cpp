#include "UI/DreamShaderInstanceFactory.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderSettings.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dialogs/DlgPickPath.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		void NotifyInstance(const FText& Message, bool bSuccess)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
			if (Item.IsValid())
			{
				Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
			}
		}

	}

	bool IsMemoryOnlyMaterial(UMaterialInterface* Material)
	{
		UPackage* Package = Material ? Material->GetPackage() : nullptr;
		return Package && Package->HasAnyPackageFlags(PKG_NewlyCreated);
	}

	UMaterialInterface* MaterializeDreamShaderMaterial(UMaterialInterface* Material, FString& OutError)
	{
		if (!IsMemoryOnlyMaterial(Material))
		{
			return Material;
		}

		UDreamShaderMaterialInstance* DreamInstance = Cast<UDreamShaderMaterialInstance>(Material);
		if (!DreamInstance || DreamInstance->SourceFilePath.IsEmpty())
		{
			OutError = LOCTEXT("MaterializeNoSource", "This material is memory-only and has no DreamShader source file to materialize from.").ToString();
			return nullptr;
		}

		const FString ObjectPath = Material->GetPathName();
		FString Message;
		if (!FMaterialGenerator::GenerateAssetsFromFile(DreamInstance->SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ false))
		{
			OutError = FText::Format(LOCTEXT("FactoryMaterializeFailed", "Failed to materialize the material to disk: {0}"), FText::FromString(Message)).ToString();
			return nullptr;
		}

		UMaterialInterface* Persisted = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
		if (!Persisted)
		{
			OutError = FText::Format(LOCTEXT("FactoryReloadFailed", "Materialized the material but could not reload it at {0}."), FText::FromString(ObjectPath)).ToString();
			return nullptr;
		}
		return Persisted;
	}

	void GetDefaultInstanceDestination(UMaterialInterface* Parent, FString& OutPackagePath, FString& OutAssetName)
	{
		OutPackagePath.Reset();
		OutAssetName.Reset();
		if (!Parent)
		{
			return;
		}

		const FString ParentPackage = FPackageName::ObjectPathToPackageName(Parent->GetPathName());
		const FString ParentDir = FPackageName::GetLongPackagePath(ParentPackage);
		const FString ParentLeaf = FPackageName::GetShortName(ParentPackage);

		const FString Subfolder = GetDefault<UDreamShaderSettings>()->InstanceSubfolder;
		OutPackagePath = Subfolder.IsEmpty() ? ParentDir : (ParentDir / Subfolder);

		const FString BasePackageName = OutPackagePath / FString::Printf(TEXT("MI_%s"), *ParentLeaf); // I18N-EXEMPT: asset name prefix, not display text
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString UniquePackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, /*Suffix*/ TEXT(""), UniquePackageName, OutAssetName);
	}

	FCreateInstanceResult CreateDreamShaderMaterialInstance(
		UMaterialInterface* Parent,
		const FString& AssetName,
		const FString& PackagePath,
		bool bOpenAfterCreate)
	{
		FCreateInstanceResult Result;

		if (!Parent)
		{
			Result.Error = LOCTEXT("NoParent", "No parent material was provided.").ToString();
			return Result;
		}
		if (AssetName.IsEmpty() || PackagePath.IsEmpty())
		{
			Result.Error = LOCTEXT("BadName", "Provide a name and a destination folder.").ToString();
			return Result;
		}

		UMaterialInterface* PersistedParent = MaterializeDreamShaderMaterial(Parent, Result.Error);
		if (!PersistedParent)
		{
			return Result;
		}

		const FString PackageName = PackagePath / AssetName;
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName); // I18N-EXEMPT: object path construction, not display text
		if (FPackageName::DoesPackageExist(PackageName) || FindObject<UObject>(nullptr, *ObjectPath))
		{
			Result.Error = FText::Format(LOCTEXT("FactoryAssetExists", "An asset already exists at {0}."), FText::FromString(PackageName)).ToString();
			return Result;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			Result.Error = FText::Format(LOCTEXT("FactoryCreatePackageFailed", "Failed to create package {0}."), FText::FromString(PackageName)).ToString();
			return Result;
		}

		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Package, *AssetName, RF_Public | RF_Standalone);
		if (!Instance)
		{
			Result.Error = LOCTEXT("NewObjectFailed", "Failed to create the material instance object.").ToString();
			return Result;
		}

		Instance->SetParentEditorOnly(PersistedParent);
		Instance->PostEditChange();

		FAssetRegistryModule::AssetCreated(Instance);
		Instance->MarkPackageDirty();

		FString SaveError;
		if (!SaveAssetPackage(Instance, SaveError))
		{
			// Roll back the half-created asset. Left as-is it would be a dirty, AssetRegistry-registered,
			// RF_Standalone object that survives GC: it would show up in the Content Browser, could be
			// silently persisted by Save-All / editor exit, and -- because the collision guard above uses
			// FindObject on the object path -- would block a retry with the same name. Renaming it into the
			// transient package frees the target path and lets GC reclaim it.
			FAssetRegistryModule::AssetDeleted(Instance);
			Instance->ClearFlags(RF_Public | RF_Standalone);
			Instance->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
			Instance->MarkAsGarbage();
			if (Package)
			{
				Package->SetDirtyFlag(false);
			}
			Result.Error = SaveError;
			return Result;
		}

		if (bOpenAfterCreate && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Instance);
		}

		Result.bSucceeded = true;
		Result.Instance = Instance;
		return Result;
	}

	void OpenCreateInstanceDialog(UMaterialInterface* Parent)
	{
		if (!Parent)
		{
			NotifyInstance(LOCTEXT("SelectFirst", "Select a material to create an instance of."), false);
			return;
		}

		FString DefaultPath;
		FString DefaultName;
		GetDefaultInstanceDestination(Parent, DefaultPath, DefaultName);

		// Field state shared with the dialog's widgets and its Create handler.
		TSharedRef<FString> NameValue = MakeShared<FString>(DefaultName);
		TSharedRef<FString> PathValue = MakeShared<FString>(DefaultPath);
		TSharedRef<bool> OpenAfterValue = MakeShared<bool>(true);

		// Built up front so the Browse handler can capture a valid (already-assigned) pointer.
		TSharedRef<SEditableTextBox> PathBox = SNew(SEditableTextBox)
			.Text(FText::FromString(*PathValue))
			.OnTextChanged_Lambda([PathValue](const FText& NewText) { *PathValue = NewText.ToString(); });

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateInstanceTitle", "Create material instance"))
			.ClientSize(FVector2D(480.0f, 240.0f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		const auto CloseWindow = [Window]()
		{
			Window->RequestDestroyWindow();
		};

		Window->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(16.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SGridPanel)
					.FillColumn(1, 1.0f)

					+ SGridPanel::Slot(0, 0).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ParentLabel", "Parent"))
					]
					+ SGridPanel::Slot(1, 0).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Parent->GetName()))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SGridPanel::Slot(0, 1).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NameLabel", "Name"))
					]
					+ SGridPanel::Slot(1, 1).Padding(4.0f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(*NameValue))
						.OnTextChanged_Lambda([NameValue](const FText& NewText) { *NameValue = NewText.ToString(); })
					]

					+ SGridPanel::Slot(0, 2).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("PathLabel", "Folder"))
					]
					+ SGridPanel::Slot(1, 2).Padding(4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							PathBox
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("Browse", "Browse..."))
							.ToolTipText(LOCTEXT("BrowseTip", "Pick the destination folder."))
							.OnClicked_Lambda([PathValue, PathBox]()
							{
								TSharedRef<SDlgPickPath> PickPath = SNew(SDlgPickPath)
									.Title(LOCTEXT("PickFolderTitle", "Choose a destination folder"))
									.DefaultPath(FText::FromString(*PathValue));
								if (PickPath->ShowModal() == EAppReturnType::Ok)
								{
									*PathValue = PickPath->GetPath().ToString();
									PathBox->SetText(FText::FromString(*PathValue));
								}
								return FReply::Handled();
							})
						]
					]

					+ SGridPanel::Slot(1, 3).Padding(4.0f)
					[
						SNew(SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						.OnCheckStateChanged_Lambda([OpenAfterValue](ECheckBoxState State) { *OpenAfterValue = (State == ECheckBoxState::Checked); })
						[
							SNew(STextBlock).Text(LOCTEXT("OpenAfter", "Open the instance after creating"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SSpacer)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([CloseWindow]() { CloseWindow(); return FReply::Handled(); })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.Text(LOCTEXT("Create", "Create"))
						.OnClicked_Lambda([WeakParent = TWeakObjectPtr<UMaterialInterface>(Parent), NameValue, PathValue, OpenAfterValue, CloseWindow]()
						{
							UMaterialInterface* ParentPtr = WeakParent.Get();
							if (!ParentPtr)
							{
								NotifyInstance(LOCTEXT("ParentGone", "The parent material is no longer available."), false);
								CloseWindow();
								return FReply::Handled();
							}
							const FCreateInstanceResult Outcome = CreateDreamShaderMaterialInstance(
								ParentPtr, *NameValue, *PathValue, *OpenAfterValue);
							if (Outcome.bSucceeded)
							{
								NotifyInstance(
									FText::Format(LOCTEXT("InstanceCreated", "Created {0}"), FText::FromString(*NameValue)),
									true);
								CloseWindow();
							}
							else
							{
								NotifyInstance(FText::FromString(Outcome.Error), false);
							}
							return FReply::Handled();
						})
					]
				]
			]);

		GEditor->EditorAddModalWindow(Window);
	}
}

#undef LOCTEXT_NAMESPACE
