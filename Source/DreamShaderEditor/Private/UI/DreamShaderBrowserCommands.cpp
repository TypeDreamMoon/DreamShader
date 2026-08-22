// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/DreamShaderBrowserCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderBrowserCommands::FDreamShaderBrowserCommands()
		: TCommands<FDreamShaderBrowserCommands>(
			TEXT("DreamShaderMaterialBrowser"),
			LOCTEXT("CommandContext", "Material Content Browser"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	void FDreamShaderBrowserCommands::RegisterCommands()
	{
		UI_COMMAND(Refresh, "Refresh", "Rescan the source roots and recompute every status.", EUserInterfaceActionType::Button, FInputChord(EKeys::F5));
		UI_COMMAND(CompileSelected, "Compile", "Force-recompile the selected sources (in memory).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));
		UI_COMMAND(CompileStale, "Compile stale", "Recompile every source whose generated asset is stale or missing.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(CompileAll, "Compile all", "Force-recompile every .dsm/.dsf source (in memory).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::B));
		UI_COMMAND(OpenMaterial, "Open material", "Open the generated material in its asset editor.", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
		UI_COMMAND(OpenSource, "Open source", "Open the .dsm/.dsf/.dsh in your preferred text editor, at the first error when there is one.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Enter));
		UI_COMMAND(CreateInstance, "Create instance", "Create a material instance that shares this material's compiled shader map.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Materialize, "Materialize", "Write this memory-only material (and its base) to disk.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(RevealInContentBrowser, "Reveal in Content Browser", "Select the generated asset in the main Content Browser.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(CopySourcePath, "Copy source path", "Copy the absolute source file path to the clipboard.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(CopyAssetPath, "Copy asset path", "Copy the generated asset's object path to the clipboard.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(RevertToSource, "Revert to Source", "Rebuild this asset from its DreamShader source, discarding every hand edit in it. The source file is not modified.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AdoptIntoSource, "Adopt Into Source", "Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DetachFromDreamShader, "Detach From DreamShader", "Keep this asset exactly as it is and stop DreamShader from ever rebuilding it.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ExportSource, "Export DSM / DSF", "Decompile this hand-authored asset into a DreamShader source file.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(FocusSearch, "Search", "Focus the search box.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F));
		UI_COMMAND(ToggleTileView, "Tiles", "Show the sources as thumbnail tiles instead of a list.", EUserInterfaceActionType::ToggleButton, FInputChord());
	}
}

#undef LOCTEXT_NAMESPACE
