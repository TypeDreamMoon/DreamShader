// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCompilerInterface.h. The registry is one pointer, owned by nobody: the compiler module
// registers its service when it starts and clears the pointer when it shuts down.
//
// Out of line, in this module's DLL, on purpose: a registry held in an inline function's static would
// exist once per DLL, and the editor module would never see what the compiler module registered.

#include "DreamShaderCompilerInterface.h"

#include "Modules/ModuleManager.h"

namespace UE::DreamShader
{
	namespace
	{
		IDreamShaderCompiler* GDreamShaderRegisteredCompiler = nullptr;
	}

	IDreamShaderCompiler* GetDreamShaderCompiler()
	{
#if WITH_EDITOR
		// On demand, but only where loading a module is legal and meaningful: on the game thread, not
		// while the engine exits (that would bring a module back in the middle of teardown), and only
		// when this target has the module at all.
		if (GDreamShaderRegisteredCompiler == nullptr && IsInGameThread() && !IsEngineExitRequested())
		{
			static const FName CompilerModuleName(TEXT("DreamShaderCompiler"));
			FModuleManager& ModuleManager = FModuleManager::Get();
			if (!ModuleManager.IsModuleLoaded(CompilerModuleName) && ModuleManager.ModuleExists(TEXT("DreamShaderCompiler")))
			{
				ModuleManager.LoadModule(CompilerModuleName);
			}
		}
#endif
		return GDreamShaderRegisteredCompiler;
	}

	void RegisterDreamShaderCompiler(IDreamShaderCompiler* Compiler)
	{
		GDreamShaderRegisteredCompiler = Compiler;
	}
}
