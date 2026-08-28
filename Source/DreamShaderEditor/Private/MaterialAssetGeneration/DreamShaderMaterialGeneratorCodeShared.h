#pragma once

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "Engine/Texture2DArray.h"
#include "Engine/VolumeTexture.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialValueType.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	inline FString NormalizeCodeReuseLiteralText(FString Text)
	{
		Text.TrimStartAndEndInline();
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		while (Text.Contains(TEXT("  ")))
		{
			Text.ReplaceInline(TEXT("  "), TEXT(" "));
		}
		return Text;
	}

	// Mirror of UMaterialGraph::GetValidOutputIndex (Engine/Source/Editor/UnrealEd/Private/MaterialGraph.cpp).
	// The material editor rebuilds every wire through that function when it opens a graph, so a connection
	// whose stored OutputIndex disagrees with the result is silently re-pointed at another pin -- and the
	// next Apply/Save writes that pin back through UMaterialExpression::ConnectExpression, which replaces
	// the inline component mask with the pin's own mask. Generated assets must only carry inline masks this
	// function leaves alone; see IsInlineInputMaskGraphStable.
	inline int32 ResolveGraphEditorOutputIndex(
		const UMaterialExpression* Expression,
		const int32 OutputIndex,
		const int32 Mask,
		const int32 MaskR,
		const int32 MaskG,
		const int32 MaskB,
		const int32 MaskA)
	{
		if (!Expression || Expression->Outputs.Num() == 0)
		{
			return 0;
		}

		// The engine distrusts OutputIndex 0 whenever the input carries a mask, because that combination
		// used to mean "pre-OutputIndex legacy connection".
		const bool bOutputIndexIsValid = Expression->Outputs.IsValidIndex(OutputIndex)
			&& (OutputIndex != 0 || Mask == 0);

		int32 ResolvedIndex = 0;
		for (; ResolvedIndex < Expression->Outputs.Num(); ++ResolvedIndex)
		{
			const FExpressionOutput& Output = Expression->Outputs[ResolvedIndex];
			if ((bOutputIndexIsValid && ResolvedIndex == OutputIndex)
				|| (!bOutputIndexIsValid
					&& Output.Mask == Mask
					&& Output.MaskR == MaskR
					&& Output.MaskG == MaskG
					&& Output.MaskB == MaskB
					&& Output.MaskA == MaskA))
			{
				break;
			}
		}

		if (ResolvedIndex >= Expression->Outputs.Num())
		{
			// The engine falls back to the last output when nothing matches -- this is the step that turns
			// "SceneTexture Color.r" into "SceneTexture InvSize".
			ResolvedIndex = Expression->Outputs.Num() - 1;
		}

		return ResolvedIndex;
	}

	// True when a connection carrying ChannelMask survives a material-editor round trip untouched: the graph
	// re-points it at the pin it already names, so LinkMaterialExpressionsFromGraph's IsExpressionConnected
	// check short-circuits and the mask is never rewritten.
	inline bool IsInlineInputMaskGraphStable(
		const UMaterialExpression* Expression,
		const int32 OutputIndex,
		const int32 ChannelMask)
	{
		if (ChannelMask == 0)
		{
			return true;
		}

		return ResolveGraphEditorOutputIndex(
			Expression,
			OutputIndex,
			1,
			(ChannelMask & 0x1) != 0 ? 1 : 0,
			(ChannelMask & 0x2) != 0 ? 1 : 0,
			(ChannelMask & 0x4) != 0 ? 1 : 0,
			(ChannelMask & 0x8) != 0 ? 1 : 0) == OutputIndex;
	}

	// Some expressions publish one value through several masked outputs (TextureSample's RGB/R/G/B/A/RGBA,
	// VertexColor, ...). When *every* output is masked they are component views of the same value, so a
	// swizzle can name the matching output instead of relying on an inline mask -- graph-stable, and the pin
	// the editor draws then tells the truth. Mixed sets (SceneTexture's Color/Size/InvSize) are genuinely
	// different values and must never be retargeted this way.
	inline bool TryRetargetChannelMaskToOutput(
		const UMaterialExpression* Expression,
		const int32 ChannelMask,
		int32& OutOutputIndex)
	{
		if (!Expression || Expression->Outputs.Num() < 2 || ChannelMask == 0)
		{
			return false;
		}

		for (const FExpressionOutput& Output : Expression->Outputs)
		{
			if (Output.Mask == 0)
			{
				return false;
			}
		}

		for (int32 Index = 0; Index < Expression->Outputs.Num(); ++Index)
		{
			const FExpressionOutput& Output = Expression->Outputs[Index];
			const int32 OutputChannelMask =
				(Output.MaskR != 0 ? 0x1 : 0)
				| (Output.MaskG != 0 ? 0x2 : 0)
				| (Output.MaskB != 0 ? 0x4 : 0)
				| (Output.MaskA != 0 ? 0x8 : 0);
			if (OutputChannelMask == ChannelMask)
			{
				OutOutputIndex = Index;
				return true;
			}
		}

		return false;
	}

	inline void ConnectCodeValueToInput(FExpressionInput& Input, const FCodeValue& Value)
	{
		if (Value.Expression)
		{
			Input.Connect(Value.OutputIndex, Value.Expression);
			Input.Mask = 0;
			Input.MaskR = 0;
			Input.MaskG = 0;
			Input.MaskB = 0;
			Input.MaskA = 0;
			if (Value.bHasInputMask)
			{
				Input.Mask = 1;
				Input.MaskR = Value.InputMaskR ? 1 : 0;
				Input.MaskG = Value.InputMaskG ? 1 : 0;
				Input.MaskB = Value.InputMaskB ? 1 : 0;
				Input.MaskA = Value.InputMaskA ? 1 : 0;

				// Every inline mask must name a pin the material graph editor resolves back to the same
				// output; otherwise the first Apply/Save silently re-points the wire and drops the mask.
				// FCodeGraphBuilder::ApplyChannelMaskToValue is what guarantees this -- if this ever fires,
				// some other path grew its own masking shortcut.
				ensureMsgf(
					IsInlineInputMaskGraphStable(
						Value.Expression,
						Value.OutputIndex,
						(Value.InputMaskR ? 0x1 : 0)
							| (Value.InputMaskG ? 0x2 : 0)
							| (Value.InputMaskB ? 0x4 : 0)
							| (Value.InputMaskA ? 0x8 : 0)),
					TEXT("DreamShader emitted a component mask on '%s' output %d that the material graph editor cannot round-trip."),
					*Value.Expression->GetClass()->GetName(),
					Value.OutputIndex);
			}
		}
	}

	inline void ClearCodeValueInputMask(FCodeValue& Value)
	{
		Value.bHasInputMask = false;
		Value.InputMaskR = false;
		Value.InputMaskG = false;
		Value.InputMaskB = false;
		Value.InputMaskA = false;
	}

	inline bool ApplyCodeValueInputMask(FCodeValue& Value, const int32 ChannelMask, const int32 ComponentCount)
	{
		if (ChannelMask == 0 || ComponentCount <= 0 || ComponentCount > 4)
		{
			return false;
		}

		Value.bHasInputMask = true;
		Value.InputMaskR = (ChannelMask & 0x1) != 0;
		Value.InputMaskG = (ChannelMask & 0x2) != 0;
		Value.InputMaskB = (ChannelMask & 0x4) != 0;
		Value.InputMaskA = (ChannelMask & 0x8) != 0;
		Value.ComponentCount = ComponentCount;
		Value.bIsTextureObject = false;
		Value.bIsMaterialAttributes = false;
		Value.bIsSubstrateMaterial = false;
		return true;
	}

	inline bool TryResolveExpressionOutputIndex(const UMaterialExpression* Expression, const FString& OutputSpecifier, int32& OutIndex)
	{
		if (!Expression || Expression->Outputs.Num() == 0)
		{
			return false;
		}

		const FName DesiredOutput(*OutputSpecifier.TrimStartAndEnd());
		if (DesiredOutput.IsNone())
		{
			OutIndex = 0;
			return true;
		}

		for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
		{
			const FExpressionOutput& Output = Expression->Outputs[OutputIndex];
			if (!Output.OutputName.IsNone())
			{
				if (Output.OutputName == DesiredOutput)
				{
					OutIndex = OutputIndex;
					return true;
				}
				continue;
			}

			if (Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("RG")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (Output.MaskR && Output.MaskG && Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("RGB")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (Output.MaskR && Output.MaskG && Output.MaskB && Output.MaskA && DesiredOutput == FName(TEXT("RGBA")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (Output.MaskR && !Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("R")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (!Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("G")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (!Output.MaskR && !Output.MaskG && Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("B")))
			{
				OutIndex = OutputIndex;
				return true;
			}
			if (!Output.MaskR && !Output.MaskG && !Output.MaskB && Output.MaskA && DesiredOutput == FName(TEXT("A")))
			{
				OutIndex = OutputIndex;
				return true;
			}
		}

		return false;
	}

	inline bool IsMaterialAttributesComponentType(const int32 ComponentCount, const bool bIsTextureObject, const bool bIsSubstrateMaterial = false)
	{
		return ComponentCount == 0 && !bIsTextureObject && !bIsSubstrateMaterial;
	}

	inline bool IsSpecialNonNumericCodeValue(const FCodeValue& Value)
	{
		return Value.bIsTextureObject || Value.bIsMaterialAttributes || Value.bIsSubstrateMaterial;
	}

	inline bool IsNumericCodeValue(const FCodeValue& Value)
	{
		return !IsSpecialNonNumericCodeValue(Value) && Value.ComponentCount > 0;
	}

	inline bool IsTextureMaterialValueType(const EMaterialValueType ValueType)
	{
		switch (ValueType)
		{
		case MCT_Texture:
		case MCT_Texture2D:
		case MCT_TextureCube:
		case MCT_Texture2DArray:
		case MCT_TextureExternal:
		case MCT_VolumeTexture:
			return true;
		default:
			return false;
		}
	}

	inline bool IsSubstrateMaterialValueType(const EMaterialValueType ValueType)
	{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		return ValueType == MCT_Substrate;
#else
		return ValueType == MCT_Strata;
#endif
	}

	inline FString MakeSubstrateRequiresUE54Error()
	{
		return TEXT("Substrate requires Unreal Engine 5.4 or newer.");
	}

	inline int32 GetComponentCountForMaterialValueType(const EMaterialValueType ValueType)
	{
		switch (ValueType)
		{
		case MCT_Float:
		case MCT_Float1:
		case MCT_LWCScalar:
		case MCT_StaticBool:
		case MCT_Bool:
			return 1;
		case MCT_Float2:
		case MCT_LWCVector2:
			return 2;
		case MCT_Float3:
		case MCT_LWCVector3:
			return 3;
		case MCT_Float4:
		case MCT_LWCVector4:
			return 4;
		default:
			return 0;
		}
	}

	inline bool TryResolveMaterialValueType(
		const EMaterialValueType ValueType,
		int32& OutComponentCount,
		bool& bOutIsTextureObject)
	{
		if (IsTextureMaterialValueType(ValueType))
		{
			OutComponentCount = 0;
			bOutIsTextureObject = true;
			return true;
		}
		if (ValueType == MCT_MaterialAttributes)
		{
			OutComponentCount = 0;
			bOutIsTextureObject = false;
			return true;
		}
		if (IsSubstrateMaterialValueType(ValueType))
		{
			OutComponentCount = 0;
			bOutIsTextureObject = false;
			return true;
		}

		const int32 ComponentCount = GetComponentCountForMaterialValueType(ValueType);
		if (ComponentCount > 0)
		{
			OutComponentCount = ComponentCount;
			bOutIsTextureObject = false;
			return true;
		}

		return false;
	}

	inline EMaterialValueType GetDreamShaderExpressionInputValueType(UMaterialExpression* Expression, const int32 InputIndex)
	{
		if (!Expression)
		{
			return MCT_Unknown;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		return Expression->GetInputValueType(InputIndex);
#else
		return static_cast<EMaterialValueType>(Expression->GetInputType(InputIndex));
#endif
	}

	/**
	 * Width of a TextureProperty node's output, which the engine does not reflect.
	 *
	 * UMaterialExpressionTextureProperty never overrides GetOutputValueType, so the base class answers
	 * with the scalar default and every Texture Size / Texel Size read looks like a float. The real
	 * width is decided at translate time by GetTexturePropertyValueType: float3 for a texture that has
	 * a third axis -- 2D array, volume, sparse volume -- and float2 for everything else. Decompiling it
	 * as a scalar produces source that widens the value by hand, which then appends two float3s and
	 * fails to compile. Same rule as the engine, applied where the engine forgot to publish it.
	 */
	inline bool TryGetTexturePropertyOutputValueType(UMaterialExpression* Expression, EMaterialValueType& OutValueType)
	{
		const UMaterialExpressionTextureProperty* TextureProperty = Cast<UMaterialExpressionTextureProperty>(Expression);
		if (!TextureProperty)
		{
			return false;
		}

		// The width follows the texture wired into TextureObject, so an unconnected pin has no answer
		// to give -- leave it to the caller rather than guessing float2 and being wrong on an array.
		const FExpressionInput TracedTextureInput = TextureProperty->TextureObject.GetTracedInput();
		const UMaterialExpressionTextureBase* TextureExpression =
			Cast<UMaterialExpressionTextureBase>(TracedTextureInput.Expression);
		const UTexture* Texture = TextureExpression ? ToRawPtr(TextureExpression->Texture) : nullptr;
		if (!Texture)
		{
			return false;
		}

		OutValueType = (Texture->IsA<UTexture2DArray>() || Texture->IsA<UVolumeTexture>() || Texture->IsA<USparseVolumeTexture>())
			? MCT_Float3
			: MCT_Float2;
		return true;
	}

	inline EMaterialValueType GetDreamShaderExpressionOutputValueType(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression)
		{
			return MCT_Unknown;
		}

		EMaterialValueType TexturePropertyValueType = MCT_Unknown;
		if (TryGetTexturePropertyOutputValueType(Expression, TexturePropertyValueType))
		{
			return TexturePropertyValueType;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		return Expression->GetOutputValueType(OutputIndex);
#else
		return static_cast<EMaterialValueType>(Expression->GetOutputType(OutputIndex));
#endif
	}

	inline int32 GetDreamShaderExpressionInputCount(UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return 0;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 5)
		return Expression->CountInputs();
#else
		return Expression->GetInputsView().Num();
#endif
	}

	// Whether the material editor draws a preview thumbnail on this node, which is worth a node's
	// height in the layout pass. UE 5.7 exposes it as ShouldShowPreview(); earlier engines have the
	// two flags it is composed from and nothing that reads them.
	inline bool DoesDreamShaderExpressionShowPreview(const UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return false;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
		return Expression->ShouldShowPreview();
#else
		return !Expression->bHidePreviewWindow && !Expression->bCollapsed;
#endif
	}

	inline bool ConnectDreamShaderSetMaterialAttributeInput(
		UMaterialExpressionSetMaterialAttributes* Expression,
		const EMaterialProperty Attribute,
		UMaterialExpression* InputExpression,
		const int32 OutputIndex)
	{
		if (!Expression || !InputExpression || OutputIndex == INDEX_NONE)
		{
			return false;
		}

		int32 InputIndex = 0;
		if (Attribute != MP_MaterialAttributes)
		{
			const FGuid AttributeId = FMaterialAttributeDefinitionMap::GetID(Attribute);
			const int32 ExistingAttributeIndex = Expression->AttributeSetTypes.Find(AttributeId);
			if (ExistingAttributeIndex != INDEX_NONE)
			{
				InputIndex = ExistingAttributeIndex + 1;
			}
			else
			{
				const int32 NewAttributeIndex = Expression->AttributeSetTypes.Add(AttributeId);
				Expression->PreEditChange(nullptr);
				InputIndex = Expression->Inputs.Add(FExpressionInput());
				if (NewAttributeIndex == INDEX_NONE || !Expression->Inputs.IsValidIndex(InputIndex))
				{
					return false;
				}
				Expression->Inputs[InputIndex].InputName = FName(*FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(AttributeId, Expression->Material).ToString());
			}
		}

		if (!Expression->Inputs.IsValidIndex(InputIndex))
		{
			return false;
		}

		Expression->Inputs[InputIndex].Connect(OutputIndex, InputExpression);
		return Expression->Inputs[InputIndex].IsConnected();
	}

	inline void RebuildDreamShaderCustomOutputs(UMaterialExpressionCustom* Expression)
	{
		if (!Expression)
		{
			return;
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		Expression->RebuildOutputs();
#else
		Expression->Outputs.Reset(Expression->AdditionalOutputs.Num() + 1);
		if (Expression->AdditionalOutputs.Num() == 0)
		{
			Expression->bShowOutputNameOnPin = false;
			Expression->Outputs.Add(FExpressionOutput(TEXT("")));
		}
		else
		{
			Expression->bShowOutputNameOnPin = true;
			Expression->Outputs.Add(FExpressionOutput(TEXT("return")));
			for (const FCustomOutput& CustomOutput : Expression->AdditionalOutputs)
			{
				if (!CustomOutput.OutputName.IsNone())
				{
					Expression->Outputs.Add(FExpressionOutput(CustomOutput.OutputName));
				}
			}
		}
#endif
	}

	inline UClass* GetDreamShaderScreenPositionExpressionClass()
	{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		return UMaterialExpressionScreenPosition::StaticClass();
#else
		return FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionScreenPosition"));
#endif
	}

	inline bool IsDreamShaderScreenPositionExpression(const UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return false;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		return Expression->IsA<UMaterialExpressionScreenPosition>();
#else
		UClass* ScreenPositionClass = GetDreamShaderScreenPositionExpressionClass();
		return ScreenPositionClass && Expression->IsA(ScreenPositionClass);
#endif
	}

	inline UClass* GetDreamShaderObjectPositionExpressionClass()
	{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 5)
		return UMaterialExpressionObjectPositionWS::StaticClass();
#else
		return FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionObjectPositionWS"));
#endif
	}

	inline bool IsDreamShaderObjectPositionExpression(const UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return false;
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 5)
		return Expression->IsA<UMaterialExpressionObjectPositionWS>();
#else
		UClass* ObjectPositionClass = GetDreamShaderObjectPositionExpressionClass();
		return ObjectPositionClass && Expression->IsA(ObjectPositionClass);
#endif
	}

	inline bool IsDreamShaderRotatorExpression(const UMaterialExpression* Expression)
	{
		return Expression && Expression->GetClass()->GetName().Equals(TEXT("MaterialExpressionRotator"), ESearchCase::IgnoreCase);
	}

	// StaticClass() for an engine material-expression class that carries no export macro, e.g.
	// UMaterialExpressionSceneDepth: UCLASS() with neither MinimalAPI nor ENGINE_API.
	//
	// From UE 5.6, UHT emits DECLARE_CLASS2 with an exported Z_Construct_<Class>_NoRegister, so
	// StaticClass() resolves from a plugin regardless. UE 5.5 and earlier emit
	// DECLARE_CLASS(..., NO_API): GetPrivateStaticClass never leaves Engine.dll and naming
	// StaticClass() is an unresolved external -- a *link* error, so it compiles clean on every
	// engine and only shows up in a full BuildPlugin. There the class is resolved by script path.
	//
	// Takes the class name without the leading U. Null below 5.6 if the class is not loaded, so
	// call sites must guard, the same way ObjectPositionWS and ScreenPosition already do.
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
#define DREAMSHADER_ENGINE_EXPRESSION_CLASS(ExpressionName) U##ExpressionName::StaticClass()
#else
#define DREAMSHADER_ENGINE_EXPRESSION_CLASS(ExpressionName) \
	FindObject<UClass>(nullptr, TEXT("/Script/Engine.") TEXT(#ExpressionName))
#endif

	inline bool TryResolveKnownExpressionOutputComponentCount(
		const UMaterialExpression* Expression,
		const int32 OutputIndex,
		int32& OutComponentCount)
	{
		(void)OutputIndex;

		if (!Expression)
		{
			return false;
		}

		if (Cast<UMaterialExpressionTextureCoordinate>(Expression)
			|| Cast<UMaterialExpressionPanner>(Expression)
			|| IsDreamShaderScreenPositionExpression(Expression)
			|| IsDreamShaderRotatorExpression(Expression))
		{
			OutComponentCount = 2;
			return true;
		}

		if (Cast<UMaterialExpressionWorldPosition>(Expression)
			|| IsDreamShaderObjectPositionExpression(Expression)
			|| Cast<UMaterialExpressionCameraVectorWS>(Expression)
			|| Cast<UMaterialExpressionVertexNormalWS>(Expression)
			|| Cast<UMaterialExpressionVertexTangentWS>(Expression)
			|| Cast<UMaterialExpressionTransform>(Expression)
			|| Cast<UMaterialExpressionTransformPosition>(Expression))
		{
			OutComponentCount = 3;
			return true;
		}

		const FString ClassName = Expression->GetClass()->GetName();
		if (ClassName.Equals(TEXT("MaterialExpressionSceneTexelSize"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 2;
			return true;
		}
		if (ClassName.Equals(TEXT("MaterialExpressionSkyAtmosphereLightDirection"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 3;
			return true;
		}
		if (ClassName.Equals(TEXT("MaterialExpressionPixelNormalWS"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionCrossProduct"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 3;
			return true;
		}
		if (ClassName.Equals(TEXT("MaterialExpressionPixelDepth"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 1;
			return true;
		}
		if (ClassName.Equals(TEXT("MaterialExpressionTwoSidedSign"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionArctangent2Fast"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionLength"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionMaterialXLuminance"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 1;
			return true;
		}

		return false;
	}

	inline bool TrySplitMemberTarget(const FString& TargetText, FString& OutBaseName, FString& OutMemberName)
	{
		FString Left;
		FString Right;
		if (!TargetText.TrimStartAndEnd().Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			return false;
		}

		OutBaseName = Left.TrimStartAndEnd();
		OutMemberName = Right.TrimStartAndEnd();
		return !OutBaseName.IsEmpty() && !OutMemberName.IsEmpty();
	}

	inline bool ResolveTypeNameForComponentCount(const int32 ComponentCount, FString& OutTypeName)
	{
		switch (ComponentCount)
		{
		case 0: OutTypeName = TEXT("MaterialAttributes"); return true;
		case 1: OutTypeName = TEXT("float"); return true;
		case 2: OutTypeName = TEXT("float2"); return true;
		case 3: OutTypeName = TEXT("float3"); return true;
		case 4: OutTypeName = TEXT("float4"); return true;
		default:
			return false;
		}
	}

	inline bool TryResolveMaterialAttributesBreakOutputIndex(const EMaterialProperty Property, int32& OutOutputIndex)
	{
		switch (Property)
		{
		case MP_BaseColor: OutOutputIndex = 0; return true;
		case MP_Metallic: OutOutputIndex = 1; return true;
		case MP_Specular: OutOutputIndex = 2; return true;
		case MP_Roughness: OutOutputIndex = 3; return true;
		case MP_Anisotropy: OutOutputIndex = 4; return true;
		case MP_EmissiveColor: OutOutputIndex = 5; return true;
		case MP_Opacity: OutOutputIndex = 6; return true;
		case MP_OpacityMask: OutOutputIndex = 7; return true;
		case MP_Normal: OutOutputIndex = 8; return true;
		case MP_Tangent: OutOutputIndex = 9; return true;
		case MP_WorldPositionOffset: OutOutputIndex = 10; return true;
		case MP_SubsurfaceColor: OutOutputIndex = 11; return true;
		case MP_CustomData0: OutOutputIndex = 12; return true;
		case MP_CustomData1: OutOutputIndex = 13; return true;
		case MP_AmbientOcclusion: OutOutputIndex = 14; return true;
		case MP_Refraction: OutOutputIndex = 15; return true;
		case MP_CustomizedUVs0: OutOutputIndex = 16; return true;
		case MP_CustomizedUVs1: OutOutputIndex = 17; return true;
		case MP_CustomizedUVs2: OutOutputIndex = 18; return true;
		case MP_CustomizedUVs3: OutOutputIndex = 19; return true;
		case MP_CustomizedUVs4: OutOutputIndex = 20; return true;
		case MP_CustomizedUVs5: OutOutputIndex = 21; return true;
		case MP_CustomizedUVs6: OutOutputIndex = 22; return true;
		case MP_CustomizedUVs7: OutOutputIndex = 23; return true;
		case MP_PixelDepthOffset: OutOutputIndex = 24; return true;
		case MP_Displacement: OutOutputIndex = 26; return true;
#if DREAMSHADER_WITH_MOON_ENGINE
		// Matches the tail of UMaterialExpressionBreakMaterialAttributes' output list, which appends
		// these five after Displacement.
		case MP_MoonEncodedAttribute0: OutOutputIndex = 27; return true;
		case MP_MoonEncodedAttribute1: OutOutputIndex = 28; return true;
		case MP_MoonEncodedAttribute2: OutOutputIndex = 29; return true;
		case MP_MoonEncodedAttribute3: OutOutputIndex = 30; return true;
		case MP_MoonEncodedAttribute4: OutOutputIndex = 31; return true;
#endif
		default:
			return false;
		}
	}

	inline bool IsIdentifierBoundary(const FString& Text, const int32 Index)
	{
		if (!Text.IsValidIndex(Index))
		{
			return true;
		}

		const TCHAR Char = Text[Index];
		return !(FChar::IsAlnum(Char) || Char == TCHAR('_'));
	}

	inline void SkipWhitespace(const FString& Text, int32& InOutIndex)
	{
		while (Text.IsValidIndex(InOutIndex) && FChar::IsWhitespace(Text[InOutIndex]))
		{
			++InOutIndex;
		}
	}

	inline bool FindMatchingDelimiter(
		const FString& Text,
		const int32 OpenIndex,
		const TCHAR OpenChar,
		const TCHAR CloseChar,
		int32& OutCloseIndex)
	{
		if (!Text.IsValidIndex(OpenIndex) || Text[OpenIndex] != OpenChar)
		{
			return false;
		}

		int32 Depth = 0;
		bool bInString = false;
		for (int32 Index = OpenIndex; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && Text.IsValidIndex(Index + 1))
				{
					++Index;
				}
				else if (Char == TCHAR('"'))
				{
					bInString = false;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				continue;
			}

			if (Char == OpenChar)
			{
				++Depth;
				continue;
			}

			if (Char == CloseChar)
			{
				--Depth;
				if (Depth == 0)
				{
					OutCloseIndex = Index;
					return true;
				}
			}
		}

		return false;
	}
}
