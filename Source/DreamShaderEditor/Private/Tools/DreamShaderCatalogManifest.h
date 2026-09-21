// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builtin catalog's manifest on disk.
//
// The manifest is the SAME data as the process-wide catalog (DreamShaderBuiltinCatalog.h, in the
// compiler module) as JSON, written beside the other bridge manifests
// (`<Project>/Saved/DreamShader/Bridge/`). It exists so the language service and the tools can bind
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
	 * `<Project>/Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json` -- the directory the
	 * other manifests live in, taken from FDreamShaderWorkspaceService::GetMaterialExpressionManifestFilePath
	 * rather than spelled again, so moving that directory moves this file with it.
	 */
	FString GetDreamShaderBuiltinCatalogManifestFilePath();

	/**
	 * Writes the catalog manifest, creating its directory.
	 *
	 * @param OutputFilePath  Empty for the default path above.
	 * @param OutWrittenPath  The path actually written.
	 * @param OutError        Plain text on failure; the caller owns the DSHnnnn code.
	 */
	bool ExportDreamShaderBuiltinCatalogManifest(
		const FString& OutputFilePath,
		FString& OutWrittenPath,
		FString& OutError);
}
