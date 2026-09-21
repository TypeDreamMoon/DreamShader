// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Whether two IR modules are the same assets, said without looking at how either was written down.
//
// DumpDreamShaderIRText is the golden format: it numbers nodes in creation order and prints where each one came
// from, which is what a golden wants and what makes it useless for the question "did rewriting this source change what
// it builds". A `.dsm` and the `.dss` it migrated to make their nodes in another order, on other lines, under other
// variable names, and are still the same material.
//
// So this compares graphs from their roots: a node is its op, its class, its properties, its output types and the
// nodes it reads, in order -- and nothing else. Two graphs are equivalent when the sink, the function inputs, the
// function outputs and the other statement roots are, one by one.
//
// Users: `dsc migrate` (DSH9098), and the decompile round trip tests, which compare what a source built with what
// its decompiled text builds.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"

namespace UE::DreamShader::IR
{
	struct FBuiltinCatalog;

	struct FIRCompareOptions
	{
		/** Product Name, AssetPathOverride and the 1.x root: where the asset goes. Off when the two sides say that in different ways. */
		bool bCompareDestinations = true;
		/** `#pragma material` settings, the backend, `@library`, `@desc`. */
		bool bCompareSettings = true;
		/** FIRNode::DebugName: the variable a value was assigned to. It reaches the asset as a node description, nothing more. */
		bool bCompareDebugNames = false;
		/** The region a node sits in, by title. */
		bool bCompareRegions = false;
		/** The file and line comments the Custom code builder writes around a body. Off, `Code` is compared without them. */
		bool bCompareCustomMarkers = false;
		/**
		 * The blanks of Custom `Code`. Off, a line is compared trimmed and with every run of blanks as one, and a blank
		 * line is not compared at all: a migrated body lost the indentation of its `Namespace` block (the last line of
		 * the body with it), and a lifted call written in another spelling is padded to another length.
		 */
		bool bCompareCodeSpacing = true;
		/**
		 * Set: a Constant wired to a pin that has a `Const*` twin is compared as the twin property of that value. The
		 * 1.x front end wires the node, because 1.x did (it wrote `ConstY` only where the source said `ConstY`); the
		 * same call in a `.dss` is the property, so a migrated file builds the twin and means the same number. The
		 * catalog is the one both modules were built against.
		 */
		const FBuiltinCatalog* ConstTwinCatalog = nullptr;
		/**
		 * A swizzle that takes every component of its operand in order (`.rgb` of a float3). Off, it is compared as its
		 * operand: a 1.x product keeps such a mask as the node 1.x built, and the same text in a `.dss` builds none.
		 */
		bool bCompareIdentitySwizzles = true;
	};

	/**
	 * True when A and B describe the same products with equivalent graphs. OutDifference is the first difference
	 * found, in words, followed down to the deepest node that differs; empty when equivalent.
	 *
	 * One normalisation is not optional, because it is the engine's and not an opinion: a FunctionInput's PreviewValue is
	 * compared over the components its InputType reads, and a missing one is the engine's own (0, 0, 0, 1).
	 */
	DREAMSHADERLANG_API bool AreDreamShaderIRModulesEquivalent(
		const FIRModule& A,
		const FIRModule& B,
		const FIRCompareOptions& Options,
		FString& OutDifference);
}
