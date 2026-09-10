// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderPreprocessorExpression.h"

// For EvaluateDreamShaderConditionExpression's declaration at the bottom of this file. That public
// wrapper lives here rather than in the scanner's translation unit because it is the EVALUATOR's
// public face -- it does nothing the scanner does, and putting it next to the scanner would suggest
// otherwise.
#include "DreamShaderPreprocessor.h"

#include "Internationalization/Text.h"
#include "Math/NumericLimits.h"

#define LOCTEXT_NAMESPACE "DreamShader.Preprocessor.Expression"

namespace UE::DreamShader::Private
{
	namespace
	{
		// -------------------------------------------------------------------------------------------
		// Diagnostics.
		// -------------------------------------------------------------------------------------------

		/** Where the condition being evaluated lives. Carried around only to build messages. */
		struct FConditionSite
		{
			/** `#if` or `#elif`, so a message names the directive the reader is looking at. */
			const TCHAR* Directive = TEXT("#if");
			/** Never null in practice; a pointer only so the struct stays copyable. */
			const FString* FilePath = nullptr;
			/** 1-based physical line of the directive. */
			int32 Line = 0;
		};

		/**
		 * A line number is an identifier, not a quantity.
		 *
		 * FText::AsNumber's default grouping would print line 1234 as "1,234", and the `file(line):`
		 * shape the rest of the plugin emits is parsed by editors that expect bare digits there.
		 */
		FText FormatPlainNumber(const int32 InNumber)
		{
			return FText::AsNumber(InNumber, &FNumberFormattingOptions::DefaultNoGrouping());
		}

		/**
		 * Every DSH1034 raised while evaluating a condition comes through here, and the same holds for
		 * DSH1040 and DSH1041 below.
		 *
		 * One raise site per code is deliberate. .skill/gen-diagnostics.ps1 documents a code from the
		 * message literal it finds at the raise, so a code raised from six places with six wordings
		 * would be documented by whichever the scanner happened to reach first. The part that varies
		 * arrives as {3} instead, which is where it belongs: it is detail, not identity.
		 */
		bool FailConditionSyntax(const FConditionSite& Site, const FText& Detail, FDreamShaderTextError& OutError)
		{
			return FailWith(OutError, TEXT("DSH1034"), FText::Format(
				LOCTEXT("InvalidConditionExpression", "{0}({1}): invalid '{2}' condition: {3}"),
				FText::FromString(*Site.FilePath),
				FormatPlainNumber(Site.Line),
				FText::FromString(Site.Directive),
				Detail));
		}

		bool FailMissingCondition(const FConditionSite& Site, FDreamShaderTextError& OutError)
		{
			return FailWith(OutError, TEXT("DSH1036"), FText::Format(
				LOCTEXT("MissingConditionExpression", "{0}({1}): '{2}' requires a condition expression."),
				FText::FromString(*Site.FilePath),
				FormatPlainNumber(Site.Line),
				FText::FromString(Site.Directive)));
		}

		bool FailConditionType(const FConditionSite& Site, const FText& Detail, FDreamShaderTextError& OutError)
		{
			return FailWith(OutError, TEXT("DSH1040"), FText::Format(
				LOCTEXT("ConditionTypeMismatch", "{0}({1}): type mismatch in '{2}' condition: {3}"),
				FText::FromString(*Site.FilePath),
				FormatPlainNumber(Site.Line),
				FText::FromString(Site.Directive),
				Detail));
		}

		bool FailConditionDivideByZero(const FConditionSite& Site, const TCHAR* InOperator, FDreamShaderTextError& OutError)
		{
			return FailWith(OutError, TEXT("DSH1041"), FText::Format(
				LOCTEXT("ConditionDivideByZero", "{0}({1}): the right operand of '{2}' in this '{3}' condition is zero."),
				FText::FromString(*Site.FilePath),
				FormatPlainNumber(Site.Line),
				FText::FromString(InOperator),
				FText::FromString(Site.Directive)));
		}

		// -------------------------------------------------------------------------------------------
		// Values.
		// -------------------------------------------------------------------------------------------

		/**
		 * The two-type domain the grammar admits: a 64-bit signed integer (booleans are 0 and 1) and
		 * a string.
		 *
		 * Strings exist for one reason -- `DS_PLATFORM == "Windows"` -- so they take part in equality
		 * and nothing else. Every other operator refuses them rather than coercing, because the
		 * coercions C would apply here (a string decaying to a pointer, then to a nonzero number) turn
		 * a typo into a branch that silently always fires.
		 */
		struct FValue
		{
			bool bIsString = false;
			int64 Integer = 0;
			FString String;

