// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Pure DreamShaderLang literal parsers, extracted from DreamShaderMaterialGeneratorSupport.cpp as
// part of decoupling that oversized translation unit. No UObject or graph state — leaf utilities
// shared (in namespace UE::DreamShader::Editor::Private) via DreamShaderMaterialGeneratorPrivate.h.

#include "DreamShaderGeneratedAssets.h"
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
