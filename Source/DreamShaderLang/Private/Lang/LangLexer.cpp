// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShaderLang 2.0 lexer.
//
// One pass, no backtracking, no regular expressions: at every position exactly one of the rules in
// LangLexer.h applies, decided by the current character and at most three of lookahead. Two
// properties matter more than anything else here and every branch below preserves them:
//
//   1. THE LEXER NEVER GIVES UP. Every error emits a token anyway -- a malformed number is still
//      one number token, an unterminated string is still one string token, an unplaceable character
//      is still one Unknown token -- so the parser's cursor stays aligned with the source and one
//      typo does not turn the rest of the file into noise.
//   2. EVERY SPAN IS REAL. Line and column come from FLangSourceText::MakeSpan, never from a local
//      newline count, so a CRLF file and an LF file report identical positions and a diagnostic
//      raised here points at the same physical line the preprocessor reported.
//
// Whitespace tracking exists for one reason: `bAtLineStart`, which is what tells the parser that a
// `///` block is attached to the declaration below it and what makes `#` a directive rather than an
// unknown character. It means "nothing but spaces and tabs has been seen on this physical line
// yet" -- a comment counts as something, which is why `/* c */ #pragma x` is NOT a directive line.

#include "Lang/LangLexer.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/CString.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Lexer"

namespace UE::DreamShader::Lang
{
	namespace Private
	{
		namespace Lexer
		{
			// Deliberately ASCII, not FChar::IsAlpha and friends: the language is ASCII, and a
			// locale- or Unicode-aware classifier would let a full-width digit or a Cyrillic 'a'
			// into an identifier and produce a symbol nothing downstream can match.
			static bool IsDecimalDigit(const TCHAR Character)
			{
				return Character >= TEXT('0') && Character <= TEXT('9');
			}

			static bool IsHexDigit(const TCHAR Character)
			{
				return (Character >= TEXT('0') && Character <= TEXT('9'))
					|| (Character >= TEXT('a') && Character <= TEXT('f'))
					|| (Character >= TEXT('A') && Character <= TEXT('F'));
			}

			static uint64 HexDigitValue(const TCHAR Character)
			{
				if (Character >= TEXT('0') && Character <= TEXT('9'))
				{
					return static_cast<uint64>(Character - TEXT('0'));
				}
				if (Character >= TEXT('a') && Character <= TEXT('f'))
				{
					return static_cast<uint64>(Character - TEXT('a')) + 10;
				}
				return static_cast<uint64>(Character - TEXT('A')) + 10;
			}

			static bool IsIdentifierStart(const TCHAR Character)
			{
				return (Character >= TEXT('a') && Character <= TEXT('z'))
					|| (Character >= TEXT('A') && Character <= TEXT('Z'))
					|| Character == TEXT('_');
			}

			static bool IsIdentifierChar(const TCHAR Character)
			{
				return IsIdentifierStart(Character) || IsDecimalDigit(Character);
			}

			/** `f F h H` make a float, `u U l L` an integer. Anything else after a number is garbage. */
			static bool IsNumericSuffix(const TCHAR Character)
			{
				return Character == TEXT('f') || Character == TEXT('F')
					|| Character == TEXT('h') || Character == TEXT('H')
					|| Character == TEXT('u') || Character == TEXT('U')
					|| Character == TEXT('l') || Character == TEXT('L');
			}

			static bool IsFloatSuffix(const TCHAR Character)
			{
				return Character == TEXT('f') || Character == TEXT('F')
					|| Character == TEXT('h') || Character == TEXT('H');
			}

			static bool IsLineTerminator(const TCHAR Character)
			{
				return Character == TEXT('\n') || Character == TEXT('\r');
			}

			/** Whitespace that does NOT end a line. Vertical tab and form feed are legal HLSL spacing. */
			static bool IsInlineWhitespace(const TCHAR Character)
			{
				return Character == TEXT(' ') || Character == TEXT('\t')
					|| Character == TEXT('\v') || Character == TEXT('\f');
			}

