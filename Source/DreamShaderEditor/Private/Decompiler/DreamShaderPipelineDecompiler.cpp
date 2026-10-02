// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPipelineDecompiler.h. This file is the mapping: which field of the asset becomes which field of the
// payload, and what the text cannot say.
//
// Diagnostics owned by this file: DSH9211-DSH9225.

#include "Decompiler/DreamShaderPipelineDecompiler.h"

#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamPassTypes.h"
// GetDreamShaderBuiltinCatalog: the re-parse check binds the printed text like a compile would, engine facts aside.
#include "DreamShaderBuiltinCatalog.h"
// ResolveDreamShaderProductDestination: where the printed file would put its asset (DSH9224).
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderModule.h"
// The bare-name rule, read the way a `.dsi`'s Parent is read: one product of that name under the root.
#include "DreamShaderProductIndex.h"
// FormatDreamShaderFloatLiteral: the shortest literal of a float, which is what the printer writes.
#include "Lang/LangHlslText.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangParser.h"
#include "Lang/LangPipelineSource.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Pass/DreamPassSpellings.h"
#include "Semantic/LangBound.h"
// ToInvariantWireString: the `// Warning:` lines are English in every culture.
#include "DreamShaderTextWireUtils.h"

#include "Engine/Texture.h"
#include "MaterialValueType.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CString.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "DreamShader.Decompiler.Pipeline"

namespace UE::DreamShader::Editor::Private
{
	namespace PipelineDecompile
	{
		namespace IR = UE::DreamShader::IR;
		namespace Lang = UE::DreamShader::Lang;
		using IR::FIRPropertyValue;
		using Lang::FLangSpan;

		/**
		 * The double the printer's literal for this float reads back as. The asset keeps float32; the printer writes the
		 * shortest literal that is that float again (`0.1`, not `0.100000001`), and the binder holds the literal as a
		 * double -- so this, and not the float widened, is what a payload re-read from the text holds.
		 */
		static double Canonical(const float Value)
		{
			return FCString::Atod(*Lang::FormatDreamShaderFloatLiteral(static_cast<double>(Value)));
		}

		static FString NameText(const FName Name)
		{
			return Name.IsNone() ? FString() : Name.ToString();
		}

		static FString ObjectPathOf(const UObject* Object)
		{
			return Object ? Object->GetPathName() : FString();
		}

		/** `/Game/X/PP_Glow` for the asset named after its package, the object path for anything else: a `.dsi`'s Parent spelling. */
		static FString MakeObjectReference(const UObject* Object)
		{
			const FString ObjectPath = Object->GetPathName();
			const FString PackageName = Object->GetOutermost() ? Object->GetOutermost()->GetName() : FString();
			return (!PackageName.IsEmpty() && FPackageName::GetShortName(PackageName).Equals(Object->GetName(), ESearchCase::CaseSensitive))
				? PackageName
				: ObjectPath;
		}

		/**
		 * A shader file in the one spelling the compiler gives FIRPass::ShaderFilePath (DreamShaderCompiler,
		 * Pass/DreamShaderPassShaderText.cpp, NormalizeDreamPassShaderFilePath): absolute, forward slashes, `..` collapsed.
		 * Not NormalizeSourceFilePath, whose MakeStandardFilename turns a file under the engine root into `../../../...`,
		 * which a payload the compiler made would never compare equal to.
		 */
		static FString NormalizeShaderFile(const FString& Path)
		{
			if (Path.IsEmpty())
			{
				return FString();
			}
			FString Result = FPaths::ConvertRelativePathToFull(Path);
			Result.ReplaceInline(TEXT("\\"), TEXT("/"));
			FPaths::CollapseRelativeDirectories(Result);
			FPaths::RemoveDuplicateSlashes(Result);
			return Result;
		}

		/** One file named twice: a relative and an absolute spelling, or two that differ in case only (Windows). */
		static bool IsSameShaderFile(const FString& A, const FString& B)
		{
			return !A.IsEmpty() && !B.IsEmpty() && NormalizeShaderFile(A).Equals(NormalizeShaderFile(B), ESearchCase::IgnoreCase);
		}

