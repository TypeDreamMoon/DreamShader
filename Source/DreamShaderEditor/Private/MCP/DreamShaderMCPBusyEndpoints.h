// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

// DreamMCP.BusyEndpoints, the protocol's counter of how many DreamMCP endpoints in this process are running a job.
//
// One engine process can serve several Dream plugins, each with an endpoint of its own. A host caps its frame rate
// while idle and lifts the cap while a job runs, because a job steps once a frame; a plugin that looked only at its
// own endpoint would cap the frame rate under another plugin's running job as soon as its own went idle. So every
// endpoint adds one to this integer console variable when it turns busy and takes it back when it turns idle or
// stops, and a host lifts the cap while the variable is above 0. Whichever plugin starts first registers the variable;
// the others find it and use the same one.

#pragma once

#include "CoreMinimal.h"

class IConsoleVariable;

namespace UE::DreamShader::Editor::MCP
{
	/** The variable's name, the same in every Dream plugin. */
	extern const TCHAR* const DreamMCPBusyEndpointsName;

	/** The variable: found when another plugin registered it first, registered otherwise. Game thread. */
	IConsoleVariable* FindOrRegisterBusyEndpoints();

	/** Its value, 0 when nobody has registered it yet. */
	int32 GetBusyEndpoints();

	/** The frame cap a host wants: none (0) while any endpoint in the process is busy, 15 while all are idle. */
	int32 GetHostFrameCap(int32 BusyEndpoints);

	/** Sets t.MaxFPS to GetHostFrameCap(GetBusyEndpoints()) when it is not that already, and returns the cap. Game thread. */
	int32 ApplyHostFrameCap();

	/** One endpoint's part in the counter: never more than one. The destructor takes back what is held. */
	class FBusyEndpointShare
	{
	public:
		FBusyEndpointShare() = default;
		FBusyEndpointShare(const FBusyEndpointShare&) = delete;
		FBusyEndpointShare& operator=(const FBusyEndpointShare&) = delete;
		~FBusyEndpointShare() { Set(false); }

		/** true adds one when this endpoint holds none; false takes it back when it holds one. */
		void Set(bool bBusy);
		bool IsHeld() const { return bHeld; }

	private:
		bool bHeld = false;
	};
}
