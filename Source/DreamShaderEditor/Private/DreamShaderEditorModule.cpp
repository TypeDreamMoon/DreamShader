#include "DreamShaderEditorBridge.h"

#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

class FDreamShaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Bridge = MakeUnique<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge>();
		Bridge->Startup();
	}

	virtual void ShutdownModule() override
	{
		if (Bridge)
		{
			Bridge->Shutdown();
			Bridge.Reset();
		}
	}

private:
	TUniquePtr<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge> Bridge;
};

IMPLEMENT_MODULE(FDreamShaderEditorModule, DreamShaderEditor)
