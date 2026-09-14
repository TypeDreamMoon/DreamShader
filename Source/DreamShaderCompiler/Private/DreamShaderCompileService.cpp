#include "DreamShaderCompileService.h"

namespace UE::DreamShader::Compiler
{
	FDreamShaderCompileResult FDreamShaderCompileService::CompileAssets(const FString& SourceFilePath, const bool bForce, const EThinCustomPersistence Persistence)
	{
		FDreamShaderCompileRequest Request;
		Request.SourceFilePath = SourceFilePath;
		Request.bForce = bForce;
		Request.ThinCustomPersistence = Persistence;
		return Compiler.CompileAssets(Request);
	}

	FDreamShaderCompileResult FDreamShaderCompileService::CompileMaterial(const FString& SourceFilePath, const bool bForce, const EThinCustomPersistence Persistence)
	{
		FDreamShaderCompileRequest Request;
		Request.SourceFilePath = SourceFilePath;
		Request.bForce = bForce;
		Request.ThinCustomPersistence = Persistence;
		return Compiler.CompileMaterial(Request);
	}
}
