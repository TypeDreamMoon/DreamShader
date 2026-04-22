#pragma once

#include "CoreMinimal.h"
#include "DreamShaderTypes.h"

#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "SceneTypes.h"

class UMaterial;
class UMaterialFunction;
class UMaterialExpression;
class UClass;
class FProperty;

namespace UE::DreamShader::Editor::Private
{
	struct FResolvedMaterialProperty
	{
		EMaterialProperty Property = MP_EmissiveColor;
		ECustomMaterialOutputType OutputType = CMOT_Float1;
	};

	struct FResolvedNamedOutput
	{
		FString Name;
		ECustomMaterialOutputType OutputType = CMOT_Float1;
	};

	enum class ECodeTokenType : uint8
	{
		Identifier,
		Number,
		String,
		LeftParen,
		RightParen,
		Comma,
		Dot,
		Plus,
		Minus,
		Star,
		Slash,
		Equals,
		ScopeResolution,
		End,
	};

	struct FCodeToken
	{
		ECodeTokenType Type = ECodeTokenType::End;
		FString Text;
	};

	enum class ECodeExpressionKind : uint8
	{
		Name,
		NumberLiteral,
		StringLiteral,
		Call,
		MemberAccess,
		Binary,
		Unary,
	};

	struct FCodeExpression;

	struct FCodeCallArgument
	{
		FString Name;
		TSharedPtr<FCodeExpression> Expression;
		bool bIsNamed = false;
	};

	struct FCodeExpression
	{
		ECodeExpressionKind Kind = ECodeExpressionKind::Name;
		FString Text;
		TSharedPtr<FCodeExpression> Left;
		TSharedPtr<FCodeExpression> Right;
		TArray<FCodeCallArgument> Arguments;
	};

	struct FCodeCondition
	{
		FString Operator;
		TSharedPtr<FCodeExpression> Left;
		TSharedPtr<FCodeExpression> Right;
	};

	struct FCodeStatement
	{
		bool bIsDeclaration = false;
		bool bIsExpressionStatement = false;
		bool bIsIfStatement = false;
		bool bUsesBraceInitializer = false;
		FString DeclaredType;
		FString TargetName;
		FString InitializerText;
		TSharedPtr<FCodeExpression> Expression;
		FCodeCondition Condition;
		TArray<FCodeStatement> ThenStatements;
		TArray<FCodeStatement> ElseStatements;
	};

	struct FCodeValue
	{
		UMaterialExpression* Expression = nullptr;
		int32 OutputIndex = 0;
		int32 ComponentCount = 1;
		bool bIsTextureObject = false;
	};

	bool ParseCodeStatements(const FString& InCode, TArray<FCodeStatement>& OutStatements, FString& OutError);

