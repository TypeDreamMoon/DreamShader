// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader's DreamMCP actions: check, compile, diagnostics, lookup, preview, info, decompile.
//
// Each one forwards to what the plugin already does -- the compile pipeline, the compiler service, the bridge's
// compile, the preview renderer, the builtin catalog, the decompile service -- and turns the answer into the
// result body. None of them compiles, renders or decompiles anything by a route of its own.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::MCP
{
	class FEndpoint;

	/** Registers every action the manifest names. */
	void RegisterDreamShaderMCPActions(FEndpoint& Endpoint);

	/** Forgets what the actions remember between calls (the diagnostics of the last check of each file). */
	void ResetDreamShaderMCPActionState();
}
