// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Tokens of DreamShaderLang 2.0. One lexer serves both front ends; the legacy front end only
// changes how it interprets identifiers, never how text is cut into tokens.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Lang
{
	enum class ELangTokenKind : uint8
	{
		EndOfFile,

		/** A name that is not a reserved word. Type names, `material`, `UE`, legacy section words are all identifiers. */
		Identifier,
		/** A reserved word; FLangToken::Keyword says which. */
		Keyword,

		/** Decimal, hex (`0x`), optional `u`/`U`/`l`/`L` suffix. Text is the lexeme; Integer holds the value. */
		IntLiteral,
		/** Has a `.`, an exponent, or an `f`/`F`/`h`/`H` suffix. Text is the lexeme; Real holds the value. */
		FloatLiteral,
		/** `"..."` with C escapes already resolved into Text. */
		StringLiteral,

		/** One `///` line. Text is everything after the three slashes, with one leading space removed. */
		DocComment,
		/**
		 * A `#` line: `#pragma ...`, `#include ...`, or anything else that starts with `#` at the
		 * start of a line. Text is everything after the `#`, trimmed, trailing `//` comment removed.
		 * The lexer never judges these; the parser decides which are meaningful where.
		 */
		Directive,

		// -- punctuation
		LeftParen, RightParen, LeftBrace, RightBrace, LeftBracket, RightBracket,
		Comma, Semicolon, Colon, Dot, Question,

		// -- operators, longest match first when lexing
		Plus, Minus, Star, Slash, Percent,
		PlusPlus, MinusMinus,
		Ampersand, Pipe, Caret, Tilde, Bang,
		AmpersandAmpersand, PipePipe,
		Less, Greater, LessEqual, GreaterEqual, EqualEqual, BangEqual,
		LessLess, GreaterGreater,
		Assign,
		PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
		AmpersandAssign, PipeAssign, CaretAssign, LessLessAssign, GreaterGreaterAssign,

		/** A character the lexer could not place. Reported as DSH2101; emitted so the parser fails at the right spot. */
		Unknown,
	};

	/**
	 * Reserved words. Case-sensitive, matching HLSL. Type names are deliberately NOT keywords:
	 * `float3`, `Texture2D`, `material` and user struct names all reach the parser as identifiers,
	 * and the parser decides from context.
	 */
	enum class ELangKeyword : uint8
	{
		None,
		Uniform, Static, Const, Extern, Export,
		In, Out, InOut,
		Struct,
		If, Else, For, While, Do, Return, Break, Continue, Discard,
		True, False,
		/** `import "..."` -- the 1.x spelling of `#include`, kept as an alias in both front ends. */
		Import,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(ELangTokenKind Kind);
	DREAMSHADERLANG_API const TCHAR* LexToString(ELangKeyword Keyword);
	/** True and the keyword when Text is a reserved word (case-sensitive). */
	DREAMSHADERLANG_API bool TryGetLangKeyword(const FString& Text, ELangKeyword& OutKeyword);
	/** The source spelling of a punctuation/operator token kind, e.g. `+=`; empty for the others. */
	DREAMSHADERLANG_API const TCHAR* GetLangTokenSpelling(ELangTokenKind Kind);

	struct FLangToken
	{
		ELangTokenKind Kind = ELangTokenKind::EndOfFile;
		ELangKeyword Keyword = ELangKeyword::None;
		FLangSpan Span;
		/**
		 * The lexeme for identifiers, keywords and numbers; the resolved value for strings; the
		 * payload for DocComment and Directive (see the kinds above). Empty for punctuation.
		 */
		FString Text;
		/** Integer value for IntLiteral (hex already converted). */
		uint64 Integer = 0;
		/** Numeric value for FloatLiteral. */
		double Real = 0.0;
		/** True for an unsigned integer literal (`u`/`U` suffix). */
		bool bUnsigned = false;
		/** True when the token is the first non-whitespace thing on its physical line. */
		bool bAtLineStart = false;

		bool Is(ELangTokenKind InKind) const { return Kind == InKind; }
		bool IsKeyword(ELangKeyword InKeyword) const { return Kind == ELangTokenKind::Keyword && Keyword == InKeyword; }
		bool IsIdentifier(const TCHAR* Name) const { return Kind == ELangTokenKind::Identifier && Text.Equals(Name, ESearchCase::CaseSensitive); }
	};
}
