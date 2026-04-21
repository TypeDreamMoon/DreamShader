#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader
{
	enum class ETextShaderPropertyType : uint8
	{
		Scalar,
		Vector,
		Texture2D,
	};

	enum class ETextShaderTextureType : uint8
	{
		Texture2D,
		TextureCube,
		Texture2DArray,
	};

	enum class ETextShaderPropertySource : uint8
	{
		Parameter,
		UEBuiltin,
	};

	struct FTextShaderPropertyDefinition
	{
		FString Name;
		ETextShaderPropertySource Source = ETextShaderPropertySource::Parameter;
		FString UEBuiltinFunctionName;
		TMap<FString, FString> UEBuiltinArguments;
		ETextShaderPropertyType Type = ETextShaderPropertyType::Scalar;
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		int32 ComponentCount = 1;
		bool bHasDefaultValue = false;
		double ScalarDefaultValue = 0.0;
		FLinearColor VectorDefaultValue = FLinearColor::White;
		FString TextureDefaultObjectPath;
	};

	struct FTextShaderOutputBinding
	{
		enum class ETargetKind : uint8
		{
			MaterialProperty,
			ExpressionInput,
		};

		ETargetKind TargetKind = ETargetKind::MaterialProperty;
		FString MaterialProperty;
		FString ExpressionClass;
		TMap<FString, FString> ExpressionArguments;
		int32 ExpressionPinIndex = INDEX_NONE;
		FString TargetText;
		FString SourceText;
	};

	struct FTextShaderVariableDeclaration
	{
		FString Type;
		FString Name;
	};

	struct FTextShaderFunctionParameter
	{
		FString Type;
		FString Name;
	};

	struct FTextShaderFunctionDefinition
	{
		FString Name;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Results;
		FString HLSL;
	};

	struct FTextShaderMaterialFunctionDefinition
	{
		FString Name;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Outputs;
		TMap<FString, FString> Settings;
		FString Code;
		FString HLSL;
	};

	struct FTextShaderDefinition
	{
		FString Name;
		TArray<FTextShaderPropertyDefinition> Properties;
		TMap<FString, FString> Settings;
		TArray<FTextShaderVariableDeclaration> OutputDeclarations;
		TArray<FTextShaderOutputBinding> Outputs;
		FString Code;
		FString HLSL;
		TArray<FTextShaderFunctionDefinition> Functions;
		TArray<FTextShaderMaterialFunctionDefinition> MaterialFunctions;
		TArray<FString> Warnings;

		DREAMSHADER_API bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
		DREAMSHADER_API FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
	};

	DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
}
