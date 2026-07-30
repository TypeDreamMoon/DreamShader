// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Definitions of the DreamShader graph-decompiler free helpers (see DreamShaderGraphDecompilerHelpers.h).
// Extracted byte-for-byte from DreamShaderGraphDecompiler.cpp's anonymous namespace.

#include "DreamShaderGraphDecompilerHelpers.h"

#include "Decompiler/DreamShaderGraphDecompiler.h"

#include "Decompiler/DreamShaderDecompileService.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeShared.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "VirtualFunction/DreamShaderVirtualFunctionService.h"

#include "CoreGlobals.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Engine/Texture.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#include "Materials/MaterialExpressionSubstrate.h"
#endif
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialShared.h"
#include "MaterialValueType.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"


namespace UE::DreamShader::Editor::Private
{
	// Every caller writes the result inside a "..." literal, and the DSL scanner reads those literals one
	// line at a time -- a raw newline (comment nodes and #Region names routinely carry them) would break
	// the statement in half. The scanner unescapes \r \n \t, so escaping here round-trips exactly.
	FString EscapeDreamShaderString(const FString& InText)
	{
		FString Result = InText;
		Result.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Result.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Result.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Result.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Result.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Result;
	}

	FString EscapeDreamShaderCodeString(const FString& InText)
	{
		return EscapeDreamShaderString(InText);
	}

	FString GetDreamShaderTypeForFunctionInput(EFunctionInputType InputType)
	{
		switch (InputType)
		{
		case FunctionInput_Scalar:
			return TEXT("float");
		case FunctionInput_Vector2:
			return TEXT("float2");
		case FunctionInput_Vector3:
			return TEXT("float3");
		case FunctionInput_Vector4:
			return TEXT("float4");
		case FunctionInput_Texture2D:
			return TEXT("Texture2D");
		case FunctionInput_TextureCube:
			return TEXT("TextureCube");
		case FunctionInput_Texture2DArray:
			return TEXT("Texture2DArray");
		case FunctionInput_VolumeTexture:
			return TEXT("VolumeTexture");
		case FunctionInput_MaterialAttributes:
			return TEXT("MaterialAttributes");
		case FunctionInput_Substrate:
			return TEXT("Substrate");
		case FunctionInput_StaticBool:
			// Not "bool": that token declares a scalar function input, so a StaticBool input would come
			// back as a float pin and every static-bool value wired into it would be a type mismatch.
			return TEXT("StaticBool");
		case FunctionInput_Bool:
			return TEXT("bool");
		default:
			return TEXT("float4");
		}
	}

	FString GetDreamShaderTypeForMaterialValueType(EMaterialValueType ValueType)
	{
		switch (ValueType)
		{
		case MCT_Float:
		case MCT_Float1:
		case MCT_LWCScalar:
			return TEXT("float");
		case MCT_Float2:
		case MCT_LWCVector2:
			return TEXT("float2");
		case MCT_Float3:
		case MCT_LWCVector3:
			return TEXT("float3");
		case MCT_Float4:
		case MCT_LWCVector4:
			return TEXT("float4");
		case MCT_Texture2D:
		case MCT_Texture:
			return TEXT("Texture2D");
		case MCT_TextureCube:
			return TEXT("TextureCube");
		case MCT_Texture2DArray:
			return TEXT("Texture2DArray");
		case MCT_VolumeTexture:
			return TEXT("VolumeTexture");
		case MCT_MaterialAttributes:
			return TEXT("MaterialAttributes");
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		case MCT_Substrate:
#else
		case MCT_Strata:
#endif
			return TEXT("Substrate");
		case MCT_StaticBool:
		case MCT_Bool:
			return TEXT("bool");
		default:
			return TEXT("float4");
		}
	}

	FString GetDreamShaderTypeForFunctionOutput(const UMaterialExpressionFunctionOutput* OutputExpression);

