// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Lang
{
	enum class ELangFrontend : uint8
	{
		/** From the file kind: `.dss` and `.dsh` take the 2.0 front end, `.dsm` and `.dsf` the legacy one. */
		Auto,
		/** The 2.0 syntax. */
		Dss,
		/** The 1.x syntax (second front end, M4). Not available yet: reports DSH2199. */
		Legacy,
	};

	struct FLangParseOptions
	{
		ELangFrontend Frontend = ELangFrontend::Auto;
	};

	struct FLangParseResult
	{
		/** The tree. Present even after errors, holding whatever declarations parsed cleanly, so a language service can keep working on a broken file. */
		TUniquePtr<FModule> Module;
		FLangDiagnosticSink Diagnostics;

		bool Succeeded() const { return Module.IsValid() && !Diagnostics.HasErrors(); }
	};

	/**
	 * Parses one source text into a module. The text is expected to be already preprocessed
	 * (`#if` resolved, line count conserved); a `#` line that is not `#pragma` / `#include` reaching
	 * the parser outside an opaque body is DSH3201.
	 *
	 * Recovery: an error inside a declaration skips to the next declaration boundary (a `;` or a
	 * matched `}` at brace depth zero) and continues, so one broken function does not hide the
	 * rest of the file; an error inside a statement skips to the next `;` or `}` of that block.
	 * The result's Diagnostics therefore may hold several errors; Succeeded() is false if any.
	 */
	DREAMSHADERLANG_API FLangParseResult ParseDreamShaderLang(
		const FLangSourceText& Source,
		const FLangParseOptions& Options = FLangParseOptions());

	/** Parses a single expression on its own -- for tests, the language service and `#pragma` values. */
	DREAMSHADERLANG_API FExprPtr ParseDreamShaderLangExpression(
		const FLangSourceText& Source,
		FLangDiagnosticSink& Diagnostics);
}
