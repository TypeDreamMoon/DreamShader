#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"

class UMaterialInterface;
class UMaterialFunction;
class UToolMenu;
struct FFileChangeData;
struct FToolMenuSection;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderEditorBridge : public TSharedFromThis<FDreamShaderEditorBridge, ESPMode::ThreadSafe>
	{
	public:
		struct FDiagnosticRecord
		{
			FString FilePath;
			FString Message;
			FString Detail;
			FString Stage;
			FString AssetPath;
			FString ShaderPlatform;
			FString QualityLevel;
			FString Code;
			int32 Line = 1;
			int32 Column = 1;
			FString Severity = TEXT("error");
			FString Source = TEXT("DreamShader");
		};

		void Startup();
		void Shutdown();

	private:
		static FString GetBridgeDirectory();
		static FString GetRequestDirectory();
		static FString GetDiagnosticsFilePath();
		static FString GetSourceFileMetadata(UObject* Asset);

		void QueueFullScan();
		void QueueSourceFile(const FString& SourceFilePath);
		void QueueDependentMaterialsForHeader(const FString& HeaderFilePath);
		void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
		bool Tick(float DeltaSeconds);
		void ProcessRequestFiles();
		void ProcessReadyFiles();
		void ProcessSourceFile(const FString& SourceFilePath);
		void OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface);
		void RegisterMenus();
		void PopulateMaterialFunctionAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialFunctionEditorToolbar(FToolMenuSection& InSection);
		void PopulateMaterialFunctionDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void RequestRecompileAll();
		void RequestCleanGeneratedShaders();
		void OpenDreamShaderWorkspace();
		void CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void OpenVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CleanGeneratedShaderDirectory();
		void RebuildDependencyGraph();
		void SyncVirtualFunctionDefinitions();
		void SetDiagnostics(const FString& SourceFilePath, TArray<FDiagnosticRecord>&& Diagnostics);
		void ClearDiagnostics(const FString& SourceFilePath);
		void ClearDiagnosticsForSourceAndDependencies(const FString& SourceFilePath);
		void UpdateDiagnosticsFile() const;
		TArray<FDiagnosticRecord> BuildErrorDiagnostics(const FString& SourceFilePath, const FString& ErrorMessage) const;

	private:
		TMap<FString, double> PendingFiles;
		TMap<FString, TArray<FDiagnosticRecord>> DiagnosticsByFile;
		TMap<FString, TSet<FString>> HeaderDependentsByFile;
		FString WatchedSourceDirectory;
		FDelegateHandle DirectoryWatcherHandle;
		FTSTicker::FDelegateHandle TickerHandle;
		FDelegateHandle MaterialCompilationFinishedHandle;
		FDelegateHandle ToolMenusStartupCallbackHandle;
		bool bIsShuttingDown = false;
		bool bMenusRegistered = false;
	};
}
