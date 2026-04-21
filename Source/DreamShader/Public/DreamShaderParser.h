#pragma once

#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
	class DREAMSHADER_API FTextShaderParser
	{
	public:
		static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FString& OutError);
	};
}
