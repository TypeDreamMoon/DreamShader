// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamPassPipelineCustomization.h.
//
// Diagnostics owned by this file: none. The problems it shows are UDreamPassPipeline::Validate's, as the runtime words them.

#include "Pass/DreamPassPipelineCustomization.h"

// ResolveDreamPassShaderSourceFile: the `.usf` behind an HLSL pass's virtual path, for its Open button.
#include "Decompiler/DreamShaderPipelineDecompiler.h"
#include "DreamPassPipeline.h"
#include "DreamPassTypes.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
// The slot counts and the slot directories a snapshot lives in.
#include "DreamShaderPassModule.h"
// FormatDreamShaderFloatLiteral: numbers read the way the `.dsp` writes them.
#include "Lang/LangHlslText.h"
#include "Lang/LangInstanceSource.h"
#include "Pass/DreamPassSpellings.h"
#include "Provenance/DreamShaderProvenanceActions.h"
#include "UI/DreamShaderBrowserStyle.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShader.PassEditor.PipelineDetails"

namespace UE::DreamShader::Editor::Private
{
	bool FDreamPassPipelineCustomization::bRegistered = false;

	namespace PipelineDetails
	{
		// The class's name, not StaticClass(): Unregister runs at module shutdown, when the class may be gone already.
		const FName ClassName(TEXT("DreamPassPipeline"));
		const FName OverviewCategory(TEXT("DreamPassOverview"));
		const FName FrameCategory(TEXT("DreamPassFrame"));
		const FName BuffersCategory(TEXT("DreamPassBuffers"));
		const FLinearColor WarningColor(0.90f, 0.62f, 0.12f);

		// The rows spell settings and bindings the way a `.dsp` does: they are the source's words, and are not translated.

		FString FloatText(const double Value)
		{
			return UE::DreamShader::Lang::FormatDreamShaderFloatLiteral(Value);
		}

		FString ConstantText(const FDreamPassParameterValue& Value)
		{
			switch (Value.Type)
			{
			case EDreamPassParameterType::Int:
				return FString::FromInt(Value.Int);
			case EDreamPassParameterType::Bool:
				return Value.Bool ? FString(TEXT("true")) : FString(TEXT("false"));
			case EDreamPassParameterType::Texture:
				return Value.Texture ? Value.Texture->GetName() : FString(TEXT("None"));
			default:
				break;
			}
			const int32 Width = PassSpelling::ParameterWidth(Value.Type);
			if (Width <= 1)
			{
				return FloatText(Value.Vector.X);
			}
			TArray<FString> Channels;
			for (int32 Channel = 0; Channel < Width; ++Channel)
			{
				Channels.Add(FloatText(Value.Vector[Channel]));
			}
			return FString::Printf(TEXT("float%d(%s)"), Width, *FString::Join(Channels, TEXT(", "))); // I18N-EXEMPT: .dsp syntax
		}

		FString BindingText(const FDreamPassBufferBinding& Binding)
		{
			const FString Buffer = Binding.Buffer.ToString() + (Binding.bPrevious ? TEXT(".Previous") : TEXT(""));
			return (Binding.Slot.IsNone() || Binding.Slot == Binding.Buffer)
				? Buffer
				: FString::Printf(TEXT("%s = %s"), *Binding.Slot.ToString(), *Buffer); // I18N-EXEMPT: .dsp syntax
		}

		FString ParamText(const FDreamPassParamBinding& Param)
		{
			FString Value;
			switch (Param.Source)
			{
			case EDreamPassParamSource::Parameter:
				Value = Param.Parameter.ToString();
				break;
			case EDreamPassParamSource::Weight:
				Value = UE::DreamPass::WeightParameterName.ToString();
				break;
			case EDreamPassParamSource::Constant:
				Value = ConstantText(Param.Constant);
				break;
			}
			if (Param.Source != EDreamPassParamSource::Constant)
			{
				if (Param.Multiplier != 1.0f)
				{
					Value = FString::Printf(TEXT("%s * %s"), *Value, *FloatText(Param.Multiplier)); // I18N-EXEMPT: .dsp syntax
				}
				if (Param.Offset != 0.0f)
				{
					Value = FString::Printf(TEXT("%s + %s"), *Value, *FloatText(Param.Offset)); // I18N-EXEMPT: .dsp syntax
				}
			}
			return FString::Printf(TEXT("%s = %s"), *Param.Target.ToString(), *Value); // I18N-EXEMPT: .dsp syntax
		}

