// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderGeneratedAssetDigest.h for what the digest is for. This file is the policy: what
// counts as content, and therefore what counts as a hand edit.
//
// The governing principle is "the digest covers exactly what regeneration would destroy". A property
// regeneration never touches -- a preview mesh, a thumbnail angle, a physical material -- survives a
// rebuild untouched, so flagging it as divergence would refuse a rebuild in order to protect
// something that was never in danger. Conversely everything the rebuild resets or recreates is in,
// because that is precisely the set a user can lose without being asked.

#include "DreamShaderGeneratedAssetDigest.h"

#include "DreamShaderMaterialGeneratorCodeShared.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstance.h"
#include "Misc/Crc.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// Bumped whenever the text this file produces changes shape. Paired with the engine version
		// because the property set the reflection walk sees is the engine's, not ours: an engine
		// upgrade that adds one UPROPERTY to one expression class would otherwise re-fingerprint every
		// asset in the project at once and report the whole library as hand-edited.
		constexpr const TCHAR* DigestFormatVersion = TEXT("DSD1");

		// Node properties a user is free to change without meaning anything by it. Node coordinates are
		// the important entry: regeneration reassigns them from the Layout section anyway (they are
		// documented as not preserved), so a dragged node is not an edit to the material.
		//
		// NodeColor is here for a sharper reason, found by the clean-rebuild test rather than by
		// reading: a named reroute seeds its display colour from its own PATH NAME
		// (MaterialExpressions.cpp, CookDeterminism::MakeRandomColor). Regeneration deletes and
		// recreates the node, which lands on a new numeric suffix, which repaints it -- so leaving it
		// in made two rebuilds of one unchanged source disagree, and every material in the project
		// would have reported itself hand-edited the moment it was rebuilt.
		bool IsCosmeticDigestPropertyName(const FString& Name)
		{
			const FString NormalizedName = UE::DreamShader::NormalizeSettingKey(Name);
			return NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("NodeColor"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("MaterialExpressionEditorX"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("MaterialExpressionEditorY"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bCommentBubbleVisible"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bCollapsed"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bHidePreviewWindow"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bShowOutputNameOnPin"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bRealtimePreview"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Desc"));
		}

		bool IsDigestSafeStructInner(const UScriptStruct* Struct, int32 Depth);

		bool IsDigestSafeProperty(const FProperty* Property, const int32 Depth)
		{
			if (!Property)
			{
				return false;
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				return IsDigestSafeStructInner(StructProperty->Struct, Depth + 1);
			}

			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				return IsDigestSafeProperty(ArrayProperty->Inner, Depth);
			}

			if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
			{
				return IsDigestSafeProperty(SetProperty->ElementProp, Depth);
			}

			if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
			{
				return IsDigestSafeProperty(MapProperty->KeyProp, Depth)
					&& IsDigestSafeProperty(MapProperty->ValueProp, Depth);
			}

			return true;
		}

		bool IsDigestSafeStructInner(const UScriptStruct* Struct, const int32 Depth)
		{
			if (!Struct)
			{
				return false;
			}

			// A struct nested this deep inside another is not something a material expression exposes
			// as a value; refusing it costs nothing and bounds the walk.
			if (Depth > 4)
			{
				return false;
			}

			const FName StructName = Struct->GetFName();
			// An expression input reaches the connected expression by pointer, and its exported text is
			// that object's path. Digesting it would make renaming or moving the package read as an
			// edit to every node in it. Connections are digested structurally instead, below.
			if (StructName == NAME_ExpressionInput
				|| Struct->GetName().Equals(TEXT("MaterialAttributesInput"), ESearchCase::IgnoreCase))
			{
				return false;
			}
			// Pin ids and named-reroute variable ids are deliberately carried across a regeneration
			// (that is what keeps call sites wired), so they are identity, not content.
			if (Struct == TBaseStructure<FGuid>::Get())
			{
				return false;
			}

			static TMap<const UScriptStruct*, bool> SafeStructCache;
			if (const bool* Cached = SafeStructCache.Find(Struct))
			{
				return *Cached;
			}

			// Seed the cache before recursing so a self-referential struct terminates.
			SafeStructCache.Add(Struct, true);

			bool bSafe = true;
			for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				if (!IsDigestSafeProperty(*It, Depth))
				{
					bSafe = false;
					break;
				}
			}

			SafeStructCache.Add(Struct, bSafe);
			return bSafe;
		}

		// One property's value as digest text, or false if the property does not contribute.
		bool TryBuildDigestPropertyValue(const UObject* Object, const FProperty* Property, FString& OutValue)
		{
			if (!Object || !IsDigestProperty(Property))
			{
				return false;
			}

			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
			if (!ValuePtr)
			{
				return false;
			}

			// FText exports with its namespace/key envelope, which is not stable enough to compare
			// across sessions; the display string is the part a user can actually edit.
			if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				OutValue = TextProperty->GetPropertyValue(ValuePtr).ToString();
				return true;
			}

			// Object references digest as their path, so swapping a texture registers while the
			// pointer value (which differs every session) does not.
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				const UObject* Referenced = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				OutValue = Referenced ? Referenced->GetPathName() : TEXT("None");
				return true;
			}

			OutValue.Reset();
			Property->ExportTextItem_Direct(OutValue, ValuePtr, nullptr, nullptr, PPF_None);
			return true;
		}

		void AppendObjectProperties(const UObject* Object, const TCHAR* LinePrefix, FString& InOutText)
		{
			if (!Object)
			{
				return;
			}

			// Sorted by name: the reflection iterator walks a class's own properties before its
			// super's, so relying on its order would make the digest depend on where in a hierarchy a
			// property happens to be declared.
			TArray<TPair<FString, FString>> Values;
			for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				FString Value;
				if (TryBuildDigestPropertyValue(Object, *It, Value))
				{
					Values.Emplace(It->GetName(), MoveTemp(Value));
				}
			}

			Values.Sort([](const TPair<FString, FString>& Left, const TPair<FString, FString>& Right)
			{
				return Left.Key < Right.Key;
			});

			for (const TPair<FString, FString>& Value : Values)
			{
				InOutText += FString::Printf(TEXT("%s %s=%s\n"), LinePrefix, *Value.Key, *Value.Value);
			}
		}

		FString MakeConnectionText(const FExpressionInput* Input, const TMap<UMaterialExpression*, int32>& IndexByExpression)
		{
			if (!Input || !Input->Expression)
			{
				return TEXT("-");
			}

			const int32* SourceIndex = IndexByExpression.Find(Input->Expression);
			return FString::Printf(
				TEXT("%d:%d:%d%d%d%d%d"),
				SourceIndex ? *SourceIndex : INDEX_NONE,
				Input->OutputIndex,
				Input->Mask,
				Input->MaskR,
				Input->MaskG,
				Input->MaskB,
				Input->MaskA);
		}

		// The graph body, shared by materials and material functions: every node, its digest
		// properties, and every connection into it addressed by node index.
		void AppendExpressionGraph(
			TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,
			const TMap<UMaterialExpression*, int32>& IndexByExpression,
			FString& InOutText)
		{
			for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
			{
				UMaterialExpression* Expression = Expressions[ExpressionIndex].Get();
				if (!Expression)
				{
					InOutText += FString::Printf(TEXT("NODE %d <null>\n"), ExpressionIndex);
					continue;
				}

				InOutText += FString::Printf(
					TEXT("NODE %d %s\n"),
					ExpressionIndex,
					*Expression->GetClass()->GetPathName());

				AppendObjectProperties(Expression, TEXT(" ARG"), InOutText);

				const int32 InputCount = GetDreamShaderExpressionInputCount(Expression);
				for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
				{
					const FExpressionInput* Input = Expression->GetInput(InputIndex);
					const FName InputName = Expression->GetInputName(InputIndex);
					InOutText += FString::Printf(
						TEXT(" IN %d %s=%s\n"),
						InputIndex,
						InputName.IsNone() ? TEXT("-") : *InputName.ToString(),
						*MakeConnectionText(Input, IndexByExpression));
				}
			}
		}

		TMap<UMaterialExpression*, int32> BuildExpressionIndex(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
		{
			TMap<UMaterialExpression*, int32> IndexByExpression;
			IndexByExpression.Reserve(Expressions.Num());
			for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
			{
				if (UMaterialExpression* Expression = Expressions[ExpressionIndex].Get())
				{
					IndexByExpression.Add(Expression, ExpressionIndex);
				}
			}
			return IndexByExpression;
		}

		// The material render state DreamShader owns. Read as typed fields rather than by name so that
		// this list and ResetMaterialToDefaults (DreamShaderMaterialGeneratorSupport.cpp), which is the
		// set of properties a rebuild restores, cannot drift apart without a compile error. Everything
		// absent here is untouched by a rebuild and therefore not a hand edit worth blocking on.
		void AppendMaterialSettings(UMaterial* Material, FString& InOutText)
		{
			InOutText += FString::Printf(TEXT("SET BlendMode=%d\n"), static_cast<int32>(Material->BlendMode.GetValue()));
			InOutText += FString::Printf(TEXT("SET MaterialDomain=%d\n"), static_cast<int32>(Material->MaterialDomain.GetValue()));
			InOutText += FString::Printf(TEXT("SET ShadingModels=%u\n"), Material->GetShadingModels().GetShadingModelField());
			InOutText += FString::Printf(TEXT("SET TwoSided=%d\n"), Material->TwoSided ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET OpacityMaskClipValue=%s\n"), *FString::SanitizeFloat(Material->OpacityMaskClipValue));
			InOutText += FString::Printf(TEXT("SET Wireframe=%d\n"), Material->Wireframe ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET DitheredLODTransition=%d\n"), Material->DitheredLODTransition ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET DitherOpacityMask=%d\n"), Material->DitherOpacityMask ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bAllowNegativeEmissiveColor=%d\n"), Material->bAllowNegativeEmissiveColor ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bCastDynamicShadowAsMasked=%d\n"), Material->bCastDynamicShadowAsMasked ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bCastRayTracedShadows=%d\n"), Material->bCastRayTracedShadows ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bEnableResponsiveAA=%d\n"), Material->bEnableResponsiveAA ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bScreenSpaceReflections=%d\n"), Material->bScreenSpaceReflections ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bContactShadows=%d\n"), Material->bContactShadows ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bDisableDepthTest=%d\n"), Material->bDisableDepthTest ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bOutputTranslucentVelocity=%d\n"), Material->bOutputTranslucentVelocity ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bWriteOnlyAlpha=%d\n"), Material->bWriteOnlyAlpha ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET BlendableOutputAlpha=%d\n"), Material->BlendableOutputAlpha ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET TranslucencyLightingMode=%d\n"), static_cast<int32>(Material->TranslucencyLightingMode.GetValue()));
			InOutText += FString::Printf(TEXT("SET bTangentSpaceNormal=%d\n"), Material->bTangentSpaceNormal ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bAlwaysEvaluateWorldPositionOffset=%d\n"), Material->bAlwaysEvaluateWorldPositionOffset ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bFullyRough=%d\n"), Material->bFullyRough ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bIsSky=%d\n"), Material->bIsSky ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET bIsThinSurface=%d\n"), Material->bIsThinSurface ? 1 : 0);
			InOutText += FString::Printf(TEXT("SET MaterialDecalResponse=%d\n"), static_cast<int32>(Material->MaterialDecalResponse.GetValue()));
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
			InOutText += FString::Printf(TEXT("SET bHasPixelAnimation=%d\n"), Material->bHasPixelAnimation ? 1 : 0);
