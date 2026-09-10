#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * The DreamShaderLang module has no start-up work: it is a library of pure functions over text.
 * The module object exists so the plugin descriptor can name it and so dependents can link it.
 */
class FDreamShaderLangModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};