		FString FilterText(const FDreamPassMeshFilter& Filter)
		{
			TArray<FString> Clauses;
			for (const FDreamPassFilterClause& Clause : Filter.AnyOf)
			{
				TArray<FString> Terms;
				for (const FDreamPassFilterTerm& Term : Clause.AllOf)
				{
					switch (Term.Kind)
					{
					case EDreamPassFilterKind::Stencil:
						Terms.Add(Term.StencilMask == 255
							? FString::Printf(TEXT("Stencil(%d)"), Term.StencilValue) // I18N-EXEMPT: .dsp syntax
							: FString::Printf(TEXT("Stencil(%d, %d)"), Term.StencilValue, Term.StencilMask)); // I18N-EXEMPT: .dsp syntax
						break;
					case EDreamPassFilterKind::Layer:
					{
						TArray<FString> Names;
						for (const FName Layer : Term.LayerNames)
						{
							Names.Add(Layer.ToString());
						}
						const FString Inside = Names.IsEmpty() ? FString::Printf(TEXT("0x%X"), static_cast<uint32>(Term.LayerMask)) : FString::Join(Names, TEXT(" | ")); // I18N-EXEMPT: .dsp syntax
						Terms.Add(FString::Printf(TEXT("Layer(%s)"), *Inside)); // I18N-EXEMPT: .dsp syntax
						break;
					}
					case EDreamPassFilterKind::List:
						Terms.Add(FString::Printf(TEXT("List(%s)"), *Term.ListName.ToString())); // I18N-EXEMPT: .dsp syntax
						break;
					}
				}
				Clauses.Add(FString::Join(Terms, TEXT(" & ")));
			}
			return Clauses.IsEmpty() ? FString(TEXT("-")) : FString::Join(Clauses, TEXT(" | "));
		}

		/** An HLSL pass's slot: a compute pass, or a fullscreen pass with HLSL and no material. False for every other pass. */
		bool GetHlslSlot(const FDreamPassDesc& Pass, bool& bOutCompute, int32& OutSlot)
		{
			if (Pass.Kind == EDreamPassKind::Compute)
			{
				bOutCompute = true;
				OutSlot = Pass.Compute.Slot;
				return true;
			}
			if (Pass.Kind == EDreamPassKind::Fullscreen && Pass.Fullscreen.RunsHlsl())
			{
				bOutCompute = false;
				OutSlot = Pass.Fullscreen.PixelSlot;
				return true;
			}
			return false;
		}

		/**
		 * Where an HLSL pass's code is in its `.dsp` when it is written there (DreamShader_Plan/10): the line of its own `hlsl`
		 * block, or of its entry in the file's block. 0 for a shader file.
		 */
		int32 GetInlineHlslLine(const UDreamPassPipeline& Pipeline, const FDreamPassDesc& Pass)
		{
			namespace Lang = UE::DreamShader::Lang;
			const bool bCompute = Pass.Kind == EDreamPassKind::Compute;
			if (!bCompute && Pass.Kind != EDreamPassKind::Fullscreen)
			{
				return 0;
			}
			switch (bCompute ? Pass.Compute.HlslSource : Pass.Fullscreen.HlslSource)
			{
			case EDreamPassHlslSource::Block:
			case EDreamPassHlslSource::Body:
				return FMath::Max(bCompute ? Pass.Compute.InlineHlslLine : Pass.Fullscreen.InlineHlslLine, 1);
			case EDreamPassHlslSource::Shared:
			{
				const Lang::FHlslTextScan Scan = Lang::ScanHlslText(Pipeline.SharedHlsl);
				const Lang::FHlslTopLevelFunction* Function = Scan.FindFunction(bCompute ? Pass.Compute.Entry : Pass.Fullscreen.Entry);
				const int32 Offset = Function ? Lang::GetHlslLineOfOffset(Pipeline.SharedHlsl, Function->NameOffset) - 1 : 0;
				return FMath::Max(Pipeline.SharedHlslLine + Offset, 1);
			}
			case EDreamPassHlslSource::File:
			default:
				return 0;
			}
		}

		/** The keys an inline pass's code is written with: its `hlsl` block, or the `Entry` it picks from the file's. False for a shader file. */
		bool AddInlineHlslKeys(const EDreamPassHlslSource Source, const FString& Entry, TArray<FString>& Keys)
		{
			switch (Source)
			{
			case EDreamPassHlslSource::Block:
				if (!Entry.Equals(TEXT("Main"), ESearchCase::CaseSensitive))
				{
					Keys.Add(FString::Printf(TEXT("Entry = %s"), *Entry)); // I18N-EXEMPT: .dsp syntax
				}
				Keys.Add(TEXT("hlsl { ... }")); // I18N-EXEMPT: .dsp syntax
				return true;
			case EDreamPassHlslSource::Body:
				Keys.Add(TEXT("hlsl { ... }")); // I18N-EXEMPT: .dsp syntax
				return true;
			case EDreamPassHlslSource::Shared:
				Keys.Add(FString::Printf(TEXT("Entry = %s"), *Entry)); // I18N-EXEMPT: .dsp syntax
				return true;
			case EDreamPassHlslSource::File:
			default:
				return false;
			}
		}

		const FString& GetShaderPath(const FDreamPassDesc& Pass)
		{
			return Pass.Kind == EDreamPassKind::Compute ? Pass.Compute.ShaderPath : Pass.Fullscreen.ShaderPath;
		}

