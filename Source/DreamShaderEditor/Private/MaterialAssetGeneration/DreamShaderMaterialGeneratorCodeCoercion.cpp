// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder::CoerceValueToType (3 overloads): adapt an FCodeValue to an expected component
// count / texture / Substrate / MaterialAttributes shape (truncate-forbidden masks, scalar splat,
// AppendVector growth). Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp;
// member declarations stay in the FCodeGraphBuilder class header.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::CoerceValueToType(
		const FCodeValue& InValue,
		const int32 ExpectedComponentCount,
		const bool bExpectedTexture,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		return CoerceValueToType(InValue, ExpectedComponentCount, bExpectedTexture, ETextShaderTextureType::Texture2D, OutValue, OutError);
	}

	bool FCodeGraphBuilder::CoerceValueToType(
		const FCodeValue& InValue,
		const int32 ExpectedComponentCount,
		const bool bExpectedTexture,
		const ETextShaderTextureType ExpectedTextureType,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		return CoerceValueToType(InValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, false, OutValue, OutError);
	}

	bool FCodeGraphBuilder::CoerceValueToType(
		const FCodeValue& InValue,
		const int32 ExpectedComponentCount,
		const bool bExpectedTexture,
		const ETextShaderTextureType ExpectedTextureType,
		const bool bExpectedSubstrateMaterial,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		if (IsMaterialAttributesComponentType(ExpectedComponentCount, bExpectedTexture, bExpectedSubstrateMaterial))
		{
			if (!InValue.bIsMaterialAttributes)
			{
				OutError = TEXT("Expected a MaterialAttributes value.");
				return false;
			}

			OutValue = InValue;
			return true;
		}

		if (bExpectedSubstrateMaterial)
		{
			if (!InValue.bIsSubstrateMaterial)
			{
				OutError = TEXT("Expected a Substrate value.");
				return false;
			}

			OutValue = InValue;
			return true;
		}

		if (bExpectedTexture)
		{
			if (!InValue.bIsTextureObject)
			{
				OutError = TEXT("Expected a texture object value.");
				return false;
			}

			if (InValue.TextureType != ExpectedTextureType)
			{
				OutError = TEXT("Expected a texture object value with a matching texture type.");
				return false;
			}

			OutValue = InValue;
			return true;
		}

		if (InValue.bIsMaterialAttributes)
		{
			OutError = TEXT("MaterialAttributes values cannot be assigned to numeric outputs.");
			return false;
		}

		if (InValue.bIsSubstrateMaterial)
		{
			OutError = TEXT("Substrate values cannot be assigned to numeric outputs.");
			return false;
		}

		if (InValue.bIsTextureObject)
		{
			OutError = TEXT("Texture objects cannot be assigned to numeric outputs.");
			return false;
		}

		if (InValue.ComponentCount == ExpectedComponentCount)
		{
			OutValue = InValue;
			return true;
		}

		if (ExpectedComponentCount > 0 && InValue.ComponentCount > ExpectedComponentCount)
		{
			static const TCHAR* LeadingSwizzles[] = { TEXT(""), TEXT("r"), TEXT("rg"), TEXT("rgb"), TEXT("rgba") };
			check(ExpectedComponentCount < UE_ARRAY_COUNT(LeadingSwizzles));
			return CreateSwizzleExpression(InValue, LeadingSwizzles[ExpectedComponentCount], OutValue, OutError);
		}

		if (ExpectedComponentCount > 1 && InValue.ComponentCount == 1)
		{
			TArray<FCodeValue> ReplicatedParts;
			ReplicatedParts.Reserve(ExpectedComponentCount);
			for (int32 Index = 0; Index < ExpectedComponentCount; ++Index)
			{
				ReplicatedParts.Add(InValue);
			}

			if (!AppendValues(ReplicatedParts, OutValue, OutError))
			{
				return false;
			}

			OutValue.ComponentCount = ExpectedComponentCount;
			return true;
		}

		OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("Expected %d component(s) but got %d."),
			ExpectedComponentCount,
			InValue.ComponentCount);
		return false;
	}
}
