#include "DreamShaderEditorCompileAdapter.h"

// The 1.x generator still spells the ThinCustom state as a "bTransient" bool; M4 deletes that
// parameter with the generator. Ephemeral is the only value that maps to it -- Graph materials and
// material functions ignore it (see EThinCustomPersistence).
namespace UE::DreamShader::Editor
{
	UE::DreamShader::Compiler::FDreamShaderCompileResult FEditorCompileAdapter::CompileAssets(const UE::DreamShader::Compiler::FDreamShaderCompileRequest& Request)
	{
		UE::DreamShader::Compiler::FDreamShaderCompileResult Result;
		Result.bSucceeded = FMaterialGenerator::GenerateAssetsFromFile(Request.SourceFilePath, Result.Message, Request.bForce,
			Request.ThinCustomPersistence == UE::DreamShader::Compiler::EThinCustomPersistence::Ephemeral);
		return Result;
	}

	UE::DreamShader::Compiler::FDreamShaderCompileResult FEditorCompileAdapter::CompileMaterial(const UE::DreamShader::Compiler::FDreamShaderCompileRequest& Request)
	{
		UE::DreamShader::Compiler::FDreamShaderCompileResult Result;
		Result.bSucceeded = FMaterialGenerator::GenerateMaterialFromFile(Request.SourceFilePath, Result.Message, Request.bForce,
			Request.ThinCustomPersistence == UE::DreamShader::Compiler::EThinCustomPersistence::Ephemeral);
		return Result;
	}

	FEditorCompileAdapter& GetEditorCompileAdapter()
	{
		static FEditorCompileAdapter Adapter;
		return Adapter;
	}
}
