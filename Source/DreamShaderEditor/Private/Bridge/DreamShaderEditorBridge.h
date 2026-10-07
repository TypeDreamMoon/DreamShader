#pragma once

#include "CoreMinimal.h"

#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "Bridge/DreamShaderDivergenceNotice.h"
#include "Bridge/DreamShaderPreviewWebSocketServer.h"

#include "Containers/Ticker.h"

class FJsonObject;
class SNotificationItem;
class UMaterialInterface;
class UMaterial;
class UMaterialFunction;
class UMaterialInstanceConstant;
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
		bool CompileSourceFile(const FString& SourceFilePath, bool bForce, FString& OutMessage);

		/**
		 * Queue a rebuild for source files whose TEXT a plugin tool just rewrote on disk (the asset
		 * rename sync). Dispatches exactly like the directory watcher -- headers fan out to their
		 * dependents, functions rebuild themselves and their dependents, materials rebuild -- but
		 * bypasses the Auto Compile On Save gate: a file the plugin itself edited must be rebuilt, or
		 * the asset and the text it claims to come from would disagree until the user noticed.
		 */
		void RequestRebuildAfterSourceRewrite(const TArray<FString>& RewrittenSourceFiles);

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
		/** Flips the global Ephemeral-materials visibility setting and re-announces every such
		 *  instance to the asset registry. Toasts the new count. */
		void ToggleShowEphemeralMaterials();
		/**
		 * Decompile a hand-authored asset into a new source under the project root's Decompiled/ tree and open it. Toasts.
		 * A material and a material function become a 2.0 `.dss`, a material instance a `.dsi`; the Legacy variants write
		 * the 1.x `.dsm` / `.dsf`, which lives through 2.0.x.
		 */
		void ExportMaterialToDreamShaderFile(TWeakObjectPtr<UMaterial> Material);
		void ExportMaterialFunctionToDreamShaderFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void ExportMaterialInstanceToDreamShaderFile(TWeakObjectPtr<UMaterialInstanceConstant> Instance);
		void ExportMaterialToLegacyDreamShaderFile(TWeakObjectPtr<UMaterial> Material);
		void ExportMaterialFunctionToLegacyDreamShaderFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);

		bool IsBusy() const { return bBusy; }
		const FString& GetBusyAction() const { return BusyAction; }
		const FString& GetLastResult() const { return LastResult; }
		bool IsBridgeOwner() const { return bIsBridgeOwner; }

	private:
#if WITH_DEV_AUTOMATION_TESTS
		friend struct FDreamShaderBridgeTestAccess;
		uint32 ProcessIdForTesting = 0;
		bool bSynchronousDirectoryChangesForTesting = false;
