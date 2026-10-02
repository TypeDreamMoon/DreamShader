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
	const TCHAR* GetSourceKindExtension(EBrowserSourceKind Kind, ENewSourceLanguage Language)
	{
		const bool bLang2 = Language == ENewSourceLanguage::Lang2;
		switch (Kind)
		{
		// One `.dss` holds materials and functions alike: what an `export` is, is its signature's to say.
		case EBrowserSourceKind::Material: return bLang2 ? TEXT("dss") : TEXT("dsm");
		case EBrowserSourceKind::Function: return bLang2 ? TEXT("dss") : TEXT("dsf");
		case EBrowserSourceKind::Header: return TEXT("dsh");
		case EBrowserSourceKind::Instance: return TEXT("dsi");
		case EBrowserSourceKind::Pipeline: return TEXT("dsp");
		}
		return TEXT("dsm");
	}

	namespace
	{
		const TCHAR* GetTemplateFileName(const FNewSourceRequest& Request)
		{
			const bool bLang2 = Request.Language == ENewSourceLanguage::Lang2;
			switch (Request.Kind)
			{
			case EBrowserSourceKind::Material: return bLang2 ? TEXT("NewMaterial.dss") : TEXT("NewMaterial.dsm");
			case EBrowserSourceKind::Function: return bLang2 ? TEXT("NewFunction.dss") : TEXT("NewFunction.dsf");
			case EBrowserSourceKind::Header: return TEXT("NewHeader.dsh");
			case EBrowserSourceKind::Instance: return TEXT("NewInstance.dsi");
			case EBrowserSourceKind::Pipeline:
				switch (Request.PipelineTemplate)
				{
				case ENewPipelineTemplate::MeshMask: return TEXT("NewPipelineMeshMask.dsp");
				case ENewPipelineTemplate::Compute: return TEXT("NewPipelineCompute.dsp");
				case ENewPipelineTemplate::PostProcess: return TEXT("NewPipelinePostProcess.dsp");
				}
				return TEXT("NewPipelinePostProcess.dsp");
			}
			return TEXT("NewMaterial.dsm");
		}

		/** One file a pipeline template writes besides its `.dsp`. */
		struct FPipelineCompanionSpec
		{
			/** Under Resources/Templates. */
			const TCHAR* TemplateFileName = TEXT("");
			/** The file's stem; {BASE} is the pipeline's stem without `CP_`. */
			const TCHAR* StemPattern = TEXT("");
			const TCHAR* Extension = TEXT("");
		};

		/** The materials the passes of each template name, which the `.dsp` would not build without. A shader is in the `.dsp`. */
		TArray<FPipelineCompanionSpec> GetPipelineCompanionSpecs(const ENewPipelineTemplate Template)
		{
			switch (Template)
			{
			case ENewPipelineTemplate::MeshMask:
				return {
					{ TEXT("NewPipelineMeshMask_Mask.dss"), TEXT("M_{BASE}Mask"), TEXT("dss") },
					{ TEXT("NewPipelineMeshMask_Composite.dss"), TEXT("PP_{BASE}Composite"), TEXT("dss") },
				};
			case ENewPipelineTemplate::Compute:
				// Its compute shader is the pass's `hlsl` block: one file.
				return {};
			case ENewPipelineTemplate::PostProcess:
				break;
			}
			return {
				{ TEXT("NewPipelinePostProcess_Composite.dss"), TEXT("PP_{BASE}"), TEXT("dss") },
			};
		}

		FString GetTemplatesDirectory()
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
			return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Templates")) : FString();
		}

		// Under a root the editor may write into. IsWritableSourceFilePath is deliberately true for a
		// path outside every root (an ad-hoc source is allowed to exist anywhere); a NEW file is not
		// ad-hoc, it must land where the scan and the watcher will find it.
		bool IsUnderWritableRoot(const FString& NormalizedDirectory)
		{
			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(NormalizedDirectory / TEXT("x.dsm"));
			return Root != nullptr && Root->bWritable;
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

		// A Parent value as the pragma quotes it: trimmed, and without quotes of its own, which would end the string early.
		FString MakeNewSourceParentText(const FString& ParentReference)
		{
			FString Text = ParentReference.TrimStartAndEnd();
			Text.ReplaceInline(TEXT("\""), TEXT(""));
			return Text;
		}

		// What a pipeline's companions are named after: `CP_Glow` makes `PP_Glow`, a stem without the prefix stands as it is.
		FString MakePipelineBase(const FString& Stem)
		{
			FString Base = Stem;
			if (Base.StartsWith(TEXT("CP_"), ESearchCase::CaseSensitive))
			{
				Base.RightChopInline(3);
			}
			return IsValidStem(Base) ? Base : Stem;
		}

		// Every placeholder a template may hold, for one file.
		struct FTemplateValues
		{
			FString Name;
			FString Stem;
			FString FileName;
			FString AssetPath;
			FString Parent;
			FString Base;
			FString Pipeline;
			FString ShaderPath;
		};

		// The values every file of one request shares: its parent, and what a pipeline's files say about each other.
		FTemplateValues MakeSharedTemplateValues(const FNewSourceRequest& Request)
		{
			FTemplateValues Values;
			Values.Parent = MakeNewSourceParentText(Request.ParentReference);
			Values.Base = MakePipelineBase(Request.FileStem);
			Values.Pipeline = FString::Printf(TEXT("%s.%s"), *Request.FileStem, GetSourceKindExtension(Request.Kind, Request.Language)); // I18N-EXEMPT: file name
			// Next to the `.dsp`, named relative to it: the engine compiles a snapshot of the file under the mapped
			// /DreamPassUser directory, never the file itself, so where it sits is the author's choice.
			Values.ShaderPath = FString::Printf(TEXT("%s.usf"), *Values.Base); // I18N-EXEMPT: file name
			return Values;
		}

		void ApplyTemplateValues(FString& Text, const FTemplateValues& Values)
		{
			Text.ReplaceInline(TEXT("{NAME}"), *Values.Name);
			Text.ReplaceInline(TEXT("{STEM}"), *Values.Stem);
			Text.ReplaceInline(TEXT("{FILENAME}"), *Values.FileName);
			Text.ReplaceInline(TEXT("{ASSETPATH}"), *Values.AssetPath);
			Text.ReplaceInline(TEXT("{PARENT}"), *Values.Parent);
			Text.ReplaceInline(TEXT("{BASE}"), *Values.Base);
			Text.ReplaceInline(TEXT("{PIPELINE}"), *Values.Pipeline);
			Text.ReplaceInline(TEXT("{SHADERPATH}"), *Values.ShaderPath);
		}

		bool LoadNewSourceTemplate(const TCHAR* TemplateFileName, FString& OutText, FString& OutError)
		{
			const FString TemplatePath = FPaths::Combine(GetTemplatesDirectory(), TemplateFileName);
			if (!FFileHelper::LoadFileToString(OutText, *TemplatePath))
			{
				OutError = FText::Format(LOCTEXT("TemplateMissing", "The template '{0}' is missing from the plugin."), FText::FromString(TemplatePath)).ToString();
				return false;
			}
			return true;
		}
	}

	bool RenderNewSourceTemplate(const FNewSourceRequest& Request, FString& OutText, FString& OutError)
	{
		if (!LoadNewSourceTemplate(GetTemplateFileName(Request), OutText, OutError))
		{
			return false;
		}

		const FString Directory = UE::DreamShader::NormalizeSourceFilePath(Request.Directory);
		// A 1.x block says where its asset goes (Name=); a `.dss` export is named by the stem and lands by its file's
		// folder. Both come to the same /Game path, which is what {ASSETPATH} shows.
		FTemplateValues Values = MakeSharedTemplateValues(Request);
		Values.Name = MakeBlockName(Directory, Request.FileStem);
		Values.Stem = Request.FileStem;
		Values.FileName = FString::Printf(TEXT("%s.%s"), *Request.FileStem, GetSourceKindExtension(Request.Kind, Request.Language)); // I18N-EXEMPT: file name
		Values.AssetPath = TEXT("/Game/") + Values.Name;
		ApplyTemplateValues(OutText, Values);
		return true;
	}

	bool RenderNewSourceCompanions(const FNewSourceRequest& Request, TArray<FNewSourceCompanion>& OutCompanions, FString& OutError)
	{
		OutCompanions.Reset();
		if (Request.Kind != EBrowserSourceKind::Pipeline)
		{
			return true;
		}

		const FString Directory = UE::DreamShader::NormalizeSourceFilePath(Request.Directory);
		const FTemplateValues Shared = MakeSharedTemplateValues(Request);
		for (const FPipelineCompanionSpec& Spec : GetPipelineCompanionSpecs(Request.PipelineTemplate))
		{
			FString Text;
			if (!LoadNewSourceTemplate(Spec.TemplateFileName, Text, OutError))
			{
				OutCompanions.Reset();
				return false;
			}

			FTemplateValues Values = Shared;
			Values.Stem = Spec.StemPattern;
			Values.Stem.ReplaceInline(TEXT("{BASE}"), *Shared.Base);
			Values.FileName = FString::Printf(TEXT("%s.%s"), *Values.Stem, Spec.Extension); // I18N-EXEMPT: file name
			// Every companion sits next to the pipeline; a material lands by its folder in /Game as the pipeline does.
			Values.Name = MakeBlockName(Directory, Values.Stem);
			Values.AssetPath = TEXT("/Game/") + Values.Name;
			ApplyTemplateValues(Text, Values);

			FNewSourceCompanion& Companion = OutCompanions.AddDefaulted_GetRef();
			Companion.FilePath = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Directory, Values.FileName));
			Companion.Text = MoveTemp(Text);
		}
		return true;
	}

	bool CreateNewSourceFile(const FNewSourceRequest& Request, FString& OutFilePath, FString& OutError)
	{
		if (!IsValidStem(Request.FileStem))
		{
			OutError = LOCTEXT("NewSourceBadName", "The name must be an identifier: letters, digits and underscores, not starting with a digit.").ToString();
			return false;
		}
		if (Request.Kind == EBrowserSourceKind::Instance && MakeNewSourceParentText(Request.ParentReference).IsEmpty())
		{
			// A `.dsi` without a Parent fails its first compile (DSH7252); saying so here costs the user nothing.
			OutError = LOCTEXT("NewSourceNeedsParent", "An instance needs a parent: the asset path of a material, or the name of a product under the same source root.").ToString();
			return false;
		}
		const FString Directory = UE::DreamShader::NormalizeSourceFilePath(Request.Directory);
		if (Directory.IsEmpty() || !IsUnderWritableRoot(Directory))
		{
			OutError = LOCTEXT("NewSourceReadOnly", "Choose a folder under the project's DShader root. A plugin's sources are read-only.").ToString();
			return false;
		}
		const FString FilePath = UE::DreamShader::NormalizeSourceFilePath(
			Directory / FString::Printf(TEXT("%s.%s"), *Request.FileStem, GetSourceKindExtension(Request.Kind, Request.Language))); // I18N-EXEMPT: file name
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

		// A pipeline's materials and shader: all of them or none, and never over a file that is there already.
		TArray<FNewSourceCompanion> Companions;
		if (!RenderNewSourceCompanions(Request, Companions, OutError))
		{
			return false;
		}
		for (const FNewSourceCompanion& Companion : Companions)
		{
			if (IFileManager::Get().FileExists(*Companion.FilePath))
			{
				OutError = FText::Format(
					LOCTEXT("NewSourceCompanionExists", "'{0}' already exists, and the template writes it for '{1}'; choose another name."),
					FText::FromString(Companion.FilePath),
					FText::FromString(FilePath)).ToString();
				return false;
			}
		}

		// The companions first: the watcher may compile the pipeline as soon as it lands, and its material references
		// resolve to sources that are there.
		TArray<FString> Written;
		for (const FNewSourceCompanion& Companion : Companions)
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Companion.FilePath), true);
			if (!FFileHelper::SaveStringToFile(Companion.Text, *Companion.FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				for (const FString& Undo : Written)
				{
					IFileManager::Get().Delete(*Undo);
				}
				OutError = FText::Format(LOCTEXT("NewSourceWriteFailed", "Could not write '{0}'."), FText::FromString(Companion.FilePath)).ToString();
				return false;
			}
			Written.Add(Companion.FilePath);
		}

		IFileManager::Get().MakeDirectory(*Directory, true);
		if (!FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			for (const FString& Undo : Written)
			{
				IFileManager::Get().Delete(*Undo);
			}
			OutError = FText::Format(LOCTEXT("NewSourceWriteFailed", "Could not write '{0}'."), FText::FromString(FilePath)).ToString();
			return false;
		}
		OutFilePath = FilePath;
		return true;
	}

	void OpenNewSourceDialog(
		EBrowserSourceKind Kind,
		const FString& DefaultDirectory,
		TFunction<void(const FString&)> OnCreated,
		const FString& DefaultParent,
		ENewSourceLanguage Language,
		ENewPipelineTemplate PipelineTemplate)
	{
		// Default into the project root when the caller's directory is not writable (a plugin's).
		FString StartDirectory = UE::DreamShader::NormalizeSourceFilePath(DefaultDirectory);
		if (StartDirectory.IsEmpty() || !IsUnderWritableRoot(StartDirectory))
		{
			StartDirectory = UE::DreamShader::NormalizeSourceFilePath(UE::DreamShader::GetSourceShaderDirectory());
		}

		const bool bLang2 = Language == ENewSourceLanguage::Lang2;
		FText Title;
		FText DefaultStem;
		switch (Kind)
		{
		case EBrowserSourceKind::Pipeline:
			// The `CP_` the companions drop from their names; the rest says which chain it is.
			switch (PipelineTemplate)
			{
			case ENewPipelineTemplate::MeshMask:
				Title = LOCTEXT("NewPipelineMeshMaskTitle", "New mesh mask chain (.dsp)");
				DefaultStem = INVTEXT("CP_Outline");
				break;
			case ENewPipelineTemplate::Compute:
				Title = LOCTEXT("NewPipelineComputeTitle", "New compute chain (.dsp)");
				DefaultStem = INVTEXT("CP_WindField");
				break;
			case ENewPipelineTemplate::PostProcess:
				Title = LOCTEXT("NewPipelinePostProcessTitle", "New fullscreen post-process chain (.dsp)");
				DefaultStem = INVTEXT("CP_SoftGlow");
				break;
			}
			break;
		case EBrowserSourceKind::Function:
			Title = bLang2
				? LOCTEXT("NewFunctionDssTitle", "New material function (.dss)")
				: LOCTEXT("NewFunctionTitle", "New material function (.dsf)");
			// In a `.dss` the stem is the export's name and so the asset's, which is a function's usual MF_.
			DefaultStem = bLang2 ? INVTEXT("MF_NewFunction") : INVTEXT("F_NewFunction");
			break;
		case EBrowserSourceKind::Header:
			Title = LOCTEXT("NewHeaderTitle", "New header (.dsh)");
			DefaultStem = INVTEXT("Common");
			break;
		case EBrowserSourceKind::Instance:
			Title = LOCTEXT("NewInstanceTitle", "New material instance (.dsi)");
			DefaultStem = INVTEXT("MI_NewInstance");
			break;
		case EBrowserSourceKind::Material:
			Title = bLang2
				? LOCTEXT("NewMaterialDssTitle", "New material (.dss)")
				: LOCTEXT("NewMaterialTitle", "New material (.dsm)");
			DefaultStem = INVTEXT("M_NewMaterial");
			break;
		}
		const bool bInstance = Kind == EBrowserSourceKind::Instance;
		const bool bPipeline = Kind == EBrowserSourceKind::Pipeline;

		// What a pipeline template writes besides its `.dsp`.
		FText PipelineHint;
		if (bPipeline)
		{
			switch (PipelineTemplate)
			{
			case ENewPipelineTemplate::MeshMask:
				PipelineHint = LOCTEXT("NewPipelineMeshMaskHint", "Written with M_<name>Mask.dss and PP_<name>Composite.dss next to it, the two materials its passes draw (<name> is the name without CP_). The objects it outlines are those added to the list of that name (UDreamPassSubsystem::AddToList).");
				break;
			case ENewPipelineTemplate::Compute:
				PipelineHint = LOCTEXT("NewPipelineComputeHint", "One file: the compute shader its pass runs is written in the pass's hlsl block. The field it writes is exported as a render target any material can read.");
				break;
			case ENewPipelineTemplate::PostProcess:
				PipelineHint = LOCTEXT("NewPipelinePostProcessHint", "Written with PP_<name>.dss next to it, the Post Process material its fullscreen pass draws (<name> is the name without CP_).");
				break;
			}
		}

		TSharedRef<FString> StemValue = MakeShared<FString>(DefaultStem.ToString());
		TSharedRef<FString> DirectoryValue = MakeShared<FString>(StartDirectory);
		TSharedRef<FString> ParentValue = MakeShared<FString>(DefaultParent.TrimStartAndEnd());

		TSharedRef<SEditableTextBox> DirectoryBox = SNew(SEditableTextBox)
			.Text(FText::FromString(*DirectoryValue))
			.OnTextChanged_Lambda([DirectoryValue](const FText& NewText) { *DirectoryValue = NewText.ToString(); });

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(520.0f, bPipeline ? 280.0f : (bInstance ? 240.0f : (bLang2 ? 220.0f : 200.0f))))
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		const auto CloseWindow = [Window]() { Window->RequestDestroyWindow(); };

		TSharedRef<SGridPanel> Fields = SNew(SGridPanel)
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
			];

		if (bInstance)
		{
			Fields->AddSlot(0, 2).Padding(4.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("NewSourceParentLabel", "Parent"))
			];
			Fields->AddSlot(1, 2).Padding(4.0f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(*ParentValue))
				.HintText(LOCTEXT("NewSourceParentHint", "/Game/Materials/M_Base, or the name of a product"))
				.OnTextChanged_Lambda([ParentValue](const FText& NewText) { *ParentValue = NewText.ToString().TrimStartAndEnd(); })
			];
		}

		Window->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(16.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					Fields
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 6.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(bPipeline
						? LOCTEXT("NewSourcePipelineHint", "The file is written from the plugin's template and compiled by the watcher on save. Its pipeline lands at the folder's /Game path, and runs where something activates it: the global pipelines of Project Settings > DreamShader Custom Pass, or a Dream Pass Volume.")
						: bInstance
						? LOCTEXT("NewSourceInstanceHint", "The file is written from the plugin's template and compiled by the watcher on save. Its asset lands at the folder's /Game path; add one uniform per parameter to override.")
						: bLang2
							? LOCTEXT("NewSourceDssHint", "The file is written from the plugin's template and compiled by the watcher on save. The name is the export's, and so the asset's; the folder decides where under /Game it lands.")
							: LOCTEXT("NewSourceHint", "The file is written from the plugin's template and compiled by the watcher on save."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility(bPipeline ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(PipelineHint)
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
						.OnClicked_Lambda([Kind, Language, PipelineTemplate, StemValue, DirectoryValue, ParentValue, OnCreated, CloseWindow]()
						{
							FNewSourceRequest Request;
							Request.Kind = Kind;
							Request.Language = Language;
							Request.Directory = *DirectoryValue;
							Request.FileStem = *StemValue;
							Request.ParentReference = *ParentValue;
							Request.PipelineTemplate = PipelineTemplate;
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
