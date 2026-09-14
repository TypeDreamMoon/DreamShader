// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Lowering: FBoundModule -> FIRModule. One graph per product.
//
// The builder walks the AST a second time. It never resolves a name: every question it could ask
// -- what does this identifier mean, what type is this expression, which pin did this argument
// match, is this loop bounded -- was answered by the binder and is read out of FBoundModule's side
// tables by AST node address (FBoundModule::Expressions, StatementRegions, LoopTripCounts). If the
// builder ever has to look something up, the binder has a gap; say so rather than working around it.
//
// Values are SSA. A local variable is a mutable *environment slot* holding a lowered value, not a
// mutable node: `x = x + 1` rebinds the slot to a new Add node and leaves the old one alone. A
// struct-typed local is a slot holding a field map (structs never become nodes -- plan section 11 #1
// (c)); a `material`-typed local is a slot holding the attribute map of contract section 6.2.
//
// Control flow is erased. `if` becomes StaticSwitch or Select over every slot the two branches
// disagree about; loops are unrolled with the binder's trip count; `return` becomes a merge at the
// end of the function; a helper call is inlined. Anything with no graph form is refused with a
// diagnostic that points at `/// @custom`, which is the escape hatch for all of it.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Lang
{
	struct FBoundModule;
}

namespace UE::DreamShader::IR
{
	struct FBuiltinCatalog;

	struct FIRBuildOptions
	{
		/** A loop whose trip count is higher than this is refused (DSH4360) even if the binder proved it. */
		int32 MaxUnrolledIterations = 64;
		/** Helper inlining depth before the builder refuses (DSH6221). */
		int32 MaxInlineDepth = 32;
		/** Keep FIRNode::DebugName (the variable a value was first assigned to). Layout hints match on it. */
		bool bKeepDebugNames = true;
		/**
		 * An override for the catalog the build reads. Normally null: the catalog travels on
		 * `FBoundModule::Catalog`, which the binder sets, and every catalog index in the bound
		 * module is an index into that one. Setting this replaces it, which is what a tool that
		 * loaded a catalog from JSON does when it re-lowers a module bound elsewhere.
		 *
		 * With neither, the builder reports DSH4352 once and lowers what it can -- arithmetic,
		 * uniforms, helpers, custom nodes -- but no `UE.*` call and no material attribute.
		 */
		const FBuiltinCatalog* Catalog = nullptr;
	};

	/**
	 * Lowers every product of a bound module. Returns a module even when it reported errors -- the
	 * partial graph is what `dsc dump-ir` shows and what the tests diff -- so callers ask
	 * Diagnostics.HasErrors(), not whether the pointer is valid. Null is returned only when Bound
	 * has no Module at all.
	 *
	 * The bound module (and the AST behind it) must outlive the call, not the result: nothing in
	 * FIRModule points back into either.
	 */
	DREAMSHADERLANG_API TUniquePtr<FIRModule> BuildDreamShaderIR(
		const Lang::FBoundModule& Bound,
		const FIRBuildOptions& Options,
		Lang::FLangDiagnosticSink& Diagnostics);
}
