// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A UDreamPassPipeline read back into the payload a `.dsp` binds to (IR::FIRPassPipeline), and from there into text by
// the Lang printer (BuildDreamShaderPipelineModule). The emitter maps the payload's canonical strings onto the
// DreamShaderPass enums; this maps them back, so what a build writes into a pipeline and what a decompile reads out of
// it are one shape.
//
// Two callers, one reader: `dsc decompile` (through the IR decompiler's routing) wants the text, and gets it checked --
// printed, parsed and bound again without the engine, and compared with what the asset holds, which the `.dss` path does
// only as far as the parse and the `.dsi` path not at all (DreamShader_Plan/05 §8). Adopt wants the payload
// alone, to splice what differs into the file it came from (RewriteDreamShaderPipelineSource), after
// KeepEquivalentPipelineSpellings has given every value the asset did not change the author's own spelling back.
//
// Diagnostics: DSH9211-9225 (DSH9210 is the IR decompiler's, DSH9226-9228 Adopt's).

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"

class UDreamPassPipeline;

namespace UE::DreamShader::Editor::Private
{
	struct FPipelineDecompileOptions
	{
		/** Where the `.dsp` will live: the file the printer names, and the root a bare material name is looked up under. */
		FString TargetSourceFilePath;
		/** `Material = "PP_X"` when the product index finds exactly that material under the target's root, built by a `.dss`. */
		bool bPreferBareMaterialNames = true;
		/** Comment lines for the head of the text, without `//` ("Decompiled by DreamShader from ..."). Text only. */
		TArray<FString> HeaderComments;
		/** The caller asked to keep the asset where it is (-KeepAssetPath), which a `.dsp` has no way to say: DSH9224 when it moves. */
		bool bKeepAssetPath = false;
	};

	/**
	 * Pipeline -> payload. False (with errors) when there is nothing to read; a warning or a note for every value the
	 * text cannot carry the way the asset has it. Floats come back as the double their shortest literal reads as, so a
	 * payload re-read from the printed text compares equal to this one.
	 */
	bool DecompileDreamPassPipeline(
		const UDreamPassPipeline* Pipeline,
		const FPipelineDecompileOptions& Options,
		UE::DreamShader::IR::FIRPassPipeline& OutPipeline,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * DecompileDreamPassPipeline, the printer, and CheckDreamShaderPipelineText over what it printed. The decompile's warnings
	 * are written into the head of the text as `// Warning:` lines; the check's go to Diagnostics only.
	 */
	bool DecompileDreamPassPipelineToText(
		const UDreamPassPipeline* Pipeline,
		const FPipelineDecompileOptions& Options,
		FString& OutText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The re-parse check: Text parsed and bound engine-free (FBindOptions::PipelineReferences = null) states Expected --
	 * the bound pipeline's payload, which is what its PassPipeline product carries, compared by CompareDreamShaderPipelines.
	 * The facts only the engine knows -- what a material reference or a shader path resolves to, a `.usf`'s
	 * [numthreads] -- are taken from Expected where the text spells the reference the same way. A warning (DSH9221-9223),
	 * never an error: the text is still the best the decompiler has. True when it agrees.
	 */
	bool CheckDreamShaderPipelineText(
		const FString& FilePath,
		const FString& Text,
		const UE::DreamShader::IR::FIRPassPipeline& Expected,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Every value of Desired that means what Current's means takes Current's spelling: a material or a shader that
	 * resolves to the same asset or file keeps the reference as written, a float equal at the precision the asset keeps
	 * (float32) keeps the double the source's literal or expression made, a flag set that is the same mask keeps its
	 * written form, and a key written although it states the default stays written. Parameters, buffers and passes are
	 * matched by name, bindings by position. Adopt runs it so the splice touches what changed and nothing else.
	 */
	void KeepEquivalentPipelineSpellings(
		const UE::DreamShader::IR::FIRPassPipeline& Current,
		UE::DreamShader::IR::FIRPassPipeline& Desired);

	/**
	 * The first diagnostic of a pipeline decompile a write-back must not go past: an error, or a value the text would
	 * drop or change (DSH9211, 9212, 9215-9217, 9219, 9220) -- rebuilding from such a text would silently take it out of
	 * the asset. Null when there is none.
	 */
	const UE::DreamShader::Lang::FLangDiagnostic* FindPipelineWriteBackBlocker(const UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The file an HLSL pass's ShaderPath names. A virtual path (`/Project/Passes/Blur.usf`) maps by the walk
	 * GetShaderSourceFilePath makes over the shader directory mappings, without its error report for a path no mapping
	 * covers; any other path is relative to the folder of the `.dsp` the pipeline was built from (PipelineSourceFile, as the
	 * asset records it: project-relative, or absolute outside the project). Empty when neither works.
	 */
	FString ResolveDreamPassShaderSourceFile(const FString& ShaderPath, const FString& PipelineSourceFile);
}
