// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The 2.0 compile pipeline for a `.dss` source: preprocess -> parse -> bind -> lower -> validate ->
// emit. This is the seam the 1.x generator dispatches through: a `.dss` path arriving at
// FMaterialGenerator is handed here and never reaches the 1.x code; a `.dsm` / `.dsf` never
// reaches this. The two pipelines coexist until M4 deletes the 1.x generator, and this header is
// the whole of what the 1.x side knows about the new one.
//
// FROZEN for batch 1 (M2+M3): the two functions below are called from DreamShaderMaterialGenerator.cpp.

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader::Editor::Compiler
{
	/** `.dss` -- the 2.0 language. `.dsh` headers are never compiled on their own, so they answer false too. */
	bool IsDreamShaderLang2Source(const FString& SourceFilePath);

	/**
	 * Compiles one `.dss` into its assets: one material, or one asset per exported function. Every
	 * asset is saved to disk, a ThinCustom instance included (plan §5: there is no transient request
	 * in 2.0). The 1.x entry points take a persistence request and the dispatch does not forward it, so
	 * a `.dss` ThinCustom product has no Ephemeral state yet; that arrives with the 1.x generator
	 * retirement in M4. bForce rebuilds even when the source hash says the assets are current. On failure OutError carries the first error in the 1.x wire form and the
	 * diagnostics store has all of them.
	 */
	bool CompileDreamShaderLang2File(const FString& SourceFilePath, bool bForce, UE::DreamShader::FDreamShaderError& OutError);
}
