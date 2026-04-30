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

	struct FTextShaderMetadata
	{
		FString Group;
		bool bHasSortPriority = false;
		int32 SortPriority = 32;
		FString Description;
		TMap<FString, FString> ReflectedProperties;
	};

	struct FTextShaderPropertyDefinition
	{
		FString Name;
		ETextShaderPropertySource Source = ETextShaderPropertySource::Parameter;
		FString ParameterNodeType;
		FString UEBuiltinFunctionName;
		TMap<FString, FString> UEBuiltinArguments;
		ETextShaderPropertyType Type = ETextShaderPropertyType::Scalar;
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		int32 ComponentCount = 1;
		bool bConst = false;
		bool bHasDefaultValue = false;
		double ScalarDefaultValue = 0.0;
		FLinearColor VectorDefaultValue = FLinearColor::White;
		FString TextureDefaultObjectPath;
		FTextShaderMetadata Metadata;
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
		bool bOptional = false;
		bool bHasDefaultValue = false;
		FString DefaultValueText;
		FTextShaderMetadata Metadata;
	};

	struct FTextShaderFunctionDefinition
	{
		FString Name;
		bool bSelfContained = false;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Results;
		FString HLSL;
	};

	struct FTextShaderMaterialFunctionDefinition
	{
		FString Name;
		FString Root;
		TArray<FTextShaderPropertyDefinition> Properties;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Outputs;
		TMap<FString, FString> Settings;
		FString Code;
		FString HLSL;
	};

	struct FTextShaderVirtualFunctionDefinition
	{
		FString Name;
		FString Asset;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Outputs;
		TMap<FString, FString> Options;
	};

	struct FTextShaderDefinition
	{
		FString Name;
		FString Root;
		TArray<FTextShaderPropertyDefinition> Properties;
		TMap<FString, FString> Settings;
		TArray<FTextShaderVariableDeclaration> OutputDeclarations;
		TArray<FTextShaderOutputBinding> Outputs;
		FString Code;
		FString HLSL;
		TArray<FTextShaderFunctionDefinition> Functions;
		TArray<FTextShaderMaterialFunctionDefinition> MaterialFunctions;
		TArray<FTextShaderVirtualFunctionDefinition> VirtualFunctions;
		TArray<FString> Warnings;

		DREAMSHADER_API bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
		DREAMSHADER_API FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
	};

	DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
}
