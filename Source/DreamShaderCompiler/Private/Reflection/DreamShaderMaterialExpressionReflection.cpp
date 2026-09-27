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
		if (!StructProperty || !StructProperty->Struct)
		{
			return false;
		}
		if (StructProperty->Struct->GetName().Equals(TEXT("MaterialAttributesInput"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		// FExpressionInput, and the FMaterialInput family the reflection mirrors beside it: SubstrateShadingModels
		// declares its ShadingModel pin as an FShadingModelMaterialInput. In C++ each of those is an FExpressionInput
		// first, so the property's value is one wherever a pin is looked for.
		static const FName MaterialInputName(TEXT("MaterialInput"));
		for (const UStruct* Struct = StructProperty->Struct; Struct; Struct = Struct->GetSuperStruct())
		{
			if (Struct->GetFName() == NAME_ExpressionInput || Struct->GetFName() == MaterialInputName)
			{
				return true;
			}
		}
		return false;
	}

}
