#pragma once

#include "CoreMinimal.h"

#ifndef DREAMSHADERCOMPILER_API
#define DREAMSHADERCOMPILER_API
#endif

namespace UE::DreamShader::Compiler
{
	struct DREAMSHADERCOMPILER_API FDreamShaderCompileRequest
	{
		FString SourceFilePath;
		bool bForce = false;
		bool bTransient = false;
	};

	struct DREAMSHADERCOMPILER_API FDreamShaderCompileResult
	{
		bool bSucceeded = false;
		FText Message;
	};

	class DREAMSHADERCOMPILER_API IDreamShaderCompiler
	{
	public:
		virtual ~IDreamShaderCompiler() = default;

		virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
		virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
	};
}