	FString MakeDreamShaderDeclarationName(const FString& InName, const TCHAR* FallbackPrefix, int32 Index)
	{
		const FString Trimmed = InName.TrimStartAndEnd();
		FString Result;
		Result.Reserve(Trimmed.Len());
		for (int32 CharIndex = 0; CharIndex < Trimmed.Len(); ++CharIndex)
		{
			const TCHAR Char = Trimmed[CharIndex];
			if (CharIndex == 0)
			{
				Result.AppendChar(FChar::IsAlpha(Char) || Char == TCHAR('_') ? Char : TCHAR('_'));
			}
			else
			{
				Result.AppendChar(FChar::IsAlnum(Char) || Char == TCHAR('_') ? Char : TCHAR('_'));
			}
		}

		for (int32 CharIndex = Result.Len() - 1; CharIndex > 0; --CharIndex)
		{
			if (Result[CharIndex] == TCHAR('_') && Result[CharIndex - 1] == TCHAR('_'))
			{
				Result.RemoveAt(CharIndex, 1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
		}

		if (Result.IsEmpty() || Result == TEXT("DreamShaderSymbol"))
		{
			Result = FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1);
		}
		return Result;
	}

	FString MakeFunctionParameterMetadataSuffix(
		const FString& Description,
		const int32 SortPriority,
		const int32 DefaultSortPriority)
	{
		TArray<FString> MetadataEntries;
		if (!Description.TrimStartAndEnd().IsEmpty())
		{
			MetadataEntries.Add(FString::Printf(TEXT("Description=\"%s\";"), *EscapeDreamShaderString(Description.TrimStartAndEnd())));
		}
		if (SortPriority != DefaultSortPriority)
		{
			MetadataEntries.Add(FString::Printf(TEXT("SortPriority=%d;"), SortPriority));
		}

		return MetadataEntries.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" [\n\t\t\t%s\n\t\t]"), *FString::Join(MetadataEntries, TEXT("\n\t\t\t")));
	}

	FString MakePreviewValueText(EFunctionInputType InputType, const FVector4f& PreviewValue)
	{
		switch (InputType)
		{
		case FunctionInput_Scalar:
			return FString::SanitizeFloat(PreviewValue.X);
		case FunctionInput_StaticBool:
		case FunctionInput_Bool:
			return PreviewValue.X != 0.0f ? TEXT("true") : TEXT("false");
		case FunctionInput_Vector2:
			return FString::Printf(TEXT("float2(%g, %g)"), PreviewValue.X, PreviewValue.Y);
		case FunctionInput_Vector3:
			return FString::Printf(TEXT("float3(%g, %g, %g)"), PreviewValue.X, PreviewValue.Y, PreviewValue.Z);
		case FunctionInput_Vector4:
			return FString::Printf(TEXT("float4(%g, %g, %g, %g)"), PreviewValue.X, PreviewValue.Y, PreviewValue.Z, PreviewValue.W);
		default:
			return FString();
		}
	}

	bool BuildVirtualFunctionDefinition(const UMaterialFunction* MaterialFunction, FString& OutDefinition, FString& OutError)
	{
		const bool bBuilt = FDreamShaderVirtualFunctionService::BuildDefinition(
			MaterialFunction,
			[](const UMaterialExpressionFunctionOutput* OutputExpression)
			{
				return GetDreamShaderTypeForFunctionOutput(OutputExpression);
			},
			OutDefinition,
			OutError);

		if (!bBuilt)
		{
			return false;
		}

		if (MaterialFunction
			&& MaterialFunction->GetPathName().Equals(
				TEXT("/Engine/Functions/Engine_MaterialFunctions02/ScreenResolution.ScreenResolution"),
				ESearchCase::IgnoreCase))
		{
			OutDefinition.ReplaceInline(TEXT("\t\tfloat Visible_Resolution;"), TEXT("\t\tfloat2 Visible_Resolution;"));
			OutDefinition.ReplaceInline(TEXT("\t\tfloat Buffer_Resolution;"), TEXT("\t\tfloat2 Buffer_Resolution;"));
		}

		return true;
	}

	bool BuildVirtualFunctionCallText(const UMaterialFunction* MaterialFunction, FString& OutCallText, FString& OutError)
	{
		return FDreamShaderVirtualFunctionService::BuildCallText(MaterialFunction, OutCallText, OutError);
	}

	FString FormatDreamShaderFloat(const double Value)
	{
		return FString::SanitizeFloat(Value);
	}

	FString FormatDreamShaderVector2(const double X, const double Y)
	{
		return FString::Printf(TEXT("float2(%s, %s)"), *FormatDreamShaderFloat(X), *FormatDreamShaderFloat(Y));
	}

	FString FormatDreamShaderVector3(const double X, const double Y, const double Z)
	{
		return FString::Printf(
			TEXT("float3(%s, %s, %s)"),
			*FormatDreamShaderFloat(X),
			*FormatDreamShaderFloat(Y),
			*FormatDreamShaderFloat(Z));
	}

	FString FormatDreamShaderVector4(const double X, const double Y, const double Z, const double W)
	{
		return FString::Printf(
			TEXT("float4(%s, %s, %s, %s)"),
			*FormatDreamShaderFloat(X),
			*FormatDreamShaderFloat(Y),
			*FormatDreamShaderFloat(Z),
			*FormatDreamShaderFloat(W));
	}

	FString FormatDreamShaderColor(const FLinearColor& Color)
	{
		return FormatDreamShaderVector4(Color.R, Color.G, Color.B, Color.A);
	}

	FString WrapExpressionForSuffix(const FString& ExpressionText)
	{
		const FString Trimmed = ExpressionText.TrimStartAndEnd();
		const bool bSimple =
			!Trimmed.IsEmpty()
			&& !Trimmed.Contains(TEXT(" "))
			&& !Trimmed.Contains(TEXT("+"))
			&& !Trimmed.Contains(TEXT("-"))
			&& !Trimmed.Contains(TEXT("*"))
			&& !Trimmed.Contains(TEXT("/"));
		return bSimple ? Trimmed : FString::Printf(TEXT("(%s)"), *Trimmed);
	}

	bool IsSwizzleComponentChar(const TCHAR Character)
	{
		switch (FChar::ToLower(Character))
		{
		case TEXT('r'):
		case TEXT('g'):
		case TEXT('b'):
		case TEXT('a'):
		case TEXT('x'):
		case TEXT('y'):
		case TEXT('z'):
		case TEXT('w'):
			return true;
		default:
			return false;
		}
	}

	bool IsSwizzleText(const FString& Text)
	{
		if (Text.IsEmpty() || Text.Len() > 4)
		{
			return false;
		}

		for (const TCHAR Character : Text)
		{
			if (!IsSwizzleComponentChar(Character))
			{
				return false;
			}
		}
		return true;
	}

	int32 GetSwizzleComponentIndex(const TCHAR Character)
	{
		switch (FChar::ToLower(Character))
		{
		case TEXT('r'):
		case TEXT('x'):
			return 0;
		case TEXT('g'):
		case TEXT('y'):
			return 1;
		case TEXT('b'):
		case TEXT('z'):
			return 2;
		case TEXT('a'):
		case TEXT('w'):
			return 3;
		default:
			return INDEX_NONE;
		}
	}

	bool TrySplitTrailingSwizzle(const FString& ExpressionText, FString& OutBaseText, FString& OutSwizzleText)
	{
		const FString Trimmed = ExpressionText.TrimStartAndEnd();
		int32 DotIndex = INDEX_NONE;
		if (!Trimmed.FindLastChar(TEXT('.'), DotIndex) || DotIndex <= 0 || DotIndex + 1 >= Trimmed.Len())
		{
			return false;
		}

		const FString CandidateSwizzle = Trimmed.Mid(DotIndex + 1).ToLower();
		if (!IsSwizzleText(CandidateSwizzle))
		{
			return false;
		}

		OutBaseText = Trimmed.Left(DotIndex);
		OutSwizzleText = CandidateSwizzle;
		return !OutBaseText.TrimStartAndEnd().IsEmpty();
	}

	bool TryComposeTrailingSwizzle(
		const FString& ExpressionText,
		const FString& RequestedSwizzle,
		FString& OutExpressionText)
	{
		FString BaseText;
		FString ExistingSwizzle;
		if (!TrySplitTrailingSwizzle(ExpressionText, BaseText, ExistingSwizzle))
		{
			return false;
		}

		FString ComposedSwizzle;
		ComposedSwizzle.Reserve(RequestedSwizzle.Len());
		for (const TCHAR RequestedComponent : RequestedSwizzle)
		{
			const int32 ComponentIndex = GetSwizzleComponentIndex(RequestedComponent);
			if (!ExistingSwizzle.IsValidIndex(ComponentIndex))
			{
				return false;
			}
			ComposedSwizzle += ExistingSwizzle[ComponentIndex];
		}

		OutExpressionText = FString::Printf(TEXT("%s.%s"), *WrapExpressionForSuffix(BaseText), *ComposedSwizzle);
		return true;
	}

	FString MakeInputMaskSuffix(const FExpressionInput& Input)
	{
		if (!Input.Mask)
		{
			return FString();
		}

		FString Suffix;
		if (Input.MaskR)
		{
			Suffix += TEXT("r");
		}
		if (Input.MaskG)
		{
			Suffix += TEXT("g");
		}
		if (Input.MaskB)
		{
			Suffix += TEXT("b");
		}
		if (Input.MaskA)
		{
			Suffix += TEXT("a");
		}
		return Suffix;
	}

	FString MakeSwizzleExpression(const FString& ExpressionText, const FString& SwizzleText)
	{
		if (SwizzleText.IsEmpty())
		{
			return ExpressionText;
		}

		const FString NormalizedSwizzle = SwizzleText.ToLower();
		FString ComposedExpression;
		if (IsSwizzleText(NormalizedSwizzle)
			&& TryComposeTrailingSwizzle(ExpressionText, NormalizedSwizzle, ComposedExpression))
		{
			return ComposedExpression;
		}

		return FString::Printf(TEXT("%s.%s"), *WrapExpressionForSuffix(ExpressionText), *NormalizedSwizzle);
	}

	FString ApplyInputMask(const FString& ExpressionText, const FExpressionInput& Input)
	{
		const FString MaskSuffix = MakeInputMaskSuffix(Input);
		if (MaskSuffix.IsEmpty())
		{
			return ExpressionText;
		}

		return MakeSwizzleExpression(ExpressionText, MaskSuffix);
	}

	FString MakeDreamShaderObjectPathLiteral(const UObject* Object)
	{
		if (!Object)
		{
			return FString();
		}

		FString PackageName = Object->GetOutermost() ? Object->GetOutermost()->GetName() : FString();
		PackageName.TrimStartAndEndInline();
		PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (PackageName.IsEmpty())
		{
			return Object->GetPathName();
		}

		const auto BuildRootedLiteral = [](const TCHAR* RootName, const FString& RelativePath)
		{
			return FString::Printf(TEXT("Path(%s, \"%s\")"), RootName, *EscapeDreamShaderString(RelativePath));
		};

		if (PackageName.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
		{
			return BuildRootedLiteral(TEXT("Game"), PackageName.Mid(6));
		}
		if (PackageName.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase))
		{
			return BuildRootedLiteral(TEXT("Engine"), PackageName.Mid(8));
		}

		FString BestPluginName;
		FString BestMountedPath;
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			FString MountedPath = Plugin->GetMountedAssetPath();
			MountedPath.TrimStartAndEndInline();
			MountedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (MountedPath.EndsWith(TEXT("/")))
			{
				MountedPath.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			if (!MountedPath.StartsWith(TEXT("/")))
			{
				MountedPath = TEXT("/") + MountedPath;
			}
			if (MountedPath.IsEmpty() || MountedPath == TEXT("/"))
			{
				MountedPath = TEXT("/") + Plugin->GetName();
			}

			if ((PackageName.Equals(MountedPath, ESearchCase::IgnoreCase)
				|| PackageName.StartsWith(MountedPath + TEXT("/"), ESearchCase::IgnoreCase))
				&& MountedPath.Len() > BestMountedPath.Len())
			{
				BestMountedPath = MountedPath;
				BestPluginName = Plugin->GetName();
			}
		}

		if (!BestPluginName.IsEmpty())
		{
			FString RelativePath = PackageName.Mid(BestMountedPath.Len());
			while (RelativePath.StartsWith(TEXT("/")))
			{
				RelativePath.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			return FString::Printf(
				TEXT("Path(Plugins.%s, \"%s\")"),
				*BestPluginName,
				*EscapeDreamShaderString(RelativePath));
		}

		return FString::Printf(TEXT("Path(\"%s\")"), *EscapeDreamShaderString(PackageName));
	}

	FString GetDreamShaderTypeForCustomOutputType(const ECustomMaterialOutputType OutputType)
	{
		switch (OutputType)
		{
		case CMOT_Float1:
			return TEXT("float");
		case CMOT_Float2:
			return TEXT("float2");
		case CMOT_Float3:
			return TEXT("float3");
		case CMOT_Float4:
			return TEXT("float4");
		case CMOT_MaterialAttributes:
			return TEXT("MaterialAttributes");
		default:
			return TEXT("float4");
		}
	}

	FString GetDreamShaderTypeForComponentCount(const int32 ComponentCount)
	{
		if (ComponentCount <= 1)
		{
			return TEXT("float");
		}
		if (ComponentCount == 2)
		{
			return TEXT("float2");
		}
		if (ComponentCount == 3)
		{
			return TEXT("float3");
		}
		return TEXT("float4");
	}

	int32 GetComponentCountForFunctionInputType(const EFunctionInputType InputType)
	{
		switch (InputType)
		{
		case FunctionInput_Vector2:
			return 2;
		case FunctionInput_Vector3:
			return 3;
		case FunctionInput_Vector4:
			return 4;
		case FunctionInput_Texture2D:
		case FunctionInput_TextureCube:
		case FunctionInput_Texture2DArray:
		case FunctionInput_VolumeTexture:
		case FunctionInput_Substrate:
			return 0;
		case FunctionInput_Scalar:
		case FunctionInput_StaticBool:
		case FunctionInput_Bool:
		default:
			return 1;
		}
	}

	int32 GetOutputMaskComponentCount(const UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression || !Expression->Outputs.IsValidIndex(OutputIndex))
		{
			return 0;
		}

		const FExpressionOutput& Output = Expression->Outputs[OutputIndex];
		return (Output.MaskR ? 1 : 0)
			+ (Output.MaskG ? 1 : 0)
			+ (Output.MaskB ? 1 : 0)
			+ (Output.MaskA ? 1 : 0);
	}

	UMaterialExpressionFunctionOutput* ResolveFunctionCallOutputExpression(UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex)
	{
		if (!FunctionCall)
		{
			return nullptr;
		}

		FString DesiredOutputName;
		if (FunctionCall->FunctionOutputs.IsValidIndex(OutputIndex))
		{
			const FFunctionExpressionOutput& FunctionOutput = FunctionCall->FunctionOutputs[OutputIndex];
			if (FunctionOutput.ExpressionOutput)
			{
				return FunctionOutput.ExpressionOutput;
			}
			DesiredOutputName = FunctionOutput.Output.OutputName.ToString();
		}

		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
		if (!MaterialFunction)
		{
			return nullptr;
		}

		TArray<FFunctionExpressionInput> FunctionInputs;
		TArray<FFunctionExpressionOutput> FunctionOutputs;
		MaterialFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);
		if (FunctionOutputs.IsValidIndex(OutputIndex) && FunctionOutputs[OutputIndex].ExpressionOutput)
		{
			return FunctionOutputs[OutputIndex].ExpressionOutput;
		}

		if (!DesiredOutputName.IsEmpty())
		{
			for (const FFunctionExpressionOutput& FunctionOutput : FunctionOutputs)
			{
				UMaterialExpressionFunctionOutput* OutputExpression = FunctionOutput.ExpressionOutput;
				if (OutputExpression
					&& OutputExpression->OutputName.ToString().Equals(DesiredOutputName, ESearchCase::IgnoreCase))
				{
					return OutputExpression;
				}
			}
		}

		return nullptr;
	}

	// Switch-style nodes forward whichever input the platform/quality/static condition selects, so the
	// node itself carries no size information -- UE reports MCT_Unknown (or a placeholder) for their
	// output value type. Without resolving them from their connected inputs the generic fallback below
	// assumes float4, which then oversizes everything downstream: an AppendVector fed by a float3
	// material-function output was emitted as `float5(...)`, a type the language does not have, so the
	// decompiled file could not be generated back. VertexInterpolator is the same shape (VS in, PS out).
	static bool IsDreamShaderPassThroughExpression(const UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return false;
		}

		static const TCHAR* PassThroughClassNames[] =
		{
			TEXT("MaterialExpressionFeatureLevelSwitch"),
			TEXT("MaterialExpressionQualitySwitch"),
			TEXT("MaterialExpressionShadingPathSwitch"),
			TEXT("MaterialExpressionShaderStageSwitch"),
			TEXT("MaterialExpressionReflectionCapturePassSwitch"),
			TEXT("MaterialExpressionPathTracingQualitySwitch"),
			TEXT("MaterialExpressionRayTracingQualitySwitch"),
			TEXT("MaterialExpressionPreviousFrameSwitch"),
			TEXT("MaterialExpressionVertexInterpolator"),
		};

		const FString ClassName = Expression->GetClass()->GetName();
		for (const TCHAR* PassThroughClassName : PassThroughClassNames)
		{
			if (ClassName.Equals(PassThroughClassName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	int32 GetExpressionOutputComponentCount(UMaterialExpression* Expression, const int32 OutputIndex, TSet<UMaterialExpression*>* VisitingExpressions)
	{
		if (!Expression)
		{
			return 1;
		}

		TSet<UMaterialExpression*> LocalVisitingExpressions;
		if (!VisitingExpressions)
		{
			VisitingExpressions = &LocalVisitingExpressions;
		}
		if (VisitingExpressions->Contains(Expression))
		{
			return 1;
		}
		VisitingExpressions->Add(Expression);

		const EMaterialValueType EarlyOutputType = GetDreamShaderExpressionOutputValueType(Expression, OutputIndex);
		if (IsSubstrateMaterialValueType(EarlyOutputType) || EarlyOutputType == MCT_MaterialAttributes || IsTextureMaterialValueType(EarlyOutputType))
		{
			VisitingExpressions->Remove(Expression);
			return 0;
		}

		auto ResolveInputComponentCount = [VisitingExpressions](const FExpressionInput& Input, const int32 DefaultComponentCount) -> int32
		{
			const FExpressionInput TracedInput = Input.GetTracedInput();
			return TracedInput.Expression
				? GetExpressionOutputComponentCount(TracedInput.Expression, TracedInput.OutputIndex, VisitingExpressions)
				: DefaultComponentCount;
		};

		auto Finish = [VisitingExpressions, Expression](const int32 ComponentCount) -> int32
		{
			VisitingExpressions->Remove(Expression);
			return ComponentCount;
		};

		int32 KnownComponentCount = 0;
		if (TryResolveKnownExpressionOutputComponentCount(Expression, OutputIndex, KnownComponentCount) && KnownComponentCount > 0)
		{
			return Finish(KnownComponentCount);
		}

		if (Cast<UMaterialExpressionTextureCoordinate>(Expression)
			|| Cast<UMaterialExpressionPanner>(Expression)
			|| IsDreamShaderRotatorExpression(Expression))
		{
			return Finish(2);
		}

		if (Cast<UMaterialExpressionWorldPosition>(Expression)
			|| IsDreamShaderObjectPositionExpression(Expression)
			|| Cast<UMaterialExpressionCameraVectorWS>(Expression)
			|| Cast<UMaterialExpressionVertexNormalWS>(Expression)
			|| Cast<UMaterialExpressionVertexTangentWS>(Expression))
		{
			return Finish(3);
		}

		if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			return Finish(GetComponentCountForFunctionInputType(FunctionInput->InputType.GetValue()));
		}

		if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (UMaterialExpressionFunctionOutput* FunctionOutput = ResolveFunctionCallOutputExpression(FunctionCall, OutputIndex))
			{
				const FString FunctionOutputType = GetDreamShaderTypeForFunctionOutput(FunctionOutput);
				if (FunctionOutputType.Equals(TEXT("float2"), ESearchCase::IgnoreCase))
				{
					return Finish(2);
				}
				if (FunctionOutputType.Equals(TEXT("float3"), ESearchCase::IgnoreCase))
				{
					return Finish(3);
				}
				if (FunctionOutputType.Equals(TEXT("float4"), ESearchCase::IgnoreCase))
				{
					return Finish(4);
				}
				if (FunctionOutputType.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase)
					|| FunctionOutputType.StartsWith(TEXT("Texture"), ESearchCase::IgnoreCase))
				{
					return Finish(0);
				}
				return Finish(1);
			}
		}

		if (UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression))
		{
			const int32 MaskComponentCount =
				(Mask->R ? 1 : 0)
				+ (Mask->G ? 1 : 0)
				+ (Mask->B ? 1 : 0)
				+ (Mask->A ? 1 : 0);
			return Finish(MaskComponentCount > 0 ? MaskComponentCount : 1);
		}

		if (UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(Expression))
		{
			return Finish(FMath::Clamp(
				ResolveInputComponentCount(Append->A, 1) + ResolveInputComponentCount(Append->B, 1),
				1,
				4));
		}

		if (UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(Add->A, 1), ResolveInputComponentCount(Add->B, 1)));
		}
		if (UMaterialExpressionSubtract* Subtract = Cast<UMaterialExpressionSubtract>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(Subtract->A, 1), ResolveInputComponentCount(Subtract->B, 1)));
		}
		if (UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(Multiply->A, 1), ResolveInputComponentCount(Multiply->B, 1)));
		}
		if (UMaterialExpressionDivide* Divide = Cast<UMaterialExpressionDivide>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(Divide->A, 1), ResolveInputComponentCount(Divide->B, 1)));
		}
		if (UMaterialExpressionLinearInterpolate* Lerp = Cast<UMaterialExpressionLinearInterpolate>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(Lerp->A, 1), ResolveInputComponentCount(Lerp->B, 1)));
		}
		if (UMaterialExpressionIf* IfExpression = Cast<UMaterialExpressionIf>(Expression))
		{
			int32 ComponentCount = FMath::Max(
				ResolveInputComponentCount(IfExpression->AGreaterThanB, 1),
				ResolveInputComponentCount(IfExpression->ALessThanB, 1));
			if (IfExpression->AEqualsB.GetTracedInput().Expression)
			{
				ComponentCount = FMath::Max(ComponentCount, ResolveInputComponentCount(IfExpression->AEqualsB, 1));
			}
			return Finish(ComponentCount);
		}
		if (UMaterialExpressionStaticSwitchParameter* StaticSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(StaticSwitch->A, 1), ResolveInputComponentCount(StaticSwitch->B, 1)));
		}
		if (UMaterialExpressionStaticComponentMaskParameter* StaticComponentMask = Cast<UMaterialExpressionStaticComponentMaskParameter>(Expression))
		{
			const int32 MaskComponentCount =
				(StaticComponentMask->DefaultR ? 1 : 0)
				+ (StaticComponentMask->DefaultG ? 1 : 0)
				+ (StaticComponentMask->DefaultB ? 1 : 0)
				+ (StaticComponentMask->DefaultA ? 1 : 0);
			return Finish(MaskComponentCount > 0 ? MaskComponentCount : 1);
		}
		if (UMaterialExpressionOneMinus* OneMinus = Cast<UMaterialExpressionOneMinus>(Expression))
		{
			return Finish(ResolveInputComponentCount(OneMinus->Input, 1));
		}
		if (UMaterialExpressionPower* Power = Cast<UMaterialExpressionPower>(Expression))
		{
			return Finish(ResolveInputComponentCount(Power->Base, 1));
		}
		if (UMaterialExpressionNormalize* Normalize = Cast<UMaterialExpressionNormalize>(Expression))
		{
			return Finish(ResolveInputComponentCount(Normalize->VectorInput, 1));
		}
		if (UMaterialExpressionAbs* Abs = Cast<UMaterialExpressionAbs>(Expression))
		{
			return Finish(ResolveInputComponentCount(Abs->Input, 1));
		}
		if (UMaterialExpressionSaturate* Saturate = Cast<UMaterialExpressionSaturate>(Expression))
		{
			return Finish(ResolveInputComponentCount(Saturate->Input, 1));
		}
		if (UMaterialExpressionFloor* Floor = Cast<UMaterialExpressionFloor>(Expression))
		{
			return Finish(ResolveInputComponentCount(Floor->Input, 1));
		}
		if (UMaterialExpressionCeil* Ceil = Cast<UMaterialExpressionCeil>(Expression))
		{
			return Finish(ResolveInputComponentCount(Ceil->Input, 1));
		}
		if (UMaterialExpressionFrac* Frac = Cast<UMaterialExpressionFrac>(Expression))
		{
			return Finish(ResolveInputComponentCount(Frac->Input, 1));
		}
		if (UMaterialExpressionSquareRoot* SquareRoot = Cast<UMaterialExpressionSquareRoot>(Expression))
		{
			return Finish(ResolveInputComponentCount(SquareRoot->Input, 1));
		}
		if (UMaterialExpressionSine* Sine = Cast<UMaterialExpressionSine>(Expression))
		{
			return Finish(ResolveInputComponentCount(Sine->Input, 1));
		}
		if (UMaterialExpressionCosine* Cosine = Cast<UMaterialExpressionCosine>(Expression))
		{
			return Finish(ResolveInputComponentCount(Cosine->Input, 1));
		}

		const FString ClassName = Expression->GetClass()->GetName();
		if (ClassName.Equals(TEXT("MaterialExpressionPixelNormalWS"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionCrossProduct"), ESearchCase::IgnoreCase))
		{
			return Finish(3);
		}
		if (ClassName.Equals(TEXT("MaterialExpressionPixelDepth"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionTwoSidedSign"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionArctangent2Fast"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionLength"), ESearchCase::IgnoreCase)
			|| ClassName.Equals(TEXT("MaterialExpressionMaterialXLuminance"), ESearchCase::IgnoreCase))
		{
			return Finish(1);
		}

		if (UMaterialExpressionStaticSwitch* StaticSwitch = Cast<UMaterialExpressionStaticSwitch>(Expression))
		{
			return Finish(FMath::Max(ResolveInputComponentCount(StaticSwitch->A, 1), ResolveInputComponentCount(StaticSwitch->B, 1)));
		}
		if (IsDreamShaderPassThroughExpression(Expression))
		{
			int32 PassThroughComponentCount = 0;
			const int32 InputCount = GetDreamShaderExpressionInputCount(Expression);
			for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
			{
				if (const FExpressionInput* Input = Expression->GetInput(InputIndex))
				{
					PassThroughComponentCount = FMath::Max(PassThroughComponentCount, ResolveInputComponentCount(*Input, 0));
				}
			}
			if (PassThroughComponentCount > 0)
			{
				return Finish(PassThroughComponentCount);
			}
		}

		const EMaterialValueType OutputType = GetDreamShaderExpressionOutputValueType(Expression, OutputIndex);
		if (IsTextureMaterialValueType(OutputType) || OutputType == MCT_MaterialAttributes)
		{
			return Finish(0);
		}

		const int32 MaskComponentCount = GetOutputMaskComponentCount(Expression, OutputIndex);
		if (MaskComponentCount > 0)
		{
			return Finish(MaskComponentCount);
		}

		const int32 TypeComponentCount = GetComponentCountForMaterialValueType(OutputType);
		return Finish(TypeComponentCount > 0 ? TypeComponentCount : 4);
	}

	FString GetDreamShaderTypeForFunctionOutput(const UMaterialExpressionFunctionOutput* OutputExpression)
	{
		if (!OutputExpression)
		{
			return TEXT("float4");
		}

		const FExpressionInput TracedInput = OutputExpression->A.GetTracedInput();
		if (TracedInput.Expression)
		{
			const EMaterialValueType OutputType = GetDreamShaderExpressionOutputValueType(TracedInput.Expression, TracedInput.OutputIndex);
			if (IsSubstrateMaterialValueType(OutputType))
			{
				return TEXT("Substrate");
			}
			if (OutputType == MCT_MaterialAttributes)
			{
				return TEXT("MaterialAttributes");
			}
			if (OutputType == MCT_StaticBool || OutputType == MCT_Bool || IsTextureMaterialValueType(OutputType))
			{
				return GetDreamShaderTypeForMaterialValueType(OutputType);
			}

			const FString MaskSuffix = MakeInputMaskSuffix(OutputExpression->A);
			if (!MaskSuffix.IsEmpty())
			{
				return GetDreamShaderTypeForComponentCount(MaskSuffix.Len());
			}

			const int32 ComponentCount = GetExpressionOutputComponentCount(TracedInput.Expression, TracedInput.OutputIndex);
			if (ComponentCount > 0)
			{
				return GetDreamShaderTypeForComponentCount(ComponentCount);
			}

			return GetDreamShaderTypeForMaterialValueType(OutputType);
		}

		const EMaterialValueType OutputType = GetDreamShaderExpressionOutputValueType(const_cast<UMaterialExpressionFunctionOutput*>(OutputExpression), 0);
		if (IsSubstrateMaterialValueType(OutputType))
		{
			return TEXT("Substrate");
		}
		if (OutputType == MCT_MaterialAttributes)
		{
			return TEXT("MaterialAttributes");
		}
		return GetDreamShaderTypeForMaterialValueType(OutputType);
	}

	FString GetEnumLiteralText(const UEnum* Enum, const int64 Value)
	{
		if (!Enum)
		{
			return FString::FromInt(static_cast<int32>(Value));
		}

		const FString Name = Enum->GetNameStringByValue(Value);
		return Name.IsEmpty() ? FString::FromInt(static_cast<int32>(Value)) : Name;
	}

	FString GetDreamShaderTypeForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression)
		{
			return TEXT("float4");
		}

		if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (UMaterialExpressionFunctionOutput* FunctionOutput = ResolveFunctionCallOutputExpression(FunctionCall, OutputIndex))
			{
				return GetDreamShaderTypeForFunctionOutput(FunctionOutput);
			}
		}

		const EMaterialValueType OutputType = GetDreamShaderExpressionOutputValueType(Expression, OutputIndex);
		if (IsSubstrateMaterialValueType(OutputType))
		{
			return TEXT("Substrate");
		}
		if (OutputType == MCT_MaterialAttributes)
		{
			return TEXT("MaterialAttributes");
		}
		if (OutputType == MCT_StaticBool || OutputType == MCT_Bool || IsTextureMaterialValueType(OutputType))
		{
			return GetDreamShaderTypeForMaterialValueType(OutputType);
		}
		const int32 ComponentCount = GetExpressionOutputComponentCount(Expression, OutputIndex);
		if (ComponentCount > 0)
		{
			return GetDreamShaderTypeForComponentCount(ComponentCount);
		}
		return GetDreamShaderTypeForMaterialValueType(OutputType);
	}

	FString GetCustomOutputName(const UMaterialExpressionCustom* CustomExpression, const int32 OutputIndex)
	{
		if (!CustomExpression || OutputIndex <= 0)
		{
			return FString();
		}

		const int32 AdditionalOutputIndex = OutputIndex - 1;
		if (!CustomExpression->AdditionalOutputs.IsValidIndex(AdditionalOutputIndex))
		{
			return FString();
		}

		return CustomExpression->AdditionalOutputs[AdditionalOutputIndex].OutputName.ToString();
	}

	FString GetFunctionCallOutputName(const UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex)
	{
		if (!FunctionCall || !FunctionCall->FunctionOutputs.IsValidIndex(OutputIndex))
		{
			return FString();
		}

		const FFunctionExpressionOutput& Output = FunctionCall->FunctionOutputs[OutputIndex];
		if (Output.ExpressionOutput)
		{
			return Output.ExpressionOutput->OutputName.ToString();
		}
		return Output.Output.OutputName.ToString();
	}

	FString GetMaterialExpressionShortName(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}

		FString Name = Class->GetName();
		Name.RemoveFromStart(TEXT("U"), ESearchCase::CaseSensitive);
		Name.RemoveFromStart(TEXT("MaterialExpression"), ESearchCase::CaseSensitive);
		return Name;
	}

	FString GetMaterialDomainText(const EMaterialDomain Domain)
	{
		switch (Domain)
		{
		case MD_Surface:
			return TEXT("Surface");
		case MD_DeferredDecal:
			return TEXT("DeferredDecal");
		case MD_LightFunction:
			return TEXT("LightFunction");
		case MD_Volume:
			return TEXT("Volume");
		case MD_PostProcess:
			return TEXT("PostProcess");
		case MD_UI:
			return TEXT("UI");
		case MD_RuntimeVirtualTexture:
			return TEXT("RuntimeVirtualTexture");
		default:
			return TEXT("Surface");
		}
	}

	FString GetBlendModeText(const EBlendMode BlendMode)
	{
		switch (BlendMode)
		{
		case BLEND_Opaque:
			return TEXT("Opaque");
		case BLEND_Masked:
			return TEXT("Masked");
		case BLEND_Translucent:
			return TEXT("Translucent");
		case BLEND_Additive:
			return TEXT("Additive");
		case BLEND_Modulate:
			return TEXT("Modulate");
		case BLEND_AlphaComposite:
			return TEXT("AlphaComposite");
		case BLEND_AlphaHoldout:
			return TEXT("AlphaHoldout");
		case BLEND_TranslucentColoredTransmittance:
			return TEXT("Translucent");
		default:
			return TEXT("Opaque");
		}
	}

	FString GetShadingModelText(const UMaterial* Material)
	{
		if (!Material)
		{
			return TEXT("DefaultLit");
		}

		const FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
		if (ShadingModels.HasShadingModel(MSM_Unlit))
		{
			return TEXT("Unlit");
		}
		if (ShadingModels.HasShadingModel(MSM_Subsurface))
		{
			return TEXT("Subsurface");
		}
		if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin))
		{
			return TEXT("PreintegratedSkin");
		}
		if (ShadingModels.HasShadingModel(MSM_ClearCoat))
		{
			return TEXT("ClearCoat");
		}
		if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile))
		{
			return TEXT("SubsurfaceProfile");
		}
		if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage))
		{
			return TEXT("TwoSidedFoliage");
		}
		if (ShadingModels.HasShadingModel(MSM_Hair))
		{
			return TEXT("Hair");
		}
		if (ShadingModels.HasShadingModel(MSM_Cloth))
		{
			return TEXT("Cloth");
		}
		if (ShadingModels.HasShadingModel(MSM_Eye))
		{
			return TEXT("Eye");
		}
		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater))
		{
			return TEXT("SingleLayerWater");
		}
		if (ShadingModels.HasShadingModel(MSM_ThinTranslucent))
		{
			return TEXT("ThinTranslucent");
		}
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		if (ShadingModels.HasShadingModel(MSM_Strata))
		{
			return TEXT("Substrate");
		}
