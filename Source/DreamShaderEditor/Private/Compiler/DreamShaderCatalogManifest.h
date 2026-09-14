// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builtin catalog: one per process, and its manifest on disk.
//
// FBuiltinCatalog is how engine knowledge reaches a front end that may not include Engine.h
// (IRCatalog.h). Building it walks every UMaterialExpression class by reflection, which is far too
// expensive to do per compile and completely stable in between, so it is built once and cached --
// with a revision that anything loading or unloading a module bumps, because a plugin that mounts
// after the first compile brings expression classes with it and a catalog that predates it would
// report them as unknown names.
//
// The manifest is the SAME data as JSON, written beside the other bridge manifests
// (`<Project>/Saved/DreamShader/Bridge/`). It exists so the language service and the tools can bind
// `UE.*` without an editor: LoadBuiltinCatalogFromJson on that file must produce what reflection
// produced here, and the round trip through SaveBuiltinCatalogToJson is what lets that be tested.

#pragma once

#include "CoreMinimal.h"

#include "IR/IRCatalog.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * The process-wide catalog, built from reflection on first use.
	 *
	 * Rebuilt when the module revision has moved since the cached copy was made. Game thread only:
	 * it iterates UClasses.
	 */
	const UE::DreamShader::IR::FBuiltinCatalog& GetDreamShaderBuiltinCatalog();

	/** Forces the next GetDreamShaderBuiltinCatalog to rebuild. `export-catalog` calls it, so the manifest is never a stale copy of a stale cache. */
	void InvalidateDreamShaderBuiltinCatalog();

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
