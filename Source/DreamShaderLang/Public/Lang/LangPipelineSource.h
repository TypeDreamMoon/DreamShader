// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `.dsp` source text without the compiler: what a Custom Pass pipeline payload looks like as text (what the
// decompiler writes for a UDreamPassPipeline), in-place rewrites of an existing file (Adopt), the references a host
// resolves before binding, and the canonical spellings the payload uses -- the same tables the DreamShaderPass
// runtime keeps (UE::DreamPass::LexToString), which a test holds equal.
//
// Core-only.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Lang
{
	struct FBoundModule;

	/**
	 * The `Material = "..."` and `Shader = "..."` values of a parsed `.dsp`, each once, in source order: what the host
	 * resolves into FPipelineReferences before it binds the file.
	 */
	DREAMSHADERLANG_API void CollectDreamShaderPipelineReferences(const FModule& Module, TArray<FString>& OutMaterials, TArray<FString>& OutShaders);

	/** The `.dsp` tree for a payload: `#pragma pipeline`, uniforms, buffers, passes, in the payload's order. */
	DREAMSHADERLANG_API TUniquePtr<FModule> BuildDreamShaderPipelineModule(const IR::FIRPassPipeline& Pipeline, const FString& FilePath);

	/** PrintDreamShaderLang(*BuildDreamShaderPipelineModule(...)): the decompiler's text. Defaults are not written. */
	DREAMSHADERLANG_API FString PrintDreamShaderPipeline(const IR::FIRPassPipeline& Pipeline, const FString& FilePath, const FLangPrintOptions& Options = FLangPrintOptions());

	/**
	 * Adopt: edits turning Original (parsed as Parsed, bound as Bound) into a file that states Desired, touching only
	 * the declarations and keys that differ, so comments and order survive -- a span splice, never a reprint.
	 */
	DREAMSHADERLANG_API bool RewriteDreamShaderPipelineSource(
		const FLangSourceText& Original, const FModule& Parsed, const FBoundModule& Bound, const IR::FIRPassPipeline& Desired,
		TArray<FLangSourceEdit>& OutEdits, FString& OutText, FLangDiagnosticSink& Diagnostics);

	/**
	 * Structural equality of two payloads, ignoring source references: what the round trip `.dsp` -> asset -> `.dsp`
	 * -> asset checks. OutDifferences, when given, receives one line per difference.
	 */
	DREAMSHADERLANG_API bool CompareDreamShaderPipelines(const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B, TArray<FString>* OutDifferences = nullptr);

	/** The canonical spellings, in the order of the runtime's enums (EDreamPassInjection, EDreamPassBufferFormat). */
	DREAMSHADERLANG_API TConstArrayView<const TCHAR*> GetDreamShaderPassInjectionNames();
	DREAMSHADERLANG_API TConstArrayView<const TCHAR*> GetDreamShaderPassFormatNames();
}
