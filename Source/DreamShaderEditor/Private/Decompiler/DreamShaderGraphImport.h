// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The reverse direction, editor half: a material or material function graph read into a DreamShader IR product.
//
// This is the emitter inverted (Compiler: Emitter/DreamShaderIREmitter*.cpp), node class by node class: what the
// emitter writes for an IR op is what this reads back as that op, and everything else is a Reflected node over the
// builtin catalog. The product it makes is not yet the one a build would make -- the emitter's hand-lowered shapes are
// still spelled out, and nodes nothing reads are still there -- so the caller runs the passes (fold and dedupe off,
// prune on), the validator and RaiseDreamShaderIR over it before BuildDreamShaderAstFromIR. DreamShaderIRDecompiler.cpp
// is that caller.
//
// Design: Plan/m4m5/research-decompiler.md section 3. Diagnostics: DSH9060-9074.

#pragma once

#include "CoreMinimal.h"
#include "Decompile/IRToAst.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"

class UObject;
class UMaterialFunctionInterface;

namespace UE::DreamShader::IR
{
	struct FBuiltinCatalog;
}

namespace UE::DreamShader::Editor::Private
{
	struct FGraphImportOptions
	{
		/** Node positions and free comment boxes as `#pragma layout` hints (bExportDecompiledLayout). Regions are read either way. */
		bool bImportLayout = true;
		/** Null: the process catalog (GetDreamShaderBuiltinCatalog). A test hands over its own. */
		const UE::DreamShader::IR::FBuiltinCatalog* Catalog = nullptr;
	};

	/** What an import learns beyond the product itself. Shared by every product of one module. */
	struct FGraphImportContext
	{
		/** The interface of every function asset a graph called, once per asset: FIRToAstOptions::ExternInterfaces. */
		TArray<UE::DreamShader::Lang::FIRToAstExternInterface> ExternInterfaces;
		/** Normalized asset path -> index into ExternInterfaces. */
		TMap<FString, int32> InterfaceByPath;
		/** Function assets whose interface is being read right now, outermost first: a function that calls itself. */
		TArray<FString> InterfacesInProgress;
	};

	/**
	 * Appends one product to InOutModule: a UMaterial, or a UMaterialFunction of any usage (function, layer, blend).
	 * A ThinCustom pair is imported through its hidden base, which the caller resolves. False with an error when the
	 * asset is of no kind this reads; a graph that reads only in part still answers true, with warnings.
	 */
	bool ImportDreamShaderGraphToIR(
		UObject* Asset,
		const FGraphImportOptions& Options,
		UE::DreamShader::IR::FIRModule& InOutModule,
		FGraphImportContext& InOutContext,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The interface of a function asset -- pins in the asset's own order, typed -- for an `extern` prototype. Output
	 * types come from reading the function's graph, so this is as expensive as an import; InOutContext caches it.
	 * Null when the asset has no graph to read.
	 */
	const UE::DreamShader::Lang::FIRToAstExternInterface* ImportDreamShaderFunctionInterface(
		UMaterialFunctionInterface* Function,
		const FGraphImportOptions& Options,
		FGraphImportContext& InOutContext,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);
}