#endif
			InOutText += FString::Printf(TEXT("SET NumCustomizedUVs=%d\n"), Material->NumCustomizedUVs);
		}
	}

	bool IsDigestSafeStruct(const UScriptStruct* Struct)
	{
		return IsDigestSafeStructInner(Struct, 0);
	}

	bool IsDigestProperty(const FProperty* Property)
	{
		if (!Property
			|| Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient)
			|| !Property->HasAnyPropertyFlags(CPF_Edit)
			|| IsMaterialExpressionInputProperty(Property)
			|| IsCosmeticDigestPropertyName(Property->GetName()))
		{
			return false;
		}

		return IsDigestSafeProperty(Property, 0);
	}

	FString MakeDigestSchemaTag()
	{
		return FString::Printf(TEXT("%s-%d.%d"), DigestFormatVersion, DREAMSHADER_UE_MAJOR, DREAMSHADER_UE_MINOR);
	}

	FString BuildMaterialDigestText(UMaterial* Material)
	{
		if (!Material)
		{
			return FString();
		}

		FString Text = TEXT("KIND Material\n");
		AppendMaterialSettings(Material, Text);

		TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
		const TMap<UMaterialExpression*, int32> IndexByExpression = BuildExpressionIndex(Expressions);
		Text += FString::Printf(TEXT("NODES %d\n"), Expressions.Num());
		AppendExpressionGraph(Expressions, IndexByExpression, Text);

		// The material property inputs the Outputs block wires. Nulled and rebuilt on every
		// regeneration, so a rewired output is squarely a hand edit.
		for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
		{
			const FExpressionInput* Input =
				Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(MaterialPropertyIndex));
			if (!Input || !Input->Expression)
			{
				continue;
			}

			Text += FString::Printf(
				TEXT("OUT %d=%s\n"),
				MaterialPropertyIndex,
				*MakeConnectionText(Input, IndexByExpression));
		}

		return Text;
	}

	FString BuildMaterialFunctionDigestText(UMaterialFunction* MaterialFunction)
	{
		if (!MaterialFunction)
		{
			return FString();
		}

		// The asset-level fields regeneration reapplies from the source block's settings; anything
		// absent from the source is cleared, so all four are destroyable state.
		FString Text = TEXT("KIND MaterialFunction\n");
		Text += FString::Printf(TEXT("FN Usage=%d\n"), static_cast<int32>(MaterialFunction->GetMaterialFunctionUsage()));
		Text += FString::Printf(TEXT("FN Description=%s\n"), *MaterialFunction->Description);
		Text += FString::Printf(TEXT("FN ExposeToLibrary=%d\n"), MaterialFunction->bExposeToLibrary ? 1 : 0);
		Text += FString::Printf(TEXT("FN UserExposedCaption=%s\n"), *MaterialFunction->GetUserExposedCaption());
		for (const FText& Category : MaterialFunction->LibraryCategoriesText)
		{
			Text += FString::Printf(TEXT("FN Category=%s\n"), *Category.ToString());
		}

		TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = MaterialFunction->GetExpressions();
		const TMap<UMaterialExpression*, int32> IndexByExpression = BuildExpressionIndex(Expressions);
		Text += FString::Printf(TEXT("NODES %d\n"), Expressions.Num());
		AppendExpressionGraph(Expressions, IndexByExpression, Text);

		return Text;
	}

	FString BuildMaterialInstanceDigestText(UMaterialInstance* Instance)
	{
		if (!Instance)
		{
			return FString();
		}

		FString Text = TEXT("KIND MaterialInstance\n");
		Text += FString::Printf(TEXT("MI Parent=%s\n"), Instance->Parent ? *Instance->Parent->GetPathName() : TEXT("None"));

		// Every override array in one reflection sweep rather than a hand-written list: the set grows
		// between engine versions (texture collections, sparse volume textures), and a missed array
		// would be a parameter a user can tune and silently lose. Regeneration calls
		// ClearParameterValuesEditorOnly, so on a freshly generated instance every one of these is
		// empty -- any content at all is somebody's hand edit.
		TArray<TPair<FString, FString>> ParameterValues;
		for (TFieldIterator<FProperty> It(Instance->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property
				|| !CastField<FArrayProperty>(Property)
				|| !Property->GetName().EndsWith(TEXT("ParameterValues"), ESearchCase::CaseSensitive))
			{
				continue;
			}

			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Instance);
			if (!ValuePtr)
			{
				continue;
			}

			FString Value;
			Property->ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);
			ParameterValues.Emplace(Property->GetName(), MoveTemp(Value));
		}

		ParameterValues.Sort([](const TPair<FString, FString>& Left, const TPair<FString, FString>& Right)
		{
			return Left.Key < Right.Key;
		});
		for (const TPair<FString, FString>& Value : ParameterValues)
		{
			Text += FString::Printf(TEXT("MI %s=%s\n"), *Value.Key, *Value.Value);
		}

		const FStaticParameterSet& StaticParameters = Instance->GetStaticParameters();
		for (const FStaticSwitchParameter& Switch : StaticParameters.StaticSwitchParameters)
		{
			Text += FString::Printf(
				TEXT("MI Switch %s=%d/%d\n"),
				*Switch.ParameterInfo.Name.ToString(),
				Switch.Value ? 1 : 0,
				Switch.bOverride ? 1 : 0);
		}
		for (const FStaticComponentMaskParameter& Mask : StaticParameters.EditorOnly.StaticComponentMaskParameters)
		{
			Text += FString::Printf(
				TEXT("MI Mask %s=%d%d%d%d/%d\n"),
				*Mask.ParameterInfo.Name.ToString(),
				Mask.R ? 1 : 0,
				Mask.G ? 1 : 0,
				Mask.B ? 1 : 0,
				Mask.A ? 1 : 0,
				Mask.bOverride ? 1 : 0);
		}

		// The ThinCustom pair is one unit: the graph lives on the hidden base, so an edit made there
		// has to register against the instance, which is the asset the ownership metadata sits on.
		if (UMaterial* BaseMaterial = Cast<UMaterial>(Instance->Parent))
		{
			if (BaseMaterial->GetOutermost() == Instance->GetOutermost())
			{
				Text += TEXT("BASE\n");
				Text += BuildMaterialDigestText(BaseMaterial);
			}
		}

		return Text;
	}

	FString BuildOutputDigestText(UObject* Asset)
	{
		// Instance first: UDreamShaderMaterialInstance is a UMaterialInstance, never a UMaterial, but
		// checking in the other order would be a live trap for whoever adds the next asset class.
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Asset))
		{
			return BuildMaterialInstanceDigestText(Instance);
		}
		if (UMaterial* Material = Cast<UMaterial>(Asset))
		{
			return BuildMaterialDigestText(Material);
		}
		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Asset))
		{
			return BuildMaterialFunctionDigestText(MaterialFunction);
		}
		return FString();
	}

	FString BuildOutputDigest(UObject* Asset)
	{
		const FString DigestText = BuildOutputDigestText(Asset);
		if (DigestText.IsEmpty())
		{
			return FString();
		}

		return FString::Printf(TEXT("%s:%08x"), *MakeDigestSchemaTag(), FCrc::StrCrc32(*DigestText));
	}
}
