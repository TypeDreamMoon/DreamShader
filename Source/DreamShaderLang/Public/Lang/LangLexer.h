// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

namespace UE::DreamShader::Lang
{
	struct FLangLexOptions
	{
		/** Emit `///` lines as DocComment tokens. Off, they are skipped like any comment. */
		bool bEmitDocComments = true;
		/** Emit `#` lines as Directive tokens. Off, they are skipped. */
		bool bEmitDirectives = true;
		/** Emit `//` line comments and block comments as Comment tokens, delimiters included. Off, they are skipped as today. */
		bool bEmitComments = false;
		/**
		 * 1.x sources: `'...'` closed on its own line is one StringLiteral token holding what is between the quotes, so
		 * the engine's asset shell -- `Texture2D'/Game/T.T'`, written without double quotes as a Properties default --
		 * lexes as a word and a string instead of failing on the quote (DSH2101). Whoever reads such a value takes the
		 * source text of the token run, quotes included. Off, `'` is an unexpected character, which is what it is in 2.0.
		 */
		bool bLexAssetShellQuotes = false;
	};

	/**
	 * Cuts a whole source text into tokens.
	 *
	 * Rules, in the order the lexer tries them at each position:
	 *   - whitespace and newlines are skipped; the next token gets bAtLineStart when a newline
	 *     (or the start of the text) separated it from the previous token;
	 *   - `//` starts a line comment, skipped -- except `///` (and not `////`), which becomes a
	 *     DocComment token holding the rest of the line;
	 *   - `/*` starts a block comment, skipped; unterminated is DSH2102 at the opening;
	 *   - `#` at line start (only whitespace before it on the line) becomes one Directive token for
	 *     the rest of the line, with a trailing `//` comment removed; `#` anywhere else is Unknown
	 *     and reported as DSH2106;
	 *   - a string literal `"..."` resolves the C escapes `\\ \" \n \r \t \0`; unterminated is
	 *     DSH2103, an unknown escape is DSH2104 (the backslash is kept, lexing continues);
	 *   - a number: `0x` hex or decimal digits; a `.` or an exponent makes it a float; suffixes
	 *     `f F h H` (float), `u U l L` (integer); a malformed number (`1.2.3`, `0x` with no digits,
	 *     `1e`) is DSH2105 and still yields one token so the parser stays aligned;
	 *   - an identifier `[A-Za-z_][A-Za-z0-9_]*`, promoted to Keyword when TryGetLangKeyword says so;
	 *   - punctuation and operators, longest match first (`<<=` before `<<` before `<`);
	 *   - anything else is Unknown, reported as DSH2101, and lexing continues at the next character.
	 *
	 * Returns false when any error was recorded. The token array always ends with EndOfFile.
	 */
	DREAMSHADERLANG_API bool LexDreamShaderLang(
		const FLangSourceText& Source,
		const FLangLexOptions& Options,
		TArray<FLangToken>& OutTokens,
		FLangDiagnosticSink& Diagnostics);
}
