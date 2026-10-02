#pragma once

#include "CoreMinimal.h"

namespace UE::DreamPass
{
	/** r.DreamPass.Enable: 0 turns every pipeline off for the session, whatever activates it. Any thread. */
	bool IsEnabledByConsole();

	/** r.DreamPass.DisablePipelines: comma-separated pipeline asset names that do not run. Game thread. */
	bool IsPipelineDisabledByConsole(const FString& PipelineName);

	/** r.DreamPass.Visualize: `<Pipeline>.<Buffer>` to draw in the corner of the view, empty for none. Render thread. */
	FString GetVisualizeTarget_RenderThread();
}