		/** One object named twice: `/Game/X/M` and `/Game/X/M.M` are the same asset. */
		static bool IsSameObjectPath(const FString& A, const FString& B)
		{
			const auto Expand = [](const FString& Path)
			{
				if (Path.StartsWith(TEXT("/")) && !Path.Contains(TEXT(".")))
				{
					return FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path));
				}
				return Path;
			};
			return Expand(A).Equals(Expand(B), ESearchCase::IgnoreCase);
		}

		/** The resolution a buffer gets when its source writes none: its first writer's (DreamShader_Plan/05 §4). */
		static EDreamPassBufferResolution InferBufferResolution(const UDreamPassPipeline& Pipeline, const FName Buffer, const bool bFrameOrder)
		{
			int32 First = INDEX_NONE;
			for (int32 PassIndex = 0; PassIndex < Pipeline.Passes.Num(); ++PassIndex)
			{
				const FDreamPassDesc& Pass = Pipeline.Passes[PassIndex];
				const bool bWrites = Pass.Writes.ContainsByPredicate([Buffer](const FDreamPassBufferBinding& Binding) { return Binding.Buffer == Buffer && !Binding.bPrevious; })
					|| (Pass.Kind == EDreamPassKind::Mesh && Pass.Mesh.Depth == EDreamPassDepthMode::Own && Pass.Mesh.DepthBuffer == Buffer);
				if (!bWrites)
				{
					continue;
				}
				if (!bFrameOrder)
				{
					First = PassIndex;
					break;
				}
				if (First == INDEX_NONE || UE::DreamPass::GetInjectionOrder(Pass.Injection) < UE::DreamPass::GetInjectionOrder(Pipeline.Passes[First].Injection))
				{
					First = PassIndex;
				}
			}
			if (First == INDEX_NONE)
			{
				return EDreamPassBufferResolution::Render;
			}
			return UE::DreamPass::IsOutputResolutionInjection(Pipeline.Passes[First].Injection)
				? EDreamPassBufferResolution::Output
				: EDreamPassBufferResolution::Render;
		}

		/** One decompile: the asset, what was asked, and the product index, refreshed at most once. */
		struct FContext
		{
			const UDreamPassPipeline& Pipeline;
			const FPipelineDecompileOptions& Options;
			Lang::FLangDiagnosticSink& Diagnostics;
			bool bIndexRefreshed = false;

			FContext(const UDreamPassPipeline& InPipeline, const FPipelineDecompileOptions& InOptions, Lang::FLangDiagnosticSink& InDiagnostics)
				: Pipeline(InPipeline)
				, Options(InOptions)
				, Diagnostics(InDiagnostics)
			{
			}

			/** The material's bare name, when the products under the target's own source root have exactly one of that name, it is this one, and a `.dss` builds it. */
			bool TryMakeBareMaterialName(const UMaterialInterface* Material, FString& OutName)
			{
				const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(Options.TargetSourceFilePath);
				if (!Root)
				{
					return false;
				}

				::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex& Index = ::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex::Get();
				if (!bIndexRefreshed)
				{
					Index.Refresh();
					bIndexRefreshed = true;
				}
				// Unique among the material products of the root, the way the reference stage reads a bare name back
				// (DreamShaderPipelineReferences.cpp): a pipeline of the same name does not make it ambiguous.
				TArray<const ::UE::DreamShader::Editor::Compiler::FDreamShaderProductRecord*> Records;
				Index.FindByName(Root->Directory, Material->GetName(), Records);
				Records.RemoveAll([](const ::UE::DreamShader::Editor::Compiler::FDreamShaderProductRecord* Record)
				{
					return Record->Kind != IR::EIRProductKind::Material && Record->Kind != IR::EIRProductKind::MaterialInstance;
				});
				if (Records.Num() != 1
					|| !Records[0]->ObjectPath.Equals(Material->GetPathName(), ESearchCase::IgnoreCase)
					|| !UE::DreamShader::IsDreamShaderLang2File(Records[0]->SourceFilePath))
				{
					return false;
				}
				OutName = Material->GetName();
				return true;
			}

			FString MakeMaterialReference(const UMaterialInterface* Material)
			{
				FString BareName;
				if (Options.bPreferBareMaterialNames && !Options.TargetSourceFilePath.IsEmpty() && TryMakeBareMaterialName(Material, BareName))
				{
					return BareName;
				}
				return MakeObjectReference(Material);
			}

			void FillParameter(const FDreamPassParameterDesc& Desc, IR::FIRPassParameter& Out)
			{
				Out.Name = NameText(Desc.Name);
				Out.Type = PassSpelling::ParameterType(Desc.Default.Type);
				switch (Desc.Default.Type)
				{
				case EDreamPassParameterType::Int:
					Out.Default = FIRPropertyValue::MakeInt(Desc.Default.Int);
					break;
				case EDreamPassParameterType::Bool:
					Out.Default = FIRPropertyValue::MakeBool(Desc.Default.Bool);
					break;
				case EDreamPassParameterType::Texture:
				{
					const UTexture* Texture = Desc.Default.Texture.Get();
					Out.Default = FIRPropertyValue::MakeObject(ObjectPathOf(Texture));
					// A `.dsp` texture parameter is a Texture2D, and the payload has no other spelling for its type.
					if (Texture && Texture->GetMaterialType() != MCT_Texture2D)
					{
						Diagnostics.Warning(TEXT("DSH9218"), FLangSpan(), FText::Format(
							LOCTEXT("TextureDefaultNot2DKept", "The parameter '{0}' defaults to '{1}', which is not a 2D texture. A '.dsp' declares every texture parameter 'Texture2D', and the text does so here too, keeping this default; it builds as it is."),
							FText::FromString(Out.Name),
							FText::FromString(Texture->GetPathName())));
					}
					break;
				}
				default:
				{
					const int32 Width = PassSpelling::ParameterWidth(Desc.Default.Type);
					const FVector4f& Vector = Desc.Default.Vector;
					double Channels[4] = { Canonical(Vector.X), Canonical(Vector.Y), Canonical(Vector.Z), Canonical(Vector.W) };
					for (int32 Channel = Width; Channel < 4; ++Channel)
					{
						Channels[Channel] = 0.0;
					}
					Out.Default = FIRPropertyValue::MakeFloat4(Channels, Width);
					break;
				}
				}
				Out.Group = Desc.Group;
				Out.Description = Desc.Description;
				Out.bHasSlider = Desc.bHasSlider;
				if (Desc.bHasSlider)
				{
					Out.SliderMin = Canonical(Desc.SliderMin);
					Out.SliderMax = Canonical(Desc.SliderMax);
				}
				Out.SortPriority = Desc.SortPriority;
			}

			void FillBuffer(const FDreamPassBufferDesc& Desc, IR::FIRPassBuffer& Out)
			{
				Out.Name = NameText(Desc.Name);
				Out.Format = UE::DreamPass::LexToString(Desc.Format);
				Out.Resolution = PassSpelling::Resolution(Desc.Resolution);
				if (Desc.Resolution == EDreamPassBufferResolution::Fixed)
				{
					// `Size = int2(w, h)`, which excludes Scale: the asset may still hold one, and the text has no place for it.
					Out.FixedWidth = Desc.FixedSize.X;
					Out.FixedHeight = Desc.FixedSize.Y;
					Out.bResolutionWritten = false;
				}
				else
				{
					Out.Scale = Canonical(Desc.Scale);
					// Written only where leaving it out would read back as another resolution. The binder infers it from the first
					// writer; "first" is read both ways here, so the key stays whenever the two readings could disagree.
					Out.bResolutionWritten = Desc.Resolution != InferBufferResolution(Pipeline, Desc.Name, /*bFrameOrder*/ true)
						|| Desc.Resolution != InferBufferResolution(Pipeline, Desc.Name, /*bFrameOrder*/ false);
				}
				Out.bClear = Desc.bClear;
				if (Desc.bClear)
				{
					Out.ClearValue[0] = Canonical(Desc.ClearValue.R);
					Out.ClearValue[1] = Canonical(Desc.ClearValue.G);
					Out.ClearValue[2] = Canonical(Desc.ClearValue.B);
					Out.ClearValue[3] = Canonical(Desc.ClearValue.A);
				}
				Out.Mips = Desc.Mips;
				Out.bHistory = Desc.bHistory;
				Out.bExport = Desc.bExport;
				Out.Description = Desc.Description;
			}

			static void FillBinding(const FDreamPassBufferBinding& In, IR::FIRPassBinding& Out)
			{
				Out.Buffer = NameText(In.Buffer);
				// A fullscreen material pass's one write has no name of its own in the asset; `write B;` is how the source said it.
				Out.Slot = In.Slot.IsNone() ? Out.Buffer : In.Slot.ToString();
				Out.bPrevious = In.bPrevious;
			}

			/** False when the binding has no `.dsp` spelling and is left out. */
			bool FillParam(const FDreamPassDesc& Pass, const FDreamPassParamBinding& In, IR::FIRPassParam& Out)
			{
				Out.Target = NameText(In.Target);
				Out.SourceKind = PassSpelling::ParamSource(In.Source);
				switch (In.Source)
				{
				case EDreamPassParamSource::Parameter:
					Out.Parameter = NameText(In.Parameter);
					Out.Multiplier = Canonical(In.Multiplier);
					Out.Offset = Canonical(In.Offset);
					return true;
				case EDreamPassParamSource::Weight:
					Out.Multiplier = Canonical(In.Multiplier);
					Out.Offset = Canonical(In.Offset);
					return true;
				case EDreamPassParamSource::Constant:
					break;
				}

				Out.ConstantType = PassSpelling::ParameterType(In.Constant.Type);
				switch (In.Constant.Type)
				{
				case EDreamPassParameterType::Int:
					Out.Constant = FIRPropertyValue::MakeInt(In.Constant.Int);
					return true;
				case EDreamPassParameterType::Bool:
					Out.Constant = FIRPropertyValue::MakeBool(In.Constant.Bool);
					return true;
				case EDreamPassParameterType::Texture:
					Diagnostics.Warning(TEXT("DSH9219"), FLangSpan(), FText::Format(
						LOCTEXT("TextureConstantParam", "The pass '{0}' binds '{1}' to a texture constant, which a 'param' cannot state; the binding is left out of the text."),
						FText::FromName(Pass.Name),
						FText::FromString(Out.Target)));
					return false;
				default:
				{
					const int32 Width = PassSpelling::ParameterWidth(In.Constant.Type);
					const FVector4f& Vector = In.Constant.Vector;
					double Channels[4] = { Canonical(Vector.X), Canonical(Vector.Y), Canonical(Vector.Z), Canonical(Vector.W) };
					for (int32 Channel = Width; Channel < 4; ++Channel)
					{
						Channels[Channel] = 0.0;
					}
					Out.Constant = FIRPropertyValue::MakeFloat4(Channels, Width);
					return true;
				}
				}
			}

			void FillShader(const FString& ShaderPath, const FString& Entry, IR::FIRPass& Out) const
			{
				// The asset stores the path as its `.dsp` wrote it. A virtual path reads the same from anywhere; a relative one is
				// relative to that `.dsp`, so a text written somewhere else -- Decompiled/Pipelines -- names the file relative to
				// itself. A pipeline that does not know its `.dsp` keeps the path as it is.
				Out.ShaderReference = ShaderPath;
				Out.ShaderVirtualPath = ShaderPath.StartsWith(TEXT("/")) ? ShaderPath : FString();
				Out.ShaderFilePath = ResolveDreamPassShaderSourceFile(ShaderPath, Pipeline.SourceFilePath);
				Out.Entry = Entry;

				if (Out.ShaderVirtualPath.IsEmpty() && !Out.ShaderFilePath.IsEmpty() && !Options.TargetSourceFilePath.IsEmpty())
				{
					FString FromTarget = Out.ShaderFilePath;
					const FString TargetDirectory = FPaths::GetPath(UE::DreamShader::NormalizeSourceFilePath(Options.TargetSourceFilePath)) / TEXT("");
					if (FPaths::MakePathRelativeTo(FromTarget, *TargetDirectory))
					{
						Out.ShaderReference = FromTarget;
					}
				}
			}

			/**
			 * A pass whose HLSL is in its `.dsp` (DreamShader_Plan/10): where it is, and its own block's text as it was written.
			 * False, with nothing filled, for a pass whose code is a shader file.
			 */
			bool FillInlineHlsl(const EDreamPassHlslSource Source, const FString& Entry, const FString& InlineHlsl, const int32 InlineHlslLine, IR::FIRPass& Out) const
			{
				switch (Source)
				{
				case EDreamPassHlslSource::Block:
					Out.HlslSource = IR::PassHlslSource::Block;
					break;
				case EDreamPassHlslSource::Body:
					Out.HlslSource = IR::PassHlslSource::Body;
					break;
				case EDreamPassHlslSource::Shared:
					Out.HlslSource = IR::PassHlslSource::Shared;
					break;
				case EDreamPassHlslSource::File:
				default:
					return false;
				}
				Out.Entry = Entry;
				if (Source != EDreamPassHlslSource::Shared)
				{
					Out.InlineHlsl = InlineHlsl;
					Out.InlineHlslLine = InlineHlslLine;
				}
				return true;
			}

			void FillFullscreen(const FDreamPassDesc& Pass, IR::FIRPass& Out)
			{
				const FDreamPassFullscreenSettings& Settings = Pass.Fullscreen;
				const bool bHasShader = !Settings.ShaderPath.IsEmpty();

				if (Settings.Material)
				{
					if (bHasShader)
					{
						Diagnostics.Warning(TEXT("DSH9212"), FLangSpan(), FText::Format(
							LOCTEXT("FullscreenMaterialAndShader", "The pass '{0}' has both a material, '{1}', and a shader, '{2}'; a fullscreen pass names one of the two, and the text keeps the material, which is what the pass draws."),
							FText::FromName(Pass.Name),
							FText::FromString(Settings.Material->GetPathName()),
							FText::FromString(Settings.ShaderPath)));
					}
					Out.MaterialReference = MakeMaterialReference(Settings.Material);
					Out.MaterialObjectPath = Settings.Material->GetPathName();
					return;
				}

				if (FillInlineHlsl(Settings.HlslSource, Settings.Entry, Settings.InlineHlsl, Settings.InlineHlslLine, Out))
				{
					return;
				}

				if (bHasShader)
				{
					FillShader(Settings.ShaderPath, Settings.Entry, Out);
					return;
				}

				Diagnostics.Warning(TEXT("DSH9211"), FLangSpan(), FText::Format(
					LOCTEXT("FullscreenNothing", "The pass '{0}' has neither a material nor a shader, so the text names neither, and it does not build until one is given."),
					FText::FromName(Pass.Name)));
			}

			void FillCompute(const FDreamPassDesc& Pass, IR::FIRPass& Out)
			{
				const FDreamPassComputeSettings& Settings = Pass.Compute;
				if (!FillInlineHlsl(Settings.HlslSource, Settings.Entry, Settings.InlineHlsl, Settings.InlineHlslLine, Out))
				{
					FillShader(Settings.ShaderPath, Settings.Entry, Out);
				}

				// `Threads` is written only where it differs from what the payload assumes unwritten. A source that left it to
				// the `.usf`'s [numthreads] built the same numbers, and a rebuild of the text reads them there again.
				const IR::FIRPass Unwritten;
				Out.ThreadsX = Settings.ThreadGroupSize.X;
				Out.ThreadsY = Settings.ThreadGroupSize.Y;
				Out.ThreadsZ = Settings.ThreadGroupSize.Z;
				Out.bThreadsWritten = Out.ThreadsX != Unwritten.ThreadsX || Out.ThreadsY != Unwritten.ThreadsY || Out.ThreadsZ != Unwritten.ThreadsZ;

				// An entry in the `.dsp` has its [numthreads] there to read: `Threads` repeats it only when they differ.
				const bool bOwnBlock = Out.HlslSource.Equals(IR::PassHlslSource::Block, ESearchCase::CaseSensitive);
				const bool bShared = Out.HlslSource.Equals(IR::PassHlslSource::Shared, ESearchCase::CaseSensitive);
				if (bOwnBlock || bShared)
				{
					const UE::DreamShader::Lang::FHlslTextScan Scan = UE::DreamShader::Lang::ScanHlslText(bShared ? Pipeline.SharedHlsl : Out.InlineHlsl);
					const UE::DreamShader::Lang::FHlslTopLevelFunction* Function = Scan.FindFunction(Out.Entry);
					if (Function && Function->bComputeEntry && Function->GroupSize == Settings.ThreadGroupSize)
					{
						Out.bThreadsWritten = false;
					}
				}

				Out.DispatchMode = PassSpelling::DispatchMode(Settings.DispatchMode);
				if (Settings.DispatchMode == EDreamPassDispatchMode::Buffer)
				{
					Out.DispatchBuffer = NameText(Settings.DispatchBuffer);
					Out.DispatchScale = Canonical(Settings.DispatchScale);
				}
				else
				{
					Out.DispatchX = Settings.DispatchSize.X;
					Out.DispatchY = Settings.DispatchSize.Y;
					Out.DispatchZ = Settings.DispatchSize.Z;
				}
			}

			void FillFilterTerm(const FDreamPassDesc& Pass, const FDreamPassFilterTerm& In, IR::FIRPassFilterTerm& Out)
			{
				Out.Kind = PassSpelling::FilterKind(In.Kind);
				switch (In.Kind)
				{
				case EDreamPassFilterKind::Stencil:
					Out.StencilValue = In.StencilValue;
					Out.StencilMask = In.StencilMask;
					return;
				case EDreamPassFilterKind::List:
					Out.List = NameText(In.ListName);
					return;
				case EDreamPassFilterKind::Layer:
					break;
				}

				// The names as they were spelled, which the compiler keeps beside the bits for exactly this.
				if (!In.LayerNames.IsEmpty())
				{
					for (const FName Layer : In.LayerNames)
					{
						Out.Layers.Add(NameText(Layer));
					}
					return;
				}
				if (In.LayerMask == 0)
				{
					return;
				}

				// A term edited by hand, or made before the names were kept: the bits, read through today's layer table.
				const TArray<FName>& Table = UDreamPassSettings::Get().LayerNames;
				const uint32 Mask = static_cast<uint32>(In.LayerMask);
				TArray<FString> Dropped;
				for (int32 Bit = 0; Bit < UDreamPassSettings::MaxLayers; ++Bit)
				{
					if ((Mask & (1u << Bit)) == 0)
					{
						continue;
					}
					if (Table.IsValidIndex(Bit) && !Table[Bit].IsNone())
					{
						Out.Layers.Add(Table[Bit].ToString());
					}
					else
					{
						Dropped.Add(FString::FromInt(Bit));
					}
				}
				if (!Out.Layers.IsEmpty())
				{
					Diagnostics.Info(TEXT("DSH9214"), FLangSpan(), FText::Format(
						LOCTEXT("LayerNamesFromTable", "A layer filter of the pass '{0}' kept no spelling of its layers, so they are written as the project's layer table names its bits today: {1}."),
						FText::FromName(Pass.Name),
						FText::FromString(FString::Join(Out.Layers, TEXT(" | ")))));
				}
				if (!Dropped.IsEmpty())
				{
					Diagnostics.Warning(TEXT("DSH9215"), FLangSpan(), FText::Format(
						LOCTEXT("LayerBitsUnnamed", "A layer filter of the pass '{0}' selects layer bit(s) {1}, which the project's layer table has no name for; the text cannot say them and leaves them out."),
						FText::FromName(Pass.Name),
						FText::FromString(FString::Join(Dropped, TEXT(", ")))));
				}
			}

			void FillMesh(const FDreamPassDesc& Pass, IR::FIRPass& Out)
			{
				const FDreamPassMeshSettings& Settings = Pass.Mesh;
				for (const FDreamPassFilterClause& Clause : Settings.Filter.AnyOf)
				{
					IR::FIRPassFilterClause& OutClause = Out.Filter.AddDefaulted_GetRef();
					for (const FDreamPassFilterTerm& Term : Clause.AllOf)
					{
						FillFilterTerm(Pass, Term, OutClause.AllOf.AddDefaulted_GetRef());
					}
				}

				Out.MeshMode = PassSpelling::MeshMode(Settings.Mode);
				if (Settings.OverrideMaterial)
				{
					Out.MaterialReference = MakeMaterialReference(Settings.OverrideMaterial);
					Out.MaterialObjectPath = Settings.OverrideMaterial->GetPathName();
				}

				Out.Depth = PassSpelling::Depth(Settings.Depth);
				if (Settings.Depth == EDreamPassDepthMode::Own)
				{
					Out.DepthBuffer = NameText(Settings.DepthBuffer);
					if (Settings.DepthBuffer.IsNone())
					{
						Diagnostics.Warning(TEXT("DSH9220"), FLangSpan(), FText::Format(
							LOCTEXT("OwnDepthWithoutBuffer", "The pass '{0}' tests against its own depth but names no Depth32 buffer for it; the text writes 'Own()' empty, and it does not build until one is named."),
							FText::FromName(Pass.Name)));
					}
				}
				Out.Cull = PassSpelling::Cull(Settings.Cull);
				Out.Blend = PassSpelling::Blend(Settings.Blend);

				// Empty is the default set, so an empty mask has no spelling of its own.
				if (Settings.Usage == 0)
				{
					Diagnostics.Warning(TEXT("DSH9217"), FLangSpan(), FText::Format(
						LOCTEXT("MeshUsageEmpty", "The pass '{0}' checks its override material for no usage flag at all, which a '.dsp' cannot say; the text leaves 'Usage' out, and a rebuild checks the default set ({1})."),
						FText::FromName(Pass.Name),
						FText::FromString(PassSpelling::JoinFlags(PassSpelling::MeshUsageFlags(), PassSpelling::DefaultMeshUsageMask()))));
				}
				else if (Settings.Usage != PassSpelling::DefaultMeshUsageMask())
				{
					Out.Usage = PassSpelling::FlagNames(PassSpelling::MeshUsageFlags(), Settings.Usage);
				}

				Out.Nanite = PassSpelling::Nanite(Settings.Nanite);
				if (Settings.Nanite == EDreamPassNanitePolicy::AssignStencil)
				{
					Out.AssignedStencilValue = Settings.AssignedStencilValue;
				}
				if (Settings.Nanite != EDreamPassNanitePolicy::Skip)
				{
					// What StencilMask writes; AssignStencil ends in a StencilMask too.
					Out.NaniteValue[0] = Canonical(Settings.NaniteValue.R);
					Out.NaniteValue[1] = Canonical(Settings.NaniteValue.G);
					Out.NaniteValue[2] = Canonical(Settings.NaniteValue.B);
					Out.NaniteValue[3] = Canonical(Settings.NaniteValue.A);
				}
			}

			void FillPass(const FDreamPassDesc& Pass, IR::FIRPass& Out)
			{
				Out.Name = NameText(Pass.Name);
				Out.Kind = PassSpelling::Kind(Pass.Kind);
				Out.Injection = UE::DreamPass::LexToString(Pass.Injection);
				// Every compiled pass carries its own; the source wrote one where it is not the pipeline's default.
				Out.bInjectionWritten = Pass.Injection != Pipeline.DefaultInjection;
				Out.EnabledParameter = NameText(Pass.EnabledParameter);
				Out.Description = Pass.Description;

				for (const FDreamPassBufferBinding& Read : Pass.Reads)
				{
					FillBinding(Read, Out.Reads.AddDefaulted_GetRef());
				}
				for (const FDreamPassBufferBinding& Write : Pass.Writes)
				{
					FillBinding(Write, Out.Writes.AddDefaulted_GetRef());
				}
				for (const FDreamPassParamBinding& Param : Pass.Params)
				{
					IR::FIRPassParam OutParam;
					if (FillParam(Pass, Param, OutParam))
					{
						Out.Params.Add(MoveTemp(OutParam));
					}
				}

				switch (Pass.Kind)
				{
				case EDreamPassKind::Fullscreen:
					FillFullscreen(Pass, Out);
					break;
				case EDreamPassKind::Compute:
					FillCompute(Pass, Out);
					break;
				case EDreamPassKind::Mesh:
					FillMesh(Pass, Out);
					break;
				case EDreamPassKind::Clear:
					Out.ClearValue[0] = Canonical(Pass.Clear.Value.R);
					Out.ClearValue[1] = Canonical(Pass.Clear.Value.G);
					Out.ClearValue[2] = Canonical(Pass.Clear.Value.B);
					Out.ClearValue[3] = Canonical(Pass.Clear.Value.A);
					break;
				case EDreamPassKind::Copy:
					// A read and a write, and nothing else.
					break;
				}
			}
		};

		/** `// Warning: DSH9215: ...` lines for the file's head, one per warning, single-line. */
		static void CollectWarningComments(const Lang::FLangDiagnosticSink& Sink, TArray<FString>& OutLines)
		{
			for (const Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (Diagnostic.Severity != Lang::ELangSeverity::Warning)
				{
					continue;
				}
				// English whatever the editor's culture: the comment is part of a source file the team shares.
				FString Message = ToInvariantWireString(Diagnostic.Message);
				Message.ReplaceInline(TEXT("\r"), TEXT(" "));
				Message.ReplaceInline(TEXT("\n"), TEXT(" "));
				OutLines.Add(FString::Printf(TEXT("Warning: %s: %s"), *Diagnostic.Code, *Message)); // I18N-EXEMPT: a comment in a source file
			}
		}

		/** Leading comment trivia on the first declaration, the way BuildDreamShaderAstFromIR writes its HeaderComments. */
		static void ApplyHeaderComments(Lang::FModule& Module, const TArray<FString>& Lines)
		{
			if (Lines.IsEmpty())
			{
				return;
			}

			TArray<Lang::FLangComment> Comments;
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				FString Line = Lines[Index];
				Line.ReplaceInline(TEXT("\r"), TEXT(" "));
				Line.ReplaceInline(TEXT("\n"), TEXT(" "));
				Line.TrimStartAndEndInline();
				Lang::FLangComment Comment;
				Comment.Text = Line.IsEmpty() ? FString(TEXT("//")) : FString(TEXT("// ")) + Line;
				Comment.Span.Line = Index + 1;
				Comments.Add(MoveTemp(Comment));
			}

			if (Module.Declarations.Num() == 0 || !Module.Declarations[0])
			{
				Module.TrailingComments.Insert(MoveTemp(Comments), 0);
				return;
			}

			// The printer leaves a blank line under leading comments where the node's line says there was one.
			Lang::FDecl& First = *Module.Declarations[0];
			First.Span.Line = Comments.Num() + 2;
			First.Span.Length = FMath::Max(1, First.Span.Length);
			Module.Trivia.FindOrAdd(&First).Leading.Insert(MoveTemp(Comments), 0);
		}

		static void SnapDouble(double& Desired, const double Current)
		{
			// Equal at the precision the asset keeps: the source's own double (`1.0 / 3.0` folded) stands.
			if (static_cast<float>(Desired) == static_cast<float>(Current))
			{
				Desired = Current;
			}
		}

		static void SnapPropertyValue(FIRPropertyValue& Desired, const FIRPropertyValue& Current)
		{
			if (Desired.Kind != Current.Kind)
			{
				return;
			}
			switch (Desired.Kind)
			{
			case IR::EIRPropertyKind::Float:
				SnapDouble(Desired.F, Current.F);
				return;
			case IR::EIRPropertyKind::Float4:
				if (Desired.N == Current.N)
				{
					for (int32 Channel = 0; Channel < FMath::Clamp(Desired.N, 0, 4); ++Channel)
					{
						SnapDouble(Desired.V[Channel], Current.V[Channel]);
					}
				}
				return;
			case IR::EIRPropertyKind::Object:
				if (!Desired.S.IsEmpty() && IsSameObjectPath(Desired.S, Current.S))
				{
					Desired.S = Current.S;
				}
				return;
			default:
				return;
			}
		}

		/** A list of flag spellings as a set: `Game | Editor` and `Editor | Game`, or an empty list and the default set, are one value. */
		static void SnapFlagList(TArray<FString>& Desired, const TArray<FString>& Current, const TArray<FString>& DefaultSet)
		{
			const auto AsSet = [&DefaultSet](const TArray<FString>& List)
			{
				TSet<FString> Set;
				for (const FString& Name : (List.IsEmpty() ? DefaultSet : List))
				{
					Set.Add(Name);
				}
				return Set;
			};
			const TSet<FString> DesiredSet = AsSet(Desired);
			const TSet<FString> CurrentSet = AsSet(Current);
			if (DesiredSet.Num() == CurrentSet.Num() && DesiredSet.Includes(CurrentSet))
			{
				Desired = Current;
			}
		}

		template <typename TItem>
		static const TItem* FindByName(const TArray<TItem>& Items, const FString& Name)
		{
			return Items.FindByPredicate([&Name](const TItem& Item) { return Item.Name.Equals(Name, ESearchCase::CaseSensitive); });
		}

		/**
		 * What an engine-free bind cannot know, taken from Expected where the re-read text spells the reference as Expected
		 * does: the object a material reference resolves to, the virtual path and file of a shader, a `.usf`'s [numthreads].
		 */
		static void TakeEngineFacts(const IR::FIRPassPipeline& Expected, IR::FIRPassPipeline& Reread)
		{
			for (IR::FIRPass& Pass : Reread.Passes)
			{
				const IR::FIRPass* Known = FindByName(Expected.Passes, Pass.Name);
				if (!Known)
				{
					continue;
				}
				if (!Pass.MaterialReference.IsEmpty() && Pass.MaterialReference.Equals(Known->MaterialReference, ESearchCase::CaseSensitive))
				{
					Pass.MaterialObjectPath = Known->MaterialObjectPath;
				}
				if (!Pass.ShaderReference.IsEmpty() && Pass.ShaderReference.Equals(Known->ShaderReference, ESearchCase::CaseSensitive))
				{
					Pass.ShaderVirtualPath = Known->ShaderVirtualPath;
					Pass.ShaderFilePath = Known->ShaderFilePath;
				}
				if (!Pass.bThreadsWritten && !Known->bThreadsWritten)
				{
					Pass.ThreadsX = Known->ThreadsX;
					Pass.ThreadsY = Known->ThreadsY;
					Pass.ThreadsZ = Known->ThreadsZ;
				}
			}
		}
	}

	bool DecompileDreamPassPipeline(
		const UDreamPassPipeline* Pipeline,
		const FPipelineDecompileOptions& Options,
		UE::DreamShader::IR::FIRPassPipeline& OutPipeline,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		namespace IR = UE::DreamShader::IR;
		using UE::DreamShader::Lang::FLangSpan;

		OutPipeline = IR::FIRPassPipeline();
		if (!Pipeline)
		{
			return Diagnostics.Error(TEXT("DSH9225"), FLangSpan(), LOCTEXT("NoPipeline", "There is no pass pipeline to decompile."));
		}
		const int32 ErrorsBefore = Diagnostics.NumErrors();

		PipelineDecompile::FContext Context(*Pipeline, Options, Diagnostics);

		OutPipeline.Order = Pipeline->Order;
		OutPipeline.DefaultInjection = UE::DreamPass::LexToString(Pipeline->DefaultInjection);
		OutPipeline.EnabledParameter = PipelineDecompile::NameText(Pipeline->EnabledParameter);

		// Empty is Game | Editor, so the default and the empty mask share a spelling, and only the default may have it.
		if (Pipeline->Views == 0)
		{
			Diagnostics.Warning(TEXT("DSH9216"), FLangSpan(), FText::Format(
				LOCTEXT("ViewsEmpty", "'{0}' runs in no kind of view, which a '.dsp' cannot say; the text leaves 'Views' out, and a rebuild runs in {1}."),
				FText::FromString(Pipeline->GetPathName()),
				FText::FromString(PassSpelling::JoinFlags(PassSpelling::ViewFlags(), PassSpelling::DefaultViewMask()))));
		}
		else if (Pipeline->Views != PassSpelling::DefaultViewMask())
		{
			OutPipeline.Views = PassSpelling::FlagNames(PassSpelling::ViewFlags(), Pipeline->Views);
		}
		OutPipeline.Requires = PassSpelling::FlagNames(PassSpelling::RequirementFlags(), Pipeline->Requires);
#if WITH_EDITORONLY_DATA
		// The file's `hlsl` block, as it was written: what each inline pass's slot was generated from.
		OutPipeline.bHasSharedHlsl = Pipeline->bHasSharedHlsl;
		OutPipeline.SharedHlsl = Pipeline->SharedHlsl;
		OutPipeline.SharedHlslLine = Pipeline->SharedHlslLine;
#endif

		for (const FDreamPassParameterDesc& Parameter : Pipeline->Parameters)
		{
			Context.FillParameter(Parameter, OutPipeline.Parameters.AddDefaulted_GetRef());
		}
		for (const FDreamPassBufferDesc& Buffer : Pipeline->Buffers)
		{
			Context.FillBuffer(Buffer, OutPipeline.Buffers.AddDefaulted_GetRef());
		}
		for (const FDreamPassDesc& Pass : Pipeline->Passes)
		{
			Context.FillPass(Pass, OutPipeline.Passes.AddDefaulted_GetRef());
		}

		return Diagnostics.NumErrors() == ErrorsBefore;
	}

	bool DecompileDreamPassPipelineToText(
		const UDreamPassPipeline* Pipeline,
		const FPipelineDecompileOptions& Options,
		FString& OutText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		namespace IR = UE::DreamShader::IR;
		namespace Lang = UE::DreamShader::Lang;
		using Lang::FLangSpan;

		OutText.Reset();

		// The decompile's own warnings go into the file's head; the ones the check below raises are about the decompiler.
		Lang::FLangDiagnosticSink DecompileDiagnostics(Diagnostics.GetFilePath());
		IR::FIRPassPipeline Payload;
		const bool bDecompiled = DecompileDreamPassPipeline(Pipeline, Options, Payload, DecompileDiagnostics);
		TArray<FString> HeaderLines = Options.HeaderComments;
		PipelineDecompile::CollectWarningComments(DecompileDiagnostics, HeaderLines);
		Diagnostics.Append(MoveTemp(DecompileDiagnostics));
		if (!bDecompiled)
		{
			return false;
		}

		const FString FilePath = Options.TargetSourceFilePath.IsEmpty()
			? FString::Printf(TEXT("%s.dsp"), *Pipeline->GetName()) // I18N-EXEMPT: file name
			: Options.TargetSourceFilePath;

		// A `.dsp` names its asset after its file and has no `/// @name`: the text can keep the asset's path only by living
		// where the asset's source would.
		if (Options.bKeepAssetPath && Pipeline->GetOutermost() && Pipeline->GetOutermost() != GetTransientPackage())
		{
			const FString PackageName = Pipeline->GetOutermost()->GetName();
			IR::FIRProduct Probe;
			Probe.Kind = IR::EIRProductKind::PassPipeline;
			Probe.Name = FPaths::GetBaseFilename(FilePath);
			FString DerivedPackage;
			FString DerivedObjectPath;
			FString DestinationError;
			const bool bDerived = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderProductDestination(
				Probe, FilePath, DerivedPackage, DerivedObjectPath, DestinationError);
			if (!bDerived || !DerivedPackage.Equals(PackageName, ESearchCase::IgnoreCase))
			{
				Diagnostics.Warning(TEXT("DSH9224"), FLangSpan(), FText::Format(
					LOCTEXT("PipelinePathNotKept", "A '.dsp' names its pipeline after its file and has no '/// @name', so '{0}' builds '{1}' where it is written, not '{2}'; move the file to where the pipeline's source belongs to keep its path."),
					FText::FromString(FilePath),
					FText::FromString(bDerived ? DerivedPackage : DestinationError),
					FText::FromString(PackageName)));
			}
		}

		const TUniquePtr<Lang::FModule> Module = Lang::BuildDreamShaderPipelineModule(Payload, FilePath);
		if (!Module.IsValid())
		{
			return Diagnostics.Error(TEXT("DSH9225"), FLangSpan(), FText::Format(
				LOCTEXT("PipelineTreeNotBuilt", "The text of '{0}' could not be laid out; this is a defect of the decompiler, not of the asset."),
				FText::FromString(Pipeline->GetPathName())));
		}
		PipelineDecompile::ApplyHeaderComments(*Module, HeaderLines);
		OutText = Lang::PrintDreamShaderLang(*Module);

		// The round trip a `.dsp` promises (DreamShader_Plan/05 §8): the text, read again, is the asset.
		CheckDreamShaderPipelineText(FilePath, OutText, Payload, Diagnostics);
		return true;
	}

	bool CheckDreamShaderPipelineText(
		const FString& FilePath,
		const FString& Text,
		const UE::DreamShader::IR::FIRPassPipeline& Expected,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		namespace IR = UE::DreamShader::IR;
		namespace Lang = UE::DreamShader::Lang;
		using Lang::FLangSpan;

		// Auto: the `.dsp` extension takes the pipeline front end.
		const Lang::FLangSourceText Printed(FilePath, Text);
		Lang::FLangParseOptions ParseOptions;
		ParseOptions.Frontend = Lang::ELangFrontend::Auto;
		const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Printed, ParseOptions);
		if (!Parsed.Succeeded())
		{
			const Lang::FLangDiagnostic* First = Parsed.Diagnostics.FirstError();
			Diagnostics.Warning(TEXT("DSH9221"), First ? First->Span : FLangSpan(), FText::Format(
				LOCTEXT("PipelineTextDoesNotParse", "The decompiled pipeline does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler."),
				FText::FromString(First ? First->Code : FString(TEXT("DSH0000"))),
				First ? First->Message : LOCTEXT("PipelineTextNoParseReason", "the parser gave no reason")));
			return false;
		}

		// Engine-free: the references are taken as written, and every check that needs the project is skipped. The bound
		// pipeline's payload is what its PassPipeline product carries (the IR builder copies it over, stamping files).
		Lang::FBindOptions BindOptions;
		BindOptions.Catalog = &::UE::DreamShader::Editor::Compiler::GetDreamShaderBuiltinCatalog();
		BindOptions.PipelineReferences = nullptr;
		const Lang::FLangBindResult Bound = Lang::BindDreamShaderLang(*Parsed.Module, BindOptions);
		if (!Bound.Succeeded() || !Bound.Bound->Pipeline.bIsPipeline)
		{
			const Lang::FLangDiagnostic* First = Bound.Diagnostics.FirstError();
			Diagnostics.Warning(TEXT("DSH9222"), First ? First->Span : FLangSpan(), FText::Format(
				LOCTEXT("PipelineTextDoesNotBindEither", "The decompiled pipeline parses but does not bind back into a pipeline: {0}: {1}. It is written as it is; either the asset breaks a rule a '.dsp' is checked against (an edit by hand can), or this is a defect of the decompiler."),
				FText::FromString(First ? First->Code : FString(TEXT("DSH0000"))),
				First ? First->Message : LOCTEXT("PipelineTextNoProduct", "the text builds no pipeline")));
			return false;
		}

		IR::FIRPassPipeline Reread = Bound.Bound->Pipeline.Payload;
		PipelineDecompile::TakeEngineFacts(Expected, Reread);
		KeepEquivalentPipelineSpellings(Expected, Reread);

		TArray<FString> Differences;
		if (!Lang::CompareDreamShaderPipelines(Expected, Reread, &Differences))
		{
			Diagnostics.Warning(TEXT("DSH9223"), FLangSpan(), FText::Format(
				LOCTEXT("PipelineTextDiffers", "The decompiled pipeline reads back as a different pipeline ({0} difference(s); the first: {1}). It is written as it is; this is a defect of the decompiler or the printer."),
				FText::AsNumber(Differences.Num()),
				FText::FromString(Differences.IsEmpty() ? FString(TEXT("-")) : Differences[0])));
			return false;
		}
		return true;
	}

	void KeepEquivalentPipelineSpellings(const UE::DreamShader::IR::FIRPassPipeline& Current, UE::DreamShader::IR::FIRPassPipeline& Desired)
	{
		namespace IR = UE::DreamShader::IR;
		using namespace PipelineDecompile;

		const TArray<FString> DefaultViews = PassSpelling::FlagNames(PassSpelling::ViewFlags(), PassSpelling::DefaultViewMask());
		SnapFlagList(Desired.Views, Current.Views, DefaultViews);
		SnapFlagList(Desired.Requires, Current.Requires, TArray<FString>());

		for (IR::FIRPassParameter& Parameter : Desired.Parameters)
		{
			const IR::FIRPassParameter* Was = FindByName(Current.Parameters, Parameter.Name);
			if (!Was || !Parameter.Type.Equals(Was->Type, ESearchCase::CaseSensitive))
			{
				continue;
			}
			SnapPropertyValue(Parameter.Default, Was->Default);
			if (Parameter.bHasSlider && Was->bHasSlider)
			{
				SnapDouble(Parameter.SliderMin, Was->SliderMin);
				SnapDouble(Parameter.SliderMax, Was->SliderMax);
			}
		}

		for (IR::FIRPassBuffer& Buffer : Desired.Buffers)
		{
			const IR::FIRPassBuffer* Was = FindByName(Current.Buffers, Buffer.Name);
			if (!Was)
			{
				continue;
			}
			SnapDouble(Buffer.Scale, Was->Scale);
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				SnapDouble(Buffer.ClearValue[Channel], Was->ClearValue[Channel]);
			}
			// A `Resolution = ...` the author wrote although it is the inferred one stays written, and the other way round.
			if (Buffer.Resolution.Equals(Was->Resolution, ESearchCase::CaseSensitive))
			{
				Buffer.bResolutionWritten = Was->bResolutionWritten;
			}
		}

		const TArray<FString> DefaultUsage = PassSpelling::FlagNames(PassSpelling::MeshUsageFlags(), PassSpelling::DefaultMeshUsageMask());
		for (IR::FIRPass& Pass : Desired.Passes)
		{
			const IR::FIRPass* Was = FindByName(Current.Passes, Pass.Name);
			if (!Was || !Pass.Kind.Equals(Was->Kind, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Pass.Injection.Equals(Was->Injection, ESearchCase::CaseSensitive))
			{
				Pass.bInjectionWritten = Was->bInjectionWritten;
			}

			// The same asset, spelled as the author spelled it: a bare name stays bare, a package path stays one.
			if (!Pass.MaterialObjectPath.IsEmpty() && !Was->MaterialObjectPath.IsEmpty() && IsSameObjectPath(Pass.MaterialObjectPath, Was->MaterialObjectPath))
			{
				Pass.MaterialReference = Was->MaterialReference;
				Pass.MaterialObjectPath = Was->MaterialObjectPath;
			}
			// The same file: a path relative to the `.dsp` stays relative, spelled as the author spelled it (`./Blur.usf`,
			// `Passes/../Blur.usf`). A relative reference decompiles without a virtual path, so the file decides as well.
			const bool bSameVirtualPath = !Pass.ShaderVirtualPath.IsEmpty() && Pass.ShaderVirtualPath.Equals(Was->ShaderVirtualPath, ESearchCase::IgnoreCase);
			if (bSameVirtualPath || IsSameShaderFile(Pass.ShaderFilePath, Was->ShaderFilePath))
			{
				Pass.ShaderReference = Was->ShaderReference;
				Pass.ShaderVirtualPath = Was->ShaderVirtualPath;
				Pass.ShaderFilePath = Was->ShaderFilePath;
			}

			if (Pass.ThreadsX == Was->ThreadsX && Pass.ThreadsY == Was->ThreadsY && Pass.ThreadsZ == Was->ThreadsZ)
			{
				Pass.bThreadsWritten = Was->bThreadsWritten;
			}
			SnapDouble(Pass.DispatchScale, Was->DispatchScale);
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				SnapDouble(Pass.NaniteValue[Channel], Was->NaniteValue[Channel]);
				SnapDouble(Pass.ClearValue[Channel], Was->ClearValue[Channel]);
			}
			SnapFlagList(Pass.Usage, Was->Usage, DefaultUsage);

			for (int32 Index = 0; Index < Pass.Params.Num() && Index < Was->Params.Num(); ++Index)
			{
				IR::FIRPassParam& Param = Pass.Params[Index];
				const IR::FIRPassParam& WasParam = Was->Params[Index];
				if (!Param.Target.Equals(WasParam.Target, ESearchCase::CaseSensitive) || !Param.SourceKind.Equals(WasParam.SourceKind, ESearchCase::CaseSensitive))
				{
					continue;
				}
				SnapDouble(Param.Multiplier, WasParam.Multiplier);
				SnapDouble(Param.Offset, WasParam.Offset);
				if (Param.ConstantType.Equals(WasParam.ConstantType, ESearchCase::CaseSensitive))
				{
					SnapPropertyValue(Param.Constant, WasParam.Constant);
				}
			}
		}
	}

	FString ResolveDreamPassShaderSourceFile(const FString& ShaderPath, const FString& PipelineSourceFile)
	{
		if (ShaderPath.IsEmpty())
		{
			return FString();
		}
		if (!ShaderPath.StartsWith(TEXT("/")))
		{
			// Relative to the `.dsp`: the engine never reads the file itself -- it compiles the slot's snapshot -- so the
			// author may keep it anywhere, and next to the pipeline is the usual place. The asset records its `.dsp`
			// project-relative (MakeProjectRelativeSourcePath), absolute only when it lies outside the project.
			if (PipelineSourceFile.IsEmpty())
			{
				return FString();
			}
			const FString PipelineFile = FPaths::IsRelative(PipelineSourceFile)
				? FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), PipelineSourceFile)
				: PipelineSourceFile;
			// Spelled as the compiler spells the file it resolves for the same reference (ResolveDreamPassShaderReference), so
			// a decompiled payload and a compiled one name one file the same way.
			return PipelineDecompile::NormalizeShaderFile(FPaths::Combine(FPaths::GetPath(PipelineFile), ShaderPath));
		}
		const TMap<FString, FString>& Mappings = AllShaderSourceDirectoryMappings();
		FString Directory = FPaths::GetPath(ShaderPath);
		FString Relative = FPaths::GetCleanFilename(ShaderPath);
		while (!Directory.IsEmpty())
		{
			if (const FString* Mapped = Mappings.Find(Directory))
			{
				return PipelineDecompile::NormalizeShaderFile(FPaths::Combine(*Mapped, Relative));
			}
			Relative = FPaths::GetCleanFilename(Directory) / Relative;
			Directory = FPaths::GetPath(Directory);
		}
		return FString();
	}

	const UE::DreamShader::Lang::FLangDiagnostic* FindPipelineWriteBackBlocker(const UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		static const TCHAR* const BlockingCodes[] =
		{
			TEXT("DSH9211"), // a fullscreen pass with neither a material nor a shader
			TEXT("DSH9212"), // ... with both: the shader is dropped
			TEXT("DSH9215"), // layer bits without a name: dropped
			TEXT("DSH9216"), // no view at all: becomes the default
			TEXT("DSH9217"), // no usage flag at all: becomes the default set
			TEXT("DSH9219"), // a texture constant in a param: dropped
			TEXT("DSH9220"), // own depth without a buffer
		};
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Error)
			{
				return &Diagnostic;
			}
			for (const TCHAR* Code : BlockingCodes)
			{
				if (Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive))
				{
					return &Diagnostic;
				}
			}
		}
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
