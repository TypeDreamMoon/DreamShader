// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPBusyEndpoints.h"

#include "HAL/IConsoleManager.h"

namespace UE::DreamShader::Editor::MCP
{
	const TCHAR* const DreamMCPBusyEndpointsName = TEXT("DreamMCP.BusyEndpoints");

	IConsoleVariable* FindOrRegisterBusyEndpoints()
	{
		IConsoleManager& Console = IConsoleManager::Get();
		if (IConsoleVariable* Existing = Console.FindConsoleVariable(DreamMCPBusyEndpointsName))
		{
			return Existing;
		}
		// Registered with the console manager, which keeps it when this module unloads: the other plugins may still
		// count on it.
		return Console.RegisterConsoleVariable(DreamMCPBusyEndpointsName, 0,
			TEXT("How many DreamMCP endpoints in this process are running a job. Each endpoint adds one when it turns busy and ") // I18N-EXEMPT: console help
			TEXT("takes it back when it turns idle; a DreamMCP host caps its frame rate only while this is 0."), // I18N-EXEMPT: console help
			ECVF_Default);
	}

	int32 GetBusyEndpoints()
	{
		const IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(DreamMCPBusyEndpointsName);
		return Variable ? Variable->GetInt() : 0;
	}

	int32 GetHostFrameCap(const int32 BusyEndpoints)
	{
		return BusyEndpoints > 0 ? 0 : 15;
	}

	int32 ApplyHostFrameCap()
	{
		const int32 Cap = GetHostFrameCap(GetBusyEndpoints());
		if (IConsoleVariable* const MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
		{
			// Only on a change: it is asked every frame, and another plugin's host housekeeping asks it too.
			if (!FMath::IsNearlyEqual(MaxFps->GetFloat(), static_cast<float>(Cap)))
			{
				MaxFps->Set(static_cast<float>(Cap), ECVF_SetByCode);
			}
		}
		return Cap;
	}

	void FBusyEndpointShare::Set(const bool bBusy)
	{
		if (bBusy == bHeld)
		{
			return;
		}
		IConsoleVariable* const Variable = FindOrRegisterBusyEndpoints();
		if (!Variable)
		{
			return;
		}
		const int32 Now = Variable->GetInt();
		// Never below 0: a plugin that took back more than it added would otherwise hide another's running job.
		Variable->Set(bBusy ? Now + 1 : FMath::Max(0, Now - 1), ECVF_SetByCode);
		bHeld = bBusy;
	}
}
