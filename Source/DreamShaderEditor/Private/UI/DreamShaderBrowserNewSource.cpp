// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/DreamShaderBrowserNewSource.h"

#include "DreamShaderModule.h"
#include "UI/DreamShaderBrowserActions.h"

#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
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
	const TCHAR* GetSourceKindExtension(EBrowserSourceKind Kind)
	{
		switch (Kind)
		{
		case EBrowserSourceKind::Function: return TEXT("dsf");
		case EBrowserSourceKind::Header: return TEXT("dsh");
		default: return TEXT("dsm");
		}
	}

	namespace
	{
		const TCHAR* GetTemplateFileName(EBrowserSourceKind Kind)
		{
			switch (Kind)
			{
			case EBrowserSourceKind::Function: return TEXT("NewFunction.dsf");
			case EBrowserSourceKind::Header: return TEXT("NewHeader.dsh");
			default: return TEXT("NewMaterial.dsm");
			}
		}

		FString GetTemplatesDirectory()
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
			return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Templates")) : FString();
		}

		bool IsValidStem(const FString& Stem)
		{
			if (Stem.IsEmpty() || !(FChar::IsAlpha(Stem[0]) || Stem[0] == TCHAR('_')))
			{
				return false;
			}
			for (const TCHAR Char : Stem)
			{
				if (!(FChar::IsAlnum(Char) || Char == TCHAR('_')))
				{
					return false;
				}
			}
			return true;
		}

		// The block's Name= for a file at Directory/Stem: the directory relative to its root, then
		// the stem. A file straight in the root is just the stem.
		FString MakeBlockName(const FString& NormalizedDirectory, const FString& Stem)
		{
			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(NormalizedDirectory / Stem);
			if (Root && NormalizedDirectory.Len() > Root->Directory.Len())
			{
				return NormalizedDirectory.Mid(Root->Directory.Len() + 1) / Stem;
			}
			return Stem;
		}
	}

	bool RenderNewSourceTemplate(const FNewSourceRequest& Request, FString& OutText, FString& OutError)
	{
		const FString TemplatePath = FPaths::Combine(GetTemplatesDirectory(), GetTemplateFileName(Request.Kind));
		if (!FFileHelper::LoadFileToString(OutText, *TemplatePath))
		{
			OutError = FText::Format(LOCTEXT("TemplateMissing", "The template '{0}' is missing from the plugin."), FText::FromString(TemplatePath)).ToString();
			return false;
		}

		const FString Directory = UE::DreamShader::NormalizeSourceFilePath(Request.Directory);
		const FString FileName = FString::Printf(TEXT("%s.%s"), *Request.FileStem, GetSourceKindExtension(Request.Kind)); // I18N-EXEMPT: file name
		const FString BlockName = MakeBlockName(Directory, Request.FileStem);
		OutText.ReplaceInline(TEXT("{NAME}"), *BlockName);
		OutText.ReplaceInline(TEXT("{FILENAME}"), *FileName);
		OutText.ReplaceInline(TEXT("{ASSETPATH}"), *(TEXT("/Game/") + BlockName));
		return true;
	}

	bool CreateNewSourceFile(const FNewSourceRequest& Request, FString& OutFilePath, FString& OutError)
	{
		if (!IsValidStem(Request.FileStem))
		{
			OutError = LOCTEXT("NewSourceBadName", "The name must be an identifier: letters, digits and underscores, not starting with a digit.").ToString();
			return false;
		}
		const FString Directory = UE::DreamShader::NormalizeSourceFilePath(Request.Directory);
		if (Directory.IsEmpty() || !UE::DreamShader::IsWritableSourceFilePath(Directory / TEXT("x.dsm")))
		{
			OutError = LOCTEXT("NewSourceReadOnly", "Choose a folder under the project's DShader root. A plugin's sources are read-only.").ToString();
			return false;
		}
		const FString FilePath = UE::DreamShader::NormalizeSourceFilePath(
			Directory / FString::Printf(TEXT("%s.%s"), *Request.FileStem, GetSourceKindExtension(Request.Kind))); // I18N-EXEMPT: file name
		if (IFileManager::Get().FileExists(*FilePath))
		{
			OutError = FText::Format(LOCTEXT("NewSourceExists", "'{0}' already exists."), FText::FromString(FilePath)).ToString();
			return false;
		}

		FString Text;
		if (!RenderNewSourceTemplate(Request, Text, OutError))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*Directory, true);
		if (!FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FText::Format(LOCTEXT("NewSourceWriteFailed", "Could not write '{0}'."), FText::FromString(FilePath)).ToString();
			return false;
		}
		OutFilePath = FilePath;
		return true;
	}

	void OpenNewSourceDialog(EBrowserSourceKind Kind, const FString& DefaultDirectory, TFunction<void(const FString&)> OnCreated)
	{
		// Default into the project root when the caller's directory is not writable (a plugin's).
		FString StartDirectory = UE::DreamShader::NormalizeSourceFilePath(DefaultDirectory);
		if (StartDirectory.IsEmpty() || !UE::DreamShader::IsWritableSourceFilePath(StartDirectory / TEXT("x.dsm")))
		{
			StartDirectory = UE::DreamShader::NormalizeSourceFilePath(UE::DreamShader::GetSourceShaderDirectory());
		}

		FText Title;
		FText DefaultStem;
		switch (Kind)
		{
		case EBrowserSourceKind::Function:
			Title = LOCTEXT("NewFunctionTitle", "New material function (.dsf)");
			DefaultStem = INVTEXT("F_NewFunction");
			break;
		case EBrowserSourceKind::Header:
			Title = LOCTEXT("NewHeaderTitle", "New header (.dsh)");
			DefaultStem = INVTEXT("Common");
			break;
		default:
			Title = LOCTEXT("NewMaterialTitle", "New material (.dsm)");
			DefaultStem = INVTEXT("M_NewMaterial");
			break;
		}

		TSharedRef<FString> StemValue = MakeShared<FString>(DefaultStem.ToString());
		TSharedRef<FString> DirectoryValue = MakeShared<FString>(StartDirectory);

		TSharedRef<SEditableTextBox> DirectoryBox = SNew(SEditableTextBox)
			.Text(FText::FromString(*DirectoryValue))
			.OnTextChanged_Lambda([DirectoryValue](const FText& NewText) { *DirectoryValue = NewText.ToString(); });

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(520.0f, 200.0f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		const auto CloseWindow = [Window]() { Window->RequestDestroyWindow(); };

		Window->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(16.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SGridPanel)
					.FillColumn(1, 1.0f)

					+ SGridPanel::Slot(0, 0).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NewSourceNameLabel", "Name"))
					]
					+ SGridPanel::Slot(1, 0).Padding(4.0f)
					[
						SNew(SEditableTextBox)
						.Text(DefaultStem)
						.SelectAllTextWhenFocused(true)
						.OnTextChanged_Lambda([StemValue](const FText& NewText) { *StemValue = NewText.ToString().TrimStartAndEnd(); })
					]

					+ SGridPanel::Slot(0, 1).Padding(4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NewSourceFolderLabel", "Folder"))
					]
					+ SGridPanel::Slot(1, 1).Padding(4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							DirectoryBox
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("NewSourceBrowse", "Browse..."))
							.OnClicked_Lambda([DirectoryValue, DirectoryBox, Window]()
							{
								IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
								FString Chosen;
								if (DesktopPlatform && DesktopPlatform->OpenDirectoryDialog(
										FSlateApplication::Get().FindBestParentWindowHandleForDialogs(Window),
										LOCTEXT("NewSourcePickFolder", "Choose a source folder").ToString(),
										*DirectoryValue,
										Chosen))
								{
									*DirectoryValue = UE::DreamShader::NormalizeSourceFilePath(Chosen);
									DirectoryBox->SetText(FText::FromString(*DirectoryValue));
								}
								return FReply::Handled();
							})
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 6.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NewSourceHint", "The file is written from the plugin's template and compiled by the watcher on save."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SSpacer)
				]

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("NewSourceCancel", "Cancel"))
						.OnClicked_Lambda([CloseWindow]() { CloseWindow(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.Text(LOCTEXT("NewSourceCreate", "Create"))
						.OnClicked_Lambda([Kind, StemValue, DirectoryValue, OnCreated, CloseWindow]()
						{
							FNewSourceRequest Request;
							Request.Kind = Kind;
							Request.Directory = *DirectoryValue;
							Request.FileStem = *StemValue;
							FString FilePath;
							FString Error;
							if (CreateNewSourceFile(Request, FilePath, Error))
							{
								FDreamShaderBrowserActions::Notify(FText::Format(LOCTEXT("NewSourceCreated", "Created {0}"), FText::FromString(FPaths::GetCleanFilename(FilePath))), true);
								CloseWindow();
								if (OnCreated) { OnCreated(FilePath); }
							}
							else
							{
								FDreamShaderBrowserActions::Notify(FText::FromString(Error), false);
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
