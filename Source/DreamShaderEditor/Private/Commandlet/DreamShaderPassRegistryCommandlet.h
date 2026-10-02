// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `pass-registry`: the Custom Pass HLSL slot registry (`<source root>/.dreampass`) from the command line. The registry is
// a committed file that every teammate's editor and every cook builds the global shaders from, so the verbs that change
// it are explicit ones; listing it changes nothing.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	/**
	 * `pass-registry [-Gc | -Rebuild]`.
	 *
	 *   (no flag)   every slot with what it is -- Live, Reserved, SnapshotMissing, PipelineGone, PassGone or Unknown --
	 *               judged by running the front end of every `.dsp` that owns one; nothing is written
	 *   -Gc         frees the PipelineGone and PassGone slots
	 *   -Rebuild    compiles every `.dsp` again (a Registry.json that does not parse is moved aside first, and every slot
	 *               assigned afresh), collects the garbage, and writes the registry files from Registry.json
	 *
	 * False when anything failed; the summary line ends `RESULT=OK` or `RESULT=FAILED`, as every 2.0 verb's does.
	 */
	bool RunDreamShaderPassRegistryCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
}
