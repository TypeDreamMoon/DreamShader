#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"

#include "Misc/Crc.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "Interfaces/IPluginManager.h"
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
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialParameterCollection.h"
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
			OutProperty = { MP_Refraction, CMOT_Float3 };
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
		else if (Matches(TEXT("CustomData0")))
		{
			OutProperty = { MP_CustomData0, CMOT_Float1 };
		}
		else if (Matches(TEXT("CustomData1")))
		{
			OutProperty = { MP_CustomData1, CMOT_Float1 };
		}
		else if (Matches(TEXT("DiffuseColor")))
		{
			OutProperty = { MP_DiffuseColor, CMOT_Float3 };
		}
		else if (Matches(TEXT("SpecularColor")))
		{
			OutProperty = { MP_SpecularColor, CMOT_Float3 };
		}
		else if (Matches(TEXT("SurfaceThickness")))
		{
			OutProperty = { MP_SurfaceThickness, CMOT_Float1 };
		}
		else if (Matches(TEXT("Displacement")))
		{
			OutProperty = { MP_Displacement, CMOT_Float1 };
		}
		else if (Matches(TEXT("CustomizedUV0")) || Matches(TEXT("CustomizedUVs0")))
		{
			OutProperty = { MP_CustomizedUVs0, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV1")) || Matches(TEXT("CustomizedUVs1")))
		{
			OutProperty = { MP_CustomizedUVs1, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV2")) || Matches(TEXT("CustomizedUVs2")))
		{
			OutProperty = { MP_CustomizedUVs2, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV3")) || Matches(TEXT("CustomizedUVs3")))
		{
			OutProperty = { MP_CustomizedUVs3, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV4")) || Matches(TEXT("CustomizedUVs4")))
		{
			OutProperty = { MP_CustomizedUVs4, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV5")) || Matches(TEXT("CustomizedUVs5")))
		{
			OutProperty = { MP_CustomizedUVs5, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV6")) || Matches(TEXT("CustomizedUVs6")))
		{
			OutProperty = { MP_CustomizedUVs6, CMOT_Float2 };
		}
		else if (Matches(TEXT("CustomizedUV7")) || Matches(TEXT("CustomizedUVs7")))
		{
			OutProperty = { MP_CustomizedUVs7, CMOT_Float2 };
		}
		else if (Matches(TEXT("MooaEncodedAttribute0")))
		{
			OutProperty = { MP_MooaEncodedAttribute0, CMOT_Float4 };
		}
		else if (Matches(TEXT("MooaEncodedAttribute1")))
		{
			OutProperty = { MP_MooaEncodedAttribute1, CMOT_Float4 };
		}
		else if (Matches(TEXT("MooaEncodedAttribute2")))
		{
			OutProperty = { MP_MooaEncodedAttribute2, CMOT_Float4 };
		}
		else if (Matches(TEXT("MooaEncodedAttribute3")))
		{
			OutProperty = { MP_MooaEncodedAttribute3, CMOT_Float4 };
		}
		else if (Matches(TEXT("MooaEncodedAttribute4")))
		{
			OutProperty = { MP_MooaEncodedAttribute4, CMOT_Float4 };
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

	struct FResolvedMaterialSettingTarget
	{
		UObject* OwnerObject = nullptr;
		void* ContainerPtr = nullptr;
		FProperty* Property = nullptr;
		int32 ArrayIndex = 0;
	};

	struct FParsedMaterialSettingSegment
	{
		FString Name;
		int32 ArrayIndex = 0;
		bool bHasArrayIndex = false;
	};

	static bool SplitSimpleArgumentList(const FString& InText, TArray<FString>& OutArguments)
	{
		OutArguments.Reset();

		FString Current;
		bool bInString = false;
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Character = InText[Index];
			if (Character == TCHAR('"'))
			{
				bInString = !bInString;
				Current.AppendChar(Character);
				continue;
			}

			if (!bInString && Character == TCHAR(','))
			{
				OutArguments.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}

			Current.AppendChar(Character);
		}

		if (bInString)
		{
			return false;
		}

		OutArguments.Add(Current.TrimStartAndEnd());
		return true;
	}

	static FString TrimMatchingQuotes(const FString& InText)
	{
		FString Result = InText.TrimStartAndEnd();
		if (Result.Len() >= 2 && Result.StartsWith(TEXT("\"")) && Result.EndsWith(TEXT("\"")))
		{
			Result = Result.Mid(1, Result.Len() - 2);
		}
		return Result;
	}

	static bool ResolveContentPluginAssetReferenceRoot(
		const FString& Root,
		const FString& PluginName,
		FString& OutPackagePath,
		FString& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but no enabled plugin with that name was found."), *Root, *PluginName);
			return false;
		}

		if (!Plugin->IsEnabled())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin is not enabled."), *Root, *PluginName);
			return false;
		}

		if (!Plugin->CanContainContent())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin cannot contain content."), *Root, *PluginName);
			return false;
		}

		const FString ContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
		if (!IFileManager::Get().DirectoryExists(*ContentDir))
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but its Content directory does not exist: '%s'."), *Root, *PluginName, *ContentDir);
			return false;
		}

		if (!Plugin->IsMounted())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin content is not mounted."), *Root, *PluginName);
			return false;
		}

		FString MountedAssetPath = Plugin->GetMountedAssetPath();
		MountedAssetPath.TrimStartAndEndInline();
		MountedAssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (MountedAssetPath.EndsWith(TEXT("/")))
		{
			MountedAssetPath.LeftChopInline(1, EAllowShrinking::No);
		}
		if (!MountedAssetPath.StartsWith(TEXT("/")))
		{
			MountedAssetPath = TEXT("/") + MountedAssetPath;
		}
		if (MountedAssetPath.IsEmpty() || MountedAssetPath == TEXT("/"))
		{
			MountedAssetPath = TEXT("/") + Plugin->GetName();
		}

		OutPackagePath = MountedAssetPath;
		return true;
	}

	static bool ResolveAssetReferenceRootPackagePath(
		const FString& Root,
		FString& OutPackagePath,
		FString& OutError)
	{
		FString Normalized = Root;
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

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.");
			return false;
		}

		const FString RootSegment = Segments[0].TrimStartAndEnd();
		int32 FirstFolderSegmentIndex = 1;
		if (RootSegment.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Game");
		}
		else if (RootSegment.Equals(TEXT("Engine"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Engine");
		}
		else if (RootSegment.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
			|| RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
		{
			const int32 PluginPrefixLength = RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase) ? 8 : 7;
			const FString PluginName = RootSegment.Mid(PluginPrefixLength).TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("Asset Path root '%s' has an invalid plugin name."), *Root);
				return false;
			}
			if (!ResolveContentPluginAssetReferenceRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
		}
		else if ((RootSegment.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
			|| RootSegment.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)) && Segments.IsValidIndex(1))
		{
			const FString PluginName = Segments[1].TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("Asset Path root '%s' has an invalid plugin name."), *Root);
				return false;
			}
			if (!ResolveContentPluginAssetReferenceRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
			FirstFolderSegmentIndex = 2;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported asset Path root '%s'. Use Game, Engine, or Plugin.PluginName."), *Root);
			return false;
		}

		for (int32 Index = FirstFolderSegmentIndex; Index < Segments.Num(); ++Index)
		{
			OutPackagePath += TEXT("/");
			OutPackagePath += Segments[Index].TrimStartAndEnd();
		}

		return true;
	}

	bool TryResolveDreamShaderAssetReference(const FString& InText, FString& OutObjectPath, FString& OutError)
	{
		OutObjectPath.Reset();

		FString Candidate = InText.TrimStartAndEnd();
		if (Candidate.IsEmpty())
		{
			OutError = TEXT("Asset reference cannot be empty.");
			return false;
		}

		FString RootName;
		FString AssetPath;
		if (Candidate.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase))
		{
			if (!Candidate.EndsWith(TEXT(")")))
			{
				OutError = TEXT("Asset Path(...) reference is missing a closing ')'.");
				return false;
			}

			const FString ArgumentBlock = Candidate.Mid(5, Candidate.Len() - 6);
			TArray<FString> Arguments;
			if (!SplitSimpleArgumentList(ArgumentBlock, Arguments))
			{
				OutError = TEXT("Asset Path(...) contains an unterminated string literal.");
				return false;
			}

			if (Arguments.Num() == 1)
			{
				AssetPath = Arguments[0];
			}
			else if (Arguments.Num() == 2)
			{
				RootName = Arguments[0];
				AssetPath = Arguments[1];
			}
			else
			{
				OutError = TEXT("Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game|Engine|Plugin.PluginName, asset path).");
				return false;
			}
		}
		else
		{
			AssetPath = Candidate;
		}

		RootName = TrimMatchingQuotes(RootName);
		AssetPath = TrimMatchingQuotes(AssetPath);
		AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (AssetPath.IsEmpty())
		{
			OutError = TEXT("Asset reference requires a non-empty path.");
			return false;
		}

		FString LongObjectPath;
		if (AssetPath.StartsWith(TEXT("/")))
		{
			LongObjectPath = AssetPath;
		}
		else
		{
			FString PackageRoot;
			if (!ResolveAssetReferenceRootPackagePath(RootName, PackageRoot, OutError))
			{
				return false;
			}
			LongObjectPath = PackageRoot + TEXT("/") + AssetPath;
		}

		const int32 LastSlashIndex = LongObjectPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const int32 LastDotIndex = LongObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastSlashIndex == INDEX_NONE || LastSlashIndex >= LongObjectPath.Len() - 1)
		{
			OutError = FString::Printf(TEXT("Invalid asset path '%s'."), *LongObjectPath);
			return false;
		}

		if (LastDotIndex == INDEX_NONE || LastDotIndex < LastSlashIndex)
		{
			const FString AssetName = LongObjectPath.Mid(LastSlashIndex + 1);
			LongObjectPath += TEXT(".") + AssetName;
		}

		FText PathError;
		if (!FPackageName::IsValidObjectPath(LongObjectPath, &PathError))
		{
			OutError = PathError.ToString();
			return false;
		}

		OutObjectPath = LongObjectPath;
		return true;
	}

	static FString NormalizeEnumLookupKey(const FString& InKey)
	{
		FString Normalized = UE::DreamShader::NormalizeSettingKey(InKey);
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));
		Normalized.ReplaceInline(TEXT("_"), TEXT(""));
		Normalized.ReplaceInline(TEXT("-"), TEXT(""));
		Normalized.ReplaceInline(TEXT(":"), TEXT(""));
		Normalized.ReplaceInline(TEXT("."), TEXT(""));
		Normalized.ReplaceInline(TEXT("/"), TEXT(""));
		return Normalized;
	}

	static bool TryResolveEnumLiteral(UEnum* Enum, const FString& InValue, int64& OutEnumValue)
	{
		if (!Enum)
		{
			return false;
		}

		const FString Candidate = NormalizeEnumLookupKey(InValue);
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}

			const FString ShortName = Enum->GetNameStringByIndex(Index);
			const FString FullName = Enum->GetNameByIndex(Index).ToString();
			const FString DisplayName = Enum->GetDisplayNameTextByIndex(Index).ToString();
			const int32 PrefixSeparatorIndex = ShortName.Find(TEXT("_"));
			const FString PrefixlessShortName = PrefixSeparatorIndex != INDEX_NONE ? ShortName.Mid(PrefixSeparatorIndex + 1) : FString();

			const auto MatchesValue = [&Candidate](const FString& Name)
			{
				return !Name.IsEmpty() && NormalizeEnumLookupKey(Name) == Candidate;
			};

			if (MatchesValue(ShortName)
				|| MatchesValue(FullName)
				|| MatchesValue(DisplayName)
				|| MatchesValue(PrefixlessShortName))
			{
				OutEnumValue = Enum->GetValueByIndex(Index);
				return true;
			}
		}

		return false;
	}

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

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (!bHasParsedAssetReference)
			{
				OutError = FString::Printf(TEXT("Object property '%s' expects Path(...) or an absolute Unreal object path."), *Property->GetName());
				return false;
			}

			UObject* LoadedObject = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *AssetObjectPath);
			if (!LoadedObject)
			{
				OutError = FString::Printf(TEXT("Failed to load asset '%s' for '%s'."), *AssetObjectPath, *Property->GetName());
				return false;
			}

			if (!LoadedObject->IsA(ObjectProperty->PropertyClass))
			{
				OutError = FString::Printf(
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
				if (!TryResolveEnumLiteral(Enum, TrimmedValue, EnumValue))
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
		const FString ImportValue = bHasParsedAssetReference ? AssetObjectPath : TrimmedValue;
		if (Property->ImportText_Direct(*ImportValue, ValuePtr, Target, PPF_None, &ImportErrors) != nullptr)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Property '%s' on '%s' is not a supported literal type yet."), *Property->GetName(), *Target->GetClass()->GetName());
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

	static FString NormalizeMaterialSettingLookupKey(const FString& InKey)
	{
		FString Normalized = UE::DreamShader::NormalizeSettingKey(InKey);
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));
		Normalized.ReplaceInline(TEXT("_"), TEXT(""));
		Normalized.ReplaceInline(TEXT("-"), TEXT(""));
		return Normalized;
	}

	static bool SplitMaterialSettingPath(const FString& InKey, TArray<FString>& OutSegments)
	{
		OutSegments.Reset();

		FString Current;
		int32 BracketDepth = 0;
		for (int32 Index = 0; Index < InKey.Len(); ++Index)
		{
			const TCHAR Character = InKey[Index];
			if (Character == TCHAR('['))
			{
				++BracketDepth;
			}
			else if (Character == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
			}
			else if (Character == TCHAR('.') && BracketDepth == 0)
			{
				OutSegments.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}

			Current.AppendChar(Character);
		}

		if (!Current.IsEmpty())
		{
			OutSegments.Add(Current.TrimStartAndEnd());
		}

		return !OutSegments.IsEmpty();
	}

	static bool ParseMaterialSettingSegment(const FString& InSegmentText, FParsedMaterialSettingSegment& OutSegment, FString& OutError)
	{
		OutSegment = {};
		FString Segment = InSegmentText.TrimStartAndEnd();
		if (Segment.IsEmpty())
		{
			OutError = TEXT("Setting path segment cannot be empty.");
			return false;
		}

		const int32 OpenBracketIndex = Segment.Find(TEXT("["));
		if (OpenBracketIndex == INDEX_NONE)
		{
			OutSegment.Name = Segment;
			return true;
		}

		const int32 CloseBracketIndex = Segment.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (CloseBracketIndex == INDEX_NONE || CloseBracketIndex <= OpenBracketIndex || CloseBracketIndex != Segment.Len() - 1)
		{
			OutError = FString::Printf(TEXT("Invalid array setting segment '%s'."), *InSegmentText);
			return false;
		}

		const FString IndexText = Segment.Mid(OpenBracketIndex + 1, CloseBracketIndex - OpenBracketIndex - 1).TrimStartAndEnd();
		int32 ParsedIndex = INDEX_NONE;
		if (!ParseIntegerLiteral(IndexText, ParsedIndex) || ParsedIndex < 0)
		{
			OutError = FString::Printf(TEXT("Invalid array index '%s' in setting segment '%s'."), *IndexText, *InSegmentText);
			return false;
		}

		OutSegment.Name = Segment.Left(OpenBracketIndex).TrimStartAndEnd();
		OutSegment.ArrayIndex = ParsedIndex;
		OutSegment.bHasArrayIndex = true;
		if (OutSegment.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Invalid array setting segment '%s'."), *InSegmentText);
			return false;
		}

		return true;
	}

	static bool IsSpecialMaterialSettingKey(const FString& InKey)
	{
		const FString Key = NormalizeMaterialSettingLookupKey(InKey);
		return Key == TEXT("blendmode")
			|| Key == TEXT("rendertype")
			|| Key == TEXT("shadingmodel")
			|| Key == TEXT("materialdomain")
			|| Key == TEXT("domain");
	}

	static const TMap<FString, FString>& GetMaterialSettingAliases()
	{
		static const TMap<FString, FString> Aliases = []()
		{
			TMap<FString, FString> Result;
			Result.Add(TEXT("lightingmode"), TEXT("TranslucencyLightingMode"));
			Result.Add(TEXT("translucentlightingmode"), TEXT("TranslucencyLightingMode"));
			Result.Add(TEXT("refractionmode"), TEXT("RefractionMethod"));
			Result.Add(TEXT("physicalmaterial"), TEXT("PhysMaterial"));
			Result.Add(TEXT("physicalmaterialmask"), TEXT("PhysMaterialMask"));
			Result.Add(TEXT("lightmass"), TEXT("LightmassSettings"));
			Result.Add(TEXT("mobileseparatetranslucency"), TEXT("bEnableMobileSeparateTranslucency"));
			Result.Add(TEXT("alwaysevaluateworldpositionoffset"), TEXT("bAlwaysEvaluateWorldPositionOffset"));
			Result.Add(TEXT("responsiveaa"), TEXT("bEnableResponsiveAA"));
			Result.Add(TEXT("thinsurface"), TEXT("bIsThinSurface"));
			return Result;
		}();
		return Aliases;
	}

	static bool TryResolveMaterialSettingPropertyOnStruct(const UStruct* InStruct, const FString& InKey, FProperty*& OutProperty)
	{
		FString LookupKey = NormalizeMaterialSettingLookupKey(InKey);
		if (const FString* Alias = GetMaterialSettingAliases().Find(LookupKey))
		{
			LookupKey = NormalizeMaterialSettingLookupKey(*Alias);
		}

		for (TFieldIterator<FProperty> It(InStruct, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			if (NormalizeMaterialSettingLookupKey(PropertyName) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}

			if (PropertyName.Len() > 1
				&& PropertyName[0] == TCHAR('b')
				&& FChar::IsUpper(PropertyName[1])
				&& NormalizeMaterialSettingLookupKey(PropertyName.Mid(1)) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}

			const FString DisplayName = Property->GetMetaData(TEXT("DisplayName"));
			if (!DisplayName.IsEmpty() && NormalizeMaterialSettingLookupKey(DisplayName) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}
		}

		OutProperty = nullptr;
		return false;
	}

	static bool ResolveMaterialSettingTarget(UObject* RootObject, const FString& InKey, FResolvedMaterialSettingTarget& OutTarget, FString& OutError)
	{
		if (!RootObject)
		{
			OutError = TEXT("Invalid material setting target.");
			return false;
		}

		TArray<FString> Segments;
		if (!SplitMaterialSettingPath(InKey, Segments))
		{
			OutError = FString::Printf(TEXT("Invalid material setting path '%s'."), *InKey);
			return false;
		}

		void* CurrentContainer = RootObject;
		const UStruct* CurrentStruct = RootObject->GetClass();

		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			FParsedMaterialSettingSegment Segment;
			if (!ParseMaterialSettingSegment(Segments[SegmentIndex], Segment, OutError))
			{
				return false;
			}

			FProperty* Property = nullptr;
			if (!TryResolveMaterialSettingPropertyOnStruct(CurrentStruct, Segment.Name, Property))
			{
				OutError = FString::Printf(TEXT("Unsupported material setting '%s'."), *InKey);
				return false;
			}

			if (Segment.bHasArrayIndex)
			{
				if (Property->ArrayDim <= 1)
				{
					OutError = FString::Printf(TEXT("Setting '%s' is not an indexed array property."), *Segments[SegmentIndex]);
					return false;
				}

				if (Segment.ArrayIndex >= Property->ArrayDim)
				{
					OutError = FString::Printf(
						TEXT("Array index %d is out of range for setting '%s' (max %d)."),
						Segment.ArrayIndex,
						*Segments[SegmentIndex],
						Property->ArrayDim - 1);
					return false;
				}
			}
			else if (Property->ArrayDim > 1)
			{
				OutError = FString::Printf(TEXT("Setting '%s' requires an explicit [index]."), *Segments[SegmentIndex]);
				return false;
			}

			const int32 ArrayIndex = Segment.bHasArrayIndex ? Segment.ArrayIndex : 0;
			if (SegmentIndex == Segments.Num() - 1)
			{
				OutTarget.OwnerObject = RootObject;
				OutTarget.ContainerPtr = CurrentContainer;
				OutTarget.Property = Property;
				OutTarget.ArrayIndex = ArrayIndex;
				return true;
			}

			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (!StructProperty || !StructProperty->Struct)
			{
				OutError = FString::Printf(TEXT("Setting path '%s' cannot continue through '%s'."), *InKey, *Segments[SegmentIndex]);
				return false;
			}

			CurrentContainer = StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer, ArrayIndex);
			CurrentStruct = StructProperty->Struct;
		}

		OutError = FString::Printf(TEXT("Invalid material setting path '%s'."), *InKey);
		return false;
	}

	static bool ValidateGenericMaterialSetting(const FString& InKey, const FString& InValue, FString& OutError)
	{
		UMaterial* ProbeMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!ProbeMaterial)
		{
			OutError = TEXT("Failed to create a transient material for Settings validation.");
			return false;
		}

		FResolvedMaterialSettingTarget Target;
		if (!ResolveMaterialSettingTarget(ProbeMaterial, InKey, Target, OutError))
		{
			return false;
		}

		FString LiteralError;
		void* ValuePtr = Target.Property->ContainerPtrToValuePtr<void>(Target.ContainerPtr, Target.ArrayIndex);
		if (!SetMaterialExpressionLiteralProperty(Target.OwnerObject, Target.Property, ValuePtr, InValue, LiteralError))
		{
			OutError = FString::Printf(TEXT("Invalid value '%s' for setting '%s'. %s"), *InValue, *InKey, *LiteralError);
			return false;
		}

		return true;
	}

	static bool ApplyGenericMaterialSetting(UMaterial* Material, const FString& InKey, const FString& InValue, FString& OutError)
	{
		FResolvedMaterialSettingTarget Target;
		if (!ResolveMaterialSettingTarget(Material, InKey, Target, OutError))
		{
			return false;
		}

		FString LiteralError;
		void* ValuePtr = Target.Property->ContainerPtrToValuePtr<void>(Target.ContainerPtr, Target.ArrayIndex);
		if (!SetMaterialExpressionLiteralProperty(Target.OwnerObject, Target.Property, ValuePtr, InValue, LiteralError))
		{
			OutError = FString::Printf(TEXT("Invalid value '%s' for setting '%s'. %s"), *InValue, *InKey, *LiteralError);
			return false;
		}

		return true;
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

	static bool IsFunctionIdentifierStart(const TCHAR Char)
	{
		return FChar::IsAlpha(Char) || Char == TCHAR('_');
	}

	static bool IsFunctionIdentifierPart(const TCHAR Char)
	{
		return FChar::IsAlnum(Char) || Char == TCHAR('_');
	}

	static int32 SkipInlineWhitespace(const FString& Source, int32 Index)
	{
		while (Source.IsValidIndex(Index) && FChar::IsWhitespace(Source[Index]))
		{
			++Index;
		}
		return Index;
	}

	static bool TryReadQualifiedIdentifierToken(const FString& Source, int32& InOutIndex, FString& OutIdentifier)
	{
		if (!Source.IsValidIndex(InOutIndex) || !IsFunctionIdentifierStart(Source[InOutIndex]))
		{
			return false;
		}

		const int32 IdentifierStart = InOutIndex++;
		while (Source.IsValidIndex(InOutIndex) && IsFunctionIdentifierPart(Source[InOutIndex]))
		{
			++InOutIndex;
		}

		OutIdentifier = Source.Mid(IdentifierStart, InOutIndex - IdentifierStart);
		while (Source.IsValidIndex(InOutIndex + 2)
			&& Source[InOutIndex] == TCHAR(':')
			&& Source[InOutIndex + 1] == TCHAR(':')
			&& IsFunctionIdentifierStart(Source[InOutIndex + 2]))
		{
			const int32 NextIdentifierStart = InOutIndex + 2;
			int32 NextIdentifierEnd = NextIdentifierStart + 1;
			while (Source.IsValidIndex(NextIdentifierEnd) && IsFunctionIdentifierPart(Source[NextIdentifierEnd]))
			{
				++NextIdentifierEnd;
			}

			OutIdentifier += TEXT("::") + Source.Mid(NextIdentifierStart, NextIdentifierEnd - NextIdentifierStart);
			InOutIndex = NextIdentifierEnd;
		}

		return true;
	}

	static void AddFunctionLookupEntries(
		const FTextShaderFunctionDefinition& Function,
		TMap<FString, const FTextShaderFunctionDefinition*>& OutFunctionsBySpelling,
		TMap<FString, FString>& OutGeneratedNamesBySpelling)
	{
		const FString GeneratedName = BuildGeneratedFunctionSymbolName(Function);

		OutFunctionsBySpelling.Add(Function.Name, &Function);
		OutGeneratedNamesBySpelling.Add(Function.Name, GeneratedName);

		if (!GeneratedName.Equals(Function.Name, ESearchCase::CaseSensitive))
		{
			OutFunctionsBySpelling.Add(GeneratedName, &Function);
			OutGeneratedNamesBySpelling.Add(GeneratedName, GeneratedName);
		}
	}

	static void CollectDreamShaderFunctionCalls(
		const FString& Source,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		TArray<const FTextShaderFunctionDefinition*>& OutFunctions)
	{
		TSet<const FTextShaderFunctionDefinition*> SeenFunctions;
		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					Index += 2;
				}
				else
				{
					if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					++Index;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Index += 2;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Index += 2;
				continue;
			}

			if (IsFunctionIdentifierStart(Char))
			{
				int32 IdentifierEnd = Index;
				FString Identifier;
				if (TryReadQualifiedIdentifierToken(Source, IdentifierEnd, Identifier))
				{
					const int32 PostIdentifier = SkipInlineWhitespace(Source, IdentifierEnd);
					if (Source.IsValidIndex(PostIdentifier) && Source[PostIdentifier] == TCHAR('('))
					{
						if (const FTextShaderFunctionDefinition* const* Function = FunctionsBySpelling.Find(Identifier))
						{
							if (!SeenFunctions.Contains(*Function))
							{
								SeenFunctions.Add(*Function);
								OutFunctions.Add(*Function);
							}
						}
					}

					Index = IdentifierEnd;
					continue;
				}
			}

			++Index;
		}
	}

	static FString RewriteDreamShaderFunctionReferences(
		const FString& Source,
		const TMap<FString, FString>& ReplacementBySpelling)
	{
		FString Result;
		Result.Reserve(Source.Len() + 128);

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					Result.AppendChar(Next);
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					Result.AppendChar(Source[Index + 1]);
					Index += 2;
				}
				else
				{
					if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					++Index;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				Result.AppendChar(Char);
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (IsFunctionIdentifierStart(Char))
			{
				int32 IdentifierEnd = Index;
				FString Identifier;
				if (TryReadQualifiedIdentifierToken(Source, IdentifierEnd, Identifier))
				{
					const int32 PostIdentifier = SkipInlineWhitespace(Source, IdentifierEnd);
					if (Source.IsValidIndex(PostIdentifier) && Source[PostIdentifier] == TCHAR('('))
					{
						if (const FString* Replacement = ReplacementBySpelling.Find(Identifier))
						{
							Result += *Replacement;
							Index = IdentifierEnd;
							continue;
						}
					}

					Result += Source.Mid(Index, IdentifierEnd - Index);
					Index = IdentifierEnd;
					continue;
				}
			}

			Result.AppendChar(Char);
			++Index;
		}

		return Result;
	}

	static void AppendIndentedCode(FString& OutSource, const FString& Source, const FString& Indent)
	{
		int32 LineStart = 0;
		while (LineStart < Source.Len())
		{
			const int32 NewLineIndex = Source.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
			const bool bHasNewLine = NewLineIndex != INDEX_NONE;
			const int32 LineEnd = bHasNewLine ? NewLineIndex : Source.Len();

			OutSource += Indent;
			OutSource += Source.Mid(LineStart, LineEnd - LineStart);
			OutSource += TEXT("\n");

			if (!bHasNewLine)
			{
				break;
			}

			LineStart = NewLineIndex + 1;
		}
	}

	static FString BuildGeneratedFunctionParameterList(const FTextShaderFunctionDefinition& Function)
	{
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

		return FString::Join(Parameters, TEXT(", "));
	}

	static void AppendGeneratedFunctionDefinition(
		const FTextShaderFunctionDefinition& Function,
		const TMap<FString, FString>& ReplacementBySpelling,
		const FString& Indent,
		FString& OutSource)
	{
		const FString ReturnType = Function.Results.IsEmpty() ? TEXT("void") : Function.Results[0].Type;
		const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(Function);

		OutSource += FString::Printf(TEXT("%s%s %s(%s)\n%s{\n"), *Indent, *ReturnType, *GeneratedFunctionName, *BuildGeneratedFunctionParameterList(Function), *Indent);

		if (!Function.Results.IsEmpty())
		{
			OutSource += FString::Printf(TEXT("%s\t%s %s = (%s)0;\n"), *Indent, *Function.Results[0].Type, *Function.Results[0].Name, *Function.Results[0].Type);
		}

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FTextShaderFunctionParameter& Output = Function.Results[ResultIndex];
			OutSource += FString::Printf(TEXT("%s\t%s = (%s)0;\n"), *Indent, *Output.Name, *Output.Type);
		}

		const FString RewrittenFunctionHLSL = RewriteDreamShaderFunctionReferences(Function.HLSL, ReplacementBySpelling);
		AppendIndentedCode(OutSource, RewrittenFunctionHLSL, Indent + TEXT("\t"));

		if (!Function.Results.IsEmpty())
		{
			OutSource += FString::Printf(TEXT("%s\treturn %s;\n"), *Indent, *Function.Results[0].Name);
		}

		OutSource += FString::Printf(TEXT("%s}\n"), *Indent);
	}

	static FString BuildSelfContainedWrapperTypeName(const FString& WrapperNameHint)
	{
		const FString SanitizedHint = UE::DreamShader::SanitizeIdentifier(WrapperNameHint.IsEmpty() ? TEXT("Generated") : WrapperNameHint);
		return FString::Printf(TEXT("generated_wrapper_%s_%08X"), *SanitizedHint, FCrc::StrCrc32(*WrapperNameHint));
	}

	static FString BuildSelfContainedWrapperVariableName(const FString& WrapperNameHint)
	{
		return FString::Printf(TEXT("__ds_wrapper_%08X"), FCrc::StrCrc32(*WrapperNameHint));
	}

	static bool CollectEmbeddedFunctionClosure(
		const FTextShaderDefinition& Definition,
		const TArray<const FTextShaderFunctionDefinition*>& RootFunctions,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		TArray<const FTextShaderFunctionDefinition*>& OutOrderedFunctions,
		FString& OutError)
	{
		TMap<const FTextShaderFunctionDefinition*, TArray<const FTextShaderFunctionDefinition*>> DependenciesByFunction;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			TArray<const FTextShaderFunctionDefinition*> Dependencies;
			CollectDreamShaderFunctionCalls(Function.HLSL, FunctionsBySpelling, Dependencies);
			DependenciesByFunction.Add(&Function, MoveTemp(Dependencies));
		}

		enum class EVisitState : uint8
		{
			Unvisited,
			Visiting,
			Visited
		};

		TMap<const FTextShaderFunctionDefinition*, EVisitState> VisitStates;
		TArray<const FTextShaderFunctionDefinition*> VisitStack;
		TFunction<bool(const FTextShaderFunctionDefinition*)> VisitFunction;
		VisitFunction = [&](const FTextShaderFunctionDefinition* Function) -> bool
		{
			const EVisitState VisitState = VisitStates.FindRef(Function);
			if (VisitState == EVisitState::Visited)
			{
				return true;
			}

			if (VisitState == EVisitState::Visiting)
			{
				int32 CycleStartIndex = VisitStack.IndexOfByKey(Function);
				if (CycleStartIndex == INDEX_NONE)
				{
					CycleStartIndex = 0;
				}

				TArray<FString> CycleNames;
				for (int32 Index = CycleStartIndex; Index < VisitStack.Num(); ++Index)
				{
					CycleNames.Add(VisitStack[Index]->Name);
				}
				CycleNames.Add(Function->Name);

				OutError = FString::Printf(
					TEXT("SelfContained Function cycle detected: %s. HLSL Custom nodes cannot compile recursive DreamShader functions."),
					*FString::Join(CycleNames, TEXT(" -> ")));
				return false;
			}

			VisitStates.Add(Function, EVisitState::Visiting);
			VisitStack.Add(Function);

			const TArray<const FTextShaderFunctionDefinition*>* Dependencies = DependenciesByFunction.Find(Function);
			if (Dependencies)
			{
				for (const FTextShaderFunctionDefinition* Dependency : *Dependencies)
				{
					if (!VisitFunction(Dependency))
					{
						return false;
					}
				}
			}

			VisitStack.Pop();
			VisitStates.Add(Function, EVisitState::Visited);
			OutOrderedFunctions.Add(Function);
			return true;
		};

		for (const FTextShaderFunctionDefinition* RootFunction : RootFunctions)
		{
			if (!RootFunction || !VisitFunction(RootFunction))
			{
				return false;
			}
		}

		return true;
	}

	bool PrepareCustomNodeCode(
		const FTextShaderDefinition& Definition,
		const FString& SourceCode,
		const TArray<FString>& RequestedEmbeddedFunctionNames,
		const FString& WrapperNameHint,
		FString& OutCode,
		bool& bOutUsesGeneratedInclude,
		FString& OutError)
	{
		OutCode = SourceCode;
		bOutUsesGeneratedInclude = false;
		OutError.Reset();

		if (Definition.Functions.IsEmpty())
		{
			return true;
		}

		TMap<FString, const FTextShaderFunctionDefinition*> FunctionsBySpelling;
		TMap<FString, FString> GeneratedNamesBySpelling;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			AddFunctionLookupEntries(Function, FunctionsBySpelling, GeneratedNamesBySpelling);
		}

		TArray<const FTextShaderFunctionDefinition*> DirectCalls;
		CollectDreamShaderFunctionCalls(SourceCode, FunctionsBySpelling, DirectCalls);

		TArray<const FTextShaderFunctionDefinition*> RootFunctions;
		TSet<const FTextShaderFunctionDefinition*> SeenRoots;
		if (!RequestedEmbeddedFunctionNames.IsEmpty())
		{
			for (const FString& RequestedName : RequestedEmbeddedFunctionNames)
			{
				const FTextShaderFunctionDefinition* const* Function = FunctionsBySpelling.Find(RequestedName);
				if (!Function)
				{
					OutError = FString::Printf(TEXT("Unknown SelfContained DreamShader Function '%s'."), *RequestedName);
					return false;
				}

				if (!SeenRoots.Contains(*Function))
				{
					SeenRoots.Add(*Function);
					RootFunctions.Add(*Function);
				}
			}
		}
		else
		{
			for (const FTextShaderFunctionDefinition* Function : DirectCalls)
			{
				if (Function && Function->bSelfContained && !SeenRoots.Contains(Function))
				{
					SeenRoots.Add(Function);
					RootFunctions.Add(Function);
				}
			}
		}

		if (RootFunctions.IsEmpty())
		{
			bOutUsesGeneratedInclude = !DirectCalls.IsEmpty();
			return true;
		}

		TArray<const FTextShaderFunctionDefinition*> EmbeddedFunctions;
		if (!CollectEmbeddedFunctionClosure(Definition, RootFunctions, FunctionsBySpelling, EmbeddedFunctions, OutError))
		{
			return false;
		}

		TSet<const FTextShaderFunctionDefinition*> EmbeddedFunctionSet;
		TMap<FString, FString> EmbeddedReferenceReplacements;
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			EmbeddedFunctionSet.Add(Function);
		}

		const FString WrapperTypeName = BuildSelfContainedWrapperTypeName(WrapperNameHint);
		const FString WrapperVariableName = BuildSelfContainedWrapperVariableName(WrapperNameHint);
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(*Function);
			EmbeddedReferenceReplacements.Add(Function->Name, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
			if (!GeneratedFunctionName.Equals(Function->Name, ESearchCase::CaseSensitive))
			{
				EmbeddedReferenceReplacements.Add(GeneratedFunctionName, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
			}
		}

		FString WrapperSource;
		WrapperSource += FString::Printf(TEXT("struct %s\n{\n"), *WrapperTypeName);
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			AppendGeneratedFunctionDefinition(*Function, GeneratedNamesBySpelling, TEXT("\t"), WrapperSource);
			WrapperSource += TEXT("\n");
		}
		WrapperSource += FString::Printf(TEXT("};\n%s %s;\n"), *WrapperTypeName, *WrapperVariableName);

		OutCode = WrapperSource + TEXT("\n") + RewriteDreamShaderFunctionReferences(SourceCode, EmbeddedReferenceReplacements);

		for (const FTextShaderFunctionDefinition* DirectCall : DirectCalls)
		{
			if (!EmbeddedFunctionSet.Contains(DirectCall))
			{
				bOutUsesGeneratedInclude = true;
				break;
			}
		}

		return true;
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
		TMap<FString, FString> GeneratedNamesBySpelling;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			const FString GeneratedName = BuildGeneratedFunctionSymbolName(Function);
			GeneratedNamesBySpelling.Add(Function.Name, GeneratedName);
			if (!GeneratedName.Equals(Function.Name, ESearchCase::CaseSensitive))
			{
				GeneratedNamesBySpelling.Add(GeneratedName, GeneratedName);
			}
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

			AppendGeneratedFunctionDefinition(Function, GeneratedNamesBySpelling, FString(), OutSource);
			OutSource += TEXT("\n");
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
		Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
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

		for (const TPair<FString, FString>& Setting : Definition.Settings)
		{
			if (IsSpecialMaterialSettingKey(Setting.Key))
			{
				continue;
			}

			if (!ValidateGenericMaterialSetting(Setting.Key, Setting.Value, OutError))
			{
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

		for (const TPair<FString, FString>& Setting : Definition.Settings)
		{
			if (IsSpecialMaterialSettingKey(Setting.Key))
			{
				continue;
			}

			if (!ApplyGenericMaterialSetting(Material, Setting.Key, Setting.Value, OutError))
			{
				return false;
			}
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

	static FString FormatMetadataContext(const FTextShaderPropertyDefinition& Property)
	{
		return FString::Printf(TEXT("property '%s'"), *Property.Name);
	}

	static void ApplyExpressionMetadata(UMaterialExpression* Expression, const FTextShaderMetadata& Metadata)
	{
		if (!Expression)
		{
			return;
		}

		if (!Metadata.Description.IsEmpty())
		{
			Expression->Desc = Metadata.Description;
		}

		if (UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression))
		{
			if (!Metadata.Group.IsEmpty())
			{
				Parameter->Group = FName(*Metadata.Group);
			}
			if (Metadata.bHasSortPriority)
			{
				Parameter->SortPriority = Metadata.SortPriority;
			}
		}

		if (UMaterialExpressionTextureSampleParameter* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
		{
			if (!Metadata.Group.IsEmpty())
			{
				TextureParameter->Group = FName(*Metadata.Group);
			}
			if (Metadata.bHasSortPriority)
			{
				TextureParameter->SortPriority = Metadata.SortPriority;
			}
		}

		if (UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression))
		{
			if (!Metadata.Group.IsEmpty())
			{
				CollectionParameter->Group = FName(*Metadata.Group);
			}
			if (Metadata.bHasSortPriority)
			{
				CollectionParameter->SortPriority = Metadata.SortPriority;
			}
		}
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

		OutError = FString::Printf(TEXT("'%s' does not expose a ParameterName property."), *Expression->GetClass()->GetName());
		return false;
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

			if (FProperty* TextureProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), TEXT("Texture")))
			{
				return SetMaterialExpressionLiteralProperty(Expression, TextureProperty, Property.TextureDefaultObjectPath, OutError);
			}
			if (FProperty* TextureObjectProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), TEXT("TextureObject")))
			{
				return SetMaterialExpressionLiteralProperty(Expression, TextureObjectProperty, Property.TextureDefaultObjectPath, OutError);
			}

			OutError = FString::Printf(TEXT("'%s' does not expose a Texture property for %s."), *Expression->GetClass()->GetName(), *FormatMetadataContext(Property));
			return false;
		}

		FProperty* DefaultValueProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), TEXT("DefaultValue"));
		if (!DefaultValueProperty)
		{
			return true;
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
			TEXT("(R=%f,G=%f,B=%f,A=%f)"),
			Property.VectorDefaultValue.R,
			Property.VectorDefaultValue.G,
			Property.VectorDefaultValue.B,
			Property.VectorDefaultValue.A);
		if (SetMaterialExpressionLiteralProperty(Expression, DefaultValueProperty, LinearColorText, OutError))
		{
			return true;
		}

		const FString VectorText = FString::Printf(
			TEXT("(X=%f,Y=%f,Z=%f,W=%f)"),
			Property.VectorDefaultValue.R,
			Property.VectorDefaultValue.G,
			Property.VectorDefaultValue.B,
			Property.VectorDefaultValue.A);
		return SetMaterialExpressionLiteralProperty(Expression, DefaultValueProperty, VectorText, OutError);
	}

	static UMaterialExpression* CreateGenericParameterNodeExpression(
		UMaterial* Material,
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
			UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, -800, PositionY));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a '%s' node for property '%s'."), *Property.ParameterNodeType, *Property.Name);
			return nullptr;
		}

		if (!SetExpressionParameterName(Expression, Property.Name, OutError)
			|| !SetExpressionDefaultValue(Expression, Property, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FormatMetadataContext(Property), *OutError);
			return nullptr;
		}

		if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(Expression))
		{
			TextureExpression->AutoSetSampleType();
		}

		ApplyExpressionMetadata(Expression, Property.Metadata);
		return Expression;
	}

	static UMaterialExpression* CreateParameterExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		int32 PositionY,
		FString& OutError)
	{
		check(Material);

		if (!Property.ParameterNodeType.IsEmpty()
			&& !Property.ParameterNodeType.Equals(TEXT("ScalarParameter"), ESearchCase::IgnoreCase)
			&& !Property.ParameterNodeType.Equals(TEXT("VectorParameter"), ESearchCase::IgnoreCase)
			&& !Property.ParameterNodeType.Equals(TEXT("TextureObjectParameter"), ESearchCase::IgnoreCase))
		{
			return CreateGenericParameterNodeExpression(Material, Property, PositionY, OutError);
		}

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
			ApplyExpressionMetadata(Expression, Property.Metadata);
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
			ApplyExpressionMetadata(ParameterExpression, Property.Metadata);

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
		ApplyExpressionMetadata(Expression, Property.Metadata);
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

		ApplyExpressionMetadata(Expression, Property.Metadata);
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
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCollectionParameter::StaticClass(), -800, PositionY));
			if (!Expression)
			{
				OutError = MakeError(TEXT("Failed to create the native CollectionParameter node."));
				return nullptr;
			}

			Expression->Collection = Collection;
			Expression->ParameterName = ParameterName;
			Expression->ParameterId = Collection->GetParameterId(ParameterName);
			if (!Expression->ExpressionGUID.IsValid())
			{
				Expression->ExpressionGUID = FGuid::NewGuid();
			}
			ApplyExpressionMetadata(Expression, Property.Metadata);
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

	static bool AppendSanitizedPackageSegment(
		const FString& RawSegment,
		const FString& ErrorContext,
		FString& InOutPackagePath,
		FString& OutError)
	{
		FString Segment = RawSegment.TrimStartAndEnd();
		if (Segment.IsEmpty())
		{
			return true;
		}

		const FString FolderName = ObjectTools::SanitizeObjectName(Segment);
		if (FolderName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s contains an invalid folder segment."), *ErrorContext);
			return false;
		}

		InOutPackagePath += TEXT("/");
		InOutPackagePath += FolderName;
		return true;
	}

	static bool ResolveProjectContentPluginPackageRoot(
		const FString& Root,
		const FString& PluginName,
		FString& OutPackagePath,
		FString& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but no enabled plugin with that name was found."), *Root, *PluginName);
			return false;
		}

		const FString PluginBaseDir = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
		const FString ProjectPluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
		if (Plugin->GetType() != EPluginType::Project || !FPaths::IsUnderDirectory(PluginBaseDir, ProjectPluginsDir))
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' must reference a project plugin under '%s'."), *Root, *ProjectPluginsDir);
			return false;
		}

		if (!Plugin->IsEnabled())
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin is not enabled."), *Root, *PluginName);
			return false;
		}

		if (!Plugin->CanContainContent())
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin cannot contain content."), *Root, *PluginName);
			return false;
		}

		const FString ContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
		if (!IFileManager::Get().DirectoryExists(*ContentDir))
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but its Content directory does not exist: '%s'."), *Root, *PluginName, *ContentDir);
			return false;
		}

		if (!Plugin->IsMounted())
		{
			OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin content is not mounted."), *Root, *PluginName);
			return false;
		}

		FString MountedAssetPath = Plugin->GetMountedAssetPath();
		MountedAssetPath.TrimStartAndEndInline();
		MountedAssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (MountedAssetPath.EndsWith(TEXT("/")))
		{
			MountedAssetPath.LeftChopInline(1, EAllowShrinking::No);
		}
		if (!MountedAssetPath.StartsWith(TEXT("/")))
		{
			MountedAssetPath = TEXT("/") + MountedAssetPath;
		}

		if (MountedAssetPath.IsEmpty() || MountedAssetPath == TEXT("/"))
		{
			MountedAssetPath = TEXT("/") + Plugin->GetName();
		}

		OutPackagePath = MountedAssetPath;
		return true;
	}

	static bool ResolveDreamShaderRootPackagePath(
		const FString& Root,
		FString& OutPackagePath,
		FString& OutError)
	{
		FString Normalized = Root;
		Normalized.TrimStartAndEndInline();
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

		if (Normalized.IsEmpty())
		{
			OutPackagePath = TEXT("/Game");
			return true;
		}

		const bool bHadLeadingSlash = Normalized.StartsWith(TEXT("/"));
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
			OutPackagePath = TEXT("/Game");
			return true;
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutPackagePath = TEXT("/Game");
			return true;
		}

		const FString RootSegment = Segments[0].TrimStartAndEnd();
		int32 FirstFolderSegmentIndex = 1;
		if (RootSegment.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Game");
		}
		else if (RootSegment.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
			|| RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
		{
			const int32 PluginPrefixLength = RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase) ? 8 : 7;
			FString PluginName = RootSegment.Mid(PluginPrefixLength).TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid plugin name."), *Root);
				return false;
			}

			if (!ResolveProjectContentPluginPackageRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
		}
		else if ((RootSegment.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
			|| RootSegment.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)) && Segments.IsValidIndex(1))
		{
			FString PluginName = Segments[1].TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid plugin name."), *Root);
				return false;
			}

			if (!ResolveProjectContentPluginPackageRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
			FirstFolderSegmentIndex = 2;
		}
		else if (bHadLeadingSlash)
		{
			if (RootSegment.IsEmpty() || ObjectTools::SanitizeObjectName(RootSegment) != RootSegment)
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid package root."), *Root);
				return false;
			}

			OutPackagePath = TEXT("/") + RootSegment;
		}
		else
		{
			OutPackagePath = TEXT("/Game");
			FirstFolderSegmentIndex = 0;
		}

		for (int32 Index = FirstFolderSegmentIndex; Index < Segments.Num(); ++Index)
		{
			if (!AppendSanitizedPackageSegment(Segments[Index], FString::Printf(TEXT("DreamShader Root '%s'"), *Root), OutPackagePath, OutError))
			{
				return false;
			}
		}

		return true;
	}

	bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		const FString& Root,
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

		FString PackagePath;
		if (!ResolveDreamShaderRootPackagePath(Root, PackagePath, OutError))
		{
			return false;
		}

		for (int32 Index = 0; Index < Segments.Num() - 1; ++Index)
		{
			if (!AppendSanitizedPackageSegment(
				Segments[Index],
				FString::Printf(TEXT("DreamShader asset name '%s'"), *AssetName),
				PackagePath,
				OutError))
			{
				return false;
			}
		}

		OutPackageName = PackagePath + TEXT("/") + OutAssetLeafName;
		OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *OutAssetLeafName);
		FText PathError;
		if (!FPackageName::IsValidObjectPath(OutObjectPath, &PathError))
		{
			const FString PathErrorText = PathError.ToString();
			OutError = PathErrorText.IsEmpty()
				? FString::Printf(TEXT("DreamShader asset path '%s' is not a valid Unreal object path."), *OutObjectPath)
				: PathErrorText;
			return false;
		}

		return true;
	}

	bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FString& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, OutError))
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
		if (!ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, OutError))
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
		if (InTypeName.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("StaticBoolParameter"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 1;
			bOutIsTexture = false;
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_StaticBool);
			return true;
		}

		if (!TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture))
		{
			return false;
		}

		if (bOutIsTexture)
		{
			if (InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_TextureCube);
			}
			else if (InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Texture2DArray);
			}
			else
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Texture2D);
			}
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
		const auto IsSimpleOutputReference = [](const FString& InText) -> bool
		{
			const FString Candidate = InText.TrimStartAndEnd();
			if (Candidate.IsEmpty())
			{
				return false;
			}

			for (int32 Index = 0; Index < Candidate.Len(); ++Index)
			{
				const TCHAR Char = Candidate[Index];
				if (Index == 0)
				{
					if (!(FChar::IsAlpha(Char) || Char == TCHAR('_')))
					{
						return false;
					}
				}
				else if (!(FChar::IsAlnum(Char) || Char == TCHAR('_')))
				{
					return false;
				}
			}

			return true;
		};

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
			const FString SourceName = Binding.SourceText.TrimStartAndEnd();
			const bool bIsSimpleSourceReference = IsSimpleOutputReference(SourceName);
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

			if (SourceName.Equals(TEXT("return"), ESearchCase::IgnoreCase))
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

			if (!bIsSimpleSourceReference)
			{
				continue;
			}

			if (const int32* ExistingIndex = OutputOrder.Find(SourceName))
			{
				if (OutNamedOutputs[*ExistingIndex].OutputType != BindingOutputType && bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(TEXT("Output variable '%s' is bound to incompatible material properties."), *SourceName);
					return false;
				}
			}
			else
			{
				FResolvedNamedOutput& Output = OutNamedOutputs.AddDefaulted_GetRef();
				Output.Name = SourceName;
				Output.OutputType = BindingOutputType;

				if (const ECustomMaterialOutputType* DeclaredType = DeclaredOutputTypes.Find(SourceName))
				{
					if (bHasImplicitTypeFromTarget && *DeclaredType != BindingOutputType)
					{
						OutError = FString::Printf(
							TEXT("Output variable '%s' is declared as '%s' but bound material property '%s' expects a different type."),
							*SourceName,
							*DeclaredOutputTypeTexts.FindChecked(SourceName),
							*Binding.MaterialProperty);
						return false;
					}

					Output.OutputType = *DeclaredType;
				}
				else if (!bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(
						TEXT("Output variable '%s' must declare an explicit type before binding to expression target '%s'."),
						*SourceName,
						*Binding.TargetText);
					return false;
				}

				OutputOrder.Add(SourceName, OutNamedOutputs.Num() - 1);
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
