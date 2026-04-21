#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"

#include "Misc/Crc.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	bool ResolveMaterialProperty(const FString& InName, FResolvedMaterialProperty& OutProperty)
	{
		const auto Matches = [&InName](const TCHAR* Candidate)
		{
			return InName.Equals(Candidate, ESearchCase::IgnoreCase);
		};

		if (Matches(TEXT("BaseColor")))
		{
			OutProperty = { MP_BaseColor, CMOT_Float3 };
		}
		else if (Matches(TEXT("EmissiveColor")) || Matches(TEXT("Emissive")))
		{
			OutProperty = { MP_EmissiveColor, CMOT_Float3 };
		}
		else if (Matches(TEXT("Opacity")))
		{
			OutProperty = { MP_Opacity, CMOT_Float1 };
		}
		else if (Matches(TEXT("OpacityMask")))
		{
			OutProperty = { MP_OpacityMask, CMOT_Float1 };
		}
		else if (Matches(TEXT("Metallic")))
		{
			OutProperty = { MP_Metallic, CMOT_Float1 };
		}
		else if (Matches(TEXT("Specular")))
		{
			OutProperty = { MP_Specular, CMOT_Float1 };
		}
		else if (Matches(TEXT("Roughness")))
		{
			OutProperty = { MP_Roughness, CMOT_Float1 };
		}
		else if (Matches(TEXT("Normal")))
		{
			OutProperty = { MP_Normal, CMOT_Float3 };
		}
		else if (Matches(TEXT("AmbientOcclusion")) || Matches(TEXT("AO")))
		{
			OutProperty = { MP_AmbientOcclusion, CMOT_Float1 };
		}
		else if (Matches(TEXT("Refraction")))
		{
			OutProperty = { MP_Refraction, CMOT_Float1 };
		}
		else if (Matches(TEXT("WorldPositionOffset")) || Matches(TEXT("WPO")))
		{
			OutProperty = { MP_WorldPositionOffset, CMOT_Float3 };
		}
		else if (Matches(TEXT("PixelDepthOffset")) || Matches(TEXT("PDO")))
		{
			OutProperty = { MP_PixelDepthOffset, CMOT_Float1 };
		}
		else if (Matches(TEXT("SubsurfaceColor")))
		{
			OutProperty = { MP_SubsurfaceColor, CMOT_Float3 };
		}
		else if (Matches(TEXT("ClearCoat")))
		{
			OutProperty = { MP_CustomData0, CMOT_Float1 };
		}
		else if (Matches(TEXT("ClearCoatRoughness")))
		{
			OutProperty = { MP_CustomData1, CMOT_Float1 };
		}
		else if (Matches(TEXT("Anisotropy")))
		{
			OutProperty = { MP_Anisotropy, CMOT_Float1 };
		}
		else if (Matches(TEXT("Tangent")))
		{
			OutProperty = { MP_Tangent, CMOT_Float3 };
		}
		else
		{
			return false;
		}

		return true;
	}

	static bool TryResolveMaterialDomain(const FString& InValue, EMaterialDomain& OutDomain)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveMaterialDomain(InValue, OutDomain);
	}

	static bool TryResolveBlendMode(const FString& InValue, EBlendMode& OutBlendMode)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveBlendMode(InValue, OutBlendMode);
	}

	static bool TryResolveShadingModel(const FString& InValue, EMaterialShadingModel& OutShadingModel)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveShadingModel(InValue, OutShadingModel);
	}

	bool TryResolveCustomOutputType(const FString& InTypeName, ECustomMaterialOutputType& OutOutputType)
	{
		FString TypeName = InTypeName;
		TypeName.TrimStartAndEndInline();
		TypeName.ToLowerInline();
		TypeName.ReplaceInline(TEXT(" "), TEXT(""));

		if (TypeName == TEXT("float")
			|| TypeName == TEXT("float1")
			|| TypeName == TEXT("half")
			|| TypeName == TEXT("half1")
			|| TypeName == TEXT("int")
			|| TypeName == TEXT("uint")
			|| TypeName == TEXT("bool"))
		{
			OutOutputType = CMOT_Float1;
			return true;
		}
		if (TypeName == TEXT("float2")
			|| TypeName == TEXT("half2")
			|| TypeName == TEXT("vec2")
			|| TypeName == TEXT("ivec2")
			|| TypeName == TEXT("uvec2")
			|| TypeName == TEXT("bvec2")
			|| TypeName == TEXT("int2")
			|| TypeName == TEXT("uint2")
			|| TypeName == TEXT("bool2"))
		{
			OutOutputType = CMOT_Float2;
			return true;
		}
		if (TypeName == TEXT("float3")
			|| TypeName == TEXT("half3")
			|| TypeName == TEXT("vec3")
			|| TypeName == TEXT("ivec3")
			|| TypeName == TEXT("uvec3")
			|| TypeName == TEXT("bvec3")
			|| TypeName == TEXT("int3")
			|| TypeName == TEXT("uint3")
			|| TypeName == TEXT("bool3"))
		{
			OutOutputType = CMOT_Float3;
			return true;
		}
		if (TypeName == TEXT("float4")
			|| TypeName == TEXT("half4")
			|| TypeName == TEXT("vec4")
			|| TypeName == TEXT("ivec4")
			|| TypeName == TEXT("uvec4")
			|| TypeName == TEXT("bvec4")
			|| TypeName == TEXT("int4")
			|| TypeName == TEXT("uint4")
			|| TypeName == TEXT("bool4"))
		{
			OutOutputType = CMOT_Float4;
			return true;
		}
		if (TypeName == TEXT("materialattributes"))
		{
			OutOutputType = CMOT_MaterialAttributes;
			return true;
		}

		return false;
	}

	bool ParseScalarLiteral(const FString& InText, double& OutValue)
	{
		const FString Candidate = InText.TrimStartAndEnd();
		return LexTryParseString(OutValue, *Candidate);
	}

	bool ParseBooleanLiteral(const FString& InText, bool& OutValue)
	{
		const FString Candidate = InText.TrimStartAndEnd();
		if (Candidate.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue = true;
			return true;
		}
		if (Candidate.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue = false;
			return true;
		}
		return false;
	}

	bool ParseIntegerLiteral(const FString& InText, int32& OutValue)
	{
		const FString Candidate = InText.TrimStartAndEnd();
		return LexTryParseString(OutValue, *Candidate);
	}

	static bool ParseVectorLiteral(const FString& InText, TArray<double>& OutValues)
	{
		OutValues.Reset();

		FString Candidate = InText.TrimStartAndEnd();
		const int32 OpenParenIndex = Candidate.Find(TEXT("("));
		const int32 CloseParenIndex = Candidate.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
		{
			return false;
		}

		const FString ValueBlock = Candidate.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
		TArray<FString> Parts;
		ValueBlock.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.IsEmpty())
		{
			return false;
		}

		for (const FString& Part : Parts)
		{
			double ParsedValue = 0.0;
			if (!LexTryParseString(ParsedValue, *Part.TrimStartAndEnd()))
			{
				return false;
			}

			OutValues.Add(ParsedValue);
		}

		return true;
	}

	static bool TryGetUEBuiltinArgument(const FTextShaderPropertyDefinition& Property, const TCHAR* Key, FString& OutValue)
	{
		if (const FString* Value = Property.UEBuiltinArguments.Find(UE::DreamShader::NormalizeSettingKey(Key)))
		{
			OutValue = *Value;
			return true;
		}

		return false;
	}

	static bool ValidateUEBuiltinArgumentNames(
		const FTextShaderPropertyDefinition& Property,
		TConstArrayView<const TCHAR*> AllowedArgumentNames,
		FString& OutError)
	{
		for (const TPair<FString, FString>& Argument : Property.UEBuiltinArguments)
		{
			bool bKnownArgument = false;
			for (const TCHAR* AllowedName : AllowedArgumentNames)
			{
				if (Argument.Key == UE::DreamShader::NormalizeSettingKey(AllowedName))
				{
					bKnownArgument = true;
					break;
				}
			}

			if (!bKnownArgument)
			{
				OutError = FString::Printf(
					TEXT("UE.%s for property '%s' does not support argument '%s'."),
					*Property.UEBuiltinFunctionName,
					*Property.Name,
					*Argument.Key);
				return false;
			}
		}

		return true;
	}

	static bool TryResolveWorldPositionShaderOffset(const FString& InValue, EWorldPositionIncludedOffsets& OutValue)
	{
		FString Value = InValue;
		Value.TrimStartAndEndInline();
		Value.ToLowerInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));

		if (Value == TEXT("default") || Value == TEXT("includingshaderoffsets") || Value == TEXT("absolute"))
		{
			OutValue = WPT_Default;
			return true;
		}

		if (Value == TEXT("excludeallshaderoffsets") || Value == TEXT("excludingallshaderoffsets") || Value == TEXT("nooffsets"))
		{
			OutValue = WPT_ExcludeAllShaderOffsets;
			return true;
		}

		if (Value == TEXT("camerarelative"))
		{
			OutValue = WPT_CameraRelative;
			return true;
		}

		if (Value == TEXT("camerarelativenooffsets") || Value == TEXT("camerarelativeexcludeoffsets"))
		{
			OutValue = WPT_CameraRelativeNoOffsets;
			return true;
		}

		return false;
	}

	static bool TryResolvePositionOrigin(const FString& InValue, EPositionOrigin& OutValue)
	{
		FString Value = InValue;
		Value.TrimStartAndEndInline();
		Value.ToLowerInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));

		if (Value == TEXT("absolute") || Value == TEXT("world"))
		{
			OutValue = EPositionOrigin::Absolute;
			return true;
		}

		if (Value == TEXT("camerarelative"))
		{
			OutValue = EPositionOrigin::CameraRelative;
			return true;
		}

		return false;
	}

	static bool TryResolvePropertyReference(
		const FString& InReferenceName,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		UMaterialExpression*& OutExpression)
	{
		const FString ReferenceName = InReferenceName.TrimStartAndEnd();
		if (ReferenceName.IsEmpty())
		{
			return false;
		}

		if (UMaterialExpression* const* ExactMatch = AvailableExpressions.Find(ReferenceName))
		{
			OutExpression = *ExactMatch;
			return true;
		}

		for (const TPair<FString, UMaterialExpression*>& Pair : AvailableExpressions)
		{
			if (Pair.Key.Equals(ReferenceName, ESearchCase::IgnoreCase))
			{
				OutExpression = Pair.Value;
				return true;
			}
		}

		return false;
	}

	UMaterialExpression* CreateScalarLiteralExpression(UMaterial* Material, const double Value, const int32 PositionY)
	{
		auto* Expression = Cast<UMaterialExpressionConstant>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->R = static_cast<float>(Value);
		}
		return Expression;
	}

	static UMaterialExpression* CreateVectorLiteralExpression(
		UMaterial* Material,
		const TArray<double>& Components,
		const int32 ExpectedComponentCount,
		const int32 PositionY)
	{
		if (ExpectedComponentCount == 2)
		{
			auto* Expression = Cast<UMaterialExpressionConstant2Vector>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant2Vector::StaticClass(), -1120, PositionY));
			if (Expression)
			{
				Expression->R = static_cast<float>(Components[0]);
				Expression->G = static_cast<float>(Components[1]);
			}
			return Expression;
		}

		if (ExpectedComponentCount == 3)
		{
			auto* Expression = Cast<UMaterialExpressionConstant3Vector>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), -1120, PositionY));
			if (Expression)
			{
				Expression->Constant = FLinearColor(
					static_cast<float>(Components[0]),
					static_cast<float>(Components[1]),
					static_cast<float>(Components[2]),
					1.0f);
			}
			return Expression;
		}

		auto* Expression = Cast<UMaterialExpressionConstant4Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant4Vector::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->Constant = FLinearColor(
				static_cast<float>(Components[0]),
				static_cast<float>(Components[1]),
				static_cast<float>(Components[2]),
				static_cast<float>(Components[3]));
		}
		return Expression;
	}

	static UMaterialExpression* CreateLiteralExpression(
		UMaterial* Material,
		const FString& InValueText,
		const int32 ExpectedComponentCount,
		const int32 PositionY,
		FString& OutError)
	{
		if (ExpectedComponentCount == 1)
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(InValueText, ParsedValue))
			{
				OutError = FString::Printf(TEXT("Expected a scalar literal but got '%s'."), *InValueText);
				return nullptr;
			}

			UMaterialExpression* Expression = CreateScalarLiteralExpression(Material, ParsedValue, PositionY);
			if (!Expression)
			{
				OutError = TEXT("Failed to create a scalar constant expression.");
			}
			return Expression;
		}

		TArray<double> Components;
		if (!ParseVectorLiteral(InValueText, Components))
		{
			OutError = FString::Printf(TEXT("Expected a float%d-style literal like '(...)' but got '%s'."), ExpectedComponentCount, *InValueText);
			return nullptr;
		}

		if (Components.Num() != ExpectedComponentCount)
		{
			OutError = FString::Printf(
				TEXT("Expected %d components but got %d in literal '%s'."),
				ExpectedComponentCount,
				Components.Num(),
				*InValueText);
			return nullptr;
		}

		UMaterialExpression* Expression = CreateVectorLiteralExpression(Material, Components, ExpectedComponentCount, PositionY);
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a float%d constant expression."), ExpectedComponentCount);
		}
		return Expression;
	}

	static bool ResolveExpressionInputValue(
		UMaterial* Material,
		const FString& InValueText,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 ExpectedComponentCount,
		const int32 PositionY,
		UMaterialExpression*& OutExpression,
		FString& OutError)
	{
		if (TryResolvePropertyReference(InValueText, AvailableExpressions, OutExpression))
		{
			return true;
		}

		OutExpression = CreateLiteralExpression(Material, InValueText, ExpectedComponentCount, PositionY, OutError);
		if (OutExpression)
		{
			return true;
		}

		OutError = FString::Printf(
			TEXT("%s It must reference a previously declared property or use a compatible literal."),
			*OutError);
		return false;
	}

	UClass* ResolveMaterialExpressionClass(const FString& ClassSpecifier)
	{
		FString Candidate = ClassSpecifier.TrimStartAndEnd();
		if (Candidate.IsEmpty())
		{
			return nullptr;
		}

		if (Candidate.Contains(TEXT("/")) || Candidate.Contains(TEXT(".")))
		{
			if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *Candidate))
			{
				if (LoadedClass->IsChildOf(UMaterialExpression::StaticClass()))
				{
					return LoadedClass;
				}
			}
		}

		TArray<FString> CandidateNames;
		CandidateNames.Add(Candidate);
		if (!Candidate.StartsWith(TEXT("U")))
		{
			CandidateNames.Add(TEXT("U") + Candidate);
		}
		if (!Candidate.StartsWith(TEXT("MaterialExpression")))
		{
			CandidateNames.Add(TEXT("MaterialExpression") + Candidate);
		}
		if (!Candidate.StartsWith(TEXT("UMaterialExpression")))
		{
			CandidateNames.Add(TEXT("UMaterialExpression") + Candidate);
		}

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class || !Class->IsChildOf(UMaterialExpression::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}

			for (const FString& NameOption : CandidateNames)
			{
				if (Class->GetName().Equals(NameOption, ESearchCase::IgnoreCase))
				{
					return Class;
				}
			}
		}

		return nullptr;
	}

	FProperty* FindMaterialExpressionArgumentProperty(UClass* ExpressionClass, const FString& ArgumentName)
	{
		if (!ExpressionClass)
		{
			return nullptr;
		}

		const FString NormalizedArgument = UE::DreamShader::NormalizeSettingKey(ArgumentName);
		for (TFieldIterator<FProperty> It(ExpressionClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (Property && UE::DreamShader::NormalizeSettingKey(Property->GetName()) == NormalizedArgument)
			{
				return Property;
			}
		}

		return nullptr;
	}

	bool IsMaterialExpressionInputProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty
			&& StructProperty->Struct
			&& StructProperty->Struct->GetFName() == NAME_ExpressionInput;
	}

	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, const FString& ValueText, FString& OutError)
	{
		if (!Target || !Property)
		{
			OutError = TEXT("Invalid reflected property target.");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
		const FString TrimmedValue = ValueText.TrimStartAndEnd();

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool bValue = false;
			if (!ParseBooleanLiteral(TrimmedValue, bValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid boolean value for '%s'."), *TrimmedValue, *Property->GetName());
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
				OutError = FString::Printf(TEXT("'%s' is not a valid integer value for '%s'."), *TrimmedValue, *Property->GetName());
				return false;
			}
			IntProperty->SetPropertyValue(ValuePtr, ParsedValue);
			return true;
		}

		if (FUInt32Property* UIntProperty = CastField<FUInt32Property>(Property))
		{
			int32 ParsedValue = 0;
			if (!ParseIntegerLiteral(TrimmedValue, ParsedValue) || ParsedValue < 0)
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid unsigned integer value for '%s'."), *TrimmedValue, *Property->GetName());
				return false;
			}
			UIntProperty->SetPropertyValue(ValuePtr, static_cast<uint32>(ParsedValue));
			return true;
		}

		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(TrimmedValue, ParsedValue))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid numeric value for '%s'."), *TrimmedValue, *Property->GetName());
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
				OutError = FString::Printf(TEXT("'%s' is not a valid numeric value for '%s'."), *TrimmedValue, *Property->GetName());
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

		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				int64 EnumValue = INDEX_NONE;
				for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
				{
					const FString ShortName = Enum->GetNameStringByIndex(Index);
					const FString FullName = Enum->GetNameByIndex(Index).ToString();
					if (UE::DreamShader::NormalizeSettingKey(ShortName) == UE::DreamShader::NormalizeSettingKey(TrimmedValue)
						|| UE::DreamShader::NormalizeSettingKey(FullName) == UE::DreamShader::NormalizeSettingKey(TrimmedValue))
					{
						EnumValue = Enum->GetValueByIndex(Index);
						break;
					}
				}

				if (EnumValue == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid enum value for '%s'."), *TrimmedValue, *Property->GetName());
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
				for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
				{
					const FString ShortName = Enum->GetNameStringByIndex(Index);
					const FString FullName = Enum->GetNameByIndex(Index).ToString();
					if (UE::DreamShader::NormalizeSettingKey(ShortName) == UE::DreamShader::NormalizeSettingKey(TrimmedValue)
						|| UE::DreamShader::NormalizeSettingKey(FullName) == UE::DreamShader::NormalizeSettingKey(TrimmedValue))
					{
						EnumValue = Enum->GetValueByIndex(Index);
						break;
					}
				}

				if (EnumValue == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid enum value for '%s'."), *TrimmedValue, *Property->GetName());
					return false;
				}

				ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
				return true;
			}

			int32 ParsedValue = 0;
			if (!ParseIntegerLiteral(TrimmedValue, ParsedValue) || ParsedValue < 0 || ParsedValue > MAX_uint8)
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid byte value for '%s'."), *TrimmedValue, *Property->GetName());
				return false;
			}

			ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(ParsedValue));
			return true;
		}

		FOutputDeviceNull ImportErrors;
		if (Property->ImportText_Direct(*TrimmedValue, ValuePtr, Target, PPF_None, &ImportErrors) != nullptr)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Property '%s' on '%s' is not a supported literal type yet."), *Property->GetName(), *Target->GetClass()->GetName());
		return false;
	}

	static bool TryGetSettingValue(const FTextShaderDefinition& Definition, const TCHAR* Key, FString& OutValue)
	{
		return Definition.TryGetSetting(Key, OutValue);
	}

	static bool TryGetSettingValue(const FTextShaderDefinition& Definition, const TCHAR* KeyA, const TCHAR* KeyB, FString& OutValue)
	{
		return Definition.TryGetSetting(KeyA, OutValue) || Definition.TryGetSetting(KeyB, OutValue);
	}

	static bool ValidateBooleanSetting(const FTextShaderDefinition& Definition, const TCHAR* Key, FString& OutError)
	{
		if (FString Value; TryGetSettingValue(Definition, Key, Value))
		{
			bool bParsedValue = false;
			if (!ParseBooleanLiteral(Value, bParsedValue))
			{
				OutError = FString::Printf(TEXT("Invalid boolean value '%s' for %s."), *Value, Key);
				return false;
			}
		}

		return true;
	}

	static void ApplyBooleanSetting(UMaterial* Material, const FTextShaderDefinition& Definition, const TCHAR* Key, const TFunctionRef<void(bool)>& Setter)
	{
		if (FString Value; TryGetSettingValue(Definition, Key, Value))
		{
			bool bParsedValue = false;
			verify(ParseBooleanLiteral(Value, bParsedValue));
			Setter(bParsedValue);
		}
	}

	FString EnsureTopLevelReturn(const FString& InHLSL)
	{
		const FString Sanitized = InHLSL.Replace(TEXT("\r\n"), TEXT("\n"));
		if (Sanitized.Contains(TEXT("return")))
		{
			return Sanitized;
		}

		return Sanitized + TEXT("\nreturn 0.0;");
	}

	bool IsTextureFunctionParameterType(const FString& InTypeName)
	{
		return InTypeName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase);
	}

	FString BuildGeneratedFunctionSymbolName(const FTextShaderFunctionDefinition& Function)
	{
		return UE::DreamShader::SanitizeIdentifier(Function.Name);
	}

	static uint32 GetSourcePathHash(const FString& SourceFilePath)
	{
		return FCrc::StrCrc32(*UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
	}

	static FString BuildGeneratedIncludeGuardMacro(const FString& SourceFilePath)
	{
		return FString::Printf(
			TEXT("DREAMSHADER_GENERATED_%s_%08X"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)).ToUpper(),
			GetSourcePathHash(SourceFilePath));
	}

	static FString RewriteDreamShaderFunctionReferences(
		const FString& Source,
		const TMap<FString, FString>& GeneratedFunctionNamesByDreamName)
	{
		FString Result = Source;
		TArray<FString> FunctionNames;
		GeneratedFunctionNamesByDreamName.GetKeys(FunctionNames);
		FunctionNames.Sort([](const FString& Left, const FString& Right)
		{
			return Left.Len() > Right.Len();
		});

		for (const FString& FunctionName : FunctionNames)
		{
			const FString* GeneratedFunctionName = GeneratedFunctionNamesByDreamName.Find(FunctionName);
			if (GeneratedFunctionName && !GeneratedFunctionName->Equals(FunctionName, ESearchCase::CaseSensitive))
			{
				const FString ReplacementName = *GeneratedFunctionName;
				Result.ReplaceInline(*FunctionName, *ReplacementName, ESearchCase::CaseSensitive);
			}
		}

		return Result;
	}

	static bool BuildFunctionIncludeSource(
		const FString& SourceFilePath,
		const FTextShaderDefinition& Definition,
		FString& OutSource,
		FString& OutError)
	{
		OutSource.Reset();
		OutSource += TEXT("// Auto-generated by DreamShader.\n");
		OutSource += TEXT("// Changes will be overwritten the next time the source file is saved.\n\n");

		const FString IncludeGuard = BuildGeneratedIncludeGuardMacro(SourceFilePath);
		OutSource += FString::Printf(TEXT("#ifndef %s\n#define %s\n\n"), *IncludeGuard, *IncludeGuard);

		TSet<FString> SeenFunctionNames;
		TSet<FString> SeenGeneratedFunctionNames;
		TMap<FString, FString> GeneratedFunctionNamesByDreamName;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			GeneratedFunctionNamesByDreamName.Add(Function.Name, BuildGeneratedFunctionSymbolName(Function));
		}

		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			const FString NormalizedFunctionName = UE::DreamShader::NormalizeSettingKey(Function.Name);
			if (SeenFunctionNames.Contains(NormalizedFunctionName))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' is declared more than once."), *Function.Name);
				return false;
			}
			SeenFunctionNames.Add(NormalizedFunctionName);

			const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(Function);
			const FString NormalizedGeneratedFunctionName = UE::DreamShader::NormalizeSettingKey(GeneratedFunctionName);
			if (SeenGeneratedFunctionNames.Contains(NormalizedGeneratedFunctionName))
			{
				OutError = FString::Printf(
					TEXT("DreamShader Function '%s' collides with another generated helper symbol '%s'. Rename the Function or Namespace."),
					*Function.Name,
					*GeneratedFunctionName);
				return false;
			}
			SeenGeneratedFunctionNames.Add(NormalizedGeneratedFunctionName);

			const FString ReturnType = Function.Results.IsEmpty() ? TEXT("void") : Function.Results[0].Type;

			TArray<FString> Parameters;
			for (const FTextShaderFunctionParameter& Input : Function.Inputs)
			{
				Parameters.Add(FString::Printf(TEXT("%s %s"), *Input.Type, *Input.Name));
				if (IsTextureFunctionParameterType(Input.Type))
				{
					Parameters.Add(FString::Printf(TEXT("SamplerState %sSampler"), *Input.Name));
				}
			}
			for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
			{
				const FTextShaderFunctionParameter& Output = Function.Results[ResultIndex];
				Parameters.Add(FString::Printf(TEXT("out %s %s"), *Output.Type, *Output.Name));
			}

			OutSource += FString::Printf(TEXT("%s %s(%s)\n{\n"), *ReturnType, *GeneratedFunctionName, *FString::Join(Parameters, TEXT(", ")));

			if (!Function.Results.IsEmpty())
			{
				OutSource += FString::Printf(TEXT("\t%s %s = (%s)0;\n"), *Function.Results[0].Type, *Function.Results[0].Name, *Function.Results[0].Type);
			}

			for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
			{
				const FTextShaderFunctionParameter& Output = Function.Results[ResultIndex];
				OutSource += FString::Printf(TEXT("\t%s = (%s)0;\n"), *Output.Name, *Output.Type);
			}

			const FString RewrittenFunctionHLSL = RewriteDreamShaderFunctionReferences(Function.HLSL, GeneratedFunctionNamesByDreamName);
			OutSource += RewrittenFunctionHLSL;
			if (!RewrittenFunctionHLSL.EndsWith(TEXT("\n")))
			{
				OutSource += TEXT("\n");
			}

			if (!Function.Results.IsEmpty())
			{
				OutSource += FString::Printf(TEXT("\treturn %s;\n"), *Function.Results[0].Name);
			}

			OutSource += TEXT("}\n\n");
		}

		OutSource += FString::Printf(TEXT("#endif // %s\n"), *IncludeGuard);
		return true;
	}

	FString BuildGeneratedIncludeVirtualPath(const FString& SourceFilePath)
	{
		const FString BaseName = FString::Printf(
			TEXT("%s_%08x.ush"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)),
			GetSourcePathHash(SourceFilePath));
		return FString::Printf(TEXT("%s/%s"), *UE::DreamShader::GetGeneratedShaderVirtualDirectory(), *BaseName);
	}

	static FString BuildGeneratedIncludeRealPath(const FString& SourceFilePath)
	{
		const FString BaseName = FString::Printf(
			TEXT("%s_%08x.ush"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)),
			GetSourcePathHash(SourceFilePath));
		return FPaths::Combine(UE::DreamShader::GetGeneratedShaderDirectory(), BaseName);
	}

	bool WriteGeneratedInclude(const FString& SourceFilePath, const FTextShaderDefinition& Definition, FString& OutError)
	{
		const FString IncludePath = BuildGeneratedIncludeRealPath(SourceFilePath);
		FString IncludeSource;
		if (!BuildFunctionIncludeSource(SourceFilePath, Definition, IncludeSource, OutError))
		{
			return false;
		}

		if (!FFileHelper::SaveStringToFile(IncludeSource, *IncludePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("Failed to write generated helper include '%s'."), *IncludePath);
			return false;
		}
		return true;
	}

	void ClearMaterialExpressions(UMaterial* Material)
	{
		if (!Material)
		{
			return;
		}

		for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
		{
			if (FExpressionInput* ExpressionInput = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(MaterialPropertyIndex)))
			{
				ExpressionInput->Expression = nullptr;
			}
		}

		int32 SafetyCounter = 0;
		while (!Material->GetExpressions().IsEmpty() && SafetyCounter < 64)
		{
			TArray<UMaterialExpression*> ExpressionSnapshot;
			ExpressionSnapshot.Reserve(Material->GetExpressions().Num());
			for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
			{
				if (Expression)
				{
					ExpressionSnapshot.Add(Expression.Get());
				}
			}

			if (ExpressionSnapshot.IsEmpty())
			{
				break;
			}

			for (UMaterialExpression* Expression : ExpressionSnapshot)
			{
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
			}

			++SafetyCounter;
		}
	}

	void ClearMaterialFunctionExpressions(UMaterialFunction* MaterialFunction)
	{
		if (!MaterialFunction)
		{
			return;
		}

		int32 SafetyCounter = 0;
		while (!MaterialFunction->GetExpressions().IsEmpty() && SafetyCounter < 64)
		{
			TArray<UMaterialExpression*> ExpressionSnapshot;
			ExpressionSnapshot.Reserve(MaterialFunction->GetExpressions().Num());
			for (const TObjectPtr<UMaterialExpression>& Expression : MaterialFunction->GetExpressions())
			{
				if (Expression)
				{
					ExpressionSnapshot.Add(Expression.Get());
				}
			}

			if (ExpressionSnapshot.IsEmpty())
			{
				break;
			}

			for (UMaterialExpression* Expression : ExpressionSnapshot)
			{
				UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MaterialFunction, Expression);
			}

			++SafetyCounter;
		}
	}

	void ResetMaterialToDefaults(UMaterial* Material)
	{
		check(Material);

		Material->BlendMode = BLEND_Opaque;
		Material->MaterialDomain = MD_Surface;
		Material->SetShadingModel(MSM_DefaultLit);
		Material->TwoSided = false;
		Material->OpacityMaskClipValue = 0.3333f;
		Material->Wireframe = false;
		Material->DitheredLODTransition = false;
		Material->DitherOpacityMask = false;
		Material->bAllowNegativeEmissiveColor = false;
		Material->bCastDynamicShadowAsMasked = false;
		Material->bEnableResponsiveAA = false;
		Material->bScreenSpaceReflections = false;
		Material->bContactShadows = false;
		Material->bDisableDepthTest = false;
		Material->bOutputTranslucentVelocity = false;
		Material->bTangentSpaceNormal = true;
		Material->bFullyRough = false;
		Material->bIsSky = false;
		Material->bIsThinSurface = false;
		Material->bHasPixelAnimation = false;
		Material->NumCustomizedUVs = 0;
	}

	bool ValidateSettings(const FTextShaderDefinition& Definition, FString& OutError)
	{
		if (FString BlendModeValue; TryGetSettingValue(Definition, TEXT("BlendMode"), TEXT("RenderType"), BlendModeValue))
		{
			EBlendMode BlendMode = BLEND_MAX;
			if (!TryResolveBlendMode(BlendModeValue, BlendMode))
			{
				OutError = FString::Printf(TEXT("Unsupported BlendMode/RenderType '%s'."), *BlendModeValue);
				return false;
			}
		}

		if (FString ShadingModelValue; Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue))
		{
			EMaterialShadingModel ShadingModel = MSM_MAX;
			if (!TryResolveShadingModel(ShadingModelValue, ShadingModel))
			{
				OutError = FString::Printf(TEXT("Unsupported ShadingModel '%s'."), *ShadingModelValue);
				return false;
			}
		}

		if (FString MaterialDomainValue; TryGetSettingValue(Definition, TEXT("MaterialDomain"), TEXT("Domain"), MaterialDomainValue))
		{
			EMaterialDomain Domain = MD_Surface;
			if (!TryResolveMaterialDomain(MaterialDomainValue, Domain))
			{
				OutError = FString::Printf(TEXT("Unsupported MaterialDomain '%s'."), *MaterialDomainValue);
				return false;
			}
		}

		if (!ValidateBooleanSetting(Definition, TEXT("TwoSided"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("Wireframe"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("DitheredLODTransition"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("DitherOpacityMask"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("AllowNegativeEmissiveColor"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("CastDynamicShadowAsMasked"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("ResponsiveAA"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("ScreenSpaceReflections"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("ContactShadows"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("DisableDepthTest"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("OutputTranslucentVelocity"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("TangentSpaceNormal"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("FullyRough"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("IsSky"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("ThinSurface"), OutError)
			|| !ValidateBooleanSetting(Definition, TEXT("HasPixelAnimation"), OutError))
		{
			return false;
		}

		if (FString ClipValue; Definition.TryGetSetting(TEXT("OpacityMaskClipValue"), ClipValue))
		{
			double ParsedClipValue = 0.3333;
			if (!ParseScalarLiteral(ClipValue, ParsedClipValue))
			{
				OutError = FString::Printf(TEXT("Invalid scalar value '%s' for OpacityMaskClipValue."), *ClipValue);
				return false;
			}
		}

		if (FString CustomizedUVValue; Definition.TryGetSetting(TEXT("NumCustomizedUVs"), CustomizedUVValue))
		{
			int32 ParsedCustomizedUVs = 0;
			if (!ParseIntegerLiteral(CustomizedUVValue, ParsedCustomizedUVs) || ParsedCustomizedUVs < 0)
			{
				OutError = FString::Printf(TEXT("Invalid integer value '%s' for NumCustomizedUVs."), *CustomizedUVValue);
				return false;
			}
		}

		return true;
	}

	bool ApplySettings(UMaterial* Material, const FTextShaderDefinition& Definition, FString& OutError)
	{
		check(Material);

		if (!ValidateSettings(Definition, OutError))
		{
			return false;
		}

		if (FString BlendModeValue; TryGetSettingValue(Definition, TEXT("BlendMode"), TEXT("RenderType"), BlendModeValue))
		{
			EBlendMode BlendMode = BLEND_Opaque;
			verify(TryResolveBlendMode(BlendModeValue, BlendMode));
			Material->BlendMode = BlendMode;
		}

		if (FString ShadingModelValue; Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue))
		{
			EMaterialShadingModel ShadingModel = MSM_DefaultLit;
			verify(TryResolveShadingModel(ShadingModelValue, ShadingModel));
			Material->SetShadingModel(ShadingModel);
		}

		if (FString MaterialDomainValue; TryGetSettingValue(Definition, TEXT("MaterialDomain"), TEXT("Domain"), MaterialDomainValue))
		{
			EMaterialDomain Domain = MD_Surface;
			verify(TryResolveMaterialDomain(MaterialDomainValue, Domain));
			Material->MaterialDomain = Domain;
		}

		ApplyBooleanSetting(Material, Definition, TEXT("TwoSided"), [Material](const bool bValue) { Material->TwoSided = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("Wireframe"), [Material](const bool bValue) { Material->Wireframe = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("DitheredLODTransition"), [Material](const bool bValue) { Material->DitheredLODTransition = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("DitherOpacityMask"), [Material](const bool bValue) { Material->DitherOpacityMask = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("AllowNegativeEmissiveColor"), [Material](const bool bValue) { Material->bAllowNegativeEmissiveColor = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("CastDynamicShadowAsMasked"), [Material](const bool bValue) { Material->bCastDynamicShadowAsMasked = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("ResponsiveAA"), [Material](const bool bValue) { Material->bEnableResponsiveAA = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("ScreenSpaceReflections"), [Material](const bool bValue) { Material->bScreenSpaceReflections = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("ContactShadows"), [Material](const bool bValue) { Material->bContactShadows = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("DisableDepthTest"), [Material](const bool bValue) { Material->bDisableDepthTest = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("OutputTranslucentVelocity"), [Material](const bool bValue) { Material->bOutputTranslucentVelocity = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("TangentSpaceNormal"), [Material](const bool bValue) { Material->bTangentSpaceNormal = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("FullyRough"), [Material](const bool bValue) { Material->bFullyRough = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("IsSky"), [Material](const bool bValue) { Material->bIsSky = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("ThinSurface"), [Material](const bool bValue) { Material->bIsThinSurface = bValue; });
		ApplyBooleanSetting(Material, Definition, TEXT("HasPixelAnimation"), [Material](const bool bValue) { Material->bHasPixelAnimation = bValue; });

		if (FString ClipValue; Definition.TryGetSetting(TEXT("OpacityMaskClipValue"), ClipValue))
		{
			double ParsedClipValue = 0.3333;
			ParseScalarLiteral(ClipValue, ParsedClipValue);
			Material->OpacityMaskClipValue = static_cast<float>(ParsedClipValue);
		}

		if (FString CustomizedUVValue; Definition.TryGetSetting(TEXT("NumCustomizedUVs"), CustomizedUVValue))
		{
			int32 ParsedCustomizedUVs = 0;
			verify(ParseIntegerLiteral(CustomizedUVValue, ParsedCustomizedUVs));
			Material->NumCustomizedUVs = ParsedCustomizedUVs;
		}

		return true;
	}

	static const TCHAR* GetTextureTypeLabel(const ETextShaderTextureType TextureType)
	{
		switch (TextureType)
		{
		case ETextShaderTextureType::TextureCube:
			return TEXT("TextureCube");
		case ETextShaderTextureType::Texture2DArray:
			return TEXT("Texture2DArray");
		case ETextShaderTextureType::Texture2D:
		default:
			return TEXT("Texture2D");
		}
	}

	static UMaterialExpression* CreateParameterExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		int32 PositionY,
		FString& OutError)
	{
		check(Material);

		if (Property.Type == ETextShaderPropertyType::Scalar)
		{
			auto* Expression = Cast<UMaterialExpressionScalarParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create a scalar parameter node for property '%s'."), *Property.Name);
				return nullptr;
			}

			Expression->ParameterName = FName(*Property.Name);
			if (Property.bHasDefaultValue)
			{
				Expression->DefaultValue = static_cast<float>(Property.ScalarDefaultValue);
			}
			return Expression;
		}

		if (Property.Type == ETextShaderPropertyType::Vector)
		{
			auto* ParameterExpression = Cast<UMaterialExpressionVectorParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -800, PositionY));
			if (!ParameterExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create a vector parameter node for property '%s'."), *Property.Name);
				return nullptr;
			}

			ParameterExpression->ParameterName = FName(*Property.Name);
			if (Property.bHasDefaultValue)
			{
				ParameterExpression->DefaultValue = Property.VectorDefaultValue;
			}

			if (Property.ComponentCount >= 4)
			{
				return ParameterExpression;
			}

			auto* MaskExpression = Cast<UMaterialExpressionComponentMask>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionComponentMask::StaticClass(), -560, PositionY));
			if (!MaskExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create a component mask for property '%s'."), *Property.Name);
				return nullptr;
			}

			MaskExpression->Input.Expression = ParameterExpression;
			MaskExpression->R = Property.ComponentCount >= 1;
			MaskExpression->G = Property.ComponentCount >= 2;
			MaskExpression->B = Property.ComponentCount >= 3;
			MaskExpression->A = false;
			return MaskExpression;
		}

		auto* Expression = Cast<UMaterialExpressionTextureObjectParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureObjectParameter::StaticClass(), -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a texture parameter node for property '%s'."), *Property.Name);
			return nullptr;
		}

		Expression->ParameterName = FName(*Property.Name);
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

			if (Property.TextureType == ETextShaderTextureType::TextureCube)
			{
				if (!Cast<UTextureCube>(DefaultTexture))
				{
					OutError = FString::Printf(
						TEXT("Texture property '%s' expects %s but '%s' is a '%s'."),
						*Property.Name,
						GetTextureTypeLabel(Property.TextureType),
						*Property.TextureDefaultObjectPath,
						*DefaultTexture->GetClass()->GetName());
					return nullptr;
				}
			}
			else if (Property.TextureType == ETextShaderTextureType::Texture2DArray)
			{
				if (!Cast<UTexture2DArray>(DefaultTexture))
				{
					OutError = FString::Printf(
						TEXT("Texture property '%s' expects %s but '%s' is a '%s'."),
						*Property.Name,
						GetTextureTypeLabel(Property.TextureType),
						*Property.TextureDefaultObjectPath,
						*DefaultTexture->GetClass()->GetName());
					return nullptr;
				}
			}
			else if (Cast<UTextureCube>(DefaultTexture) || Cast<UTexture2DArray>(DefaultTexture))
			{
				OutError = FString::Printf(
					TEXT("Texture property '%s' expects %s but '%s' is a '%s'."),
					*Property.Name,
					GetTextureTypeLabel(Property.TextureType),
					*Property.TextureDefaultObjectPath,
					*DefaultTexture->GetClass()->GetName());
				return nullptr;
			}

			Expression->Texture = DefaultTexture;
			Expression->AutoSetSampleType();
		}
		else
		{
			Expression->SetDefaultTexture();
		}
		return Expression;
	}

	static bool ResolveFlexibleExpressionInputValue(
		UMaterial* Material,
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
			OutExpression = CreateScalarLiteralExpression(Material, ScalarValue, PositionY);
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

			OutExpression = CreateVectorLiteralExpression(Material, Components, ComponentCount, PositionY);
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
			UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("UE.%s for property '%s': failed to create '%s'."),
				*Property.UEBuiltinFunctionName,
				*Property.Name,
				*ExpressionClass->GetName());
			return nullptr;
		}

		for (const TPair<FString, FString>& Argument : Property.UEBuiltinArguments)
		{
			if (Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("Class"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("OutputType"))
				|| Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("ResultType")))
			{
				continue;
			}

			FProperty* BoundProperty = FindMaterialExpressionArgumentProperty(ExpressionClass, Argument.Key);
			if (!BoundProperty)
			{
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
				if (!ResolveFlexibleExpressionInputValue(Material, Argument.Value, AvailableExpressions, PositionY - 80, InputExpression, InputError))
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
				if (!SetMaterialExpressionLiteralProperty(Expression, BoundProperty, Argument.Value, LiteralError))
				{
					OutError = FString::Printf(TEXT("UE.%s for property '%s': %s"),
						*Property.UEBuiltinFunctionName,
						*Property.Name,
						*LiteralError);
					return nullptr;
				}
			}
		}

		return Expression;
	}

	static UMaterialExpression* CreateUEBuiltinExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		check(Material);

		const auto MakeError = [&Property](const FString& Message)
		{
			return FString::Printf(TEXT("UE.%s for property '%s': %s"), *Property.UEBuiltinFunctionName, *Property.Name, *Message);
		};

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
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureCoordinate::StaticClass(), -800, PositionY));
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
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTime::StaticClass(), -800, PositionY));
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
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionPanner::StaticClass(), -800, PositionY));
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
					if (!ResolveExpressionInputValue(Material, CoordinateValue, AvailableExpressions, 2, PositionY - 80, CoordinateExpression, InputError))
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
				if (!ResolveExpressionInputValue(Material, TimeValue, AvailableExpressions, 1, PositionY - 40, TimeExpression, InputError))
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
				if (!ResolveExpressionInputValue(Material, SpeedValue, AvailableExpressions, 2, PositionY + 40, SpeedExpression, InputError))
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
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionWorldPosition::StaticClass(), -800, PositionY));
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

			auto* Expression = Cast<UMaterialExpressionObjectPositionWS>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionObjectPositionWS::StaticClass(), -800, PositionY));
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
					UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCameraVectorWS::StaticClass(), -800, PositionY));
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

			UMaterialExpression* Expression =
				Cast<UMaterialExpressionScreenPosition>(
					UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScreenPosition::StaticClass(), -800, PositionY));
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
					UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVertexColor::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native VertexColor node."));
			}
			return Expression;
		}

		if (Property.UEBuiltinArguments.Contains(UE::DreamShader::NormalizeSettingKey(TEXT("OutputType")))
			|| Property.UEBuiltinArguments.Contains(UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))))
		{
			return CreateGenericUEExpression(Material, Property, AvailableExpressions, PositionY, OutError);
		}

		OutError = MakeError(TEXT("This builtin is not implemented by the material generator yet. For generic MaterialExpression support, add OutputType=\"float1/2/3/4/Texture2D\"."));
		return nullptr;
	}

	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 PositionY,
		FString& OutError)
	{
		if (Property.Source == ETextShaderPropertySource::UEBuiltin)
		{
			return CreateUEBuiltinExpression(Material, Property, AvailableExpressions, PositionY, OutError);
		}

		UMaterialExpression* Expression = CreateParameterExpression(Material, Property, PositionY, OutError);
		if (!Expression)
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Failed to create a parameter node for property '%s'."), *Property.Name);
			}
		}
		return Expression;
	}

	bool TryGetComponentCountForOutputType(const ECustomMaterialOutputType OutputType, int32& OutComponentCount)
	{
		switch (OutputType)
		{
		case CMOT_Float1:
			OutComponentCount = 1;
			return true;
		case CMOT_Float2:
			OutComponentCount = 2;
			return true;
		case CMOT_Float3:
			OutComponentCount = 3;
			return true;
		case CMOT_Float4:
			OutComponentCount = 4;
			return true;
		default:
			return false;
		}
	}

	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture)
	{
		bOutIsTexture = false;

		ECustomMaterialOutputType OutputType = CMOT_Float1;
		if (TryResolveCustomOutputType(InTypeName, OutputType) && TryGetComponentCountForOutputType(OutputType, OutComponentCount))
		{
			return true;
		}

		if (InTypeName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("SamplerState"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 0;
			bOutIsTexture = true;
			return true;
		}

		return false;
	}

	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture)
	{
		for (const FTextShaderVariableDeclaration& Declaration : Definition.OutputDeclarations)
		{
			if (Declaration.Name.Equals(VariableName, ESearchCase::IgnoreCase))
			{
				return TryResolveCodeDeclaredType(Declaration.Type, OutComponentCount, bOutIsTexture);
			}
		}

		return false;
	}

	bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FString& OutError)
	{
		FString Normalized = AssetName;
		Normalized.TrimStartAndEndInline();
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Normalized.StartsWith(TEXT("/")))
		{
			Normalized.RightChopInline(1, EAllowShrinking::No);
		}
		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, EAllowShrinking::No);
		}

		if (Normalized.IsEmpty())
		{
			OutError = TEXT("DreamShader asset name must resolve to a non-empty asset path.");
			return false;
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("DreamShader asset name must resolve to a non-empty asset path.");
			return false;
		}

		OutAssetLeafName = ObjectTools::SanitizeObjectName(Segments.Last());
		if (OutAssetLeafName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("DreamShader asset name '%s' produced an invalid asset name."), *AssetName);
			return false;
		}

		FString PackagePath = TEXT("/Game");
		for (int32 Index = 0; Index < Segments.Num() - 1; ++Index)
		{
			const FString FolderName = ObjectTools::SanitizeObjectName(Segments[Index]);
			if (FolderName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("DreamShader asset name '%s' contains an invalid folder segment."), *AssetName);
				return false;
			}

			PackagePath += TEXT("/");
			PackagePath += FolderName;
		}

		OutPackageName = PackagePath + TEXT("/") + OutAssetLeafName;
		OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *OutAssetLeafName);
		return true;
	}

	bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FString& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		if (UObject* ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			OutMaterial = Cast<UMaterial>(ExistingObject);
			if (!OutMaterial)
			{
				OutError = FString::Printf(TEXT("Asset '%s' already exists and is not a Material."), *ObjectPath);
				return false;
			}

			return true;
		}

		UPackage* MaterialPackage = CreatePackage(*PackageName);
		if (!MaterialPackage)
		{
			OutError = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
			return false;
		}

		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		OutMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
			UMaterial::StaticClass(),
			MaterialPackage,
			FName(*AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));

		if (!OutMaterial)
		{
			OutError = FString::Printf(TEXT("Failed to create material '%s'."), *ObjectPath);
			return false;
		}

		FAssetRegistryModule::AssetCreated(OutMaterial);
		return true;
	}

	bool CreateOrReuseMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FString& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		if (UObject* ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			OutFunction = Cast<UMaterialFunction>(ExistingObject);
			if (!OutFunction)
			{
				OutError = FString::Printf(TEXT("Asset '%s' already exists and is not a MaterialFunction."), *ObjectPath);
				return false;
			}

			return true;
		}

		UPackage* FunctionPackage = CreatePackage(*PackageName);
		if (!FunctionPackage)
		{
			OutError = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
			return false;
		}

		UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
		OutFunction = Cast<UMaterialFunction>(Factory->FactoryCreateNew(
			UMaterialFunction::StaticClass(),
			FunctionPackage,
			FName(*AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));

		if (!OutFunction)
		{
			OutError = FString::Printf(TEXT("Failed to create material function '%s'."), *ObjectPath);
			return false;
		}

		FAssetRegistryModule::AssetCreated(OutFunction);
		return true;
	}

	bool TryResolveMaterialFunctionParameterType(
		const FString& InTypeName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		int32& OutFunctionInputTypeValue)
	{
		if (!TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture))
		{
			return false;
		}

		if (bOutIsTexture)
		{
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Texture2D);
			return true;
		}

		switch (OutComponentCount)
		{
		case 1:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Scalar);
			return true;
		case 2:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector2);
			return true;
		case 3:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector3);
			return true;
		case 4:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector4);
			return true;
		default:
			return false;
		}
	}

	bool ValidateOutputs(
		const FTextShaderDefinition& Definition,
		TArray<FResolvedNamedOutput>& OutNamedOutputs,
		bool& bOutUsesReturn,
		ECustomMaterialOutputType& OutReturnType,
		FString& OutError)
	{
		OutNamedOutputs.Reset();
		bOutUsesReturn = false;
		OutReturnType = CMOT_Float1;

		TMap<FString, ECustomMaterialOutputType> DeclaredOutputTypes;
		TMap<FString, FString> DeclaredOutputTypeTexts;
		for (const FTextShaderVariableDeclaration& Declaration : Definition.OutputDeclarations)
		{
			if (Declaration.Name.Equals(TEXT("return"), ESearchCase::IgnoreCase))
			{
				OutError = TEXT("Outputs declarations cannot use the reserved name 'return'.");
				return false;
			}

			ECustomMaterialOutputType DeclaredType = CMOT_Float1;
			if (!TryResolveCustomOutputType(Declaration.Type, DeclaredType))
			{
				OutError = FString::Printf(TEXT("Unsupported output type '%s' for '%s'."), *Declaration.Type, *Declaration.Name);
				return false;
			}

			if (const ECustomMaterialOutputType* ExistingType = DeclaredOutputTypes.Find(Declaration.Name))
			{
				if (*ExistingType != DeclaredType)
				{
					OutError = FString::Printf(TEXT("Output variable '%s' is declared with conflicting types."), *Declaration.Name);
					return false;
				}
			}
			else
			{
				DeclaredOutputTypes.Add(Declaration.Name, DeclaredType);
				DeclaredOutputTypeTexts.Add(Declaration.Name, Declaration.Type);
			}
		}

		TMap<FString, int32> OutputOrder;
		for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
		{
			ECustomMaterialOutputType BindingOutputType = CMOT_Float1;
			bool bHasImplicitTypeFromTarget = false;
			if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
			{
				FResolvedMaterialProperty ResolvedProperty;
				if (!ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty))
				{
					OutError = FString::Printf(TEXT("Unsupported material output '%s'."), *Binding.MaterialProperty);
					return false;
				}

				BindingOutputType = ResolvedProperty.OutputType;
				bHasImplicitTypeFromTarget = true;
			}

			if (Binding.VariableName.Equals(TEXT("return"), ESearchCase::IgnoreCase))
			{
				if (Binding.TargetKind != FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					OutError = TEXT("The reserved output name 'return' can only bind to Base material properties.");
					return false;
				}

				if (!bOutUsesReturn)
				{
					bOutUsesReturn = true;
					OutReturnType = BindingOutputType;
				}
				else if (OutReturnType != BindingOutputType)
				{
					OutError = TEXT("The return value is bound to material properties with incompatible types.");
					return false;
				}

				continue;
			}

			if (const int32* ExistingIndex = OutputOrder.Find(Binding.VariableName))
			{
				if (OutNamedOutputs[*ExistingIndex].OutputType != BindingOutputType && bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(TEXT("Output variable '%s' is bound to incompatible material properties."), *Binding.VariableName);
					return false;
				}
			}
			else
			{
				FResolvedNamedOutput& Output = OutNamedOutputs.AddDefaulted_GetRef();
				Output.Name = Binding.VariableName;
				Output.OutputType = BindingOutputType;

				if (const ECustomMaterialOutputType* DeclaredType = DeclaredOutputTypes.Find(Binding.VariableName))
				{
					if (bHasImplicitTypeFromTarget && *DeclaredType != BindingOutputType)
					{
						OutError = FString::Printf(
							TEXT("Output variable '%s' is declared as '%s' but bound material property '%s' expects a different type."),
							*Binding.VariableName,
							*DeclaredOutputTypeTexts.FindChecked(Binding.VariableName),
							*Binding.MaterialProperty);
						return false;
					}

					Output.OutputType = *DeclaredType;
				}
				else if (!bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(
						TEXT("Output variable '%s' must declare an explicit type before binding to expression target '%s'."),
						*Binding.VariableName,
						*Binding.TargetText);
					return false;
				}

				OutputOrder.Add(Binding.VariableName, OutNamedOutputs.Num() - 1);
			}
		}

		return true;
	}

	FString BuildSourceHash(const FString& SourceText)
	{
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*SourceText));
	}

	static FString GetSourceMetadataValue(UObject* Asset, const TCHAR* Key)
	{
		if (!Asset)
		{
			return FString();
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return FString();
		}

		return Package->GetMetaData().GetValue(Asset, Key);
	}

	bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset || SourceHash.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceFileRaw = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile"));
		if (ExistingSourceFileRaw.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceFile = UE::DreamShader::NormalizeSourceFilePath(ExistingSourceFileRaw);
		const FString ExistingSourceHash = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceHash"));

		return ExistingSourceFile.Equals(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath), ESearchCase::IgnoreCase)
			&& ExistingSourceHash.Equals(SourceHash, ESearchCase::CaseSensitive);
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath)
	{
		ApplySourceMetadata(Asset, SourceFilePath, FString());
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset)
		{
			return;
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return;
		}

		FMetaData& MetaData = Package->GetMetaData();
		MetaData.SetValue(Asset, TEXT("DreamShader.SourceFile"), *UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
		if (!SourceHash.IsEmpty())
		{
			MetaData.SetValue(Asset, TEXT("DreamShader.SourceHash"), *SourceHash);
			MetaData.SetValue(Asset, TEXT("DreamShader.GeneratedAtUtc"), *FDateTime::UtcNow().ToIso8601());
		}
	}

	bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		check(Asset);

		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Asset->GetOutermost());
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			OutError = FString::Printf(TEXT("Generated DreamShader asset '%s' could not be saved."), *Asset->GetPathName());
			return false;
		}

		return true;
	}
}
