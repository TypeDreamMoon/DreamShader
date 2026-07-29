#pragma once

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
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
#include "Materials/MaterialExpressionSquareRoot.h"
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

	inline EMaterialValueType GetDreamShaderExpressionOutputValueType(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression)
		{
			return MCT_Unknown;
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
#ifdef MOON_ENGINE
		case MP_MooaEncodedAttribute0: OutOutputIndex = 27; return true;
		case MP_MooaEncodedAttribute1: OutOutputIndex = 28; return true;
		case MP_MooaEncodedAttribute2: OutOutputIndex = 29; return true;
		case MP_MooaEncodedAttribute3: OutOutputIndex = 30; return true;
		case MP_MooaEncodedAttribute4: OutOutputIndex = 31; return true;
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
