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
	 * 2.0 has no transient request (plan §5): materials and material functions ALWAYS save.
	 */
	bool EmitDreamShaderIRProduct(
		const IR::FIRModule& Module,
		int32 ProductIndex,
		const FIREmitContext& Context,
		UObject*& OutAsset,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Fills the builtin catalog from UE reflection: every non-abstract UMaterialExpression subclass,
	 * its FExpressionInput pins, its editable literal properties, its outputs, and the material
	 * attribute table.
	 *
	 * Deterministic: expressions are sorted by class name and everything inside an entry keeps the
	 * engine's own declaration order, because unit P exports this as JSON for the tools and the
	 * language service and a reordering would read as a diff on every export.
	 */
	void BuildBuiltinCatalogFromReflection(IR::FBuiltinCatalog& OutCatalog);

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
	void WriteDreamShaderSourceSpans(UObject* Asset, const TMap<FGuid, IR::FIRSourceRef>& Spans);
}
