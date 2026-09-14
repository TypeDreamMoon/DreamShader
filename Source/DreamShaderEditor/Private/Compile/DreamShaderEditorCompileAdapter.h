#pragma once

#include "DreamShaderCompileService.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"

namespace UE::DreamShader::Editor
{
	class FEditorCompileAdapter final : public UE::DreamShader::Compiler::IDreamShaderCompiler
	{
	public:
		virtual UE::DreamShader::Compiler::FDreamShaderCompileResult CompileAssets(const UE::DreamShader::Compiler::FDreamShaderCompileRequest& Request) override;
		virtual UE::DreamShader::Compiler::FDreamShaderCompileResult CompileMaterial(const UE::DreamShader::Compiler::FDreamShaderCompileRequest& Request) override;
	};

	FEditorCompileAdapter& GetEditorCompileAdapter();
}
