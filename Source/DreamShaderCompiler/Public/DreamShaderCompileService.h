#pragma once

#include "DreamShaderCompilerInterfaces.h"

namespace UE::DreamShader::Compiler
{
	class DREAMSHADERCOMPILER_API FDreamShaderCompileService
	{
	public:
		explicit FDreamShaderCompileService(IDreamShaderCompiler& InCompiler)
			: Compiler(InCompiler)
		{
		}

		/**
		 * Persistence applies to ThinCustom products only (see EThinCustomPersistence); Graph materials
		 * and material functions always save. It defaults to Materialized so a headless caller keeps
		 * writing assets to disk.
		 */
		FDreamShaderCompileResult CompileAssets(const FString& SourceFilePath, bool bForce = false, EThinCustomPersistence Persistence = EThinCustomPersistence::Materialized);
		FDreamShaderCompileResult CompileMaterial(const FString& SourceFilePath, bool bForce = false, EThinCustomPersistence Persistence = EThinCustomPersistence::Materialized);

	private:
		IDreamShaderCompiler& Compiler;
	};
}
