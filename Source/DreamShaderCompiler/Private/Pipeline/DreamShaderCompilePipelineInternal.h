// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Pipeline helpers the compiler service needs and nothing outside this module may call.
// Defined in DreamShaderCompilePipeline.cpp.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderPreprocessor.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * The preprocessor dialect a file is read in, by its kind: `.dsm` / `.dsf` Legacy, `.dsh` Mixed,
	 * `.dss` / `.dsi` Lang2. The pipeline asks it for the compiled file and the include resolver for every header, so a
	 * header is read the same way whichever kind of source includes it.
	 */
	UE::DreamShader::EDreamShaderPreprocessDialect GetDreamShaderPreprocessDialectForFile(const FString& FilePath);

	/**
	 * Words a finished pipeline run as a compile report, in the shape FDreamShaderCompileResult::Message
	 * documents and `.skill/dsc.ps1` parses.
	 *
	 * On success: one line per product in emit order -- `Generated <Kind> <ObjectPath> from <Source>.`, or the
	 * 1.x `Skipped ...` line for a product the emitter left alone (DSH8237, DSH8209) -- then a `Warnings:` block
	 * holding the run's warning diagnostics followed by ExtraWarnings (the service passes the lines
	 * RaiseGenerationWarning collected). On failure: the first error and every other diagnostic in the wire
	 * form, or DSH9039 when the run failed without raising anything.
	 *
	 * Returns bPipelineSucceeded. OutError is reset first and always holds the report.
	 */
	bool WordDreamShaderPipelineReport(
		const FDreamShaderLang2PipelineResult& Result,
		bool bPipelineSucceeded,
		const TArray<FString>& ExtraWarnings,
		::UE::DreamShader::FDreamShaderError& OutError);
}
