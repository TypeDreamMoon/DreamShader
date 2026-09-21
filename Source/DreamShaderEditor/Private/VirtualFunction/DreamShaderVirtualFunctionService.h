#pragma once

#include "CoreMinimal.h"
#include "DreamShaderTypes.h"

class UMaterialExpressionFunctionOutput;
class UMaterialFunction;

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderVirtualFunctionService
	{
		using FOutputTypeResolver = TFunctionRef<FString(const UMaterialExpressionFunctionOutput*)>;

		static bool BuildDefinition(
			const UMaterialFunction* MaterialFunction,
			FOutputTypeResolver OutputTypeResolver,
			FString& OutDefinition,
			FString& OutError);
		static bool BuildCallTextFromSignature(
			const FString& FunctionName,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			FString& OutCallText,
			FString& OutError);
		static bool BuildCallText(const UMaterialFunction* MaterialFunction, FString& OutCallText, FString& OutError);
		static FString MakeDefinitionFilePath(const UMaterialFunction* MaterialFunction);

		/**
		 * The 2.0 spelling of a VirtualFunction definition, for a `.dss` or a `.dsh`: an `extern` prototype under a `///` block
		 * with `@asset <object path>`, the definition's `@desc`, and `@pin <Param> <Engine Name>` wherever an engine pin name
		 * had to be renamed to an identifier. LegacyDefinition is the 1.x block BuildDefinition writes for MaterialFunction; it
		 * is read back through the legacy front end, so the output rule (a first output named Result is the return value, every
		 * other output an `out` parameter after the inputs) and the type spellings are the front end's own, and printed with
		 * the 2.0 printer.
		 */
		static bool BuildExternPrototype(
			const UMaterialFunction* MaterialFunction,
			const FString& LegacyDefinition,
			FString& OutPrototype,
			FString& OutError);

		/**
		 * A 2.0 call of an extern prototype: `Name(<inputs>, <out targets>)`, arguments named after the parameters. With
		 * bFirstOutputIsReturnValue, Outputs[0] is the return value and is left to the expression the call sits in.
		 */
		static bool BuildExternCallTextFromSignature(
			const FString& FunctionName,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			bool bFirstOutputIsReturnValue,
			FString& OutCallText,
			FString& OutError);
	};
}