		/** The kind and its keys, as the pass's block in the `.dsp` says them. */
		FString PassSettingsText(const FDreamPassDesc& Pass)
		{
			TArray<FString> Keys;
			switch (Pass.Kind)
			{
			case EDreamPassKind::Fullscreen:
				if (Pass.Fullscreen.Material)
				{
					Keys.Add(FString::Printf(TEXT("Material = \"%s\""), *Pass.Fullscreen.Material->GetName())); // I18N-EXEMPT: .dsp syntax
				}
				else if (AddInlineHlslKeys(Pass.Fullscreen.HlslSource, Pass.Fullscreen.Entry, Keys))
				{
				}
				else if (!Pass.Fullscreen.ShaderPath.IsEmpty())
				{
					Keys.Add(FString::Printf(TEXT("Shader = \"%s\""), *Pass.Fullscreen.ShaderPath)); // I18N-EXEMPT: .dsp syntax
					Keys.Add(FString::Printf(TEXT("Entry = %s"), *Pass.Fullscreen.Entry)); // I18N-EXEMPT: .dsp syntax
				}
				break;

			case EDreamPassKind::Compute:
			{
				const FDreamPassComputeSettings& Compute = Pass.Compute;
				if (!AddInlineHlslKeys(Compute.HlslSource, Compute.Entry, Keys))
				{
					Keys.Add(FString::Printf(TEXT("Shader = \"%s\""), *Compute.ShaderPath)); // I18N-EXEMPT: .dsp syntax
					Keys.Add(FString::Printf(TEXT("Entry = %s"), *Compute.Entry)); // I18N-EXEMPT: .dsp syntax
				}
				Keys.Add(FString::Printf(TEXT("Threads = uint3(%d, %d, %d)"), Compute.ThreadGroupSize.X, Compute.ThreadGroupSize.Y, Compute.ThreadGroupSize.Z)); // I18N-EXEMPT: .dsp syntax
				if (Compute.DispatchMode == EDreamPassDispatchMode::Buffer)
				{
					Keys.Add(Compute.DispatchScale == 1.0f
						? FString::Printf(TEXT("Dispatch = %s"), *Compute.DispatchBuffer.ToString()) // I18N-EXEMPT: .dsp syntax
						: FString::Printf(TEXT("Dispatch = %s * %s"), *Compute.DispatchBuffer.ToString(), *FloatText(Compute.DispatchScale))); // I18N-EXEMPT: .dsp syntax
				}
				else
				{
					Keys.Add(FString::Printf(TEXT("Dispatch = uint3(%d, %d, %d)"), Compute.DispatchSize.X, Compute.DispatchSize.Y, Compute.DispatchSize.Z)); // I18N-EXEMPT: .dsp syntax
				}
				break;
			}

			case EDreamPassKind::Mesh:
			{
				const FDreamPassMeshSettings& Mesh = Pass.Mesh;
				Keys.Add(FString::Printf(TEXT("Filter = %s"), *FilterText(Mesh.Filter))); // I18N-EXEMPT: .dsp syntax
				if (Mesh.OverrideMaterial)
				{
					Keys.Add(FString::Printf(TEXT("Material = \"%s\""), *Mesh.OverrideMaterial->GetName())); // I18N-EXEMPT: .dsp syntax
				}
				Keys.Add(FString::Printf(TEXT("Mode = %s"), PassSpelling::MeshMode(Mesh.Mode))); // I18N-EXEMPT: .dsp syntax
				Keys.Add(Mesh.Depth == EDreamPassDepthMode::Own
					? FString::Printf(TEXT("Depth = Own(%s)"), *Mesh.DepthBuffer.ToString()) // I18N-EXEMPT: .dsp syntax
					: FString::Printf(TEXT("Depth = %s"), PassSpelling::Depth(Mesh.Depth))); // I18N-EXEMPT: .dsp syntax
				if (Mesh.Cull != EDreamPassCullMode::Auto)
				{
					Keys.Add(FString::Printf(TEXT("Cull = %s"), PassSpelling::Cull(Mesh.Cull))); // I18N-EXEMPT: .dsp syntax
				}
				if (Mesh.Blend != EDreamPassBlendMode::Replace)
				{
					Keys.Add(FString::Printf(TEXT("Blend = %s"), PassSpelling::Blend(Mesh.Blend))); // I18N-EXEMPT: .dsp syntax
				}
				if (Mesh.Nanite == EDreamPassNanitePolicy::AssignStencil)
				{
					Keys.Add(FString::Printf(TEXT("Nanite = AssignStencil(%d)"), Mesh.AssignedStencilValue)); // I18N-EXEMPT: .dsp syntax
				}
				else if (Mesh.Nanite != EDreamPassNanitePolicy::Skip)
				{
					Keys.Add(FString::Printf(TEXT("Nanite = %s"), PassSpelling::Nanite(Mesh.Nanite))); // I18N-EXEMPT: .dsp syntax
				}
				break;
			}

			case EDreamPassKind::Clear:
				Keys.Add(FString::Printf(TEXT("Value = float4(%s, %s, %s, %s)"), // I18N-EXEMPT: .dsp syntax
					*FloatText(Pass.Clear.Value.R), *FloatText(Pass.Clear.Value.G), *FloatText(Pass.Clear.Value.B), *FloatText(Pass.Clear.Value.A)));
				break;

			case EDreamPassKind::Copy:
				break;
			}

			if (!Pass.EnabledParameter.IsNone())
			{
				Keys.Add(FString::Printf(TEXT("Enabled = %s"), *Pass.EnabledParameter.ToString())); // I18N-EXEMPT: .dsp syntax
			}
			return Keys.IsEmpty()
				? FString(PassSpelling::Kind(Pass.Kind))
				: FString::Printf(TEXT("%s    %s"), PassSpelling::Kind(Pass.Kind), *FString::Join(Keys, TEXT("    "))); // I18N-EXEMPT: .dsp syntax
		}

