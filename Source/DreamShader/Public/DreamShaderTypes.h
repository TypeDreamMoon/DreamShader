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
		VolumeTexture,
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
		/**
		 * True when the declared type token names the texture dimension (Texture2D / TextureCube /
		 * Texture2DArray / VolumeTexture, or a TextureSampleParameter* variant). Tokens such as
		 * TextureObjectParameter map to a single Unreal node class that holds any texture dimension, so
		 * TextureType stays at its Texture2D default and the generator infers the real dimension from
		 * the assigned default asset instead of rejecting it.
		 */
		bool bHasExplicitTextureType = false;
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
		bool bHasDefaultValue = false;
		FString DefaultValueText;
	};

	struct FTextShaderGraphRegion
	{
		FString Name;
		int32 StartLine = 1;
		int32 EndLine = MAX_int32;
	};

	struct FTextShaderLayoutNode
	{
		FString Var;
		int32 X = 0;
		int32 Y = 0;
	};

	struct FTextShaderLayoutComment
	{
		FString Name;
		int32 X = 0;
		int32 Y = 0;
		int32 W = 420;
		int32 H = 240;
		FLinearColor Color = FLinearColor(0.10f, 0.16f, 0.22f, 0.35f);
	};

	struct FTextShaderLayout
	{
		TArray<FTextShaderLayoutNode> Nodes;
		TArray<FTextShaderLayoutComment> Comments;
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

	enum class ETextShaderMaterialFunctionKind : uint8
	{
		ShaderFunction,
		MaterialLayer,
		MaterialLayerBlend,
	};

	inline const TCHAR* LexToString(const ETextShaderMaterialFunctionKind Kind)
	{
		switch (Kind)
		{
		case ETextShaderMaterialFunctionKind::MaterialLayer:
			return TEXT("ShaderLayer");
		case ETextShaderMaterialFunctionKind::MaterialLayerBlend:
			return TEXT("ShaderLayerBlend");
		case ETextShaderMaterialFunctionKind::ShaderFunction:
		default:
			return TEXT("ShaderFunction");
		}
	}

	struct FTextShaderMaterialFunctionDefinition
	{
		FString Name;
		FString Root;
		ETextShaderMaterialFunctionKind Kind = ETextShaderMaterialFunctionKind::ShaderFunction;
		TArray<FTextShaderPropertyDefinition> Properties;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Outputs;
		TMap<FString, FString> Settings;
		FString Code;
		int32 CodeStartIndex = INDEX_NONE;
		TArray<FTextShaderGraphRegion> GraphRegions;
		FTextShaderLayout Layout;
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
		int32 CodeStartIndex = INDEX_NONE;
		TArray<FTextShaderGraphRegion> GraphRegions;
		FTextShaderLayout Layout;
		FString HLSL;
		TArray<FTextShaderFunctionDefinition> Functions;
		TArray<FTextShaderFunctionDefinition> GraphFunctions;
		TArray<FTextShaderMaterialFunctionDefinition> MaterialFunctions;
		TArray<FTextShaderVirtualFunctionDefinition> VirtualFunctions;
		TArray<FString> Warnings;

		DREAMSHADER_API bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
		DREAMSHADER_API FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
	};

	DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
}
