#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"

class UMaterialInterface;
struct FFileChangeData;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderEditorBridge
	{
	public:
		struct FDiagnosticRecord
		{
			FString Message;
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
		void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
		bool Tick(float DeltaSeconds);
		void ProcessRequestFiles();
		void ProcessReadyFiles();
		void ProcessSourceFile(const FString& SourceFilePath);
		void OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface);
		void RegisterMenus();
		void RequestRecompileAll();
		void SetDiagnostics(const FString& SourceFilePath, TArray<FDiagnosticRecord>&& Diagnostics);
		void ClearDiagnostics(const FString& SourceFilePath);
		void UpdateDiagnosticsFile() const;
		TArray<FDiagnosticRecord> BuildErrorDiagnostics(const FString& SourceFilePath, const FString& ErrorMessage) const;

	private:
		TMap<FString, double> PendingFiles;
		TMap<FString, TArray<FDiagnosticRecord>> DiagnosticsByFile;
		FDelegateHandle DirectoryWatcherHandle;
		FTSTicker::FDelegateHandle TickerHandle;
		FDelegateHandle MaterialCompilationFinishedHandle;
	};
}