			static FValue MakeInteger(const int64 InValue)
			{
				FValue Value;
				Value.Integer = InValue;
				return Value;
			}

			static FValue MakeBoolean(const bool bInValue)
			{
				return MakeInteger(bInValue ? 1 : 0);
			}

			static FValue MakeString(const FString& InValue)
			{
				FValue Value;
				Value.bIsString = true;
				Value.String = InValue;
				return Value;
			}
		};

		/**
		 * Parses a whole string as an integer literal: decimal or `0x` hexadecimal, optional leading
		 * sign. Returns false for anything else, including a trailing remainder.
		 *
		 * Whole-string and not prefix, because this is also what decides whether a define's VALUE is a
		 * number or a string: `1abc` has to come out a string rather than the number 1 with the rest
		 * quietly dropped.
		 *
		 * Base is spelled out instead of handing the job to Strtoi64 with base 0, which would also
		 * accept C's octal -- and a switch written `#define MASK 0755` meaning 493 is a trap nobody
		 * asked for in a language that has no octal anywhere else.
		 */
		bool TryParsePreprocessorInteger(const FString& InText, int64& OutValue)
		{
			int32 Index = 0;
			const int32 Length = InText.Len();

			bool bNegative = false;
			if (Index < Length && (InText[Index] == TCHAR('+') || InText[Index] == TCHAR('-')))
			{
				bNegative = InText[Index] == TCHAR('-');
				++Index;
			}

			uint64 Base = 10;
			if (Index + 1 < Length
				&& InText[Index] == TCHAR('0')
				&& (InText[Index + 1] == TCHAR('x') || InText[Index + 1] == TCHAR('X')))
			{
				Base = 16;
				Index += 2;
			}

			// `0x` on its own, an empty string, or a lone sign: no digits means no literal.
			if (Index >= Length)
			{
				return false;
			}

			// The magnitude is accumulated unsigned so the range check below can be exact for both
			// signs: the most negative int64 has a magnitude one larger than the most positive.
			const uint64 Limit = bNegative
				? static_cast<uint64>(MIN_int64)
				: static_cast<uint64>(MAX_int64);

			uint64 Magnitude = 0;
			for (; Index < Length; ++Index)
			{
				const TCHAR Character = InText[Index];

				uint64 Digit = 0;
				if (Character >= TCHAR('0') && Character <= TCHAR('9'))
				{
					Digit = static_cast<uint64>(Character - TCHAR('0'));
				}
				else if (Base == 16 && Character >= TCHAR('a') && Character <= TCHAR('f'))
				{
					Digit = 10 + static_cast<uint64>(Character - TCHAR('a'));
				}
				else if (Base == 16 && Character >= TCHAR('A') && Character <= TCHAR('F'))
				{
					Digit = 10 + static_cast<uint64>(Character - TCHAR('A'));
				}
				else
				{
					// Also where a hex digit in a decimal literal lands, and where the run the
					// tokenizer handed over stops being a literal at all: `12abc` fails here.
					return false;
				}

				// Refused rather than wrapped: a literal that does not fit is a mistake, and folding it
				// modulo 2^64 would make which branch compiles depend on arithmetic nobody wrote.
				if (Magnitude > (Limit - Digit) / Base)
				{
					return false;
				}
				Magnitude = Magnitude * Base + Digit;
			}

			// Negating through uint64: -MIN_int64 has no int64 answer, and writing it as `-(int64)M`
			// would be undefined behaviour for exactly the value this branch exists to accept.
			OutValue = bNegative
				? static_cast<int64>(~Magnitude + 1)
				: static_cast<int64>(Magnitude);
			return true;
		}

		// -------------------------------------------------------------------------------------------
		// Tokens.
		// -------------------------------------------------------------------------------------------

		enum class ETokenKind : uint8
		{
			End,
			Identifier,
			Integer,
			String,
			Operator,
		};

		struct FToken
		{
			ETokenKind Kind = ETokenKind::End;

			/**
			 * How the token was written: the name, the operator, the digits, or the quoted literal
			 * including its quotes.
			 *
			 * Kept for every kind so a message can show what the author typed rather than what the
			 * evaluator made of it -- "unexpected '0x1F'" beats "unexpected '31'".
			 */
			FString Spelling;

			/** String tokens only: the literal with its escapes resolved. */
			FString StringValue;

			/** Integer tokens only. */
			int64 Integer = 0;
		};

