// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPService.h"

#include "MCP/DreamShaderMCPActions.h"
#include "MCP/DreamShaderMCPEndpoint.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "Async/Async.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "LevelEditorViewport.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Settings/EditorLoadingSavingSettings.h"

#include <atomic>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <TlHelp32.h>
#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <unistd.h>
#endif

namespace UE::DreamShader::Editor::MCP
{
	namespace
	{
		TUniquePtr<FEndpoint> GEndpoint;
		FString GStartupError;
		FDelegateHandle GPostEngineInitHandle;
		EEndpointMode GMode = EEndpointMode::Editor;

		/** A host's own lifetime: the parent it watches, the shutdown it carries out, the frame cap it keeps. */
		struct FHostLifetime
		{
			FTSTicker::FDelegateHandle TickerHandle;
			FProcHandle Parent;
			uint32 ParentPid = 0;
			double NextParentCheck = 0.0;
			double ShutdownRequestedAt = 0.0;
			bool bExitRequested = false;
			int32 LastFrameCap = -1;
		};
		FHostLifetime GHost;

		/** The startup watchdog: a host that is not answering within the timeout exits instead of hanging. */
		std::atomic<bool> GHostStarted{ false };
		std::atomic<bool> GWatchdogStop{ false };
		TFuture<void> GWatchdog;
		/** The discovery file the endpoint published, for the watchdog to take back: it may not touch the endpoint. */
		FCriticalSection GDiscoveryPathLock;
		FString GDiscoveryPath;

		/** `-run=` without a script path or a `Commandlet` suffix: `DreamShaderHost`. */
		FString GetRunCommandletName()
		{
			FString Name;
			if (!FParse::Value(FCommandLine::Get(), TEXT("-run="), Name))
			{
				return FString();
			}
			int32 Dot = INDEX_NONE;
			if (Name.FindLastChar(TEXT('.'), Dot))
			{
				Name.RightChopInline(Dot + 1);
			}
			Name.RemoveFromEnd(TEXT("Commandlet"), ESearchCase::IgnoreCase);
			return Name;
		}

		/** The process that started this one: the MCP server, when it started the host. 0 when it cannot be told. */
		uint32 FindParentProcessId()
		{
#if PLATFORM_WINDOWS
			const DWORD Self = ::GetCurrentProcessId();
			const HANDLE Snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (Snapshot == INVALID_HANDLE_VALUE)
			{
				return 0;
			}
			uint32 Parent = 0;
			PROCESSENTRY32W Entry;
			Entry.dwSize = sizeof(Entry);
			if (::Process32FirstW(Snapshot, &Entry))
			{
				do
				{
					if (Entry.th32ProcessID == Self)
					{
						Parent = Entry.th32ParentProcessID;
						break;
					}
				}
				while (::Process32NextW(Snapshot, &Entry));
			}
			::CloseHandle(Snapshot);
			return Parent;
#elif PLATFORM_LINUX || PLATFORM_MAC
			return static_cast<uint32>(getppid());
#else
			return 0;
#endif
		}

		void RequestHostExit(const TCHAR* Why)
		{
			if (GHost.bExitRequested)
			{
				return;
			}
			GHost.bExitRequested = true;
			UE_LOG(LogDreamShader, Display, TEXT("DreamMCP host: %s; exiting."), Why);
			RequestEngineExit(TEXT("DreamMCP host"));
		}

