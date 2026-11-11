// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Which DreamMCP endpoint this process runs, if any, and its lifetime.
//
//   * A host -- any process with -DreamMCPHost -- runs a `host` endpoint. A server starts one of two: the windowless
//     editor (`-DreamMCPHost -RenderOffscreen`, no -run=) when it needs a renderer, the plugin's host commandlet
//     (`-run=DreamShaderHost -nullrhi -DreamMCPHost`) when it does not. Every Dream plugin in the process honours the
//     switch, so each serves its own route from whichever host the server started. A commandlet named Dream...Host
//     counts as a host without the switch too.
//   * An editor -- not a commandlet, the editor bridge started, the project setting on, no -NoDreamMCP -- runs an
//     `editor` endpoint.
//   * Every other process -- `-run=DreamShader compile`, a cook, an automation run -- runs none.
//
// The endpoint starts after the engine has initialized (the compiler needs the editor's subsystems) and stops
// with the module, deleting its discovery file. A host also accepts POST /shutdown, exits when the process that
// started it goes away, exits with code 3 when it cannot start answering within -DreamMCPStartupTimeout seconds,
// and -- as a windowless editor -- writes no config, does not auto-save, renders no realtime viewport and caps its
// frame rate while idle.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::MCP
{
	class FEndpoint;

	/** `-DreamMCPHost` is on the command line. */
	bool HasDreamMCPHostSwitch();

	/** A process with -DreamMCPHost, or a commandlet run as `-run=Dream<...>Host` (or `...HostCommandlet`). */
	bool IsDreamMCPHostProcess();

	/** From the editor module's StartupModule. bEditorBridge: the bridge started in this process. */
	void StartupDreamShaderMCP(bool bEditorBridge);

	/** From the editor module's ShutdownModule, and from the host commandlet before it returns. */
	void ShutdownDreamShaderMCP();

	/** The running endpoint, or null. */
	FEndpoint* GetDreamShaderMCPEndpoint();

	/** Why there is no endpoint, when one was expected; empty otherwise. */
	const FString& GetDreamShaderMCPStartupError();
}
