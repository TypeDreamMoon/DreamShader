// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

namespace UE::DreamShader::Editor::Private
{
	// Every action the Material Content Browser offers, as one command set so the toolbar, the row
	// context menu, the inspector buttons and the keyboard all run the same FUICommandList bindings.
	class FDreamShaderBrowserCommands : public TCommands<FDreamShaderBrowserCommands>
	{
	public:
		FDreamShaderBrowserCommands();

		virtual void RegisterCommands() override;

		TSharedPtr<FUICommandInfo> Refresh;
		TSharedPtr<FUICommandInfo> CompileSelected;
		TSharedPtr<FUICommandInfo> CompileStale;
		TSharedPtr<FUICommandInfo> CompileAll;
		TSharedPtr<FUICommandInfo> OpenMaterial;
		TSharedPtr<FUICommandInfo> OpenSource;
		TSharedPtr<FUICommandInfo> CreateInstance;
		TSharedPtr<FUICommandInfo> Materialize;
		TSharedPtr<FUICommandInfo> RevealInContentBrowser;
		TSharedPtr<FUICommandInfo> CopySourcePath;
		TSharedPtr<FUICommandInfo> CopyAssetPath;
		TSharedPtr<FUICommandInfo> RevertToSource;
		TSharedPtr<FUICommandInfo> AdoptIntoSource;
		TSharedPtr<FUICommandInfo> DetachFromDreamShader;
		TSharedPtr<FUICommandInfo> ExportSource;
		TSharedPtr<FUICommandInfo> FocusSearch;
		TSharedPtr<FUICommandInfo> ToggleTileView;
	};
}
