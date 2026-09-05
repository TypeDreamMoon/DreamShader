#include "DreamShaderSettings.h"

#include "DreamShaderDefineTable.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "UObject/UnrealType.h"

namespace UE::DreamShader::Private
{
	template<typename EnumType>
	static void AddDefaultEnumMapping(
		TMap<FString, TEnumAsByte<EnumType>>& InMappings,
		const FString& Alias,
		const EnumType Value)
	{
		if (!Alias.TrimStartAndEnd().IsEmpty() && !InMappings.Contains(Alias))
		{
			InMappings.Add(Alias, Value);
		}
	}

	template<typename EnumType>
	static void AddDefaultEnumMapping(
		TMap<FString, TEnumAsByte<EnumType>>& InMappings,
		const TCHAR* Alias,
		const EnumType Value)
	{
		AddDefaultEnumMapping(InMappings, FString(Alias), Value);
	}

	static FString BuildAliasFromEnumName(const UEnum* Enum, const int32 Index, const TCHAR* Prefix)
	{
		if (!Enum)
		{
			return FString();
		}

		FString Alias = Enum->GetNameStringByIndex(Index);
		int32 ScopeIndex = INDEX_NONE;
		if (Alias.FindLastChar(TEXT(':'), ScopeIndex))
		{
			Alias = Alias.Mid(ScopeIndex + 1);
		}
		Alias.RemoveFromStart(Prefix, ESearchCase::CaseSensitive);
		return Alias;
	}

	template<typename EnumType>
	static void AddReflectedEnumMappings(
		TMap<FString, TEnumAsByte<EnumType>>& InMappings,
		const UEnum* Enum,
		const TCHAR* Prefix,
		TFunctionRef<bool(int64, const FString&, bool)> ShouldSkip)
	{
		if (!Enum)
		{
			return;
		}

		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			const int64 Value = Enum->GetValueByIndex(Index);
			const FString Name = Enum->GetNameStringByIndex(Index);
			// UEnum metadata is editor-only (declared #if WITH_METADATA == WITH_EDITORONLY_DATA) and is
			// stripped from non-editor / shipping builds -- calling HasMetaData there fails to compile and
			// breaks the whole plugin's store build (issue #12). In non-editor builds no value carries a
			// "Hidden" meta, so treat every value as visible; ShouldSkip still drops values by their name.
			bool bHidden = false;
#if WITH_EDITORONLY_DATA
			bHidden = Enum->HasMetaData(TEXT("Hidden"), Index);
#endif
			if (ShouldSkip(Value, Name, bHidden))
			{
				continue;
			}

			AddDefaultEnumMapping(InMappings, BuildAliasFromEnumName(Enum, Index, Prefix), static_cast<EnumType>(Value));
		}
	}
}

UDreamShaderSettings::UDreamShaderSettings()
{
	using namespace UE::DreamShader::Private;

	SourceDirectory.Path = TEXT("DShader");
	GeneratedShaderDirectory.Path = TEXT("Intermediate/DreamShader/GeneratedShaders");
}