		/**
		 * The host's own housekeeping, on the core ticker, in both forms of host:
		 *   * POST /shutdown: exit a quarter of a second later, so the answer leaves the socket first;
		 *   * the parent process gone: exit -- a server that died must not leave an engine behind;
		 *   * in the windowless editor: no realtime viewport, and a frame cap while idle (an unattended editor is never
		 *     throttled by the engine itself, see UEditorEngine::ShouldThrottleCPUUsage).
		 */
		bool TickHost(float)
		{
			// The engine loop is turning: whatever dialog could have held the startup did not.
			GHostStarted = true;
			if (!GEndpoint.IsValid())
			{
				return true;
			}
			const double Now = FPlatformTime::Seconds();

			if (GEndpoint->IsShutdownRequested())
			{
				if (GHost.ShutdownRequestedAt == 0.0)
				{
					GHost.ShutdownRequestedAt = Now;
				}
				else if (Now - GHost.ShutdownRequestedAt > 0.25)
				{
					RequestHostExit(TEXT("shutdown requested"));
				}
			}

			if (GHost.Parent.IsValid() && Now >= GHost.NextParentCheck)
			{
				GHost.NextParentCheck = Now + 1.0;
				if (!FPlatformProcess::IsProcRunning(GHost.Parent))
				{
					RequestHostExit(*FString::Printf(TEXT("the process that started it (%u) has exited"), GHost.ParentPid)); // I18N-EXEMPT: log text
				}
			}

			if (!IsRunningCommandlet() && GEditor)
			{
				// Asserted every frame rather than once: the editor may load its settings again after startup.
				if (UEditorLoadingSavingSettings* Saving = GetMutableDefault<UEditorLoadingSavingSettings>())
				{
					Saving->bAutoSaveEnable = false;
				}

				// The name an override is listed under; nobody sees a windowless host's viewports.
				const FText Override = FText::AsCultureInvariant(TEXT("DreamMCP host")); // I18N-EXEMPT: never displayed
				for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
				{
					if (Client && !Client->HasRealtimeOverride(Override))
					{
						Client->AddRealtimeOverride(false, Override);
					}
				}

				// Fifteen frames a second while idle; no cap while a job runs, since a job steps once per frame.
				const int32 FrameCap = GEndpoint->IsBusy() ? 0 : 15;
				if (FrameCap != GHost.LastFrameCap)
				{
					if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
					{
						MaxFps->Set(static_cast<float>(FrameCap), ECVF_SetByCode);
					}
					GHost.LastFrameCap = FrameCap;
				}
			}
			return true;
		}

