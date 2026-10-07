// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Decompiler/DreamShaderInstanceDecompiler.h"

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderInstanceSchema.h"
#include "DreamShaderInstanceSettings.h"
#include "DreamShaderModule.h"
#include "DreamShaderProductIndex.h"
#include "DreamShaderVersionCompat.h"
#include "Lang/LangInstanceSource.h"

#include "Engine/Font.h"
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
#include "Engine/TextureCollection.h"
#endif
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
// FMaterialParameterInfo and its kin: Materials/MaterialParameters.h from UE 5.7, MaterialTypes.h before.
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif
#include "Misc/Paths.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "DreamShader.Decompiler.Instance"

namespace UE::DreamShader::Editor::Private
{
	namespace InstanceDecompile
	{
		using UE::DreamShader::IR::EIRParameterKind;
		using UE::DreamShader::IR::FIRPropertyValue;

		/** The kinds a `.dsi` can state, in the order their overrides are read; the engine type of each is the compiler's answer. */
		static const EIRParameterKind AllKinds[] =
		{
			EIRParameterKind::Scalar,
			EIRParameterKind::Vector,
			EIRParameterKind::DoubleVector,
			EIRParameterKind::Texture,
			EIRParameterKind::TextureCollection,
			EIRParameterKind::Font,
			EIRParameterKind::RuntimeVirtualTexture,
			EIRParameterKind::SparseVolumeTexture,
			EIRParameterKind::StaticSwitch,
			EIRParameterKind::ParameterCollection,
			EIRParameterKind::StaticComponentMask,
		};

		static FString ObjectPathOf(const UObject* Object)
		{
			return Object ? Object->GetPathName() : FString();
		}

