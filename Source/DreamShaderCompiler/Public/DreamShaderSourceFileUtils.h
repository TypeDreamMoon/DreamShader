#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	struct DREAMSHADERCOMPILER_API FDreamShaderSourceFileUtils
	{
		static bool IsPathUnderDirectory(const FString& InPath, const FString& InDirectory);
		static bool IsPackageMaterialFile(const FString& InPath);
		static void FindProjectDreamShaderSourceFiles(TArray<FString>& OutSourceFiles);
		static void FindProjectMaterialSourceFiles(TArray<FString>& OutSourceFiles);
	};
}
