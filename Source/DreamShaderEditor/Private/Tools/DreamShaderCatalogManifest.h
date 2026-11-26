// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builtin catalog's manifest on disk.
//
// The manifest is the SAME data as the process-wide catalog (DreamShaderBuiltinCatalog.h, in the
// compiler module) as JSON, written beside the other bridge manifests
// (the `builtin-catalog` document of `<Project>/Intermediate/DreamSource/shader.db`). It exists so the language service and the tools can bind
// `UE.*` without an editor: LoadBuiltinCatalogFromJson on that file must produce what reflection
// produced here, and the round trip through SaveBuiltinCatalogToJson is what lets that be tested.
//
// Until the compiler relocation this header also declared the cache itself. The cache moved down into the
// compiler module; the manifest stays here because its directory comes from
// FDreamShaderWorkspaceService, which is editor-module code. The cache header is included below, so
// every caller of the export still sees GetDreamShaderBuiltinCatalog and InvalidateDreamShaderBuiltinCatalog.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderBuiltinCatalog.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Writes the catalog manifest.
	 *
	 * @param OutputFilePath  Empty for the `builtin-catalog` document of the bridge's database
	 *                        (FDreamShaderWorkspaceService::GetBridgeDatabaseFilePath); a path writes that file,
	 *                        creating its directory.
	 * @param OutWrittenPath  The file actually written (the database, for an empty OutputFilePath).
	 * @param OutError        Plain text on failure; the caller owns the DSHnnnn code.
	 */
	bool ExportDreamShaderBuiltinCatalogManifest(
		const FString& OutputFilePath,
		FString& OutWrittenPath,
		FString& OutError);
}
