// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR emitter: one finished `IR::FIRProduct` in, one saved asset out.
//
// This is the only place in the 2.0 pipeline that touches a UObject. Everything upstream of it --
// binder, IR builder, passes, validator -- is engine-free, and by the time a product arrives here
// every node already says exactly which expression class it is and which pin each of its inputs
// feeds. So the emitter decides nothing: it walks FIRGraph::TopologicalOrder(), makes one
// UMaterialExpression per node, connects what the node says to connect, and hands the result to the
// 1.x asset machinery (destination rules, create/reuse, the ownership guard, the divergence gate,
// the atomic rollback, layout, the output digest, save, publish) unchanged.
//
// If the emitter ever has to work out what a node MEANS, the builder or a pass did not finish its
// job; that is a builder gap, reported as one, not patched here (plan §3.3, CONTRACT §3).
//
// Diagnostics: DSH8200-8289, LOCTEXT_NAMESPACE "DreamShader.Emitter". Nothing here asserts. Every
// failure is a diagnostic and leaves the asset exactly as it was, because the rollback is armed
// before the first node is destroyed and only committed once the graph is whole.

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "Lang/LangDiagnostic.h"
// EThinCustomPersistence, carried by FIREmitContext.
#include "DreamShaderCompilerInterface.h"

// GetDreamShaderBuiltinCatalog and BuildBuiltinCatalogFromReflection. The reflection producer was
// declared in this header until the M4 relocation gave the catalog one of its own; included so that
// its callers still see it.
#include "DreamShaderBuiltinCatalog.h"

