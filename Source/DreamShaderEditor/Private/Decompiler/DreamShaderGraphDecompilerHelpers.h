// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Free formatting / type-name / swizzle / material-settings helpers for the DreamShader graph
// decompiler. Extracted byte-for-byte from DreamShaderGraphDecompiler.cpp's anonymous namespace and
// promoted to external linkage so the (file-local) decompiler class can call them across TUs.

#pragma once

#include "CoreMinimal.h"
#include "SceneTypes.h"

// EMaterialDomain / EBlendMode, used by the Settings-text declarations below. Neither lives in
// SceneTypes.h, and a unity build only happened to have them already through a neighbouring TU: a
// non-unity compile of this header (a -SingleFile build, say) does not.
#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionCustom.h"

class UMaterial;
class UMaterialFunction;
class UMaterialExpression;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionMaterialFunctionCall;
class UEnum;
class UClass;
class UObject;
struct FExpressionInput;

namespace UE::DreamShader::Editor::Private
{
	FString EscapeDreamShaderString(const FString& InText);
	FString EscapeDreamShaderCodeString(const FString& InText);
	FString GetDreamShaderTypeForFunctionInput(EFunctionInputType InputType);
	FString GetDreamShaderTypeForMaterialValueType(EMaterialValueType ValueType);
	FString MakeDreamShaderDeclarationName(const FString& InName, const TCHAR* FallbackPrefix, int32 Index);
	FString MakeFunctionParameterMetadataSuffix(
		const FString& Description,
		const int32 SortPriority,
		const int32 DefaultSortPriority);
	FString MakePreviewValueText(EFunctionInputType InputType, const FVector4f& PreviewValue);
	bool BuildVirtualFunctionDefinition(const UMaterialFunction* MaterialFunction, FString& OutDefinition, FString& OutError);
	bool BuildVirtualFunctionCallText(const UMaterialFunction* MaterialFunction, FString& OutCallText, FString& OutError);
	FString FormatDreamShaderFloat(const double Value);
	FString FormatDreamShaderVector2(const double X, const double Y);
	FString FormatDreamShaderVector3(const double X, const double Y, const double Z);
	FString FormatDreamShaderVector4(const double X, const double Y, const double Z, const double W);
	FString FormatDreamShaderColor(const FLinearColor& Color);
	FString WrapExpressionForSuffix(const FString& ExpressionText);
	bool IsSwizzleComponentChar(const TCHAR Character);
	bool IsSwizzleText(const FString& Text);
	int32 GetSwizzleComponentIndex(const TCHAR Character);
	bool TrySplitTrailingSwizzle(const FString& ExpressionText, FString& OutBaseText, FString& OutSwizzleText);
	bool TryComposeTrailingSwizzle(
		const FString& ExpressionText,
		const FString& RequestedSwizzle,
		FString& OutExpressionText);
	FString MakeInputMaskSuffix(const FExpressionInput& Input);
	FString MakeSwizzleExpression(const FString& ExpressionText, const FString& SwizzleText);
	FString ApplyInputMask(const FString& ExpressionText, const FExpressionInput& Input);
	FString MakeDreamShaderObjectPathLiteral(const UObject* Object);
	FString GetDreamShaderTypeForCustomOutputType(const ECustomMaterialOutputType OutputType);
	FString GetDreamShaderTypeForComponentCount(const int32 ComponentCount);
	int32 GetComponentCountForFunctionInputType(const EFunctionInputType InputType);
	int32 GetOutputMaskComponentCount(const UMaterialExpression* Expression, const int32 OutputIndex);
	UMaterialExpressionFunctionOutput* ResolveFunctionCallOutputExpression(UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex);
	int32 GetExpressionOutputComponentCount(UMaterialExpression* Expression, const int32 OutputIndex, TSet<UMaterialExpression*>* VisitingExpressions = nullptr);
	FString GetDreamShaderTypeForFunctionOutput(const UMaterialExpressionFunctionOutput* OutputExpression);
	FString GetEnumLiteralText(const UEnum* Enum, const int64 Value);
	FString GetDreamShaderTypeForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex);
	FString GetCustomOutputName(const UMaterialExpressionCustom* CustomExpression, const int32 OutputIndex);
	FString GetFunctionCallOutputName(const UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex);
	FString GetMaterialExpressionShortName(const UClass* Class);
	FString GetMaterialDomainText(const EMaterialDomain Domain);
	FString GetBlendModeText(const EBlendMode BlendMode);
	FString GetShadingModelText(const UMaterial* Material);
	/**
	 * Writes a bool Settings line only when the material would not get this value anyway. The
	 * baseline is the UMaterial CDO unless DefaultValueOverride names a different one, which is
	 * what a setting the generator derives from another setting needs (see
	 * GetDefaultUsedWithVolumetricCloud).
	 */
	void AppendBoolMaterialSettingIfDifferent(
		TArray<FString>& Lines,
		const UMaterial* Material,
		const TCHAR* PropertyName,
		const TOptional<bool> DefaultValueOverride = TOptional<bool>());
	void AppendEnumMaterialSettingIfDifferent(
		TArray<FString>& Lines,
		const UMaterial* Material,
		const TCHAR* PropertyName);
	void AppendAdditionalMaterialSettings(TArray<FString>& Lines, const UMaterial* Material);
	FExpressionInput* GetMaterialInputForDecompile(UMaterial* Material, const EMaterialProperty Property);
}
