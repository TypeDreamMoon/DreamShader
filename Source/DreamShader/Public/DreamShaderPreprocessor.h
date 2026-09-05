// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader's conditional-compilation preprocessor. Eight directives: `#if` / `#ifdef` /
// `#ifndef` / `#elif` / `#else` / `#endif`, plus `#define` / `#undef`. Line-oriented, evaluated at
// generation time -- a branch that is not taken never reaches the parser and never becomes graph
// nodes.
//
// It does NOT descend into `Function` / `GraphFunction` bodies: those are raw HLSL handed to the
// shader compiler, and the `#` directives in them belong to ITS preprocessor. Every such directive
// in this project sits in one of those bodies, so eating them would silently pick the wrong branch
// in shipping material functions.
//
// This exists to do the one thing a StaticSwitch node cannot: cut the DECLARATION layer. A material
// that needs a different ShadingModel or a different Outputs block under Substrate has nowhere to
// put a switch node, because the difference is in what the material IS, not in what it computes.
//
// See Plan/preprocessor-conditionals.md for the full design and the decisions behind it.

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderDefineTable.h"
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader
{
	/**
	 * Recorded in FDreamShaderPreprocessResult::TouchedDefines for a name that was read while it was
	 * NOT defined.
	 *
	 * It has to be recorded, not skipped. The build key folds in the touched set, so a name absent
	 * from the map is a name whose later appearance would not change the hash -- and a source that
	 * starts taking a different branch without its asset rebuilding is a silent data loss. The
	 * sentinel is spelled so no real define value can collide with it.
	 */
	inline const TCHAR* const GDreamShaderUndefinedDefineSentinel = TEXT("<undef>");

	struct FDreamShaderPreprocessResult
	{
		/**
		 * The preprocessed text. Its LINE COUNT IS EXACTLY THAT OF THE INPUT -- every directive line
		 * and every elided line is emitted as an empty line rather than removed.
		 *
		 * This is not tidiness, it is the load-bearing invariant of the whole feature. The generator's
		 * diagnostics mapper recovers physical line numbers by counting emitted lines inside each
		 * file's `// Begin/End DreamShader source:` block (the import inliner blanks its own directive
		 * lines for the same reason). Drop one line here and every error below it in the file points
		 * at the wrong place, in every source that uses conditionals.
		 */
		FString Text;

		/**
		 * Every define name actually READ during this run, mapped to the value it had, or to
		 * GDreamShaderUndefinedDefineSentinel when it had none.
		 *
		 * Short-circuited operands are not read and so are not listed, and neither are the defines
		 * mentioned inside a branch that was skipped. Both omissions are correct: preprocessing is
		 * deterministic and the position of the k-th evaluated condition depends only on the results
		 * of the k-1 before it, so if every value in this map is unchanged the output is byte-identical.
		 * A define inside a dead branch cannot matter until the condition that killed the branch
		 * changes -- and that condition IS in the map.
		 *
		 * This is what makes the precise (rather than fold-everything) build key provably sound.
		 */
		FDreamShaderDefineValueMap TouchedDefines;

		/**
		 * True when the input contained any preprocessor directive at all, taken or not.
		 *
		 * The Adopt action reads this: adopting rewrites the source from the generated asset, and the
		 * asset only ever holds the post-cut result, so adopting a conditional source would silently
		 * delete its conditionals. Adopt refuses instead (DSH8149).
		 */
		bool bHadDirectives = false;
	};

	/**
	 * Runs the preprocessor over one file's text.
	 *
	 * Pure text in, pure text out -- no asset, no world, no engine state beyond the define table it is
	 * handed, which is what lets it be unit-tested exhaustively.
	 *
	 * MUST run before import extraction, not after: `#if` is allowed to wrap an `Import` line, and an
	 * import that a false branch cut has to be gone before anything goes looking for imports to
	 * inline. The dependency graph service deliberately scans the RAW file instead, so its answer is
	 * the union over all branches -- editing a `.dsh` reachable only from a dead branch still
	 * triggers a rebuild. Over-rebuilding is harmless; under-rebuilding is corruption.
	 *
	 * `#define` is FILE-LOCAL, unlike C. A definition in a header is not visible to the file that
	 * imports it, precisely because this runs before inlining. That trade buys `#if` around `Import`,
	 * and it drops C's include-order-dependent macro state on the floor. Central switches belong in
	 * the settings table or in RegisterDreamShaderDefine.
	 *
	 * @param InText                    The file's raw text.
	 * @param InFilePathForDiagnostics  Used only to build error messages.
	 * @param InDefines                 Resolved table, normally from ResolveDreamShaderDefines().
	 * @param OutResult                 Valid only when this returns true.
	 * @param OutError                  DSH1030..DSH1042 on failure.
	 */
	DREAMSHADER_API bool PreprocessDreamShaderSource(
		const FString& InText,
		const FString& InFilePathForDiagnostics,
		const FDreamShaderDefineTable& InDefines,
		FDreamShaderPreprocessResult& OutResult,
		FDreamShaderTextError& OutError);

	/**
	 * Cheap scan for whether text contains any preprocessor directive, without evaluating anything.
	 *
	 * For callers that only need the Adopt-gate answer and have no define table to hand -- and for
	 * the ones that must give that answer about a file whose conditions would fail to evaluate.
	 */
	DREAMSHADER_API bool DreamShaderSourceHasPreprocessorDirectives(const FString& InText);

	/**
	 * Evaluates ONE `#if` / `#elif` condition on its own -- no file, no directives, no branch state.
	 *
	 * This exists so a TOOL can ask what a condition means without owning a source file to wrap it
	 * in. Its first caller is the editor's `DreamShader.PreprocessorDefines` manifest, which
	 * publishes a conformance vector -- a list of expressions together with the answers this
	 * evaluator gives them -- so that the VS Code extension's own JavaScript re-implementation of
	 * this grammar can be tested against the C++ one instead of drifting away from it in silence.
	 *
	 * Synthesizing a one-line `#if <expr>` file and running PreprocessDreamShaderSource over it
	 * would have avoided widening this header, and was rejected. The answer would then also carry
	 * the directive scanner's behaviour -- its `//` stripping, its line bookkeeping, its own
	 * trailing-token checks -- so an expectation published as being about the EXPRESSION would
	 * quietly be an expectation about two layers at once. A conformance failure that cannot be
	 * attributed to a layer is worth very little.
	 *
	 * Two intentional differences from the in-file path:
	 *
	 *   * No touched-define set is collected. That recording exists to make the build key precise
	 *     (see FDreamShaderPreprocessResult::TouchedDefines); a caller asking a question rather than
	 *     compiling a file has no key to protect.
	 *   * Messages name no file and always say line 1, because there is neither. The CODE is still
	 *     exact, and the code is what a caller of this should be reading. Anything that needs a
	 *     located diagnostic has a real file, and should therefore be running the preprocessor.
	 *
	 * @param InExpression  Condition text, without the `#if`, and with any `//` comment already gone.
	 * @param InDefines     The table names resolve against. Free-standing: it need not be the
	 *                      process's resolved set, and for the conformance vector it deliberately is
	 *                      not one.
	 * @param bOutResult    Valid only when this returns true.
	 * @param OutError      DSH1034, DSH1036, DSH1040, DSH1041 or DSH1042 on failure.
	 */
	DREAMSHADER_API bool EvaluateDreamShaderConditionExpression(
		const FString& InExpression,
		const FDreamShaderDefineTable& InDefines,
		bool& bOutResult,
		FDreamShaderTextError& OutError);

	/**
	 * Folds a touched-define map into a stable string for the build key.
	 *
	 * Two requirements, both load-bearing:
	 *
	 * STABLE -- sorted by name (case-sensitively), because TMap iteration order is not stable and an
	 * unsorted fold would produce a different hash for the same inputs on a different run, which
	 * reads as "everything is stale, every time".
	 *
	 * INJECTIVE -- two different maps must never fold to the same string. A naive `Name=Value;` join
	 * is not: a value may legitimately contain `=` and `;` (`-Define=A=1;B=2` is a valid define), so
	 * {A: "1;B=2"} and {A: "1", B: "2"} would collide and one of them would silently reuse the
	 * other's cached asset. Length-prefix the parts, or escape a delimiter that values cannot spell.
	 * The same applies at the boundaries: {AB: "C"} and {A: "BC"} must differ.
	 */
	DREAMSHADER_API FString BuildDreamShaderDefineKeyFragment(const FDreamShaderDefineValueMap& TouchedDefines);
}