	bool ResolveMaterialProperty(const FString& InName, FResolvedMaterialProperty& OutProperty);
	bool TryResolveCustomOutputType(const FString& InTypeName, ECustomMaterialOutputType& OutOutputType);
	bool ParseScalarLiteral(const FString& InText, double& OutValue);
	bool ParseBooleanLiteral(const FString& InText, bool& OutValue);
	bool ParseIntegerLiteral(const FString& InText, int32& OutValue);
	bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FString& OutError);
	UMaterialExpression* CreateScalarLiteralExpression(UMaterial* Material, double Value, int32 PositionY);
	FString EnsureTopLevelReturn(const FString& InHLSL);
	bool PrepareCustomNodeCode(
		const FTextShaderDefinition& Definition,
		const FString& SourceCode,
		const TArray<FString>& RequestedEmbeddedFunctionNames,
		const FString& WrapperNameHint,
		FString& OutCode,
		bool& bOutUsesGeneratedInclude,
		FString& OutError);
	bool IsTextureFunctionParameterType(const FString& InTypeName);
	FString BuildGeneratedFunctionSymbolName(const FTextShaderFunctionDefinition& Function);
	FString BuildGeneratedIncludeVirtualPath(const FString& SourceFilePath);
	bool WriteGeneratedInclude(const FString& SourceFilePath, const FTextShaderDefinition& Definition, FString& OutError);
	void ClearMaterialExpressions(UMaterial* Material);
	void ClearMaterialFunctionExpressions(UMaterialFunction* MaterialFunction);
	void ResetMaterialToDefaults(UMaterial* Material);
	bool ValidateSettings(const FTextShaderDefinition& Definition, FString& OutError);
	bool ApplySettings(UMaterial* Material, const FTextShaderDefinition& Definition, FString& OutError);
	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		int32 PositionY,
		FString& OutError);
	bool TryGetComponentCountForOutputType(ECustomMaterialOutputType OutputType, int32& OutComponentCount);
	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture);
	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture);
	bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FString& OutError);
	bool CreateOrReuseMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FString& OutError);
	bool TryResolveMaterialFunctionParameterType(
		const FString& InTypeName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		int32& OutFunctionInputTypeValue);
	bool ValidateOutputs(
		const FTextShaderDefinition& Definition,
		TArray<FResolvedNamedOutput>& OutNamedOutputs,
		bool& bOutUsesReturn,
		ECustomMaterialOutputType& OutReturnType,
		FString& OutError);
	FString BuildSourceHash(const FString& SourceText);
	bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath);
	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	bool SaveAssetPackage(UObject* Asset, FString& OutError);
	UClass* ResolveMaterialExpressionClass(const FString& ClassSpecifier);
	FProperty* FindMaterialExpressionArgumentProperty(UClass* ExpressionClass, const FString& ArgumentName);
	bool IsMaterialExpressionInputProperty(const FProperty* Property);
	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, const FString& ValueText, FString& OutError);

	class FCodeGraphBuilder
	{
	public:
		FCodeGraphBuilder(
			UMaterial* InMaterial,
			UMaterialFunction* InMaterialFunction,
			const FTextShaderDefinition& InDefinition,
			const FString& InSourceFilePath,
			const FString& InIncludeVirtualPath);

		bool Build(
			const TArray<FCodeStatement>& Statements,
			TMap<FString, FCodeValue>& InOutValues,
			FString& OutError);
		bool EvaluateOutputExpression(const FString& ExpressionText, FCodeValue& OutValue, FString& OutError);

	private:
		UMaterial* Material = nullptr;
		UMaterialFunction* MaterialFunction = nullptr;
		const FTextShaderDefinition& Definition;
		FString SourceFilePath;
		FString IncludeVirtualPath;
		TMap<FString, FCodeValue>* Values = nullptr;
		int32 NextNodeY = 320;

		FCodeValue* FindValue(const FString& Name) const;
		int32 ConsumeNodeY();
		UMaterialExpression* CreateExpression(TSubclassOf<UMaterialExpression> ExpressionClass, int32 PositionX, int32 PositionY) const;
		UMaterialExpression* CreateScalarLiteralNode(double Value, int32 PositionY) const;
		bool CreateDefaultValue(const FString& DeclaredType, FCodeValue& OutValue, FString& OutError);
		bool CoerceValueToType(const FCodeValue& InValue, int32 ExpectedComponentCount, bool bExpectedTexture, FCodeValue& OutValue, FString& OutError);
		bool EvaluateBraceInitializer(const FString& ConstructorType, const FString& InitializerText, FCodeValue& OutValue, FString& OutError);
		bool ResolveTargetTypeForAssignment(const FCodeStatement& Statement, FString& OutTypeName, FString& OutError) const;

		static bool TryFlattenQualifiedName(const TSharedPtr<FCodeExpression>& Expression, FString& OutName);
		bool TryExtractTextLiteral(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const;
		bool TryExtractLiteralText(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const;
		bool TryExtractScalarLiteral(const TSharedPtr<FCodeExpression>& Expression, double& OutValue) const;
		bool TryExtractIntegerLiteral(const TSharedPtr<FCodeExpression>& Expression, int32& OutValue) const;
		bool TryExtractBooleanLiteral(const TSharedPtr<FCodeExpression>& Expression, bool& OutValue) const;
		const FCodeCallArgument* FindNamedArgument(const TArray<FCodeCallArgument>& Arguments, const TCHAR* Name) const;
		const FCodeCallArgument* FindPositionalArgument(const TArray<FCodeCallArgument>& Arguments, int32 PositionIndex) const;
		bool ExecuteExpressionStatement(const TSharedPtr<FCodeExpression>& Expression, FString& OutError);
		bool ExecuteStatement(const FCodeStatement& Statement, FString& OutError);
		bool ExecuteIfStatement(const FCodeStatement& Statement, FString& OutError);
		bool CreateConditionalValue(
			const FCodeCondition& Condition,
			const FCodeValue& TrueValue,
			const FCodeValue& FalseValue,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateExpression(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool EvaluateUnary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool EvaluateBinary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool CreateBinaryOperatorNode(
			const FString& Operator,
			const FCodeValue& LeftValue,
			const FCodeValue& RightValue,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateMemberAccess(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool CreateSingleChannelMask(
			const FCodeValue& BaseValue,
			int32 ChannelIndex,
			FCodeValue& OutValue,
			FString& OutError);
		bool AppendValues(const TArray<FCodeValue>& Parts, FCodeValue& OutValue, FString& OutError);
		bool CreateSwizzleExpression(
			const FCodeValue& BaseValue,
			const FString& Swizzle,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateCall(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		static bool IsVectorConstructorName(const FString& InName);
		static int32 GetConstructorComponentCount(const FString& InName);
		bool EvaluateVectorConstructor(
			const FString& ConstructorName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		const FTextShaderFunctionDefinition* FindFunctionDefinition(const FString& FunctionName) const;
		const FTextShaderMaterialFunctionDefinition* FindMaterialFunctionDefinition(const FString& FunctionName) const;
		bool EvaluateCustomFunctionCall(
			const FString& FunctionName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		bool ExecuteCustomFunctionCall(
			const FTextShaderFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool EvaluateMaterialFunctionCall(
			const FTextShaderMaterialFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		static FString BuildFunctionArgumentList(const FTextShaderFunctionDefinition& Function, const TArray<FString>& ResultVariableNames);
		bool TryResolveVectorTransformBasis(const FString& InText, EMaterialVectorCoordTransformSource& OutSource) const;
		bool TryResolveVectorTransformTarget(const FString& InText, EMaterialVectorCoordTransform& OutTarget) const;
		bool TryResolvePositionTransformBasis(const FString& InText, EMaterialPositionTransformSource& OutBasis) const;
		bool EvaluateUEBuiltinCall(
			const FString& CalleeName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
	};
}
