#include "DreamPassConsole.h"

#include "DreamPassPipeline.h"
#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"

namespace UE::DreamPass
{
	static TAutoConsoleVariable<int32> CVarEnable(
		TEXT("r.DreamPass.Enable"),
		1,
		TEXT("0: no DreamShader Custom Pass pipeline runs, whatever activates it. 1: pipelines run (default)."),
		ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<FString> CVarDisablePipelines(
		TEXT("r.DreamPass.DisablePipelines"),
		TEXT(""),
		TEXT("Comma-separated DreamShader Custom Pass pipeline asset names that do not run, e.g. CP_Highlight,CP_XRay."),
		ECVF_Default);

	// A string variable cannot be ECVF_RenderThreadSafe (FConsoleManager::RegisterConsoleVariable asserts it): it is read
	// on the game thread, into each family's snapshot, which is how the render thread's visualize pass sees it.
	static TAutoConsoleVariable<FString> CVarVisualize(
		TEXT("r.DreamPass.Visualize"),
		TEXT(""),
		TEXT("<Pipeline>.<Buffer>: draws that buffer of that pipeline in the lower left corner of every view that runs it, e.g. CP_Highlight.Blurred. Empty: off."),
		ECVF_Default);

	bool IsEnabledByConsole()
	{
		return CVarEnable.GetValueOnAnyThread() != 0;
	}

	bool IsPipelineDisabledByConsole(const FString& PipelineName)
	{
		const FString List = CVarDisablePipelines.GetValueOnGameThread();
		if (List.IsEmpty())
		{
			return false;
		}
		TArray<FString> Names;
		List.ParseIntoArray(Names, TEXT(","), true);
		for (FString& Name : Names)
		{
			if (Name.TrimStartAndEnd().Equals(PipelineName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FString GetVisualizeTarget()
	{
		return CVarVisualize.GetValueOnGameThread();
	}

	static void DumpWorlds(const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar)
	{
		if (!GEngine)
		{
			return;
		}

		int32 Dumped = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(World);
			if (!Subsystem)
			{
				continue;
			}
			Subsystem->DumpState(Ar);
			++Dumped;
		}

		if (Dumped == 0)
		{
			Ar.Logf(TEXT("DreamPass: no world has a Custom Pass subsystem."));
		}
	}

	static FAutoConsoleCommand CommandDump(
		TEXT("DreamPass.Dump"),
		TEXT("Lists, for every world, what activates DreamShader Custom Pass pipelines and what the last frame ran or skipped."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpWorlds));
}
