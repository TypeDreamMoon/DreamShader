#include "DreamShaderEditorBridge.h"

#include "Bridge/DreamShaderPreviewWebSocketServer.h"
#include "DreamShaderCompileService.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"
#include "Preview/DreamShaderPreviewRenderer.h"
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
#include "ObjectTools.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "ContentBrowserMenuContexts.h"
#include "DirectoryWatcherModule.h"
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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
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

	}


	FString FDreamShaderEditorBridge::GetBridgeDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"));
	}

	FString FDreamShaderEditorBridge::GetRequestDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("Requests"));
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

	void FDreamShaderEditorBridge::Startup()
	{
		bIsShuttingDown = false;

		IFileManager::Get().MakeDirectory(*GetBridgeDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetRequestDirectory(), true);
		IFileManager::Get().MakeDirectory(*FDreamShaderPreviewRenderer::GetPreviewDirectory(), true);
		FDreamShaderWorkspaceService::ResetBridgeDatabase();

		FDreamShaderWorkspaceService::ExportMaterialExpressionManifest();
		FDreamShaderWorkspaceService::ExportDreamShaderSettingsManifest();
		FDreamShaderWorkspaceService::ExportSubstrateBuiltinsManifest();
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
		DiagnosticsStore.Reset();
	}

	void FDreamShaderEditorBridge::QueueFullScan()
	{
		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(SourceFiles);
		RebuildDependencyGraph();

		const double Now = FPlatformTime::Seconds();
		for (FString& SourceFile : SourceFiles)
		{
			PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFile), Now);
		}
	}

	void FDreamShaderEditorBridge::HandlePostEngineInit()
	{
		// In-memory generation is the editor's always-on behavior (source files are the authoring
		// surface; the editor never writes per-material .uasset files).
		GenerateAllInMemoryMaterials();
	}

	void FDreamShaderEditorBridge::HandleSettingsPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
	{
		// Switching the default compiler backend changes how every backend-less source materializes, so
		// regenerate everything in memory to reflect the new choice.
		if (!Object || !Object->IsA<UDreamShaderSettings>()
			|| Event.GetPropertyName() != GET_MEMBER_NAME_CHECKED(UDreamShaderSettings, DefaultBackend))
		{
			return;
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader default compiler backend changed; regenerating all source files in memory."));
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
		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);

		if (SourceFiles.IsEmpty())
		{
			return;
		}

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
			const bool bSuccess = FMaterialGenerator::GenerateAssetsFromFile(NormalizedPath, Message, true, true);
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

	void FDreamShaderEditorBridge::QueueSourceFile(const FString& SourceFilePath)
	{
		PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath), FPlatformTime::Seconds());
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

		ProcessRequestFiles();
		ProcessReadyFiles();
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
		TArray<FString> RequestFiles;
		IFileManager::Get().FindFiles(RequestFiles, *FPaths::Combine(GetRequestDirectory(), TEXT("*.json")), true, false);

		for (const FString& RequestFileName : RequestFiles)
		{
			const FString RequestPath = FPaths::Combine(GetRequestDirectory(), RequestFileName);

			FString RequestText;
			if (!FFileHelper::LoadFileToString(RequestText, *RequestPath))
			{
				IFileManager::Get().Delete(*RequestPath);
				continue;
			}

			TSharedPtr<FJsonObject> RequestObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestText);
			if (FJsonSerializer::Deserialize(Reader, RequestObject) && RequestObject.IsValid())
			{
				FString Action;
				FString Scope;
				RequestObject->TryGetStringField(TEXT("action"), Action);
				RequestObject->TryGetStringField(TEXT("scope"), Scope);
				if (Action.Equals(TEXT("recompile"), ESearchCase::IgnoreCase))
				{
					if (Scope.Equals(TEXT("all"), ESearchCase::IgnoreCase))
					{
						RequestRecompileAll();
					}
					else if (Scope.Equals(TEXT("file"), ESearchCase::IgnoreCase))
					{
						FString SourceFilePath;
						if (RequestObject->TryGetStringField(TEXT("sourceFile"), SourceFilePath) && !SourceFilePath.IsEmpty())
						{
							QueueSourceFile(SourceFilePath);
						}
					}
				}
				else if (Action.Equals(TEXT("cleanGeneratedShaders"), ESearchCase::IgnoreCase))
				{
					RequestCleanGeneratedShaders();
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
					FString RequestId;
					RequestObject->TryGetStringField(TEXT("requestId"), RequestId);
					FDreamShaderPreviewRenderer::WritePreviewResult(PreviewResult, bPreviewSucceeded ? TEXT("ready") : TEXT("error"), RequestId);
					if (bPreviewSucceeded)
					{
						UE_LOG(LogDreamShader, Display, TEXT("DreamShader preview: %s"), *PreviewResult.Message);
					}
					else
					{
						UE_LOG(LogDreamShader, Error, TEXT("DreamShader preview: %s"), *PreviewResult.Message);
					}
				}
			}

			IFileManager::Get().Delete(*RequestPath);
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

		for (const FString& ReadyFile : ReadyFiles)
		{
			PendingFiles.Remove(ReadyFile);
			if (IFileManager::Get().FileExists(*ReadyFile))
			{
				ProcessSourceFile(ReadyFile);
			}
		}
	}

	void FDreamShaderEditorBridge::ProcessSourceFile(const FString& SourceFilePath)
	{
		UE::DreamShader::Compiler::FDreamShaderCompileService CompileService(UE::DreamShader::Editor::GetEditorCompileAdapter());
		const UE::DreamShader::Compiler::FDreamShaderCompileResult Result = CompileService.CompileAssets(SourceFilePath, false, /*bInMemory*/ true);
		if (Result.bSucceeded)
		{
			ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
			UpdateDiagnosticsFile();
			UE_LOG(LogDreamShader, Display, TEXT("%s"), *Result.Message);
			return;
		}

		TArray<FDreamShaderDiagnosticRecord> Diagnostics =
			FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics(SourceFilePath, Result.Message);
		ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
		UE_LOG(LogDreamShader, Error, TEXT("%s"), *Result.Message);
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

					const FString DisplayMessage = FString::Printf(
						TEXT("[%s / %s] %s"),
						*ShaderPlatformLabel,
						*QualityLabel,
						*(bHasParsedLocation ? ParsedLocation.Message : GetFirstMeaningfulErrorLine(RawError)));

					const FString DeduplicationKey = FString::Printf(
						TEXT("%s|%s|%s|%s|%d|%d"),
						*SourceFilePath,
						*ShaderPlatformLabel,
						*QualityLabel,
						*DisplayMessage,
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
					Diagnostic.Detail = RawError;
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
				LOCTEXT("DreamShaderRecompileTooltip", "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)));
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

		if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar")))
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.RecompileAllToolbar"),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)),
				LOCTEXT("DreamShaderRecompileToolbarLabel", "DSM"),
				LOCTEXT("DreamShaderRecompileToolbarTooltip", "Recompile all DreamShader .dsm and .dsf source files."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh"))));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.OpenWorkspaceToolbar"),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::OpenDreamShaderWorkspace)),
				LOCTEXT("DreamShaderOpenWorkspaceToolbarLabel", "Open Dream Shader Workspace (VSCode)"),
				LOCTEXT("DreamShaderOpenWorkspaceToolbarTooltip", "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor"))));
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

	void FDreamShaderEditorBridge::PopulateMaterialDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterial> Material)
	{
		if (!InMenu || !Material.IsValid())
		{
			return;
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

		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader queued a full .dsm/.dsf recompile scan."));
	}

	void FDreamShaderEditorBridge::RequestCleanGeneratedShaders()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		CleanGeneratedShaderDirectory();
		QueueFullScan();
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

	void FDreamShaderEditorBridge::UpdateDiagnosticsFile() const
	{
		DiagnosticsStore.WriteToFile(GetDiagnosticsFilePath());
		DiagnosticsStore.WriteToDirectory(GetDiagnosticsDirectory());
		DiagnosticsStore.WriteToDatabase(FDreamShaderWorkspaceService::GetBridgeDatabaseFilePath());
	}
}

#undef LOCTEXT_NAMESPACE
