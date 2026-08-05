#pragma once

#include "CoreMinimal.h"

#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "Bridge/DreamShaderPreviewWebSocketServer.h"

#include "Containers/Ticker.h"

class UMaterialInterface;
class UMaterial;
class UMaterialFunction;
class UToolMenu;
struct FFileChangeData;
struct FToolMenuSection;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderEditorBridge : public TSharedFromThis<FDreamShaderEditorBridge, ESPMode::ThreadSafe>
	{
	public:
		void Startup();
		void Shutdown();

	private:
		static FString GetBridgeDirectory();
		static FString GetRequestDirectory();
		static FString GetDiagnosticsFilePath();
		static FString GetDiagnosticsDirectory();
		static FString GetSourceFileMetadata(UObject* Asset);

		void QueueFullScan();
		void HandlePostEngineInit();
		void HandleSettingsPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
		void GenerateAllInMemoryMaterials();
		void QueueSourceFile(const FString& SourceFilePath);
		void QueueDependentSourcesForImport(const FString& ImportFilePath);
		void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
		bool Tick(float DeltaSeconds);
		// Separate from Tick() (which only runs every 0.1s -- plenty for polling request/ready
		// files on disk, but far too slow for streamed preview frames: it hard-caps deliverable
		// preview frame rate at 10 FPS no matter what dreamshader.previewLiveFrameRate or the
		// panel's FPS control ask for). Registered as its own every-frame ticker so the preview
		// WebSocket server can actually deliver up to the 60 FPS ceiling it now supports.
		bool TickPreview(float DeltaSeconds);
		void ProcessRequestFiles();
		void ProcessReadyFiles();
		void ProcessSourceFile(const FString& SourceFilePath);
		void OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface);
		void RegisterMenus();
		void PopulateMaterialAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialFunctionAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialEditorToolbar(FToolMenuSection& InSection);
		void PopulateMaterialDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterial> Material);
		void PopulateMaterialFunctionDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void RequestRecompileAll();
		void RequestCleanGeneratedShaders();
		void RequestCleanPersistedGeneratedAssets();
		int32 CollectPersistedGeneratedAssets(TArray<UObject*>& OutAssets);
		void ToggleShowInMemoryMaterialsInContentBrowser();
		void OpenDreamShaderWorkspace();
		void ExportMaterialToDreamShaderFile(TWeakObjectPtr<UMaterial> Material);
		void ExportMaterialFunctionToDreamShaderFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void OpenVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionReference(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CleanGeneratedShaderDirectory();
		void RebuildDependencyGraph();
		void SyncVirtualFunctionDefinitions();
		void SetDiagnostics(const FString& SourceFilePath, TArray<FDreamShaderDiagnosticRecord>&& Diagnostics);
		void ClearDiagnostics(const FString& SourceFilePath);
		void ClearDiagnosticsForSourceAndDependencies(const FString& SourceFilePath);
		void UpdateDiagnosticsFile() const;

	private:
		TMap<FString, double> PendingFiles;
		FDreamShaderDiagnosticsStore DiagnosticsStore;
		TUniquePtr<FDreamShaderPreviewWebSocketServer> PreviewWebSocketServer;
		TMap<FString, TSet<FString>> HeaderDependentsByFile;
		/** One registration per source root, keyed by the watched directory. */
		TMap<FString, FDelegateHandle> DirectoryWatcherHandles;
		FTSTicker::FDelegateHandle TickerHandle;
		FTSTicker::FDelegateHandle PreviewTickerHandle;
		FDelegateHandle MaterialCompilationFinishedHandle;
		FDelegateHandle ToolMenusStartupCallbackHandle;
		FDelegateHandle PostEngineInitHandle;
		FDelegateHandle SettingsChangedHandle;
		bool bIsShuttingDown = false;
		bool bMenusRegistered = false;
	};
}