			/**
			 * Punctuation and operators in LONGEST-FIRST order. The lexer walks this table and takes
			 * the first prefix that matches, so `<<=` must be tried before `<<` before `<`; get the
			 * order wrong and `a <<= b` lexes as `<` `<` `=`.
			 *
			 * The spellings themselves are not repeated here: they come from GetLangTokenSpelling,
			 * so the lexer and the printer can never disagree about what an operator looks like.
			 */
			static const ELangTokenKind GOperatorKindsLongestFirst[] =
			{
				// three characters
				ELangTokenKind::LessLessAssign,
				ELangTokenKind::GreaterGreaterAssign,

				// two characters
				ELangTokenKind::PlusPlus,
				ELangTokenKind::MinusMinus,
				ELangTokenKind::AmpersandAmpersand,
				ELangTokenKind::PipePipe,
				ELangTokenKind::EqualEqual,
				ELangTokenKind::BangEqual,
				ELangTokenKind::LessEqual,
				ELangTokenKind::GreaterEqual,
				ELangTokenKind::PlusAssign,
				ELangTokenKind::MinusAssign,
				ELangTokenKind::StarAssign,
				ELangTokenKind::SlashAssign,
				ELangTokenKind::PercentAssign,
				ELangTokenKind::AmpersandAssign,
				ELangTokenKind::PipeAssign,
				ELangTokenKind::CaretAssign,
				ELangTokenKind::LessLess,
				ELangTokenKind::GreaterGreater,

				// one character
				ELangTokenKind::LeftParen,
				ELangTokenKind::RightParen,
				ELangTokenKind::LeftBrace,
				ELangTokenKind::RightBrace,
				ELangTokenKind::LeftBracket,
				ELangTokenKind::RightBracket,
				ELangTokenKind::Comma,
				ELangTokenKind::Semicolon,
				ELangTokenKind::Colon,
				ELangTokenKind::Dot,
				ELangTokenKind::Question,
				ELangTokenKind::Plus,
				ELangTokenKind::Minus,
				ELangTokenKind::Star,
				ELangTokenKind::Slash,
				ELangTokenKind::Percent,
				ELangTokenKind::Ampersand,
				ELangTokenKind::Pipe,
				ELangTokenKind::Caret,
				ELangTokenKind::Tilde,
				ELangTokenKind::Bang,
				ELangTokenKind::Less,
				ELangTokenKind::Greater,
				ELangTokenKind::Assign,
			};

			/**
			 * Cuts a trailing `//` comment off one directive line.
			 *
			 * String-aware on purpose: `#include "/Game/A//B.dsh"` and
			 * `#pragma material(Name = "http://x")` both carry a `//` that is data, not a comment.
			 * Only the top level of the line is scanned; escapes inside the string are honoured so
			 * a `\"` does not close it.
			 */
			static FString StripTrailingLineComment(const FString& Line)
			{
				const TCHAR* const Characters = *Line;
				const int32 Length = Line.Len();

				bool bInString = false;
				for (int32 Index = 0; Index < Length; ++Index)
				{
					const TCHAR Character = Characters[Index];

					if (bInString)
					{
						if (Character == TEXT('\\') && Index + 1 < Length)
						{
							++Index;
						}
						else if (Character == TEXT('"'))
						{
							bInString = false;
						}
						continue;
					}

					if (Character == TEXT('"'))
					{
						bInString = true;
						continue;
					}

					if (Character == TEXT('/') && Index + 1 < Length && Characters[Index + 1] == TEXT('/'))
					{
						return Line.Left(Index);
					}
				}

				return Line;
			}
		}
	}