		FText DescribeToken(const FToken& InToken)
		{
			if (InToken.Kind == ETokenKind::End)
			{
				return LOCTEXT("TokenEndOfCondition", "the end of the condition");
			}

			return FText::Format(LOCTEXT("TokenSpelling", "'{0}'"), FText::FromString(InToken.Spelling));
		}

		bool TokenizeCondition(
			const FString& InExpression,
			const FConditionSite& Site,
			TArray<FToken>& OutTokens,
			FDreamShaderTextError& OutError)
		{
			// Two-character operators are tried first so `<=` never tokenizes as `<` followed by a
			// stray `=`, which would report the wrong thing about a perfectly ordinary comparison.
			static const TCHAR* const MultiCharacterOperators[] =
			{
				TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">="), TEXT("&&"), TEXT("||")
			};
			static const TCHAR SingleCharacterOperators[] =
			{
				TCHAR('!'), TCHAR('<'), TCHAR('>'), TCHAR('+'), TCHAR('-'),
				TCHAR('*'), TCHAR('/'), TCHAR('%'), TCHAR('('), TCHAR(')')
			};

			int32 Index = 0;
			const int32 Length = InExpression.Len();

			while (Index < Length)
			{
				const TCHAR Character = InExpression[Index];

				if (FChar::IsWhitespace(Character))
				{
					++Index;
					continue;
				}

				if (FChar::IsAlpha(Character) || Character == TCHAR('_'))
				{
					const int32 Start = Index;
					while (Index < Length && (FChar::IsAlnum(InExpression[Index]) || InExpression[Index] == TCHAR('_')))
					{
						++Index;
					}

					FToken& Token = OutTokens.AddDefaulted_GetRef();
					Token.Kind = ETokenKind::Identifier;
					Token.Spelling = InExpression.Mid(Start, Index - Start);
					continue;
				}

				if (FChar::IsDigit(Character))
				{
					// The run swallows every character a literal could contain, hex digits included, so
					// `0xZZ` and `12abc` arrive whole and fail as one bad literal. Stopping at the first
					// non-digit instead would tokenize them as a number followed by a name and report
					// "unexpected token 'abc'", which points at the wrong half of the mistake.
					const int32 Start = Index;
					while (Index < Length && (FChar::IsAlnum(InExpression[Index]) || InExpression[Index] == TCHAR('_')))
					{
						++Index;
					}

					const FString Literal = InExpression.Mid(Start, Index - Start);
					int64 IntegerValue = 0;
					if (!TryParsePreprocessorInteger(Literal, IntegerValue))
					{
						return FailConditionSyntax(Site, FText::Format(
							LOCTEXT("BadIntegerLiteral", "'{0}' is not a valid integer literal (decimal, or 0x hexadecimal)."),
							FText::FromString(Literal)), OutError);
					}

					FToken& Token = OutTokens.AddDefaulted_GetRef();
					Token.Kind = ETokenKind::Integer;
					Token.Spelling = Literal;
					Token.Integer = IntegerValue;
					continue;
				}

				if (Character == TCHAR('"'))
				{
					const int32 Start = Index;
					++Index;

					FString Unescaped;
					bool bTerminated = false;
					while (Index < Length)
					{
						const TCHAR Current = InExpression[Index];

						if (Current == TCHAR('\\') && Index + 1 < Length)
						{
							// `\"` and `\\` are what the syntax promises; the rest follow
							// UnescapeDreamShaderStringLiteral so the two dialects of string literal a
							// reader meets in one file do not disagree about what a backslash means.
							const TCHAR Escaped = InExpression[Index + 1];
							switch (Escaped)
							{
							case TCHAR('n'):
								Unescaped.AppendChar(TCHAR('\n'));
								break;
							case TCHAR('r'):
								Unescaped.AppendChar(TCHAR('\r'));
								break;
							case TCHAR('t'):
								Unescaped.AppendChar(TCHAR('\t'));
								break;
							default:
								Unescaped.AppendChar(Escaped);
								break;
							}
							Index += 2;
							continue;
						}

						if (Current == TCHAR('"'))
						{
							bTerminated = true;
							++Index;
							break;
						}

						Unescaped.AppendChar(Current);
						++Index;
					}

					if (!bTerminated)
					{
						return FailConditionSyntax(Site,
							LOCTEXT("UnterminatedConditionString", "unterminated string literal."), OutError);
					}

					FToken& Token = OutTokens.AddDefaulted_GetRef();
					Token.Kind = ETokenKind::String;
					Token.Spelling = InExpression.Mid(Start, Index - Start);
					Token.StringValue = Unescaped;
					continue;
				}

				bool bMatchedOperator = false;
				for (const TCHAR* const Candidate : MultiCharacterOperators)
				{
					if (FCString::Strncmp(*InExpression + Index, Candidate, 2) == 0)
					{
						FToken& Token = OutTokens.AddDefaulted_GetRef();
						Token.Kind = ETokenKind::Operator;
						Token.Spelling = Candidate;
						Index += 2;
						bMatchedOperator = true;
						break;
					}
				}
				if (bMatchedOperator)
				{
					continue;
				}

				for (const TCHAR Candidate : SingleCharacterOperators)
				{
					if (Character == Candidate)
					{
						FToken& Token = OutTokens.AddDefaulted_GetRef();
						Token.Kind = ETokenKind::Operator;
						Token.Spelling = FString::ChrN(1, Candidate);
						++Index;
						bMatchedOperator = true;
						break;
					}
				}
				if (bMatchedOperator)
				{
					continue;
				}

				// A lone `&` or `|` lands here, which is the point: bitwise operators are not in the
				// grammar, and accepting `A & B` as something else would be worse than refusing it.
				return FailConditionSyntax(Site, FText::Format(
					LOCTEXT("UnexpectedCharacter", "unexpected character '{0}'."),
					FText::FromString(FString::ChrN(1, Character))), OutError);
			}

