// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderHostCommandlet.h"

#include "MCP/DreamShaderMCPEndpoint.h"
#include "MCP/DreamShaderMCPService.h"

#include "DreamShaderModule.h"

#include "AssetCompilingManager.h"
#include "Commandlets/Commandlet.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

UDreamShaderHostCommandlet::UDreamShaderHostCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UDreamShaderHostCommandlet::Main(const FString& Params)
{
	using namespace UE::DreamShader::Editor::MCP;

	FEndpoint* const Endpoint = GetDreamShaderMCPEndpoint();
	if (!Endpoint)
	{
		const FString& Why = GetDreamShaderMCPStartupError();
		UE_LOG(LogDreamShader, Error, TEXT("DreamShaderHost: no DreamMCP endpoint is running%s. RESULT=FAILED"),
			Why.IsEmpty() ? TEXT(" (-NoDreamMCP?)") : *FString::Printf(TEXT(": %s"), *Why));
		return 1;
	}

	UE_LOG(LogDreamShader, Display, TEXT("DreamShaderHost: ready on http://127.0.0.1:%d%s (render: %s). POST %s/shutdown to stop."),
		Endpoint->GetPort(), *Endpoint->GetManifest().Route, Endpoint->CanRender() ? TEXT("yes") : TEXT("no"), *Endpoint->GetManifest().Route);
	GLog->Flush();

	double LastTime = FPlatformTime::Seconds();
	double IdleSince = 0.0;
	bool bWorkSinceCollect = false;

	// Until the engine is asked to exit: POST /shutdown, the parent process gone (both watched by the endpoint
	// service, MCP/DreamShaderMCPService.cpp, the same way in the windowless editor host), or Ctrl+C.
	while (!IsEngineExitRequested())
	{
		const double Now = FPlatformTime::Seconds();
		const double DeltaTime = FMath::Clamp(Now - LastTime, 0.0, 1.0);
		LastTime = Now;
		FApp::SetCurrentTime(Now);

		// One engine frame, the engine's own way for a commandlet: the engine tick, the task graph, the core ticker
		// the HTTP listeners and the jobs run on, the deferred commands. This host does not render (the windowless
		// editor is the one that does), so there is no render-thread frame to wrap around it.
		CommandletHelpers::TickEngine(nullptr, DeltaTime);
		// Not part of the engine tick: FEngineLoop::Tick calls it itself, and nothing else would finish a shader map
		// or a mesh build in this process.
		FAssetCompilingManager::Get().ProcessAsyncTasks(true);
		GLog->FlushThreadedLogs();

		// Garbage left by the jobs -- the IR of a check, the materials a compile loaded -- is collected once the
		// endpoint has been idle for a moment, never in the middle of a job.
		if (Endpoint->IsBusy())
		{
			bWorkSinceCollect = true;
			IdleSince = 0.0;
		}
		else if (bWorkSinceCollect)
		{
			if (IdleSince == 0.0)
			{
				IdleSince = Now;
			}
			else if (Now - IdleSince > 2.0)
			{
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				bWorkSinceCollect = false;
				IdleSince = 0.0;
			}
		}

		// Idle, about a hundred frames a second: near zero CPU, and a request waits a few milliseconds at most. Busy,
		// no sleep at all.
		FPlatformProcess::Sleep(Endpoint->IsBusy() ? 0.0f : 0.008f);
	}

	UE_LOG(LogDreamShader, Display, TEXT("DreamShaderHost: stopping."));
	ShutdownDreamShaderMCP();
	return 0;
}
