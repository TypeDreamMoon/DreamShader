// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Expression factory for generated DreamShader materials/functions: turns an FTextShaderProperty
// definition into the matching UMaterialExpression (const literals, scalar/vector/texture
// parameters, and UE builtin nodes), applies metadata, and validates texture types. Extracted
// byte-for-byte from DreamShaderMaterialGeneratorSupport.cpp. Cross-TU dependencies are the
// exposed literal/input helpers (CreateOwnedMaterialExpression, CreateScalarLiteralExpressionEx,
// CreateVectorLiteralExpression, ResolveExpressionInputValue, TryResolvePropertyReference,
// SetMaterialExpressionLiteralProperty) declared in DreamShaderMaterialGeneratorPrivate.h.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "Misc/Crc.h"

#include "EdGraph/EdGraphNode.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "DreamShaderMaterialGeneratorPrivate.h"

namespace UE::DreamShader::Editor::Private
{
	static const TCHAR* GetTextureTypeLabel(const ETextShaderTextureType TextureType)
	{
		switch (TextureType)
		{
		case ETextShaderTextureType::TextureCube:
			return TEXT("TextureCube");
		case ETextShaderTextureType::Texture2DArray:
			return TEXT("Texture2DArray");
		case ETextShaderTextureType::VolumeTexture:
			return TEXT("VolumeTexture");
		case ETextShaderTextureType::Texture2D:
		default:
			return TEXT("Texture2D");
		}
	}

	static const TCHAR* GetDefaultTextureObjectPath(const ETextShaderTextureType TextureType)
	{
		switch (TextureType)
		{
		case ETextShaderTextureType::TextureCube:
			return TEXT("/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube");
		case ETextShaderTextureType::VolumeTexture:
			return TEXT("/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture");
		case ETextShaderTextureType::Texture2DArray:
			return nullptr;
		case ETextShaderTextureType::Texture2D:
		default:
			return TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
		}
	}

	static bool DoesTextureMatchType(const UTexture* Texture, const ETextShaderTextureType TextureType)
	{
		if (!Texture)
		{
			return false;
		}

		switch (TextureType)
		{
		case ETextShaderTextureType::TextureCube:
			return Cast<UTextureCube>(Texture) != nullptr;
		case ETextShaderTextureType::Texture2DArray:
			return Cast<UTexture2DArray>(Texture) != nullptr;
		case ETextShaderTextureType::VolumeTexture:
			return Cast<UVolumeTexture>(Texture) != nullptr;
		case ETextShaderTextureType::Texture2D:
		default:
			return !Cast<UTextureCube>(Texture)
				&& !Cast<UTexture2DArray>(Texture)
				&& !Cast<UVolumeTexture>(Texture);
		}
	}

	static bool ValidateTextureType(
		const FTextShaderPropertyDefinition& Property,
		const UTexture* Texture,
		const TCHAR* Context,
		FString& OutError)
	{
		const ETextShaderTextureType ExpectedTextureType = ResolveEffectiveTextureType(Property);
		if (DoesTextureMatchType(Texture, ExpectedTextureType))
		{
			return true;
		}

		OutError = FString::Printf(
			TEXT("%s texture property '%s' expects %s but '%s' is a '%s'."),
			Context,
			*Property.Name,
			GetTextureTypeLabel(ExpectedTextureType),
			Property.TextureDefaultObjectPath.IsEmpty() ? TEXT("<default>") : *Property.TextureDefaultObjectPath,
			Texture ? *Texture->GetClass()->GetName() : TEXT("None"));
		return false;
	}

	static FString FormatMetadataContext(const FTextShaderPropertyDefinition& Property)
	{
		return FString::Printf(TEXT("property '%s'"), *Property.Name);
	}

	static FString ResolveMetadataReflectionPropertyName(const FString& Key)
	{
		const FString NormalizedKey = UE::DreamShader::NormalizeSettingKey(Key);
		if (NormalizedKey == UE::DreamShader::NormalizeSettingKey(TEXT("Description"))
			|| NormalizedKey == UE::DreamShader::NormalizeSettingKey(TEXT("Tooltip")))
		{
			return TEXT("Desc");
		}
		if (NormalizedKey == UE::DreamShader::NormalizeSettingKey(TEXT("Category")))
		{
			return TEXT("Group");
		}
		if (NormalizedKey == UE::DreamShader::NormalizeSettingKey(TEXT("Sort")))
		{
			return TEXT("SortPriority");
		}
		return Key;
	}

