// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// An inline FExpressionInput mask, read the way the IR spells a Swizzle.
//
// The 2.0 emitter never writes one (a Swizzle is an output selection or a ComponentMask node), but
// 1.x assets and hand-wired graphs carry them, and both readers of a foreign graph -- the decompiler's importer and
// the parity oracle that normalises a 1.x graph before comparing it -- have to agree on what such a mask means
// (Tools/Parity/README.md, PD-1). This is that meaning, in one place.
//
// The mask bits of a pin name ABSOLUTE channels (R, G, B, A) of the expression's value; the value that arrives through
// a masked OUTPUT has only that output's channels. So the mask is resolved against the output: a pin that keeps G and B
// of an output publishing R, G and B keeps the operand's components 1 and 2, which the IR writes "yz".

#pragma once

#include "CoreMinimal.h"

struct FExpressionInput;
struct FExpressionOutput;

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderInlineMask
	{
		/** The pin carries a mask at all. */
		bool bMasked = false;
		/** The mask keeps every channel of its operand: no Swizzle, the IR builder never makes that one either. */
		bool bIdentity = false;
		/** Components of the operand the mask keeps, in IR spelling: a canonical ascending subset of "xyzw". */
		FString Relative;
		/** How many channels the operand has. */
		int32 OperandWidth = 0;
	};

	/**
	 * SourceOutput is the output the pin reads, or null when there is none to ask (a reroute's). UnmaskedOperandWidth is
	 * the width of the value when that output is not itself a channel view; the caller knows it -- the importer from IR
	 * typing, the oracle from the 1.x width helper.
	 */
	FDreamShaderInlineMask ResolveDreamShaderInlineMask(
		const FExpressionInput& Input,
		const FExpressionOutput* SourceOutput,
		int32 UnmaskedOperandWidth);
}
