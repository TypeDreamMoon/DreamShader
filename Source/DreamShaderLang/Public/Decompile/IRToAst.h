// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The reverse direction, Lang half: an FIRModule turned back into a DreamShaderLang AST that the
// printer writes as `.dss`.
//
// The editor imports an asset graph into IR (ImportDreamShaderGraphToIR), runs the passes with fold and
// dedupe off and prune on, and validates. The rest is engine-free and lives here: RaiseDreamShaderIR
// undoes the shapes the emitter lowers to, BuildDreamShaderAstFromIR recovers declarations, statements,
// names, regions and layout pragmas, and PrintDreamShaderLang writes the text. Widths come from IR
// typing. A decompiled FIRModule carries DebugName, Region and LayoutHints exactly as a build would;
// that is the one agreement with the importer beyond this header, with one addition. An importer does
// not know what a node will be called, so a Node hint's Var may be `$<n>`: the FIRStatementBinding of
// that Name says which node the hint places (bindings follow their nodes through the passes), and the
// builder writes the pragma under the variable that node's value ended up as. A hint for a value that
// was written inline is dropped (DSH9077).
//
// Design: Plan/m4m5/research-decompiler.md sections 3.9 and 4; CONTRACT section 2.4.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::IR
{
	struct FBuiltinCatalog;
}

namespace UE::DreamShader::Lang
{
	/** One pin of a function asset a decompiled graph calls. */
	struct FIRToAstExternPin
	{
		/** The engine's pin name, as the FunctionCall node's Inputs / OutputNames spell it. */
		FString Name;
		IR::FIRType Type = IR::FIRType::Float(1);
		/** Inputs: the pin may be left unconnected (bUsePreviewValueAsDefault). */
		bool bOptional = false;
		/** Inputs: Default holds the preview value the engine substitutes for an unconnected pin. */
		bool bHasDefault = false;
		double Default[4] = { 0.0, 0.0, 0.0, 0.0 };
		FString Description;
	};

	/**
	 * The whole interface of one called asset, in the asset's own pin order (SortPriority, then index). An FIRNode of a
	 * FunctionCall carries only the pins that call connected; the `extern` prototype needs all of them, so the importer
	 * hands them over here. A call whose asset has no entry gets a prototype inferred from the call itself (DSH9081).
	 */
	struct FIRToAstExternInterface
	{
		/** Matches the FunctionCall's Prop::FunctionPath: `/Game/Functions/MF_X.MF_X` or `/Game/Functions/MF_X`. */
		FString AssetPath;
		FString Description;
		TArray<FIRToAstExternPin> Inputs;
		TArray<FIRToAstExternPin> Outputs;
	};

	struct FIRToAstOptions
	{
		/** Write `#pragma layout(...)` lines (bExportDecompiledLayout). */
		bool bEmitLayout = true;
		/** Write Backend even when it equals the default (the Adopt route). */
		bool bEmitBackend = false;
		IR::EIRBackend DefaultBackend = IR::EIRBackend::Graph;
		/**
		 * Non-empty: write `/// @name` with it. Decided by the editor. It names the module's ONE product; in a module of
		 * several products each one carries its own FIRProduct::AssetPathOverride instead and this is ignored.
		 */
		FString AssetPathOverride;
		/** "Decompiled from ...", warnings: leading comment trivia of the module (FLangTrivia). Each entry is one line, without `//`. */
		TArray<FString> HeaderComments;
		/** Interfaces of the function assets the module's graphs call, one per asset. */
		TArray<FIRToAstExternInterface> ExternInterfaces;
		/**
		 * Parallel to FIRModule::Products, or empty: the asset each product was imported from. A FunctionCall whose
		 * FunctionPath is one of these is written as a call to that function of this file, not as an `extern` to the
		 * asset the file itself makes.
		 */
		TArray<FString> ProductAssetPaths;
		/** Prefer operator and intrinsic spellings where a reflected call is the exact one (`x * 2.0` for `UE.Multiply(A = x, B = 2.0)`; RD-1). */
		bool bReadable = false;
	};

	DREAMSHADERLANG_API TUniquePtr<FModule> BuildDreamShaderAstFromIR(
		const IR::FIRModule& Module,
		const IR::FBuiltinCatalog& Catalog,
		const FIRToAstOptions& Options,
		FLangDiagnosticSink& Diagnostics);

	/**
	 * Raises the emitter's lowered shapes back to source-level ones, in place (research-decompiler.md section 3.9).
	 * With a catalog it also reads the Substrate sugar back: `A + B`, `A * w` and `lerp(A, B, t)` for the three
	 * composition nodes, and `BaseColor = / Metallic = / Haziness = / Transmittance =` for a conversion node that feeds
	 * one BSDF and nothing else. Both are graph-exact, like every other rule; the catalog is what says which engine class
	 * a node is, whatever short name it goes by.
	 */
	DREAMSHADERLANG_API void RaiseDreamShaderIR(IR::FIRModule& Module, FLangDiagnosticSink& Diagnostics, const IR::FBuiltinCatalog* Catalog = nullptr);
}
