// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderInstanceSchema.h.
//
// Kinds are walked by INDEX, never by a hand-written list of enumerators, for the reason
// Assets/DreamShaderThinCustomParameterOverrides.cpp gives: the set grows between engine versions. The enumerators an
// older supported engine may not have (texture collections, parameter collections) are named only behind a version
// guard.

#include "DreamShaderInstanceSchema.h"

#include "DreamShaderVersionCompat.h"

#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "Misc/CString.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderInstanceSchemaDetail
	{
		bool TryGetIRKindForEngineParameterType(const EMaterialParameterType Type, IR::EIRParameterKind& OutKind)
		{
			switch (Type)
			{
			case EMaterialParameterType::Scalar:                OutKind = IR::EIRParameterKind::Scalar; return true;
			case EMaterialParameterType::Vector:                OutKind = IR::EIRParameterKind::Vector; return true;
			case EMaterialParameterType::DoubleVector:          OutKind = IR::EIRParameterKind::DoubleVector; return true;
			case EMaterialParameterType::Texture:               OutKind = IR::EIRParameterKind::Texture; return true;
			case EMaterialParameterType::Font:                  OutKind = IR::EIRParameterKind::Font; return true;
			case EMaterialParameterType::RuntimeVirtualTexture: OutKind = IR::EIRParameterKind::RuntimeVirtualTexture; return true;
			case EMaterialParameterType::SparseVolumeTexture:   OutKind = IR::EIRParameterKind::SparseVolumeTexture; return true;
			case EMaterialParameterType::StaticSwitch:          OutKind = IR::EIRParameterKind::StaticSwitch; return true;
			case EMaterialParameterType::StaticComponentMask:   OutKind = IR::EIRParameterKind::StaticComponentMask; return true;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			case EMaterialParameterType::TextureCollection:     OutKind = IR::EIRParameterKind::TextureCollection; return true;
			case EMaterialParameterType::ParameterCollection:   OutKind = IR::EIRParameterKind::ParameterCollection; return true;
#endif
			default:
				return false;
			}
		}

		FString GetDreamShaderSchemaObjectPath(const UObject* Object)
		{
			return Object ? Object->GetPathName() : FString();
		}

		/** The dimension of a default texture, as a `.dsi` override has to spell it; None when there is no texture. */
		Lang::ETextureKind GetDreamShaderSchemaTextureKind(const UObject* Texture)
		{
			if (!Texture)
			{
				return Lang::ETextureKind::None;
			}
			if (Texture->IsA<UTextureCube>())
			{
				return Lang::ETextureKind::TextureCube;
			}
			if (Texture->IsA<UTexture2DArray>())
			{
				return Lang::ETextureKind::Texture2DArray;
			}
			if (Texture->IsA<UVolumeTexture>())
			{
				return Lang::ETextureKind::Texture3D;
			}
			return Lang::ETextureKind::Texture2D;
		}

		/** A parameter's value in FIRInstanceOverride::Value's encoding. The caller has checked Value.Type matches Entry.Kind. */
		void FillDreamShaderSchemaParentValue(const FMaterialParameterValue& Value, IR::FIRParameterSchemaEntry& Entry)
		{
			switch (Entry.Kind)
			{
			case IR::EIRParameterKind::Scalar:
			{
				const double Channels[4] = { Value.AsScalar(), 0.0, 0.0, 0.0 };
				Entry.ParentValue = IR::FIRPropertyValue::MakeFloat4(Channels, 1);
				return;
			}
			case IR::EIRParameterKind::Vector:
			{
				const FLinearColor Color = Value.AsLinearColor();
				const double Channels[4] = { Color.R, Color.G, Color.B, Color.A };
				Entry.ParentValue = IR::FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case IR::EIRParameterKind::DoubleVector:
			{
				const FVector4d Vector = Value.AsVector4d();
				const double Channels[4] = { Vector.X, Vector.Y, Vector.Z, Vector.W };
				Entry.ParentValue = IR::FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case IR::EIRParameterKind::StaticSwitch:
				Entry.ParentValue = IR::FIRPropertyValue::MakeBool(Value.AsStaticSwitch());
				return;
			case IR::EIRParameterKind::StaticComponentMask:
			{
				const FStaticComponentMaskValue Mask = Value.AsStaticComponentMask();
				const double Channels[4] = { Mask.R ? 1.0 : 0.0, Mask.G ? 1.0 : 0.0, Mask.B ? 1.0 : 0.0, Mask.A ? 1.0 : 0.0 };
				Entry.ParentValue = IR::FIRPropertyValue::MakeFloat4(Channels, 4);
				return;
			}
			case IR::EIRParameterKind::Texture:
			{
				const UObject* Texture = Value.AsTextureObject();
				Entry.TextureKind = GetDreamShaderSchemaTextureKind(Texture);
				Entry.ParentValue = IR::FIRPropertyValue::MakeObject(GetDreamShaderSchemaObjectPath(Texture));
				return;
			}
			case IR::EIRParameterKind::RuntimeVirtualTexture:
			case IR::EIRParameterKind::SparseVolumeTexture:
			case IR::EIRParameterKind::TextureCollection:
				// AsTextureObject answers every texture-like kind with the object itself.
				Entry.ParentValue = IR::FIRPropertyValue::MakeObject(GetDreamShaderSchemaObjectPath(Value.AsTextureObject()));
				return;
			case IR::EIRParameterKind::Font:
			{
				// Not AsTextureObject, which hands back the font's page texture rather than the font.
				const UObject* FontObject = Value.Font.Value;
				Entry.ParentValue = IR::FIRPropertyValue::MakeObject(GetDreamShaderSchemaObjectPath(FontObject));
				Entry.ParentFontPage = Value.Font.Page;
				return;
			}
			case IR::EIRParameterKind::ParameterCollection:
			{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				const UObject* Collection = Value.ParameterCollection;
				Entry.ParentValue = IR::FIRPropertyValue::MakeObject(GetDreamShaderSchemaObjectPath(Collection));
#else
				Entry.ParentValue = IR::FIRPropertyValue::MakeObject(FString());
#endif
				return;
			}
			}
		}
	}

	bool TryGetEngineParameterTypeForIRKind(const IR::EIRParameterKind Kind, EMaterialParameterType& OutType)
	{
		switch (Kind)
		{
		case IR::EIRParameterKind::Scalar:                OutType = EMaterialParameterType::Scalar; return true;
		case IR::EIRParameterKind::Vector:                OutType = EMaterialParameterType::Vector; return true;
		case IR::EIRParameterKind::DoubleVector:          OutType = EMaterialParameterType::DoubleVector; return true;
		case IR::EIRParameterKind::Texture:               OutType = EMaterialParameterType::Texture; return true;
		case IR::EIRParameterKind::Font:                  OutType = EMaterialParameterType::Font; return true;
		case IR::EIRParameterKind::RuntimeVirtualTexture: OutType = EMaterialParameterType::RuntimeVirtualTexture; return true;
		case IR::EIRParameterKind::SparseVolumeTexture:   OutType = EMaterialParameterType::SparseVolumeTexture; return true;
		case IR::EIRParameterKind::StaticSwitch:          OutType = EMaterialParameterType::StaticSwitch; return true;
		case IR::EIRParameterKind::StaticComponentMask:   OutType = EMaterialParameterType::StaticComponentMask; return true;
		case IR::EIRParameterKind::TextureCollection:
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			OutType = EMaterialParameterType::TextureCollection;
			return true;
#else
			return false;
#endif
		case IR::EIRParameterKind::ParameterCollection:
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			OutType = EMaterialParameterType::ParameterCollection;
			return true;
#else
			return false;
#endif
		}
		return false;
	}

	bool BuildParameterSchemaFromAsset(const UMaterialInterface* Parent, IR::FIRParameterSchema& OutSchema)
	{
		using namespace DreamShaderInstanceSchemaDetail;

		OutSchema = IR::FIRParameterSchema();
		if (!Parent)
		{
			return false;
		}

		OutSchema.bValid = true;
		OutSchema.Origin = TEXT("asset");
		OutSchema.ParentObjectPath = Parent->GetPathName();
		OutSchema.bParentIsInstance = Parent->IsA<UMaterialInstance>();

		for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
		{
			const EMaterialParameterType Type = static_cast<EMaterialParameterType>(TypeIndex);
			IR::EIRParameterKind Kind = IR::EIRParameterKind::Scalar;
			if (!TryGetIRKindForEngineParameterType(Type, Kind))
			{
				continue;
			}

			// For an instance this walks the chain, so every parameter the root declares is listed with the value this
			// parent effectively has.
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
			Parent->GetAllParametersOfType(Type, Parameters);
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Parameter : Parameters)
			{
				IR::FIRParameterSchemaEntry& Entry = OutSchema.Parameters.AddDefaulted_GetRef();
				Entry.Name = Parameter.Key.Name.ToString();
				Entry.Kind = Kind;

				switch (Parameter.Key.Association)
				{
				case EMaterialParameterAssociation::LayerParameter:
					Entry.Association = IR::EIRParameterAssociation::Layer;
					break;
				case EMaterialParameterAssociation::BlendParameter:
					Entry.Association = IR::EIRParameterAssociation::Blend;
					break;
				default:
					Entry.Association = IR::EIRParameterAssociation::Global;
					break;
				}
				Entry.AssociationIndex = Entry.Association == IR::EIRParameterAssociation::Global ? INDEX_NONE : Parameter.Key.Index;

				// The As* accessors check the type, so a value of another type is left at the entry's default.
				if (Parameter.Value.Value.Type == Type)
				{
					FillDreamShaderSchemaParentValue(Parameter.Value.Value, Entry);
				}

#if WITH_EDITORONLY_DATA
				Entry.Group = Parameter.Value.Group.IsNone() ? FString() : Parameter.Value.Group.ToString();
				Entry.SortPriority = Parameter.Value.SortPriority;
#endif
			}
		}

		// A stable order: the maps iterate in no order anything may depend on.
		OutSchema.Parameters.Sort([](const IR::FIRParameterSchemaEntry& Left, const IR::FIRParameterSchemaEntry& Right)
		{
			if (Left.SortPriority != Right.SortPriority)
			{
				return Left.SortPriority < Right.SortPriority;
			}
			return FCString::Strcmp(*Left.Name, *Right.Name) < 0;
		});
		return true;
	}
}
