#include "UI/SDreamShaderProjectPage.h"

#include "UI/DreamShaderInstanceFactory.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "UI/SDreamShaderInspector.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderSettings.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		void OpenAssetForEditing(const FAssetData& AssetData)
		{
			if (!GEditor)
			{
				return;
			}
			if (UObject* Asset = AssetData.GetAsset())
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
			}
		}

		// Flip the global "show in-memory materials" setting and broadcast create/delete for every
		// memory-only DreamShader instance so the asset registry (and this picker) add or drop their tiles
		// immediately -- the instances are otherwise hidden via UDreamShaderMaterialInstance::IsAsset().
		// Mirrors the Tools-menu toggle; the setting is global, so it affects the main Content Browser too.
		void SetShowInMemoryMaterials(bool bShow)
		{
			UDreamShaderSettings* Settings = GetMutableDefault<UDreamShaderSettings>();
			if (Settings->bShowInMemoryMaterialsInContentBrowser == bShow)
			{
				return;
			}
			Settings->bShowInMemoryMaterialsInContentBrowser = bShow;
			Settings->TryUpdateDefaultConfigFile();

			for (TObjectIterator<UDreamShaderMaterialInstance> It; It; ++It)
			{
				UDreamShaderMaterialInstance* Instance = *It;
				if (Instance->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
				{
					if (bShow)
					{
						FAssetRegistryModule::AssetCreated(Instance);
					}
					else
					{
						FAssetRegistryModule::AssetDeleted(Instance);
					}
				}
			}
		}
	}

	void SDreamShaderProjectPage::Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		check(Model.IsValid());

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		FAssetPickerConfig PickerConfig;
		PickerConfig.Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		PickerConfig.Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
		// Recursive so UDreamShaderMaterialInstance (a UMaterialInstanceConstant subclass) is included.
		PickerConfig.Filter.bRecursiveClasses = true;
		PickerConfig.Filter.PackagePaths.Add("/Game");
		PickerConfig.Filter.bRecursivePaths = true;
		PickerConfig.InitialAssetViewType = EAssetViewType::Tile;
		PickerConfig.bAllowDragging = true;
		PickerConfig.bCanShowClasses = false;
		PickerConfig.bForceShowEngineContent = false;
		PickerConfig.bShowPathInColumnView = true;
		PickerConfig.SelectionMode = ESelectionMode::Single;
		PickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SDreamShaderProjectPage::OnAssetSelected);
		PickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateStatic(&OpenAssetForEditing);
		PickerConfig.AssetShowWarningText = LOCTEXT("EmptyProject", "No materials found under /Game.");

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(8.0f, 5.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.Text(LOCTEXT("CreateInstance", "Create instance"))
						.ToolTipText(LOCTEXT("CreateInstanceTip", "Create a material instance of the selected material (shares its compiled shader map)."))
						.OnClicked(this, &SDreamShaderProjectPage::OnCreateInstanceClicked)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text_Lambda([this]()
						{
							return SelectedAsset.IsValid()
								? FText::Format(LOCTEXT("SelectedFmt", "Selected: {0}"), FText::FromName(SelectedAsset.AssetName))
								: LOCTEXT("NoneSelected", "Select a material, then create an instance.");
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GetDefault<UDreamShaderSettings>()->bShowInMemoryMaterialsInContentBrowser ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState State) { SetShowInMemoryMaterials(State == ECheckBoxState::Checked); })
						.ToolTipText(LOCTEXT("ShowInMemoryTip", "Show DreamShader's memory-only materials here and in the Content Browser (global setting)."))
						[
							SNew(STextBlock).Text(LOCTEXT("ShowInMemory", "Show in-memory materials"))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.62f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
				]

				+ SSplitter::Slot()
				.Value(0.38f)
				[
					SAssignNew(Inspector, SDreamShaderInspector)
					.Model(Model)
				]
			]
		];
	}

	void SDreamShaderProjectPage::OnAssetSelected(const FAssetData& AssetData)
	{
		SelectedAsset = AssetData;
		if (Inspector.IsValid())
		{
			Inspector->SetMaterial(Cast<UMaterialInterface>(AssetData.GetAsset()));
		}
	}

	FReply SDreamShaderProjectPage::OnCreateInstanceClicked()
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(SelectedAsset.GetAsset()))
		{
			OpenCreateInstanceDialog(Material);
		}
		else
		{
			OpenCreateInstanceDialog(nullptr); // shows the "select a material first" toast
		}
		return FReply::Handled();
	}
}

#undef LOCTEXT_NAMESPACE