#endif
		return TEXT("DefaultLit");
	}

	void AppendBoolMaterialSettingIfDifferent(
		TArray<FString>& Lines,
		const UMaterial* Material,
		const TCHAR* PropertyName)
	{
		if (!Material || !PropertyName)
		{
			return;
		}

		const FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(UMaterial::StaticClass(), FName(PropertyName));
		if (!BoolProperty)
		{
			return;
		}

		const UMaterial* DefaultMaterial = GetDefault<UMaterial>();
		const bool bValue = BoolProperty->GetPropertyValue_InContainer(Material);
		const bool bDefaultValue = DefaultMaterial
			? BoolProperty->GetPropertyValue_InContainer(DefaultMaterial)
			: false;
		if (bValue == bDefaultValue)
		{
			return;
		}

		Lines.Add(FString::Printf(
			TEXT("\t\t%s = %s;"),
			PropertyName,
			bValue ? TEXT("true") : TEXT("false")));
	}

	void AppendEnumMaterialSettingIfDifferent(
		TArray<FString>& Lines,
		const UMaterial* Material,
		const TCHAR* PropertyName)
	{
		if (!Material || !PropertyName || !*PropertyName)
		{
			return;
		}

		const FProperty* Property = Material->GetClass()->FindPropertyByName(FName(PropertyName));
		if (!Property)
		{
			return;
		}

		const UMaterial* DefaultMaterial = GetDefault<UMaterial>();
		if (!DefaultMaterial)
		{
			return;
		}

		const UEnum* Enum = nullptr;
		int64 Value = INDEX_NONE;
		int64 DefaultValue = INDEX_NONE;

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
			Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
				EnumProperty->ContainerPtrToValuePtr<void>(Material));
			DefaultValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
				EnumProperty->ContainerPtrToValuePtr<void>(DefaultMaterial));
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
			if (!Enum)
			{
				return;
			}

			Value = ByteProperty->GetPropertyValue_InContainer(Material);
			DefaultValue = ByteProperty->GetPropertyValue_InContainer(DefaultMaterial);
		}
		else
		{
			return;
		}

		if (Value == DefaultValue)
		{
			return;
		}

		Lines.Add(FString::Printf(
			TEXT("\t\t%s = \"%s\";"),
			PropertyName,
			*GetEnumLiteralText(Enum, Value)));
	}

	void AppendAdditionalMaterialSettings(TArray<FString>& Lines, const UMaterial* Material)
	{
		const TCHAR* BoolSettingNames[] =
		{
			TEXT("TwoSided"),
			TEXT("Wireframe"),
			TEXT("DitheredLODTransition"),
			TEXT("DitherOpacityMask"),
			TEXT("bAllowNegativeEmissiveColor"),
			TEXT("bCastDynamicShadowAsMasked"),
			TEXT("bEnableResponsiveAA"),
			TEXT("bScreenSpaceReflections"),
			TEXT("bContactShadows"),
			TEXT("bDisableDepthTest"),
			TEXT("bOutputTranslucentVelocity"),
			TEXT("bTangentSpaceNormal"),
			TEXT("bFullyRough"),
			TEXT("bIsSky"),
			TEXT("bIsThinSurface"),
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
			TEXT("bHasPixelAnimation"),
#endif
			TEXT("bUsedWithSkeletalMesh"),
			TEXT("bUsedWithMorphTargets"),
			TEXT("bUsedWithClothing"),
			TEXT("bUsedWithNanite"),
			TEXT("bUsedWithEditorCompositing"),
			TEXT("bUsedWithParticleSprites"),
			TEXT("bUsedWithBeamTrails"),
			TEXT("bUsedWithMeshParticles"),
			TEXT("bUsedWithNiagaraSprites"),
			TEXT("bUsedWithNiagaraRibbons"),
			TEXT("bUsedWithNiagaraMeshParticles"),
			TEXT("bUsedWithGeometryCache"),
			TEXT("bUsedWithStaticLighting"),
			TEXT("bUsedWithSplineMeshes"),
			TEXT("bUsedWithInstancedStaticMeshes"),
			TEXT("bUsedWithGeometryCollections"),
			TEXT("bUsedWithHairStrands"),
			TEXT("bUsedWithWater"),
			TEXT("bUsedWithVirtualHeightfieldMesh"),
			TEXT("bCastRayTracedShadows"),
			TEXT("bWriteOnlyAlpha"),
			TEXT("BlendableOutputAlpha"),
			TEXT("bAlwaysEvaluateWorldPositionOffset")
		};

		for (const TCHAR* BoolSettingName : BoolSettingNames)
		{
			AppendBoolMaterialSettingIfDifferent(Lines, Material, BoolSettingName);
		}

		const TCHAR* EnumSettingNames[] =
		{
			TEXT("MaterialDecalResponse")
		};

		for (const TCHAR* EnumSettingName : EnumSettingNames)
		{
			AppendEnumMaterialSettingIfDifferent(Lines, Material, EnumSettingName);
		}
	}

	FExpressionInput* GetMaterialInputForDecompile(UMaterial* Material, const EMaterialProperty Property)
	{
		if (!Material)
		{
			return nullptr;
		}

		if (FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(Property))
		{
			return MaterialInput;
		}

		FMaterialInputDescription Description;
		if (Material->GetExpressionInputDescription(Property, Description))
		{
			return Description.Input;
		}

		return nullptr;
	}
}
