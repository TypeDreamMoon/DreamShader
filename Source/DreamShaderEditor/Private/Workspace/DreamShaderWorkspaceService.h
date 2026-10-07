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
		static FString GetBridgeDirectory();
#if WITH_DEV_AUTOMATION_TESTS
		/** Isolates persistence tests from the running project's bridge files. Empty restores the project directory. */
		static FString BridgeDirectoryForTesting;
#endif
		static FString GetMaterialExpressionManifestFilePath();
		static FString GetDreamShaderSettingsManifestFilePath();
		static FString GetSubstrateBuiltinsManifestFilePath();
		static FString GetPreprocessorDefinesManifestFilePath();
		static FString GetPassKeysManifestFilePath();
		static FString GetBridgeDatabaseFilePath();
		static void ResetBridgeDatabase();
		/** settings.json, and pass-keys.json with it: the layer names it lists are a project setting too. */
		static void ExportDreamShaderSettingsManifest();
		static void ExportMaterialExpressionManifest();
		static void ExportSubstrateBuiltinsManifest();
		static void ExportPreprocessorDefinesManifest();
		/**
		 * pass-keys.json: what a `.dsp` may say, for an editor's completion -- the injection points in frame order, the
		 * pass kinds and the keys each takes, the buffer formats and keys, the binding statements, the filter kinds, the
		 * mesh pass's modes, the view and requirement flags, the built-in buffers, and the project's layer names.
		 */
		static void ExportPassKeysManifest();
		static bool WriteDreamShaderWorkspaceFile(FString& OutWorkspaceFilePath, FString& OutError);
	};
}
