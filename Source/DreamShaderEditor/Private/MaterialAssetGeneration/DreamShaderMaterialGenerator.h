#pragma once

#include "CoreMinimal.h"

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
	};
}
