#include "UI/DreamShaderMaterialBrowser.h"

#include "UI/DreamShaderInstanceFactory.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "UI/SDreamShaderGenPage.h"
#include "UI/SDreamShaderProjectPage.h"

#include "ContentBrowserMenuContexts.h"
#include "DreamShaderMaterialInstance.h"
#include "Framework/Docking/TabManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	const FName FDreamShaderMaterialBrowser::TabId(TEXT("DreamShaderMaterialBrowser"));

	namespace
	{
		const FName GDreamShaderBrowserMenuOwner(TEXT("DreamShaderMaterialBrowser"));
		FDelegateHandle GMenuStartupHandle;

		TSharedRef<SDockTab> SpawnMaterialBrowserTab(const FSpawnTabArgs& Args)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SDreamShaderMaterialBrowser)
				];
		}

		// Content Browser right-click on a material instance -> "Create DreamShader instance".
		void PopulateInstanceCreateMenu(FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
			if (!Context || Context->SelectedAssets.Num() != 1)
			{
				return;
			}
			UMaterialInterface* Material = Cast<UMaterialInterface>(Context->SelectedAssets[0].GetAsset());
			if (!Material)
			{
				return;
			}
			InSection.AddMenuEntry(
				"DreamShader.CreateInstance",
				LOCTEXT("CBCreateInstance", "Create DreamShader instance"),
				LOCTEXT("CBCreateInstanceTip", "Create a material instance that shares this material's compiled shader map."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MaterialInstanceConstant"),
				FUIAction(FExecuteAction::CreateLambda([WeakMaterial = TWeakObjectPtr<UMaterialInterface>(Material)]()
				{
					if (UMaterialInterface* M = WeakMaterial.Get()) { OpenCreateInstanceDialog(M); }
				})));
		}

		void ExtendInstanceContextMenu(UClass* AssetClass)
		{
			if (UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(AssetClass))
			{
				FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
				Section.AddDynamicEntry(
					TEXT("DreamShader.InstanceCreateActions"),
					FNewToolMenuSectionDelegate::CreateStatic(&PopulateInstanceCreateMenu));
			}
		}
	}

	void FDreamShaderMaterialBrowser::Register()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
				TabId,
				FOnSpawnTab::CreateStatic(&SpawnMaterialBrowserTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Material Content Browser"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Browse, manage, and create instances of project and DreamShader-generated materials."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material"));

		GMenuStartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScope(GDreamShaderBrowserMenuOwner);
			if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
			{
				FToolMenuSection& Section = ToolsMenu->FindOrAddSection("DreamShader");
				Section.AddMenuEntry(
					"OpenMaterialContentBrowser",
					LOCTEXT("OpenTabLabel", "Material Content Browser"),
					LOCTEXT("OpenTabTooltip", "Open the DreamShader Material Content Browser."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material"),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(FDreamShaderMaterialBrowser::TabId);
					})));
			}

			// Menu inheritance does not auto-propagate to subclasses (the CB menu name is per exact class),
			// so extend both the stock instance class and the DreamShader subclass.
			ExtendInstanceContextMenu(UMaterialInstanceConstant::StaticClass());
			ExtendInstanceContextMenu(UDreamShaderMaterialInstance::StaticClass());
		}));
	}

	void FDreamShaderMaterialBrowser::Unregister()
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
		}

		if (GMenuStartupHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(GMenuStartupHandle);
			GMenuStartupHandle.Reset();
		}
		if (UObjectInitialized())
		{
			UToolMenus::UnregisterOwner(GDreamShaderBrowserMenuOwner);
		}
	}

	void SDreamShaderMaterialBrowser::Construct(const FArguments& InArgs)
	{
		Model = MakeShared<FDreamShaderBrowserModel>();

		const auto MakePageToggle = [this](int32 PageIndex, const FText& Label) -> TSharedRef<SWidget>
		{
			return SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.Padding(FMargin(14.0f, 4.0f))
				.IsChecked(this, &SDreamShaderMaterialBrowser::GetPageCheckState, PageIndex)
				.OnCheckStateChanged(this, &SDreamShaderMaterialBrowser::OnPageCheckStateChanged, PageIndex)
				[
					SNew(STextBlock)
					.Text(Label)
				];
		};

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
				.Padding(FMargin(8.0f, 6.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						MakePageToggle(0, LOCTEXT("ProjectPage", "Project"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						MakePageToggle(1, LOCTEXT("GenPage", "Dream Shader Gen"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(PageSwitcher, SWidgetSwitcher)
				.WidgetIndex(ActivePageIndex)

				+ SWidgetSwitcher::Slot()
				[
					SNew(SDreamShaderProjectPage)
					.Model(Model)
				]

				+ SWidgetSwitcher::Slot()
				[
					SNew(SDreamShaderGenPage)
					.Model(Model)
				]
			]
		];

		// After both pages exist, so each gets the first OnChanged.
		Model->RefreshAll();
	}

	void SDreamShaderMaterialBrowser::SetActivePage(int32 PageIndex)
	{
		ActivePageIndex = PageIndex;
		if (PageSwitcher.IsValid())
		{
			PageSwitcher->SetActiveWidgetIndex(PageIndex);
		}
	}

	ECheckBoxState SDreamShaderMaterialBrowser::GetPageCheckState(int32 PageIndex) const
	{
		return ActivePageIndex == PageIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void SDreamShaderMaterialBrowser::OnPageCheckStateChanged(ECheckBoxState NewState, int32 PageIndex)
	{
		// Radio behavior: only respond to a fresh selection; re-clicking the active page is a no-op.
		if (NewState == ECheckBoxState::Checked)
		{
			SetActivePage(PageIndex);
		}
	}
}

#undef LOCTEXT_NAMESPACE
