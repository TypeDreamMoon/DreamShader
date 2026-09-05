#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderEditorLaunchUtils
	{
		static bool LaunchVSCodeWorkspace(const FString& WorkspaceFilePath);
		static bool LaunchVSCodeFile(const FString& FilePath, int32 Line = 1, int32 Column = 1);
		static bool LaunchTextFileWithNotepad(const FString& FilePath);
		static bool LaunchTextFileInPreferredEditor(const FString& FilePath, int32 Line = 1, int32 Column = 1);
	};

	struct FDreamShaderWorkspaceService
	{
		static FString GetMaterialExpressionManifestFilePath();
		static FString GetDreamShaderSettingsManifestFilePath();
		static FString GetSubstrateBuiltinsManifestFilePath();
		static FString GetPreprocessorDefinesManifestFilePath();
		static FString GetBridgeDatabaseFilePath();
		static void ResetBridgeDatabase();
		static void ExportDreamShaderSettingsManifest();
		static void ExportMaterialExpressionManifest();
		static void ExportSubstrateBuiltinsManifest();
		static void ExportPreprocessorDefinesManifest();
		static bool WriteDreamShaderWorkspaceFile(FString& OutWorkspaceFilePath, FString& OutError);
	};
}