class UMaterialExpression;
class UObject;

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Everything the emitter needs that is not in the IR.
	 *
	 * `Catalog` is the reflection snapshot the front end was bound against; the emitter reads it for
	 * the material-attribute table (attribute name -> `MP_*`) and for the pin/property shape of a
	 * reflected class. It must be the SAME catalog the binder used, or a node could name a pin this
	 * engine does not have.
	 *
	 * `SourceFilePath` / `SourceHash` are stamped onto the asset as DreamShader.SourceFile /
	 * DreamShader.SourceHash, which is what the next compile's skip check and the provenance
	 * actions read. `bForce` rebuilds even when that hash says the asset is current.
	 *
	 * `EmittedProductAssetPaths` maps a product index in the same FIRModule to the object path the
	 * asset it was emitted to actually landed on. A `FunctionCall` node with Prop::LocalFunction set
	 * is a call to another product of this file, and the pipeline (unit P) compiles products in
	 * dependency order, so by the time a caller is emitted its callee is in here. See the report's
	 * "Contract changes needed": this field is an addition to the §5 signature.
	 */
	struct FIREmitContext
	{
		const IR::FBuiltinCatalog* Catalog = nullptr;
		FString SourceFilePath;
		FString SourceHash;
		bool bForce = false;
		TMap<int32, FString> EmittedProductAssetPaths;
		/** The state a ThinCustom product ends in: the compile request's, forwarded by the pipeline. Ignored by every other kind. */
		::UE::DreamShader::EThinCustomPersistence ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;
		/**
		 * Whether the user cancelled. Asked while a graph is being rebuilt, which is the one stretch a cancel can land in
		 * the middle of: the old graph is detached by then, and answering true rolls it back (DSH8298) exactly as any
		 * other failed rebuild is rolled back. Unset: the emit never cancels.
		 */
		TFunction<bool()> IsCancelled;
	};

	/**
	 * Emits one product of a module into its asset and saves it.
	 *
	 * Returns true when the asset is current afterwards -- which includes the two "nothing to do"
	 * answers: the source hash was unchanged and bForce was not set, and another editor owns writing
	 * this project's generated assets. OutAsset is set in both of those cases too, so a caller can
	 * still report which asset the product maps to. Returns false only when the product could not be
	 * emitted, with at least one error in Diagnostics and the asset rolled back to its prior graph.
	 *
	 * OutAsset is the UMaterial / UMaterialFunction, or -- for a ThinCustom material -- the
	 * UDreamShaderMaterialInstance, which is the addressable half of that pair.
	 *
	 * Materials and material functions always save; a ThinCustom product follows Context.ThinCustomPersistence
	 * (plan v2 section 5), and storage still decides.
	 */
	DREAMSHADERCOMPILER_API bool EmitDreamShaderIRProduct(
		const IR::FIRModule& Module,
		int32 ProductIndex,
		const FIREmitContext& Context,
		UObject*& OutAsset,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Writes the node -> source-line table into the asset's package metadata, key
	 * `DreamShader.SourceSpans` (plan §11 #10, CONTRACT §2).
	 *
	 * JSON object keyed by the expression's MaterialExpressionGuid:
	 *   { "<guid>": { "file", "line", "col", "len", "callFile", "callLine", "callCol" } }
	 * `callLine`/`callCol` are present only for a node made while inlining a helper, and `callFile`
	 * only when that call site is in another file than the span (CONTRACT 6.13 #6) -- a helper
	 * inlined from an included `.dsh`. Never Desc: Desc is the user's, and the digest reads it.
	 */
	DREAMSHADERCOMPILER_API void WriteDreamShaderSourceSpans(UObject* Asset, const TMap<FGuid, IR::FIRSourceRef>& Spans);

	/**
	 * Writes `DreamShader.DecompileHints` into the asset's package metadata, beside DreamShader.SourceSpans (agreement A6; wire
	 * schema version 1, research-decompiler section 6.5):
	 *
	 *   { "version": 1,
	 *     "names":       { "<ExpressionGuid>": "Albedo" },
	 *     "nodeRegions": { "<ExpressionGuid>": 0 },
	 *     "regions":     [ { "name": "Sampling", "parent": -1 } ],
	 *     "comments":    [ { "name": "Notes", "x": -800, "y": 120, "w": 420, "h": 240, "color": [0.10, 0.16, 0.22, 0.35] } ] }
	 *
	 * `names` holds the variable a node's value was first assigned to; `nodeRegions` an index into `regions`, whose `parent` is
	 * another index or -1; `comments` the `#pragma layout(Comment, ...)` boxes exactly as the source declared them, so `w`/`h`
	 * appear only when the pragma gave a size and `color` (linear RGBA) only when it gave one. Rewritten in full on every build;
	 * the output digest never reads package metadata.
	 */
	DREAMSHADERCOMPILER_API void WriteDreamShaderDecompileHints(UObject* Asset, const FString& Json);

	/**
	 * THE swizzle rule, in the one place it is decided (plan §3.3 as amended by CONTRACT §6.13 #22).
	 *
	 * A Swizzle is never an inline FExpressionInput mask. Every pin the emitter connects is written
	 * with Mask = 0 (FIREmitter::ConnectValueToInput), so UMaterialGraph::GetValidOutputIndex -- which
	 * re-points a masked wire on output 0 at whichever output matches its mask when the material
	 * editor rebuilds the graph (DSK2) -- never has anything to re-point. A Swizzle is one of two
	 * things instead:
	 *
	 * - OUTPUT SELECTION, when the mask is a strictly ascending prefix narrower than its operand
	 *   (`x`, `xy`, `xyz`), the operand is the whole of its expression's output, and the expression
	 *   publishes a NAMED output with exactly those leading channels (`R` / `RGB` on VectorParameter,
	 *   TextureSample, Constant4Vector, ParticleColor, DecalColor, ...). The wire leaves that pin,
	 *   which is exactly what the editor writes when a user drags from it. No engine class that
	 *   publishes such a named output mixes it with an unmasked output of a different value, so this
	 *   never re-points a read at a different value (checked against 5.8's MaterialExpressions.cpp).
	 * - a ComponentMask node, in every other case.
	 *
	 * Answers true, with the engine output index, for the first case. Exposed so that anything else
	 * that has to judge a graph by the same rule -- the parity oracle normalising a 1.x graph's inline
	 * masks -- asks the emitter rather than restating it.
	 */
	DREAMSHADERCOMPILER_API bool TryResolveSwizzleAsNamedOutput(
		const UMaterialExpression* Expression,
		int32 OperandOutputIndex,
		int32 OperandWidth,
		const FString& Mask,
		int32& OutOutputIndex);
}