		void StartHostLifetime()
		{
			uint32 ParentPid = 0;
			if (!FParse::Value(FCommandLine::Get(), TEXT("-DreamMCPParentPid="), ParentPid))
			{
				ParentPid = FindParentProcessId();
			}
			GHost.ParentPid = ParentPid;
			if (ParentPid != 0)
			{
				// A handle opened now keeps naming that process even if its id is reused later.
				GHost.Parent = FPlatformProcess::OpenProcess(ParentPid);
			}
			GHost.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickHost), 0.0f);
			UE_LOG(LogDreamShader, Display, TEXT("DreamMCP host: watching %s."),
				GHost.Parent.IsValid() ? *FString::Printf(TEXT("parent process %u"), ParentPid) : TEXT("no parent process")); // I18N-EXEMPT: log text
		}

		void StopHostLifetime()
		{
			if (GHost.TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(GHost.TickerHandle);
			}
			if (GHost.Parent.IsValid())
			{
				FPlatformProcess::CloseProc(GHost.Parent);
			}
			GHost = FHostLifetime();
		}

		/**
		 * A host is started by a server that waits for `hello`. A modal dialog the engine shows before that -- the
		 * unattended switch keeps most of them away, not necessarily all -- would hold the game thread forever, so a
		 * host whose engine loop has not turned once within -DreamMCPStartupTimeout seconds of this module loading
		 * (600 by default: a first start compiles shaders) says so and exits with code 3 instead of hanging. What
		 * blocks before the module loads (the engine's own module checks) is the server's hello timeout's to catch.
		 */
		void StartWatchdog()
		{
			int32 TimeoutSeconds = 600;
			FParse::Value(FCommandLine::Get(), TEXT("-DreamMCPStartupTimeout="), TimeoutSeconds);
			if (TimeoutSeconds <= 0)
			{
				return;
			}
			GHostStarted = false;
			GWatchdogStop = false;
			GWatchdog = Async(EAsyncExecution::Thread, [TimeoutSeconds]()
			{
				const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
				while (!GHostStarted && !GWatchdogStop)
				{
					if (FPlatformTime::Seconds() > Deadline)
					{
						UE_LOG(LogDreamShader, Error,
							TEXT("DreamMCP host: not answering after %d s -- a dialog or a hang holds the engine before the endpoint could start. Exiting with code 3."),
							TimeoutSeconds);
						// The endpoint may already have published itself; a forced exit would leave the file naming a dead
						// process. The file is taken back only while it still names this one.
						FString DiscoveryPath;
						{
							FScopeLock Lock(&GDiscoveryPathLock);
							DiscoveryPath = GDiscoveryPath;
						}
						FString Existing;
						if (!DiscoveryPath.IsEmpty() && FFileHelper::LoadFileToString(Existing, *DiscoveryPath)
							&& Existing.Contains(FString::Printf(TEXT("\"pid\":%u,"), FPlatformProcess::GetCurrentProcessId())))
						{
							IFileManager::Get().Delete(*DiscoveryPath, false, true, true);
						}
						GLog->Flush();
						FPlatformMisc::RequestExitWithStatus(/*Force*/ true, 3);
						return;
					}
					FPlatformProcess::Sleep(0.25f);
				}
			});
		}

		void StopWatchdog()
		{
			GWatchdogStop = true;
			if (GWatchdog.IsValid())
			{
				GWatchdog.Wait();
				GWatchdog = TFuture<void>();
			}
		}

		void StartNow()
		{
			FEndpointOptions Options;
			Options.Mode = GMode;
			int32 Port = Options.Port;
			if (FParse::Value(FCommandLine::Get(), TEXT("-DreamMCPPort="), Port))
			{
				Options.Port = Port;
			}
			// The environment first: a server that starts the host passes the token there, because a command line is
			// visible to every process on the machine.
			const FString EnvironmentToken = FPlatformMisc::GetEnvironmentVariable(TEXT("DREAM_MCP_TOKEN")).TrimStartAndEnd();
			FString Token;
			if (!EnvironmentToken.IsEmpty())
			{
				Options.Token = EnvironmentToken;
			}
			else if (FParse::Value(FCommandLine::Get(), TEXT("-DreamMCPToken="), Token))
			{
				Options.Token = Token;
			}
			// A commandlet never counts as rendering, -AllowCommandletRendering or not: the preview scene draws nothing in
			// one (its primitives get no scene proxy, the engine's own thumbnails included). The windowless editor is the
			// host that renders.
			Options.bCanRender = FApp::CanEverRender() && !GUsingNullRHI && !IsRunningCommandlet();

			GEndpoint = MakeUnique<FEndpoint>();
			RegisterDreamShaderMCPActions(*GEndpoint);
			FString Error;
			if (!GEndpoint->Start(Options, Error))
			{
				GStartupError = Error;
				GEndpoint.Reset();
				UE_LOG(LogDreamShader, Error, TEXT("DreamMCP: the %s endpoint did not start: %s."), LexEndpointMode(GMode), *Error);
				if (GMode == EEndpointMode::Host)
				{
					// A host with no endpoint is of no use to anyone, and a server waits for its hello: say why and go.
					GHostStarted = true;
					RequestHostExit(TEXT("the endpoint did not start"));
				}
				return;
			}
			GStartupError.Reset();
			UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: %s endpoint listening on http://127.0.0.1:%d%s (render: %s); %s follows once it answers its own hello."),
				LexEndpointMode(GMode), GEndpoint->GetPort(), *GEndpoint->GetManifest().Route,
				Options.bCanRender ? TEXT("yes") : TEXT("no"), *GEndpoint->GetDiscoveryFilePath());

			if (GMode == EEndpointMode::Host)
			{
				{
					FScopeLock Lock(&GDiscoveryPathLock);
					GDiscoveryPath = GEndpoint->GetDiscoveryFilePath();
				}
				StartHostLifetime();
			}
		}

		/**
		 * A windowless editor started as a host is nobody's editor: it must not leave traces in the user's settings
		 * (recent projects, layouts, per-project preferences are written through the config system) and must not
		 * auto-save packages. Called from module startup, before the editor writes any of that during its own init.
		 */
		void QuietEditorHost()
		{
			if (IsRunningCommandlet())
			{
				return;
			}
			if (GConfig)
			{
				GConfig->DisableFileOperations();
			}
			// The editor records every project it opens in %LOCALAPPDATA%/UnrealEngine/Editor/ProjectEditorRecords.json
			// (5.6+), by direct file IO the config switch above does not cover. The engine lets the file be moved: this
			// host's record goes beside its discovery file instead. An engine without the variable has no such file.
			if (IConsoleVariable* Records = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.ProjectEditorRecordsFile")))
			{
				Records->Set(*FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamMCP"), TEXT("ProjectEditorRecords.json"))), ECVF_SetByCode);
			}
			if (UEditorLoadingSavingSettings* Saving = GetMutableDefault<UEditorLoadingSavingSettings>())
			{
				Saving->bAutoSaveEnable = false;
			}
		}
	}

	bool HasDreamMCPHostSwitch()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("DreamMCPHost"));
	}

	bool IsDreamMCPHostProcess()
	{
		if (HasDreamMCPHostSwitch())
		{
			return true;
		}
		if (!IsRunningCommandlet())
		{
			return false;
		}
		const FString Name = GetRunCommandletName();
		return Name.StartsWith(TEXT("Dream"), ESearchCase::IgnoreCase) && Name.EndsWith(TEXT("Host"), ESearchCase::IgnoreCase);
	}

	void StartupDreamShaderMCP(const bool bEditorBridge)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("NoDreamMCP")))
		{
			UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: the endpoint is off (-NoDreamMCP)."));
			return;
		}

		if (IsDreamMCPHostProcess())
		{
			GMode = EEndpointMode::Host;
			QuietEditorHost();
			StartWatchdog();
		}
		else if (IsRunningCommandlet())
		{
			return;
		}
		else
		{
			if (!bEditorBridge)
			{
				return;
			}
			const UDreamShaderSettings* const Settings = GetDefault<UDreamShaderSettings>();
			if (Settings && !Settings->bEnableDreamMCPEndpoint)
			{
				UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: the endpoint is off (Project Settings > DreamShader > Enable DreamMCP Endpoint)."));
				return;
			}
			GMode = EEndpointMode::Editor;
		}

		// After the engine has initialized: the compiler reaches editor subsystems that are not there while startup
		// modules load. A host gets there before its commandlet's Main or its first editor frame.
		if (GEngine && GEngine->IsInitialized())
		{
			StartNow();
			return;
		}
		GPostEngineInitHandle = DREAMSHADER_POST_ENGINE_INIT_DELEGATE().AddLambda([]()
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(GPostEngineInitHandle);
			GPostEngineInitHandle.Reset();
			StartNow();
		});
	}

	void ShutdownDreamShaderMCP()
	{
		StopWatchdog();
		if (GPostEngineInitHandle.IsValid())
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(GPostEngineInitHandle);
			GPostEngineInitHandle.Reset();
		}
		StopHostLifetime();
		if (GEndpoint.IsValid())
		{
			GEndpoint->Stop();
			GEndpoint.Reset();
		}
		ResetDreamShaderMCPActionState();
	}

	FEndpoint* GetDreamShaderMCPEndpoint()
	{
		return GEndpoint.Get();
	}

	const FString& GetDreamShaderMCPStartupError()
	{
		return GStartupError;
	}
}

