#include "DreamShaderEditorBridge.h"
#include "DreamShaderDiagnostic.h"

#include "Bridge/DreamShaderPreviewWebSocketServer.h"
#include "DreamShaderCompileService.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
// The provenance helpers behind the Revert/Adopt/Detach actions, and the source loader + parser the
// Adopt action uses to refuse a file that declares more than one asset.
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.h"
#include "DreamShaderParser.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
// GetDreamShaderDefineRevision, polled in Tick so a define change invalidates the in-memory materials.
#include "DreamShaderDefineTable.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Provenance/DreamShaderProvenanceActions.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"
#include "VirtualFunction/DreamShaderVirtualFunctionService.h"
#include "VirtualFunction/DreamShaderVirtualFunctionSyncService.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "CoreGlobals.h"
#include "DreamShaderMaterialInstance.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
// ON_SCOPE_EXIT, used by GenerateAllInMemoryMaterials to stamp the define revision it swept against
// on every exit path.
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "ContentBrowserMenuContexts.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "IMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "MaterialEditorContext.h"
#include "MaterialShared.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCore.h"
#include "RHIStrings.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		static const FName DreamShaderToolMenuOwnerName(TEXT("DreamShaderEditor"));

		void ShowDreamShaderNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			Info.bUseLargeFont = false;
			if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Notification->SetCompletionState(CompletionState);
			}
		}

		FString GetShaderPlatformLabel(const EShaderPlatform ShaderPlatform)
		{
			const FName ShaderFormat = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
			return ShaderFormat.IsNone()
				? FString::Printf(TEXT("Platform %d"), static_cast<int32>(ShaderPlatform))
				: ShaderFormat.ToString();
		}

		FString GetMaterialQualityLevelLabel(const EMaterialQualityLevel::Type QualityLevel)
		{
			return ::LexToString(QualityLevel);
		}

		FString GetFirstMeaningfulErrorLine(const FString& InError)
		{
			TArray<FString> Lines;
			InError.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					return Trimmed;
				}
			}
			return InError.TrimStartAndEnd();
		}

		/**
		 * Bumped only when a change would make an older client misread a newer editor.
		 *
		 * Deliberately mirrors DreamFX's numbering rather than starting its own count: the
		 * two bridges now speak the same shape, and a shared client that had to remember two
		 * unrelated version lines for one contract would be a worse contract.
		 */
		constexpr int32 BridgeProtocolVersion = 1;

		/** How often the heartbeat is rewritten while idle. Matches DreamFX. */
		constexpr double HeartbeatSeconds = 2.0;
		// Several heartbeats, so an owner that is merely busy compiling is not declared dead. A
		// compile blocks the game thread, and blocking for a few seconds is ordinary.
		constexpr double OwnerLockStaleSeconds = 30.0;

		/**
		 * Writes a file the way a reader that is polling for it needs it written.
		 *
		 * The client watches for `Responses/<id>.json` to exist and then reads it. A plain
		 * write makes the file exist while it is still half a file, so the client sees
		 * truncated JSON -- rarely, and only under load, which is the worst way for a bug to
		 * behave. Writing beside the target and renaming makes appearing and being complete
		 * the same event.
		 */
		bool WriteFileAtomically(const FString& Path, const FString& Text)
		{
			const FString Temporary = Path + TEXT(".tmp");
			if (!FFileHelper::SaveStringToFile(Text, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				return false;
			}
			// Move rather than Copy: a rename within one volume is atomic, a copy is not.
			if (!IFileManager::Get().Move(*Path, *Temporary, /*bReplace=*/true))
			{
				IFileManager::Get().Delete(*Temporary);
				return false;
			}
			return true;
		}

		void WriteDiagnosticsArray(
			const TSharedRef<TJsonWriter<>>& Writer,
			const TArray<FDreamShaderDiagnosticRecord>* Diagnostics,
			const FString& FallbackFilePath)
		{
			Writer->WriteArrayStart(TEXT("diagnostics"));
			if (Diagnostics != nullptr)
			{
				for (const FDreamShaderDiagnosticRecord& Record : *Diagnostics)
				{
					Writer->WriteObjectStart();
					// Each diagnostic keeps its own file: a .dsm that pulls in a broken .dsh
					// fails at the .dsh's position, and attributing it to the .dsm would send
					// the author to the wrong file.
					//
					// `FilePath` is often empty, because the store keys records by source and
					// only fills this in when a diagnostic belongs to a *different* file than
					// the one being compiled. Writing that empty string through would hand the
					// client a location it cannot open, so the compiled file stands in.
					Writer->WriteValue(TEXT("file"),
						Record.FilePath.IsEmpty() ? FallbackFilePath : Record.FilePath);
					Writer->WriteValue(TEXT("line"), FMath::Max(1, Record.Line));
					Writer->WriteValue(TEXT("column"), FMath::Max(1, Record.Column));
					Writer->WriteValue(TEXT("severity"), Record.Severity);
					if (!Record.Code.IsEmpty())
					{
						Writer->WriteValue(TEXT("code"), Record.Code);
					}
					if (!Record.Stage.IsEmpty())
					{
						Writer->WriteValue(TEXT("stage"), Record.Stage);
					}
					if (!Record.AssetPath.IsEmpty())
					{
						Writer->WriteValue(TEXT("assetPath"), Record.AssetPath);
					}
					// The wire, not a UI: an invariant string, never the localised display
					// text, or a client on a non-English editor gets messages it cannot match.
					Writer->WriteValue(TEXT("message"), ToInvariantWireString(Record.Message));
					Writer->WriteObjectEnd();
				}
			}
			Writer->WriteArrayEnd();
		}

	}


	FString FDreamShaderEditorBridge::GetBridgeDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"));
	}

	FString FDreamShaderEditorBridge::GetRequestDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("Requests"));
	}

	FString FDreamShaderEditorBridge::GetResponseDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("Responses"));
	}

	FString FDreamShaderEditorBridge::GetStatusFilePath()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("status.json"));
	}

	FString FDreamShaderEditorBridge::GetDiagnosticsFilePath()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("diagnostics.json"));
	}

	FString FDreamShaderEditorBridge::GetDiagnosticsDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("diagnostics"));
	}

	FString FDreamShaderEditorBridge::GetSourceFileMetadata(UObject* Asset)
	{
		if (!Asset)
		{
			return FString();
		}

		if (UPackage* Package = Asset->GetOutermost())
		{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			return Package->GetMetaData().GetValue(Asset, TEXT("DreamShader.SourceFile"));
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				return MetaData->GetValue(Asset, TEXT("DreamShader.SourceFile"));
			}
#endif
		}

		return FString();
	}


	FString FDreamShaderEditorBridge::GetOwnerLockFilePath()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("owner.lock"));
	}

	/**
	 * Decide whether this process is the one that serves the bridge for this project.
	 *
	 * The bridge directory is per-PROJECT, not per-process: one Requests folder, one status.json, one
	 * heartbeat. Two editors open on the same project therefore both scanned the same request queue and
	 * both answered it -- whichever polled first consumed the file, the other read a half-deleted one,
	 * and both overwrote status.json with their own pid, so a client could not even tell which editor it
	 * was talking to. Ownership is a lock file naming the owning pid, refreshed on the heartbeat.
	 *
	 * Taking over is deliberately conservative: an owner is believed while its process is alive AND its
	 * heartbeat is recent. The pid test alone would hand the bridge to a second editor whenever the
	 * first was mid-compile (a compile blocks the game thread, so the heartbeat stops); the heartbeat
	 * test alone would leave the bridge unowned for a stale window after a hard crash.
	 */
	bool FDreamShaderEditorBridge::TryAcquireBridgeOwnership()
	{
		const uint32 SelfPid = FPlatformProcess::GetCurrentProcessId();

		FString LockText;
		if (FFileHelper::LoadFileToString(LockText, *GetOwnerLockFilePath()))
		{
			TSharedPtr<FJsonObject> LockObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LockText);
			if (FJsonSerializer::Deserialize(Reader, LockObject) && LockObject.IsValid())
			{
				int32 OwnerPid = 0;
				FString HeartbeatText;
				LockObject->TryGetNumberField(TEXT("pid"), OwnerPid);
				LockObject->TryGetStringField(TEXT("heartbeat"), HeartbeatText);

				FDateTime OwnerHeartbeat;
				const bool bHeartbeatParsed = FDateTime::ParseIso8601(*HeartbeatText, OwnerHeartbeat);
				const bool bHeartbeatFresh = bHeartbeatParsed
					&& (FDateTime::UtcNow() - OwnerHeartbeat).GetTotalSeconds() < OwnerLockStaleSeconds;

				if (OwnerPid > 0
					&& static_cast<uint32>(OwnerPid) != SelfPid
					&& bHeartbeatFresh
					&& FPlatformProcess::IsApplicationRunning(static_cast<uint32>(OwnerPid)))
				{
					if (bIsBridgeOwner)
					{
						UE_LOG(LogDreamShader, Warning,
							TEXT("DreamShader bridge ownership was taken over by process %d; this editor stops serving requests."),
							OwnerPid);
					}
					bIsBridgeOwner = false;
					SetMayWriteGeneratedAssetsToDisk(false);
					return false;
				}
			}
		}

		const bool bWasOwner = bIsBridgeOwner;
		bIsBridgeOwner = true;
		SetMayWriteGeneratedAssetsToDisk(true);
		RefreshBridgeOwnershipLock();

		if (!bWasOwner)
		{
			UE_LOG(LogDreamShader, Display, TEXT("DreamShader bridge owned by this editor (pid %u)."), SelfPid);
		}
		return true;
	}

	void FDreamShaderEditorBridge::RefreshBridgeOwnershipLock()
	{
		if (!bIsBridgeOwner)
		{
			return;
		}

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("pid"), static_cast<int32>(FPlatformProcess::GetCurrentProcessId()));
		Writer->WriteValue(TEXT("heartbeat"), FDateTime::UtcNow().ToIso8601());
		Writer->WriteObjectEnd();
		Writer->Close();

		WriteFileAtomically(GetOwnerLockFilePath(), Text);
	}

	void FDreamShaderEditorBridge::ReleaseBridgeOwnership()
	{
		if (!bIsBridgeOwner)
		{
			return;
		}

		bIsBridgeOwner = false;
		IFileManager::Get().Delete(*GetOwnerLockFilePath());
	}

	void FDreamShaderEditorBridge::PublishStatus()
	{
		// Non-owners stay silent rather than fighting over the file. A client reading a status.json
		// that alternates between two pids cannot tell which editor is about to answer it.
		if (!bIsBridgeOwner)
		{
			return;
		}

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("protocol"), BridgeProtocolVersion);
		Writer->WriteValue(TEXT("pid"), static_cast<int32>(FPlatformProcess::GetCurrentProcessId()));
		Writer->WriteValue(TEXT("project"), FApp::GetProjectName());
		Writer->WriteValue(TEXT("projectDir"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		Writer->WriteValue(TEXT("engineDir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
		Writer->WriteValue(TEXT("pluginVersion"),
			Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("unknown"));

		// A compile blocks the game thread, so the heartbeat stops while one runs. Saying
		// *what* is running lets a client tell "busy for 40 seconds" from "died 40 seconds
		// ago" -- without it the only safe reading of a stale heartbeat is "dead", and every
		// real compile would look like a crash.
		Writer->WriteValue(TEXT("busy"), bBusy);
		if (bBusy)
		{
			Writer->WriteValue(TEXT("busyAction"), BusyAction);
		}
		if (!LastResult.IsEmpty())
		{
			Writer->WriteValue(TEXT("lastResult"), LastResult);
		}
		Writer->WriteValue(TEXT("heartbeatUtc"), FDateTime::UtcNow().ToIso8601());
		Writer->WriteObjectEnd();
		Writer->Close();

		WriteFileAtomically(GetStatusFilePath(), Text);
		LastHeartbeatSeconds = FPlatformTime::Seconds();
	}

	void FDreamShaderEditorBridge::RespondTo(
		const FString& RequestId,
		bool bOk,
		const FString& Message,
		const TArray<FDreamShaderDiagnosticRecord>* Diagnostics,
		double DurationMs,
		const FString& FallbackFilePath)
	{
		// No id means a client that is not listening for an answer -- every request the
		// shipped VSCode extension sends is of that shape. Writing a response anyway would
		// litter the directory with files nobody ever deletes.
		if (RequestId.IsEmpty())
		{
			return;
		}

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("protocol"), BridgeProtocolVersion);
		Writer->WriteValue(TEXT("requestId"), RequestId);
		Writer->WriteValue(TEXT("ok"), bOk);
		Writer->WriteValue(TEXT("durationMs"), static_cast<int32>(DurationMs));
		Writer->WriteValue(TEXT("message"), Message);
		WriteDiagnosticsArray(Writer, Diagnostics, FallbackFilePath);
		Writer->WriteObjectEnd();
		Writer->Close();

		WriteFileAtomically(FPaths::Combine(GetResponseDirectory(), RequestId + TEXT(".json")), Text);
	}

	bool FDreamShaderEditorBridge::IsAbandoned(const FString& RequestPath) const
	{
		// A request waits on disk when the editor is closed -- that is the point of using
		// files. What must not happen is executing it later: by then the client has long
		// since timed out, fallen back to the CLI and moved on, so the work is nobody's, and
		// a `recompile` served at startup is a compile pass no one asked for.
		//
		// The cutoff is this bridge's own start time rather than an age limit, because age is
		// the wrong question: a request queued behind a five-minute compile is old and still
		// wanted. "Was anyone listening when it was written" is the actual condition.
		const FDateTime Written = IFileManager::Get().GetTimeStamp(*RequestPath);
		return Written != FDateTime::MinValue() && Written < ListeningSince;
	}

	void FDreamShaderEditorBridge::ResolvePendingResponses(
		const FString& SourceFilePath,
		bool bOk,
		const FString& Message)
	{
		const FString Key = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		TArray<FPendingResponse> Waiting;
		if (!PendingResponsesBySource.RemoveAndCopyValue(Key, Waiting))
		{
			return;
		}

		const double Now = FPlatformTime::Seconds();
		const TArray<FDreamShaderDiagnosticRecord>* Diagnostics = DiagnosticsStore.FindDiagnostics(Key);
		for (const FPendingResponse& Pending : Waiting)
		{
			// Measured from acceptance, not from the start of the compile: the debounce wait
			// is time the caller spent waiting, and reporting only the compile would say
			// "0 ms" for a request that took half a second to come back.
			RespondTo(Pending.RequestId, bOk, Message, Diagnostics,
				(Now - Pending.AcceptedAtSeconds) * 1000.0, Key);
		}

		LastResult = FString::Printf(TEXT("%s (%s)"), bOk ? TEXT("ok") : TEXT("failed"), *Message);
		PublishStatus();
	}

	void FDreamShaderEditorBridge::Startup()
	{
		bIsShuttingDown = false;

		IFileManager::Get().MakeDirectory(*GetBridgeDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetRequestDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetResponseDirectory(), true);

		// Before anything touches the request queue or the status file.
		TryAcquireBridgeOwnership();

		// Responses left by a previous session are answers nobody is waiting for any more,
		// and a client that reconnects and finds one would act on a stale result.
		{
			IFileManager& Files = IFileManager::Get();
			TArray<FString> Stale;
			Files.FindFiles(Stale, *FPaths::Combine(GetResponseDirectory(), TEXT("*.json")), true, false);
			for (const FString& Name : Stale)
			{
				Files.Delete(*FPaths::Combine(GetResponseDirectory(), Name));
			}
		}

		bBusy = false;
		BusyAction.Reset();
		LastResult.Reset();
		PendingResponsesBySource.Reset();
		// Stamped before the first poll, so anything already in the queue is recognised as
		// having been written to a room with nobody in it.
		ListeningSince = FDateTime::UtcNow();
		PublishStatus();

		IFileManager::Get().MakeDirectory(*FDreamShaderPreviewRenderer::GetPreviewDirectory(), true);
		FDreamShaderWorkspaceService::ResetBridgeDatabase();

		FDreamShaderWorkspaceService::ExportMaterialExpressionManifest();
		FDreamShaderWorkspaceService::ExportDreamShaderSettingsManifest();
		FDreamShaderWorkspaceService::ExportSubstrateBuiltinsManifest();
		FDreamShaderWorkspaceService::ExportPreprocessorDefinesManifest();
		SyncVirtualFunctionDefinitions();

		// Registered unconditionally and gated inside on the LIVE setting: caching the flag here
		// made a mid-session Project Settings toggle silently ineffective until the next restart.
		PostEngineInitHandle = DREAMSHADER_POST_ENGINE_INIT_DELEGATE().AddSP(
			AsShared(),
			&FDreamShaderEditorBridge::HandlePostEngineInit);
		SettingsChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(
			AsShared(),
			&FDreamShaderEditorBridge::HandleSettingsPropertyChanged);

		QueueFullScan();
		UpdateDiagnosticsFile();

		PreviewWebSocketServer = MakeUnique<FDreamShaderPreviewWebSocketServer>();
		PreviewWebSocketServer->Startup(17864);

		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			// One watch per source root. Without this a plugin's sources would compile on a full scan
			// but never on save, which reads as auto-compile being broken for that plugin.
			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				if (Root.Directory.IsEmpty() || DirectoryWatcherHandles.Contains(Root.Directory))
				{
					continue;
				}

				FDelegateHandle RootWatcherHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
					Root.Directory,
					IDirectoryWatcher::FDirectoryChanged::CreateSP(AsShared(), &FDreamShaderEditorBridge::OnDirectoryChanged),
					RootWatcherHandle,
					IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
				DirectoryWatcherHandles.Add(Root.Directory, RootWatcherHandle);
			}
		}

		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddSP(
			AsShared(),
			&FDreamShaderEditorBridge::OnMaterialCompilationFinished);

		ToolMenusStartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::RegisterMenus));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::Tick),
			0.1f);

		// 0.0f means "call every tick" (as opposed to waiting InDelay seconds between calls) --
		// needed here because the preview WebSocket server's own per-client frame throttle
		// (dreamshader.previewLiveFrameRate / the panel's FPS control, up to 60) can't deliver
		// faster than whatever rate this ticker actually runs at. Sharing the 0.1s ticker above
		// would silently cap every preview session at 10 FPS regardless of that setting.
		PreviewTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::TickPreview),
			0.0f);
	}

	void FDreamShaderEditorBridge::Shutdown()
	{
		bIsShuttingDown = true;
		FDreamShaderWorkspaceService::ResetBridgeDatabase();

		// The heartbeat is how a client tells a running editor from a closed one, and a file
		// that simply stops being updated is indistinguishable from one whose editor hung.
		// Deleting it says "gone" in a way a timeout cannot: the client falls back to the CLI
		// immediately instead of waiting out its liveness window first.
		IFileManager::Get().Delete(*GetStatusFilePath());

		// Released rather than left to go stale, so a second editor on this project picks the bridge
		// up on its next heartbeat instead of after the whole staleness window.
		ReleaseBridgeOwnership();

		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}

		if (PreviewTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PreviewTickerHandle);
			PreviewTickerHandle.Reset();
		}

		if (PreviewWebSocketServer)
		{
			PreviewWebSocketServer->Shutdown();
			PreviewWebSocketServer.Reset();
		}

		if (PostEngineInitHandle.IsValid())
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
		}

		if (SettingsChangedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(SettingsChangedHandle);
			SettingsChangedHandle.Reset();
		}

		if (MaterialCompilationFinishedHandle.IsValid())
		{
			UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
			MaterialCompilationFinishedHandle.Reset();
		}

		if (ToolMenusStartupCallbackHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(ToolMenusStartupCallbackHandle);
			ToolMenusStartupCallbackHandle.Reset();
		}

		if (!IsEngineExitRequested() && !GExitPurge)
		{
			UToolMenus::UnregisterOwner(DreamShaderToolMenuOwnerName);
		}

		if (!DirectoryWatcherHandles.IsEmpty())
		{
			if (FDirectoryWatcherModule* DirectoryWatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
			{
				if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule->Get())
				{
					for (const TPair<FString, FDelegateHandle>& Watch : DirectoryWatcherHandles)
					{
						DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Watch.Key, Watch.Value);
					}
				}
			}

			DirectoryWatcherHandles.Reset();
		}

		PendingFiles.Reset();
		ForcedPendingFiles.Reset();
		DiagnosticsStore.Reset();
	}

	void FDreamShaderEditorBridge::QueueFullScan(const bool bForce)
	{
		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(SourceFiles);
		RebuildDependencyGraph();

		const double Now = FPlatformTime::Seconds();
		for (FString& SourceFile : SourceFiles)
		{
			const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			PendingFiles.Add(Normalized, Now);
			if (bForce)
			{
				ForcedPendingFiles.Add(Normalized);
			}
		}
	}

	void FDreamShaderEditorBridge::HandlePostEngineInit()
	{
		// In-memory generation is the editor's always-on behavior (source files are the authoring
		// surface; the editor never writes per-material .uasset files).
		//
		// This call is also what arms Tick's define poll: the sweep stamps the revision it ran against,
		// so every define contributed while startup modules were loading is already accounted for by
		// the materials it just produced, and the first tick does not order a rebuild for them.
		GenerateAllInMemoryMaterials();
	}

	void FDreamShaderEditorBridge::HandleSettingsPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
	{
		if (!Object || !Object->IsA<UDreamShaderSettings>())
		{
			return;
		}

		// Every setting the build key folds in (BuildSourceHash), because each one changes what a given
		// source compiles into and the assets already generated are stale the moment it moves. The
		// backend is the loudest of them -- it decides whether a Shader block is a UMaterial or a thin
		// instance -- but a mapping table that retargets a Settings key is no less of a change.
		//
		// PreprocessorDefines belongs in that list for a sharper reason than the others: it is the only
		// entry here that changes the source TEXT the parser sees, by cutting a different branch. It is
		// also the only one whose effect is invisible in the generated asset, which holds nothing but
		// the post-cut result -- so an editor that ignored this edit would keep showing materials built
		// from branches the project no longer selects, with no way to tell from the asset that anything
		// was wrong. The claim in the paragraph above ("every setting the build key folds in") stops
		// being true the moment this name is missing, and the omission reads as a deliberate exclusion
		// rather than an oversight.
		const FName ChangedProperty = Event.GetPropertyName();
		if (ChangedProperty != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, DefaultBackend)
			&& ChangedProperty != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, ShadingModelMappings)
			&& ChangedProperty != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, BlendModeMappings)
			&& ChangedProperty != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, MaterialDomainMappings)
			&& ChangedProperty != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, PreprocessorDefines))
		{
			return;
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader setting '%s' changed; regenerating all source files."), *ChangedProperty.ToString());
		// No force any more, and that is the point: this used to be the one caller that forced, because
		// the key hashed the source text and could not see the setting the user had just changed. Now it
		// can, so every affected asset fails the skip check on its own -- and, just as importantly, one
		// that the setting does NOT affect is still skipped instead of being needlessly rebuilt.
		GenerateAllInMemoryMaterials();

		// Stale persisted assets shadow the in-memory versions; point the user at the cleanup.
		TArray<UObject*> ShadowingAssets;
		if (CollectPersistedGeneratedAssets(ShadowingAssets) > 0)
		{
			ShowDreamShaderNotification(
				FText::Format(
					LOCTEXT("DreamShaderInMemoryModeShadowed", "{0} previously generated asset(s) are still saved on disk and shadow the in-memory materials. Run Tools > DreamShader > Clean Persisted Generated Assets to remove them."),
					FText::AsNumber(ShadowingAssets.Num())),
				SNotificationItem::CS_Fail);
		}
	}

	void FDreamShaderEditorBridge::GenerateAllInMemoryMaterials()
	{
		// Snapshot BEFORE the pass and recorded after it, because what this sweep can honestly claim is
		// "the materials in memory were built against the table as it stood when I started". A define
		// registered while the pass is running did NOT reach the files already visited, so recording
		// the post-pass value would swallow that bump along with the rebuild it was owed. Recording the
		// pre-pass value at worst leaves Tick one redundant sweep, which every build key then skips.
		//
		// Kept here, in the one function every whole-project regeneration goes through, rather than at
		// each caller: HandleSettingsPropertyChanged already sweeps on a PreprocessorDefines edit, and
		// without a single choke point Tick's poll would sweep the whole project a second time for the
		// same edit, one tick later, finding nothing to do.
		const uint32 SweptDefineRevision = UE::DreamShader::GetDreamShaderDefineRevision();
		ON_SCOPE_EXIT
		{
			LastGeneratedDefineRevision = SweptDefineRevision;
			bDefineRevisionBaselineTaken = true;
		};

		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);

		if (SourceFiles.IsEmpty())
		{
			// Still counts as swept, by way of the scope guard above: with no sources there is nothing
			// that could be stale, and returning without a baseline would leave Tick sweeping an empty
			// project on every single define change.
			return;
		}

		// Same reason as the drain in ProcessReadyFiles: a whole-project sweep is the batch most likely
		// to contain both a function and its callers.
		FDreamShaderDependencyGraphService::SortByDependencyOrder(SourceFiles);

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader in-memory material mode: generating %d source file(s) in memory..."), SourceFiles.Num());

		int32 SuccessCount = 0;
		int32 FailCount = 0;
		for (const FString& SourceFile : SourceFiles)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			if (UE::DreamShader::IsDreamShaderHeaderFile(NormalizedPath))
			{
				continue;
			}

			FString Message;
			// Never forced. Forcing was free while an in-memory asset regenerated regardless of its
			// build key; it stopped being free once an asset that exists on disk started being rebuilt
			// AND SAVED as one, because this sweep runs on every editor launch -- it would rewrite every
			// persisted generated asset each time, for rebuilds the key had already ruled out. The key
			// is what decides, and it now covers the settings a caller might once have forced past.
			const bool bSuccess = FMaterialGenerator::GenerateAssetsFromFile(NormalizedPath, Message, /*bForce*/ false, /*bTransient*/ true);
			if (bSuccess)
			{
				++SuccessCount;
				UE_LOG(LogDreamShader, Display, TEXT("  [In-Memory] %s"), *Message);
			}
			else
			{
				++FailCount;
				UE_LOG(LogDreamShader, Warning, TEXT("  [In-Memory] Failed: %s"), *Message);
			}
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader in-memory material generation complete: %d succeeded, %d failed."), SuccessCount, FailCount);

		// A ThinCustom/Instance-backend material that is hiding itself reports "Generated ... (virtual)"
		// like any other and then cannot be found anywhere — no Content Browser tile, no registry hit,
		// no folder (AssetCreated no-ops on !IsAsset(), so it does not register its path either). That
		// reads as a generation failure. Name the count and the way out once per pass so a successful
		// build is never mistaken for a lost asset. Graph-backend materials are always visible and are
		// deliberately not counted here.
		if (!GetDefault<UDreamShaderSettings>()->bShowInMemoryMaterialsInContentBrowser)
		{
			int32 HiddenCount = 0;
			for (TObjectIterator<UDreamShaderMaterialInstance> It; It; ++It)
			{
				const UDreamShaderMaterialInstance* Instance = *It;
				if (IsValid(Instance) && Instance->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
				{
					++HiddenCount;
				}
			}

			if (HiddenCount > 0)
			{
				UE_LOG(LogDreamShader, Display,
					TEXT("  %d of them are memory-only ThinCustom materials hidden from the Content Browser and the asset registry (their folders will not appear either). ")
					TEXT("Enable Tools > DreamShader > Show In-Memory Materials to browse them, or set Backend = \"Graph\" on a source to make it always visible."),
					HiddenCount);
			}
		}
	}

	void FDreamShaderEditorBridge::QueueSourceFile(const FString& SourceFilePath, const bool bForce)
	{
		const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		PendingFiles.Add(Normalized, FPlatformTime::Seconds());
		if (bForce)
		{
			ForcedPendingFiles.Add(Normalized);
		}
	}

	void FDreamShaderEditorBridge::QueueDependentSourcesForImport(const FString& ImportFilePath)
	{
		const FString NormalizedImportPath = UE::DreamShader::NormalizeSourceFilePath(ImportFilePath);
		const TSet<FString> SourcesToQueue =
			FDreamShaderDependencyGraphService::RebuildAndCollectDependentsForImport(ImportFilePath, HeaderDependentsByFile);

		const double Now = FPlatformTime::Seconds();
		for (const FString& SourceFile : SourcesToQueue)
		{
			PendingFiles.Add(SourceFile, Now);
		}

		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		if (Settings && Settings->bVerboseLogs)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader queued %d dependent source file(s) for import '%s'."),
				SourcesToQueue.Num(),
				*NormalizedImportPath);
		}
	}

	void FDreamShaderEditorBridge::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
	{
		TArray<FFileChangeData> ChangesCopy = FileChanges;
		TWeakPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> WeakBridge = AsWeak();
		AsyncTask(ENamedThreads::GameThread, [WeakBridge, Changes = MoveTemp(ChangesCopy)]()
		{
			TSharedPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> Bridge = WeakBridge.Pin();
			if (!Bridge.IsValid() || Bridge->bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
			{
				return;
			}

			// Tell the listeners about the tree before the auto-compile gate below: the set of files
			// changed whether or not anything is going to be compiled for it.
			bool bTreeChanged = false;
			for (const FFileChangeData& FileChange : Changes)
			{
				if (FileChange.Action == FFileChangeData::FCA_RescanRequired)
				{
					bTreeChanged = true;
				}
				else if (UE::DreamShader::IsDreamShaderSourceFile(FileChange.Filename))
				{
					if (FileChange.Action == FFileChangeData::FCA_Modified)
					{
						Bridge->SourceFileModifiedEvent.Broadcast(UE::DreamShader::NormalizeSourceFilePath(FileChange.Filename));
					}
					else
					{
						bTreeChanged = true;
					}
				}
			}
			if (bTreeChanged)
			{
				Bridge->SourceTreeChangedEvent.Broadcast();
			}

			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			if (Settings && !Settings->bAutoCompileOnSave)
			{
				return;
			}

			for (const FFileChangeData& FileChange : Changes)
			{
				if (FileChange.Action == FFileChangeData::FCA_RescanRequired)
				{
					Bridge->QueueFullScan();
					continue;
				}

				if (!UE::DreamShader::IsDreamShaderSourceFile(FileChange.Filename))
				{
					continue;
				}

				if (FileChange.Action == FFileChangeData::FCA_Added || FileChange.Action == FFileChangeData::FCA_Modified)
				{
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						Bridge->QueueDependentSourcesForImport(FileChange.Filename);
					}
					else if (UE::DreamShader::IsDreamShaderFunctionFile(FileChange.Filename))
					{
						if (!FDreamShaderSourceFileUtils::IsPackageMaterialFile(FileChange.Filename))
						{
							Bridge->QueueSourceFile(FileChange.Filename);
						}
						Bridge->QueueDependentSourcesForImport(FileChange.Filename);
					}
					else if (FDreamShaderSourceFileUtils::IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->RebuildDependencyGraph();
						Bridge->QueueSourceFile(FileChange.Filename);
					}
				}
				else if (FileChange.Action == FFileChangeData::FCA_Removed)
				{
					const FString SourceFile = UE::DreamShader::NormalizeSourceFilePath(FileChange.Filename);
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename) || UE::DreamShader::IsDreamShaderFunctionFile(FileChange.Filename))
					{
						Bridge->QueueDependentSourcesForImport(FileChange.Filename);
						if (UE::DreamShader::IsDreamShaderFunctionFile(FileChange.Filename))
						{
							Bridge->PendingFiles.Remove(SourceFile);
							Bridge->ForcedPendingFiles.Remove(SourceFile);
							Bridge->ClearDiagnosticsForSourceAndDependencies(SourceFile);
							Bridge->UpdateDiagnosticsFile();
						}
					}
					else if (FDreamShaderSourceFileUtils::IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->PendingFiles.Remove(SourceFile);
						Bridge->ForcedPendingFiles.Remove(SourceFile);
						Bridge->ClearDiagnosticsForSourceAndDependencies(SourceFile);
						Bridge->RebuildDependencyGraph();
						Bridge->UpdateDiagnosticsFile();
					}
					UE_LOG(LogDreamShader, Display, TEXT("DreamShader source removed, existing generated assets were left untouched: %s"), *FileChange.Filename);
				}
			}
		});
	}

	bool FDreamShaderEditorBridge::Tick(float DeltaSeconds)
	{
		(void)DeltaSeconds;

		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return false;
		}

		// A define change invalidates every in-memory material generated before it. What sits in memory
		// is the branch the OLD define set selected; a `dsc` run -- a fresh process, reading the new
		// set -- writes a different one. Left out of step, the editor shows one material and the disk
		// holds another, with nothing about either looking wrong, which is precisely the shape of the
		// in-memory-versus-disk accidents this plugin has been bitten by before.
		//
		// Polled rather than hooked, because only ONE of the four define tiers announces itself.
		// UDreamShaderSettings::PostEditChangeProperty does (and takes care to bump the revision before
		// calling Super, so the bridge's own settings handler already sees the new value) -- but
		// RegisterDreamShaderDefine, UnregisterDreamShaderDefinesFrom and the provider delegates have
		// no notification at all, and a plugin that contributes a switch after startup is exactly the
		// case this feature was asked for. A uint32 compare at 10Hz covers all four and depends on none
		// of their internal ordering.
		//
		// It does not double up with the settings path: GenerateAllInMemoryMaterials stamps the
		// revision it swept against, so a sweep that handler already ran leaves nothing here to do.
		//
		// Ahead of the queue drains below, so a file sitting in the debounce queue is not compiled
		// against the old set moments before the sweep rebuilds it anyway.
		const uint32 CurrentDefineRevision = UE::DreamShader::GetDreamShaderDefineRevision();
		if (bDefineRevisionBaselineTaken && CurrentDefineRevision != LastGeneratedDefineRevision)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader preprocessor defines changed (revision %u -> %u); regenerating in-memory materials."),
				LastGeneratedDefineRevision,
				CurrentDefineRevision);

			// Republished on the same edge, not on a poll of its own. The manifest's `defines` array
			// IS the resolved table, so it is stale for exactly as long as the in-memory materials
			// are -- and an extension greying out branches from a table the editor has already moved
			// on from is the same class of accident as an in-memory material disagreeing with disk,
			// only quieter, because nothing about the greyed-out lines looks wrong.
			//
			// Ahead of the sweep rather than after it: the sweep may take a while, and the manifest
			// is a snapshot of the define table, which is already current.
			FDreamShaderWorkspaceService::ExportPreprocessorDefinesManifest();

			// Unforced, for the same reason HandleSettingsPropertyChanged stopped forcing: the build
			// key folds in the defines each source actually READ (BuildSourceHash takes the touched
			// set), so an affected asset fails the skip check by itself and one that reads no defines
			// is correctly left alone. Forcing here would rebuild every asset in the project each time
			// any define moved, most of them for nothing.
			//
			// That makes this the second half of a pair, and the pair is only useful whole: drop the
			// touched-define fold out of the build key and this sweep still runs, still logs, and skips
			// every file -- an invalidation that looks like it works and does nothing.
			GenerateAllInMemoryMaterials();
		}

		ProcessRequestFiles();
		ProcessReadyFiles();

		// Only when nothing happened. A request or a compile republishes the status itself,
		// with the busy flag set, and re-stamping it here would just cost an extra write.
		if (FPlatformTime::Seconds() - LastHeartbeatSeconds > HeartbeatSeconds)
		{
			// Re-evaluated every heartbeat rather than only at startup, so the bridge moves to whichever
			// editor is still open when the owner exits -- including an owner that exited without
			// releasing the lock.
			TryAcquireBridgeOwnership();
			PublishStatus();
		}
		return true;
	}

	bool FDreamShaderEditorBridge::TickPreview(float DeltaSeconds)
	{
		(void)DeltaSeconds;

		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return false;
		}

		if (PreviewWebSocketServer)
		{
			PreviewWebSocketServer->Tick();
		}
		return true;
	}

	void FDreamShaderEditorBridge::ProcessRequestFiles()
	{
		// One consumer per project. Two editors polling the same folder means one deletes the file
		// the other is mid-read of, and both write a response for it.
		if (!bIsBridgeOwner)
		{
			return;
		}

		TArray<FString> RequestFiles;
		IFileManager::Get().FindFiles(RequestFiles, *FPaths::Combine(GetRequestDirectory(), TEXT("*.json")), true, false);
		if (RequestFiles.Num() == 0)
		{
			return;
		}

		// Oldest first, so a client that fired two requests gets them in the order it sent
		// them. Names carry a timestamp precisely so this is a sort and not a guess.
		RequestFiles.Sort();

		for (const FString& RequestFileName : RequestFiles)
		{
			const FString RequestPath = FPaths::Combine(GetRequestDirectory(), RequestFileName);

			if (IsAbandoned(RequestPath))
			{
				IFileManager::Get().Delete(*RequestPath);
				UE_LOG(LogDreamShader, Display,
					TEXT("DreamShader bridge discarded '%s' -- written before this editor started listening, so whoever sent it has already given up."),
					*RequestFileName);
				continue;
			}

			FString RequestText;
			if (!FFileHelper::LoadFileToString(RequestText, *RequestPath))
			{
				// Most likely still being written -- the shipped VSCode extension writes
				// these in place rather than renaming them in, so a half-written file is a
				// real state here. Left alone; the next poll picks it up. Deleting it, which
				// is what this used to do, threw the request away instead.
				continue;
			}

			TSharedPtr<FJsonObject> RequestObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestText);
			const bool bParsed = FJsonSerializer::Deserialize(Reader, RequestObject) && RequestObject.IsValid();

			FString RequestId;
			if (bParsed)
			{
				RequestObject->TryGetStringField(TEXT("requestId"), RequestId);
			}

			// Deleted before it is served, never after. A request that crashes the editor
			// would otherwise be replayed on every start.
			IFileManager::Get().Delete(*RequestPath);

			const double StartedAt = FPlatformTime::Seconds();

			if (!bParsed)
			{
				RespondTo(RequestId, false, TEXT("The request is not valid JSON."));
				continue;
			}

			// A missing `protocol` is a client that predates versioning, not a mismatch.
			// Every request the shipped VSCode extension sends is of that shape, and
			// rejecting them would break it on every machine that has it installed. Only a
			// field that is present *and* different is a real disagreement.
			int32 Protocol = BridgeProtocolVersion;
			RequestObject->TryGetNumberField(TEXT("protocol"), Protocol);
			if (Protocol != BridgeProtocolVersion)
			{
				RespondTo(RequestId, false, FString::Printf(
					TEXT("Protocol %d is not understood; this editor speaks %d. Update the DreamShaderLang extension or the plugin so the two match."),
					Protocol, BridgeProtocolVersion));
				continue;
			}

			FString Action;
			FString Scope;
			RequestObject->TryGetStringField(TEXT("action"), Action);
			RequestObject->TryGetStringField(TEXT("scope"), Scope);

			bBusy = true;
			BusyAction = Action;
			PublishStatus();

			if (Action.Equals(TEXT("ping"), ESearchCase::IgnoreCase))
			{
				RespondTo(RequestId, true, TEXT("alive"), nullptr,
					(FPlatformTime::Seconds() - StartedAt) * 1000.0);
			}
			else if (Action.Equals(TEXT("recompile"), ESearchCase::IgnoreCase))
			{
				if (Scope.Equals(TEXT("all"), ESearchCase::IgnoreCase))
				{
					RequestRecompileAll();
					// Through the debounce queue, which drains across ticks, so the result is
					// not knowable yet. Saying so beats blocking the caller for minutes or
					// inventing an answer.
					RespondTo(RequestId, true,
						TEXT("Queued a full rescan; the editor reports the result when the batch drains."),
						nullptr, (FPlatformTime::Seconds() - StartedAt) * 1000.0);
				}
				else if (Scope.Equals(TEXT("file"), ESearchCase::IgnoreCase))
				{
					FString SourceFilePath;
					if (RequestObject->TryGetStringField(TEXT("sourceFile"), SourceFilePath) && !SourceFilePath.IsEmpty())
					{
						// An explicit request means "rebuild": the hash skip is for saves the watcher sees.
						QueueSourceFile(SourceFilePath, /*bForce*/ true);
						// Parked, not answered: the compile happens a few ticks later, after
						// the debounce window. Answering now would report success before
						// anything had been attempted.
						if (!RequestId.IsEmpty())
						{
							PendingResponsesBySource
								.FindOrAdd(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath))
								.Add(FPendingResponse{ RequestId, StartedAt });
						}
					}
					else
					{
						RespondTo(RequestId, false,
							TEXT("recompile with scope 'file' needs a non-empty 'sourceFile'."));
					}
				}
				else
				{
					RespondTo(RequestId, false, FString::Printf(
						TEXT("recompile needs scope 'all' or 'file'; got '%s'."), *Scope));
				}
			}
			else if (Action.Equals(TEXT("cleanGeneratedShaders"), ESearchCase::IgnoreCase))
			{
				RequestCleanGeneratedShaders();
				RespondTo(RequestId, true, TEXT("Cleaned generated shaders and queued a rescan."),
					nullptr, (FPlatformTime::Seconds() - StartedAt) * 1000.0);
			}
			else if (Action.Equals(TEXT("previewMaterial"), ESearchCase::IgnoreCase))
			{
				FDreamShaderPreviewRequest PreviewRequest;
				RequestObject->TryGetStringField(TEXT("sourceFile"), PreviewRequest.SourceFilePath);
				RequestObject->TryGetStringField(TEXT("mesh"), PreviewRequest.Mesh);
				double Width = PreviewRequest.Width;
				double Height = PreviewRequest.Height;
				RequestObject->TryGetNumberField(TEXT("width"), Width);
				RequestObject->TryGetNumberField(TEXT("height"), Height);
				PreviewRequest.Width = FMath::Clamp(FMath::RoundToInt(Width), 64, 2048);
				PreviewRequest.Height = FMath::Clamp(FMath::RoundToInt(Height), 64, 2048);

				FDreamShaderPreviewResult PreviewResult;
				const bool bPreviewSucceeded = FDreamShaderPreviewRenderer::RenderMaterialPreview(PreviewRequest, PreviewResult);
				FDreamShaderPreviewRenderer::WritePreviewResult(PreviewResult, bPreviewSucceeded ? TEXT("ready") : TEXT("error"), RequestId);
				if (bPreviewSucceeded)
				{
					UE_LOG(LogDreamShader, Display, TEXT("DreamShader preview: %s"), *PreviewResult.Message.ToString());
				}
				else
				{
					UE_LOG(LogDreamShader, Error, TEXT("DreamShader preview: %s"), *PreviewResult.Message.ToString());
				}
				RespondTo(RequestId, bPreviewSucceeded, ToInvariantWireString(PreviewResult.Message),
					nullptr, (FPlatformTime::Seconds() - StartedAt) * 1000.0);
			}
			else
			{
				// Never silent. A client that asked for something this build does not have
				// needs to be told so, not left waiting for a response that is never coming.
				RespondTo(RequestId, false, FString::Printf(
					TEXT("Unknown action '%s'. This build understands: ping, recompile, cleanGeneratedShaders, previewMaterial."),
					*Action));
			}

			bBusy = false;
			BusyAction.Reset();
			PublishStatus();
		}
	}

	void FDreamShaderEditorBridge::ProcessReadyFiles()
	{
		const double Now = FPlatformTime::Seconds();
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		const double SaveDebounceSeconds = Settings ? FMath::Clamp(static_cast<double>(Settings->SaveDebounceSeconds), 0.05, 10.0) : 0.25;
		TArray<FString> ReadyFiles;
		for (const TPair<FString, double>& PendingFile : PendingFiles)
		{
			if (Now - PendingFile.Value >= SaveDebounceSeconds)
			{
				ReadyFiles.Add(PendingFile.Key);
			}
		}

		// Dependencies first. PendingFiles is a map, so without this the batch drained in whatever order
		// it happened to iterate -- and a caller compiled before the function it calls binds against the
		// previous version of that function's pins.
		FDreamShaderDependencyGraphService::SortByDependencyOrder(ReadyFiles);

		// The compile below is where the time actually goes, and it blocks the game thread --
		// which stops the heartbeat. A client reading a stopped heartbeat with `busy` unset
		// can only conclude the editor hung, so a big material would make a healthy editor
		// look dead and get itself refused. Marking the whole drain busy is what makes the
		// stopped heartbeat legible as work. Wrapping only the request dispatch, as an earlier
		// revision of this did, leaves exactly the long operations uncovered.
		if (!ReadyFiles.IsEmpty())
		{
			bBusy = true;
			BusyAction = ReadyFiles.Num() == 1
				? FString::Printf(TEXT("compile %s"), *FPaths::GetCleanFilename(ReadyFiles[0]))
				: FString::Printf(TEXT("compile %d file(s)"), ReadyFiles.Num());
			PublishStatus();
		}

		for (const FString& ReadyFile : ReadyFiles)
		{
			PendingFiles.Remove(ReadyFile);
			if (IFileManager::Get().FileExists(*ReadyFile))
			{
				ProcessSourceFile(ReadyFile);
			}
			else
			{
				// The source went away inside the debounce window -- renamed, or deleted. No
				// compile will run, so anyone waiting on one has to be told now; otherwise
				// the request sits until the client's timeout, waiting for an answer this
				// bridge has already decided never to produce.
				ResolvePendingResponses(ReadyFile, false,
					TEXT("The source file no longer exists; nothing was compiled."));
			}
		}

		if (!ReadyFiles.IsEmpty())
		{
			bBusy = false;
			BusyAction.Reset();
			PublishStatus();
		}
	}

	void FDreamShaderEditorBridge::ProcessSourceFile(const FString& SourceFilePath)
	{
		const bool bForce = ForcedPendingFiles.Remove(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath)) > 0;
		FString Message;
		CompileSourceFile(SourceFilePath, bForce, /*bInMemory*/ true, Message);
	}

	bool FDreamShaderEditorBridge::CompileSourceFile(const FString& InSourceFilePath, const bool bForce, const bool bInMemory, FString& OutMessage)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		UE::DreamShader::Compiler::FDreamShaderCompileService CompileService(UE::DreamShader::Editor::GetEditorCompileAdapter());
		const UE::DreamShader::Compiler::FDreamShaderCompileResult Result = CompileService.CompileAssets(SourceFilePath, bForce, bInMemory);
		OutMessage = ToInvariantWireString(Result.Message);
		if (Result.bSucceeded)
		{
			ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
			UpdateDiagnosticsFile();
			UE_LOG(LogDreamShader, Display, TEXT("%s"), *OutMessage);
			ResolvePendingResponses(SourceFilePath, true, OutMessage);
			return true;
		}

		TArray<FDreamShaderDiagnosticRecord> Diagnostics =
			FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics(SourceFilePath, Result.Message);
		ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
		UE_LOG(LogDreamShader, Error, TEXT("%s"), *OutMessage);
		// After SetDiagnostics, so the response carries this compile's findings rather than
		// whatever the store held before it ran.
		ResolvePendingResponses(SourceFilePath, false, OutMessage);
		return false;
	}

	void FDreamShaderEditorBridge::OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface)
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		UMaterial* Material = Cast<UMaterial>(MaterialInterface);
		if (!Material)
		{
			return;
		}

		const FString SourceFilePath = GetSourceFileMetadata(Material);
		if (SourceFilePath.IsEmpty())
		{
			return;
		}

		TArray<FDreamShaderDiagnosticRecord> Diagnostics;
		const FString MaterialAssetPath = Material->GetPathName();
		TSet<FString> SeenDiagnosticKeys;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
		for (int32 ShaderPlatformIndex = 0; ShaderPlatformIndex < EShaderPlatform::SP_NumPlatforms; ++ShaderPlatformIndex)
		{
			const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(ShaderPlatformIndex);
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < static_cast<int32>(EMaterialQualityLevel::Num); ++QualityLevelIndex)
			{
				const EMaterialQualityLevel::Type QualityLevel = static_cast<EMaterialQualityLevel::Type>(QualityLevelIndex);
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(ShaderPlatform, QualityLevel);
				if (!MaterialResource)
				{
					continue;
				}

				const FString ShaderPlatformLabel = GetShaderPlatformLabel(ShaderPlatform);
				const FString QualityLabel = GetMaterialQualityLevelLabel(QualityLevel);
#else
		static constexpr ERHIFeatureLevel::Type FeatureLevels[] =
		{
			ERHIFeatureLevel::ES3_1,
			ERHIFeatureLevel::SM5,
			ERHIFeatureLevel::SM6,
		};
		for (const ERHIFeatureLevel::Type FeatureLevel : FeatureLevels)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < static_cast<int32>(EMaterialQualityLevel::Num); ++QualityLevelIndex)
			{
				const EMaterialQualityLevel::Type QualityLevel = static_cast<EMaterialQualityLevel::Type>(QualityLevelIndex);
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(FeatureLevel, QualityLevel);
				if (!MaterialResource)
				{
					continue;
				}

				const FString ShaderPlatformLabel = ::LexToString(FeatureLevel);
				const FString QualityLabel = GetMaterialQualityLevelLabel(QualityLevel);
#endif
				for (const FString& Error : MaterialResource->GetCompileErrors())
				{
					const FString RawError = Error.TrimStartAndEnd();
					if (RawError.IsEmpty())
					{
						continue;
					}

					FDreamShaderDiagnosticLocation ParsedLocation;
					const bool bHasParsedLocation = FDreamShaderDiagnosticsStore::TryParseErrorLocation(RawError, ParsedLocation);
					const bool bMapsToDreamShaderSource = bHasParsedLocation && UE::DreamShader::IsDreamShaderSourceFile(ParsedLocation.FilePath);

					// ParsedLocation.Message is FText (dynamic error text); the platform/quality labels
					// are dynamic FStrings. The LOCTEXT pattern is the source-language literal, so the
					// funnel serializes an English "[platform / quality] message" regardless of culture.
					FText ErrorText;
					if (bHasParsedLocation)
					{
						ErrorText = ParsedLocation.Message;
					}
					else
					{
						ErrorText = FText::FromString(GetFirstMeaningfulErrorLine(RawError));
					}
					const FText DisplayMessage = FText::Format(
						LOCTEXT("MaterialCompileErrorHeader", "[{0} / {1}] {2}"),
						FText::FromString(ShaderPlatformLabel),
						FText::FromString(QualityLabel),
						ErrorText);

					const FString DeduplicationKey = FString::Printf(
						TEXT("%s|%s|%s|%s|%d|%d"),
						*SourceFilePath,
						*ShaderPlatformLabel,
						*QualityLabel,
						*DisplayMessage.ToString(),
						bMapsToDreamShaderSource ? ParsedLocation.Line : 1,
						bMapsToDreamShaderSource ? ParsedLocation.Column : 1);
					if (SeenDiagnosticKeys.Contains(DeduplicationKey))
					{
						continue;
					}
					SeenDiagnosticKeys.Add(DeduplicationKey);

					FDreamShaderDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
					Diagnostic.FilePath = bMapsToDreamShaderSource ? ParsedLocation.FilePath : SourceFilePath;
					Diagnostic.Message = DisplayMessage;
					// RawError is dynamic shader compiler output; FText::FromString keeps it out of gather.
					Diagnostic.Detail = FText::FromString(RawError);
					Diagnostic.Stage = TEXT("materialCompile");
					Diagnostic.AssetPath = MaterialAssetPath;
					Diagnostic.ShaderPlatform = ShaderPlatformLabel;
					Diagnostic.QualityLevel = QualityLabel;
					Diagnostic.Code = TEXT("material-compile");
					Diagnostic.Source = TEXT("DreamShader Material Compile");
					Diagnostic.Line = bMapsToDreamShaderSource ? ParsedLocation.Line : 1;
					Diagnostic.Column = bMapsToDreamShaderSource ? ParsedLocation.Column : 1;
				}
			}
		}

		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
	}

	namespace
	{
		/**
		 * The Dream-family combo button, shared with DreamFX and DreamGUI. Each of the three
		 * plugins keeps its own copy of this function (shapes shared, packages not): every one
		 * tries to add the combo, a FindEntry check makes that idempotent, and the entry belongs
		 * to the owner name "DreamToolsShared" which no module ever unregisters -- otherwise
		 * unloading whichever plugin won the race would take the other plugins' door with it.
		 * The combo opens the shared menu "DreamTools.OpenInVSCode"; each plugin adds its own
		 * section there under its own owner.
		 */
		void EnsureDreamToolsCombo()
		{
			UToolMenus* ToolMenus = UToolMenus::Get();
			const FName SharedMenuName(TEXT("DreamTools.Actions"));

			if (!ToolMenus->IsMenuRegistered(SharedMenuName))
			{
				FToolMenuOwnerScoped SharedOwner(TEXT("DreamToolsShared"));
				ToolMenus->RegisterMenu(SharedMenuName);
			}

			UToolMenu* Toolbar = ToolMenus->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar"));
			if (Toolbar == nullptr)
			{
				return;
			}
			FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("DreamTools"));
			if (Section.FindEntry(TEXT("DreamTools.OpenWorkspaceCombo")) != nullptr)
			{
				return;
			}

			FToolMenuOwnerScoped SharedOwner(TEXT("DreamToolsShared"));
			Section.AddEntry(FToolMenuEntry::InitComboButton(
				TEXT("DreamTools.OpenWorkspaceCombo"),
				FUIAction(),
				FNewToolMenuChoice(FOnGetContent::CreateLambda([]
				{
					return UToolMenus::Get()->GenerateWidget(TEXT("DreamTools.Actions"), FToolMenuContext());
				})),
				LOCTEXT("DreamToolsComboLabel", "Dream"),
				LOCTEXT("DreamToolsComboTooltip",
					"Dream-family language tools: open a source workspace in VSCode, or rebuild a whole source tree (DreamShader / DreamFX / DreamUI)."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor"))));
		}

		/**
		 * A whole-tree recompile behind a confirmation: it sits one menu row under the workspace
		 * openers, and a mis-click costs minutes. Every rebuild entry in the Dream menu asks first.
		 */
		bool ConfirmRecompileAllDreamShader()
		{
			return FMessageDialog::Open(EAppMsgType::YesNo,
				LOCTEXT("RecompileAllConfirm",
					"Recompile every DreamShader .dsm and .dsf source file?\n\n"
					"This rebuilds all generated materials and can take a while on a large tree."))
				== EAppReturnType::Yes;
		}
	}

	void FDreamShaderEditorBridge::RegisterMenus()
	{
		if (bIsShuttingDown || bMenusRegistered || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		bMenusRegistered = true;

		FToolMenuOwnerScoped MenuOwner(DreamShaderToolMenuOwnerName);

		if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddMenuEntry(
				TEXT("DreamShader.RecompileAll"),
				LOCTEXT("DreamShaderRecompileLabel", "Recompile DSM"),
				LOCTEXT("DreamShaderRecompileTooltip", "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateSPLambda(AsShared(), [this]
				{
					if (ConfirmRecompileAllDreamShader())
					{
						RequestRecompileAll();
					}
				})));
			Section.AddMenuEntry(
				TEXT("DreamShader.CleanGeneratedShaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersLabel", "Clean Generated Shaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersTooltip", "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestCleanGeneratedShaders)));
			Section.AddMenuEntry(
				TEXT("DreamShader.CleanPersistedGeneratedAssets"),
				LOCTEXT("DreamShaderCleanPersistedGeneratedAssetsLabel", "Clean Persisted Generated Assets"),
				LOCTEXT("DreamShaderCleanPersistedGeneratedAssetsTooltip", "Delete DreamShader-generated material assets that are saved on disk (they shadow in-memory material mode). Shows a confirmation with the full list; source files are untouched and regenerate in memory."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestCleanPersistedGeneratedAssets)));
			Section.AddMenuEntry(
				TEXT("DreamShader.ToggleShowInMemoryMaterials"),
				LOCTEXT("DreamShaderToggleShowInMemoryMaterialsLabel", "Show In-Memory Materials"),
				LOCTEXT("DreamShaderToggleShowInMemoryMaterialsTooltip", "Show memory-only ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. Graph-backend materials are plain UMaterials and are always visible, so this toggle does not affect them. While shown, an explicit Save on one would persist it to disk (the shadow warning and Clean command cover recovery)."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::ToggleShowInMemoryMaterialsInContentBrowser),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([]()
					{
						return GetDefault<UDreamShaderSettings>()->bShowInMemoryMaterialsInContentBrowser;
					})),
				EUserInterfaceActionType::ToggleButton);
			Section.AddMenuEntry(
				TEXT("DreamShader.OpenWorkspace"),
				LOCTEXT("DreamShaderOpenWorkspaceLabel", "Open Dream Shader Workspace (VSCode)"),
				LOCTEXT("DreamShaderOpenWorkspaceTooltip", "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::OpenDreamShaderWorkspace)));
		}

		// Both toolbar buttons moved into the Dream-family combo: three plugins, one button, and
		// the whole-tree recompile now asks before it runs.
		EnsureDreamToolsCombo();
		if (UToolMenu* SharedMenu = UToolMenus::Get()->ExtendMenu(TEXT("DreamTools.Actions")))
		{
			FToolMenuSection& SharedSection = SharedMenu->FindOrAddSection(TEXT("DreamShader"),
				LOCTEXT("DreamShaderSharedSectionLabel", "DreamShader"));
			SharedSection.AddMenuEntry(
				TEXT("DreamShader.OpenWorkspaceShared"),
				LOCTEXT("DreamShaderOpenWorkspaceSharedLabel", "DreamShader Workspace"),
				LOCTEXT("DreamShaderOpenWorkspaceToolbarTooltip", "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::OpenDreamShaderWorkspace)));
			SharedSection.AddMenuEntry(
				TEXT("DreamShader.RecompileAllShared"),
				LOCTEXT("DreamShaderRecompileSharedLabel", "Recompile DSM"),
				LOCTEXT("DreamShaderRecompileSharedTooltip", "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateSPLambda(AsShared(), [this]
				{
					if (ConfirmRecompileAllDreamShader())
					{
						RequestRecompileAll();
					}
				})));
		}

		if (UToolMenu* MaterialFunctionAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialFunction::StaticClass()))
		{
			FToolMenuSection& Section = MaterialFunctionAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.VirtualFunctionAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu));
		}
		if (UToolMenu* MaterialLayerAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialFunctionMaterialLayer::StaticClass()))
		{
			FToolMenuSection& Section = MaterialLayerAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.MaterialLayerAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu));
		}
		if (UToolMenu* MaterialLayerBlendAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialFunctionMaterialLayerBlend::StaticClass()))
		{
			FToolMenuSection& Section = MaterialLayerBlendAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.MaterialLayerBlendAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu));
		}

		if (UToolMenu* MaterialAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterial::StaticClass()))
		{
			FToolMenuSection& Section = MaterialAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.MaterialAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialAssetMenu));
		}

		// The ThinCustom backend is the project default, so the asset a user is most likely to hand-edit
		// is a UDreamShaderMaterialInstance -- and it was the one asset class with no DreamShader menu
		// on it at all, which left the divergence report naming actions that could not be reached.
		if (UToolMenu* InstanceAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UDreamShaderMaterialInstance::StaticClass()))
		{
			FToolMenuSection& Section = InstanceAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.MaterialInstanceAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialInstanceAssetMenu));
		}

		if (UToolMenu* MaterialEditorToolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.MaterialEditor.ToolBar")))
		{
			FToolMenuSection& Section = MaterialEditorToolbar->FindOrAddSection(TEXT("DreamShader"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.MaterialEditorToolbarActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialEditorToolbar));
		}
	}

	void FDreamShaderEditorBridge::PopulateMaterialAssetMenu(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (!Context || Context->SelectedAssets.Num() != 1 || !Context->SelectedAssets[0].IsInstanceOf(UMaterial::StaticClass()))
		{
			return;
		}

		UMaterial* Material = Cast<UMaterial>(Context->SelectedAssets[0].GetAsset());
		if (!Material)
		{
			return;
		}

		InSection.AddSubMenu(
			TEXT("DreamShader.MaterialActions"),
			LOCTEXT("DreamShaderMaterialActionsLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialActionsTooltip", "DreamShader actions for this Material."),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialDreamShaderMenu,
				TWeakObjectPtr<UMaterial>(Material)),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (!Context || Context->SelectedAssets.Num() != 1)
		{
			return;
		}

		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Context->SelectedAssets[0].GetAsset());
		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddSubMenu(
			TEXT("DreamShader.MaterialFunctionActions"),
			LOCTEXT("DreamShaderMaterialFunctionActionsLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionActionsTooltip", "DreamShader actions for this Material Function."),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
	}

	void FDreamShaderEditorBridge::PopulateMaterialInstanceAssetMenu(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (!Context || Context->SelectedAssets.Num() != 1)
		{
			return;
		}

		UDreamShaderMaterialInstance* Instance = Cast<UDreamShaderMaterialInstance>(Context->SelectedAssets[0].GetAsset());
		if (!Instance)
		{
			return;
		}

		InSection.AddSubMenu(
			TEXT("DreamShader.MaterialInstanceActions"),
			LOCTEXT("DreamShaderMaterialInstanceActionsLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialInstanceActionsTooltip", "DreamShader actions for this generated material."),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialInstanceDreamShaderMenu,
				TWeakObjectPtr<UObject>(Instance)),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
	}

	void FDreamShaderEditorBridge::PopulateMaterialInstanceDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UObject> Instance)
	{
		if (!InMenu || !Instance.IsValid())
		{
			return;
		}

		FToolMenuSection& ProvenanceSection = InMenu->AddSection(
			TEXT("DreamShader.ProvenanceActions"),
			LOCTEXT("DreamShaderInstanceProvenanceActionsSection", "Generated Asset"));
		PopulateProvenanceActions(ProvenanceSection, Instance);
	}

	void FDreamShaderEditorBridge::PopulateMaterialEditorToolbar(FToolMenuSection& InSection)
	{
		const UMaterialEditorMenuContext* Context = InSection.FindContext<UMaterialEditorMenuContext>();
		TSharedPtr<IMaterialEditor> MaterialEditor = Context ? Context->MaterialEditor.Pin() : nullptr;
		if (!MaterialEditor.IsValid())
		{
			return;
		}

		UMaterial* Material = nullptr;
		UMaterialFunction* MaterialFunction = nullptr;
		const TArray<UObject*>* EditingObjects = MaterialEditor->GetObjectsCurrentlyBeingEdited();
		if (EditingObjects)
		{
			for (UObject* EditingObject : *EditingObjects)
			{
				Material = Cast<UMaterial>(EditingObject);
				if (Material)
				{
					break;
				}
				MaterialFunction = Cast<UMaterialFunction>(EditingObject);
				if (MaterialFunction)
				{
					break;
				}
			}
		}

		if (Material)
		{
			InSection.AddEntry(FToolMenuEntry::InitComboButton(
				TEXT("DreamShader.MaterialToolbarMenu"),
				FUIAction(),
				FNewToolMenuDelegate::CreateSP(
					AsShared(),
					&FDreamShaderEditorBridge::PopulateMaterialDreamShaderMenu,
					TWeakObjectPtr<UMaterial>(Material)),
				LOCTEXT("DreamShaderMaterialToolbarMenuLabel", "DreamShader"),
				LOCTEXT("DreamShaderMaterialToolbarMenuTooltip", "DreamShader actions for this Material."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings"))));
			return;
		}

		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddEntry(FToolMenuEntry::InitComboButton(
			TEXT("DreamShader.MaterialFunctionToolbarMenu"),
			FUIAction(),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuTooltip", "DreamShader actions for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings"))));
	}

	void FDreamShaderEditorBridge::PopulateProvenanceActions(FToolMenuSection& InSection, TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		if (!AssetObject || !HasDreamShaderSourceMetadata(AssetObject))
		{
			// Not a generated asset: none of the three actions mean anything, and offering "stop
			// managing this" on something DreamShader never managed is just noise.
			return;
		}

		const EDreamShaderDigestState State = ClassifyGeneratedAsset(AssetObject);
		const bool bDiverged = State == EDreamShaderDigestState::Diverged;

		InSection.AddMenuEntry(
			TEXT("DreamShader.RevertToSource"),
			bDiverged
				? LOCTEXT("DreamShaderRevertDivergedLabel", "Revert to Source (discards your edits)")
				: LOCTEXT("DreamShaderRevertLabel", "Revert to Source"),
			LOCTEXT("DreamShaderRevertTooltip", "Rebuild this asset from the DreamShader source it was generated from, discarding every hand edit in it. The source file is not modified."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
			FUIAction(FExecuteAction::CreateStatic(&RevertGeneratedAssetToSource, Asset)));

		InSection.AddMenuEntry(
			TEXT("DreamShader.AdoptIntoSource"),
			LOCTEXT("DreamShaderAdoptLabel", "Adopt Into Source"),
			LOCTEXT("DreamShaderAdoptTooltip", "Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateStatic(&AdoptGeneratedAssetIntoSource, Asset)));

		InSection.AddMenuEntry(
			TEXT("DreamShader.DetachFromDreamShader"),
			LOCTEXT("DreamShaderDetachLabel", "Detach From DreamShader"),
			LOCTEXT("DreamShaderDetachTooltip", "Keep this asset exactly as it is and stop DreamShader from ever rebuilding it. It becomes an ordinary hand-authored asset."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Unlink")),
			FUIAction(FExecuteAction::CreateStatic(&DetachGeneratedAssetFromDreamShader, Asset)));
	}

	void FDreamShaderEditorBridge::PopulateMaterialDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterial> Material)
	{
		if (!InMenu || !Material.IsValid())
		{
			return;
		}

		{
			FToolMenuSection& ProvenanceSection = InMenu->AddSection(
				TEXT("DreamShader.ProvenanceActions"),
				LOCTEXT("DreamShaderProvenanceActionsSection", "Generated Asset"));
			PopulateProvenanceActions(ProvenanceSection, TWeakObjectPtr<UObject>(Material.Get()));
		}

		FToolMenuSection& Section = InMenu->AddSection(
			TEXT("DreamShader.DecompileActions"),
			LOCTEXT("DreamShaderDecompileActionsSection", "Decompiler"));
		Section.AddMenuEntry(
			TEXT("DreamShader.ExportMaterialDSM"),
			LOCTEXT("DreamShaderExportMaterialDSMLabel", "Export DSM"),
			LOCTEXT("DreamShaderExportMaterialDSMTooltip", "Export this Material graph to a DreamShader .dsm source file."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::ExportMaterialToDreamShaderFile,
				Material)));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		if (!InMenu || !MaterialFunction.IsValid())
		{
			return;
		}

		{
			FToolMenuSection& ProvenanceSection = InMenu->AddSection(
				TEXT("DreamShader.ProvenanceActions"),
				LOCTEXT("DreamShaderFunctionProvenanceActionsSection", "Generated Asset"));
			PopulateProvenanceActions(ProvenanceSection, TWeakObjectPtr<UObject>(MaterialFunction.Get()));
		}

		FToolMenuSection& DecompileSection = InMenu->AddSection(
			TEXT("DreamShader.DecompileActions"),
			LOCTEXT("DreamShaderFunctionDecompileActionsSection", "Decompiler"));
		DecompileSection.AddMenuEntry(
			TEXT("DreamShader.ExportFunctionDSF"),
			LOCTEXT("DreamShaderExportFunctionDSFLabel", "Export DSF"),
			LOCTEXT("DreamShaderExportFunctionDSFTooltip", "Export this Material Function graph to a DreamShader .dsf source file."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::ExportMaterialFunctionToDreamShaderFile,
				MaterialFunction)));

		FToolMenuSection& Section = InMenu->AddSection(
			TEXT("DreamShader.VirtualFunctionActions"),
			LOCTEXT("DreamShaderVirtualFunctionActionsSection", "VirtualFunction"));
		FDreamShaderVirtualFunctionDefinitionLocation ExistingDefinition;
		if (FDreamShaderVirtualFunctionSyncService::FindDefinitionForMaterialFunction(MaterialFunction.Get(), ExistingDefinition))
		{
			Section.AddMenuEntry(
				TEXT("DreamShader.OpenVirtualFunction"),
				LOCTEXT("DreamShaderOpenVirtualFunctionLabel", "OpenVirtualFunction"),
				LOCTEXT("DreamShaderOpenVirtualFunctionTooltip", "Open the existing DreamShader VirtualFunction definition in VSCode."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateSP(
					AsShared(),
					&FDreamShaderEditorBridge::OpenVirtualFunctionDefinitionFile,
					MaterialFunction)));
			Section.AddMenuEntry(
				TEXT("DreamShader.CopyVirtualFunctionReference"),
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceLabel", "Copy Virtual Function Reference"),
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceTooltip", "Copy a DreamShader Graph call that references this existing VirtualFunction."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
				FUIAction(FExecuteAction::CreateSP(
					AsShared(),
					&FDreamShaderEditorBridge::CopyVirtualFunctionReference,
					MaterialFunction)));
			return;
		}

		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionLabel", "CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionTooltip", "Copy a complete DreamShader VirtualFunction declaration for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionDefinition,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionLabel", "CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionTooltip", "Create a .dsh file containing the VirtualFunction declaration."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallLabel", "CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallTooltip", "Copy a DreamShader Graph call example for this VirtualFunction."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionCall,
				MaterialFunction)));
	}

	void FDreamShaderEditorBridge::RequestRecompileAll()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		QueueFullScan(/*bForce*/ true);
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader queued a full .dsm/.dsf recompile scan."));
	}

	void FDreamShaderEditorBridge::RequestCleanGeneratedShaders()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		CleanGeneratedShaderDirectory();
		QueueFullScan(/*bForce*/ true);
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cleaned generated shader includes and queued a full .dsm/.dsf recompile scan."));
	}

	int32 FDreamShaderEditorBridge::CollectPersistedGeneratedAssets(TArray<UObject*>& OutAssets)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDreamShaderMaterialInstance::StaticClass()->GetClassPathName());

		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			// Only assets that actually live on disk qualify; in-memory assets are the
			// desired end state. The provenance metadata gate means hand-authored materials are
			// never touched — only assets DreamShader itself generated (including orphans whose
			// source file has since been deleted or renamed).
			if (!FPackageName::DoesPackageExist(AssetData.PackageName.ToString()))
			{
				continue;
			}

			UObject* Asset = AssetData.GetAsset();
			if (Asset && !GetSourceFileMetadata(Asset).IsEmpty())
			{
				OutAssets.Add(Asset);
			}
		}

		return OutAssets.Num();
	}

	void FDreamShaderEditorBridge::ToggleShowInMemoryMaterialsInContentBrowser()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		UDreamShaderSettings* Settings = GetMutableDefault<UDreamShaderSettings>();
		Settings->bShowInMemoryMaterialsInContentBrowser = !Settings->bShowInMemoryMaterialsInContentBrowser;
		Settings->TryUpdateDefaultConfigFile();
		const bool bShow = Settings->bShowInMemoryMaterialsInContentBrowser;

		// IsAsset() reads the setting live; broadcast per-instance registry events so the Content
		// Browser (and open asset pickers) add/remove the tiles immediately instead of on the next
		// re-enumeration.
		int32 ToggledCount = 0;
		for (TObjectIterator<UDreamShaderMaterialInstance> It; It; ++It)
		{
			UDreamShaderMaterialInstance* Instance = *It;
			if (!IsValid(Instance) || !Instance->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
			{
				continue;
			}

			if (bShow)
			{
				FAssetRegistryModule::AssetCreated(Instance);
			}
			else
			{
				FAssetRegistryModule::AssetDeleted(Instance);
			}
			++ToggledCount;
		}

		ShowDreamShaderNotification(
			FText::Format(
				bShow
					? LOCTEXT("DreamShaderInMemoryMaterialsShown", "Showing {0} in-memory material(s) in the Content Browser and asset pickers.")
					: LOCTEXT("DreamShaderInMemoryMaterialsHidden", "Hidden {0} in-memory material(s) from the Content Browser and asset pickers."),
				FText::AsNumber(ToggledCount)),
			SNotificationItem::CS_Success);
	}

	void FDreamShaderEditorBridge::RequestCleanPersistedGeneratedAssets()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		TArray<UObject*> AssetsToDelete;
		if (CollectPersistedGeneratedAssets(AssetsToDelete) == 0)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCleanPersistedNoneFound", "No persisted DreamShader-generated assets found."),
				SNotificationItem::CS_Success);
			return;
		}

		// Standard editor delete flow: lists the assets, checks references, and asks the user to
		// confirm. Sources (.dsm/.dsf) are untouched, so everything is regenerable.
		const int32 DeletedCount = ObjectTools::DeleteObjects(AssetsToDelete, /*bShowConfirmation*/ true);
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader deleted %d of %d persisted generated asset(s)."), DeletedCount, AssetsToDelete.Num());

		if (DeletedCount > 0)
		{
			// Recreate the deleted assets in memory right away so references resolve without a restart.
			GenerateAllInMemoryMaterials();
		}

		ShowDreamShaderNotification(
			FText::Format(
				LOCTEXT("DreamShaderCleanPersistedResult", "Deleted {0} of {1} persisted generated asset(s)."),
				FText::AsNumber(DeletedCount),
				FText::AsNumber(AssetsToDelete.Num())),
			DeletedCount > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}

	void FDreamShaderEditorBridge::OpenDreamShaderWorkspace()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		FDreamShaderWorkspaceService::ExportMaterialExpressionManifest();
		FDreamShaderWorkspaceService::ExportDreamShaderSettingsManifest();
		FDreamShaderWorkspaceService::ExportSubstrateBuiltinsManifest();
		FDreamShaderWorkspaceService::ExportPreprocessorDefinesManifest();

		FString WorkspaceFilePath;
		FString Error;
		if (!FDreamShaderWorkspaceService::WriteDreamShaderWorkspaceFile(WorkspaceFilePath, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to create workspace: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to create DreamShader workspace: %s"), *Error);
			return;
		}

		if (FDreamShaderEditorLaunchUtils::LaunchVSCodeWorkspace(WorkspaceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace in VSCode: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace in VSCode: %s"), *WorkspaceFilePath);
			return;
		}

		if (FPlatformProcess::LaunchFileInDefaultExternalApplication(*WorkspaceFilePath, nullptr, ELaunchVerb::Edit, false))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace with the default editor: %s"), *WorkspaceFilePath);
			return;
		}

		if (FDreamShaderEditorLaunchUtils::LaunchTextFileWithNotepad(WorkspaceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace in Notepad: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace in Notepad: %s"), *WorkspaceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("DreamShader could not open workspace: %s"), *WorkspaceFilePath)),
			SNotificationItem::CS_Fail);
		UE_LOG(LogDreamShader, Warning, TEXT("Failed to open DreamShader workspace: %s"), *WorkspaceFilePath);
	}

	void FDreamShaderEditorBridge::ExportMaterialToDreamShaderFile(TWeakObjectPtr<UMaterial> Material)
	{
		UMaterial* MaterialAsset = Material.Get();
		if (!MaterialAsset)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderExportMaterialNoAsset", "DreamShader could not find the selected Material."),
				SNotificationItem::CS_Fail);
			return;
		}

		FDreamShaderDecompileService DecompileService(GetGraphDecompiler());
		UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = MaterialAsset;
		const UE::DreamShader::Editor::FDreamShaderDecompileResult Result = DecompileService.DecompileAsset(Request);
		if (!Result.bSucceeded)
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to export DSM: %s"), *Result.Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to export Material '%s' to DSM: %s"), *MaterialAsset->GetPathName(), *Result.Error);
			return;
		}

		const FString SourceFilePath = Result.OutputFilePath;
		FString SaveError;
		if (!FDecompiledSourceWriter::Save(Result, SaveError))
		{
			ShowDreamShaderNotification(
				FText::FromString(SaveError),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write decompiled Material DSM file '%s': %s"), *SourceFilePath, *SaveError);
			return;
		}

		if (!FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(SourceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Exported DSM but could not open it: %s"), *SourceFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Exported DSM '%s' but failed to open it."), *SourceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Exported DSM: %s"), *SourceFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Exported Material '%s' to DSM '%s'."), *MaterialAsset->GetPathName(), *SourceFilePath);
	}

	void FDreamShaderEditorBridge::ExportMaterialFunctionToDreamShaderFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderExportFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FDreamShaderDecompileService DecompileService(GetGraphDecompiler());
		UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = Function;
		const UE::DreamShader::Editor::FDreamShaderDecompileResult Result = DecompileService.DecompileAsset(Request);
		if (!Result.bSucceeded)
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to export DSF: %s"), *Result.Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to export MaterialFunction '%s' to DSF: %s"), *Function->GetPathName(), *Result.Error);
			return;
		}

		const FString SourceFilePath = Result.OutputFilePath;
		FString SaveError;
		if (!FDecompiledSourceWriter::Save(Result, SaveError))
		{
			ShowDreamShaderNotification(
				FText::FromString(SaveError),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write decompiled MaterialFunction DSF file '%s': %s"), *SourceFilePath, *SaveError);
			return;
		}

		if (!FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(SourceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Exported DSF but could not open it: %s"), *SourceFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Exported DSF '%s' but failed to open it."), *SourceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Exported DSF: %s"), *SourceFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Exported MaterialFunction '%s' to DSF '%s'."), *Function->GetPathName(), *SourceFilePath);
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildGraphDecompilerVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*DefinitionText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction definition for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction definition for '%s'.\n%s"), *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCreateVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FDreamShaderVirtualFunctionDefinitionLocation ExistingDefinition;
		if (FDreamShaderVirtualFunctionSyncService::FindDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			OpenVirtualFunctionDefinitionFile(MaterialFunction);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildGraphDecompilerVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition file for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		const FString DefinitionFilePath = FDreamShaderVirtualFunctionService::MakeDefinitionFilePath(Function);
		const FString DefinitionDirectory = FPaths::GetPath(DefinitionFilePath);
		if (!IFileManager::Get().MakeDirectory(*DefinitionDirectory, true))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to create directory: %s"), *DefinitionDirectory)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to create VirtualFunction definition directory '%s'."), *DefinitionDirectory);
			return;
		}

		if (!FFileHelper::SaveStringToFile(DefinitionText, *DefinitionFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to write VirtualFunction file: %s"), *DefinitionFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write VirtualFunction definition file '%s'."), *DefinitionFilePath);
			return;
		}

		if (!FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(DefinitionFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Created VirtualFunction file but could not open it: %s"), *DefinitionFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Created VirtualFunction definition file '%s' but failed to open it."), *DefinitionFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Created VirtualFunction file: %s"), *DefinitionFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Created VirtualFunction definition file '%s' for '%s'.\n%s"), *DefinitionFilePath, *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::OpenVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderOpenVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FDreamShaderVirtualFunctionDefinitionLocation ExistingDefinition;
		if (!FDreamShaderVirtualFunctionSyncService::FindDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not find a VirtualFunction definition for %s."), *Function->GetName())),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("No VirtualFunction definition found for '%s'."), *Function->GetPathName());
			return;
		}

		if (!FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(
			ExistingDefinition.SourceFilePath,
			ExistingDefinition.Line,
			ExistingDefinition.Column))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not open VirtualFunction file: %s"), *ExistingDefinition.SourceFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to open VirtualFunction definition file '%s'."), *ExistingDefinition.SourceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Opened VirtualFunction definition: %s"), *ExistingDefinition.SourceFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("Opened VirtualFunction definition '%s' for '%s'."),
			*ExistingDefinition.SourceFilePath,
			*Function->GetPathName());
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionReference(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FDreamShaderVirtualFunctionDefinitionLocation ExistingDefinition;
		if (!FDreamShaderVirtualFunctionSyncService::FindDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not find a VirtualFunction definition for %s."), *Function->GetName())),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("No VirtualFunction definition found for '%s'."), *Function->GetPathName());
			return;
		}

		FString CallText;
		FString Error;
		if (!FDreamShaderVirtualFunctionService::BuildCallTextFromSignature(
			ExistingDefinition.FunctionName,
			ExistingDefinition.Inputs,
			ExistingDefinition.Outputs,
			CallText,
			Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction reference: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("Failed to build VirtualFunction reference for '%s' from '%s': %s"),
				*Function->GetPathName(),
				*ExistingDefinition.SourceFilePath,
				*Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*CallText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction reference for %s."), *ExistingDefinition.FunctionName)),
			SNotificationItem::CS_Success);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("Copied VirtualFunction reference for '%s' from '%s': %s"),
			*Function->GetPathName(),
			*ExistingDefinition.SourceFilePath,
			*CallText);
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionCallNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString CallText;
		FString Error;
		if (!FDreamShaderVirtualFunctionService::BuildCallText(Function, CallText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction call: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction call for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*CallText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction call for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction call for '%s': %s"), *Function->GetPathName(), *CallText);
	}

	void FDreamShaderEditorBridge::CleanGeneratedShaderDirectory()
	{
		const FString GeneratedShaderDirectory = UE::DreamShader::GetGeneratedShaderDirectory();

		// Safety guard: the generated-shader directory is user-configurable
		// (DreamShaderSettings.GeneratedShaderDirectory). A misconfiguration pointing it at the project
		// root, Content, or an arbitrary absolute path must never cause a recursive delete, so refuse to
		// operate anywhere outside the project's Intermediate tree.
		const FString FullGeneratedDir = FPaths::ConvertRelativePathToFull(GeneratedShaderDirectory);
		const FString FullIntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
		if (!FPaths::IsUnderDirectory(FullGeneratedDir, *FullIntermediateDir))
		{
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader refused to clean generated shaders: '%s' is not inside the project Intermediate directory. "
					 "Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning."),
				*GeneratedShaderDirectory);
			return;
		}

		IFileManager& FileManager = IFileManager::Get();

		// Delete only the shader files we generate (*.ush), one at a time -- never DeleteDirectory the
		// whole tree. Even if the directory is (mis)shared with unrelated files, nothing but our own
		// generated shaders is removed, and the directory itself is left in place.
		TArray<FString> GeneratedShaderFiles;
		FileManager.FindFilesRecursive(
			GeneratedShaderFiles,
			*GeneratedShaderDirectory,
			TEXT("*.ush"),
			true,
			false,
			false);

		int32 DeletedFileCount = 0;
		for (const FString& GeneratedShaderFile : GeneratedShaderFiles)
		{
			if (FileManager.Delete(*GeneratedShaderFile, /*bRequireExists*/ false, /*bEvenIfReadOnly*/ true))
			{
				++DeletedFileCount;
			}
		}

		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader deleted %d generated shader file(s) from '%s'."),
			DeletedFileCount,
			*GeneratedShaderDirectory);
	}

	void FDreamShaderEditorBridge::RebuildDependencyGraph()
	{
		FDreamShaderDependencyGraphService::RebuildMaterialDependencyGraph(HeaderDependentsByFile);
	}

	void FDreamShaderEditorBridge::SyncVirtualFunctionDefinitions()
	{
		FDreamShaderVirtualFunctionSyncResult SyncResult =
			FDreamShaderVirtualFunctionSyncService::SyncDefinitions(
				[](const UMaterialFunction* Function, FString& OutDefinition, FString& OutError)
				{
					return BuildGraphDecompilerVirtualFunctionDefinition(Function, OutDefinition, OutError);
				});

		for (FDreamShaderVirtualFunctionSyncFileResult& FileResult : SyncResult.Files)
		{
			if (FileResult.UpdatedDefinitionCount > 0)
			{
				UE_LOG(
					LogDreamShader,
					Display,
					TEXT("DreamShader refreshed %d VirtualFunction definition(s) in '%s'."),
					FileResult.UpdatedDefinitionCount,
					*FileResult.SourceFilePath);
			}

			if (FileResult.Diagnostics.IsEmpty())
			{
				if (FileResult.DefinitionCount > 0)
				{
					ClearDiagnostics(FileResult.SourceFilePath);
				}
			}
			else
			{
				SetDiagnostics(FileResult.SourceFilePath, MoveTemp(FileResult.Diagnostics));
			}
		}

		if (SyncResult.ScannedDefinitionCount > 0 || SyncResult.UpdatedDefinitionCount > 0 || SyncResult.ErrorCount > 0)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader scanned %d VirtualFunction definition(s), refreshed %d, reported %d issue(s)."),
				SyncResult.ScannedDefinitionCount,
				SyncResult.UpdatedDefinitionCount,
				SyncResult.ErrorCount);
		}
	}

	void FDreamShaderEditorBridge::SetDiagnostics(const FString& SourceFilePath, TArray<FDreamShaderDiagnosticRecord>&& Diagnostics)
	{
		DiagnosticsStore.SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
	}

	void FDreamShaderEditorBridge::ClearDiagnostics(const FString& SourceFilePath)
	{
		DiagnosticsStore.ClearDiagnostics(SourceFilePath);
	}

	void FDreamShaderEditorBridge::ClearDiagnosticsForSourceAndDependencies(const FString& SourceFilePath)
	{
		ClearDiagnostics(SourceFilePath);

		TSet<FString> Dependencies;
		TSet<FString> VisitedFiles;
		FDreamShaderDependencyGraphService::CollectHeaderDependenciesRecursive(SourceFilePath, Dependencies, VisitedFiles);
		for (const FString& HeaderFile : Dependencies)
		{
			ClearDiagnostics(HeaderFile);
		}
	}

	void FDreamShaderEditorBridge::UpdateDiagnosticsFile()
	{
		DiagnosticsStore.WriteToFile(GetDiagnosticsFilePath());
		DiagnosticsStore.WriteToDirectory(GetDiagnosticsDirectory());
		DiagnosticsStore.WriteToDatabase(FDreamShaderWorkspaceService::GetBridgeDatabaseFilePath());
		// The commit point: every compile route ends here once its findings are in the store.
		DiagnosticsChangedEvent.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