	bool ApplyExpressionMetadata(UMaterialExpression* Expression, const FTextShaderMetadata& Metadata, FString& OutError)
	{
		if (!Expression)
		{
			return true;
		}

		TMap<FString, FString> ReflectedProperties = Metadata.ReflectedProperties;
		const auto ContainsMetadataKey = [&ReflectedProperties](const TCHAR* Key)
		{
			return ReflectedProperties.Contains(UE::DreamShader::NormalizeSettingKey(Key));
		};

		if (!Metadata.Group.IsEmpty()
			&& !ContainsMetadataKey(TEXT("Group"))
			&& !ContainsMetadataKey(TEXT("Category")))
		{
			ReflectedProperties.Add(UE::DreamShader::NormalizeSettingKey(TEXT("Group")), Metadata.Group);
		}
		if (Metadata.bHasSortPriority
			&& !ContainsMetadataKey(TEXT("SortPriority"))
			&& !ContainsMetadataKey(TEXT("Sort")))
		{
			ReflectedProperties.Add(UE::DreamShader::NormalizeSettingKey(TEXT("SortPriority")), FString::FromInt(Metadata.SortPriority));
		}
		if (!Metadata.Description.IsEmpty()
			&& !ContainsMetadataKey(TEXT("Description"))
			&& !ContainsMetadataKey(TEXT("Desc"))
			&& !ContainsMetadataKey(TEXT("Tooltip")))
		{
			ReflectedProperties.Add(UE::DreamShader::NormalizeSettingKey(TEXT("Desc")), Metadata.Description);
		}

		for (const TPair<FString, FString>& ReflectedProperty : ReflectedProperties)
		{
			const FString PropertyName = ResolveMetadataReflectionPropertyName(ReflectedProperty.Key);
			FProperty* Property = FindMaterialExpressionArgumentProperty(Expression->GetClass(), PropertyName);
			if (!Property)
			{
				// Group / SortPriority / Desc are the parameter-panel organization fields auto-injected
				// from FTextShaderMetadata; they live on UMaterialExpressionParameter. A few parameter
				// nodes (e.g. UMaterialExpressionDynamicParameter) are not Parameter subclasses and do
				// not expose them -- skip those advisory fields with a warning instead of aborting the
				// whole material. Any other (author-typed) reflected property still fails hard so typos
				// surface.
				const bool bIsSoftOrganizationField =
					PropertyName.Equals(TEXT("Group"), ESearchCase::IgnoreCase)
					|| PropertyName.Equals(TEXT("SortPriority"), ESearchCase::IgnoreCase)
					|| PropertyName.Equals(TEXT("Desc"), ESearchCase::IgnoreCase);
				if (bIsSoftOrganizationField)
				{
					UE_LOG(
						LogDreamShader,
						Warning,
						TEXT("'%s' does not expose the '%s' organization field; ignoring it for this parameter."),
						*Expression->GetClass()->GetName(),
						*PropertyName);
					continue;
				}

				OutError = FString::Printf(
					TEXT("Metadata property '%s' is not a reflected property on '%s'."),
					*PropertyName,
					*Expression->GetClass()->GetName());
				return false;
			}

			FString LiteralError;
			if (!SetMaterialExpressionLiteralProperty(Expression, Property, ReflectedProperty.Value, LiteralError))
			{
				OutError = FString::Printf(
					TEXT("Metadata property '%s' on '%s': %s"),
					*PropertyName,
					*Expression->GetClass()->GetName(),
					*LiteralError);
				return false;
			}
		}

		return true;
	}

