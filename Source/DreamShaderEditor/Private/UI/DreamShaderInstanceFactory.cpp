#include "UI/DreamShaderInstanceFactory.h"
#include "DreamShaderDiagnostic.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderCompilerService.h"
#include "DreamShaderGeneratedAssets.h"
// IsDreamShaderLang2Source: which stamped sources make a parent a DreamShader product.
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderTextWireUtils.h"
#include "Provenance/DreamShaderProvenanceActions.h"
#include "UI/DreamShaderBrowserNewSource.h"
#include "UI/DreamShaderGeneratedAssetPath.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "Dialogs/DlgPickPath.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
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

		// The Parent a hand-written `.dsi` would spell: the package path when the object is named after its package (the
		// usual asset), else the full object path.
		FString MakeInstanceFactoryParentReference(const UMaterialInterface* Parent)
		{
			const FString ObjectPath = Parent->GetPathName();
			const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
			return FPackageName::GetShortName(PackageName).Equals(Parent->GetName(), ESearchCase::CaseSensitive) ? PackageName : ObjectPath;
		}

		// Through the bridge when there is one, so the diagnostics store, diagnostics.json and the browser see the result;
		// straight to the compiler otherwise. Forced: the file was written a moment ago and has to exist as an asset now.
		bool CompileInstanceFactorySource(const FString& SourceFilePath, FString& OutMessage)
		{
			if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
			{
				return Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, OutMessage);
			}
			::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
			if (!Compiler)
			{
				OutMessage = LOCTEXT("FactoryCompilerUnavailable", "The DreamShader compiler module is not available, so the instance was not compiled.").ToString();
				return false;
			}
			::UE::DreamShader::FDreamShaderCompileRequest Request;
			Request.SourceFilePath = SourceFilePath;
			Request.bForce = true;
			// A `.dsi` instance always saves; a memory-only parent is materialized by the compile itself (DSH8244).
			Request.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Ephemeral;
			const ::UE::DreamShader::FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
			OutMessage = ToInvariantWireString(Result.Message);
			return Result.bSucceeded;
		}
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

		FDreamShaderError SaveError;
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

	bool IsDreamShaderInstanceSourceParent(UMaterialInterface* Parent)
	{
		if (!Parent || !HasDreamShaderSourceMetadata(Parent))
		{
			return false;
		}
		FString SourceFilePath;
		FString Error;
		return TryResolveGeneratedAssetSourceFile(Parent, SourceFilePath, Error)
			&& ::UE::DreamShader::Editor::Compiler::IsDreamShaderLang2Source(SourceFilePath);
	}

	void GetDefaultInstanceSourceDestination(UMaterialInterface* Parent, FString& OutDirectory, FString& OutFileStem)
	{
		OutDirectory.Reset();
		OutFileStem.Reset();
		if (!Parent)
		{
			return;
		}

		// Beside the parent's source, in the same instance subfolder the unmanaged instance uses, so the `.dsi`'s asset (the
		// folder's mirrored /Game path) lands where an unmanaged instance of the same parent would have.
		FString SourceFilePath;
		FString Error;
		const bool bHasSource = TryResolveGeneratedAssetSourceFile(Parent, SourceFilePath, Error);
		const ::UE::DreamShader::FDreamShaderSourceRoot* const Root = bHasSource ? ::UE::DreamShader::FindSourceRootForFile(SourceFilePath) : nullptr;
		const FString BaseDirectory = (Root && Root->bWritable)
			? FPaths::GetPath(SourceFilePath)
			: ::UE::DreamShader::GetSourceShaderDirectory();
		const FString Subfolder = GetDefault<UDreamShaderSettings>()->InstanceSubfolder;
		OutDirectory = ::UE::DreamShader::NormalizeSourceFilePath(Subfolder.IsEmpty() ? BaseDirectory : FPaths::Combine(BaseDirectory, Subfolder));

		const FString ParentLeaf = FPackageName::GetShortName(FPackageName::ObjectPathToPackageName(Parent->GetPathName()));
		const FString BaseStem = FString::Printf(TEXT("MI_%s"), *::UE::DreamShader::SanitizeIdentifier(ParentLeaf)); // I18N-EXEMPT: file name
		OutFileStem = BaseStem;
		for (int32 Suffix = 2; IFileManager::Get().FileExists(*FPaths::Combine(OutDirectory, OutFileStem + TEXT(".dsi"))); ++Suffix)
		{
			OutFileStem = FString::Printf(TEXT("%s_%d"), *BaseStem, Suffix); // I18N-EXEMPT: file name
		}
	}

	FCreateInstanceSourceResult CreateDreamShaderInstanceSource(
		UMaterialInterface* Parent,
		const FString& Directory,
		const FString& FileStem,
		bool bOpenAfterCreate)
	{
		FCreateInstanceSourceResult Result;
		if (!Parent)
		{
			Result.Error = LOCTEXT("NoParent", "No parent material was provided.").ToString();
			return Result;
		}

		FNewSourceRequest Request;
		Request.Kind = EBrowserSourceKind::Instance;
		Request.Directory = Directory;
		Request.FileStem = FileStem;
		Request.ParentReference = MakeInstanceFactoryParentReference(Parent);
		if (!CreateNewSourceFile(Request, Result.SourceFilePath, Result.Error))
		{
			return Result;
		}

		FString CompileMessage;
		if (!CompileInstanceFactorySource(Result.SourceFilePath, CompileMessage))
		{
			Result.Error = CompileMessage;
			return Result;
		}

		// The asset the file built, by the same resolution the compile used.
		FString ObjectPath;
		FString SourceHash;
		FText ResolveError;
		if (ResolveGeneratedAssetProduct(Result.SourceFilePath, /*bMaterialOnly*/ false, ObjectPath, SourceHash, ResolveError))
		{
			Result.Instance = FindObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
			if (!Result.Instance)
			{
				Result.Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
			}
		}
		if (!Result.Instance)
		{
			Result.Error = FText::Format(
				LOCTEXT("FactoryInstanceSourceNoAsset", "'{0}' compiled, but no material instance was found at the asset path it builds."),
				FText::FromString(Result.SourceFilePath)).ToString();
			return Result;
		}

		if (bOpenAfterCreate && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Result.Instance);
		}

		Result.bSucceeded = true;
		return Result;
	}

	void OpenCreateInstanceSourceDialog(UMaterialInterface* Parent)
	{
		if (!Parent)
		{
			NotifyInstance(LOCTEXT("SelectFirst", "Select a material to create an instance of."), false);
			return;
		}

		FString DefaultDirectory;
		FString DefaultStem;
		GetDefaultInstanceSourceDestination(Parent, DefaultDirectory, DefaultStem);

		TSharedRef<FString> StemValue = MakeShared<FString>(DefaultStem);
		TSharedRef<FString> DirectoryValue = MakeShared<FString>(DefaultDirectory);
		TSharedRef<bool> OpenAfterValue = MakeShared<bool>(true);
		// Set by the secondary button; read once this modal has closed, so the unmanaged dialog never opens on top of it.
		TSharedRef<bool> OpenUnmanagedValue = MakeShared<bool>(false);
		const TWeakObjectPtr<UMaterialInterface> WeakParent(Parent);

		TSharedRef<SEditableTextBox> DirectoryBox = SNew(SEditableTextBox)
			.Text(FText::FromString(*DirectoryValue))
			.OnTextChanged_Lambda([DirectoryValue](const FText& NewText) { *DirectoryValue = NewText.ToString(); });

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateInstanceSourceTitle", "Create material instance (.dsi)"))
			.ClientSize(FVector2D(560.0f, 270.0f))
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
						SNew(STextBlock).Text(LOCTEXT("InstanceSourceParentLabel", "Parent"))
					]
					+ SGridPanel::Slot(1, 0).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(MakeInstanceFactoryParentReference(Parent)))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SGridPanel::Slot(0, 1).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("InstanceSourceNameLabel", "Name"))
					]
					+ SGridPanel::Slot(1, 1).Padding(4.0f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(*StemValue))
						.SelectAllTextWhenFocused(true)
						.OnTextChanged_Lambda([StemValue](const FText& NewText) { *StemValue = NewText.ToString().TrimStartAndEnd(); })
					]

					+ SGridPanel::Slot(0, 2).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("InstanceSourceFolderLabel", "Source folder"))
					]
					+ SGridPanel::Slot(1, 2).Padding(4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							DirectoryBox
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("InstanceSourceBrowse", "Browse..."))
							.OnClicked_Lambda([DirectoryValue, DirectoryBox, Window]()
							{
								IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
								FString Chosen;
								if (DesktopPlatform && DesktopPlatform->OpenDirectoryDialog(
										FSlateApplication::Get().FindBestParentWindowHandleForDialogs(Window),
										LOCTEXT("InstanceSourcePickFolder", "Choose a source folder").ToString(),
										*DirectoryValue,
										Chosen))
								{
									*DirectoryValue = ::UE::DreamShader::NormalizeSourceFilePath(Chosen);
									DirectoryBox->SetText(FText::FromString(*DirectoryValue));
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
							SNew(STextBlock).Text(LOCTEXT("InstanceSourceOpenAfter", "Open the instance after creating"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 6.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InstanceSourceHint", "A .dsi source is written into the folder and compiled now. Its asset lands at the folder's /Game path, and every value you tune there can be adopted back into the file."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SSpacer)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("InstanceSourceUnmanaged", "Unmanaged instance..."))
						.ToolTipText(LOCTEXT("InstanceSourceUnmanagedTip", "Create an ordinary material instance asset instead, with no source file describing it."))
						.OnClicked_Lambda([OpenUnmanagedValue, CloseWindow]()
						{
							*OpenUnmanagedValue = true;
							CloseWindow();
							return FReply::Handled();
						})
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSpacer)
					]

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
						.Text(LOCTEXT("InstanceSourceCreate", "Create .dsi"))
						.OnClicked_Lambda([WeakParent, StemValue, DirectoryValue, OpenAfterValue, CloseWindow]()
						{
							UMaterialInterface* ParentPtr = WeakParent.Get();
							if (!ParentPtr)
							{
								NotifyInstance(LOCTEXT("ParentGone", "The parent material is no longer available."), false);
								CloseWindow();
								return FReply::Handled();
							}
							const FCreateInstanceSourceResult Outcome = CreateDreamShaderInstanceSource(
								ParentPtr, *DirectoryValue, *StemValue, *OpenAfterValue);
							if (Outcome.bSucceeded)
							{
								NotifyInstance(
									FText::Format(LOCTEXT("InstanceSourceCreated", "Created {0}"), FText::FromString(FPaths::GetCleanFilename(Outcome.SourceFilePath))),
									true);
								CloseWindow();
							}
							else if (!Outcome.SourceFilePath.IsEmpty())
							{
								// The file exists and did not compile: it is the thing to fix now, so the dialog goes and the source opens.
								NotifyInstance(
									FText::Format(
										LOCTEXT("InstanceSourceCompileFailed", "Created {0}, but it did not compile: {1}"),
										FText::FromString(FPaths::GetCleanFilename(Outcome.SourceFilePath)),
										FText::FromString(Outcome.Error)),
									false);
								CloseWindow();
								FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(Outcome.SourceFilePath);
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

		if (*OpenUnmanagedValue)
		{
			if (UMaterialInterface* ParentPtr = WeakParent.Get())
			{
				OpenCreateInstanceDialog(ParentPtr);
			}
		}
	}

	void OpenCreateInstanceDialogForParent(UMaterialInterface* Parent)
	{
		if (IsDreamShaderInstanceSourceParent(Parent))
		{
			OpenCreateInstanceSourceDialog(Parent);
		}
		else
		{
			OpenCreateInstanceDialog(Parent);
		}
	}
}

#undef LOCTEXT_NAMESPACE
