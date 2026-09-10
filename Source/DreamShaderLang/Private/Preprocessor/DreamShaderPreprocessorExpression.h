// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The constant-expression evaluator behind `#if` and `#elif`.
//
// Split out from the directive scanner because the two answer different questions: the scanner
// decides which lines survive, this decides what one condition means. The split is also what keeps
// the evaluator away from the define registry -- it is handed a reader callback and never learns
// that a table, a settings object or a command line exists, which is what lets both halves be
// exercised on their own.
//
// Grammar and value domain: Plan/preprocessor-conditionals.md, section 4.

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderDiagnostic.h"
#include "Templates/Function.h"

namespace UE::DreamShader::Private
{
	/**
	 * Reads one define, and records that it was read.
	 *
	 * Returns false when the name has no definition, leaving OutValue untouched.
	 *
	 * The recording is why this is a callback and not a table reference. The build key folds in
	 * exactly the names an evaluation actually consulted, and only the caller driving the file knows
	 * where to put them and which table the recorded value has to come from. It also makes the
	 * short-circuit rule fall out for free: an operand that is never evaluated never reaches here,
	 * so it is never recorded, which is precisely what the precise build key needs.
	 */
	using FDreamShaderPreprocessorDefineReader = TFunctionRef<bool(const FString& /*Name*/, FString& /*OutValue*/)>;

	/**
	 * Raises DSH1042: the directive was already complete, and something that is not a comment follows.
	 *
	 * It lives here, in the lower of the preprocessor's two translation units, because BOTH of them
	 * reach it -- the directive scanner for `#ifdef A B`, `#undef A B`, `#else junk` and `#endif junk`,
	 * and the evaluator for `#if 1 2`, `#elif 1 2` and `#if (1))`. One code, one raise site, one
	 * wording, which is what .skill/gen-diagnostics.ps1 documents and what the reader gets.
	 *
	 * That the two halves share a code is the whole point, and it is worth stating because the
	 * tempting split is the wrong one. Cutting by which component noticed -- evaluator errors get
	 * DSH1034, scanner errors get DSH1042 -- is natural to write and incoherent to read: `#ifdef A B`
	 * means exactly `#if defined(A) B`, so it cannot be a different KIND of mistake from `#if 1 2`.
	 * The line that does survive contact with a reader is about the expression itself: incomplete or
	 * malformed is DSH1034, complete-with-leftovers is this. Whatever is left over and whichever
	 * directive it followed, the fix is the same sentence -- delete it or comment it out.
	 *
	 * @param InDirective         Spelling as written, `#` included: "#if", "#ifdef", "#endif".
	 * @param InFirstTrailingToken  ONLY the first leftover token. Quoting the whole remainder would
	 *                              bury the mistake in a line the author can already see; naming one
	 *                              token says where to put the cursor. Callers extract it.
	 */
	bool FailDreamShaderPreprocessorTrailingTokens(
		const FString& InFilePathForDiagnostics,
		int32 InLineNumber,
		const FString& InDirective,
		const FString& InFirstTrailingToken,
		FDreamShaderTextError& OutError);

	/**
	 * Evaluates one `#if` / `#elif` condition to a verdict.
	 *
	 * Call this only for a condition that is actually reached. Inside a branch that was already cut,
	 * C does not evaluate the nested conditions and neither does DreamShader: a dead branch may hold
	 * nonsense, and evaluating it anyway would both report errors nobody can act on and put names in
	 * the touched set that cannot affect the output.
	 *
	 * @param InExpression              Condition text, with any trailing `//` comment already removed.
	 * @param InDirective               Spelling used in messages -- `#if` or `#elif`.
	 * @param InFilePathForDiagnostics  File named in messages.
	 * @param InLineNumber              1-based physical line of the directive.
	 * @param InReadDefine              Name lookup; see above.
	 * @param bOutResult                Valid only when this returns true.
	 * @param OutError                  DSH1034, DSH1036, DSH1040 or DSH1041 on failure.
	 */
	bool EvaluateDreamShaderPreprocessorCondition(
		const FString& InExpression,
		const TCHAR* InDirective,
		const FString& InFilePathForDiagnostics,
		int32 InLineNumber,
		FDreamShaderPreprocessorDefineReader InReadDefine,
		bool& bOutResult,
		FDreamShaderTextError& OutError);
}