		/** `read ...    write ...    param ...`, in the pass's own order. */
		FString PassBindingsText(const FDreamPassDesc& Pass)
		{
			TArray<FString> Parts;
			const auto JoinBindings = [](const TArray<FDreamPassBufferBinding>& Bindings)
			{
				TArray<FString> Texts;
				for (const FDreamPassBufferBinding& Binding : Bindings)
				{
					Texts.Add(BindingText(Binding));
				}
				return FString::Join(Texts, TEXT(", "));
			};
			if (!Pass.Reads.IsEmpty())
			{
				Parts.Add(TEXT("read ") + JoinBindings(Pass.Reads));
			}
			if (!Pass.Writes.IsEmpty())
			{
				Parts.Add(TEXT("write ") + JoinBindings(Pass.Writes));
			}
			if (!Pass.Params.IsEmpty())
			{
				TArray<FString> Texts;
				for (const FDreamPassParamBinding& Param : Pass.Params)
				{
					Texts.Add(ParamText(Param));
				}
				Parts.Add(TEXT("param ") + FString::Join(Texts, TEXT(", ")));
			}
			return FString::Join(Parts, TEXT("    "));
		}

		FString BufferSettingsText(const FDreamPassBufferDesc& Buffer)
		{
			TArray<FString> Keys;
			Keys.Add(UE::DreamPass::LexToString(Buffer.Format));
			if (Buffer.Resolution == EDreamPassBufferResolution::Fixed)
			{
				Keys.Add(FString::Printf(TEXT("Size = int2(%d, %d)"), Buffer.FixedSize.X, Buffer.FixedSize.Y)); // I18N-EXEMPT: .dsp syntax
			}
			else
			{
				Keys.Add(FString::Printf(TEXT("Resolution = %s"), PassSpelling::Resolution(Buffer.Resolution))); // I18N-EXEMPT: .dsp syntax
				if (Buffer.Scale != 1.0f)
				{
					Keys.Add(FString::Printf(TEXT("Scale = %s"), *FloatText(Buffer.Scale))); // I18N-EXEMPT: .dsp syntax
				}
			}
			if (!Buffer.bClear)
			{
				Keys.Add(TEXT("Clear = None"));
			}
			else if (Buffer.ClearValue == FLinearColor::Transparent)
			{
				Keys.Add(TEXT("Clear = 0"));
			}
			else
			{
				Keys.Add(FString::Printf(TEXT("Clear = float4(%s, %s, %s, %s)"), // I18N-EXEMPT: .dsp syntax
					*FloatText(Buffer.ClearValue.R), *FloatText(Buffer.ClearValue.G), *FloatText(Buffer.ClearValue.B), *FloatText(Buffer.ClearValue.A)));
			}
			if (Buffer.Mips != 1)
			{
				Keys.Add(FString::Printf(TEXT("Mips = %d"), Buffer.Mips)); // I18N-EXEMPT: .dsp syntax
			}
			if (Buffer.bHistory)
			{
				Keys.Add(TEXT("History = true"));
			}
			if (Buffer.bExport)
			{
				Keys.Add(TEXT("Export = true"));
			}
			return FString::Join(Keys, TEXT("    "));
		}

		/** One line of a pass's slot state, and the colour it is drawn in. */
		struct FSlotStatus
		{
			FText Text;
			FSlateColor Color = FSlateColor::UseSubduedForeground();
		};

