#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamShaderPassModule.h"

#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphEvent.h"
#include "Stats/Stats.h"

DECLARE_GPU_STAT_NAMED(DreamPass, TEXT("DreamPass"));

DECLARE_STATS_GROUP(TEXT("DreamPass"), STATGROUP_DreamPass, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Passes run"), STAT_DreamPass_PassesRun, STATGROUP_DreamPass);
DECLARE_DWORD_COUNTER_STAT(TEXT("Passes that could not run"), STAT_DreamPass_PassesFailed, STATGROUP_DreamPass);
DECLARE_CYCLE_STAT(TEXT("Graph setup"), STAT_DreamPass_Setup, STATGROUP_DreamPass);

namespace UE::DreamPass
{
	FFamilyState* FindFamilyState(FRDGBuilder& GraphBuilder)
	{
		return GraphBuilder.Blackboard.GetMutable<FFamilyState>();
	}

	void WarnOnce(const FString& Key, const FString& Message)
	{
		static FCriticalSection Lock;
		static TSet<FString> Seen;
		{
			FScopeLock ScopeLock(&Lock);
			bool bAlreadySeen = false;
			Seen.Add(Key, &bAlreadySeen);
			if (bAlreadySeen)
			{
				return;
			}
		}
		UE_LOG(LogDreamPass, Warning, TEXT("%s"), *Message);
	}

	static bool ExecutePass(FExecuteContext& Context)
	{
		switch (Context.Pass.Kind)
		{
		case EDreamPassKind::Fullscreen:
			return Context.Pass.Material ? ExecuteFullscreenMaterialPass(Context) : ExecuteFullscreenSlotPass(Context);
		case EDreamPassKind::Compute:
			return ExecuteComputePass(Context);
		case EDreamPassKind::Mesh:
			return ExecuteMeshPass(Context);
		case EDreamPassKind::Clear:
			return ExecuteClearPass(Context);
		case EDreamPassKind::Copy:
			return ExecuteCopyPass(Context);
		}
		return false;
	}

	void RunInjection(FRDGBuilder& GraphBuilder, FFamilyState& Family, int32 ViewIndex, FInjectionContext& Context)
	{
		if (!Family.Views.IsValidIndex(ViewIndex))
		{
			return;
		}

		FViewState& State = Family.Views[ViewIndex];
		if (!State.IsActive() || State.bFinished || !State.Snapshot->HasPassesAt(Context.Injection))
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_DreamPass_Setup);
		RDG_EVENT_SCOPE_STAT(GraphBuilder, DreamPass, "DreamPass %s", LexToString(Context.Injection));

		const TPair<int32, int32> Range = State.Snapshot->RangeByInjection[int32(Context.Injection)];
		for (int32 OrderIndex = Range.Key; OrderIndex < Range.Value; ++OrderIndex)
		{
			const FScheduledPass& Scheduled = State.Snapshot->Order[OrderIndex];
			const FSnapshotPipeline& Pipeline = State.Snapshot->Pipelines[Scheduled.Pipeline];
			const FSnapshotPass& Pass = Pipeline.Passes[Scheduled.Pass];

			// Event names carry the pipeline and the pass, so a GPU capture or Insights finds them by the names the
			// `.dsp` gave them.
			RDG_EVENT_SCOPE(GraphBuilder, "%s.%s", *Pipeline.DebugName, *Pass.Name.ToString());

			FExecuteContext Execute{ GraphBuilder, Family, State, Context, Pipeline, Scheduled.Pipeline, Pass, OrderIndex };
			if (ExecutePass(Execute))
			{
				AfterPassWrites(Execute);
				State.Executed[OrderIndex] = true;
				INC_DWORD_STAT(STAT_DreamPass_PassesRun);
			}
			else
			{
				INC_DWORD_STAT(STAT_DreamPass_PassesFailed);
			}
		}
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
