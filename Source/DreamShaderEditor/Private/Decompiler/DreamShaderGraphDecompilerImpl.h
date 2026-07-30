// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Declaration of the DreamShader graph-decompiler implementation class, hoisted from the .cpp so its
// member definitions can live out-of-line (DreamShaderGraphDecompilerImpl.cpp).

#pragma once

#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "DreamShaderGraphDecompilerHelpers.h"

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
#include "DreamShaderGraphDecompilerHelpers.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDecompiledExpressionKey
	{
		const UMaterialExpression* Expression = nullptr;
		int32 OutputIndex = 0;

		friend uint32 GetTypeHash(const FDecompiledExpressionKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Expression), GetTypeHash(Key.OutputIndex));
		}

		bool operator==(const FDecompiledExpressionKey& Other) const
		{
			return Expression == Other.Expression && OutputIndex == Other.OutputIndex;
		}
	};

	struct FDecompiledValue
	{
		FString Text;
		FString Type = TEXT("float");
		int32 ComponentCount = 1;
		bool bIsTextureObject = false;
		bool bIsMaterialAttributes = false;
		bool bIsSubstrateMaterial = false;
		bool bIsSimple = true;
	};

	class FDreamShaderGraphDecompiler
	{
	public:
		bool DecompileMaterial(UMaterial* Material, const FString& DecompiledName, FString& OutSourceText, FString& OutError);

		bool DecompileFunction(
			UMaterialFunction* MaterialFunction,
			const FString& DecompiledName,
			EDreamShaderDecompiledFunctionKind FunctionKind,
			FString& OutSourceText,
			FString& OutError);

	private:
		struct FExpressionCallArgument
		{
			FString Name;
			FString Value;
			bool bInput = false;
		};

		struct FDecompiledGraphLayoutComment
		{
			FString Name;
			int32 X = 0;
			int32 Y = 0;
			int32 W = 0;
			int32 H = 0;
			FLinearColor Color = FLinearColor(0.10f, 0.16f, 0.22f, 0.35f);
		};

		void Reset();

		static void AppendSection(TArray<FString>& Lines, const TCHAR* SectionName, const TArray<FString>& LinesA);

		static void AppendSection(TArray<FString>& Lines, const TCHAR* SectionName, const TArray<FString>& LinesA, const TArray<FString>& LinesB);

		FString MakeUniqueName(const FString& DesiredName, const TCHAR* FallbackPrefix);

		bool ContainsFunctionInputName(const FString& Name) const;

		void AddPropertyDeclaration(const FString& Name, const FString& Declaration);

		FString MakeUniquePropertyName(const FString& DesiredName, const TCHAR* FallbackPrefix);

		void AddParameterNameMetadataIfNeeded(TArray<FString>& Entries, const FString& DeclarationName, const FName ParameterName);

		static FString BuildMetadataSuffix(const TArray<FString>& Entries);

		static void AddStringMetadata(TArray<FString>& Entries, const TCHAR* Key, const FString& Value);

		static void AddIntMetadata(TArray<FString>& Entries, const TCHAR* Key, const int32 Value, const int32 DefaultValue);

		static void AddBoolMetadata(TArray<FString>& Entries, const TCHAR* Key, const bool bValue, const bool bDefaultValue);

		static void AddEnumMetadata(TArray<FString>& Entries, const TCHAR* Key, const UEnum* Enum, const int64 Value, const int64 DefaultValue);

		static void AddEnumMetadataAlways(TArray<FString>& Entries, const TCHAR* Key, const UEnum* Enum, const int64 Value);

		static FString BuildLiteralEnumArgument(const UEnum* Enum, const int64 Value);

		static void AddParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionParameter* Parameter);

		static void AddTextureParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureSampleParameter* Parameter);

		static void AddTextureParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureObjectParameter* Parameter);

		static void AddTextureMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureBase* TextureExpression);

		static void AddTextureSampleMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureSample* TextureSample);

		static bool HasTextureSampleGraphInputs(const UMaterialExpressionTextureSample* TextureSample);

		void AddTextureSampleExpressionArguments(UMaterialExpressionTextureSample* TextureSample, TArray<FExpressionCallArgument>& Arguments);

		static void AddTextureSampleParameterExpressionArguments(
			const UMaterialExpressionTextureSampleParameter* TextureParameter,
			TArray<FExpressionCallArgument>& Arguments);

		static int32 GetOutputComponentCount(const UMaterialExpression* Expression, const int32 OutputIndex);

		static int32 GetComponentCountForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex);

		static bool IsTextureObjectOutput(UMaterialExpression* Expression, const int32 OutputIndex);

		static int32 FindExpressionOutputIndexByName(
			const UMaterialExpression* Expression,
			const TCHAR* OutputName,
			const int32 FallbackIndex);

		static FDecompiledValue MakeValue(
			const FString& Text,
			const FString& Type,
			const int32 ComponentCount,
			const bool bIsSimple,
			const bool bIsTextureObject = false,
			const bool bIsMaterialAttributes = false,
			const bool bIsSubstrateMaterial = false);

		static FDecompiledValue MakeExpressionValue(
			UMaterialExpression* Expression,
			const int32 OutputIndex,
			const FString& Text,
			const bool bIsSimple);

		static FDecompiledValue MakeDefaultValueForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex);

		FDecompiledValue MakeSwizzledValue(const FDecompiledValue& Source, const FString& SwizzleText);

		FString AddTemp(const FString& Type, const FString& ExpressionText, const FString& BaseName);

		FString AddTempWithName(const FString& Type, const FString& ExpressionText, const FString& Name);

		void RegisterExpressionName(UMaterialExpression* Expression, const FString& Name);

		FString AddTempForExpression(UMaterialExpression* Expression, const FString& Type, const FString& ExpressionText, const FString& BaseName);

		FString AddTempWithNameForExpression(UMaterialExpression* Expression, const FString& Type, const FString& ExpressionText, const FString& Name);

		static FString FormatGraphAssignment(const FString& Type, const FString& Name, const FString& ExpressionText);

		static FString FormatGraphSetStatement(const FString& TargetName, const FString& ExpressionText);

		static bool IsExpressionInsideComment(const UMaterialExpression* Expression, const FDecompiledGraphLayoutComment& Comment);

		FString FindBestRegionForExpression(const UMaterialExpression* Expression) const;

		void BuildLayoutLines();

		TArray<FString> BuildRegionizedGraphLines(const TArray<FString>& RawGraphLines);

		static bool TryExtractGraphAssignmentName(const FString& Line, FString& OutName);

		void CollectLayoutComments(UMaterial* Material);

		void CollectLayoutComments(UMaterialFunction* MaterialFunction);

		void FinalizeGraphLayoutMetadata();

		static FString IndentMultiline(const FString& Text, const TCHAR* Indent);

		FDecompiledValue AddTempValue(const FDecompiledValue& Value, const FString& BaseName);

		FDecompiledValue AddTempValueWithName(const FDecompiledValue& Value, const FString& Name);

		FDecompiledValue MaybeMaterializeValue(const FDecompiledValue& Value, const FString& BaseName);

		FDecompiledValue CacheExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value);

		FDecompiledValue CacheTempExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, const FString& BaseName);

		FDecompiledValue CacheNamedTempExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, const FString& Name);

		FDecompiledValue CacheReusableExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, UMaterialExpression* Expression);

		bool TracePlainReroutesForDecompile(const FExpressionInput& Input, FExpressionInput& OutInput);

		FString CompileInput(const FExpressionInput& Input, const FString& DefaultText);

		FDecompiledValue CompileInputValue(const FExpressionInput& Input, const FDecompiledValue& DefaultValue);

		FString CompileConnectedOrLiteral(const FExpressionInput& Input, const FString& LiteralText);

		FDecompiledValue CompileConnectedOrLiteralValue(
			const FExpressionInput& Input,
			const FString& LiteralText,
			const FString& Type,
			const int32 ComponentCount);

		FDecompiledValue MakeBinaryValue(
			const FString& Operator,
			const FDecompiledValue& Left,
			const FDecompiledValue& Right);

		FDecompiledValue MakeFunctionValue(const FString& FunctionName, const TArray<FDecompiledValue>& Arguments, const int32 ComponentCount);

		static int32 GetCommonNumericComponentCount(const FDecompiledValue& A, const FDecompiledValue& B);

		FDecompiledValue MakeExpressionValueWithComponentCount(
			UMaterialExpression* Expression,
			const int32 OutputIndex,
			const FString& Text,
			const bool bIsSimple,
			const int32 ComponentCount);

		FString BuildUEExpressionCallWithOutputType(
			UMaterialExpression* Expression,
			const int32 OutputIndex,
			const FString& OutputType,
			const TArray<FExpressionCallArgument>& Arguments) const;

		static bool TryCombineAppendSwizzle(
			const FDecompiledValue& A,
			const FDecompiledValue& B,
			FDecompiledValue& OutValue);

		FString CompileExpression(UMaterialExpression* Expression, const int32 OutputIndex);

		FDecompiledValue CompileExpressionValue(UMaterialExpression* Expression, const int32 OutputIndex);

		FDecompiledValue CompileNamedRerouteDeclarationValue(
			UMaterialExpressionNamedRerouteDeclaration* Declaration,
			const int32 OutputIndex);

		FString MakeExpressionOutputSelection(const FString& ExpressionText, UMaterialExpression* Expression, const int32 OutputIndex) const;
		static FString MakeExpressionOutputMaskSuffix(UMaterialExpression* Expression, const int32 OutputIndex);
		FDecompiledValue ApplyConnectionInputMask(
			const FDecompiledValue& Value,
			UMaterialExpression* Expression,
			const int32 OutputIndex,
			const FString& InputMaskSuffix);

		FDecompiledValue MakeExpressionOutputValue(FDecompiledValue Source, UMaterialExpression* Expression, const int32 OutputIndex) const;

		FString BuildUEExpressionCall(UMaterialExpression* Expression, const int32 OutputIndex, const TArray<FExpressionCallArgument>& Arguments) const;

		static bool IsControlExpressionArgumentName(const FString& Name);

		static bool IsEditorOnlyExpressionPropertyName(const FString& Name);

		static bool IsReflectedExpressionLiteralProperty(const FProperty* Property);

		static bool IsReflectedPropertyDefaultValue(const UObject* Object, const FProperty* Property);

		static bool TryBuildReflectedExpressionLiteralArgument(
			const UMaterialExpression* Expression,
			const FProperty* Property,
			FExpressionCallArgument& OutArgument);

		void AddReflectedExpressionLiteralArguments(
			const UMaterialExpression* Expression,
			TArray<FExpressionCallArgument>& Arguments) const;

		FString BuildGenericExpressionCall(UMaterialExpression* Expression, const int32 OutputIndex);

		FString BuildCustomExpressionCall(UMaterialExpressionCustom* CustomExpression, const int32 OutputIndex);

		FString BuildMaterialFunctionCall(UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex);

		FString EnsureVirtualFunctionDefinition(UMaterialFunction* MaterialFunction);

		TArray<FString> PropertyDeclarations;
		TSet<FString> PropertyNames;
		TSet<FString> ReservedNames;
		TMap<const UMaterialExpressionFunctionInput*, FString> FunctionInputNames;
		TArray<FString> GraphLines;
		TArray<FString> LayoutLines;
		TMap<const UMaterialExpression*, FString> ExpressionNames;
		TMap<FString, FString> ExpressionRegionNames;
		TArray<FDecompiledGraphLayoutComment> LayoutComments;
		TMap<FDecompiledExpressionKey, FString> ExpressionTemps;
		TMap<FDecompiledExpressionKey, FDecompiledValue> ExpressionValues;
		TSet<FDecompiledExpressionKey> CompilingExpressionKeys;
		TSet<const UMaterialExpressionNamedRerouteDeclaration*> CompilingNamedRerouteDeclarations;
		TSet<FString> TempNames;
		TArray<FString> VirtualFunctionDefinitions;
		TMap<const UMaterialFunction*, FString> VirtualFunctionNames;
		TArray<FString> Warnings;
		int32 NextTempIndex = 0;
		FScopedSlowTask* ActiveDecompileSlowTask = nullptr;
		TSet<const UMaterialExpression*> ProgressVisitedExpressions;

		void EnterExpressionProgressFrame(const UMaterialExpression* Expression);
	};
}