		FSlotStatus DescribeSlot(const FDreamPassDesc& Pass)
		{
			FSlotStatus Status;
			bool bCompute = false;
			int32 Slot = INDEX_NONE;
			if (!GetHlslSlot(Pass, bCompute, Slot))
			{
				return Status;
			}

			const int32 Count = bCompute ? UE::DreamPass::GetComputeSlotCount() : UE::DreamPass::GetPixelSlotCount();
			const FText Kind = bCompute ? LOCTEXT("ComputeSlotKind", "compute shader slot") : LOCTEXT("PixelSlotKind", "pixel shader slot");
			if (Slot == INDEX_NONE)
			{
				Status.Text = FText::Format(LOCTEXT("SlotNone", "no {0} yet: compile the source to give the pass one"), Kind);
				Status.Color = FSlateColor(WarningColor);
				return Status;
			}

			const FText Label = FText::FromString(PassSpelling::SlotLabel(bCompute, Slot));
			if (Slot < 0 || Slot >= Count)
			{
				Status.Text = FText::Format(LOCTEXT("SlotOutOfRange", "{0} {1}, which this build does not have (it has {2})"), Kind, Label, FText::AsNumber(Count));
				Status.Color = FSlateColor(BrowserErrorColor);
				return Status;
			}

			// The global shaders are built from the snapshot, never from the `.usf` itself.
			if (IFileManager::Get().DirectoryExists(*UE::DreamPass::GetSlotDirectory(bCompute, Slot)))
			{
				Status.Text = FText::Format(LOCTEXT("SlotReady", "{0} {1}"), Kind, Label);
			}
			else
			{
				Status.Text = FText::Format(LOCTEXT("SlotNoSnapshot", "{0} {1}, whose snapshot is missing: compile the source"), Kind, Label);
				Status.Color = FSlateColor(WarningColor);
			}
			return Status;
		}

		/** A text attribute over pass PassIndex of the live asset; empty once the pass is gone (the panel rebuilds then). */
		TAttribute<FText> MakePassText(const TWeakObjectPtr<UDreamPassPipeline>& Weak, const int32 PassIndex, FString (*Describe)(const FDreamPassDesc&))
		{
			return TAttribute<FText>::CreateLambda([Weak, PassIndex, Describe]()
			{
				const UDreamPassPipeline* Pipeline = Weak.Get();
				return (Pipeline && Pipeline->Passes.IsValidIndex(PassIndex)) ? FText::FromString(Describe(Pipeline->Passes[PassIndex])) : FText::GetEmpty();
			});
		}