		/** A parameter's value in FIRInstanceOverride::Value's encoding (the twin of the schema's ParentValue). */
		static void FillOverrideValue(const FMaterialParameterValue& Value, UE::DreamShader::IR::FIRInstanceOverride& Override)
		{
			switch (Override.Kind)
			{
			case EIRParameterKind::Scalar:
			{
				const double Channels[4] = { Value.AsScalar(), 0.0, 0.0, 0.0 };
				Override.Value = FIRPropertyValue::MakeFloat4(Channels, 1);
				return;
			}
			case EIRParameterKind::Vector:
			{
				const FLinearColor Color = Value.AsLinearColor();
				const double Channels[4] = { Color.R, Color.G, Color.B, Color.A };
				Override.Value = FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case EIRParameterKind::DoubleVector:
			{
				const FVector4d Vector = Value.AsVector4d();
				const double Channels[4] = { Vector.X, Vector.Y, Vector.Z, Vector.W };
				Override.Value = FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case EIRParameterKind::StaticSwitch:
				Override.Value = FIRPropertyValue::MakeBool(Value.AsStaticSwitch());
				return;
			case EIRParameterKind::StaticComponentMask:
			{
				const FStaticComponentMaskValue Mask = Value.AsStaticComponentMask();
				const double Channels[4] = { Mask.R ? 1.0 : 0.0, Mask.G ? 1.0 : 0.0, Mask.B ? 1.0 : 0.0, Mask.A ? 1.0 : 0.0 };
				Override.Value = FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case EIRParameterKind::Texture:
			case EIRParameterKind::RuntimeVirtualTexture:
			case EIRParameterKind::SparseVolumeTexture:
				Override.Value = FIRPropertyValue::MakeObject(ObjectPathOf(Value.AsTextureObject()));
				return;
			case EIRParameterKind::TextureCollection:
			{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				// AsTextureObject does not include texture collections.
				const UObject* Collection = Value.TextureCollection;
				Override.Value = FIRPropertyValue::MakeObject(ObjectPathOf(Collection));
#else
				Override.Value = FIRPropertyValue::MakeObject(FString());
#endif
				return;
			}
			case EIRParameterKind::Font:
			{
				// Not AsTextureObject, which hands back the font's page texture rather than the font.
				const UObject* FontObject = Value.Font.Value;
				Override.Value = FIRPropertyValue::MakeObject(ObjectPathOf(FontObject));
				Override.FontPage = Value.Font.Page;
				return;
			}
			case EIRParameterKind::ParameterCollection:
			{
#if DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS
				const UObject* Collection = Value.ParameterCollection;
				Override.Value = FIRPropertyValue::MakeObject(ObjectPathOf(Collection));
#else
				Override.Value = FIRPropertyValue::MakeObject(FString());
#endif
				return;
			}
			}
		}

		/** `/Game/X/M_Glow` for the asset named after its package, the object path for anything else. */
		static FString MakeParentReference(const UMaterialInterface* Parent)
		{
			const FString ObjectPath = Parent->GetPathName();
			const FString PackageName = Parent->GetOutermost() ? Parent->GetOutermost()->GetName() : FString();
			return (!PackageName.IsEmpty() && FPaths::GetBaseFilename(PackageName).Equals(Parent->GetName(), ESearchCase::CaseSensitive))
				? PackageName
				: ObjectPath;
		}

		/** The parent's bare name, when the products under the target's own source root have exactly one of that name and it is this one. */
		static bool TryMakeBareParentName(const UMaterialInterface* Parent, const FString& TargetSourceFilePath, FString& OutName)
		{
			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(TargetSourceFilePath);
			if (!Root)
			{
				return false;
			}

			::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex& Index = ::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex::Get();
			Index.Refresh();
			TArray<const ::UE::DreamShader::Editor::Compiler::FDreamShaderProductRecord*> Records;
			Index.FindByName(Root->Directory, Parent->GetName(), Records);
			if (Records.Num() != 1 || !Records[0]->ObjectPath.Equals(Parent->GetPathName(), ESearchCase::IgnoreCase))
			{
				return false;
			}
			OutName = Parent->GetName();
			return true;
		}
	}

	bool DecompileMaterialInstance(
		UMaterialInstanceConstant* Instance,
		const FInstanceDecompileOptions& Options,
		UE::DreamShader::IR::FIRInstance& OutInstance,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		namespace IR = UE::DreamShader::IR;
		using UE::DreamShader::Lang::FLangSpan;

		OutInstance = IR::FIRInstance();
		if (!Instance || !Instance->Parent)
		{
			Diagnostics.Error(TEXT("DSH9100"), FLangSpan(), FText::Format(
				LOCTEXT("InstanceWithoutParent", "'{0}' has no parent material, and a '.dsi' is nothing but overrides of one."),
				FText::FromString(Instance ? Instance->GetPathName() : FString(TEXT("<null>")))));
			return false;
		}
		const int32 ErrorsBefore = Diagnostics.NumErrors();

		const UMaterialInterface* Parent = Instance->Parent;
		OutInstance.ParentObjectPath = Parent->GetPathName();
		OutInstance.ParentReference = InstanceDecompile::MakeParentReference(Parent);
		if (Options.bPreferBareParentName && !Options.TargetSourceFilePath.IsEmpty())
		{
			FString BareName;
			if (InstanceDecompile::TryMakeBareParentName(Parent, Options.TargetSourceFilePath, BareName))
			{
				OutInstance.ParentReference = BareName;
			}
		}

		// Producer A hands the declared types of a DreamShader parent over; producer B reads the kinds off the asset.
		if (Options.ParentSchema)
		{
			OutInstance.ParentSchema = *Options.ParentSchema;
		}
		else
		{
			::UE::DreamShader::Editor::Compiler::BuildParameterSchemaFromAsset(Parent, OutInstance.ParentSchema);
		}
		const IR::FIRParameterSchema& Schema = OutInstance.ParentSchema;

		int32 SkippedLayerOverrides = 0;
		for (const IR::EIRParameterKind Kind : InstanceDecompile::AllKinds)
		{
			EMaterialParameterType Type = EMaterialParameterType::Scalar;
			if (!::UE::DreamShader::Editor::Compiler::TryGetEngineParameterTypeForIRKind(Kind, Type))
			{
				continue;
			}

			// For the instance this walks the chain and marks what the instance itself overrides.
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
			Instance->GetAllParametersOfType(Type, Parameters);
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Parameter : Parameters)
			{
				const FMaterialParameterInfo& Info = Parameter.Key;
				const FMaterialParameterMetadata& Meta = Parameter.Value;

				if (Options.Filter == EInstanceDecompileFilter::OverriddenOnly)
				{
					if (!Meta.bOverride)
					{
						continue;
					}
				}
				else
				{
					FMaterialParameterMetadata ParentMeta;
					if (!Parent->GetParameterValue(Type, FMemoryImageMaterialParameterInfo(Info), ParentMeta) || ParentMeta.Value == Meta.Value)
					{
						continue;
					}
				}

				if (Info.Association != EMaterialParameterAssociation::GlobalParameter)
				{
					++SkippedLayerOverrides;
					continue;
				}

				IR::FIRInstanceOverride Override;
				Override.ParameterName = Info.Name.ToString();
				// The printer makes it an identifier where it is not one, and says `/// @name` then.
				Override.VariableName = Override.ParameterName;
				Override.Kind = Kind;
				InstanceDecompile::FillOverrideValue(Meta.Value, Override);

				const int32 SchemaIndex = Schema.bValid ? Schema.Find(Override.ParameterName) : INDEX_NONE;
				if (SchemaIndex != INDEX_NONE && Schema.Parameters[SchemaIndex].Kind == Kind)
				{
					const IR::FIRParameterSchemaEntry& Entry = Schema.Parameters[SchemaIndex];
					Override.DeclaredType = Entry.DeclaredType;
					if (Kind == IR::EIRParameterKind::Texture && Override.DeclaredType.IsError() && Entry.TextureKind != Lang::ETextureKind::None)
					{
						// Asset-derived schemas know the texture dimension without a source declaration.
						Override.DeclaredType = IR::FIRType::TextureOf(Entry.TextureKind);
					}
				}

				// A `uniform bool` that is not static is a scalar parameter holding 0 or 1, and anything else it holds now
				// is a number the source has to be able to say.
				if (Kind == IR::EIRParameterKind::Scalar && Override.DeclaredType.IsBool())
				{
					const double Scalar = Override.Value.V[0];
					if (Scalar != 0.0 && Scalar != 1.0)
					{
						Diagnostics.Warning(TEXT("DSH9106"), FLangSpan(), FText::Format(
							LOCTEXT("BoolParameterHoldsNumber", "'{0}' is declared 'bool' by the parent's source and '{1}' sets it to {2}; the override is written as a 'float'."),
							FText::FromString(Override.ParameterName),
							FText::FromString(Instance->GetName()),
							FText::AsNumber(Scalar)));
						Override.DeclaredType = IR::FIRType::Float(1);
					}
				}

				if (Kind == IR::EIRParameterKind::Scalar && Meta.bUsedAsAtlasPosition)
				{
					Diagnostics.Warning(TEXT("DSH9105"), FLangSpan(), FText::Format(
						LOCTEXT("AtlasOverride", "'{0}' is a curve atlas row, and '{1}' picks its curve; a '.dsi' can state the row's number and nothing else, so the rebuilt instance loses the curve."),
						FText::FromString(Override.ParameterName),
						FText::FromString(Instance->GetName())));
				}

				if (!UE::DreamShader::Lang::MakeDreamShaderIdentifier(Override.ParameterName).Equals(Override.ParameterName, ESearchCase::CaseSensitive))
				{
					Diagnostics.Info(TEXT("DSH9102"), FLangSpan(), FText::Format(
						LOCTEXT("OverrideNameSanitized", "The parameter '{0}' is not a name a variable can have; it is written under another with '/// @name {0}'."),
						FText::FromString(Override.ParameterName)));
				}

				OutInstance.Overrides.Add(MoveTemp(Override));
			}
		}

		if (SkippedLayerOverrides > 0)
		{
			Diagnostics.Warning(TEXT("DSH9101"), FLangSpan(), FText::Format(
				LOCTEXT("LayerOverridesSkipped", "'{0}' overrides {1} parameter(s) of its material layers or blends, which a '.dsi' cannot address; they are left out."),
				FText::FromString(Instance->GetName()),
				FText::AsNumber(SkippedLayerOverrides)));
		}

		// By the parent's own order, then by name: the same instance always reads the same.
		OutInstance.Overrides.StableSort([&Schema](const IR::FIRInstanceOverride& A, const IR::FIRInstanceOverride& B)
		{
			const int32 IndexA = Schema.bValid ? Schema.Find(A.ParameterName) : INDEX_NONE;
			const int32 IndexB = Schema.bValid ? Schema.Find(B.ParameterName) : INDEX_NONE;
			const int32 SortA = IndexA != INDEX_NONE ? Schema.Parameters[IndexA].SortPriority : MAX_int32;
			const int32 SortB = IndexB != INDEX_NONE ? Schema.Parameters[IndexB].SortPriority : MAX_int32;
			return SortA != SortB ? SortA < SortB : A.ParameterName.Compare(B.ParameterName, ESearchCase::CaseSensitive) < 0;
		});

		TArray<FString> Unsupported;
		::UE::DreamShader::Editor::Compiler::ReadInstanceSettings(Instance, OutInstance.Settings, Unsupported);
		for (const FString& Name : Unsupported)
		{
			Diagnostics.Warning(TEXT("DSH9104"), FLangSpan(), FText::Format(
				LOCTEXT("UnsupportedInstanceState", "'{0}' overrides '{1}', which '#pragma instance' has no key for; the rebuilt instance has the parent's."),
				FText::FromString(Instance->GetName()),
				FText::FromString(Name)));
		}

		return Diagnostics.NumErrors() == ErrorsBefore;
	}

	bool DecompileMaterialInstanceToText(
		UMaterialInstanceConstant* Instance,
		const FInstanceDecompileOptions& Options,
		FString& OutText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		OutText.Reset();

		UE::DreamShader::IR::FIRInstance Payload;
		if (!DecompileMaterialInstance(Instance, Options, Payload, Diagnostics))
		{
			return false;
		}

		// `/// @name` only where the file would make another asset: a `.dsi` names its instance after the file.
		FString AssetPathOverride;
		if (Instance->GetOutermost() && Instance->GetOutermost() != GetTransientPackage())
		{
			const FString PackageName = Instance->GetOutermost()->GetName();
			AssetPathOverride = PackageName;
			if (!Options.TargetSourceFilePath.IsEmpty())
			{
				UE::DreamShader::IR::FIRProduct Probe;
				Probe.Kind = UE::DreamShader::IR::EIRProductKind::MaterialInstance;
				Probe.Name = FPaths::GetBaseFilename(Options.TargetSourceFilePath);
				FString DerivedPackage;
				FString DerivedObjectPath;
				FString DestinationError;
				if (::UE::DreamShader::Editor::Compiler::ResolveDreamShaderProductDestination(Probe, Options.TargetSourceFilePath, DerivedPackage, DerivedObjectPath, DestinationError)
					&& DerivedPackage.Equals(PackageName, ESearchCase::IgnoreCase))
				{
					AssetPathOverride.Reset();
				}
			}
		}

		OutText = UE::DreamShader::Lang::PrintDreamShaderInstance(Payload, Options.TargetSourceFilePath, AssetPathOverride);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