			FToken& EndToken = OutTokens.AddDefaulted_GetRef();
			EndToken.Kind = ETokenKind::End;
			return true;
		}

		// -------------------------------------------------------------------------------------------
		// Recursive-descent parser, evaluating as it goes.
		//
		// Every level carries a bEvaluate flag rather than building a tree first. That flag is how
		// short-circuiting is expressed: the right operand of a `&&` whose left side is false is still
		// PARSED -- so `#if 0 && (` is still a syntax error, as it is in C -- but nothing in it is
		// read, divided or type-checked. Skipping the parse instead would let a dead operand rot, and
		// evaluating it anyway would put its defines in the touched set and make the build key depend
		// on names that provably cannot change the output.
		// -------------------------------------------------------------------------------------------

		class FConditionParser
		{
		public:
			FConditionParser(
				const TArray<FToken>& InTokens,
				const FConditionSite& InSite,
				FDreamShaderPreprocessorDefineReader InReadDefine)
				: Tokens(InTokens)
				, Site(InSite)
				, ReadDefine(InReadDefine)
			{
			}

			bool ParseExpression(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				return ParseOr(bEvaluate, OutValue, OutError);
			}

			const FToken& Peek() const
			{
				return Tokens[Index];
			}

			bool IsAtEnd() const
			{
				return Peek().Kind == ETokenKind::End;
			}

		private:
			void Advance()
			{
				// The End token is a wall, not a step: every level stops on it, so clamping here means
				// no path can walk off the array even when a production bails out mid-way.
				if (Index + 1 < Tokens.Num())
				{
					++Index;
				}
			}

			bool MatchOperator(const TCHAR* InSpelling)
			{
				const FToken& Token = Peek();
				if (Token.Kind == ETokenKind::Operator && Token.Spelling.Equals(InSpelling, ESearchCase::CaseSensitive))
				{
					Advance();
					return true;
				}
				return false;
			}

			/** Truthiness for the logical operators. A string has none, and saying so beats guessing. */
			bool RequireTruth(const FValue& InValue, const TCHAR* InOperator, bool& bOutTruth, FDreamShaderTextError& OutError) const
			{
				if (InValue.bIsString)
				{
					return FailConditionType(Site, FText::Format(
						LOCTEXT("StringAsTruthValue", "'{0}' needs a number, but one operand is the string \"{1}\"."),
						FText::FromString(InOperator),
						FText::FromString(InValue.String)), OutError);
				}

				bOutTruth = InValue.Integer != 0;
				return true;
			}

			bool RequireInteger(const FValue& InValue, const TCHAR* InOperator, int64& OutInteger, FDreamShaderTextError& OutError) const
			{
				if (InValue.bIsString)
				{
					return FailConditionType(Site, FText::Format(
						LOCTEXT("StringInNumericOperator", "'{0}' is only defined for numbers, but one operand is the string \"{1}\". Strings compare only with '==' and '!='."),
						FText::FromString(InOperator),
						FText::FromString(InValue.String)), OutError);
				}

				OutInteger = InValue.Integer;
				return true;
			}

