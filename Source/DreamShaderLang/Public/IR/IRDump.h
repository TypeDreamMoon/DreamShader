// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Rendering a lowered module as text or as JSON: `dsc dump-ir`, and the corpus golden.
//
// The text form is the expectation file of every `Tests/Corpus/IR` case, so it is a FORMAT, not a
// debug print. Three rules follow from that and may not be relaxed:
//
//   1. Deterministic. The same module renders byte for byte the same, in this process and the
//      next: numbers go through one culture-invariant formatter, settings and properties are
//      emitted in sorted order, and nothing iterates a TMap in hash order.
//   2. Nothing machine-specific. Source files appear as their leaf name, never as the absolute
//      path they were compiled from, so a golden recorded on one machine matches on another.
//   3. One node per line, in index order. A Custom node's Code is a whole HLSL body; it is escaped
//      rather than allowed to break the line.
//
// The JSON form carries the same content for tools that would rather not parse the text.

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"

namespace UE::DreamShader::IR
{
	/**
	 * The module as diff-friendly text: a module header, then one block per product with its
	 * regions, layout hints, nodes and signature.
	 *
	 *     module "Toon.dss"
	 *
	 *     product 0 Material "M_Toon" backend=Graph settings{ShadingModel=Unlit}
	 *       region 0 "Sampling" parent=-1
	 *       %0 = Constant(Value=(0.5, 0.5, 0.5)) : float3 @L3:C14
	 *       %1 = Multiply(%0, %3) : float3 @L14:C9 {Sampling} "Tint"
	 *       %2 = MaterialSink(BaseColor=%1) @L20:C2
	 *       sink %2
	 */
	DREAMSHADERLANG_API FString DumpDreamShaderIRText(const FIRModule& Module);

	/** The same content as JSON, schema "dreamshader-ir" version 1. */
	DREAMSHADERLANG_API FString DumpDreamShaderIRJson(const FIRModule& Module);
}
