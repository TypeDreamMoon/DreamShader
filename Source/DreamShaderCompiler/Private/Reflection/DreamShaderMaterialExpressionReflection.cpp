// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Reflection helpers for resolving UMaterialExpression classes and their editable argument
// properties by name. Pure UE reflection lookups, no graph state. Extracted from
// DreamShaderMaterialGeneratorSupport.cpp; all three entry points stay declared in the private header.

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "Materials/MaterialExpression.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace UE::DreamShader::Editor::Private
{
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

		for (TFieldIterator<FProperty> It(ExpressionClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!CastField<FBoolProperty>(Property))
			{
				continue;
			}

			FString NormalizedPropertyName = UE::DreamShader::NormalizeSettingKey(Property->GetName());
			if (NormalizedPropertyName.StartsWith(TEXT("b")))
			{
				NormalizedPropertyName.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
				if (NormalizedPropertyName == NormalizedArgument)
				{
					return Property;
				}
			}
		}

		return nullptr;
	}

	bool IsMaterialExpressionInputProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty
			&& StructProperty->Struct
			&& (StructProperty->Struct->GetFName() == NAME_ExpressionInput
				|| StructProperty->Struct->GetName().Equals(TEXT("MaterialAttributesInput"), ESearchCase::IgnoreCase));
	}

}
