// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR passes: constant folding, swizzle canonicalisation, dedupe, prune, re-index.
//
// They run on a module the builder just produced and leave it in the shape the emitter expects:
// no duplicate nodes, no dead nodes, no identity swizzles, every FIRValue in range. The dedupe key
// of contract section 6.7 is computed here and stamped on every node; the emitter reads it and may
// not re-derive it, because the rule "two nodes with the same class and the same sorted argument
// table are one node" is what 1.x did at build time and is now a property of the IR.
//
// Every pass is idempotent and order-independent in effect: running RunDreamShaderIRPasses twice
// on the same module changes nothing, node indices included. The tests depend on that -- a golden
// IR dump is taken after the passes and must survive another round.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::IR
{
	struct FIRPassOptions
	{
		/** Fold math, Swizzle and Append whose operands are all Constant. */
		bool bFoldConstants = true;
		/** Merge nodes with equal DedupeKey (contract section 6.7). */
		bool bDedupe = true;
		/** Drop nodes no sink, function output or custom-output statement reaches. */
		bool bPrune = true;
	};

	/**
	 * Runs the passes over every product of the module, in place. FIRValues stay valid: the last
	 * pass rebuilds the node array and remaps every reference, keeping Region, Source and DebugName.
	 *
	 * Diagnostics are informational only (DSH4390, an unused uniform); a pass never fails a build.
	 */
	DREAMSHADERLANG_API void RunDreamShaderIRPasses(
		FIRModule& Module,
		const FIRPassOptions& Options,
		Lang::FLangDiagnosticSink& Diagnostics);
}
