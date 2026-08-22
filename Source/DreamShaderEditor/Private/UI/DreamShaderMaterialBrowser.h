#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	// Registers (and tears down) the "Material Content Browser" nomad tab and its Tools-menu entry. Called
	// once from the editor module's Startup/Shutdown. The tab's content is SDreamShaderBrowserShell.
	class FDreamShaderMaterialBrowser
	{
	public:
		static void Register();
		static void Unregister();

		// Opens (or fronts) the tab and shows the given source file or asset in it.
		static void OpenAndShowSource(const FString& SourceFilePath);
		static void OpenAndShowAsset(const FString& ObjectPath);

		static const FName TabId;
	};
}
