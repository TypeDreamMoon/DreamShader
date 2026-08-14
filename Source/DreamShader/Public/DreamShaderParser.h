#pragma once

#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
	class DREAMSHADER_API FTextShaderParser
	{
	public:
		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FText& OutError);
		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FString& OutError)
		{
			FText Error;
			const bool bResult = Parse(SourceText, OutDefinition, Error);
			OutError = Error.ToString();
			return bResult;
		}
	};
}
