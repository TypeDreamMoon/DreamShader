#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All);

namespace UE::DreamShader
{
	DREAMSHADER_API FString GetSourceShaderDirectory();
	DREAMSHADER_API FString GetPackageShaderDirectory();
	DREAMSHADER_API FString GetBuiltinShaderLibraryDirectory();
	DREAMSHADER_API FString GetGeneratedShaderDirectory();
	DREAMSHADER_API FString GetGeneratedShaderVirtualDirectory();
	DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
	DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
}

class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