#endif
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
		/** Shared files belong to the owner; acquisition also publishes the new owner's current diagnostics. */
		void InitializeOwnedBridgeFiles();
		void ClearOwnedBridgeFiles();
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

		/**
		 * Queue every project source. Forced when the caller MEANS a rebuild -- Recompile DSM, Clean
		 * Generated Shaders, an explicit recompile request -- because an unsaved asset now carries a
		 * source hash like a saved one, and the non-forced path would skip every unchanged source,
		 * leaving a cleaned shader directory empty. The watcher's compile-on-save stays unforced: the
		 * hash is the whole point there.
		 */
		void QueueFullScan(bool bForce = false);
		void HandlePostEngineInit();
		/** `Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json`, written once every module is loaded and when the workspace is opened. */
		void ExportBuiltinCatalogManifest();
		void HandleSettingsPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
		/** Compile every project source file. Never forces -- see the definition for why. */
		void GenerateAllSources();
		void QueueSourceFile(const FString& SourceFilePath, bool bForce = false);
		void QueueDependentSourcesForImport(const FString& ImportFilePath);
		/**
		 * After a source compiled, the sources that read what it builds, never the source itself:
		 *
		 *   a `.dss` or `.dsi`   every `.dsi` whose Parent resolves to one of its products, transitively
		 *                        (CollectInstanceDependents), so a renamed or retyped parent parameter surfaces as the
		 *                        child's error without a child edit; and every `.dsp` one of whose passes names one of its
		 *                        products as its material (CollectPipelineDependents)
		 *   a `.dsp`             every `.dss` whose UE.DreamPassBuffer reads its buffers (CollectPassBufferDependents): its
		 *                        exports may have moved
		 *
		 * The two pipeline edges only when bBuiltSomething -- the compile wrote an asset rather than skipping on an unchanged
		 * build key: a skip changed nothing the other side reads, and a `.dss` that both reads a pipeline's buffer and is
		 * one of its pass materials would otherwise bounce between the two for ever.
		 */
		void QueueDependents(const FString& SourceFilePath, bool bBuiltSomething);
		void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
		/**
		 * The `.usf` / `.ush` side of the `.dsp` sources: one watch per directory holding a file some `.dsp`'s HLSL pass
		 * compiles from (CollectDreamShaderPipelineShaderDirectories), wherever it is -- mapped or not, under a source root
		 * or not. Refreshed after every `.dsp` compile, failed ones included: a pass whose `.usf` failed its pre-check is
		 * exactly the one whose next `.usf` save must recompile it.
		 */
		void RefreshPassShaderWatchers();
		/** A `.usf` / `.ush` changed: queue every `.dsp` that compiles from it (FindDreamShaderPipelinesUsingShaderFile). */
		void OnPassShaderDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
		bool Tick(float DeltaSeconds);
		// Separate from Tick() (which only runs every 0.1s -- plenty for polling request/ready
		// files on disk, but far too slow for streamed preview frames: it hard-caps deliverable
		// preview frame rate at 10 FPS no matter what dreamshader.previewLiveFrameRate or the
		// panel's FPS control ask for). Registered as its own every-frame ticker so the preview
		// WebSocket server can actually deliver up to the 60 FPS ceiling it now supports.
		bool TickPreview(float DeltaSeconds);
		void ProcessRequestFiles();
		/**
		 * `decompile { asset, out?, format?, sourceFile?, keepAssetPath?, readable? }`: one decompile, written to disk, answered
		 * in the reveal-node envelope plus `outputFile` and `format`.
		 */
		void ServeDecompileRequest(const FString& RequestId, const FJsonObject& Request, double StartedAtSeconds);
		/** `migrate { source, check? }`: one MigrateDreamShaderSource, answered with `outputFile`, `backupFile` and `check`. */
		void ServeMigrateRequest(const FString& RequestId, const FJsonObject& Request, double StartedAtSeconds);
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
		/** Make Ephemeral: deletes the packages of Materialized ThinCustom products. */
		void RequestMakeEphemeral();
		int32 CollectPersistedGeneratedAssets(TArray<UObject*>& OutAssets);
		/**
		 * Adds whichever of the three provenance answers (Revert / Adopt / Detach, see
		 * Provenance/DreamShaderProvenanceActions.h) apply to this asset. Shared by every asset-type
		 * submenu.
		 */
		void PopulateProvenanceActions(FToolMenuSection& InSection, TWeakObjectPtr<UObject> Asset);
		void PopulateMaterialInstanceAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialInstanceDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UObject> Instance);
		/** A plain material instance: the provenance answers when a `.dsi` built it, and Export .dsi. Never the ThinCustom class. */
		void PopulateMaterialInstanceConstantAssetMenu(FToolMenuSection& InSection);
		void PopulateMaterialInstanceConstantDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialInstanceConstant> Instance);
		/** The body of every Export action: one decompile through the editor's decompile seam, written, opened, toasted. */
		void ExportAssetToDreamShaderFile(UObject* Asset, bool bLegacyText);
		void CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void OpenVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionReference(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		/** The 2.0 spelling of Copy / Create definition: an `extern` prototype with `/// @asset`, for a `.dss` or a `.dsh`. */
		void CopyVirtualFunctionPrototype(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CreateVirtualFunctionPrototypeFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction);
		void CleanGeneratedShaderDirectory();
		void RebuildDependencyGraph();
		void SyncVirtualFunctionDefinitions();
		void SetDiagnostics(
			const FString& SourceFilePath,
			EDreamShaderDiagnosticsProducer Producer,
			TArray<FDreamShaderDiagnosticRecord>&& Diagnostics,
			const FString& Scope = FString());
		/** A compile's records, replacing what this source's last compile filed. See the definition. */
		void SetCompileDiagnostics(const FString& SourceFilePath, TArray<FDreamShaderDiagnosticRecord>&& Diagnostics);
		/** The file was deleted: FDreamShaderDiagnosticsStore::ClearDiagnostics. */
		void ClearDiagnostics(const FString& SourceFilePath);
		void UpdateDiagnosticsFile();

		/**
		 * The divergence notification (Bridge/DreamShaderDivergenceNotice.h, Docs/generation/divergence.md).
		 *
		 * A refused rebuild used to reach the user as prose in the log telling them to right-click an
		 * asset -- which for an Ephemeral product has no tile to right-click. These turn
		 * that refusal into a toast carrying the three resolutions themselves.
		 */
		/** Opens/closes one rebuild round. Nested calls join the round in progress; the outermost
		 *  close is where a collapsed round emits its single summary. */
		void BeginDivergenceRound();
		void EndDivergenceRound();
		/** Inspects one failed compile; does nothing unless the failure is a divergence refusal. */
		void ReportDivergenceRefusal(const FString& SourceFilePath, const FString& CompileMessage);
		void ShowDivergenceNotification(const FDreamShaderDivergenceReport& Report);
		void ShowDivergenceSummaryNotification(int32 DivergedAssetCount);
		/** Retires every live divergence toast. Called from Shutdown. */
		void DismissDivergenceNotifications();
		/** False wherever there is no Slate application or no person: commandlets, unattended runs. */
		static bool CanShowDivergenceNotification();

	private:
		TMap<FString, double> PendingFiles;
		/** The subset of PendingFiles queued with force; consumed when the file is compiled. */
		TSet<FString> ForcedPendingFiles;
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
		/** One registration per directory holding a `.dsp` pass's shader files, keyed by the directory (RefreshPassShaderWatchers). */
		TMap<FString, FDelegateHandle> PassShaderWatcherHandles;
		FTSTicker::FDelegateHandle TickerHandle;
		FTSTicker::FDelegateHandle PreviewTickerHandle;
		FDelegateHandle MaterialCompilationFinishedHandle;
		FDelegateHandle ToolMenusStartupCallbackHandle;
		FDelegateHandle PostEngineInitHandle;
		FDelegateHandle SettingsChangedHandle;
		/**
		 * The preprocessor define-table revision the materials in THIS process were last generated
		 * against. Stamped by GenerateAllSources; compared by Tick.
		 *
		 * Polled rather than driven by an event, because no event covers the whole table. Of the four
		 * tiers only UDreamShaderSettings announces a change; RegisterDreamShaderDefine,
		 * UnregisterDreamShaderDefinesFrom and the provider delegates announce nothing, and a plugin
		 * contributing a switch after startup is precisely the case conditionals were asked for.
		 *
		 * Nor should a hook be trusted where one exists. The settings override bumps the revision
		 * BEFORE calling Super specifically so that the OnObjectPropertyChanged broadcast -- which
		 * UObject::PostEditChangeProperty fires as its very first statement -- reaches this bridge with
		 * the new value already in place. That is correct and deliberate, and it is also one statement
		 * order away from being wrong, in a file this class does not own. A uint32 compare at the
		 * bridge's 10Hz tick costs nothing and cannot be broken from the outside.
		 */
		uint32 LastGeneratedDefineRevision = 0;
		/**
		 * False until the first whole-project sweep has stamped a baseline.
		 *
		 * Needed because 0 is a legal revision and nothing reserves it as "unset": without the flag the
		 * very first tick would compare the live revision against a fabricated 0 and regenerate the
		 * whole project for nothing -- and would do it before post-engine-init, which is the point the
		 * initial sweep is deliberately deferred past because the editor subsystems it needs are not
		 * ready any earlier.
		 */
		bool bDefineRevisionBaselineTaken = false;
		bool bIsShuttingDown = false;
		bool bPostEngineInitComplete = false;
		/** True while this process holds owner.lock. Starts false: ownership is taken, not assumed. */
		bool bIsBridgeOwner = false;
		bool bMenusRegistered = false;
		/** The rebuild round currently draining, and what it has already said about it. */
		FDreamShaderDivergenceNoticeRound DivergenceRound;
		/**
		 * The live actionable toast per diverged asset, keyed by MakeDivergenceNoticeKey.
		 *
		 * Weak on purpose: the notification list owns the item, and an entry whose pointer has
		 * expired is exactly the signal that the toast left the screen and the asset may be reported
		 * again. Holding it shared would keep every toast alive forever and make that test always
		 * answer "still up".
		 */
		TMap<FString, TWeakPtr<SNotificationItem>> DivergenceNotifications;
		FSimpleMulticastDelegate DiagnosticsChangedEvent;
		FSimpleMulticastDelegate SourceTreeChangedEvent;
		FOnSourceFileModified SourceFileModifiedEvent;
	};

	FDreamShaderEditorBridge* GetDreamShaderEditorBridge();
}
