// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Reflection-based literal writer: parses a DreamShaderLang literal string and writes it into a
// UMaterialExpression FProperty by reflected type (bool/int/uint/float/enum/struct/object/asset).
// Extracted byte-for-byte from DreamShaderMaterialGeneratorSupport.cpp. Already header-declared
// (DreamShaderMaterialGeneratorPrivate.h); all literal-parser / asset-resolution dependencies live
// in sibling TUs, so this is a self-contained relocation with no new exposes.

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
	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, void* ValuePtr, const FString& ValueText, FString& OutError)
	{
		if (!Target || !Property || !ValuePtr)
		{
			OutError = TEXT("Invalid reflected property target.");
			return false;
		}

		const FString TrimmedValue = ValueText.TrimStartAndEnd();
		FString AssetObjectPath;
		const bool bHasParsedAssetReference =
			CastField<FObjectPropertyBase>(Property) != nullptr
			&& TryResolveDreamShaderAssetReference(TrimmedValue, AssetObjectPath, OutError);

		if (CastField<FObjectPropertyBase>(Property) != nullptr
			&& !bHasParsedAssetReference
			&& (TrimmedValue.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase)
				|| TrimmedValue.StartsWith(TEXT("/"))))
		{
			return false;
		}

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool bValue = false;
			if (!ParseBooleanLiteral(TrimmedValue, bValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid boolean value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			BoolProperty->SetPropertyValue(ValuePtr, bValue);
			return true;
		}

		if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			int32 ParsedValue = 0;
			if (!ParseIntegerLiteral(TrimmedValue, ParsedValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid integer value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			IntProperty->SetPropertyValue(ValuePtr, ParsedValue);
			return true;
		}

		if (FUInt32Property* UIntProperty = CastField<FUInt32Property>(Property))
		{
			uint32 ParsedValue = 0;
			if (!ParseUnsignedInteger32Literal(TrimmedValue, ParsedValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid unsigned integer value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			UIntProperty->SetPropertyValue(ValuePtr, ParsedValue);
			return true;
		}

		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(TrimmedValue, ParsedValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid numeric value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			FloatProperty->SetPropertyValue(ValuePtr, static_cast<float>(ParsedValue));
			return true;
		}

		if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(TrimmedValue, ParsedValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid numeric value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			DoubleProperty->SetPropertyValue(ValuePtr, ParsedValue);
			return true;
		}

		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			StringProperty->SetPropertyValue(ValuePtr, TrimmedValue);
			return true;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*TrimmedValue));
			return true;
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (!bHasParsedAssetReference)
			{
				OutError = FString::Printf(TEXT("Object property '%s' expects Path(...) or an absolute Unreal object path."), *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			UObject* LoadedObject = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *AssetObjectPath);
			if (!LoadedObject)
			{
				if (ObjectProperty->PropertyClass
					&& ObjectProperty->PropertyClass->IsChildOf(UTexture::StaticClass())
					&& (Property->GetName().Equals(TEXT("Texture"), ESearchCase::IgnoreCase)
						|| Property->GetName().Equals(TEXT("TextureObject"), ESearchCase::IgnoreCase)))
				{
					ObjectProperty->SetObjectPropertyValue(ValuePtr, nullptr);
					return true;
				}

				OutError = FString::Printf(TEXT("Failed to load asset '%s' for '%s'."), *AssetObjectPath, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (!LoadedObject->IsA(ObjectProperty->PropertyClass))
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("Asset '%s' is not compatible with '%s'. Expected '%s'."),
					*AssetObjectPath,
					*Property->GetName(),
					*ObjectProperty->PropertyClass->GetName());
				return false;
			}

			ObjectProperty->SetObjectPropertyValue(ValuePtr, LoadedObject);
			return true;
		}

		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				int64 EnumValue = INDEX_NONE;
				if (!TryResolveEnumLiteral(Enum, TrimmedValue, EnumValue))
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid enum value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
				return true;
			}
		}

		if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProperty->Enum)
			{
				int64 EnumValue = INDEX_NONE;
				if (!TryResolveEnumLiteral(Enum, TrimmedValue, EnumValue))
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid enum value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
				return true;
			}

			int32 ParsedValue = 0;
			if (!ParseIntegerLiteral(TrimmedValue, ParsedValue) || ParsedValue < 0 || ParsedValue > MAX_uint8)
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid byte value for '%s'."), *TrimmedValue, *Property->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(ParsedValue));
			return true;
		}

		FOutputDeviceNull ImportErrors;
		const FString ImportValue = bHasParsedAssetReference ? AssetObjectPath : TrimmedValue;
		if (Property->ImportText_Direct(*ImportValue, ValuePtr, Target, PPF_None, &ImportErrors) != nullptr)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Property '%s' on '%s' is not a supported literal type yet."), *Property->GetName(), *Target->GetClass()->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return false;
	}

	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, const FString& ValueText, FString& OutError)
	{
		if (!Target || !Property)
		{
			OutError = TEXT("Invalid reflected property target.");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
		return SetMaterialExpressionLiteralProperty(Target, Property, ValuePtr, ValueText, OutError);
	}
}
