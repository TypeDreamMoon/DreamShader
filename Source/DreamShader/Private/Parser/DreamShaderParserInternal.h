#pragma once

#include "DreamShaderParser.h"
#include "DreamShaderVersionCompat.h"

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
		bool Expect(TCHAR Expected, FText& OutError);
		bool TryConsumeKeyword(const TCHAR* Keyword);
		bool ParseIdentifier(FString& OutIdentifier, FText& OutError);
		bool ParseSimpleValue(FString& OutValue, FText& OutError);
		bool ParseAttributes(TMap<FString, FString>& OutAttributes, FText& OutError);
		bool ExtractBalancedBlock(FString& OutBlock, FText& OutError);
		bool ExtractBalancedBlock(FString& OutBlock, int32& OutContentStartIndex, FText& OutError);
	};

	FString RemoveComments(const FString& Input);
	TArray<FString> SplitStatements(const FString& BlockContent);
	TArray<FString> SplitTopLevelDelimited(const FString& Input, TCHAR Delimiter);
	bool SplitTopLevelAssignment(const FString& InText, FString& OutLeft, FString& OutRight);
	bool SplitDeclarationTypeAndName(const FString& InText, FString& OutTypeToken, FString& OutNameToken);
	FString Unquote(const FString& InValue);
	FString UnescapeDreamShaderStringLiteral(const FString& InValue);
	bool ParseScalarLiteral(const FString& InText, double& OutValue);
	bool ParseIntegerLiteral(const FString& InText, int32& OutValue);
	bool ParseVectorLiteral(const FString& InText, FLinearColor& OutColor);
	bool ParseBooleanLiteral(const FString& InText, bool& OutValue);
	bool ParseTextureAssetReference(const FString& InText, FString& OutObjectPath, FText& OutError);
	bool TryResolveUEBuiltinOutputSignature(
		const FString& InFunctionName,
		ETextShaderPropertyType& OutType,
		int32& OutComponentCount);
	bool ParseUEBuiltinPropertyType(
		const FString& InTypeToken,
		FTextShaderPropertyDefinition& OutProperty,
		FText& OutError);
	bool ParsePropertyStatements(const FString& BlockContent, TArray<FTextShaderPropertyDefinition>& OutProperties, FText& OutError);
	bool ParseSettingStatements(const FString& BlockContent, TMap<FString, FString>& OutSettings, FText& OutError);
	bool ParseTypedDeclarationStatement(const FString& Statement, FTextShaderVariableDeclaration& OutDeclaration, FText& OutError);
	bool ParseOutputStatements(
		const FString& BlockContent,
		TArray<FTextShaderVariableDeclaration>& OutOutputDeclarations,
		TArray<FTextShaderOutputBinding>& OutOutputs,
		FText& OutError);
	bool ParseLayoutStatements(const FString& BlockContent, FTextShaderLayout& OutLayout, FText& OutError);
	bool ExtractGraphRegions(
		const FString& InCode,
		FString& OutCode,
		TArray<FTextShaderGraphRegion>& OutRegions,
		FText& OutError);
	bool ParseTypedParameterStatements(const FString& BlockContent, TArray<FTextShaderFunctionParameter>& OutParameters, FText& OutError);
	bool ParseShaderBody(const FString& BodyContent, int32 BodyContentStartIndex, FTextShaderDefinition& OutDefinition, FText& OutError);
	bool ParseFunctionBody(const FString& BodyContent, FTextShaderFunctionDefinition& OutFunction, FText& OutError);
	bool ParseMaterialFunctionBody(const FString& BodyContent, int32 BodyContentStartIndex, FTextShaderMaterialFunctionDefinition& OutFunction, FText& OutError);
	bool ParseVirtualFunctionBody(const FString& BodyContent, FTextShaderVirtualFunctionDefinition& OutFunction, FText& OutError);
}
