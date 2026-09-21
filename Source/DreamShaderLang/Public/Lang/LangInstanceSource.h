// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `.dsi` source text without the compiler: the text of an instance payload (what the decompiler writes
// for a MaterialInstanceConstant), and in-place rewrites of an existing file (Adopt of a `.dsi`, and
// "Adopt tweaks as source defaults" on a `.dss`). A rewrite is a span splice over the parsed file, never
// a reprint, so `//` comments and declaration order survive (CONTRACT section 2.3).
//
// Core-only. Design: Plan/m4m5/research-instance.md section 6.6.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Lang
{
	struct FBoundModule;

	/** Replace Span with NewText; Span.Length == 0 inserts at Span.Offset. Edits never overlap and are sorted by offset. */
	struct FLangSourceEdit
	{
		FLangSpan Span;
		FString NewText;
	};

	/** The `.dsi` tree for a payload: optional `/// @name`, `#pragma instance(Parent, Settings...)`, one uniform per override in order. */
	DREAMSHADERLANG_API TUniquePtr<FModule> BuildDreamShaderInstanceModule(const IR::FIRInstance& Instance, const FString& FilePath, const FString& AssetPathOverride);

	/** PrintDreamShaderLang(*BuildDreamShaderInstanceModule(...)). The decompiler's text. */
	DREAMSHADERLANG_API FString PrintDreamShaderInstance(const IR::FIRInstance& Instance, const FString& FilePath, const FString& AssetPathOverride, const FLangPrintOptions& Options = FLangPrintOptions());

	/** Adopt: edits turning Original (parsed as Parsed, bound as Bound) into a file that states Desired, touching only differing declarations. */
	DREAMSHADERLANG_API bool RewriteDreamShaderInstanceSource(
		const FLangSourceText& Original, const FModule& Parsed, const FBoundModule& Bound, const IR::FIRInstance& Desired,
		TArray<FLangSourceEdit>& OutEdits, FString& OutText, FLangDiagnosticSink& Diagnostics);

	/** "Adopt tweaks as source defaults": the same splice over a `.dss`'s uniform initializers / `@default` / `@static` values. */
	DREAMSHADERLANG_API bool RewriteDreamShaderUniformDefaults(
		const FLangSourceText& Original, const FModule& Parsed, const FBoundModule& Bound, const TArray<IR::FIRInstanceOverride>& NewDefaults,
		TArray<FLangSourceEdit>& OutEdits, FString& OutText, FLangDiagnosticSink& Diagnostics);

	/** Shortest decimal text that reads back as the same float32. */
	DREAMSHADERLANG_API FString FormatDreamShaderFloatLiteral(double Value);

	/** "Base Color" -> "Base_Color": a valid, non-reserved 2.0 identifier. */
	DREAMSHADERLANG_API FString MakeDreamShaderIdentifier(const FString& ParameterName);
}