		TSharedRef<SWidget> MakeLinkButton(const FText& Text, const FText& ToolTip, TFunction<void()> OnClicked)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(0.0f, 1.0f))
				.HAlign(HAlign_Left)
				.ToolTipText(ToolTip)
				.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
				[
					SNew(STextBlock)
					.Text(Text)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FSlateColor(BrowserLinkColor))
				];
		}
	}

	TSharedRef<IDetailCustomization> FDreamPassPipelineCustomization::MakeInstance()
	{
		return MakeShared<FDreamPassPipelineCustomization>();
	}

	void FDreamPassPipelineCustomization::Register()
	{
		if (bRegistered)
		{
			return;
		}
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(
			PipelineDetails::ClassName,
			FOnGetDetailCustomizationInstance::CreateStatic(&FDreamPassPipelineCustomization::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();
		bRegistered = true;
	}

	void FDreamPassPipelineCustomization::Unregister()
	{
		if (!bRegistered)
		{
			return;
		}
		bRegistered = false;
		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyEditor->UnregisterCustomClassLayout(PipelineDetails::ClassName);
			PropertyEditor->NotifyCustomizationModuleChanged();
		}
	}

	FDreamPassPipelineCustomization::~FDreamPassPipelineCustomization()
	{
		if (ChangedHandle.IsValid())
		{
			UDreamPassPipeline::OnPipelineChanged.Remove(ChangedHandle);
		}
	}

	void FDreamPassPipelineCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		// One pipeline at a time: several selected keep the default layout, whose rows edit them all together.
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		UDreamPassPipeline* const Pipeline = Objects.Num() == 1 ? Cast<UDreamPassPipeline>(Objects[0].Get()) : nullptr;
		if (!Pipeline)
		{
			return;
		}

		WeakPipeline = Pipeline;
		PropertyUtilities = DetailBuilder.GetPropertyUtilities();
		StructureKey = MakeStructureKey(*Pipeline);
		*ProvenanceLabel = GetBrowserProvenanceLabel(ClassifyGeneratedAsset(Pipeline));
		if (!ChangedHandle.IsValid())
		{
			ChangedHandle = UDreamPassPipeline::OnPipelineChanged.AddSP(this, &FDreamPassPipelineCustomization::OnPipelineChanged);
		}

		BuildOverview(DetailBuilder, *Pipeline);
		BuildPasses(DetailBuilder, *Pipeline);
		BuildBuffers(DetailBuilder, *Pipeline);
	}

	void FDreamPassPipelineCustomization::BuildOverview(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			PipelineDetails::OverviewCategory, LOCTEXT("OverviewCategory", "Pipeline Overview"), ECategoryPriority::Important);

		// The stamp, made absolute: the file the asset was built from, which Revert reads and Adopt writes.
		FString SourceFilePath;
		FString SourceError;
		const bool bHasSource = TryResolveGeneratedAssetSourceFile(&Pipeline, SourceFilePath, SourceError);
		const bool bWritableSource = bHasSource && UE::DreamShader::IsWritableSourceFilePath(SourceFilePath);
		const TWeakObjectPtr<UDreamPassPipeline> Weak = WeakPipeline;
		const TSharedRef<FText> Provenance = ProvenanceLabel;

		const FText SourceText = bHasSource
			? FText::FromString(SourceFilePath)
			: (Pipeline.SourceFilePath.IsEmpty() ? LOCTEXT("SourceNone", "not built from a .dsp") : FText::FromString(Pipeline.SourceFilePath));

		Category.AddCustomRow(LOCTEXT("SourceRowFilter", "Source"))
			.NameContent()
			[
				SNew(STextBlock).Text(LOCTEXT("SourceRowName", "Source")).Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(360.0f)
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(SourceText)
					.ToolTipText(bHasSource ? FText::FromString(SourceFilePath) : FText::FromString(SourceError))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Provenance]() { return *Provenance; })
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("OpenSourceButton", "Open Source"))
						.ToolTipText(LOCTEXT("OpenSourceTip", "Open the .dsp this pipeline is built from in your preferred editor."))
						.IsEnabled(bHasSource)
						.OnClicked_Lambda([SourceFilePath]()
						{
							FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(SourceFilePath);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("RevertButton", "Revert to Source"))
						.ToolTipText(LOCTEXT("RevertTip", "Rebuild this pipeline from its .dsp, discarding every edit made here. The source file is not modified."))
						.IsEnabled(bHasSource)
						.OnClicked_Lambda([Weak]()
						{
							RevertGeneratedAssetToSource(TWeakObjectPtr<UObject>(Weak.Get()));
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("AdoptButton", "Adopt Into Source"))
						.ToolTipText(bWritableSource
							? LOCTEXT("AdoptTip", "Write the edits made here back into the .dsp: only the declarations and keys whose values changed are rewritten, so its comments and order are kept. The file is backed up first.")
							: LOCTEXT("AdoptReadOnlyTip", "This pipeline's source ships with a plugin and is read-only, or it is not found; adopt is not available."))
						.IsEnabled(bWritableSource)
						.OnClicked_Lambda([Weak]()
						{
							AdoptGeneratedAssetIntoSource(TWeakObjectPtr<UObject>(Weak.Get()));
							return FReply::Handled();
						})
					]
				]
			];

		Category.AddCustomRow(LOCTEXT("SummaryRowFilter", "Runs"))
			.NameContent()
			[
				SNew(STextBlock).Text(LOCTEXT("SummaryRowName", "Runs")).Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(360.0f)
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AutoWrapText(true)
				.Text_Lambda([Weak]()
				{
					const UDreamPassPipeline* Live = Weak.Get();
					if (!Live)
					{
						return FText::GetEmpty();
					}
					return FText::Format(
						LOCTEXT("SummaryFormat", "Order {0}, in views {1}, requiring {2}: {3} pass(es), {4} buffer(s), {5} parameter(s)."),
						FText::AsNumber(Live->Order),
						FText::FromString(PassSpelling::JoinFlags(PassSpelling::ViewFlags(), Live->Views)),
						FText::FromString(PassSpelling::JoinFlags(PassSpelling::RequirementFlags(), Live->Requires)),
						FText::AsNumber(Live->Passes.Num()),
						FText::AsNumber(Live->Buffers.Num()),
						FText::AsNumber(Live->Parameters.Num()));
				})
			];

		// What the runtime skips a pass for, after a hand edit: nothing shows while the asset checks.
		Category.AddCustomRow(LOCTEXT("ProblemsRowFilter", "Problems"))
			.Visibility(TAttribute<EVisibility>::CreateLambda([Weak]()
			{
				const UDreamPassPipeline* Live = Weak.Get();
				return (Live && !Live->Validate()) ? EVisibility::Visible : EVisibility::Collapsed;
			}))
			.NameContent()
			[
				SNew(STextBlock).Text(LOCTEXT("ProblemsRowName", "Problems")).Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(360.0f)
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor(BrowserErrorColor))
				.AutoWrapText(true)
				.Text_Lambda([Weak]()
				{
					const UDreamPassPipeline* Live = Weak.Get();
					TArray<FText> Problems;
					if (Live)
					{
						Live->Validate(&Problems);
					}
					return FText::Join(FText::FromString(TEXT("\n")), Problems);
				})
			];

