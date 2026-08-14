#pragma once

#include "DreamShaderDiagnostic.h"
#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
	class DREAMSHADER_API FTextShaderParser
	{
	public:
		/**
		 * The primary entry point. OutError carries a DSHnnnn code alongside the message, which is
		 * what the diagnostics store, the corpus expectations and the diagnose skill key off.
		 */
		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FDreamShaderTextError& OutError);

		/**
		 * Code-dropping shims for callers that have not been migrated yet. They are lossy on
		 * purpose: a caller that only wants the message should not have to know a code exists, and
		 * one that wants the code cannot get it by accident from the wrong overload.
		 */
		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FText& OutError)
		{
			FDreamShaderTextError Error;
			const bool bResult = Parse(SourceText, OutDefinition, Error);
			OutError = Error.Message;
			return bResult;
		}

		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FString& OutError)
		{
			FDreamShaderTextError Error;
			const bool bResult = Parse(SourceText, OutDefinition, Error);
			OutError = Error.Message.ToString();
			return bResult;
		}
	};
}
