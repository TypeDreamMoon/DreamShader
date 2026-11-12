// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `-run=DreamShaderHost`: the host a DreamMCP server starts when it needs no renderer (`-nullrhi -DreamMCPHost`).
// When it needs one it starts the windowless editor instead (`-DreamMCPHost -RenderOffscreen`, no -run=): a commandlet
// does not render a preview scene, so this host always reports `render: false`.
//
// It does nothing of its own but keep the game thread turning. The endpoints -- DreamShader's, and every other Dream
// plugin's in the process -- start from their modules, because the process is a host (MCP/DreamShaderMCPService.h);
// this loop ticks the engine, the task graph and the core ticker their HTTP listeners and jobs run on, and the asset
// compilers. It returns when the engine is asked to exit: POST <route>/shutdown, the process that started it gone
// (both watched by the endpoint service), or Ctrl+C.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "DreamShaderHostCommandlet.generated.h"

UCLASS()
class UDreamShaderHostCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDreamShaderHostCommandlet();

	virtual int32 Main(const FString& Params) override;
};