#if !DREAMSHADER_WITH_CUSTOM_PASS
		Category.AddCustomRow(LOCTEXT("EngineRowFilter", "Engine"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EngineTooOld", "Custom Pass runs on Unreal Engine 5.8 and later. This engine loads and saves the pipeline, and runs none of it."))
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.ColorAndOpacity(FSlateColor(PipelineDetails::WarningColor))
				.AutoWrapText(true)
			];
#endif
	}

	void FDreamPassPipelineCustomization::BuildPasses(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			PipelineDetails::FrameCategory, LOCTEXT("FrameCategory", "Passes in Frame Order"), ECategoryPriority::Important);
		if (Pipeline.Passes.IsEmpty())
		{
			Category.AddCustomRow(LOCTEXT("NoPassesFilter", "Passes"))
				.WholeRowContent()
				[
					SNew(STextBlock).Text(LOCTEXT("NoPasses", "This pipeline has no pass.")).Font(IDetailLayoutBuilder::GetDetailFontItalic())
				];
			return;
		}

		const TWeakObjectPtr<UDreamPassPipeline> Weak = WeakPipeline;

		// The `.dsp`, made absolute: where a pass whose HLSL is written in it opens.
		FString DspFile;
		FString DspError;
		const bool bHasDsp = TryResolveGeneratedAssetSourceFile(&Pipeline, DspFile, DspError);

		// The order the frame reaches them in (EDreamPassInjection's), and declaration order inside one point -- which is the
		// order they run in. Another pipeline's passes at the same point run before or after these by Order.
		for (int32 InjectionIndex = 0; InjectionIndex < int32(EDreamPassInjection::Count); ++InjectionIndex)
		{
			const EDreamPassInjection Injection = static_cast<EDreamPassInjection>(InjectionIndex);
			TArray<int32> PassIndices;
			for (int32 PassIndex = 0; PassIndex < Pipeline.Passes.Num(); ++PassIndex)
			{
				if (Pipeline.Passes[PassIndex].Injection == Injection)
				{
					PassIndices.Add(PassIndex);
				}
			}
			if (PassIndices.IsEmpty())
			{
				continue;
			}

			const FString InjectionName = UE::DreamPass::LexToString(Injection);
			IDetailGroup& Group = Category.AddGroup(
				FName(*InjectionName),
				FText::Format(LOCTEXT("InjectionGroup", "{0} ({1})"), FText::FromString(InjectionName), FText::AsNumber(PassIndices.Num())),
				/*bForAdvanced*/ false,
				/*bStartExpanded*/ true);

			for (const int32 PassIndex : PassIndices)
			{
				const FDreamPassDesc& Pass = Pipeline.Passes[PassIndex];
				const PipelineDetails::FSlotStatus Slot = PipelineDetails::DescribeSlot(Pass);

				// The `.usf` behind an HLSL pass, when its virtual path maps to a file; or the line of the `.dsp` its HLSL is on.
				bool bCompute = false;
				int32 SlotIndex = INDEX_NONE;
				const bool bHlsl = PipelineDetails::GetHlslSlot(Pass, bCompute, SlotIndex);
				const int32 InlineLine = bHlsl ? PipelineDetails::GetInlineHlslLine(Pipeline, Pass) : 0;
				const FString ShaderFile = (bHlsl && InlineLine == 0) ? ResolveDreamPassShaderSourceFile(PipelineDetails::GetShaderPath(Pass), Pipeline.SourceFilePath) : FString();

				TSharedRef<SVerticalBox> Value = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(PipelineDetails::MakePassText(Weak, PassIndex, &PipelineDetails::PassSettingsText))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(PipelineDetails::MakePassText(Weak, PassIndex, &PipelineDetails::PassBindingsText))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					];

				if (!Slot.Text.IsEmpty())
				{
					Value->AddSlot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Slot.Text)
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(Slot.Color)
						.AutoWrapText(true)
					];
				}
				if (!ShaderFile.IsEmpty())
				{
					Value->AddSlot().AutoHeight()
					[
						PipelineDetails::MakeLinkButton(
							FText::Format(LOCTEXT("OpenShader", "Open {0}"), FText::FromString(FPaths::GetCleanFilename(ShaderFile))),
							FText::FromString(ShaderFile),
							[ShaderFile]() { FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(ShaderFile); })
					];
				}
				if (InlineLine > 0 && bHasDsp)
				{
					Value->AddSlot().AutoHeight()
					[
						PipelineDetails::MakeLinkButton(
							FText::Format(LOCTEXT("OpenInlineHlsl", "Open {0} at line {1}"), FText::FromString(FPaths::GetCleanFilename(DspFile)), FText::AsNumber(InlineLine)),
							FText::FromString(DspFile),
							[DspFile, InlineLine]() { FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(DspFile, InlineLine); })
					];
				}
				Value->AddSlot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PassSkipped", "The runtime skips this pass: see Problems above."))
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
					.ColorAndOpacity(FSlateColor(BrowserErrorColor))
					.Visibility_Lambda([Weak, PassIndex]()
					{
						const UDreamPassPipeline* Live = Weak.Get();
						return (Live && Live->Passes.IsValidIndex(PassIndex) && !Live->IsPassUsable(PassIndex)) ? EVisibility::Visible : EVisibility::Collapsed;
					})
				];

				Group.AddWidgetRow()
					.FilterString(FText::FromName(Pass.Name))
					.NameContent()
					[
						SNew(STextBlock)
						.Text(FText::FromName(Pass.Name))
						.ToolTipText(FText::FromString(Pass.Description))
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
					]
					.ValueContent()
					.MinDesiredWidth(420.0f)
					.MaxDesiredWidth(TOptional<float>())
					[
						Value
					];
			}
		}
	}

	void FDreamPassPipelineCustomization::BuildBuffers(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			PipelineDetails::BuffersCategory, LOCTEXT("BuffersCategory", "Buffers"), ECategoryPriority::Important);
		if (Pipeline.Buffers.IsEmpty())
		{
			Category.AddCustomRow(LOCTEXT("NoBuffersFilter", "Buffers"))
				.WholeRowContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoBuffers", "This pipeline declares no buffer: its passes use the built-in ones only."))
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				];
			return;
		}

		const TWeakObjectPtr<UDreamPassPipeline> Weak = WeakPipeline;
		for (int32 BufferIndex = 0; BufferIndex < Pipeline.Buffers.Num(); ++BufferIndex)
		{
			const FDreamPassBufferDesc& Buffer = Pipeline.Buffers[BufferIndex];

			TSharedRef<SVerticalBox> Value = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.AutoWrapText(true)
					.Text_Lambda([Weak, BufferIndex]()
					{
						const UDreamPassPipeline* Live = Weak.Get();
						return (Live && Live->Buffers.IsValidIndex(BufferIndex))
							? FText::FromString(PipelineDetails::BufferSettingsText(Live->Buffers[BufferIndex]))
							: FText::GetEmpty();
					})
				];

			if (Buffer.bExport)
			{
				// What ordinary materials, Blueprints, UMG and Niagara read: the compiler makes it next to the pipeline.
				if (UTextureRenderTarget2D* const Target = Pipeline.GetExportTarget(Buffer.Name))
				{
					const TWeakObjectPtr<UTextureRenderTarget2D> WeakTarget(Target);
					Value->AddSlot().AutoHeight()
					[
						PipelineDetails::MakeLinkButton(
							FText::Format(LOCTEXT("ExportTarget", "exported to {0}"), FText::FromString(Target->GetName())),
							FText::Format(LOCTEXT("ExportTargetTip", "{0}: show it in the Content Browser."), FText::FromString(Target->GetPathName())),
							[WeakTarget]()
							{
								if (UTextureRenderTarget2D* const Live = WeakTarget.Get(); Live && GEditor)
								{
									GEditor->SyncBrowserToObjects(TArray<UObject*>{ Live });
								}
							})
					];
				}
				else
				{
					Value->AddSlot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportTargetMissing", "exported, but its render target does not exist yet: compile the source"))
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor(PipelineDetails::WarningColor))
						.AutoWrapText(true)
					];
				}
			}

			Category.AddCustomRow(FText::FromName(Buffer.Name))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(FText::FromName(Buffer.Name))
					.ToolTipText(FText::FromString(Buffer.Description))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
				.ValueContent()
				.MinDesiredWidth(420.0f)
				.MaxDesiredWidth(TOptional<float>())
				[
					Value
				];
		}
	}

	void FDreamPassPipelineCustomization::OnPipelineChanged(UDreamPassPipeline* Changed)
	{
		UDreamPassPipeline* const Pipeline = WeakPipeline.Get();
		if (!Pipeline || Changed != Pipeline)
		{
			return;
		}

		*ProvenanceLabel = GetBrowserProvenanceLabel(ClassifyGeneratedAsset(Pipeline));

		// A value that moved is read by the rows already. Only a change of shape rebuilds -- and never in the middle of a
		// drag, which changes values and nothing else.
		const FString NewKey = MakeStructureKey(*Pipeline);
		if (NewKey != StructureKey)
		{
			StructureKey = NewKey;
			if (const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
			{
				Utilities->RequestForceRefresh();
			}
		}
	}

	FString FDreamPassPipelineCustomization::MakeStructureKey(const UDreamPassPipeline& Pipeline)
	{
		FString Key;
		for (const FDreamPassDesc& Pass : Pipeline.Passes)
		{
			bool bCompute = false;
			int32 Slot = INDEX_NONE;
			const bool bHlsl = PipelineDetails::GetHlslSlot(Pass, bCompute, Slot);
			Key += FString::Printf(TEXT("P:%s:%d:%d:%d:%d:%s;"), // I18N-EXEMPT: a key
				*Pass.Name.ToString(), int32(Pass.Kind), int32(Pass.Injection), bHlsl ? 1 : 0, Slot, *PipelineDetails::GetShaderPath(Pass));
		}
		for (const FDreamPassBufferDesc& Buffer : Pipeline.Buffers)
		{
			Key += FString::Printf(TEXT("B:%s:%d;"), *Buffer.Name.ToString(), Buffer.bExport ? 1 : 0); // I18N-EXEMPT: a key
		}
		for (const TPair<FName, TObjectPtr<UTextureRenderTarget2D>>& Target : Pipeline.ExportTargets)
		{
			Key += FString::Printf(TEXT("E:%s:%s;"), *Target.Key.ToString(), Target.Value ? *Target.Value->GetPathName() : TEXT("")); // I18N-EXEMPT: a key
		}
		return Key;
	}
}

#undef LOCTEXT_NAMESPACE
