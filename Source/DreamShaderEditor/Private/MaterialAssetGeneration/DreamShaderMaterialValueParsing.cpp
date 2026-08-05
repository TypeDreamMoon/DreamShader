// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Pure DreamShaderLang literal parsers, extracted from DreamShaderMaterialGeneratorSupport.cpp as
// part of decoupling that oversized translation unit. No UObject or graph state — leaf utilities
// shared (in namespace UE::DreamShader::Editor::Private) via DreamShaderMaterialGeneratorPrivate.h.

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderVersionCompat.h"
#include "DreamShaderModule.h"

namespace UE::DreamShader::Editor::Private
{
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

	bool ParseUnsignedInteger32Literal(const FString& InText, uint32& OutValue)
	{
		const FString Candidate = InText.TrimStartAndEnd();
		int64 Tmp = 0;
		if (!LexTryParseString(Tmp, *Candidate) || Tmp < 0 || Tmp > static_cast<int64>(MAX_uint32))
		{
			return false;
		}
		OutValue = static_cast<uint32>(Tmp);
		return true;
	}

	bool ParseVectorLiteral(const FString& InText, TArray<double>& OutValues)
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
		else if (Matches(TEXT("MaterialAttributes")) || Matches(TEXT("Attributes")))
		{
			OutProperty = { MP_MaterialAttributes, CMOT_MaterialAttributes };
		}
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		else if (Matches(TEXT("FrontMaterial")))
		{
			OutProperty = { MP_FrontMaterial, CMOT_Float1, true };
		}
#endif
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
#if DREAMSHADER_WITH_MOON_ENGINE
		// Both spellings resolve: the engine renamed these from Mooa to Moon, and sources written
		// before that rename -- decompiled MooaToon material functions, for one -- still say Mooa.
		// Only the Moon spelling is ever emitted; see the decompiler's property-name table.
		else if (Matches(TEXT("MoonEncodedAttribute0")) || Matches(TEXT("MooaEncodedAttribute0")))
		{
			OutProperty = { MP_MoonEncodedAttribute0, CMOT_Float4 };
		}
		else if (Matches(TEXT("MoonEncodedAttribute1")) || Matches(TEXT("MooaEncodedAttribute1")))
		{
			OutProperty = { MP_MoonEncodedAttribute1, CMOT_Float4 };
		}
		else if (Matches(TEXT("MoonEncodedAttribute2")) || Matches(TEXT("MooaEncodedAttribute2")))
		{
			OutProperty = { MP_MoonEncodedAttribute2, CMOT_Float4 };
		}
		else if (Matches(TEXT("MoonEncodedAttribute3")) || Matches(TEXT("MooaEncodedAttribute3")))
		{
			OutProperty = { MP_MoonEncodedAttribute3, CMOT_Float4 };
		}
		else if (Matches(TEXT("MoonEncodedAttribute4")) || Matches(TEXT("MooaEncodedAttribute4")))
		{
			OutProperty = { MP_MoonEncodedAttribute4, CMOT_Float4 };
		}
#endif
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

	bool TryResolveWorldPositionShaderOffset(const FString& InValue, EWorldPositionIncludedOffsets& OutValue)
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

	FString NormalizeEnumLookupKey(const FString& InKey)
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

	bool TryResolveEnumLiteral(UEnum* Enum, const FString& InValue, int64& OutEnumValue)
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
}
