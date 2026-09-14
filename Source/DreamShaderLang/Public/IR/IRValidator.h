// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR validator: the gate between "the builder thinks it is finished" and "the emitter may
// touch a UObject".
//
// It exists because the emitter is meant to be mechanical. Every question the emitter would
// otherwise have to ask -- does this operand exist, is this pin real, does this material product
// have exactly one sink, is this mask within the operand's width, does this graph even have an
// order -- is asked once here, on data alone, with no engine in the process. `dsc check` stops at
// this call: if the IR validates, the only failures left are asset-side.
//
// It is recoverable: every problem it can find is reported, and it returns false when any of them
// was an error. Diagnostics are DSH4300-DSH4349.

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::IR
{
	/**
	 * Checks one lowered module against the IR's own rules and against the catalog.
	 *
	 * Catalog may be empty: the checks that need engine knowledge (a reflected class's pins and
	 * properties, the material attribute table) are then skipped rather than reported, so a unit
	 * test can validate a hand-built graph without a reflection pass.
	 *
	 * Returns true when the module is emittable. Diagnostics are appended to the sink either way;
	 * the sink's own file path is used for every one of them, so the caller sets it.
	 */
	DREAMSHADERLANG_API bool ValidateDreamShaderIR(
		const FIRModule& Module,
		const FBuiltinCatalog& Catalog,
		Lang::FLangDiagnosticSink& Diagnostics);
}