#if WITH_EDITOR
void UDreamShaderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Both names are checked, not just GetPropertyName(). Editing an entry of a TMap reports the
	// INNER property that changed: the value property happens to be generated with the map's own
	// name, but the key property is `<Name>_Key`, so renaming a define would slip straight past a
	// test that only looked at GetPropertyName(). GetMemberPropertyName() is the map itself in both
	// cases, which is what actually answers the question being asked here.
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	const FName DefinesPropertyName = GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, PreprocessorDefines);

	if (PropertyName == DefinesPropertyName || MemberPropertyName == DefinesPropertyName)
	{
		// Deliberately BEFORE Super. UObject::PostEditChangeProperty broadcasts
		// FCoreUObjectDelegates::OnObjectPropertyChanged as its very first statement, and the editor
		// bridge regenerates every source file from that broadcast -- so bumping afterwards would
		// mean that regeneration ran while every define-keyed cache still believed itself current,
		// and the edit would appear to have done nothing until something else invalidated them.
		UE::DreamShader::NotifyDreamShaderDefineSettingsChanged();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FString UDreamShaderSettings::NormalizeMappingKey(const FString& InName)
{
	FString Normalized = InName;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

void UDreamShaderSettings::BuildDefaultShadingModelMappings(
	TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings)
{
	using namespace UE::DreamShader::Private;

	AddReflectedEnumMappings<EMaterialShadingModel>(
		OutMappings,
		StaticEnum<EMaterialShadingModel>(),
		TEXT("MSM_"),
		[](const int64 Value, const FString& Name, const bool bHidden)
		{
			if (Name.Equals(TEXT("Strata"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MSM_Strata"), ESearchCase::CaseSensitive)
				|| Value == static_cast<int64>(MSM_Strata))
			{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
				return false;
#else
				return true;
#endif
			}

			return bHidden
				|| Name.Equals(TEXT("NUM"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MSM_NUM"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MAX"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MSM_MAX"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("FromMaterialExpression"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MSM_FromMaterialExpression"), ESearchCase::CaseSensitive)
				|| Value == static_cast<int64>(MSM_NUM)
				|| Value == static_cast<int64>(MSM_MAX)
				|| Value == static_cast<int64>(MSM_FromMaterialExpression);
		});

	AddDefaultEnumMapping(OutMappings, TEXT("Default Lit"), MSM_DefaultLit);
	AddDefaultEnumMapping(OutMappings, TEXT("Lit"), MSM_DefaultLit);
	AddDefaultEnumMapping(OutMappings, TEXT("Preintegrated Skin"), MSM_PreintegratedSkin);
	AddDefaultEnumMapping(OutMappings, TEXT("Clear Coat"), MSM_ClearCoat);
	AddDefaultEnumMapping(OutMappings, TEXT("Subsurface Profile"), MSM_SubsurfaceProfile);
	AddDefaultEnumMapping(OutMappings, TEXT("Two Sided Foliage"), MSM_TwoSidedFoliage);
	AddDefaultEnumMapping(OutMappings, TEXT("Single Layer Water"), MSM_SingleLayerWater);
	AddDefaultEnumMapping(OutMappings, TEXT("Thin Translucent"), MSM_ThinTranslucent);
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
	AddDefaultEnumMapping(OutMappings, TEXT("Substrate"), MSM_Strata);
	AddDefaultEnumMapping(OutMappings, TEXT("Strata"), MSM_Strata);
#endif
}

void UDreamShaderSettings::BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings)
{
	using namespace UE::DreamShader::Private;

	AddReflectedEnumMappings<EBlendMode>(
		OutMappings,
		StaticEnum<EBlendMode>(),
		TEXT("BLEND_"),
		[](const int64 Value, const FString& Name, const bool bHidden)
		{
			return bHidden
				|| Name.Equals(TEXT("MAX"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("BLEND_MAX"), ESearchCase::CaseSensitive)
				|| Value == static_cast<int64>(BLEND_MAX);
		});

	AddDefaultEnumMapping(OutMappings, TEXT("Cutout"), BLEND_Masked);
	AddDefaultEnumMapping(OutMappings, TEXT("Transparent"), BLEND_Translucent);
	AddDefaultEnumMapping(OutMappings, TEXT("PremultipliedAlpha"), BLEND_AlphaComposite);
	AddDefaultEnumMapping(OutMappings, TEXT("Premultiplied"), BLEND_AlphaComposite);
}

void UDreamShaderSettings::BuildDefaultMaterialDomainMappings(
	TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings)
{
	using namespace UE::DreamShader::Private;

	AddReflectedEnumMappings<EMaterialDomain>(
		OutMappings,
		StaticEnum<EMaterialDomain>(),
		TEXT("MD_"),
		[](const int64 Value, const FString& Name, const bool bHidden)
		{
			return (bHidden && Value != static_cast<int64>(MD_RuntimeVirtualTexture))
				|| Name.Equals(TEXT("MAX"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("MD_MAX"), ESearchCase::CaseSensitive)
				|| Value == static_cast<int64>(MD_MAX);
		});

	AddDefaultEnumMapping(OutMappings, TEXT("DeferredDecal"), MD_DeferredDecal);
	AddDefaultEnumMapping(OutMappings, TEXT("Decal"), MD_DeferredDecal);
	AddDefaultEnumMapping(OutMappings, TEXT("Light Function"), MD_LightFunction);
	AddDefaultEnumMapping(OutMappings, TEXT("Post Process"), MD_PostProcess);
	AddDefaultEnumMapping(OutMappings, TEXT("UserInterface"), MD_UI);
	AddDefaultEnumMapping(OutMappings, TEXT("User Interface"), MD_UI);
	AddDefaultEnumMapping(OutMappings, TEXT("Runtime Virtual Texture"), MD_RuntimeVirtualTexture);
	AddDefaultEnumMapping(OutMappings, TEXT("VirtualTexture"), MD_RuntimeVirtualTexture);
	AddDefaultEnumMapping(OutMappings, TEXT("Virtual Texture"), MD_RuntimeVirtualTexture);
}

FString UDreamShaderSettings::NormalizeShadingModelKey(const FString& InName)
{
	return NormalizeMappingKey(InName);
}

FString UDreamShaderSettings::NormalizeBlendModeKey(const FString& InName)
{
	return NormalizeMappingKey(InName);
}

FString UDreamShaderSettings::NormalizeMaterialDomainKey(const FString& InName)
{
	return NormalizeMappingKey(InName);
}

bool UDreamShaderSettings::TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const
{
	const FString NormalizedInput = NormalizeShadingModelKey(InName);
	for (const TPair<FString, TEnumAsByte<EMaterialShadingModel>>& Pair : ShadingModelMappings)
	{
		if (NormalizeShadingModelKey(Pair.Key) == NormalizedInput)
		{
			OutShadingModel = Pair.Value.GetValue();
			return OutShadingModel != MSM_MAX;
		}
	}

	TMap<FString, TEnumAsByte<EMaterialShadingModel>> DefaultMappings;
	BuildDefaultShadingModelMappings(DefaultMappings);
	for (const TPair<FString, TEnumAsByte<EMaterialShadingModel>>& Pair : DefaultMappings)
	{
		if (NormalizeShadingModelKey(Pair.Key) == NormalizedInput)
		{
			OutShadingModel = Pair.Value.GetValue();
			return OutShadingModel != MSM_MAX;
		}
	}

	return false;
}

bool UDreamShaderSettings::TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const
{
	const FString NormalizedInput = NormalizeBlendModeKey(InName);
	for (const TPair<FString, TEnumAsByte<EBlendMode>>& Pair : BlendModeMappings)
	{
		if (NormalizeBlendModeKey(Pair.Key) == NormalizedInput)
		{
			OutBlendMode = Pair.Value.GetValue();
			return OutBlendMode != BLEND_MAX;
		}
	}

	TMap<FString, TEnumAsByte<EBlendMode>> DefaultMappings;
	BuildDefaultBlendModeMappings(DefaultMappings);
	for (const TPair<FString, TEnumAsByte<EBlendMode>>& Pair : DefaultMappings)
	{
		if (NormalizeBlendModeKey(Pair.Key) == NormalizedInput)
		{
			OutBlendMode = Pair.Value.GetValue();
			return OutBlendMode != BLEND_MAX;
		}
	}

	return false;
}

bool UDreamShaderSettings::TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const
{
	const FString NormalizedInput = NormalizeMaterialDomainKey(InName);
	for (const TPair<FString, TEnumAsByte<EMaterialDomain>>& Pair : MaterialDomainMappings)
	{
		if (NormalizeMaterialDomainKey(Pair.Key) == NormalizedInput)
		{
			OutMaterialDomain = Pair.Value.GetValue();
			return OutMaterialDomain != MD_MAX;
		}
	}

	TMap<FString, TEnumAsByte<EMaterialDomain>> DefaultMappings;
	BuildDefaultMaterialDomainMappings(DefaultMappings);
	for (const TPair<FString, TEnumAsByte<EMaterialDomain>>& Pair : DefaultMappings)
	{
		if (NormalizeMaterialDomainKey(Pair.Key) == NormalizedInput)
		{
			OutMaterialDomain = Pair.Value.GetValue();
			return OutMaterialDomain != MD_MAX;
		}
	}

	return false;
}
