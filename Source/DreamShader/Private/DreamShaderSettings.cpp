#include "DreamShaderSettings.h"

#include "DreamShaderModule.h"

namespace UE::DreamShader::Private
{
	static void AddDefaultShadingModelMapping(
		TMap<FString, TEnumAsByte<EMaterialShadingModel>>& InMappings,
		const TCHAR* Alias,
		const EMaterialShadingModel ShadingModel)
	{
		InMappings.Add(Alias, ShadingModel);
	}

	static void AddDefaultBlendModeMapping(
		TMap<FString, TEnumAsByte<EBlendMode>>& InMappings,
		const TCHAR* Alias,
		const EBlendMode BlendMode)
	{
		InMappings.Add(Alias, BlendMode);
	}

	static void AddDefaultMaterialDomainMapping(
		TMap<FString, TEnumAsByte<EMaterialDomain>>& InMappings,
		const TCHAR* Alias,
		const EMaterialDomain MaterialDomain)
	{
		InMappings.Add(Alias, MaterialDomain);
	}
}

UDreamShaderSettings::UDreamShaderSettings()
{
	using namespace UE::DreamShader::Private;

	SourceDirectory.Path = TEXT("DShader");
	BuiltinLibraryDirectory.Path = TEXT("Plugins/DreamShader/Library");
	GeneratedShaderDirectory.Path = TEXT("Intermediate/DreamShader/GeneratedShaders");

	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Unlit"), MSM_Unlit);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("DefaultLit"), MSM_DefaultLit);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Default Lit"), MSM_DefaultLit);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Lit"), MSM_DefaultLit);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Subsurface"), MSM_Subsurface);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("PreintegratedSkin"), MSM_PreintegratedSkin);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Preintegrated Skin"), MSM_PreintegratedSkin);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("ClearCoat"), MSM_ClearCoat);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Clear Coat"), MSM_ClearCoat);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("SubsurfaceProfile"), MSM_SubsurfaceProfile);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Subsurface Profile"), MSM_SubsurfaceProfile);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("TwoSidedFoliage"), MSM_TwoSidedFoliage);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Two Sided Foliage"), MSM_TwoSidedFoliage);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Hair"), MSM_Hair);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Cloth"), MSM_Cloth);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Eye"), MSM_Eye);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("SingleLayerWater"), MSM_SingleLayerWater);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Single Layer Water"), MSM_SingleLayerWater);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("ThinTranslucent"), MSM_ThinTranslucent);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Thin Translucent"), MSM_ThinTranslucent);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Substrate"), MSM_Strata);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Strata"), MSM_Strata);
	AddDefaultShadingModelMapping(ShadingModelMappings, TEXT("Toon"), MSM_Toon);

	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Opaque"), BLEND_Opaque);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Masked"), BLEND_Masked);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Cutout"), BLEND_Masked);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Translucent"), BLEND_Translucent);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Transparent"), BLEND_Translucent);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Additive"), BLEND_Additive);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Modulate"), BLEND_Modulate);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("AlphaComposite"), BLEND_AlphaComposite);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("PremultipliedAlpha"), BLEND_AlphaComposite);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("Premultiplied"), BLEND_AlphaComposite);
	AddDefaultBlendModeMapping(BlendModeMappings, TEXT("AlphaHoldout"), BLEND_AlphaHoldout);

	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Surface"), MD_Surface);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("DeferredDecal"), MD_DeferredDecal);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Decal"), MD_DeferredDecal);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("LightFunction"), MD_LightFunction);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Light Function"), MD_LightFunction);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Volume"), MD_Volume);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("PostProcess"), MD_PostProcess);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Post Process"), MD_PostProcess);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("UI"), MD_UI);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("UserInterface"), MD_UI);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("User Interface"), MD_UI);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("RuntimeVirtualTexture"), MD_RuntimeVirtualTexture);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Runtime Virtual Texture"), MD_RuntimeVirtualTexture);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("VirtualTexture"), MD_RuntimeVirtualTexture);
	AddDefaultMaterialDomainMapping(MaterialDomainMappings, TEXT("Virtual Texture"), MD_RuntimeVirtualTexture);
}

FString UDreamShaderSettings::NormalizeShadingModelKey(const FString& InName)
{
	FString Normalized = InName;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

FString UDreamShaderSettings::NormalizeBlendModeKey(const FString& InName)
{
	FString Normalized = InName;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

FString UDreamShaderSettings::NormalizeMaterialDomainKey(const FString& InName)
{
	FString Normalized = InName;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
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

	return false;
}
