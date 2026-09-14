// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `dsc check --shaders` -- the headless shader-compilation gate (plan §13.3).
//
// WHAT IT COSTS, first, because it surprises people: a shader compile needs a real material,
// and 2.0 has no transient asset to build one into (plan section 5 -- every product saves).
// So `check -Shaders` BUILDS AND SAVES the products the way `compile` does, and only plain
// `check` is the write-nothing gate. Holding the generator's write guard instead was tried and
// is wrong: it makes the emitter skip each product rather than build it off disk, so the run
// would compile whatever was already on disk and report a clean gate on a source it never saw.
//
// THE GAP IT CLOSES. A commandlet compile stops at the graph. Everything a `@custom` body gets
// wrong -- an undeclared symbol, an implicit mip in a divergent branch, a type that only HLSL
// rejects -- surfaces later, in an editor, on somebody else's machine. CI is green and the material
// is broken. So after the graph is built, this pushes it through the same shader compilation an
// editor would, and reports what comes back as ordinary DreamShader diagnostics with `stage:
// shader`.
//
// HOW THE ERRORS ARE OBTAINED, and why it is two mechanisms rather than one.
//
// The direct read is FMaterialResource::GetCompileErrors(), reached through
// UMaterial::GetMaterialResource(ShaderPlatform, QualityLevel) -- the same route the bridge's
// OnMaterialCompilationFinished handler already takes. It works whenever the RENDERING resources
// exist.
//
// They do not exist under `-nullrhi`. UMaterial::CacheResourceShadersForRendering is gated on
// FApp::CanEverRender(), which is false when `-nullrhi` is on the command line (Misc/App.h) -- so in
// exactly the configuration CI runs in, the direct read finds nothing to read. That is the question
// plan §13.3 says M3 must answer, and this is the answer: the rendering path is dead under
// `-nullrhi`; the COOKING path is not, because cooking is what commandlets do.
//
// So the cook path (BeginCacheForCookedPlatformData / IsCachedCookedPlatformDataLoaded, pumped with
// FShaderCompilingManager::ProcessAsyncResults) is what actually drives the compilers, and its
// resources are private to UMaterial -- there is no public accessor for CachedMaterialResourcesForCooking.
// Its errors reach the log instead, in full, through MaterialImpl::HandleCacheShadersForResourcesErrors.
// This file therefore also installs an FOutputDevice for the duration and keeps the LogMaterial /
// LogShaderCompilers lines. Belt and braces on purpose: either half alone has a configuration in
// which it reports nothing at all, and "no errors" is the one answer a gate must never get wrong.
//
// MAPPING BACK TO SOURCE, best first:
//   1. The asset's `DreamShader.SourceSpans` table (CONTRACT §11 #10): the error names a
//      UMaterialExpression, the table maps its guid to a file/line/column/length. Exact.
//   2. The `// Begin/End DreamShader source:` markers unit H emits inside a Custom node's code:
//      count lines within the block, as the 1.x diagnostics mapper does.
//   3. The source file at line 1, with the raw compiler text in `detail`. Honest, not helpful.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerTools.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Editor::Compiler
{
	struct FDreamShaderShaderCheckOptions
	{
		/**
		 * Platform tokens as the user wrote them: `SM6`, `SM5`, `ES3_1`, a raw shader-format name
		 * (`PCD3D_SM6`), or a target-platform name (`Windows`). Empty means the active target
		 * platforms, and failing that the host's own shader platform.
		 */
		TArray<FString> PlatformTokens;

		/** `Low` / `Medium` / `High` / `Epic`. Empty means the project's current scalability level. */
		TArray<FString> QualityTokens;

		/**
		 * Seconds to wait for one material. A timeout is an ERROR, not a hang: #29's dynamic-loop
		 * stall is precisely a compile that never finishes, and a CI job that blocks forever on it
		 * tells nobody anything.
		 */
		double TimeoutSeconds = 120.0;
	};

	struct FDreamShaderShaderCheckStats
	{
		int32 MaterialsChecked = 0;
		int32 PlatformsRequested = 0;
		int32 ShaderErrorsReported = 0;
		bool bTimedOut = false;
		/** True when nothing could read a compile error -- `-nullrhi` with no resolvable target platform. */
		bool bErrorsUnreadable = false;
	};

	/**
	 * Parses the three command-line values. Raises DSH9026 / DSH9027 / DSH9028's sibling and returns
	 * false on an unknown token; an empty value is not an error, it selects the default.
	 */
	bool ParseDreamShaderShaderCheckOptions(
		const FString& PlatformValue,
		const FString& QualityValue,
		const FString& TimeoutValue,
		FDreamShaderShaderCheckOptions& OutOptions,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Compiles the shaders of every UMaterial the run produced and reports the result.
	 *
	 * Compiled must come from a pipeline run with bEmitAssets true. Returns false when any shader
	 * error was reported or the wait timed out; the diagnostics say which.
	 *
	 * Game thread only, and it pumps the shader compiler, so it must not be called from inside
	 * another pump.
	 */
	bool CheckDreamShaderShaders(
		const FDreamShaderLang2PipelineResult& Compiled,
		const FDreamShaderShaderCheckOptions& Options,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics,
		FDreamShaderShaderCheckStats& OutStats);
}
