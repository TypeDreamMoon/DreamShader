#pragma once

#include "DreamShaderParser.h"

namespace UE::DreamShader::Private
{
	struct FScanner
	{
		explicit FScanner(const FString& InSource);

		const FString& Source;
		int32 Index = 0;

		bool IsAtEnd() const;
		TCHAR Peek(int32 Offset = 0) const;
		void SkipIgnored();
		bool TryConsume(TCHAR Expected);
		bool Expect(TCHAR Expected, FString& OutError);
		bool TryConsumeKeyword(const TCHAR* Keyword);
		bool ParseIdentifier(FString& OutIdentifier, FString& OutError);
		bool ParseSimpleValue(FString& OutValue, FString& OutError);
		bool ParseAttributes(TMap<FString, FString>& OutAttributes, FString& OutError);
		bool ExtractBalancedBlock(FString& OutBlock, FString& OutError);
	};

	FString RemoveComments(const FString& Input);
	TArray<FString> SplitStatements(const FString& BlockContent);
	TArray<FString> SplitTopLevelDelimited(const FString& Input, TCHAR Delimiter);
	bool SplitTopLevelAssignment(const FString& InText, FString& OutLeft, FString& OutRight);
	bool SplitDeclarationTypeAndName(const FString& InText, FString& OutTypeToken, FString& OutNameToken);
	FString Unquote(const FString& InValue);
	bool ParseScalarLiteral(const FString& InText, double& OutValue);
	bool ParseIntegerLiteral(const FString& InText, int32& OutValue);
	bool ParseVectorLiteral(const FString& InText, FLinearColor& OutColor);
	bool ParseBooleanLiteral(const FString& InText, bool& OutValue);
	bool ParseTextureAssetReference(const FString& InText, FString& OutObjectPath, FString& OutError);
	bool TryResolveUEBuiltinOutputSignature(
		const FString& InFunctionName,
		ETextShaderPropertyType& OutType,
		int32& OutComponentCount);
	bool ParseUEBuiltinPropertyType(
		const FString& InTypeToken,
		FTextShaderPropertyDefinition& OutProperty,
		FString& OutError);
	bool ParsePropertyStatements(const FString& BlockContent, TArray<FTextShaderPropertyDefinition>& OutProperties, FString& OutError);
	bool ParseSettingStatements(const FString& BlockContent, TMap<FString, FString>& OutSettings, FString& OutError);
	bool ParseTypedDeclarationStatement(const FString& Statement, FTextShaderVariableDeclaration& OutDeclaration, FString& OutError);
	bool ParseOutputStatements(
		const FString& BlockContent,
		TArray<FTextShaderVariableDeclaration>& OutOutputDeclarations,
		TArray<FTextShaderOutputBinding>& OutOutputs,
		FString& OutError);
	bool ParseTypedParameterStatements(const FString& BlockContent, TArray<FTextShaderFunctionParameter>& OutParameters, FString& OutError);
	bool ParseShaderBody(const FString& BodyContent, FTextShaderDefinition& OutDefinition, FString& OutError);
	bool ParseFunctionBody(const FString& BodyContent, FTextShaderFunctionDefinition& OutFunction, FString& OutError);
	bool ParseMaterialFunctionBody(const FString& BodyContent, FTextShaderMaterialFunctionDefinition& OutFunction, FString& OutError);
}
