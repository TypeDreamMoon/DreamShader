// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderIREmitterPassPipeline.h.
//
// Diagnostics owned by this file: DSH8300-DSH8314.
//
//   destination + create/reuse     the ownership guard: a saved asset DreamShader did not generate is refused
//   stage                           the payload onto a transient pipeline: every spelling mapped, every asset loaded
//   the gates                       write owner (DSH8209), source hash (DSH8237), open editor, divergence -- the emitter's own
//   slots                           planned, snapshots built and pre-checked in memory           nothing written yet
//   render targets                  found, or made in memory, behind the same ownership guard   nothing written yet
//   commit                          snapshots, Registry.json, the registry files
//   the asset                       render target settings in place, its data, NotifyChanged, the hot reload of the slot
//                                   shaders (editor only), metadata, the digest, one save with the render targets
//   cleanup                         render targets of buffers no longer exported, unless something else reads them

#include "Emitter/DreamShaderIREmitterPassPipeline.h"

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "Emitter/DreamShaderIRAssets.h"
#include "Pass/DreamShaderPassSlotRegistry.h"

#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamPassTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter.PassPipeline"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderPassPipelineEmitDetail
	{
		template <typename EnumType>
		struct TSpelling
		{
			const TCHAR* Name;
			EnumType Value;
		};

		/** The language is case-sensitive, and so is every table below. */
		template <typename EnumType, int32 Count>
		bool TryParseSpelling(const FString& Text, const TSpelling<EnumType> (&Table)[Count], EnumType& OutValue)
		{
			for (const TSpelling<EnumType>& Entry : Table)
			{
				if (Text.Equals(Entry.Name, ESearchCase::CaseSensitive))
				{
					OutValue = Entry.Value;
					return true;
				}
			}
			return false;
		}

		const TSpelling<EDreamPassViewFlags> ViewSpellings[] =
		{
			{ TEXT("Game"),              EDreamPassViewFlags::Game },
			{ TEXT("Editor"),            EDreamPassViewFlags::Editor },
			{ TEXT("SceneCapture"),      EDreamPassViewFlags::SceneCapture },
			{ TEXT("PlanarReflection"),  EDreamPassViewFlags::PlanarReflection },
			{ TEXT("ReflectionCapture"), EDreamPassViewFlags::ReflectionCapture },
			{ TEXT("Thumbnail"),         EDreamPassViewFlags::Thumbnail },
		};

		const TSpelling<EDreamPassRequirementFlags> RequirementSpellings[] =
		{
			{ TEXT("PostProcess"),   EDreamPassRequirementFlags::PostProcess },
			{ TEXT("SceneResolve"),  EDreamPassRequirementFlags::SceneResolve },
			{ TEXT("CustomStencil"), EDreamPassRequirementFlags::CustomStencil },
		};

		const TSpelling<EDreamPassParameterType> ParameterTypeSpellings[] =
		{
			{ TEXT("float"),     EDreamPassParameterType::Float },
			{ TEXT("float2"),    EDreamPassParameterType::Float2 },
			{ TEXT("float3"),    EDreamPassParameterType::Float3 },
			{ TEXT("float4"),    EDreamPassParameterType::Float4 },
			{ TEXT("int"),       EDreamPassParameterType::Int },
			{ TEXT("bool"),      EDreamPassParameterType::Bool },
			{ TEXT("Texture2D"), EDreamPassParameterType::Texture },
		};

		const TSpelling<EDreamPassBufferResolution> ResolutionSpellings[] =
		{
			{ TEXT("Render"), EDreamPassBufferResolution::Render },
			{ TEXT("Output"), EDreamPassBufferResolution::Output },
			{ TEXT("Fixed"),  EDreamPassBufferResolution::Fixed },
		};

		const TSpelling<EDreamPassKind> KindSpellings[] =
		{
			{ TEXT("fullscreen"), EDreamPassKind::Fullscreen },
			{ TEXT("compute"),    EDreamPassKind::Compute },
			{ TEXT("mesh"),       EDreamPassKind::Mesh },
			{ TEXT("clear"),      EDreamPassKind::Clear },
			{ TEXT("copy"),       EDreamPassKind::Copy },
		};

		const TSpelling<EDreamPassParamSource> ParamSourceSpellings[] =
		{
			{ TEXT("Parameter"), EDreamPassParamSource::Parameter },
			{ TEXT("Constant"),  EDreamPassParamSource::Constant },
			{ TEXT("Weight"),    EDreamPassParamSource::Weight },
		};

		const TSpelling<EDreamPassDispatchMode> DispatchSpellings[] =
		{
			{ TEXT("Buffer"), EDreamPassDispatchMode::Buffer },
			{ TEXT("Fixed"),  EDreamPassDispatchMode::Fixed },
		};

		const TSpelling<EDreamPassFilterKind> FilterSpellings[] =
		{
			{ TEXT("Stencil"), EDreamPassFilterKind::Stencil },
			{ TEXT("Layer"),   EDreamPassFilterKind::Layer },
			{ TEXT("List"),    EDreamPassFilterKind::List },
		};

		const TSpelling<EDreamPassMeshMode> MeshModeSpellings[] =
		{
			{ TEXT("Override"),      EDreamPassMeshMode::Override },
			{ TEXT("Own"),           EDreamPassMeshMode::Own },
			{ TEXT("OwnOrOverride"), EDreamPassMeshMode::OwnOrOverride },
		};

		const TSpelling<EDreamPassDepthMode> DepthSpellings[] =
		{
			{ TEXT("TestScene"), EDreamPassDepthMode::TestScene },
			{ TEXT("None"),      EDreamPassDepthMode::None },
			{ TEXT("Own"),       EDreamPassDepthMode::Own },
		};

		const TSpelling<EDreamPassCullMode> CullSpellings[] =
		{
			{ TEXT("Auto"),  EDreamPassCullMode::Auto },
			{ TEXT("Back"),  EDreamPassCullMode::Back },
			{ TEXT("Front"), EDreamPassCullMode::Front },
			{ TEXT("None"),  EDreamPassCullMode::None },
		};

		const TSpelling<EDreamPassBlendMode> BlendSpellings[] =
		{
			{ TEXT("Replace"),    EDreamPassBlendMode::Replace },
			{ TEXT("Add"),        EDreamPassBlendMode::Add },
			{ TEXT("Max"),        EDreamPassBlendMode::Max },
			{ TEXT("Min"),        EDreamPassBlendMode::Min },
			{ TEXT("AlphaBlend"), EDreamPassBlendMode::AlphaBlend },
		};

		const TSpelling<EDreamPassMeshUsageFlags> UsageSpellings[] =
		{
			{ TEXT("StaticMesh"),            EDreamPassMeshUsageFlags::StaticMesh },
			{ TEXT("InstancedStaticMeshes"), EDreamPassMeshUsageFlags::InstancedStaticMeshes },
			{ TEXT("SkeletalMesh"),          EDreamPassMeshUsageFlags::SkeletalMesh },
			{ TEXT("Landscape"),             EDreamPassMeshUsageFlags::Landscape },
			{ TEXT("SplineMesh"),            EDreamPassMeshUsageFlags::SplineMesh },
			{ TEXT("GeometryCache"),         EDreamPassMeshUsageFlags::GeometryCache },
		};

		const TSpelling<EDreamPassNanitePolicy> NaniteSpellings[] =
		{
			{ TEXT("Skip"),          EDreamPassNanitePolicy::Skip },
			{ TEXT("StencilMask"),   EDreamPassNanitePolicy::StencilMask },
			{ TEXT("AssignStencil"), EDreamPassNanitePolicy::AssignStencil },
		};

		/** A payload number as a float4: Float4 with its N components, a scalar in x. */
		FVector4f ToVector4f(const IR::FIRPropertyValue& Value)
		{
			switch (Value.Kind)
			{
			case IR::EIRPropertyKind::Float4:
				return FVector4f(
					Value.N > 0 ? static_cast<float>(Value.V[0]) : 0.0f,
					Value.N > 1 ? static_cast<float>(Value.V[1]) : 0.0f,
					Value.N > 2 ? static_cast<float>(Value.V[2]) : 0.0f,
					Value.N > 3 ? static_cast<float>(Value.V[3]) : 0.0f);
			case IR::EIRPropertyKind::Float:
				return FVector4f(static_cast<float>(Value.F), 0.0f, 0.0f, 0.0f);
			case IR::EIRPropertyKind::Int:
				return FVector4f(static_cast<float>(Value.I), 0.0f, 0.0f, 0.0f);
			case IR::EIRPropertyKind::Bool:
				return FVector4f(Value.B ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
			default:
				return FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}

		FLinearColor ToLinearColor(const double (&Value)[4])
		{
			return FLinearColor(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2]), static_cast<float>(Value[3]));
		}

		/** A parameter default or a param constant, as the runtime's value of Type. False with OutError when a texture does not load. */
		bool MakeParameterValue(const EDreamPassParameterType Type, const IR::FIRPropertyValue& Value, FDreamPassParameterValue& OutValue, FString& OutError)
		{
			switch (Type)
			{
			case EDreamPassParameterType::Float:
				OutValue = FDreamPassParameterValue::MakeFloat(ToVector4f(Value).X);
				return true;

			case EDreamPassParameterType::Float2:
			case EDreamPassParameterType::Float3:
			case EDreamPassParameterType::Float4:
				OutValue = FDreamPassParameterValue::MakeVector(Type, ToVector4f(Value));
				return true;

			case EDreamPassParameterType::Int:
				OutValue = FDreamPassParameterValue::MakeInt(Value.Kind == IR::EIRPropertyKind::Int
					? static_cast<int32>(Value.I)
					: FMath::RoundToInt(ToVector4f(Value).X));
				return true;

			case EDreamPassParameterType::Bool:
				OutValue = FDreamPassParameterValue::MakeBool(Value.Kind == IR::EIRPropertyKind::Bool
					? Value.B
					: ToVector4f(Value).X != 0.0f);
				return true;

			case EDreamPassParameterType::Texture:
			{
				const FString Reference = Value.S.TrimStartAndEnd();
				if (Reference.IsEmpty())
				{
					OutValue = FDreamPassParameterValue::MakeTexture(nullptr);
					return true;
				}
				FString ObjectPath;
				FDreamShaderError ReferenceError;
				if (!Private::TryResolveDreamShaderAssetReference(Reference, ObjectPath, ReferenceError, UTexture::StaticClass()))
				{
					OutError = ReferenceError.Message;
					return false;
				}
				UTexture* Texture = LoadObject<UTexture>(nullptr, *ObjectPath);
				if (!Texture)
				{
					OutError = FString::Printf(TEXT("no texture loads from '%s'"), *ObjectPath); /* I18N-EXEMPT: wrapped by DSH8306 */
					return false;
				}
				OutValue = FDreamPassParameterValue::MakeTexture(Texture);
				return true;
			}
			}
			return false;
		}

		bool MapExportFormat(const EDreamPassBufferFormat Format, ETextureRenderTargetFormat& OutFormat)
		{
			switch (Format)
			{
			case EDreamPassBufferFormat::R8:      OutFormat = RTF_R8;      return true;
			case EDreamPassBufferFormat::RG8:     OutFormat = RTF_RG8;     return true;
			case EDreamPassBufferFormat::RGBA8:   OutFormat = RTF_RGBA8;   return true;
			case EDreamPassBufferFormat::R16F:    OutFormat = RTF_R16f;    return true;
			case EDreamPassBufferFormat::RG16F:   OutFormat = RTF_RG16f;   return true;
			case EDreamPassBufferFormat::RGBA16F: OutFormat = RTF_RGBA16f; return true;
			case EDreamPassBufferFormat::R32F:    OutFormat = RTF_R32f;    return true;
			case EDreamPassBufferFormat::RG32F:   OutFormat = RTF_RG32f;   return true;
			case EDreamPassBufferFormat::RGBA32F: OutFormat = RTF_RGBA32f; return true;
			// A material samples a float Texture2D through a filtering sampler: neither an integer nor a depth buffer can be one.
			case EDreamPassBufferFormat::R32U:
			case EDreamPassBufferFormat::RG32U:
			case EDreamPassBufferFormat::Depth32:
				return false;
			}
			return false;
		}

		/** `DSH8101: ...` -- an asset-layer error kept whole inside the 2.0 message. */
		FText WrapAssetError(const FDreamShaderError& Error)
		{
			if (Error.HasCode())
			{
				return FText::FromString(FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message)); /* I18N-EXEMPT: quotes an asset-layer message verbatim */
			}
			return FText::FromString(Error.Message);
		}

		UObject* FindOrLoadObject(const FString& PackageName, const FString& ObjectPath)
		{
			UObject* Existing = FindObject<UObject>(nullptr, *ObjectPath);
			if (!Existing && FPackageName::DoesPackageExist(PackageName))
			{
				Existing = LoadObject<UObject>(nullptr, *ObjectPath);
			}
			return Existing;
		}

		/** The pipeline asset, created or reused behind the ownership guard every product has. */
		bool CreateOrReuseDreamPassPipelineAsset(
			const FString& PackageName,
			const FString& ObjectPath,
			const FString& AssetName,
			UDreamPassPipeline*& OutPipeline,
			bool& bOutCreated,
			FDreamShaderError& OutError)
		{
			OutPipeline = nullptr;
			bOutCreated = false;

			if (UObject* Existing = FindOrLoadObject(PackageName, ObjectPath))
			{
				OutPipeline = Cast<UDreamPassPipeline>(Existing);
				if (!OutPipeline)
				{
					return FailWith(OutError, TEXT("DSH8302"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Asset '%s' already exists as a '%s'; a .dsp builds a DreamPassPipeline. Rename the .dsp or move the existing asset."),
						*ObjectPath,
						*Existing->GetClass()->GetName()));
				}
				if (FPackageName::DoesPackageExist(PackageName) && !Private::HasDreamShaderSourceMetadata(Existing))
				{
					return FailWith(OutError, TEXT("DSH8303"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Asset '%s' already exists and was not generated by DreamShader. Rename the .dsp or move/delete the existing asset before building it."),
						*ObjectPath));
				}
				return true;
			}

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				return FailWith(OutError, TEXT("DSH8301"), FString::Printf(TEXT("Failed to create package '%s'."), *PackageName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			OutPipeline = NewObject<UDreamPassPipeline>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
			if (!OutPipeline)
			{
				return FailWith(OutError, TEXT("DSH8301"), FString::Printf(TEXT("Failed to create the pass pipeline '%s'."), *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			bOutCreated = true;
			return true;
		}

		/**
		 * The render target of one exported buffer, created or reused IN PLACE: a material that reads the buffer lists this
		 * object among its textures, and replacing it would leave every such material out of date until it recompiled.
		 */
		bool CreateOrReuseExportTarget(
			const FString& PackageName,
			const FString& ObjectPath,
			const FString& AssetName,
			UTextureRenderTarget2D*& OutTarget,
			bool& bOutCreated,
			FDreamShaderError& OutError)
		{
			OutTarget = nullptr;
			bOutCreated = false;

			if (UObject* Existing = FindOrLoadObject(PackageName, ObjectPath))
			{
				OutTarget = Cast<UTextureRenderTarget2D>(Existing);
				if (!OutTarget)
				{
					return FailWith(OutError, TEXT("DSH8313"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Asset '%s' already exists as a '%s'; an exported buffer needs a TextureRenderTarget2D there. Move the existing asset or rename the buffer."),
						*ObjectPath,
						*Existing->GetClass()->GetName()));
				}
				if (FPackageName::DoesPackageExist(PackageName) && !Private::HasDreamShaderSourceMetadata(Existing))
				{
					return FailWith(OutError, TEXT("DSH8313"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Render target '%s' already exists and was not made by DreamShader, so it was not taken over for the exported buffer. Move it, or rename the buffer."),
						*ObjectPath));
				}
				return true;
			}

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				return FailWith(OutError, TEXT("DSH8314"), FString::Printf(TEXT("Failed to create package '%s'."), *PackageName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			OutTarget = NewObject<UTextureRenderTarget2D>(Package, FName(*AssetName), RF_Public | RF_Standalone);
			if (!OutTarget)
			{
				return FailWith(OutError, TEXT("DSH8314"), FString::Printf(TEXT("Failed to create the render target '%s'."), *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			bOutCreated = true;
			return true;
		}

		/**
		 * The settings a buffer gives its render target. Sized from the view at run time (the runtime resizes it), so a new
		 * one starts at 64x64 and an existing one keeps the size it has, except a Fixed buffer's. The addressing is the one
		 * every reader samples with -- UE.DreamPassBuffer defaults to the texture's own sampler: Clamp for a buffer the size
		 * of the view, Wrap for a fixed one that tiles. True when anything changed.
		 */
		bool NeedsExportTargetConfiguration(const UTextureRenderTarget2D& Target, const FDreamPassBufferDesc& Buffer,
			const ETextureRenderTargetFormat Format, const bool bCreated, FIntPoint& OutSize)
		{
			const TextureAddress Address = Buffer.Resolution == EDreamPassBufferResolution::Fixed ? TA_Wrap : TA_Clamp;
			const FLinearColor ClearColor = Buffer.bClear ? Buffer.ClearValue : FLinearColor::Transparent;

			OutSize = FIntPoint(Target.SizeX, Target.SizeY);
			if (Buffer.Resolution == EDreamPassBufferResolution::Fixed)
			{
				OutSize = FIntPoint(FMath::Max(Buffer.FixedSize.X, 1), FMath::Max(Buffer.FixedSize.Y, 1));
			}
			else if (bCreated || OutSize.X <= 0 || OutSize.Y <= 0)
			{
				OutSize = FIntPoint(64, 64);
			}

			return bCreated
				|| Target.RenderTargetFormat != Format
				|| Target.AddressX != Address
				|| Target.AddressY != Address
				|| !Target.ClearColor.Equals(ClearColor)
				|| !Target.bForceLinearGamma
				|| Target.bAutoGenerateMips
				|| Target.SizeX != OutSize.X
				|| Target.SizeY != OutSize.Y;
		}

		bool ConfigureExportTarget(UTextureRenderTarget2D& Target, const FDreamPassBufferDesc& Buffer, const ETextureRenderTargetFormat Format, const bool bCreated)
		{
			FIntPoint Size;
			if (!NeedsExportTargetConfiguration(Target, Buffer, Format, bCreated, Size))
			{
				return false;
			}

			Target.Modify();
			Target.RenderTargetFormat = Format;
			const TextureAddress Address = Buffer.Resolution == EDreamPassBufferResolution::Fixed ? TA_Wrap : TA_Clamp;
			Target.AddressX = Address;
			Target.AddressY = Address;
			Target.ClearColor = Buffer.bClear ? Buffer.ClearValue : FLinearColor::Transparent;
			Target.bForceLinearGamma = true;
			Target.bAutoGenerateMips = false;
			Target.SizeX = Size.X;
			Target.SizeY = Size.Y;
			if (FApp::CanEverRender())
			{
				Target.UpdateResourceImmediate(/*bClearRenderTarget*/ true);
			}
			return true;
		}

		/** Every exported buffer has its render target: a hash skip is only safe over a pipeline that is whole. */
		bool AreExportTargetsCurrent(const UDreamPassPipeline& Pipeline)
		{
			for (const FDreamPassBufferDesc& Buffer : Pipeline.Buffers)
			{
				if (!Buffer.bExport)
				{
					continue;
				}
				UTextureRenderTarget2D* Target = Pipeline.GetExportTarget(Buffer.Name);
				ETextureRenderTargetFormat Format = RTF_RGBA16f;
				FIntPoint Size;
				// A partial batch save may leave a current pipeline beside an older export, even after restarting.
				// Compare the saved configuration too; a non-null pointer alone does not make the pair current.
				if (!Target || !MapExportFormat(Buffer.Format, Format)
					|| !Private::IsGeneratedAssetPersisted(Target) || Target->GetOutermost()->IsDirty()
					|| NeedsExportTargetConfiguration(*Target, Buffer, Format, /*bCreated*/ false, Size))
				{
					return false;
				}
			}
			for (const TPair<FName, TObjectPtr<UTextureRenderTarget2D>>& Pair : Pipeline.ExportTargets)
			{
				const FDreamPassBufferDesc* Buffer = Pipeline.FindBuffer(Pair.Key);
				if (!Buffer || !Buffer->bExport)
				{
					return false;
				}
			}
			return true;
		}

		/** The data a `.dsp` owns, from the staged copy onto the asset. ExportTargets and the source fields are the caller's. */
		void CopyPipelineData(const UDreamPassPipeline& From, UDreamPassPipeline& To)
		{
			To.Order = From.Order;
			To.DefaultInjection = From.DefaultInjection;
			To.Views = From.Views;
			To.Requires = From.Requires;
			To.EnabledParameter = From.EnabledParameter;
			To.Parameters = From.Parameters;
			To.Buffers = From.Buffers;
			To.Passes = From.Passes;
#if WITH_EDITORONLY_DATA
			To.bHasSharedHlsl = From.bHasSharedHlsl;
			To.SharedHlsl = From.SharedHlsl;
			To.SharedHlslLine = From.SharedHlslLine;
#endif
		}

		/** IR::PassHlslSource as the asset spells it; File for every pass whose code is no inline HLSL. */
		EDreamPassHlslSource ToAssetHlslSource(const FString& Source)
		{
			if (Source.Equals(IR::PassHlslSource::Block, ESearchCase::CaseSensitive))
			{
				return EDreamPassHlslSource::Block;
			}
			if (Source.Equals(IR::PassHlslSource::Body, ESearchCase::CaseSensitive))
			{
				return EDreamPassHlslSource::Body;
			}
			if (Source.Equals(IR::PassHlslSource::Shared, ESearchCase::CaseSensitive))
			{
				return EDreamPassHlslSource::Shared;
			}
			return EDreamPassHlslSource::File;
		}

		/**
		 * The render targets of buffers the pipeline no longer exports, deleted once the pipeline that referenced them is saved
		 * without them -- unless something else still reads them (a material with UE.DreamPassBuffer on the old buffer), which
		 * keeps the asset and says so.
		 */
		void DeleteStaleExportTargets(
			const TArray<UTextureRenderTarget2D*>& Stale,
			const FString& PipelinePackageName,
			const Lang::FLangSpan& Span,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			if (Stale.IsEmpty())
			{
				return;
			}

			IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
			const FName PipelinePackage(*PipelinePackageName);
			for (UTextureRenderTarget2D* Target : Stale)
			{
				if (!Target || !Private::HasDreamShaderSourceMetadata(Target))
				{
					continue;
				}

				TArray<FName> Referencers;
				Registry.GetReferencers(Target->GetOutermost()->GetFName(), Referencers);
				Referencers.Remove(PipelinePackage);
				Referencers.Remove(Target->GetOutermost()->GetFName());

				int32 Deleted = 0;
				if (Referencers.IsEmpty() && GEditor)
				{
					TArray<UObject*> ToDelete;
					ToDelete.Add(Target);
					const FString TargetPath = Target->GetPathName();
					Deleted = ObjectTools::DeleteObjects(ToDelete, /*bShowConfirmation*/ false, ObjectTools::EAllowCancelDuringDelete::CancelNotAllowed);
					if (Deleted > 0)
					{
						Diagnostics.Info(TEXT("DSH8312"), Span, FText::Format(
							LOCTEXT("ExportTargetDeleted", "'{0}' was deleted: its buffer is no longer exported."),
							FText::FromString(TargetPath)));
						continue;
					}
				}

				TArray<FString> Names;
				for (const FName Referencer : Referencers)
				{
					Names.Add(Referencer.ToString());
				}
				Diagnostics.Warning(TEXT("DSH8310"), Span, FText::Format(
					LOCTEXT("ExportTargetKept", "'{0}' was left in place although its buffer is no longer exported: {1}. Delete it by hand once nothing reads it."),
					FText::FromString(Target->GetPathName()),
					Names.IsEmpty()
						? LOCTEXT("ExportTargetKeptNoDelete", "it could not be deleted here")
						: FText::Format(LOCTEXT("ExportTargetKeptReferenced", "it is still referenced by {0}"), FText::FromString(FString::Join(Names, TEXT(", "))))));
			}
		}
	}

	// ------------------------------------------------------------------------------------------- payload -> asset

	bool BuildDreamPassPipelineFromPayload(const IR::FIRProduct& Product, UDreamPassPipeline& Target, Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderPassPipelineEmitDetail;

		const IR::FIRPassPipeline& Payload = Product.PassPipeline;

		bool bOk = true;
		// A spelling the binder let through but this emitter does not know is a binder gap, reported as one.
		auto Unmapped = [&bOk, &Diagnostics](const IR::FIRSourceRef& Source, const FText& What, const FString& Value)
		{
			bOk = Diagnostics.Error(TEXT("DSH8304"), Source.Span, FText::Format(
				LOCTEXT("PayloadUnmapped", "'{1}' is not a {0} the Custom Pass runtime knows; the pipeline was not built. This is a compiler gap: the binder should have refused it."),
				What,
				FText::FromString(Value)));
		};
		const IR::FIRSourceRef& PipelineSource = Product.Source;

		Target.Order = Payload.Order;
		if (!::UE::DreamPass::LexTryParse(Payload.DefaultInjection, Target.DefaultInjection))
		{
			Unmapped(PipelineSource, LOCTEXT("WhatInjection", "injection point"), Payload.DefaultInjection);
		}

		EDreamPassViewFlags Views = EDreamPassViewFlags::None;
		for (const FString& View : Payload.Views)
		{
			EDreamPassViewFlags Flag = EDreamPassViewFlags::None;
			if (!TryParseSpelling(View, ViewSpellings, Flag))
			{
				Unmapped(PipelineSource, LOCTEXT("WhatView", "view kind"), View);
				continue;
			}
			Views |= Flag;
		}
		Target.Views = Payload.Views.IsEmpty() ? int32(EDreamPassViewFlags::Game | EDreamPassViewFlags::Editor) : int32(Views);

		EDreamPassRequirementFlags Requires = EDreamPassRequirementFlags::None;
		for (const FString& Requirement : Payload.Requires)
		{
			EDreamPassRequirementFlags Flag = EDreamPassRequirementFlags::None;
			if (!TryParseSpelling(Requirement, RequirementSpellings, Flag))
			{
				Unmapped(PipelineSource, LOCTEXT("WhatRequirement", "requirement"), Requirement);
				continue;
			}
			Requires |= Flag;
		}
		Target.Requires = int32(Requires);
		Target.EnabledParameter = Payload.EnabledParameter.IsEmpty() ? NAME_None : FName(*Payload.EnabledParameter);
#if WITH_EDITORONLY_DATA
		// The text, for the way back (decompile, Adopt): what runs is each slot's snapshot.
		Target.bHasSharedHlsl = Payload.bHasSharedHlsl;
		Target.SharedHlsl = Payload.SharedHlsl;
		Target.SharedHlslLine = Payload.SharedHlslLine;
#endif

		// ------------------------------------------------------------------------------------------------ parameters
		Target.Parameters.Reset();
		for (const IR::FIRPassParameter& Parameter : Payload.Parameters)
		{
			FDreamPassParameterDesc& Desc = Target.Parameters.AddDefaulted_GetRef();
			Desc.Name = FName(*Parameter.Name);
			EDreamPassParameterType Type = EDreamPassParameterType::Float;
			if (!TryParseSpelling(Parameter.Type, ParameterTypeSpellings, Type))
			{
				Unmapped(Parameter.Source, LOCTEXT("WhatParameterType", "parameter type"), Parameter.Type);
				continue;
			}
			FString ValueError;
			if (!MakeParameterValue(Type, Parameter.Default, Desc.Default, ValueError))
			{
				bOk = Diagnostics.Error(TEXT("DSH8306"), Parameter.Source.Span, FText::Format(
					LOCTEXT("ParameterDefaultFailed", "The default of '{0}' could not be applied: {1}."),
					FText::FromString(Parameter.Name),
					FText::FromString(ValueError)));
				continue;
			}
			Desc.Group = Parameter.Group;
			Desc.Description = Parameter.Description;
			Desc.bHasSlider = Parameter.bHasSlider;
			Desc.SliderMin = static_cast<float>(Parameter.SliderMin);
			Desc.SliderMax = static_cast<float>(Parameter.SliderMax);
			Desc.SortPriority = Parameter.SortPriority;
		}

		// --------------------------------------------------------------------------------------------------- buffers
		Target.Buffers.Reset();
		for (const IR::FIRPassBuffer& Buffer : Payload.Buffers)
		{
			FDreamPassBufferDesc& Desc = Target.Buffers.AddDefaulted_GetRef();
			Desc.Name = FName(*Buffer.Name);
			if (!::UE::DreamPass::LexTryParse(Buffer.Format, Desc.Format))
			{
				Unmapped(Buffer.Source, LOCTEXT("WhatFormat", "buffer format"), Buffer.Format);
			}
			if (!TryParseSpelling(Buffer.Resolution, ResolutionSpellings, Desc.Resolution))
			{
				Unmapped(Buffer.Source, LOCTEXT("WhatResolution", "buffer resolution"), Buffer.Resolution);
			}
			Desc.Scale = static_cast<float>(Buffer.Scale);
			// The payload's 0 x 0 means "no Size written", not a size: a buffer sized from the view keeps the struct's own
			// default, which the details panel shows when somebody switches it to Fixed.
			if (Desc.Resolution == EDreamPassBufferResolution::Fixed)
			{
				Desc.FixedSize = FIntPoint(Buffer.FixedWidth, Buffer.FixedHeight);
			}
			Desc.bClear = Buffer.bClear;
			Desc.ClearValue = ToLinearColor(Buffer.ClearValue);
			Desc.Mips = FMath::Max(1, Buffer.Mips);
			Desc.bHistory = Buffer.bHistory;
			Desc.bExport = Buffer.bExport;
			Desc.Description = Buffer.Description;

			ETextureRenderTargetFormat ExportFormat = RTF_RGBA16f;
			if (Desc.bExport && !MapExportFormat(Desc.Format, ExportFormat))
			{
				bOk = Diagnostics.Error(TEXT("DSH8308"), Buffer.Source.Span, FText::Format(
					LOCTEXT("ExportFormatRefused", "Buffer '{0}' is {1} and cannot be exported: materials, Blueprints and UMG read an exported buffer as a float texture, which an integer or a depth buffer cannot be. Export a float or normalized buffer, or drop Export."),
					FText::FromString(Buffer.Name),
					FText::FromString(Buffer.Format)));
			}
		}

		// ---------------------------------------------------------------------------------------------------- passes
		const UDreamPassSettings& Settings = UDreamPassSettings::Get();
		Target.Passes.Reset();
		for (const IR::FIRPass& Pass : Payload.Passes)
		{
			FDreamPassDesc& Desc = Target.Passes.AddDefaulted_GetRef();
			Desc.Name = FName(*Pass.Name);
			Desc.Description = Pass.Description;
			if (!TryParseSpelling(Pass.Kind, KindSpellings, Desc.Kind))
			{
				Unmapped(Pass.Source, LOCTEXT("WhatKind", "pass kind"), Pass.Kind);
				continue;
			}
			const FString& Injection = Pass.Injection.IsEmpty() ? Payload.DefaultInjection : Pass.Injection;
			if (!::UE::DreamPass::LexTryParse(Injection, Desc.Injection))
			{
				Unmapped(Pass.Source, LOCTEXT("WhatPassInjection", "injection point"), Injection);
			}
			Desc.EnabledParameter = Pass.EnabledParameter.IsEmpty() ? NAME_None : FName(*Pass.EnabledParameter);

			const bool bMaterialPass = Desc.Kind == EDreamPassKind::Fullscreen && !Pass.MaterialObjectPath.IsEmpty();
			for (const IR::FIRPassBinding& Read : Pass.Reads)
			{
				FDreamPassBufferBinding& Binding = Desc.Reads.AddDefaulted_GetRef();
				Binding.Slot = Read.Slot.IsEmpty() ? NAME_None : FName(*Read.Slot);
				Binding.Buffer = FName(*Read.Buffer);
				Binding.bPrevious = Read.bPrevious;
			}
			for (const IR::FIRPassBinding& Write : Pass.Writes)
			{
				FDreamPassBufferBinding& Binding = Desc.Writes.AddDefaulted_GetRef();
				// A material pass's one write has no name of its own: the material writes its output, whatever it is called.
				Binding.Slot = (bMaterialPass || Write.Slot.IsEmpty()) ? NAME_None : FName(*Write.Slot);
				Binding.Buffer = FName(*Write.Buffer);
				Binding.bPrevious = Write.bPrevious;
			}
			for (const IR::FIRPassParam& Param : Pass.Params)
			{
				FDreamPassParamBinding& Binding = Desc.Params.AddDefaulted_GetRef();
				Binding.Target = FName(*Param.Target);
				if (!TryParseSpelling(Param.SourceKind, ParamSourceSpellings, Binding.Source))
				{
					Unmapped(Param.Source, LOCTEXT("WhatParamSource", "param source"), Param.SourceKind);
					continue;
				}
				Binding.Parameter = Param.Parameter.IsEmpty() ? NAME_None : FName(*Param.Parameter);
				Binding.Multiplier = static_cast<float>(Param.Multiplier);
				Binding.Offset = static_cast<float>(Param.Offset);
				if (Binding.Source == EDreamPassParamSource::Constant)
				{
					EDreamPassParameterType ConstantType = EDreamPassParameterType::Float;
					if (!TryParseSpelling(Param.ConstantType, ParameterTypeSpellings, ConstantType) || ConstantType == EDreamPassParameterType::Texture)
					{
						Unmapped(Param.Source, LOCTEXT("WhatConstantType", "param constant type"), Param.ConstantType);
						continue;
					}
					FString ValueError;
					MakeParameterValue(ConstantType, Param.Constant, Binding.Constant, ValueError);
				}
			}

			auto LoadMaterial = [&Diagnostics, &bOk, &Pass](const FString& ObjectPath) -> UMaterialInterface*
			{
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
				if (!Material)
				{
					bOk = Diagnostics.Error(TEXT("DSH8305"), Pass.Source.Span, FText::Format(
						LOCTEXT("PassMaterialMissing", "The material '{0}' of pass '{1}' does not load; compile the source that builds it, or correct the Material key."),
						FText::FromString(Pass.MaterialReference.IsEmpty() ? ObjectPath : Pass.MaterialReference),
						FText::FromString(Pass.Name)));
				}
				return Material;
			};

			switch (Desc.Kind)
			{
			case EDreamPassKind::Fullscreen:
				if (bMaterialPass)
				{
					Desc.Fullscreen.Material = LoadMaterial(Pass.MaterialObjectPath);
				}
				else
				{
					// An HLSL fullscreen pass runs in an FDreamPassPS slot, whatever its output count: no wrapper material. The
					// path is kept as the `.dsp` writes it -- relative to its folder, or virtual -- which is what the decompiler
					// writes back; the slot compiles the snapshot, never this file. Inline HLSL keeps its text instead.
					Desc.Fullscreen.ShaderPath = Pass.ShaderReference;
					Desc.Fullscreen.Entry = Pass.Entry;
					Desc.Fullscreen.PixelSlot = INDEX_NONE;
					Desc.Fullscreen.HlslSource = ToAssetHlslSource(Pass.HlslSource);
#if WITH_EDITORONLY_DATA
					Desc.Fullscreen.InlineHlsl = Pass.InlineHlsl;
					Desc.Fullscreen.InlineHlslLine = Pass.InlineHlslLine;
#endif
				}
				break;

			case EDreamPassKind::Compute:
				Desc.Compute.ShaderPath = Pass.ShaderReference;
				Desc.Compute.Entry = Pass.Entry;
				Desc.Compute.Slot = INDEX_NONE;
				Desc.Compute.HlslSource = ToAssetHlslSource(Pass.HlslSource);
#if WITH_EDITORONLY_DATA
				Desc.Compute.InlineHlsl = Pass.InlineHlsl;
				Desc.Compute.InlineHlslLine = Pass.InlineHlslLine;
#endif
				Desc.Compute.ThreadGroupSize = FIntVector(FMath::Max(Pass.ThreadsX, 1), FMath::Max(Pass.ThreadsY, 1), FMath::Max(Pass.ThreadsZ, 1));
				if (!TryParseSpelling(Pass.DispatchMode, DispatchSpellings, Desc.Compute.DispatchMode))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatDispatch", "dispatch mode"), Pass.DispatchMode);
				}
				Desc.Compute.DispatchBuffer = Pass.DispatchBuffer.IsEmpty() ? NAME_None : FName(*Pass.DispatchBuffer);
				Desc.Compute.DispatchScale = static_cast<float>(Pass.DispatchScale);
				Desc.Compute.DispatchSize = FIntVector(FMath::Max(Pass.DispatchX, 1), FMath::Max(Pass.DispatchY, 1), FMath::Max(Pass.DispatchZ, 1));
				break;

			case EDreamPassKind::Mesh:
			{
				for (const IR::FIRPassFilterClause& Clause : Pass.Filter)
				{
					FDreamPassFilterClause& OutClause = Desc.Mesh.Filter.AnyOf.AddDefaulted_GetRef();
					for (const IR::FIRPassFilterTerm& Term : Clause.AllOf)
					{
						FDreamPassFilterTerm& OutTerm = OutClause.AllOf.AddDefaulted_GetRef();
						if (!TryParseSpelling(Term.Kind, FilterSpellings, OutTerm.Kind))
						{
							Unmapped(Pass.Source, LOCTEXT("WhatFilter", "filter term"), Term.Kind);
							continue;
						}
						OutTerm.StencilValue = Term.StencilValue;
						OutTerm.StencilMask = Term.StencilMask;
						OutTerm.ListName = Term.List.IsEmpty() ? NAME_None : FName(*Term.List);
						for (const FString& Layer : Term.Layers)
						{
							OutTerm.LayerNames.Add(FName(*Layer));
						}
						if (OutTerm.Kind == EDreamPassFilterKind::Layer)
						{
							TArray<FName> Unknown;
							OutTerm.LayerMask = int32(Settings.MakeLayerMask(OutTerm.LayerNames, &Unknown));
							for (const FName Layer : Unknown)
							{
								bOk = Diagnostics.Error(TEXT("DSH8307"), Pass.Source.Span, FText::Format(
									LOCTEXT("LayerUnknown", "Pass '{0}' selects the layer '{1}', which is not one of the project's pass layers (Project Settings > DreamPlugin > DreamShader Custom Pass > Layer Names)."),
									FText::FromString(Pass.Name),
									FText::FromName(Layer)));
							}
						}
					}
				}

				if (!Pass.MaterialObjectPath.IsEmpty())
				{
					Desc.Mesh.OverrideMaterial = LoadMaterial(Pass.MaterialObjectPath);
				}
				if (Pass.MeshMode.IsEmpty())
				{
					// `Material` given: it overrides; left out: every object draws with its own.
					Desc.Mesh.Mode = Pass.MaterialObjectPath.IsEmpty() ? EDreamPassMeshMode::Own : EDreamPassMeshMode::Override;
				}
				else if (!TryParseSpelling(Pass.MeshMode, MeshModeSpellings, Desc.Mesh.Mode))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatMeshMode", "mesh mode"), Pass.MeshMode);
				}
				if (!TryParseSpelling(Pass.Depth, DepthSpellings, Desc.Mesh.Depth))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatDepth", "depth mode"), Pass.Depth);
				}
				Desc.Mesh.DepthBuffer = Pass.DepthBuffer.IsEmpty() ? NAME_None : FName(*Pass.DepthBuffer);
				if (!TryParseSpelling(Pass.Cull, CullSpellings, Desc.Mesh.Cull))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatCull", "cull mode"), Pass.Cull);
				}
				if (!TryParseSpelling(Pass.Blend, BlendSpellings, Desc.Mesh.Blend))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatBlend", "blend mode"), Pass.Blend);
				}
				if (!Pass.Usage.IsEmpty())
				{
					EDreamPassMeshUsageFlags Usage = EDreamPassMeshUsageFlags::None;
					for (const FString& Spelling : Pass.Usage)
					{
						EDreamPassMeshUsageFlags Flag = EDreamPassMeshUsageFlags::None;
						if (!TryParseSpelling(Spelling, UsageSpellings, Flag))
						{
							Unmapped(Pass.Source, LOCTEXT("WhatUsage", "mesh usage"), Spelling);
							continue;
						}
						Usage |= Flag;
					}
					Desc.Mesh.Usage = int32(Usage);
				}
				if (!TryParseSpelling(Pass.Nanite, NaniteSpellings, Desc.Mesh.Nanite))
				{
					Unmapped(Pass.Source, LOCTEXT("WhatNanite", "Nanite policy"), Pass.Nanite);
				}
				Desc.Mesh.AssignedStencilValue = Pass.AssignedStencilValue;
				Desc.Mesh.NaniteValue = ToLinearColor(Pass.NaniteValue);
				break;
			}

			case EDreamPassKind::Clear:
				Desc.Clear.Value = ToLinearColor(Pass.ClearValue);
				break;

			case EDreamPassKind::Copy:
				break;
			}
		}

		return bOk;
	}

	void MakeDreamPassSlotCandidates(const IR::FIRPassPipeline& Payload, const UDreamPassPipeline& Staged, TArray<FDreamPassSlotCandidate>& OutCandidates)
	{
		OutCandidates.Reset();
		for (int32 Index = 0; Index < Staged.Passes.Num() && Index < Payload.Passes.Num(); ++Index)
		{
			const FDreamPassDesc& Pass = Staged.Passes[Index];
			const IR::FIRPass& IRPass = Payload.Passes[Index];

			const bool bInline = IR::PassHlslSource::IsInline(IRPass.HlslSource);
			bool bCompute = false;
			if (Pass.Kind == EDreamPassKind::Compute)
			{
				bCompute = true;
			}
			else if (!(Pass.Kind == EDreamPassKind::Fullscreen && !Pass.Fullscreen.Material && (!IRPass.ShaderReference.IsEmpty() || bInline)))
			{
				continue;
			}

			FDreamPassSlotCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.bCompute = bCompute;
			Candidate.PassIndex = Index;
			Candidate.PassName = IRPass.Name;
			Candidate.ShaderReference = IRPass.ShaderReference;
			Candidate.ShaderVirtualPath = IRPass.ShaderVirtualPath;
			Candidate.ShaderFilePath = IRPass.ShaderFilePath;
			Candidate.Entry = IRPass.Entry;
			Candidate.Span = IRPass.Source.Span;
			if (bInline)
			{
				// The root the slot compiles, generated from the `.dsp`'s text; its file and folder are the planner's
				// (PlanDreamPassSlots). A root that cannot be built leaves the text empty, which the planner reports.
				Candidate.bInline = true;
				Candidate.bAtBeginView = IRPass.Injection.Equals(TEXT("BeginView"), ESearchCase::CaseSensitive);
				FString RootError;
				if (!Lang::BuildDreamPassInlineHlslRoot(Payload, Index, Candidate.Inline, RootError))
				{
					Candidate.Inline = Lang::FHlslInlineRoot();
				}
				// A body form's function is the slot's own entry point already; nothing to rename.
				if (Candidate.Entry.IsEmpty())
				{
					Candidate.Entry = Lang::GetDreamPassMainEntryName(bCompute);
				}
			}
		}
	}

	FString MakeDreamPassExportTargetPath(
		const FString& PipelinePackageName,
		const FString& PipelineAssetName,
		const FString& BufferName,
		FString* OutPackageName,
		FString* OutAssetName)
	{
		const FString AssetName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("%s_%s"), *PipelineAssetName, *BufferName)); /* I18N-EXEMPT: asset name, not display text */
		const FString PackageName = FPackageName::GetLongPackagePath(PipelinePackageName) / AssetName;
		if (OutPackageName)
		{
			*OutPackageName = PackageName;
		}
		if (OutAssetName)
		{
			*OutAssetName = AssetName;
		}
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName); /* I18N-EXEMPT: object path, not display text */
	}

	bool IsDreamPassExportableBufferFormat(const FString& FormatSpelling)
	{
		EDreamPassBufferFormat Format = EDreamPassBufferFormat::RGBA16F;
		ETextureRenderTargetFormat Unused = RTF_RGBA16f;
		return ::UE::DreamPass::LexTryParse(FormatSpelling, Format) && DreamShaderPassPipelineEmitDetail::MapExportFormat(Format, Unused);
	}

	// ---------------------------------------------------------------------------------------------------- the emit

	bool EmitPassPipelineProduct(
		const IR::FIRProduct& Product,
		const FTextShaderDefinition& Definition,
		const FIREmitContext& Context,
		UObject*& OutAsset,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderPassPipelineEmitDetail;

		OutAsset = nullptr;
		const Lang::FLangSpan ProductSpan = Product.Source.Span;

#if !DREAMSHADER_WITH_CUSTOM_PASS
		(void)Definition;
		(void)Context;
		return Diagnostics.Error(TEXT("DSH8300"), ProductSpan, FText::Format(
			LOCTEXT("NeedsCustomPass", "'{0}' is a Custom Pass pipeline, which needs Unreal Engine 5.8 or later; this engine has the DreamShaderPass asset types but no runtime to run them, so nothing was built."),
			FText::FromString(Product.Name)));
#else
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		FDreamShaderError DestinationError;
		if (!Private::ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, DestinationError))
		{
			return Diagnostics.Error(TEXT("DSH8301"), ProductSpan, FText::Format(
				LOCTEXT("PipelineDestinationFailed", "'{0}' does not resolve to a valid asset path. {1}"),
				FText::FromString(Product.Name),
				WrapAssetError(DestinationError)));
		}

		// ---- stage: the whole pipeline onto a transient copy, before anything touches the asset
		// Held strongly for the whole emit: deleting a stale render target goes through ObjectTools, which collects garbage,
		// and an unreferenced transient object does not survive that (the editor crashed reading it at the scope exit).
		TStrongObjectPtr<UDreamPassPipeline> StagedHolder(NewObject<UDreamPassPipeline>(GetTransientPackage(), NAME_None, RF_Transient));
		UDreamPassPipeline* Staged = StagedHolder.Get();
		ON_SCOPE_EXIT
		{
			if (UDreamPassPipeline* StagedObject = StagedHolder.Get())
			{
				StagedObject->MarkAsGarbage();
			}
			StagedHolder.Reset();
		};
		if (!BuildDreamPassPipelineFromPayload(Product, *Staged, Diagnostics))
		{
			return false;
		}

		// ---- the asset
		UDreamPassPipeline* Pipeline = nullptr;
		bool bCreatedPipeline = false;
		FDreamShaderError CreateError;
		if (!CreateOrReuseDreamPassPipelineAsset(PackageName, ObjectPath, AssetName, Pipeline, bCreatedPipeline, CreateError) || !Pipeline)
		{
			return Diagnostics.Error(TEXT("DSH8301"), ProductSpan, FText::Format(
				LOCTEXT("CreatePipelineFailed", "The pass pipeline for '{0}' could not be created or reused. {1}"),
				FText::FromString(Product.Name),
				WrapAssetError(CreateError)));
		}
		OutAsset = Pipeline;

		// ---- the gates. A hash skip needs a pipeline that is whole as well: its slots in the registry with their
		// snapshots on disk, and every exported buffer's render target. A registry somebody deleted, or a merge that lost
		// a slot, is rebuilt by the next compile instead of hidden behind an unchanged source hash.
		bool bSaveToDisk = true;
		const bool bWouldPersist = WouldDreamShaderIRBuildPersist(Pipeline, bSaveToDisk);
		const bool bAllowHashSkip = !bCreatedPipeline
			&& IsDreamPassRegistryCurrentFor(*Pipeline, ObjectPath)
			&& AreExportTargetsCurrent(*Pipeline);
		bool bSkip = false;
		if (!CheckDreamShaderIRRebuildPreconditions(Pipeline, Context, Product, bAllowHashSkip, bWouldPersist, Diagnostics, bSkip))
		{
			return false;
		}
		if (bSkip)
		{
			return true;
		}

		// ---- the slots: planned and pre-checked in memory. The registry and the snapshots are files on disk shared by the
		// whole project, so a build that may not write (another editor owns writing) takes the slots the registry records
		// and changes nothing.
		TArray<FDreamPassSlotCandidate> Candidates;
		MakeDreamPassSlotCandidates(Product.PassPipeline, *Staged, Candidates);

		FDreamPassSlotPlan Plan;
		if (bSaveToDisk)
		{
			if (!PlanDreamPassSlots(*Staged, ObjectPath, Context.SourceFilePath, MoveTemp(Candidates), Plan, Diagnostics))
			{
				return false;
			}
			if (!PrecheckDreamPassSlots(Plan, TArray<FName>(), /*bRecheckUnchanged*/ false, Diagnostics))
			{
				// The asset, the registry and the snapshots are as they were: the previous version keeps running.
				return false;
			}
		}
		else
		{
			FDreamPassRegistry Registry;
			FString LoadError;
			LoadDreamPassRegistry(Registry, LoadError);
			Plan.Candidates = MoveTemp(Candidates);
			for (FDreamPassSlotCandidate& Candidate : Plan.Candidates)
			{
				const FDreamPassRegistrySlot* Recorded = Registry.Find(Candidate.bCompute, ObjectPath, Candidate.PassName);
				Candidate.Slot = Recorded ? Recorded->Slot : INDEX_NONE;
			}
		}

		for (const FDreamPassSlotCandidate& Candidate : Plan.Candidates)
		{
			FDreamPassDesc& Pass = Staged->Passes[Candidate.PassIndex];
			if (Candidate.bCompute)
			{
				Pass.Compute.Slot = Candidate.Slot;
			}
			else
			{
				Pass.Fullscreen.PixelSlot = Candidate.Slot;
			}
		}

		// ---- the render targets: found or made now, so an occupied name fails before anything is written; configured after
		// the commit, in place
		struct FPendingTarget
		{
			UTextureRenderTarget2D* Target = nullptr;
			const FDreamPassBufferDesc* Buffer = nullptr;
			ETextureRenderTargetFormat Format = RTF_RGBA16f;
			bool bCreated = false;
		};
		TArray<FPendingTarget> PendingTargets;
		TMap<FName, TObjectPtr<UTextureRenderTarget2D>> ExportTargets;

		for (const FDreamPassBufferDesc& Buffer : Staged->Buffers)
		{
			if (!Buffer.bExport)
			{
				continue;
			}
			ETextureRenderTargetFormat Format = RTF_RGBA16f;
			if (!MapExportFormat(Buffer.Format, Format))
			{
				// Refused while staging (DSH8308).
				continue;
			}

			FString TargetPackage;
			FString TargetName;
			const FString TargetPath = MakeDreamPassExportTargetPath(PackageName, AssetName, Buffer.Name.ToString(), &TargetPackage, &TargetName);

			FPendingTarget& Pending = PendingTargets.AddDefaulted_GetRef();
			Pending.Buffer = &Buffer;
			Pending.Format = Format;
			FDreamShaderError TargetError;
			if (!CreateOrReuseExportTarget(TargetPackage, TargetPath, TargetName, Pending.Target, Pending.bCreated, TargetError) || !Pending.Target)
			{
				return Diagnostics.Error(TEXT("DSH8309"), ProductSpan, FText::Format(
					LOCTEXT("ExportTargetFailed", "The render target of the exported buffer '{0}' could not be created or reused. {1}"),
					FText::FromName(Buffer.Name),
					WrapAssetError(TargetError)));
			}
			ExportTargets.Add(Buffer.Name, Pending.Target);
		}

		TArray<UTextureRenderTarget2D*> StaleTargets;
		for (const TPair<FName, TObjectPtr<UTextureRenderTarget2D>>& Pair : Pipeline->ExportTargets)
		{
			UTextureRenderTarget2D* Old = Pair.Value.Get();
			if (Old && !ExportTargets.FindKey(Old))
			{
				StaleTargets.AddUnique(Old);
			}
		}

		// ---- last chance to stop: from here on files change
		if (Context.IsCancelled && Context.IsCancelled())
		{
			return Diagnostics.Error(TEXT("DSH8298"), ProductSpan, FText::Format(
				LOCTEXT("PipelineEmitCancelled", "Building '{0}' was cancelled; the pipeline, its slots and its render targets are as they were before this compile."),
				FText::FromString(Product.Name)));
		}

		const bool bSlotsChanged = bSaveToDisk && Plan.HasChanges();
		if (bSlotsChanged && !CommitDreamPassSlots(Plan, Diagnostics))
		{
			return false;
		}

		// ---- the render targets' settings, on the same objects every reader already references
		TArray<UObject*> PackagesToSave;
		PackagesToSave.Add(Pipeline);
		// Render targets whose stamps or settings this build writes: new ones, reconfigured ones, unstamped ones.
		TArray<UTextureRenderTarget2D*> TouchedTargets;
		for (const FPendingTarget& Pending : PendingTargets)
		{
			const bool bChanged = ConfigureExportTarget(*Pending.Target, *Pending.Buffer, Pending.Format, Pending.bCreated);
			if (bChanged || Pending.bCreated || !Private::HasDreamShaderSourceMetadata(Pending.Target)
				|| Pending.Target->GetOutermost()->IsDirty() || !Private::IsGeneratedAssetPersisted(Pending.Target))
			{
				TouchedTargets.AddUnique(Pending.Target);
				PackagesToSave.AddUnique(Pending.Target);
			}
		}

		// ---- the asset
		Pipeline->Modify();
		CopyPipelineData(*Staged, *Pipeline);
		Pipeline->ExportTargets = MoveTemp(ExportTargets);
		Pipeline->SourceFilePath = Private::MakeProjectRelativeSourcePath(Context.SourceFilePath);
		Pipeline->SourceHash = Context.SourceHash;
		// Re-validates, moves the revision on and tells the subsystem: the next frame runs the new pipeline.
		Pipeline->NotifyChanged();

		// The registry on disk is the new one from the commit on, whatever happens to the save below: the running editor
		// compiles it now, as the next one would at startup. Synchronous, so no frame sees the new slot numbers with the
		// old shaders.
		if (bSlotsChanged)
		{
			HotReloadDreamPassShaders(Plan.bComputeChanged, Plan.bPixelChanged);
		}

		// A memory-only build stamps the path alone, as every product's does: the hash is what a skip trusts, and only a
		// build that reached the disk -- the registry and the snapshots with it -- may be skipped on it.
		if (bSaveToDisk)
		{
			Private::ApplySourceMetadata(Pipeline, Context.SourceFilePath, Context.SourceHash);
		}
		else
		{
			Private::ApplySourceMetadata(Pipeline, Context.SourceFilePath);
		}
		Private::ApplyOutputDigestMetadata(Pipeline);
		for (UTextureRenderTarget2D* Target : TouchedTargets)
		{
			// Owned by the pipeline's source: the ownership guard, Detach and the Browser read the same stamp on it. The path
			// alone, no source hash: a hash would move with every edit of the `.dsp` and dirty every render target with it.
			Private::ApplySourceMetadata(Target, Context.SourceFilePath);
			SetDreamShaderAssetMetadata(Target, TEXT("DreamShader.PassPipeline"), ObjectPath);
		}

		if (!bSaveToDisk)
		{
			for (UObject* Asset : PackagesToSave)
			{
				if (UPackage* Package = Asset ? Asset->GetPackage() : nullptr)
				{
					Package->SetDirtyFlag(false);
				}
			}
			return true;
		}

		TArray<UObject*> NewOnDisk;
		for (UObject* Asset : PackagesToSave)
		{
			Asset->MarkPackageDirty();
			if (!FPackageName::DoesPackageExist(Asset->GetOutermost()->GetName()))
			{
				NewOnDisk.Add(Asset);
			}
		}
		FDreamShaderError SaveError;
		if (!Private::SaveAssetPackages(PackagesToSave, SaveError))
		{
			// The shared save helper invalidates the source-hash skip and keeps every package dirty for a plain retry.
			return Diagnostics.Error(TEXT("DSH8311"), ProductSpan, FText::Format(
				LOCTEXT("SavePipelineFailedRetry", "'{0}' was built but could not be saved with its render targets; its slots are already in the registry. Make the packages writable and compile the source again to retry saving. {1}"),
				FText::FromString(Pipeline->GetPathName()),
				WrapAssetError(SaveError)));
		}
		for (UObject* Saved : NewOnDisk)
		{
			PublishGeneratedIRAsset(Saved);
		}

		DeleteStaleExportTargets(StaleTargets, PackageName, ProductSpan, Diagnostics);
		return true;
#endif
	}
}

#undef LOCTEXT_NAMESPACE
