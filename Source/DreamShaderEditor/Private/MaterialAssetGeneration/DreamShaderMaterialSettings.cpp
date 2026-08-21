// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader material Settings block: domain/blend/shading-model resolution, the generic
// reflection-driven setting path resolver (Foo.Bar[2] -> FProperty), boolean/typed setting
// validation+application, and the ValidateSettings/ApplySettings entry points. Self-contained:
// extracted byte-for-byte from DreamShaderMaterialGeneratorSupport.cpp with no new exposes (the
// entry points were already header-declared; every helper is settings-local).

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "Misc/Crc.h"

#include "EdGraph/EdGraphNode.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	static bool TryResolveMaterialDomain(const FString& InValue, EMaterialDomain& OutDomain)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveMaterialDomain(InValue, OutDomain);
	}

	static bool TryResolveBlendMode(const FString& InValue, EBlendMode& OutBlendMode)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveBlendMode(InValue, OutBlendMode);
	}

	static bool TryResolveShadingModel(const FString& InValue, EMaterialShadingModel& OutShadingModel)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Settings && Settings->TryResolveShadingModel(InValue, OutShadingModel);
	}

	struct FResolvedMaterialSettingTarget
	{
		UObject* OwnerObject = nullptr;
		void* ContainerPtr = nullptr;
		FProperty* Property = nullptr;
		int32 ArrayIndex = 0;
	};

	struct FParsedMaterialSettingSegment
	{
		FString Name;
		int32 ArrayIndex = 0;
		bool bHasArrayIndex = false;
	};

	static bool TryGetSettingValue(const FTextShaderDefinition& Definition, const TCHAR* Key, FString& OutValue)
	{
		return Definition.TryGetSetting(Key, OutValue);
	}

	static bool TryGetSettingValue(const FTextShaderDefinition& Definition, const TCHAR* KeyA, const TCHAR* KeyB, FString& OutValue)
	{
		return Definition.TryGetSetting(KeyA, OutValue) || Definition.TryGetSetting(KeyB, OutValue);
	}

	static bool ValidateBooleanSetting(const FTextShaderDefinition& Definition, const TCHAR* Key, FString& OutError)
	{
		if (FString Value; TryGetSettingValue(Definition, Key, Value))
		{
			bool bParsedValue = false;
			if (!ParseBooleanLiteral(Value, bParsedValue))
			{
				OutError = FString::Printf(TEXT("Invalid boolean value '%s' for %s."), *Value, Key); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		return true;
	}

	static void ApplyBooleanSetting(UMaterial* Material, const FTextShaderDefinition& Definition, const TCHAR* Key, const TFunctionRef<void(bool)>& Setter)
	{
		if (FString Value; TryGetSettingValue(Definition, Key, Value))
		{
			bool bParsedValue = false;
			verify(ParseBooleanLiteral(Value, bParsedValue));
			Setter(bParsedValue);
		}
	}

	static FString NormalizeMaterialSettingLookupKey(const FString& InKey)
	{
		FString Normalized = UE::DreamShader::NormalizeSettingKey(InKey);
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));
		Normalized.ReplaceInline(TEXT("_"), TEXT(""));
		Normalized.ReplaceInline(TEXT("-"), TEXT(""));
		return Normalized;
	}

	static bool SplitMaterialSettingPath(const FString& InKey, TArray<FString>& OutSegments)
	{
		OutSegments.Reset();

		FString Current;
		int32 BracketDepth = 0;
		for (int32 Index = 0; Index < InKey.Len(); ++Index)
		{
			const TCHAR Character = InKey[Index];
			if (Character == TCHAR('['))
			{
				++BracketDepth;
			}
			else if (Character == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
			}
			else if (Character == TCHAR('.') && BracketDepth == 0)
			{
				OutSegments.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}

			Current.AppendChar(Character);
		}

		if (!Current.IsEmpty())
		{
			OutSegments.Add(Current.TrimStartAndEnd());
		}

		return !OutSegments.IsEmpty();
	}

	static bool ParseMaterialSettingSegment(const FString& InSegmentText, FParsedMaterialSettingSegment& OutSegment, FString& OutError)
	{
		OutSegment = {};
		FString Segment = InSegmentText.TrimStartAndEnd();
		if (Segment.IsEmpty())
		{
			OutError = TEXT("Setting path segment cannot be empty.");
			return false;
		}

		const int32 OpenBracketIndex = Segment.Find(TEXT("["));
		if (OpenBracketIndex == INDEX_NONE)
		{
			OutSegment.Name = Segment;
			return true;
		}

		const int32 CloseBracketIndex = Segment.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (CloseBracketIndex == INDEX_NONE || CloseBracketIndex <= OpenBracketIndex || CloseBracketIndex != Segment.Len() - 1)
		{
			OutError = FString::Printf(TEXT("Invalid array setting segment '%s'."), *InSegmentText); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		const FString IndexText = Segment.Mid(OpenBracketIndex + 1, CloseBracketIndex - OpenBracketIndex - 1).TrimStartAndEnd();
		int32 ParsedIndex = INDEX_NONE;
		if (!ParseIntegerLiteral(IndexText, ParsedIndex) || ParsedIndex < 0)
		{
			OutError = FString::Printf(TEXT("Invalid array index '%s' in setting segment '%s'."), *IndexText, *InSegmentText); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		OutSegment.Name = Segment.Left(OpenBracketIndex).TrimStartAndEnd();
		OutSegment.ArrayIndex = ParsedIndex;
		OutSegment.bHasArrayIndex = true;
		if (OutSegment.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Invalid array setting segment '%s'."), *InSegmentText); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		return true;
	}

	static bool IsSpecialMaterialSettingKey(const FString& InKey)
	{
		const FString Key = NormalizeMaterialSettingLookupKey(InKey);
		return Key == TEXT("blendmode")
			|| Key == TEXT("rendertype")
			|| Key == TEXT("shadingmodel")
			|| Key == TEXT("materialdomain")
			|| Key == TEXT("domain")
			|| Key == TEXT("backend");
	}

	bool TryResolveBlendModeSetting(const FString& InValue, EBlendMode& OutBlendMode)
	{
		return TryResolveBlendMode(InValue, OutBlendMode);
	}

	bool TryResolveShadingModelSetting(const FString& InValue, EMaterialShadingModel& OutShadingModel)
	{
		return TryResolveShadingModel(InValue, OutShadingModel);
	}

	static const TMap<FString, FString>& GetMaterialSettingAliases()
	{
		static const TMap<FString, FString> Aliases = []()
		{
			TMap<FString, FString> Result;
			Result.Add(TEXT("lightingmode"), TEXT("TranslucencyLightingMode"));
			Result.Add(TEXT("translucentlightingmode"), TEXT("TranslucencyLightingMode"));
			Result.Add(TEXT("refractionmode"), TEXT("RefractionMethod"));
			Result.Add(TEXT("physicalmaterial"), TEXT("PhysMaterial"));
			Result.Add(TEXT("physicalmaterialmask"), TEXT("PhysMaterialMask"));
			Result.Add(TEXT("lightmass"), TEXT("LightmassSettings"));
			Result.Add(TEXT("mobileseparatetranslucency"), TEXT("bEnableMobileSeparateTranslucency"));
			Result.Add(TEXT("alwaysevaluateworldpositionoffset"), TEXT("bAlwaysEvaluateWorldPositionOffset"));
			Result.Add(TEXT("responsiveaa"), TEXT("bEnableResponsiveAA"));
			Result.Add(TEXT("thinsurface"), TEXT("bIsThinSurface"));
			return Result;
		}();
		return Aliases;
	}

	static bool TryResolveMaterialSettingPropertyOnStruct(const UStruct* InStruct, const FString& InKey, FProperty*& OutProperty)
	{
		FString LookupKey = NormalizeMaterialSettingLookupKey(InKey);
		if (const FString* Alias = GetMaterialSettingAliases().Find(LookupKey))
		{
			LookupKey = NormalizeMaterialSettingLookupKey(*Alias);
		}

		for (TFieldIterator<FProperty> It(InStruct, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			if (NormalizeMaterialSettingLookupKey(PropertyName) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}

			if (PropertyName.Len() > 1
				&& PropertyName[0] == TCHAR('b')
				&& FChar::IsUpper(PropertyName[1])
				&& NormalizeMaterialSettingLookupKey(PropertyName.Mid(1)) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}

			const FString DisplayName = Property->GetMetaData(TEXT("DisplayName"));
			if (!DisplayName.IsEmpty() && NormalizeMaterialSettingLookupKey(DisplayName) == LookupKey)
			{
				OutProperty = Property;
				return true;
			}
		}

		OutProperty = nullptr;
		return false;
	}

	static bool ResolveMaterialSettingTarget(UObject* RootObject, const FString& InKey, FResolvedMaterialSettingTarget& OutTarget, FString& OutError)
	{
		if (!RootObject)
		{
			OutError = TEXT("Invalid material setting target.");
			return false;
		}

		TArray<FString> Segments;
		if (!SplitMaterialSettingPath(InKey, Segments))
		{
			OutError = FString::Printf(TEXT("Invalid material setting path '%s'."), *InKey); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		void* CurrentContainer = RootObject;
		const UStruct* CurrentStruct = RootObject->GetClass();

		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			FParsedMaterialSettingSegment Segment;
			if (!ParseMaterialSettingSegment(Segments[SegmentIndex], Segment, OutError))
			{
				return false;
			}

			FProperty* Property = nullptr;
			if (!TryResolveMaterialSettingPropertyOnStruct(CurrentStruct, Segment.Name, Property))
			{
				OutError = FString::Printf(TEXT("Unsupported material setting '%s'."), *InKey); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (Segment.bHasArrayIndex)
			{
				if (Property->ArrayDim <= 1)
				{
					OutError = FString::Printf(TEXT("Setting '%s' is not an indexed array property."), *Segments[SegmentIndex]); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				if (Segment.ArrayIndex >= Property->ArrayDim)
				{
					OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Array index %d is out of range for setting '%s' (max %d)."),
						Segment.ArrayIndex,
						*Segments[SegmentIndex],
						Property->ArrayDim - 1);
					return false;
				}
			}
			else if (Property->ArrayDim > 1)
			{
				OutError = FString::Printf(TEXT("Setting '%s' requires an explicit [index]."), *Segments[SegmentIndex]); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			const int32 ArrayIndex = Segment.bHasArrayIndex ? Segment.ArrayIndex : 0;
			if (SegmentIndex == Segments.Num() - 1)
			{
				OutTarget.OwnerObject = RootObject;
				OutTarget.ContainerPtr = CurrentContainer;
				OutTarget.Property = Property;
				OutTarget.ArrayIndex = ArrayIndex;
				return true;
			}

			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (!StructProperty || !StructProperty->Struct)
			{
				OutError = FString::Printf(TEXT("Setting path '%s' cannot continue through '%s'."), *InKey, *Segments[SegmentIndex]); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			CurrentContainer = StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer, ArrayIndex);
			CurrentStruct = StructProperty->Struct;
		}

		OutError = FString::Printf(TEXT("Invalid material setting path '%s'."), *InKey); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return false;
	}

	static bool ValidateGenericMaterialSetting(const FString& InKey, const FString& InValue, FString& OutError)
	{
		UMaterial* ProbeMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!ProbeMaterial)
		{
			OutError = TEXT("Failed to create a transient material for Settings validation.");
			return false;
		}

		FResolvedMaterialSettingTarget Target;
		if (!ResolveMaterialSettingTarget(ProbeMaterial, InKey, Target, OutError))
		{
			return false;
		}

		FString LiteralError;
		void* ValuePtr = Target.Property->ContainerPtrToValuePtr<void>(Target.ContainerPtr, Target.ArrayIndex);
		if (!SetMaterialExpressionLiteralProperty(Target.OwnerObject, Target.Property, ValuePtr, InValue, LiteralError))
		{
			OutError = FString::Printf(TEXT("Invalid value '%s' for setting '%s'. %s"), *InValue, *InKey, *LiteralError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		return true;
	}

	static bool ApplyGenericMaterialSetting(UMaterial* Material, const FString& InKey, const FString& InValue, FString& OutError)
	{
		FResolvedMaterialSettingTarget Target;
		if (!ResolveMaterialSettingTarget(Material, InKey, Target, OutError))
		{
			return false;
		}

		FString LiteralError;
		void* ValuePtr = Target.Property->ContainerPtrToValuePtr<void>(Target.ContainerPtr, Target.ArrayIndex);
		if (!SetMaterialExpressionLiteralProperty(Target.OwnerObject, Target.Property, ValuePtr, InValue, LiteralError))
		{
			OutError = FString::Printf(TEXT("Invalid value '%s' for setting '%s'. %s"), *InValue, *InKey, *LiteralError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		return true;
	}

	bool GetDefaultUsedWithVolumetricCloud(const EMaterialDomain Domain)
	{
		return Domain == MD_Volume;
	}

	bool ValidateSettings(const FTextShaderDefinition& Definition, FString& OutError)
	{
		if (FString BlendModeValue; TryGetSettingValue(Definition, TEXT("BlendMode"), TEXT("RenderType"), BlendModeValue))
		{
			EBlendMode BlendMode = BLEND_MAX;
			if (!TryResolveBlendMode(BlendModeValue, BlendMode))
			{
				OutError = FString::Printf(TEXT("Unsupported BlendMode/RenderType '%s'."), *BlendModeValue); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		if (FString ShadingModelValue; Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue))
		{
#if !DREAMSHADER_WITH_SUBSTRATE_BUILTINS
			const FString TrimmedShadingModelValue = ShadingModelValue.TrimStartAndEnd();
			if (TrimmedShadingModelValue.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase)
				|| TrimmedShadingModelValue.Equals(TEXT("Strata"), ESearchCase::IgnoreCase))
			{
				OutError = TEXT("ShadingModel=\"Substrate\" requires Unreal Engine 5.4 or newer.");
				return false;
			}
#endif
			EMaterialShadingModel ShadingModel = MSM_MAX;
			if (!TryResolveShadingModel(ShadingModelValue, ShadingModel))
			{
				OutError = FString::Printf(TEXT("Unsupported ShadingModel '%s'."), *ShadingModelValue); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		if (FString MaterialDomainValue; TryGetSettingValue(Definition, TEXT("MaterialDomain"), TEXT("Domain"), MaterialDomainValue))
		{
			EMaterialDomain Domain = MD_Surface;
			if (!TryResolveMaterialDomain(MaterialDomainValue, Domain))
			{
				OutError = FString::Printf(TEXT("Unsupported MaterialDomain '%s'."), *MaterialDomainValue); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		for (const TPair<FString, FString>& Setting : Definition.Settings)
		{
			if (IsSpecialMaterialSettingKey(Setting.Key))
			{
				continue;
			}

			if (!ValidateGenericMaterialSetting(Setting.Key, Setting.Value, OutError))
			{
				return false;
			}
		}

		return true;
	}

	bool ApplySettings(UMaterial* Material, const FTextShaderDefinition& Definition, FString& OutError)
	{
		check(Material);

		if (!ValidateSettings(Definition, OutError))
		{
			return false;
		}

		if (FString BlendModeValue; TryGetSettingValue(Definition, TEXT("BlendMode"), TEXT("RenderType"), BlendModeValue))
		{
			EBlendMode BlendMode = BLEND_Opaque;
			verify(TryResolveBlendMode(BlendModeValue, BlendMode));
			Material->BlendMode = BlendMode;
		}

		if (FString ShadingModelValue; Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue))
		{
			EMaterialShadingModel ShadingModel = MSM_DefaultLit;
			verify(TryResolveShadingModel(ShadingModelValue, ShadingModel));
			Material->SetShadingModel(ShadingModel);
		}

		if (FString MaterialDomainValue; TryGetSettingValue(Definition, TEXT("MaterialDomain"), TEXT("Domain"), MaterialDomainValue))
		{
			EMaterialDomain Domain = MD_Surface;
			verify(TryResolveMaterialDomain(MaterialDomainValue, Domain));
			Material->MaterialDomain = Domain;
		}

		// The domain decides this flag's default, so authors never have to write it by hand: a
		// Volume-domain material is a volumetric material, and the volumetric cloud renderer
		// refuses one that is not flagged for it. Assign unconditionally rather than only inside
		// the branch above -- ResetMaterialToDefaults does not clear usage flags, so a material
		// regenerated from Volume to some other domain would otherwise keep a stale flag -- and
		// read the domain back off the material, because a source that omits Domain keeps the
		// domain it already has. The generic Settings loop runs after this and can still override
		// it with an explicit bUsedWithVolumetricCloud, and decompile derives the same default
		// through GetDefaultUsedWithVolumetricCloud so the two directions agree on what may be
		// left out of the Settings block.
		//
		// Which spelling is the portable one flips at 5.8, in both directions at once: before it,
		// SetUsageByFlag is private on UMaterial and the bUsedWith* fields are plain public
		// UPROPERTYs; from 5.8 the accessor is public and ENGINE_API while every bUsedWith* field
		// is UE_DEPRECATED in its favour. So each branch below is the only one that compiles
		// warning-free on its own engines -- caught by the BuildPlugin matrix, since a build
		// against 5.8 alone sees nothing wrong with either.
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
		Material->SetUsageByFlag(MATUSAGE_VolumetricCloud, GetDefaultUsedWithVolumetricCloud(Material->MaterialDomain));
#else
		Material->bUsedWithVolumetricCloud = GetDefaultUsedWithVolumetricCloud(Material->MaterialDomain);
#endif

		for (const TPair<FString, FString>& Setting : Definition.Settings)
		{
			if (IsSpecialMaterialSettingKey(Setting.Key))
			{
				continue;
			}

			if (!ApplyGenericMaterialSetting(Material, Setting.Key, Setting.Value, OutError))
			{
				return false;
			}
		}

		return true;
	}
}
