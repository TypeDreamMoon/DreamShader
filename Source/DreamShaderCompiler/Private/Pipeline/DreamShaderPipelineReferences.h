// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The two pipeline stages that cross between a `.dsp` and the `.dss` sources around it. Both run inside
// RunDreamShaderPipelineStages (DreamShaderCompilePipeline.cpp), the way the `.dsi` parent stage does.
//
//   * A `.dsp`, between the parse and the bind: every `Material = "..."` resolved -- a bare name is the product of a
//     `.dss` under the same source root (the product index), anything else an object path -- and read for the facts
//     the binder checks a pass against (domain, blendable location, UserSceneTexture inputs, the UE.DreamPassOutput
//     pins, usage flags); every `Shader = "..."` resolved to a file and a virtual path and scanned for its functions
//     and [numthreads] entries; the project's pass layers. In an emitting run a referenced material that is missing or
//     older than its source is compiled first (the `.dsi` DSH8264 rule). All of it is FPipelineReferences, which the
//     bind receives, plus the text the build key folds in.
//   * A `.dss`, after the lower: every `UE.DreamPassBuffer(Pipeline = ..., Buffer = ...)` resolved the same way -- a
//     bare name is the product of a `.dsp` under the same source root -- its buffer checked to exist and be exported,
//     and the Pipeline property rewritten to the object path, which is what the reflected-property writer loads. The
//     export facts it read join the `.dss` build key, so a `.dsp` that changes them rebuilds the material.
//
// Cross references between sources can form a cycle (a mesh pass whose override material reads the pipeline's own
// exported buffer). A source already being resolved further up is never compiled or front-half-run again from below:
// its asset is read as it stands, with a warning.

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * The `.dsp` reference stage. Fills OutReferences for the bind and OutBuildKeyText for the build key; every failure is a
	 * diagnostic. A reference that resolves to nothing is left with bFound / bExists false for the binder to word; only
	 * what the binder cannot see is raised here (a bare name several products share, a material that fails to compile
	 * first, a malformed path).
	 */
	void ResolveDreamShaderPipelineReferencesForPipeline(
		const Lang::FModule& Module,
		const Lang::FLangSourceText& Source,
		const FString& SourceFilePath,
		bool bCompileStaleMaterials,
		Lang::FPipelineReferences& OutReferences,
		FString& OutBuildKeyText,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Fills each pass's MaterialObjectPath, ShaderVirtualPath and ShaderFilePath from the references where the binder left
	 * them empty: the emitter reads the payload alone.
	 */
	void FillDreamShaderPipelinePayloadReferences(IR::FIRModule& Module, const Lang::FPipelineReferences& References);

	/** The `Pipeline = "..."` of every UE.DreamPassBuffer in a module, as written: the product index's `.dss` -> `.dsp` edge. */
	void CollectDreamShaderPassBufferPipelineReferences(const IR::FIRModule& Module, const IR::FBuiltinCatalog& Catalog, TArray<FString>& OutReferences);

	/**
	 * The `.dss` pass-buffer stage (DSH5315-DSH5329). Rewrites each UE.DreamPassBuffer's Pipeline property to the object
	 * path it resolves to and appends the export facts it checked to OutBuildKeyText. Does nothing while the product index
	 * is refreshing: indexing a `.dss` runs its front half, and a lookup from there would read a half-built index.
	 */
	void ResolveDreamShaderPassBufferReads(
		IR::FIRModule& Module,
		const IR::FBuiltinCatalog& Catalog,
		const Lang::FLangSourceText& Source,
		const FString& SourceFilePath,
		bool bCompileStalePipelines,
		FString& OutBuildKeyText,
		Lang::FLangDiagnosticSink& Diagnostics);
}
