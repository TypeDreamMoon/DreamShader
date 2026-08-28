#include "UI/DreamShaderMaterialBrowser.h"

#include "UI/DreamShaderInstanceFactory.h"
#include "UI/SDreamShaderBrowserShell.h"

#include "ContentBrowserMenuContexts.h"
#include "DreamShaderMaterialInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
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

		TWeakPtr<SDreamShaderBrowserShell> GActiveShell;

		TSharedRef<SDockTab> SpawnMaterialBrowserTab(const FSpawnTabArgs& Args)
		{
			TSharedRef<SDreamShaderBrowserShell> Shell = SNew(SDreamShaderBrowserShell);
			GActiveShell = Shell;
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					Shell
				];
		}

		TSharedPtr<SDreamShaderBrowserShell> InvokeShell()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FDreamShaderMaterialBrowser::TabId);
			return GActiveShell.Pin();
		}

		// Content Browser right-click on a material -> "Show in Material Content Browser".
		void PopulateShowInBrowserMenu(FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
			if (!Context || Context->SelectedAssets.Num() != 1)
			{
				return;
			}
			const FString ObjectPath = Context->SelectedAssets[0].GetObjectPathString();
			InSection.AddMenuEntry(
				"DreamShader.ShowInMaterialBrowser",
				LOCTEXT("CBShowInBrowser", "Show in Material Content Browser"),
				LOCTEXT("CBShowInBrowserTip", "Open the DreamShader Material Content Browser on this asset: its source, compile status, provenance and inheritance."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material"),
				FUIAction(FExecuteAction::CreateLambda([ObjectPath]()
				{
					FDreamShaderMaterialBrowser::OpenAndShowAsset(ObjectPath);
				})));
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

		void ExtendShowInBrowserContextMenu(UClass* AssetClass)
		{
			if (UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(AssetClass))
			{
				FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
				Section.AddDynamicEntry(
					TEXT("DreamShader.ShowInBrowserActions"),
					FNewToolMenuSectionDelegate::CreateStatic(&PopulateShowInBrowserMenu));
			}
		}
	}

	void FDreamShaderMaterialBrowser::OpenAndShowSource(const FString& SourceFilePath)
	{
		if (TSharedPtr<SDreamShaderBrowserShell> Shell = InvokeShell())
		{
			Shell->ShowSource(SourceFilePath);
		}
	}

	void FDreamShaderMaterialBrowser::OpenAndShowAsset(const FString& ObjectPath)
	{
		if (TSharedPtr<SDreamShaderBrowserShell> Shell = InvokeShell())
		{
			Shell->ShowAsset(ObjectPath);
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
			ExtendShowInBrowserContextMenu(UMaterial::StaticClass());
			ExtendShowInBrowserContextMenu(UMaterialInstanceConstant::StaticClass());
			ExtendShowInBrowserContextMenu(UDreamShaderMaterialInstance::StaticClass());
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
}

#undef LOCTEXT_NAMESPACE