	static bool SetExpressionParameterName(UMaterialExpression* Expression, const FString& ParameterName, FString& OutError)
	{
		if (!Expression)
		{
			OutError = TEXT("Invalid parameter expression.");
			return false;
		}

		if (FProperty* ParameterNameProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), TEXT("ParameterName")))
		{
			return SetMaterialExpressionLiteralProperty(Expression, ParameterNameProperty, ParameterName, OutError);
		}

		// UMaterialExpressionDynamicParameter is the one parameter node that is NOT a
		// UMaterialExpressionParameter: it has no single ParameterName, instead carrying one name per
		// output in ParamNames[]. Use the declared identifier as the primary (index 0) output name and
		// keep the engine defaults for the remaining outputs.
		if (UMaterialExpressionDynamicParameter* DynamicParameter = Cast<UMaterialExpressionDynamicParameter>(Expression))
		{
			if (DynamicParameter->ParamNames.Num() == 0)
			{
				DynamicParameter->ParamNames.Add(ParameterName);
			}
			else
			{
				DynamicParameter->ParamNames[0] = ParameterName;
			}
			return true;
		}

		OutError = FString::Printf(TEXT("'%s' does not expose a ParameterName property."), *Expression->GetClass()->GetName());
		return false;
	}

	static FString GetPropertyParameterName(const FTextShaderPropertyDefinition& Property)
	{
		if (const FString* ParameterName = Property.Metadata.ReflectedProperties.Find(UE::DreamShader::NormalizeSettingKey(TEXT("ParameterName"))))
		{
			const FString TrimmedParameterName = ParameterName->TrimStartAndEnd();
			if (!TrimmedParameterName.IsEmpty())
			{
				return TrimmedParameterName;
			}
		}

		return Property.Name;
	}

	static bool SetExpressionDefaultValue(UMaterialExpression* Expression, const FTextShaderPropertyDefinition& Property, FString& OutError)
	{
		if (!Expression || !Property.bHasDefaultValue)
		{
			return true;
		}

		if (Property.Type == ETextShaderPropertyType::Texture2D || !Property.TextureDefaultObjectPath.IsEmpty())
		{
			if (Property.TextureDefaultObjectPath.IsEmpty())
			{
				return true;
			}

			// Route the `= asset` default to whichever single-asset slot the parameter node exposes,
			// in priority order. The first matching reflected property wins; a write FAILURE on a
			// present property is returned as-is (we do not silently fall through to the next slot).
			static const TCHAR* const AssetSlotCandidates[] = {
				TEXT("Texture"),
				TEXT("TextureObject"),
				TEXT("SparseVolumeTexture"),
				TEXT("VirtualTexture"),
				TEXT("TextureCollection"),
				TEXT("Font"),
			};
			for (const TCHAR* SlotName : AssetSlotCandidates)
			{
				if (FProperty* SlotProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), SlotName))
				{
					return SetMaterialExpressionLiteralProperty(Expression, SlotProperty, Property.TextureDefaultObjectPath, OutError);
				}
			}

			OutError = FString::Printf(TEXT("'%s' does not expose a texture/asset property for %s."), *Expression->GetClass()->GetName(), *FormatMetadataContext(Property));
			return false;
		}

		FProperty* DefaultValueProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), TEXT("DefaultValue"));
		if (!DefaultValueProperty)
		{
			return true;
		}

		// A few parameter nodes are classified as Vector by output component count yet expose a scalar
		// DefaultValue -- e.g. CurveAtlasRowParameter derives from UMaterialExpressionScalarParameter and
		// its DefaultValue is a single float (the row position). Writing a (R=,G=,B=,A=) literal to a
		// float property fails outright and aborts generation, so when the reflected DefaultValue is a
		// scalar, write a single channel instead.
		if (Property.Type != ETextShaderPropertyType::Scalar
			&& (CastField<FFloatProperty>(DefaultValueProperty) != nullptr
				|| CastField<FDoubleProperty>(DefaultValueProperty) != nullptr))
		{
			return SetMaterialExpressionLiteralProperty(
				Expression,
				DefaultValueProperty,
				FString::SanitizeFloat(Property.VectorDefaultValue.R),
				OutError);
		}

		if (Property.Type == ETextShaderPropertyType::Scalar)
		{
			if (Property.ParameterNodeType.Equals(TEXT("StaticBoolParameter"), ESearchCase::IgnoreCase)
				|| Property.ParameterNodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
			{
				return SetMaterialExpressionLiteralProperty(
					Expression,
					DefaultValueProperty,
					Property.ScalarDefaultValue != 0.0 ? TEXT("true") : TEXT("false"),
					OutError);
			}

			return SetMaterialExpressionLiteralProperty(
				Expression,
				DefaultValueProperty,
				FString::SanitizeFloat(Property.ScalarDefaultValue),
				OutError);
		}

		const FString LinearColorText = FString::Printf(
			TEXT("(R=%s,G=%s,B=%s,A=%s)"),
			*FString::SanitizeFloat(Property.VectorDefaultValue.R),
			*FString::SanitizeFloat(Property.VectorDefaultValue.G),
			*FString::SanitizeFloat(Property.VectorDefaultValue.B),
			*FString::SanitizeFloat(Property.VectorDefaultValue.A));
		if (SetMaterialExpressionLiteralProperty(Expression, DefaultValueProperty, LinearColorText, OutError))
		{
			return true;
		}

		const FString VectorText = FString::Printf(
			TEXT("(X=%s,Y=%s,Z=%s,W=%s)"),
			*FString::SanitizeFloat(Property.VectorDefaultValue.R),
			*FString::SanitizeFloat(Property.VectorDefaultValue.G),
			*FString::SanitizeFloat(Property.VectorDefaultValue.B),
			*FString::SanitizeFloat(Property.VectorDefaultValue.A));
		return SetMaterialExpressionLiteralProperty(Expression, DefaultValueProperty, VectorText, OutError);
	}

	static UMaterialExpression* CreateGenericParameterNodeExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const int32 PositionY,
		FString& OutError)
	{
		UClass* ExpressionClass = ResolveMaterialExpressionClass(Property.ParameterNodeType);
		if (!ExpressionClass)
		{
			OutError = FString::Printf(TEXT("Could not resolve MaterialExpression class for parameter type '%s'."), *Property.ParameterNodeType);
			return nullptr;
		}

		auto* Expression = Cast<UMaterialExpression>(
			CreateOwnedMaterialExpression(Material, MaterialFunction, ExpressionClass, -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a '%s' node for property '%s'."), *Property.ParameterNodeType, *Property.Name);
			return nullptr;
		}

		if (!SetExpressionParameterName(Expression, GetPropertyParameterName(Property), OutError)
			|| !SetExpressionDefaultValue(Expression, Property, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
			return nullptr;
		}

		if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(Expression))
		{
			// A TextureSample parameter declared without an inline default would otherwise carry a null
			// Texture and fail to shader-compile ("Missing input Texture"). Seed the engine's
			// type-correct default texture first so a default-less sampler parameter still produces a
			// compilable material; AutoSetSampleType then derives the sampler type from it.
			if (!TextureExpression->Texture)
			{
				if (UMaterialExpressionTextureSampleParameter* SampleParameter = Cast<UMaterialExpressionTextureSampleParameter>(TextureExpression))
				{
					SampleParameter->SetDefaultTexture();
				}
			}
			TextureExpression->AutoSetSampleType();
		}

		if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
			return nullptr;
		}
		return Expression;
	}

	static UMaterialExpression* CreateConstPropertyExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const int32 PositionY,
		FString& OutError)
	{
		if (Property.Source == ETextShaderPropertySource::UEBuiltin || !Property.ParameterNodeType.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("Const property '%s' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration."),
				*Property.Name);
			return nullptr;
		}

		UMaterialExpression* Expression = nullptr;
		if (Property.Type == ETextShaderPropertyType::Scalar)
		{
			Expression = CreateScalarLiteralExpressionEx(Material, MaterialFunction, Property.ScalarDefaultValue, PositionY);
		}
		else if (Property.Type == ETextShaderPropertyType::Vector)
		{
			TArray<double> Components;
			Components.Reserve(Property.ComponentCount);
			Components.Add(Property.VectorDefaultValue.R);
			if (Property.ComponentCount >= 2)
			{
				Components.Add(Property.VectorDefaultValue.G);
			}
			if (Property.ComponentCount >= 3)
			{
				Components.Add(Property.VectorDefaultValue.B);
			}
			if (Property.ComponentCount >= 4)
			{
				Components.Add(Property.VectorDefaultValue.A);
			}
			Expression = CreateVectorLiteralExpression(Material, MaterialFunction, Components, Property.ComponentCount, PositionY);
		}
		else
		{
			auto* TextureObjectExpression = Cast<UMaterialExpressionTextureObject>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionTextureObject::StaticClass(), -800, PositionY));
			if (!TextureObjectExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create a texture object node for const property '%s'."), *Property.Name);
				return nullptr;
			}

			if (!Property.TextureDefaultObjectPath.IsEmpty())
			{
				UTexture* DefaultTexture = LoadObject<UTexture>(nullptr, *Property.TextureDefaultObjectPath);
				if (!DefaultTexture)
				{
					OutError = FString::Printf(
						TEXT("Const texture property '%s' could not load asset '%s'."),
						*Property.Name,
						*Property.TextureDefaultObjectPath);
					return nullptr;
				}

				if (!ValidateTextureType(Property, DefaultTexture, TEXT("Const"), OutError))
				{
					return nullptr;
				}

				TextureObjectExpression->Texture = DefaultTexture;
				TextureObjectExpression->AutoSetSampleType();
			}
			else if (const TCHAR* DefaultTexturePath = GetDefaultTextureObjectPath(Property.TextureType))
			{
				UTexture* DefaultTexture = LoadObject<UTexture>(nullptr, DefaultTexturePath);
				if (!DefaultTexture || !DoesTextureMatchType(DefaultTexture, Property.TextureType))
				{
					OutError = FString::Printf(
						TEXT("Const texture property '%s' could not load default %s asset '%s'."),
						*Property.Name,
						GetTextureTypeLabel(Property.TextureType),
						DefaultTexturePath);
					return nullptr;
				}

				TextureObjectExpression->Texture = DefaultTexture;
				TextureObjectExpression->AutoSetSampleType();
			}
			else
			{
				OutError = FString::Printf(
					TEXT("Const texture property '%s' with type %s requires an explicit default asset."),
					*Property.Name,
					GetTextureTypeLabel(Property.TextureType));
				return nullptr;
			}

			Expression = TextureObjectExpression;
		}

		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a const node for property '%s'."), *Property.Name);
			return nullptr;
		}

		if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
			return nullptr;
		}
		return Expression;
	}

	static UMaterialExpression* CreateParameterExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		int32 PositionY,
		FString& OutError)
	{
		if (!Property.ParameterNodeType.IsEmpty()
			&& !Property.ParameterNodeType.Equals(TEXT("ScalarParameter"), ESearchCase::IgnoreCase)
			&& !Property.ParameterNodeType.Equals(TEXT("VectorParameter"), ESearchCase::IgnoreCase)
			&& !Property.ParameterNodeType.Equals(TEXT("TextureObjectParameter"), ESearchCase::IgnoreCase))
		{
			return CreateGenericParameterNodeExpression(Material, MaterialFunction, Property, PositionY, OutError);
		}

		if (Property.Type == ETextShaderPropertyType::Scalar)
		{
			auto* Expression = Cast<UMaterialExpressionScalarParameter>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionScalarParameter::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create a scalar parameter node for property '%s'."), *Property.Name);
				return nullptr;
			}

			Expression->ParameterName = FName(*GetPropertyParameterName(Property));
			if (Property.bHasDefaultValue)
			{
				Expression->DefaultValue = static_cast<float>(Property.ScalarDefaultValue);
			}
			if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
			{
				OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
				return nullptr;
			}
			return Expression;
		}

		if (Property.Type == ETextShaderPropertyType::Vector)
		{
			auto* ParameterExpression = Cast<UMaterialExpressionVectorParameter>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionVectorParameter::StaticClass(), -800, PositionY));
			if (!ParameterExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create a vector parameter node for property '%s'."), *Property.Name);
				return nullptr;
			}

			ParameterExpression->ParameterName = FName(*GetPropertyParameterName(Property));
			if (Property.bHasDefaultValue)
			{
				ParameterExpression->DefaultValue = Property.VectorDefaultValue;
			}
			if (!ApplyExpressionMetadata(ParameterExpression, Property.Metadata, OutError))
			{
				OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
				return nullptr;
			}

			return ParameterExpression;
		}

		auto* Expression = Cast<UMaterialExpressionTextureObjectParameter>(
			CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionTextureObjectParameter::StaticClass(), -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a texture parameter node for property '%s'."), *Property.Name);
			return nullptr;
		}

		Expression->ParameterName = FName(*GetPropertyParameterName(Property));
		if (Property.bHasDefaultValue && !Property.TextureDefaultObjectPath.IsEmpty())
		{
			UTexture* DefaultTexture = LoadObject<UTexture>(nullptr, *Property.TextureDefaultObjectPath);
			if (!DefaultTexture)
			{
				OutError = FString::Printf(
					TEXT("Texture property '%s' could not load asset '%s'."),
					*Property.Name,
					*Property.TextureDefaultObjectPath);
				return nullptr;
			}

			if (!ValidateTextureType(Property, DefaultTexture, TEXT("Texture"), OutError))
			{
				return nullptr;
			}

			Expression->Texture = DefaultTexture;
			Expression->AutoSetSampleType();
		}
		else if (const TCHAR* DefaultTexturePath = GetDefaultTextureObjectPath(Property.TextureType))
		{
			UTexture* DefaultTexture = LoadObject<UTexture>(nullptr, DefaultTexturePath);
			if (!DefaultTexture || !DoesTextureMatchType(DefaultTexture, Property.TextureType))
			{
				OutError = FString::Printf(
					TEXT("Texture property '%s' could not load default %s asset '%s'."),
					*Property.Name,
					GetTextureTypeLabel(Property.TextureType),
					DefaultTexturePath);
				return nullptr;
			}

			Expression->Texture = DefaultTexture;
			Expression->AutoSetSampleType();
		}
		else
		{
			OutError = FString::Printf(
				TEXT("Texture property '%s' with type %s requires an explicit default asset."),
				*Property.Name,
				GetTextureTypeLabel(Property.TextureType));
			return nullptr;
		}
		if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
			return nullptr;
		}
		return Expression;
	}

	static bool ResolveFlexibleExpressionInputValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FString& InValueText,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		UMaterialExpression*& OutExpression,
		FString& OutError)
	{
		if (TryResolvePropertyReference(InValueText, AvailableExpressions, OutExpression))
		{
			return true;
		}

		double ScalarValue = 0.0;
		if (ParseScalarLiteral(InValueText, ScalarValue))
		{
			OutExpression = CreateScalarLiteralExpressionEx(Material, MaterialFunction, ScalarValue, PositionY);
			if (!OutExpression)
			{
				OutError = TEXT("Failed to create a scalar constant expression.");
				return false;
			}
			return true;
		}

		TArray<double> Components;
		if (ParseVectorLiteral(InValueText, Components))
		{
			const int32 ComponentCount = Components.Num();
			if (ComponentCount < 2 || ComponentCount > 4)
			{
				OutError = FString::Printf(TEXT("Unsupported vector literal '%s'."), *InValueText);
				return false;
			}

			OutExpression = CreateVectorLiteralExpression(Material, MaterialFunction, Components, ComponentCount, PositionY);
			if (!OutExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create a float%d constant expression."), ComponentCount);
				return false;
			}
			return true;
		}

		OutError = FString::Printf(TEXT("'%s' is not a valid property reference or literal input."), *InValueText);
		return false;
	}

	static UMaterialExpression* CreateGenericUEExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		FString ClassSpecifier = Property.UEBuiltinFunctionName;
		if (FString ExplicitClass; TryGetUEBuiltinArgument(Property, TEXT("Class"), ExplicitClass))
		{
			ClassSpecifier = ExplicitClass;
		}

		UClass* ExpressionClass = ResolveMaterialExpressionClass(ClassSpecifier);
		if (!ExpressionClass)
		{
			OutError = FString::Printf(TEXT("UE.%s for property '%s': could not resolve MaterialExpression class '%s'."),
				*Property.UEBuiltinFunctionName,
				*Property.Name,
				*ClassSpecifier);
			return nullptr;
		}

		auto* Expression = Cast<UMaterialExpression>(
			CreateOwnedMaterialExpression(Material, MaterialFunction, ExpressionClass, -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("UE.%s for property '%s': failed to create '%s'."),
				*Property.UEBuiltinFunctionName,
				*Property.Name,
				*ExpressionClass->GetName());
			return nullptr;
		}

		if (UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(Expression))
		{
			if (FString OutputTypeText; TryGetUEBuiltinArgument(Property, TEXT("OutputType"), OutputTypeText)
				|| TryGetUEBuiltinArgument(Property, TEXT("ResultType"), OutputTypeText))
			{
				ECustomMaterialOutputType CustomOutputType = CMOT_Float1;
				if (!TryResolveCustomOutputType(OutputTypeText, CustomOutputType))
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': OutputType '%s' is not a valid Custom node output type."),
						*Property.UEBuiltinFunctionName,
						*Property.Name,
						*OutputTypeText);
					return nullptr;
				}
				CustomExpression->OutputType = CustomOutputType;
			}
			CustomExpression->Inputs.Reset();
			CustomExpression->AdditionalOutputs.Reset();
		}

		if (!Property.UEBuiltinArguments.Contains(UE::DreamShader::NormalizeSettingKey(TEXT("ParameterName")))
			&& FindMaterialExpressionArgumentProperty(ExpressionClass, TEXT("ParameterName")))
		{
			FString ParameterNameError;
			if (!SetExpressionParameterName(Expression, Property.Name, ParameterNameError))
			{
				OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"),
					*Property.UEBuiltinFunctionName,
					*Property.Name,
					*ParameterNameError);
				return nullptr;
			}
		}

		for (const TPair<FString, FString>& Argument : Property.UEBuiltinArguments)
		{
			if (Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("Class"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("OutputType"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")))
			{
				continue;
			}

			FProperty* BoundProperty = FindMaterialExpressionArgumentProperty(ExpressionClass, Argument.Key);
			if (!BoundProperty)
			{
				if (UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(Expression))
				{
					UMaterialExpression* InputExpression = nullptr;
					FString InputError;
					if (!ResolveFlexibleExpressionInputValue(Material, MaterialFunction, Argument.Value, AvailableExpressions, PositionY - 80, InputExpression, InputError))
					{
						OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"), *Property.UEBuiltinFunctionName, *Property.Name, *InputError);
						return nullptr;
					}

					FCustomInput CustomInput;
					CustomInput.InputName = FName(*Argument.Key);
					CustomExpression->Inputs.Add(CustomInput);
					CustomExpression->Inputs.Last().Input.Expression = InputExpression;
					continue;
				}

				OutError = FString::Printf(TEXT("UE.%s for property '%s': '%s' is not a property on '%s'."),
					*Property.UEBuiltinFunctionName,
					*Property.Name,
					*Argument.Key,
					*ExpressionClass->GetName());
				return nullptr;
			}

			if (IsMaterialExpressionInputProperty(BoundProperty))
			{
				UMaterialExpression* InputExpression = nullptr;
				FString InputError;
				if (!ResolveFlexibleExpressionInputValue(Material, MaterialFunction, Argument.Value, AvailableExpressions, PositionY - 80, InputExpression, InputError))
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"), *Property.UEBuiltinFunctionName, *Property.Name, *InputError);
					return nullptr;
				}

				FExpressionInput* Input = BoundProperty->ContainerPtrToValuePtr<FExpressionInput>(Expression);
				if (!Input)
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': failed to bind input '%s'."),
						*Property.UEBuiltinFunctionName,
						*Property.Name,
						*Argument.Key);
					return nullptr;
				}
				Input->Expression = InputExpression;
			}
			else
			{
				FString LiteralError;
				void* ValuePtr = BoundProperty->ContainerPtrToValuePtr<void>(Expression);
				if (!SetMaterialExpressionLiteralProperty(Expression, BoundProperty, ValuePtr, Argument.Value, LiteralError))
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"),
						*Property.UEBuiltinFunctionName,
						*Property.Name,
						*LiteralError);
					return nullptr;
				}
			}
		}

		if (UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(Expression))
		{
			FString OutputNameText;
			int32 RequestedOutputIndex = 0;
			if (TryGetUEBuiltinArgument(Property, TEXT("Output"), OutputNameText)
				|| TryGetUEBuiltinArgument(Property, TEXT("OutputName"), OutputNameText))
			{
				OutputNameText.TrimStartAndEndInline();
				RequestedOutputIndex = 1;
			}
			else if (FString OutputIndexText; TryGetUEBuiltinArgument(Property, TEXT("OutputIndex"), OutputIndexText))
			{
				if (!ParseIntegerLiteral(OutputIndexText, RequestedOutputIndex) || RequestedOutputIndex < 0)
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': OutputIndex is out of range for '%s'."),
						*Property.UEBuiltinFunctionName,
						*Property.Name,
						*ExpressionClass->GetName());
					return nullptr;
				}
			}

			for (int32 AdditionalOutputIndex = 0; AdditionalOutputIndex < RequestedOutputIndex; ++AdditionalOutputIndex)
			{
				FCustomOutput CustomOutput;
				CustomOutput.OutputName = FName(*(
					AdditionalOutputIndex == RequestedOutputIndex - 1 && !OutputNameText.IsEmpty()
						? OutputNameText
						: FString::Printf(TEXT("Output%d"), AdditionalOutputIndex + 1)));
				CustomOutput.OutputType = CustomExpression->OutputType;
				CustomExpression->AdditionalOutputs.Add(CustomOutput);
			}

			RebuildDreamShaderCustomOutputs(CustomExpression);
		}

		if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
		{
			OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"),
				*Property.UEBuiltinFunctionName,
				*Property.Name,
				*OutError);
			return nullptr;
		}
		return Expression;
	}

	static UMaterialExpression* CreateUEBuiltinExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		const auto MakeError = [&Property](const FString& Message)
		{
			return FString::Printf(TEXT("UE.%s for property '%s': %s"), *Property.UEBuiltinFunctionName, *Property.Name, *Message);
		};

		if (Property.UEBuiltinFunctionName.Equals(TEXT("CollectionParam"), ESearchCase::IgnoreCase)
			|| Property.UEBuiltinFunctionName.Equals(TEXT("CollectionParameter"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("Collection"),
				TEXT("Asset"),
				TEXT("Parameter"),
				TEXT("ParameterName"),
				TEXT("OutputType"),
				TEXT("ResultType"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			FString CollectionText;
			if (!TryGetUEBuiltinArgument(Property, TEXT("Collection"), CollectionText))
			{
				TryGetUEBuiltinArgument(Property, TEXT("Asset"), CollectionText);
			}
			if (CollectionText.IsEmpty())
			{
				OutError = MakeError(TEXT("CollectionParam requires Collection=Path(...)."));
				return nullptr;
			}

			FString CollectionObjectPath;
			if (!TryResolveDreamShaderAssetReference(CollectionText, CollectionObjectPath, OutError))
			{
				OutError = MakeError(FString::Printf(TEXT("Collection is invalid: %s"), *OutError));
				return nullptr;
			}

			UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionObjectPath);
			if (!Collection)
			{
				OutError = MakeError(FString::Printf(TEXT("Could not load MaterialParameterCollection '%s'."), *CollectionObjectPath));
				return nullptr;
			}

			FString ParameterText;
			if (!TryGetUEBuiltinArgument(Property, TEXT("Parameter"), ParameterText))
			{
				TryGetUEBuiltinArgument(Property, TEXT("ParameterName"), ParameterText);
			}
			if (ParameterText.IsEmpty())
			{
				OutError = MakeError(TEXT("CollectionParam requires Parameter=\"Name\"."));
				return nullptr;
			}

			const FName ParameterName(*ParameterText);
			if (!Collection->GetScalarParameterByName(ParameterName) && !Collection->GetVectorParameterByName(ParameterName))
			{
				OutError = MakeError(FString::Printf(TEXT("Collection '%s' does not contain parameter '%s'."), *CollectionObjectPath, *ParameterText));
				return nullptr;
			}

			auto* Expression = Cast<UMaterialExpressionCollectionParameter>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionCollectionParameter::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native CollectionParameter node."));
				return nullptr;
			}

			Expression->Collection = Collection;
			Expression->ParameterName = ParameterName;
			Expression->ParameterId = Collection->GetParameterId(ParameterName);
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
			if (!Expression->ExpressionGUID.IsValid())
			{
				Expression->ExpressionGUID = FGuid::NewGuid();
			}
#endif
			if (!ApplyExpressionMetadata(Expression, Property.Metadata, OutError))
			{
				OutError = MakeError(OutError);
				return nullptr;
			}
			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("TexCoord"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("Index"),
				TEXT("CoordinateIndex"),
				TEXT("UTiling"),
				TEXT("VTiling"),
				TEXT("UnMirrorU"),
				TEXT("UnMirrorV"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			auto* Expression = Cast<UMaterialExpressionTextureCoordinate>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionTextureCoordinate::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native TexCoord node."));
				return nullptr;
			}

			FString IndexValue;
			const bool bHasIndex = TryGetUEBuiltinArgument(Property, TEXT("Index"), IndexValue);
			FString CoordinateIndexValue;
			const bool bHasCoordinateIndex = TryGetUEBuiltinArgument(Property, TEXT("CoordinateIndex"), CoordinateIndexValue);
			if (bHasIndex && bHasCoordinateIndex)
			{
				OutError = MakeError(TEXT("Use either Index or CoordinateIndex, not both."));
				return nullptr;
			}

			if (bHasIndex || bHasCoordinateIndex)
			{
				int32 ParsedIndex = 0;
				const FString& ValueToParse = bHasIndex ? IndexValue : CoordinateIndexValue;
				if (!ParseIntegerLiteral(ValueToParse, ParsedIndex) || ParsedIndex < 0)
				{
					OutError = MakeError(FString::Printf(TEXT("'%s' is not a valid non-negative UV channel index."), *ValueToParse));
					return nullptr;
				}

				Expression->CoordinateIndex = ParsedIndex;
			}

			if (FString TilingValue; TryGetUEBuiltinArgument(Property, TEXT("UTiling"), TilingValue))
			{
				double ParsedTiling = 1.0;
				if (!ParseScalarLiteral(TilingValue, ParsedTiling))
				{
					OutError = MakeError(FString::Printf(TEXT("UTiling value '%s' is invalid."), *TilingValue));
					return nullptr;
				}
				Expression->UTiling = static_cast<float>(ParsedTiling);
			}

			if (FString TilingValue; TryGetUEBuiltinArgument(Property, TEXT("VTiling"), TilingValue))
			{
				double ParsedTiling = 1.0;
				if (!ParseScalarLiteral(TilingValue, ParsedTiling))
				{
					OutError = MakeError(FString::Printf(TEXT("VTiling value '%s' is invalid."), *TilingValue));
					return nullptr;
				}
				Expression->VTiling = static_cast<float>(ParsedTiling);
			}

			if (FString FlagValue; TryGetUEBuiltinArgument(Property, TEXT("UnMirrorU"), FlagValue))
			{
				bool bParsedFlag = false;
				if (!ParseBooleanLiteral(FlagValue, bParsedFlag))
				{
					OutError = MakeError(FString::Printf(TEXT("UnMirrorU value '%s' is invalid."), *FlagValue));
					return nullptr;
				}
				Expression->UnMirrorU = bParsedFlag ? 1U : 0U;
			}

			if (FString FlagValue; TryGetUEBuiltinArgument(Property, TEXT("UnMirrorV"), FlagValue))
			{
				bool bParsedFlag = false;
				if (!ParseBooleanLiteral(FlagValue, bParsedFlag))
				{
					OutError = MakeError(FString::Printf(TEXT("UnMirrorV value '%s' is invalid."), *FlagValue));
					return nullptr;
				}
				Expression->UnMirrorV = bParsedFlag ? 1U : 0U;
			}

			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("Time"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("IgnorePause"),
				TEXT("Period"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			auto* Expression = Cast<UMaterialExpressionTime>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionTime::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native Time node."));
				return nullptr;
			}

			if (FString FlagValue; TryGetUEBuiltinArgument(Property, TEXT("IgnorePause"), FlagValue))
			{
				bool bParsedFlag = false;
				if (!ParseBooleanLiteral(FlagValue, bParsedFlag))
				{
					OutError = MakeError(FString::Printf(TEXT("IgnorePause value '%s' is invalid."), *FlagValue));
					return nullptr;
				}
				Expression->bIgnorePause = bParsedFlag ? 1U : 0U;
			}

			if (FString PeriodValue; TryGetUEBuiltinArgument(Property, TEXT("Period"), PeriodValue))
			{
				double ParsedPeriod = 0.0;
				if (!ParseScalarLiteral(PeriodValue, ParsedPeriod) || ParsedPeriod < 0.0)
				{
					OutError = MakeError(FString::Printf(TEXT("Period value '%s' is invalid."), *PeriodValue));
					return nullptr;
				}
				Expression->bOverride_Period = true;
				Expression->Period = static_cast<float>(ParsedPeriod);
			}

			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("Panner"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("Coordinate"),
				TEXT("Time"),
				TEXT("Speed"),
				TEXT("SpeedX"),
				TEXT("SpeedY"),
				TEXT("ConstCoordinate"),
				TEXT("FractionalPart"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			auto* Expression = Cast<UMaterialExpressionPanner>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionPanner::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native Panner node."));
				return nullptr;
			}

			if (FString CoordinateValue; TryGetUEBuiltinArgument(Property, TEXT("Coordinate"), CoordinateValue))
			{
				int32 ParsedCoordinateIndex = 0;
				if (ParseIntegerLiteral(CoordinateValue, ParsedCoordinateIndex))
				{
					Expression->ConstCoordinate = ParsedCoordinateIndex;
				}
				else
				{
					UMaterialExpression* CoordinateExpression = nullptr;
					FString InputError;
					if (!ResolveExpressionInputValue(Material, MaterialFunction, CoordinateValue, AvailableExpressions, 2, PositionY - 80, CoordinateExpression, InputError))
					{
						OutError = MakeError(FString::Printf(TEXT("Coordinate input is invalid. %s"), *InputError));
						return nullptr;
					}
					Expression->Coordinate.Expression = CoordinateExpression;
				}
			}

			if (FString CoordinateValue; TryGetUEBuiltinArgument(Property, TEXT("ConstCoordinate"), CoordinateValue))
			{
				int32 ParsedCoordinateIndex = 0;
				if (!ParseIntegerLiteral(CoordinateValue, ParsedCoordinateIndex) || ParsedCoordinateIndex < 0)
				{
					OutError = MakeError(FString::Printf(TEXT("ConstCoordinate value '%s' is invalid."), *CoordinateValue));
					return nullptr;
				}
				Expression->ConstCoordinate = ParsedCoordinateIndex;
			}

			if (FString TimeValue; TryGetUEBuiltinArgument(Property, TEXT("Time"), TimeValue))
			{
				UMaterialExpression* TimeExpression = nullptr;
				FString InputError;
				if (!ResolveExpressionInputValue(Material, MaterialFunction, TimeValue, AvailableExpressions, 1, PositionY - 40, TimeExpression, InputError))
				{
					OutError = MakeError(FString::Printf(TEXT("Time input is invalid. %s"), *InputError));
					return nullptr;
				}
				Expression->Time.Expression = TimeExpression;
			}

			if (FString SpeedValue; TryGetUEBuiltinArgument(Property, TEXT("Speed"), SpeedValue))
			{
				UMaterialExpression* SpeedExpression = nullptr;
				FString InputError;
				if (!ResolveExpressionInputValue(Material, MaterialFunction, SpeedValue, AvailableExpressions, 2, PositionY + 40, SpeedExpression, InputError))
				{
					OutError = MakeError(FString::Printf(TEXT("Speed input is invalid. %s"), *InputError));
					return nullptr;
				}
				Expression->Speed.Expression = SpeedExpression;
			}

			if (FString SpeedValue; TryGetUEBuiltinArgument(Property, TEXT("SpeedX"), SpeedValue))
			{
				double ParsedSpeed = 0.0;
				if (!ParseScalarLiteral(SpeedValue, ParsedSpeed))
				{
					OutError = MakeError(FString::Printf(TEXT("SpeedX value '%s' is invalid."), *SpeedValue));
					return nullptr;
				}
				Expression->SpeedX = static_cast<float>(ParsedSpeed);
			}

			if (FString SpeedValue; TryGetUEBuiltinArgument(Property, TEXT("SpeedY"), SpeedValue))
			{
				double ParsedSpeed = 0.0;
				if (!ParseScalarLiteral(SpeedValue, ParsedSpeed))
				{
					OutError = MakeError(FString::Printf(TEXT("SpeedY value '%s' is invalid."), *SpeedValue));
					return nullptr;
				}
				Expression->SpeedY = static_cast<float>(ParsedSpeed);
			}

			if (FString FlagValue; TryGetUEBuiltinArgument(Property, TEXT("FractionalPart"), FlagValue))
			{
				bool bParsedFlag = false;
				if (!ParseBooleanLiteral(FlagValue, bParsedFlag))
				{
					OutError = MakeError(FString::Printf(TEXT("FractionalPart value '%s' is invalid."), *FlagValue));
					return nullptr;
				}
				Expression->bFractionalPart = bParsedFlag;
			}

			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("WorldPosition"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("ShaderOffsets"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			auto* Expression = Cast<UMaterialExpressionWorldPosition>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionWorldPosition::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native WorldPosition node."));
				return nullptr;
			}

			if (FString OffsetValue; TryGetUEBuiltinArgument(Property, TEXT("ShaderOffsets"), OffsetValue))
			{
				EWorldPositionIncludedOffsets ParsedOffset = WPT_Default;
				if (!TryResolveWorldPositionShaderOffset(OffsetValue, ParsedOffset))
				{
					OutError = MakeError(FString::Printf(TEXT("ShaderOffsets value '%s' is invalid."), *OffsetValue));
					return nullptr;
				}
				Expression->WorldPositionShaderOffset = ParsedOffset;
			}

			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("ObjectPositionWS"), ESearchCase::IgnoreCase))
		{
			static const TCHAR* AllowedArguments[] =
			{
				TEXT("Origin"),
			};
			if (!ValidateUEBuiltinArgumentNames(Property, AllowedArguments, OutError))
			{
				return nullptr;
			}

			UClass* ObjectPositionClass = GetDreamShaderObjectPositionExpressionClass();
			auto* Expression = ObjectPositionClass
				? static_cast<UMaterialExpressionObjectPositionWS*>(CreateOwnedMaterialExpression(Material, MaterialFunction, ObjectPositionClass, -800, PositionY))
				: nullptr;
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native ObjectPositionWS node."));
				return nullptr;
			}

			if (FString OriginValue; TryGetUEBuiltinArgument(Property, TEXT("Origin"), OriginValue))
			{
				EPositionOrigin ParsedOrigin = EPositionOrigin::Absolute;
				if (!TryResolvePositionOrigin(OriginValue, ParsedOrigin))
				{
					OutError = MakeError(FString::Printf(TEXT("Origin value '%s' is invalid."), *OriginValue));
					return nullptr;
				}
				Expression->OriginType = ParsedOrigin;
			}

			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("CameraVectorWS"), ESearchCase::IgnoreCase))
		{
			if (!Property.UEBuiltinArguments.IsEmpty())
			{
				OutError = MakeError(TEXT("CameraVectorWS does not take any arguments."));
				return nullptr;
			}

			UMaterialExpression* Expression =
				Cast<UMaterialExpressionCameraVectorWS>(
					CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionCameraVectorWS::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native CameraVectorWS node."));
			}
			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("ScreenPosition"), ESearchCase::IgnoreCase))
		{
			if (!Property.UEBuiltinArguments.IsEmpty())
			{
				OutError = MakeError(TEXT("ScreenPosition does not take any arguments."));
				return nullptr;
			}

			UClass* ScreenPositionClass = GetDreamShaderScreenPositionExpressionClass();
			UMaterialExpression* Expression = ScreenPositionClass
				? CreateOwnedMaterialExpression(Material, MaterialFunction, ScreenPositionClass, -800, PositionY)
				: nullptr;
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native ScreenPosition node."));
			}
			return Expression;
		}

		if (Property.UEBuiltinFunctionName.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase))
		{
			if (!Property.UEBuiltinArguments.IsEmpty())
			{
				OutError = MakeError(TEXT("VertexColor does not take any arguments."));
				return nullptr;
			}

			UMaterialExpression* Expression =
				Cast<UMaterialExpressionVertexColor>(
					CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionVertexColor::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native VertexColor node."));
			}
			return Expression;
		}

		if (Property.UEBuiltinArguments.Contains(UE::DreamShader::NormalizeSettingKey(TEXT("OutputType")))
			|| Property.UEBuiltinArguments.Contains(UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))))
		{
			return CreateGenericUEExpression(Material, MaterialFunction, Property, AvailableExpressions, PositionY, OutError);
		}

		OutError = MakeError(TEXT("This builtin is not implemented by the material generator yet. For generic MaterialExpression support, add OutputType=\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\"."));
		return nullptr;
	}

	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		if (Property.bConst)
		{
			return CreateConstPropertyExpression(Material, MaterialFunction, Property, PositionY, OutError);
		}

		if (Property.Source == ETextShaderPropertySource::UEBuiltin)
		{
			return CreateUEBuiltinExpression(Material, MaterialFunction, Property, AvailableExpressions, PositionY, OutError);
		}

		UMaterialExpression* Expression = CreateParameterExpression(Material, MaterialFunction, Property, PositionY, OutError);
		if (!Expression)
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Failed to create a parameter node for property '%s'."), *Property.Name);
			}
		}
		return Expression;
	}

	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		return CreatePropertyExpression(Material, nullptr, Property, AvailableExpressions, PositionY, OutError);
	}
}