			bool ParseOr(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseAnd(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (MatchOperator(TEXT("||")))
				{
					bool bLeftTruth = false;
					if (bEvaluate && !RequireTruth(OutValue, TEXT("||"), bLeftTruth, OutError))
					{
						return false;
					}

					FValue Right;
					if (!ParseAnd(bEvaluate && !bLeftTruth, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeBoolean(false);
						continue;
					}

					if (bLeftTruth)
					{
						OutValue = FValue::MakeBoolean(true);
						continue;
					}

					bool bRightTruth = false;
					if (!RequireTruth(Right, TEXT("||"), bRightTruth, OutError))
					{
						return false;
					}
					OutValue = FValue::MakeBoolean(bRightTruth);
				}

				return true;
			}

			bool ParseAnd(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseEquality(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (MatchOperator(TEXT("&&")))
				{
					bool bLeftTruth = false;
					if (bEvaluate && !RequireTruth(OutValue, TEXT("&&"), bLeftTruth, OutError))
					{
						return false;
					}

					FValue Right;
					if (!ParseEquality(bEvaluate && bLeftTruth, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeBoolean(false);
						continue;
					}

					if (!bLeftTruth)
					{
						OutValue = FValue::MakeBoolean(false);
						continue;
					}

					bool bRightTruth = false;
					if (!RequireTruth(Right, TEXT("&&"), bRightTruth, OutError))
					{
						return false;
					}
					OutValue = FValue::MakeBoolean(bRightTruth);
				}

				return true;
			}

			bool ParseEquality(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseRelational(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (true)
				{
					bool bIsEquals = false;
					if (MatchOperator(TEXT("==")))
					{
						bIsEquals = true;
					}
					else if (!MatchOperator(TEXT("!=")))
					{
						break;
					}

					const TCHAR* const Spelling = bIsEquals ? TEXT("==") : TEXT("!=");

					FValue Right;
					if (!ParseRelational(bEvaluate, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeBoolean(false);
						continue;
					}

					if (OutValue.bIsString != Right.bIsString)
					{
						// Not silently false. Comparing a string against a number is a mistake in the
						// source -- almost always a missing pair of quotes -- and answering "not equal"
						// would let the whole conditional read as deliberate.
						return FailConditionType(Site, FText::Format(
							LOCTEXT("MixedEqualityOperands", "'{0}' cannot compare a string with a number."),
							FText::FromString(Spelling)), OutError);
					}

					const bool bEqual = OutValue.bIsString
						? OutValue.String.Equals(Right.String, ESearchCase::CaseSensitive)
						: OutValue.Integer == Right.Integer;
					OutValue = FValue::MakeBoolean(bIsEquals ? bEqual : !bEqual);
				}

				return true;
			}

			bool ParseRelational(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseAdditive(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (true)
				{
					// `<=` and `>=` are matched before `<` and `>` for the same reason the tokenizer
					// orders them that way, even though the tokenizer has already settled it.
					const TCHAR* Spelling = nullptr;
					if (MatchOperator(TEXT("<=")))
					{
						Spelling = TEXT("<=");
					}
					else if (MatchOperator(TEXT(">=")))
					{
						Spelling = TEXT(">=");
					}
					else if (MatchOperator(TEXT("<")))
					{
						Spelling = TEXT("<");
					}
					else if (MatchOperator(TEXT(">")))
					{
						Spelling = TEXT(">");
					}
					else
					{
						break;
					}

					FValue Right;
					if (!ParseAdditive(bEvaluate, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeBoolean(false);
						continue;
					}

					int64 Left = 0;
					int64 RightInteger = 0;
					if (!RequireInteger(OutValue, Spelling, Left, OutError)
						|| !RequireInteger(Right, Spelling, RightInteger, OutError))
					{
						return false;
					}

					bool bResult = false;
					if (FCString::Strcmp(Spelling, TEXT("<=")) == 0)
					{
						bResult = Left <= RightInteger;
					}
					else if (FCString::Strcmp(Spelling, TEXT(">=")) == 0)
					{
						bResult = Left >= RightInteger;
					}
					else if (FCString::Strcmp(Spelling, TEXT("<")) == 0)
					{
						bResult = Left < RightInteger;
					}
					else
					{
						bResult = Left > RightInteger;
					}

					OutValue = FValue::MakeBoolean(bResult);
				}

				return true;
			}

			bool ParseAdditive(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseMultiplicative(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (true)
				{
					bool bIsAdd = false;
					if (MatchOperator(TEXT("+")))
					{
						bIsAdd = true;
					}
					else if (!MatchOperator(TEXT("-")))
					{
						break;
					}

					const TCHAR* const Spelling = bIsAdd ? TEXT("+") : TEXT("-");

					FValue Right;
					if (!ParseMultiplicative(bEvaluate, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeInteger(0);
						continue;
					}

					int64 Left = 0;
					int64 RightInteger = 0;
					if (!RequireInteger(OutValue, Spelling, Left, OutError)
						|| !RequireInteger(Right, Spelling, RightInteger, OutError))
					{
						return false;
					}

					// Through uint64: signed overflow is undefined behaviour, and a version comparison
					// is not worth a sanitizer trap or a compiler that optimizes on the assumption it
					// cannot happen. Unsigned wraparound is defined and is what the author would see on
					// any two's-complement machine anyway.
					const uint64 Wrapped = bIsAdd
						? static_cast<uint64>(Left) + static_cast<uint64>(RightInteger)
						: static_cast<uint64>(Left) - static_cast<uint64>(RightInteger);
					OutValue = FValue::MakeInteger(static_cast<int64>(Wrapped));
				}

				return true;
			}

			bool ParseMultiplicative(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (!ParseUnary(bEvaluate, OutValue, OutError))
				{
					return false;
				}

				while (true)
				{
					const TCHAR* Spelling = nullptr;
					if (MatchOperator(TEXT("*")))
					{
						Spelling = TEXT("*");
					}
					else if (MatchOperator(TEXT("/")))
					{
						Spelling = TEXT("/");
					}
					else if (MatchOperator(TEXT("%")))
					{
						Spelling = TEXT("%");
					}
					else
					{
						break;
					}

					FValue Right;
					if (!ParseUnary(bEvaluate, Right, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeInteger(0);
						continue;
					}

					int64 Left = 0;
					int64 RightInteger = 0;
					if (!RequireInteger(OutValue, Spelling, Left, OutError)
						|| !RequireInteger(Right, Spelling, RightInteger, OutError))
					{
						return false;
					}

					const bool bIsMultiply = FCString::Strcmp(Spelling, TEXT("*")) == 0;
					if (bIsMultiply)
					{
						const uint64 Wrapped = static_cast<uint64>(Left) * static_cast<uint64>(RightInteger);
						OutValue = FValue::MakeInteger(static_cast<int64>(Wrapped));
						continue;
					}

					if (RightInteger == 0)
					{
						return FailConditionDivideByZero(Site, Spelling, OutError);
					}

					const bool bIsDivide = FCString::Strcmp(Spelling, TEXT("/")) == 0;

					// MIN_int64 / -1 has no representable answer, and on x86 it faults rather than
					// wrapping. Nothing in a compile switch justifies taking down the editor, so it is
					// answered the way the wraparound above answers everything else.
					if (Left == MIN_int64 && RightInteger == -1)
					{
						OutValue = FValue::MakeInteger(bIsDivide ? MIN_int64 : 0);
						continue;
					}

					OutValue = FValue::MakeInteger(bIsDivide ? (Left / RightInteger) : (Left % RightInteger));
				}

				return true;
			}

			bool ParseUnary(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				// Recursing rather than taking at most one prefix: the grammar in the design writes a
				// single optional sign, but `!!FOO` and `- -1` cost nothing to accept and refusing them
				// would be a rule a reader has to discover by hitting it.
				if (MatchOperator(TEXT("!")))
				{
					FValue Operand;
					if (!ParseUnary(bEvaluate, Operand, OutError))
					{
						return false;
					}

					if (!bEvaluate)
					{
						OutValue = FValue::MakeBoolean(false);
						return true;
					}

					bool bTruth = false;
					if (!RequireTruth(Operand, TEXT("!"), bTruth, OutError))
					{
						return false;
					}

					OutValue = FValue::MakeBoolean(!bTruth);
					return true;
				}

				bool bNegate = false;
				if (MatchOperator(TEXT("-")))
				{
					bNegate = true;
				}
				else if (!MatchOperator(TEXT("+")))
				{
					return ParsePrimary(bEvaluate, OutValue, OutError);
				}

				const TCHAR* const Spelling = bNegate ? TEXT("-") : TEXT("+");

				FValue Operand;
				if (!ParseUnary(bEvaluate, Operand, OutError))
				{
					return false;
				}

				if (!bEvaluate)
				{
					OutValue = FValue::MakeInteger(0);
					return true;
				}

				int64 Integer = 0;
				if (!RequireInteger(Operand, Spelling, Integer, OutError))
				{
					return false;
				}

				// Same reason as the additive operators: -MIN_int64 is undefined as a signed negation.
				OutValue = FValue::MakeInteger(bNegate
					? static_cast<int64>(~static_cast<uint64>(Integer) + 1)
					: Integer);
				return true;
			}

			bool ParsePrimary(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				if (MatchOperator(TEXT("(")))
				{
					if (!ParseOr(bEvaluate, OutValue, OutError))
					{
						return false;
					}

					if (!MatchOperator(TEXT(")")))
					{
						return FailConditionSyntax(Site, FText::Format(
							LOCTEXT("ExpectedCloseParenthesis", "expected ')' but found {0}."),
							DescribeToken(Peek())), OutError);
					}

					return true;
				}

				const FToken& Token = Peek();

				if (Token.Kind == ETokenKind::Integer)
				{
					OutValue = FValue::MakeInteger(Token.Integer);
					Advance();
					return true;
				}

				if (Token.Kind == ETokenKind::String)
				{
					OutValue = FValue::MakeString(Token.StringValue);
					Advance();
					return true;
				}

				if (Token.Kind == ETokenKind::Identifier)
				{
					// `defined` is a keyword only here, in operand position, which is also the only
					// place C treats it as one.
					if (Token.Spelling.Equals(TEXT("defined"), ESearchCase::CaseSensitive))
					{
						Advance();
						return ParseDefined(bEvaluate, OutValue, OutError);
					}

					const FString Name = Token.Spelling;
					Advance();
					ResolveIdentifier(Name, bEvaluate, OutValue);
					return true;
				}

				return FailConditionSyntax(Site, FText::Format(
					LOCTEXT("UnexpectedTokenInCondition", "unexpected {0}."),
					DescribeToken(Token)), OutError);
			}

			bool ParseDefined(const bool bEvaluate, FValue& OutValue, FDreamShaderTextError& OutError)
			{
				const bool bParenthesized = MatchOperator(TEXT("("));

				const FToken& NameToken = Peek();
				if (NameToken.Kind != ETokenKind::Identifier)
				{
					return FailConditionSyntax(Site, FText::Format(
						LOCTEXT("DefinedNeedsName", "'defined' needs a define name, but found {0}."),
						DescribeToken(NameToken)), OutError);
				}

				const FString Name = NameToken.Spelling;
				Advance();

				if (bParenthesized && !MatchOperator(TEXT(")")))
				{
					return FailConditionSyntax(Site, FText::Format(
						LOCTEXT("DefinedExpectedCloseParenthesis", "expected ')' to close 'defined({0})' but found {1}."),
						FText::FromString(Name),
						DescribeToken(Peek())), OutError);
				}

				if (!bEvaluate)
				{
					OutValue = FValue::MakeInteger(0);
					return true;
				}

				// The value is deliberately discarded: `defined` asks whether a name exists, and a
				// define spelled `0` is still defined. The read is what matters -- it is what records
				// the name, so that giving it a definition later invalidates the build key.
				FString UnusedValue;
				OutValue = FValue::MakeBoolean(ReadDefine(Name, UnusedValue));
				return true;
			}

			void ResolveIdentifier(const FString& InName, const bool bEvaluate, FValue& OutValue)
			{
				if (!bEvaluate)
				{
					OutValue = FValue::MakeInteger(0);
					return;
				}

				FString RawValue;
				if (!ReadDefine(InName, RawValue))
				{
					// C's rule: a name with no definition is the number 0, so `#if FOO` is false rather
					// than an error. The read is still recorded, undefined and all -- that sentinel is
					// what makes ADDING the define later change the build key.
					OutValue = FValue::MakeInteger(0);
					return;
				}

				const FString Trimmed = RawValue.TrimStartAndEnd();
				if (Trimmed.IsEmpty())
				{
					// A bare `#define FOO` is a marker, not the empty string, so `#if FOO` means what
					// everyone expects it to. `defined(FOO)` was always available for the other
					// question.
					OutValue = FValue::MakeInteger(1);
					return;
				}

				int64 Integer = 0;
				if (TryParsePreprocessorInteger(Trimmed, Integer))
				{
					// Trimmed and not raw: a settings row typed as `1 ` is a number with a stray space,
					// and reading it as the string "1 " would take the other branch without a word.
					OutValue = FValue::MakeInteger(Integer);
					return;
				}

				// Not trimmed here, though: once it is a string it is data, and its edges are the
				// author's business.
				OutValue = FValue::MakeString(RawValue);
			}

			const TArray<FToken>& Tokens;
			int32 Index = 0;
			FConditionSite Site;
			FDreamShaderPreprocessorDefineReader ReadDefine;
		};
	}

	bool FailDreamShaderPreprocessorTrailingTokens(
		const FString& InFilePathForDiagnostics,
		const int32 InLineNumber,
		const FString& InDirective,
		const FString& InFirstTrailingToken,
		FDreamShaderTextError& OutError)
	{
		return FailWith(OutError, TEXT("DSH1042"), FText::Format(
			LOCTEXT("TrailingTokensAfterDirective", "{0}({1}): '{2}' is already complete before '{3}'. Nothing may follow a directive but a '//' comment."),
			FText::FromString(InFilePathForDiagnostics),
			FormatPlainNumber(InLineNumber),
			FText::FromString(InDirective),
			FText::FromString(InFirstTrailingToken)));
	}

	bool EvaluateDreamShaderPreprocessorCondition(
		const FString& InExpression,
		const TCHAR* InDirective,
		const FString& InFilePathForDiagnostics,
		const int32 InLineNumber,
		FDreamShaderPreprocessorDefineReader InReadDefine,
		bool& bOutResult,
		FDreamShaderTextError& OutError)
	{
		FConditionSite Site;
		Site.Directive = InDirective;
		Site.FilePath = &InFilePathForDiagnostics;
		Site.Line = InLineNumber;

		TArray<FToken> Tokens;
		if (!TokenizeCondition(InExpression, Site, Tokens, OutError))
		{
			return false;
		}

		// One token is the End sentinel on its own: `#if` with nothing after it, or with nothing but
		// the trailing comment the caller has already stripped.
		if (Tokens.Num() <= 1)
		{
			return FailMissingCondition(Site, OutError);
		}

		FConditionParser Parser(Tokens, Site, InReadDefine);

		FValue Value;
		if (!Parser.ParseExpression(/*bEvaluate=*/true, Value, OutError))
		{
			return false;
		}

		// Parsing and leftover-checking are two steps, and they raise two different codes.
		//
		// Everything the parse itself refused is DSH1034: the expression is incomplete or malformed and
		// there is nothing to evaluate -- `(1`, `1 &&`, `&&1`, `1 &&)`. Reaching HERE means the opposite:
		// a whole, well-formed expression was consumed and something is still sitting after it -- `1 2`,
		// `1)`, `(1))`. That is not a broken expression, it is a finished directive with extra text, and
		// it is the same mistake with the same fix as `#ifdef A B` or `#endif junk`, so it carries the
		// same code as those rather than one that happens to name the component that noticed.
		if (!Parser.IsAtEnd())
		{
			return FailDreamShaderPreprocessorTrailingTokens(
				InFilePathForDiagnostics,
				InLineNumber,
				InDirective,
				Parser.Peek().Spelling,
				OutError);
		}

		if (Value.bIsString)
		{
			return FailConditionType(Site, FText::Format(
				LOCTEXT("StringAsCondition", "a condition must be a number, but this one is the string \"{0}\". Compare it with '==' instead."),
				FText::FromString(Value.String)), OutError);
		}

		bOutResult = Value.Integer != 0;
		return true;
	}
}

namespace UE::DreamShader
{
	bool EvaluateDreamShaderConditionExpression(
		const FString& InExpression,
		const FDreamShaderDefineTable& InDefines,
		bool& bOutResult,
		FDreamShaderTextError& OutError)
	{
		// A reader straight over the table, with no recording side effect. The callback shape exists
		// so the preprocessor can note which names an evaluation consulted -- see the reader type's
		// own comment -- and this caller has no build key to make precise, so it simply answers.
		auto ReadDefine = [&InDefines](const FString& Name, FString& OutValue) -> bool
		{
			const FDreamShaderDefineEntry* Entry = InDefines.Find(Name);
			if (!Entry)
			{
				return false;
			}

			OutValue = Entry->Value;
			return true;
		};

		// Empty path and line 1: there is no file here, and the header says so. A named local rather
		// than a temporary because the evaluator takes the path by reference and keeps a pointer to
		// it in FConditionSite for the length of the call.
		const FString NoFilePath;

		return Private::EvaluateDreamShaderPreprocessorCondition(
			InExpression,
			TEXT("#if"),
			NoFilePath,
			/*InLineNumber*/ 1,
			ReadDefine,
			bOutResult,
			OutError);
	}
}

#undef LOCTEXT_NAMESPACE
