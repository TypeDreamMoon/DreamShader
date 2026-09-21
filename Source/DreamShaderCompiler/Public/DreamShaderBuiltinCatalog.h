// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builtin catalog: one per process.
//
// FBuiltinCatalog is how engine knowledge reaches a front end that may not include Engine.h
// (IRCatalog.h). Building it walks every UMaterialExpression class by reflection, which is far too
// expensive to do per compile and completely stable in between, so it is built once and cached --
// with a revision that anything loading or unloading a module bumps, because a plugin that mounts
// after the first compile brings expression classes with it and a catalog that predates it would
// report them as unknown names.
//
// Split out of DreamShaderCatalogManifest.h in the compiler relocation. The cache is compiler state and
// lives here, in the compiler module, beside the reflection producer that fills it. The manifest --
// the same data as JSON, for the language service and the tools -- stays in the editor module
// (Tools/DreamShaderCatalogManifest.h), because its directory comes from the editor's workspace service.

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
	DREAMSHADERCOMPILER_API const UE::DreamShader::IR::FBuiltinCatalog& GetDreamShaderBuiltinCatalog();

	/** Forces the next GetDreamShaderBuiltinCatalog to rebuild. `export-catalog` calls it, so the manifest is never a stale copy of a stale cache. */
	DREAMSHADERCOMPILER_API void InvalidateDreamShaderBuiltinCatalog();

	/**
	 * Fills the builtin catalog from UE reflection: every non-abstract UMaterialExpression subclass,
	 * its FExpressionInput pins, its editable literal properties, its outputs, and the material
	 * attribute table.
	 *
	 * Deterministic: expressions are sorted by class name and everything inside an entry keeps the
	 * engine's own declaration order, because the catalog manifest exports this as JSON for the tools and the
	 * language service and a reordering would read as a diff on every export.
	 */
	DREAMSHADERCOMPILER_API void BuildBuiltinCatalogFromReflection(IR::FBuiltinCatalog& OutCatalog);
}
