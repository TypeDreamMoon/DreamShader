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
// At global scope on purpose: the member declaration below used to spell it inline as
// `struct FPropertyChangedEvent&`, which inside a namespace declares a NEW type in that namespace.
// Unity builds hid it (a neighbour had already pulled in the real one); a non-unity compile of this
// file alone -- which is what an adaptive build does to whichever files you are editing -- did not.
struct FPropertyChangedEvent;
struct FToolMenuSection;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderEditorBridge : public TSharedFromThis<FDreamShaderEditorBridge, ESPMode::ThreadSafe>
	{
	public:
		void Startup();
		void Shutdown();
		const TArray<FDreamShaderDiagnosticRecord>* GetDiagnosticsForSource(const FString& SourceFilePath) const { return DiagnosticsStore.FindDiagnostics(SourceFilePath); }

		/**
		 * One compile through the bridge, synchronously. The diagnostics store, diagnostics.json, any
		 * request parked on this file, and OnDiagnosticsChanged all follow from it -- which a direct
		 * FMaterialGenerator call bypasses, leaving the VSCode extension and the Material Content
		 * Browser looking at the previous result. The watcher's own compiles go through here too.
		 */
		bool CompileSourceFile(const FString& SourceFilePath, bool bForce, bool bInMemory, FString& OutMessage);

		/** After the diagnostics store was committed (written out) following one or more compiles. */
		FSimpleMulticastDelegate& OnDiagnosticsChanged() { return DiagnosticsChangedEvent; }
		/** A source file appeared, disappeared, or the watcher asked for a rescan. Fires even when
		 *  auto-compile-on-save is off: the set of files changed regardless of whether they compile. */
		FSimpleMulticastDelegate& OnSourceTreeChanged() { return SourceTreeChangedEvent; }
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSourceFileModified, const FString& /*NormalizedPath*/);
		/** An existing source file's contents changed on disk (before any compile it may trigger). */
		FOnSourceFileModified& OnSourceFileModified() { return SourceFileModifiedEvent; }

		/** Writes the VSCode workspace (and the manifests it needs) and opens it. */
		void OpenDreamShaderWorkspace();
		/** Flips the global in-memory-materials visibility setting and re-announces every such
		 *  instance to the asset registry. Toasts the new count. */
		void ToggleShowInMemoryMaterialsInContentBrowser();
		/** Decompile a hand-authored asset into a new .dsm / .dsf under the project root. Toasts. */
		void ExportMaterialToDreamShaderFile(TWeakObjectPtr<UMaterial> Material);
		void ExportMaterialFunctionToDreamShaderFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);

		bool IsBusy() const { return bBusy; }
		const FString& GetBusyAction() const { return BusyAction; }
		const FString& GetLastResult() const { return LastResult; }
		bool IsBridgeOwner() const { return bIsBridgeOwner; }

	private:
		static FString GetBridgeDirectory();
		static FString GetRequestDirectory();
		static FString GetResponseDirectory();
		static FString GetStatusFilePath();
		static FString GetDiagnosticsFilePath();
		static FString GetDiagnosticsDirectory();
		static FString GetOwnerLockFilePath();

		/**
		 * Bridge ownership: which editor process serves this project's request queue.
		 *
		 * The bridge directory is per-project, so two editors open on the same project were both
		 * consuming the same Requests folder and both overwriting status.json. See the definitions.
		 */
		bool TryAcquireBridgeOwnership();
		void RefreshBridgeOwnershipLock();
		void ReleaseBridgeOwnership();
		static FString GetSourceFileMetadata(UObject* Asset);

		/**
		 * Publishes the heartbeat.
		 *
		 * A client cannot otherwise tell a running editor from a closed one, and inferring it
		 * from `bridge.db` -- which is what external tooling had to do before this -- is a
		 * guess that a hard crash gets wrong in the expensive direction.
		 */
		void PublishStatus();

		/** Answers one request. A request that carried no id gets no response and wants none. */
		void RespondTo(
			const FString& RequestId,
			bool bOk,
			const FString& Message,
			const TArray<FDreamShaderDiagnosticRecord>* Diagnostics = nullptr,
			double DurationMs = 0.0,
			const FString& FallbackFilePath = FString());

		/** One request parked until the compile it asked for finishes. */
		struct FPendingResponse
		{
			FString RequestId;
			/** `FPlatformTime::Seconds()` when the request was accepted, not when it ran. The
			 *  wait for the debounce window is part of what the caller experiences. */
			double AcceptedAtSeconds = 0.0;
		};

		/** True for a request written while nobody was listening. See the definition. */
		bool IsAbandoned(const FString& RequestPath) const;

		/** Completes every request that was waiting on this source file. */
		void ResolvePendingResponses(const FString& SourceFilePath, bool bOk, const FString& Message);

		void QueueFullScan();
		void HandlePostEngineInit();
		void HandleSettingsPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
		/** Materialize every source file in memory. Never forces -- see the definition for why. */
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
		/**
		 * Adds whichever of the three provenance answers (Revert / Adopt / Detach, see
		 * Provenance/DreamShaderProvenanceActions.h) apply to this asset. Shared by every asset-type
		 * submenu.
		 */
		void PopulateProvenanceActions(FToolMenuSection& InSection, TWeakObjectPtr<UObject> Asset);
		void PopulateMaterialInstanceAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialInstanceDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UObject> Instance);
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
		void UpdateDiagnosticsFile();

	private:
		TMap<FString, double> PendingFiles;
		/**
		 * Requests waiting on a compile, keyed by the normalized source path.
		 *
		 * A `recompile` does not finish inside the request poll: the file goes into the
		 * debounce queue and is compiled some ticks later. Answering at dispatch time would
		 * mean reporting success before anything had been attempted, so the id is parked here
		 * and the answer is sent when the compile it asked for actually completes.
		 *
		 * An array because two clients can ask for the same file, and both deserve an answer.
		 */
		TMap<FString, TArray<FPendingResponse>> PendingResponsesBySource;
		/** When this bridge started listening. Anything written before it had no listener. */
		FDateTime ListeningSince = FDateTime::MinValue();
		double LastHeartbeatSeconds = 0.0;
		bool bBusy = false;
		FString BusyAction;
		FString LastResult;
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
		/** True while this process holds owner.lock. Starts false: ownership is taken, not assumed. */
		bool bIsBridgeOwner = false;
		bool bMenusRegistered = false;
		FSimpleMulticastDelegate DiagnosticsChangedEvent;
		FSimpleMulticastDelegate SourceTreeChangedEvent;
		FOnSourceFileModified SourceFileModifiedEvent;
	};

	FDreamShaderEditorBridge* GetDreamShaderEditorBridge();
}