	bool LexDreamShaderLang(
		const FLangSourceText& Source,
		const FLangLexOptions& Options,
		TArray<FLangToken>& OutTokens,
		FLangDiagnosticSink& Diagnostics)
	{
		using namespace UE::DreamShader::Lang::Private::Lexer;

		// The sink is shared with the parser and may already hold diagnostics from an earlier
		// stage, so the return value is decided by what THIS call added, not by HasErrors().
		const int32 ErrorsOnEntry = Diagnostics.NumErrors();

		OutTokens.Reset();

		const FString& Text = Source.GetText();
		const TCHAR* const Characters = *Text;
		const int32 Length = Text.Len();

		// A rough token every four characters; one reallocation instead of a dozen.
		OutTokens.Reserve((Length / 4) + 8);

		int32 Index = 0;
		bool bAtLineStart = true;

		while (Index < Length)
		{
			const TCHAR Character = Characters[Index];

			// ------------------------------------------------------------------ whitespace
			if (Character == TEXT('\n'))
			{
				++Index;
				bAtLineStart = true;
				continue;
			}
			if (Character == TEXT('\r'))
			{
				++Index;
				// `\r\n` is one break, exactly as FLangSourceText's line table counts it.
				if (Index < Length && Characters[Index] == TEXT('\n'))
				{
					++Index;
				}
				bAtLineStart = true;
				continue;
			}
			if (IsInlineWhitespace(Character))
			{
				++Index;
				continue;
			}

			// ------------------------------------------------------------------ line comments
			if (Character == TEXT('/') && Index + 1 < Length && Characters[Index + 1] == TEXT('/'))
			{
				const int32 CommentStart = Index;

				// `///` is documentation; `//` and `////` are ordinary comments. The `////` case is
				// what lets a decorated separator line -- `//////////////// Parameters ////` -- stay
				// a comment instead of silently becoming a doc block on the next declaration.
				const bool bIsDocComment =
					(Index + 2 < Length && Characters[Index + 2] == TEXT('/')) &&
					!(Index + 3 < Length && Characters[Index + 3] == TEXT('/'));

				int32 LineEnd = Index;
				while (LineEnd < Length && !IsLineTerminator(Characters[LineEnd]))
				{
					++LineEnd;
				}

				if (bIsDocComment && Options.bEmitDocComments)
				{
					FLangToken Token;
					Token.Kind = ELangTokenKind::DocComment;
					Token.Span = Source.MakeSpan(CommentStart, LineEnd - CommentStart);
					Token.bAtLineStart = bAtLineStart;

					// Everything after the three slashes, with ONE leading space removed, so
					// `/// @group A` and `///@group A` carry the same payload and a deliberately
					// indented continuation line keeps its extra spaces.
					int32 PayloadStart = CommentStart + 3;
					if (PayloadStart < LineEnd && Characters[PayloadStart] == TEXT(' '))
					{
						++PayloadStart;
					}
					Token.Text = Text.Mid(PayloadStart, LineEnd - PayloadStart);

					OutTokens.Add(MoveTemp(Token));
				}

				Index = LineEnd;
				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------------------ block comments
			if (Character == TEXT('/') && Index + 1 < Length && Characters[Index + 1] == TEXT('*'))
			{
				const int32 CommentStart = Index;
				Index += 2;

				bool bClosed = false;
				while (Index < Length)
				{
					if (Characters[Index] == TEXT('*') && Index + 1 < Length && Characters[Index + 1] == TEXT('/'))
					{
						Index += 2;
						bClosed = true;
						break;
					}
					++Index;
				}

				if (!bClosed)
				{
					// Reported at the OPENING `/*`: that is the place a reader has to fix, and the
					// end of file says nothing about which comment was left open.
					Diagnostics.Error(TEXT("DSH2102"), Source.MakeSpan(CommentStart, 2),
						LOCTEXT("UnterminatedBlockComment", "Unterminated block comment; expected a closing '*/'."));
					Index = Length;
				}

				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------------------ `#` lines
			if (Character == TEXT('#'))
			{
				if (!bAtLineStart)
				{
					// Not a directive: a preprocessor line always starts its own line. Emitting
					// Unknown here keeps the parser aligned and reports the real mistake once.
					FLangToken Token;
					Token.Kind = ELangTokenKind::Unknown;
					Token.Span = Source.MakeSpan(Index, 1);
					Token.bAtLineStart = false;
					Token.Text = FString::Chr(Character);

					Diagnostics.Error(TEXT("DSH2106"), Token.Span,
						LOCTEXT("HashNotAtLineStart", "A '#' directive must be the first thing on its line."));

					OutTokens.Add(MoveTemp(Token));
					++Index;
					continue;
				}

				const int32 DirectiveStart = Index;
				int32 LineEnd = Index;
				while (LineEnd < Length && !IsLineTerminator(Characters[LineEnd]))
				{
					++LineEnd;
				}

				if (Options.bEmitDirectives)
				{
					FLangToken Token;
					Token.Kind = ELangTokenKind::Directive;
					// The span covers the whole physical line, terminator excluded, so DSH3201 and
					// friends underline the directive and not just its name.
					Token.Span = Source.MakeSpan(DirectiveStart, LineEnd - DirectiveStart);
					Token.bAtLineStart = true;

					const FString Payload = Text.Mid(DirectiveStart + 1, LineEnd - (DirectiveStart + 1));
					Token.Text = StripTrailingLineComment(Payload).TrimStartAndEnd();

					OutTokens.Add(MoveTemp(Token));
				}

				Index = LineEnd;
				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------------------ string literals
			if (Character == TEXT('"'))
			{
				const int32 StringStart = Index;
				++Index; // opening quote

				FString Value;
				bool bTerminated = false;

				while (Index < Length)
				{
					const TCHAR Current = Characters[Index];

					// A string never spans a line: stopping here means the rest of the file still
					// lexes as code instead of being swallowed by a runaway literal.
					if (IsLineTerminator(Current))
					{
						break;
					}

					if (Current == TEXT('"'))
					{
						++Index;
						bTerminated = true;
						break;
					}

					if (Current == TEXT('\\'))
					{
						if (Index + 1 >= Length || IsLineTerminator(Characters[Index + 1]))
						{
							// A backslash at the very end of the line: the unterminated report
							// below is the useful message, so just consume it.
							++Index;
							continue;
						}

						const TCHAR Escape = Characters[Index + 1];
						switch (Escape)
						{
						case TEXT('\\'):
							Value.AppendChar(TEXT('\\'));
							break;
						case TEXT('"'):
							Value.AppendChar(TEXT('"'));
							break;
						case TEXT('n'):
							Value.AppendChar(TEXT('\n'));
							break;
						case TEXT('r'):
							Value.AppendChar(TEXT('\r'));
							break;
						case TEXT('t'):
							Value.AppendChar(TEXT('\t'));
							break;
						case TEXT('0'):
							// FString::AppendChar drops a null character (String.cpp: `if (InChar
							// != 0)`), so `\0` resolves to nothing. An FString cannot carry an
							// embedded NUL and no DreamShader string ever needs one.
							Value.AppendChar(TEXT('\0'));
							break;
						default:
							Diagnostics.Error(TEXT("DSH2104"), Source.MakeSpan(Index, 2), FText::Format(
								LOCTEXT("UnknownStringEscape", "Unknown escape sequence '\\{0}' in a string literal."),
								FText::FromString(FString::Chr(Escape))));
							// Keep the backslash: the value then still shows what was written, and
							// a caller round-tripping the text does not silently lose a character.
							Value.AppendChar(TEXT('\\'));
							Value.AppendChar(Escape);
							break;
						}

						Index += 2;
						continue;
					}

					Value.AppendChar(Current);
					++Index;
				}

				if (!bTerminated)
				{
					Diagnostics.Error(TEXT("DSH2103"), Source.MakeSpan(StringStart, 1),
						LOCTEXT("UnterminatedStringLiteral", "Unterminated string literal; expected a closing '\"'."));
				}

				FLangToken Token;
				Token.Kind = ELangTokenKind::StringLiteral;
				Token.Span = Source.MakeSpan(StringStart, Index - StringStart);
				Token.bAtLineStart = bAtLineStart;
				Token.Text = MoveTemp(Value);

				OutTokens.Add(MoveTemp(Token));
				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------------------ numbers
			if (IsDecimalDigit(Character) ||
				(Character == TEXT('.') && Index + 1 < Length && IsDecimalDigit(Characters[Index + 1])))
			{
				const int32 NumberStart = Index;

				bool bHex = false;
				bool bHasFractionOrExponent = false;
				bool bMalformed = false;
				uint64 IntegerValue = 0;

				if (Character == TEXT('0') && Index + 1 < Length &&
					(Characters[Index + 1] == TEXT('x') || Characters[Index + 1] == TEXT('X')))
				{
					bHex = true;
					Index += 2;

					const int32 DigitsStart = Index;
					while (Index < Length && IsHexDigit(Characters[Index]))
					{
						// Wraps on overflow rather than saturating; a literal that big is a source
						// bug the semantic pass will judge, not something the lexer should refuse.
						IntegerValue = (IntegerValue * 16) + HexDigitValue(Characters[Index]);
						++Index;
					}

					if (Index == DigitsStart)
					{
						bMalformed = true; // `0x` with nothing after it
					}
				}
				else
				{
					while (Index < Length && IsDecimalDigit(Characters[Index]))
					{
						IntegerValue = (IntegerValue * 10) + static_cast<uint64>(Characters[Index] - TEXT('0'));
						++Index;
					}

					// `1.`, `1.5` and `.5` are all floats. A `.` with no digits after it is still a
					// float, exactly like C.
					if (Index < Length && Characters[Index] == TEXT('.'))
					{
						bHasFractionOrExponent = true;
						++Index;
						while (Index < Length && IsDecimalDigit(Characters[Index]))
						{
							++Index;
						}
					}

					if (Index < Length && (Characters[Index] == TEXT('e') || Characters[Index] == TEXT('E')))
					{
						int32 Probe = Index + 1;
						if (Probe < Length && (Characters[Probe] == TEXT('+') || Characters[Probe] == TEXT('-')))
						{
							++Probe;
						}
						const bool bHasExponentDigits = (Probe < Length && IsDecimalDigit(Characters[Probe]));

						// `1e` is malformed but is still consumed as ONE token: splitting it into
						// `1` and the identifier `e` would hand the parser a plausible-looking
						// expression and hide the real mistake.
						bHasFractionOrExponent = true;
						Index = Probe;
						if (bHasExponentDigits)
						{
							while (Index < Length && IsDecimalDigit(Characters[Index]))
							{
								++Index;
							}
						}
						else
						{
							bMalformed = true;
						}
					}
				}

				// Suffixes. In hex, `f` and `F` were already eaten as digits, so `0xff` is not a
				// float and `0x1u` still finds its `u` here.
				bool bHasFloatSuffix = false;
				bool bHasIntegerSuffix = false;
				bool bUnsigned = false;
				while (Index < Length && IsNumericSuffix(Characters[Index]))
				{
					const TCHAR Suffix = Characters[Index];
					if (IsFloatSuffix(Suffix))
					{
						bHasFloatSuffix = true;
					}
					else
					{
						bHasIntegerSuffix = true;
						if (Suffix == TEXT('u') || Suffix == TEXT('U'))
						{
							bUnsigned = true;
						}
					}
					++Index;
				}

				// Anything still glued to the number -- `1.2.3`, `1abc`, `0x1.5` -- belongs to this
				// token, not to the next one.
				if (Index < Length && (IsIdentifierChar(Characters[Index]) || Characters[Index] == TEXT('.')))
				{
					bMalformed = true;
					while (Index < Length && (IsIdentifierChar(Characters[Index]) || Characters[Index] == TEXT('.')))
					{
						++Index;
					}
				}

				if (bHasFloatSuffix && bHasIntegerSuffix)
				{
					bMalformed = true; // `1fu`
				}
				if (bHasIntegerSuffix && bHasFractionOrExponent)
				{
					bMalformed = true; // `1.0u`
				}
				if (bHex && bHasFloatSuffix)
				{
					bMalformed = true; // `0x1h`
				}

				// Hex is always an integer; a decimal is a float as soon as it has a fraction, an
				// exponent or an `f`/`h` suffix.
				const bool bIsFloat = !bHex && (bHasFractionOrExponent || bHasFloatSuffix);

				FLangToken Token;
				Token.Kind = bIsFloat ? ELangTokenKind::FloatLiteral : ELangTokenKind::IntLiteral;
				Token.Span = Source.MakeSpan(NumberStart, Index - NumberStart);
				Token.bAtLineStart = bAtLineStart;
				// The RAW lexeme, suffix included, so the printer writes `1.0f`, `0x10` and `2u`
				// back exactly as they were authored.
				Token.Text = Text.Mid(NumberStart, Index - NumberStart);

				if (bIsFloat)
				{
					// Atod stops at the first character it cannot use, which is exactly right for a
					// trailing `f`/`h` suffix and for the malformed forms (`1e` reads as 1).
					Token.Real = FCString::Atod(*Token.Text);
				}
				else
				{
					Token.Integer = IntegerValue;
					Token.bUnsigned = bUnsigned;
				}

				if (bMalformed)
				{
					Diagnostics.Error(TEXT("DSH2105"), Token.Span, FText::Format(
						LOCTEXT("MalformedNumber", "Malformed number literal '{0}'."),
						FText::FromString(Token.Text)));
				}

				OutTokens.Add(MoveTemp(Token));
				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------------------ identifiers
			if (IsIdentifierStart(Character))
			{
				const int32 IdentifierStart = Index;
				while (Index < Length && IsIdentifierChar(Characters[Index]))
				{
					++Index;
				}

				FLangToken Token;
				Token.Span = Source.MakeSpan(IdentifierStart, Index - IdentifierStart);
				Token.bAtLineStart = bAtLineStart;
				Token.Text = Text.Mid(IdentifierStart, Index - IdentifierStart);

				ELangKeyword Keyword = ELangKeyword::None;
				if (TryGetLangKeyword(Token.Text, Keyword))
				{
					Token.Kind = ELangTokenKind::Keyword;
					Token.Keyword = Keyword;
				}
				else
				{
					// Type names, `material`, `UE`, `Substrate` and every legacy section word land
					// here; only the parser knows what they mean.
					Token.Kind = ELangTokenKind::Identifier;
				}

				OutTokens.Add(MoveTemp(Token));
				bAtLineStart = false;
				continue;
			}

			// ------------------------------------------------------- operators and punctuation
			{
				bool bMatched = false;
				for (const ELangTokenKind OperatorKind : GOperatorKindsLongestFirst)
				{
					const TCHAR* const Spelling = GetLangTokenSpelling(OperatorKind);
					const int32 SpellingLength = FCString::Strlen(Spelling);

					if (SpellingLength > 0 && Index + SpellingLength <= Length &&
						FCString::Strncmp(Characters + Index, Spelling, static_cast<SIZE_T>(SpellingLength)) == 0)
					{
						FLangToken Token;
						Token.Kind = OperatorKind;
						Token.Span = Source.MakeSpan(Index, SpellingLength);
						Token.bAtLineStart = bAtLineStart;
						// Text stays empty for punctuation: GetLangTokenSpelling already says what
						// it looks like, and an empty Text is how a consumer tells the two apart.
						OutTokens.Add(MoveTemp(Token));

						Index += SpellingLength;
						bAtLineStart = false;
						bMatched = true;
						break;
					}
				}

				if (bMatched)
				{
					continue;
				}
			}

			// ------------------------------------------------------------------ anything else
			{
				FLangToken Token;
				Token.Kind = ELangTokenKind::Unknown;
				Token.Span = Source.MakeSpan(Index, 1);
				Token.bAtLineStart = bAtLineStart;
				Token.Text = FString::Chr(Character);

				Diagnostics.Error(TEXT("DSH2101"), Token.Span, FText::Format(
					LOCTEXT("UnknownCharacter", "Unexpected character '{0}' in source."),
					FText::FromString(Token.Text)));

				OutTokens.Add(MoveTemp(Token));
				++Index;
				bAtLineStart = false;
			}
		}

		// The array ALWAYS ends with EndOfFile, empty span at the end of the text, so the parser's
		// cursor has something to sit on and Peek() past the last real token is always valid.
		{
			FLangToken EndToken;
			EndToken.Kind = ELangTokenKind::EndOfFile;
			EndToken.Span = Source.MakeSpan(Length, 0);
			EndToken.bAtLineStart = bAtLineStart;
			OutTokens.Add(MoveTemp(EndToken));
		}

		return Diagnostics.NumErrors() == ErrorsOnEntry;
	}
}

#undef LOCTEXT_NAMESPACE
