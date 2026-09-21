#include "DreamShaderCompilerModule.h"

#include "DreamShaderCompilerInterface.h"
#include "DreamShaderCompilerService.h"

#define LOCTEXT_NAMESPACE "FDreamShaderCompilerModule"

void FDreamShaderCompilerModule::StartupModule()
{
	// The one registration. Everything outside this module reaches the compiler through
	// ::UE::DreamShader::GetDreamShaderCompiler(), which loads this module on demand and then answers
	// whatever was registered here.
	::UE::DreamShader::RegisterDreamShaderCompiler(&::UE::DreamShader::Editor::Compiler::FDreamShaderCompilerService::Get());
}

void FDreamShaderCompilerModule::ShutdownModule()
{
	// Cleared before the DLL goes away, so a late caller gets null rather than a pointer into an unloaded image.
	::UE::DreamShader::RegisterDreamShaderCompiler(nullptr);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDreamShaderCompilerModule, DreamShaderCompiler)
