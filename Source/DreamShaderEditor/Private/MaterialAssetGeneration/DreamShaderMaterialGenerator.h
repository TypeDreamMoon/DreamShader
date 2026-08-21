#pragma once

#include "CoreMinimal.h"

// FDreamShaderError: the code-carrying overloads below. The FString ones stay so callers that
// only want the text -- notifications, tests -- are unchanged; the diagnostics store takes the
// other pair, so a DSHnnnn code survives the trip out of the generator.
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader::Editor
{
	class FMaterialGenerator
	{
	public:
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FText& OutMessage, bool bForce = false, bool bTransient = false)
		{
			FString Message;
			const bool bResult = GenerateAssetsFromFile(SourceFilePath, Message, bForce, bTransient);
			OutMessage = FText::FromString(Message);
			return bResult;
		}
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FText& OutMessage, bool bForce = false, bool bTransient = false)
		{
			FString Message;
			const bool bResult = GenerateMaterialFromFile(SourceFilePath, Message, bForce, bTransient);
			OutMessage = FText::FromString(Message);
			return bResult;
		}
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FString& OutMessage, bool bForce = false, bool bTransient = false);
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FString& OutMessage, bool bForce = false, bool bTransient = false);
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce = false, bool bTransient = false);
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce = false, bool bTransient = false);
	};
}
