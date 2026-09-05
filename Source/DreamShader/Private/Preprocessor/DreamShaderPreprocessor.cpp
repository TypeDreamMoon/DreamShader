// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderPreprocessor.h"

#include "DreamShaderPreprocessorExpression.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "DreamShader.Preprocessor"

namespace UE::DreamShader
{
	namespace
	{
		/**
		 * How deep `#if` may nest before the file is refused (DSH1037).
		 *
		 * A limit exists because the stack this scanner pushes is driven by source text and bounded by
		 * nothing else; 64 is far past any legible use and is what the design fixed.
		 */
		constexpr int32 GMaxConditionalDepth = 64;

		enum class EDirectiveKind : uint8
		{
			/** Ordinary source, or a `#region` / `#endregion` that belongs to the parser. */
			None,
			If,
			IfDef,
			IfNDef,
			Elif,
			Else,
			Endif,
			Define,
			Undef,
			/** A `#` line that is neither one of the eight above nor a `#Region` / `#EndRegion`. */
			Unknown,
		};

		/** One open `#if` / `#elif` / `#else` chain. */
		struct FConditionalFrame
		{
			/** Was the enclosing region emitting? A chain inside a dead branch can never activate. */
			bool bParentActive = false;

			/**
			 * Has any branch of this chain been taken yet?
			 *
			 * This is what makes `#elif` mean "else if" rather than "if": once a branch is taken every
			 * later one in the chain is dead, even where its own condition would be true.
			 */
			bool bBranchTaken = false;

			/** Is the branch currently open the one that emits? */
			bool bCurrentActive = false;

			bool bSeenElse = false;

			/** 1-based line of the `#if`, reported by DSH1030 when the chain is never closed. */
			int32 DirectiveLine = 0;

			/** 1-based line of the `#else`, reported by DSH1033 when something follows it. */
			int32 ElseLine = 0;
		};

		/**
		 * TDreamShaderDefineNameKeyFuncs' rule, restated for a TSet.
		 *
		 * It cannot literally BE that template. TSet needs DefaultKeyFuncs-shaped functions, whose
		 * ElementType is the key itself, while TDefaultMapKeyFuncs describes a TPair -- so the shared
		 * template does not fit here whatever it says. The rule is one line, and restating it costs
		 * less than turning this set into a map with a dummy value would; the reason for the rule is
		 * written once, on the shared template, and must stay in step with it.
		 *
		 * Only Matches is overridden, exactly as there: the inherited hash stays case-insensitive, keys
		 * equal under Matches still hash equal -- the only invariant a hash owes its container -- and
		 * `Foo`/`FOO` merely share a bucket.
		 */
		struct FDreamShaderDefineNameSetKeyFuncs : DefaultKeyFuncs<FString>
		{
			[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
			{
				return A.Equals(B, ESearchCase::CaseSensitive);
			}

			/**
			 * Declared alongside the non-template form because declaring one HIDES the whole inherited
			 * overload set. Without this, a heterogeneous lookup would either stop compiling or -- worse
			 * if the base form were reachable again some day -- fall back to the case-folding `A == B`
			 * through a path no test covers.
			 */
			template <typename ComparableKey>
			[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, ComparableKey B)
			{
				return A.Equals(B, ESearchCase::CaseSensitive);
			}
		};

		using FDefineNameSet = TSet<FString, FDreamShaderDefineNameSetKeyFuncs>;

		/** See FormatPlainNumber in the expression evaluator: a line number is not a quantity. */
		FText FormatPlainNumber(const int32 InNumber)
		{
			return FText::AsNumber(InNumber, &FNumberFormattingOptions::DefaultNoGrouping());
		}

		/** One physical line: its content, and the exact terminator that ended it. */
		struct FSourceLine
		{
			FString Content;

			/** Empty for a final line the file does not terminate; otherwise "\r\n", "\n" or "\r". */
			FString Terminator;
		};

		/**
		 * Splits text into lines while KEEPING each terminator, so the output can be rebuilt byte for
		 * byte.
		 *
		 * FString::ParseIntoArrayLines would do the splitting, but it throws the terminators away, and
		 * rejoining with a chosen one rewrites the line endings of every file that goes through here --
		 * including the overwhelming majority that contain no directive at all. That is a real cost:
		 * a source without directives has to come back unchanged to the byte, because this pass now runs
		 * on every file, its output is what the build key hashes, and a preprocessor that silently
		 * normalizes CRLF would restamp every asset in the project the first time it shipped and leave
		 * the working tree looking rewritten to anyone diffing generated text.
		 *
		 * The split itself matches ParseIntoArrayLines exactly -- CRLF as one terminator, a lone CR or a
		 * lone LF as one each -- because the import inliner re-splits the result that way and the
		 * caller's line-count check counts them that way.
		 */
		void SplitSourceLines(const FString& InText, TArray<FSourceLine>& OutLines)
		{
			const int32 Length = InText.Len();
			int32 LineStart = 0;

			for (int32 Index = 0; Index < Length; ++Index)
			{
				const TCHAR Character = InText[Index];
				if (Character != TCHAR('\n') && Character != TCHAR('\r'))
				{
					continue;
				}

				const int32 TerminatorLength =
					(Character == TCHAR('\r') && Index + 1 < Length && InText[Index + 1] == TCHAR('\n'))
						? 2
						: 1;

				FSourceLine& Line = OutLines.AddDefaulted_GetRef();
				Line.Content = InText.Mid(LineStart, Index - LineStart);
				Line.Terminator = InText.Mid(Index, TerminatorLength);

				Index += TerminatorLength - 1;
				LineStart = Index + 1;
			}

			// The trailing line always exists, even when it is empty. A file ending in a newline has one
			// more line than it has terminators, and dropping the empty last one would make the output a
			// line shorter than the input -- which is the failure this whole file is arranged to avoid.
			FSourceLine& LastLine = OutLines.AddDefaulted_GetRef();
			LastLine.Content = InText.Mid(LineStart);
		}

		// -------------------------------------------------------------------------------------------
		// Opaque regions: `Function` and `GraphFunction` bodies.
		//
		// A Function body is RAW HLSL, handed to the shader compiler as written. The `#` directives in
		// it belong to the HLSL preprocessor, which runs later, in the shader compiler, with the engine's
		// own defines in scope. They are not DreamShader's to read, and reading them anyway is silently
		// destructive rather than loudly wrong.
		//
		// The case that proves it, and the reason this rule must never be "simplified away":
		// Plugins/MoonToon/DShader/MaterialFunctions/MF_MoonToonTranslucencyShadow.dsf declares
		//
		//     Function MoonToonBlendModeSwitch(... out float3 Result)
		//     {
		//     #if MATERIALBLENDING_SOLID
		//         Result = Opaque;
		//     #elif MATERIALBLENDING_MASKED
		//         ...
		//     #else
		//         Result = 0;
		//     #endif
		//     }
		//
		// MATERIALBLENDING_SOLID is an ENGINE macro, defined per material by the shader compiler. It is
		// not, and must never be, in DreamShader's define table. Scan that body as DreamShader source
		// and every branch tests an undefined name, so every one is false, `#else` wins unconditionally,
		// and the function is cut down to `Result = 0`. It compiles. It generates. Every blend mode
		// silently returns the wrong value. The file's own comment says the engine exposes blend mode
		// only as preprocessor defines, which is precisely why the node has to be a Custom node: this is
		// a first-class DreamShaderLang idiom, not an accident. Six more files reach the same way for
		// `#include`, and two more for `#if PIXELSHADER`.
		//
		// So a Function body is OPAQUE: every line inside it passes through verbatim, `#` lines
		// included, with no directive recognition, no part in `#if` nesting, and no entry in the touched
		// set. A `#if` around the WHOLE block still works, because those directives sit outside it.
		//
		// GraphFunction is opaque for the same reason, and the reason is evidence rather than caution:
		// `Function` and `GraphFunction` are parsed by one function, ParseModernFunctionDeclaration, and
		// both end at the same `Function.HLSL = NormalizeShaderLanguageText(FunctionBody)`. The bodies
		// are the same kind of text; only what the generator does with the result differs.
		//
		// Nothing else is opaque, and each exclusion is checked rather than assumed:
		//   - `Shader { Graph { ... } }` and the `Graph` section of ShaderFunction / ShaderLayer /
		//     MaterialLayer hold DreamShaderLang statements, not HLSL (they land in `Code`, and are the
		//     blocks `#Region` is extracted from);
		//   - `VirtualFunction` refuses a Graph or Code section outright with DSH3064, so it cannot
		//     carry HLSL at all;
		//   - `Settings`, `Outputs`, `Inputs`, `Properties` and `Layout` are declaration-layer blocks,
		//     which is exactly where `#if` exists to be useful -- a StaticSwitch cannot reach them.
		//
		// Losing `#if` inside a Function body costs nothing: HLSL's own preprocessor is right there and
		// already works on that text.
		// -------------------------------------------------------------------------------------------

		/**
		 * Tracks whether the current line is inside a `Function` / `GraphFunction` body.
		 *
		 * Character-level, and aware of comments and string literals, because brace counting has to be:
		 * a `//` or a `"{"` in an HLSL body would otherwise close the region early and hand the rest of
		 * the function back to the directive scanner -- the very failure this whole mechanism exists to
		 * prevent, arrived at from the other side. Block-comment state is the one thing carried across
		 * lines, since a block comment inside a body can span them.
		 *
		 * Directive classification deliberately does NOT consult that comment state: the design records
		 * "a directive inside a block comment is still a directive" as a known limitation, shared with
		 * `#Region`, and fixing it here would make the two disagree.
		 */
		struct FOpaqueRegionTracker
		{
			enum class EState : uint8
			{
				/** Ordinary source: directives are recognized. */
				Outside,
				/** A Function declaration was seen; its body's `{` has not arrived yet. */
				SeekingBody,
				/** Inside the body. */
				InsideBody,
			};

			EState State = EState::Outside;
			int32 BraceDepth = 0;
			bool bInBlockComment = false;

			/** Asked BEFORE the line is scanned, so a declaration line is still ordinary source. */
			bool IsOpaque() const
			{
				return State != EState::Outside;
			}

			void ScanLine(const FString& InLine)
			{
				const int32 Length = InLine.Len();

				// A `#`-shaped line is never a Function declaration, whatever words follow. Without this
				// a region comment spelled `#Region Function helpers` would open an opaque region that
				// swallows the rest of the graph.
				bool bLineIsHashShaped = false;
				for (int32 Probe = 0; Probe < Length; ++Probe)
				{
					if (!FChar::IsWhitespace(InLine[Probe]))
					{
						bLineIsHashShaped = InLine[Probe] == TCHAR('#');
						break;
					}
				}

				// String and character state are per-line: neither literal may span a line in HLSL or in
				// DreamShaderLang, so resetting each line keeps one stray quote from eating the file.
				bool bInString = false;
				bool bInCharacter = false;

				for (int32 Index = 0; Index < Length; ++Index)
				{
					const TCHAR Character = InLine[Index];

					if (bInBlockComment)
					{
						if (Character == TCHAR('*') && Index + 1 < Length && InLine[Index + 1] == TCHAR('/'))
						{
							bInBlockComment = false;
							++Index;
						}
						continue;
					}

					if (bInString || bInCharacter)
					{
						if (Character == TCHAR('\\') && Index + 1 < Length)
						{
							++Index;
							continue;
						}
						if (bInString && Character == TCHAR('"'))
						{
							bInString = false;
						}
						else if (bInCharacter && Character == TCHAR('\''))
						{
							bInCharacter = false;
						}
						continue;
					}

					if (Character == TCHAR('/') && Index + 1 < Length)
					{
						if (InLine[Index + 1] == TCHAR('/'))
						{
							// Nothing after a line comment can affect the state, and returning here is
							// what keeps a `// }` from closing a body.
							return;
						}
						if (InLine[Index + 1] == TCHAR('*'))
						{
							bInBlockComment = true;
							++Index;
							continue;
						}
					}

					if (Character == TCHAR('"'))
					{
						bInString = true;
						continue;
					}

					if (Character == TCHAR('\'') && HasClosingQuoteOnLine(InLine, Index))
					{
						// Only when the line actually closes it. An apostrophe with no partner is far
						// likelier than a character literal in either language, and treating one as a
						// literal would blind the rest of the line -- including a `}` that ends a body.
						bInCharacter = true;
						continue;
					}

					if (State == EState::Outside)
					{
						if (FChar::IsAlpha(Character) || Character == TCHAR('_'))
						{
							// Whole identifier runs, so `MaterialFunction` and `VirtualFunction` are one
							// token each and cannot match on their `Function` tail.
							const int32 Start = Index;
							while (Index < Length && (FChar::IsAlnum(InLine[Index]) || InLine[Index] == TCHAR('_')))
							{
								++Index;
							}

							const FString Token = InLine.Mid(Start, Index - Start);
							--Index;

							// Case-sensitive, matching FScanner::TryConsumeKeyword, which is what
							// actually decides whether the parser sees a Function block.
							if (!bLineIsHashShaped
								&& (Token.Equals(TEXT("Function"), ESearchCase::CaseSensitive)
									|| Token.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive)))
							{
								State = EState::SeekingBody;
								BraceDepth = 0;
							}
						}

						// Braces outside a Function body belong to Shader, Namespace and the section
						// blocks, and are none of this tracker's business.
						continue;
					}

					if (Character == TCHAR('{'))
					{
						++BraceDepth;
						State = EState::InsideBody;
						continue;
					}

					if (Character == TCHAR('}') && State == EState::InsideBody)
					{
						--BraceDepth;
						if (BraceDepth <= 0)
						{
							State = EState::Outside;
							BraceDepth = 0;
						}
					}
				}
			}

		private:
			static bool HasClosingQuoteOnLine(const FString& InLine, const int32 InOpenIndex)
			{
				for (int32 Index = InOpenIndex + 1; Index < InLine.Len(); ++Index)
				{
					if (InLine[Index] == TCHAR('\\'))
					{
						++Index;
						continue;
					}
					if (InLine[Index] == TCHAR('\''))
					{
						return true;
					}
				}
				return false;
			}
		};

		struct FDirectiveKeywordEntry
		{
			const TCHAR* Keyword;
			EDirectiveKind Kind;
		};

		/**
		 * The eight preprocessor directives, and the only spellings that are them.
		 *
		 * Lowercase, matched case-sensitively, matching HLSL and C. `#IF` is not this table's `if`; it is
		 * an unknown directive, and DSH1035 says so with the right spelling attached rather than letting
		 * it through to mean nothing.
		 */
		constexpr FDirectiveKeywordEntry GDirectiveKeywords[] =
		{
			{ TEXT("if"),     EDirectiveKind::If },
			{ TEXT("ifdef"),  EDirectiveKind::IfDef },
			{ TEXT("ifndef"), EDirectiveKind::IfNDef },
			{ TEXT("elif"),   EDirectiveKind::Elif },
			{ TEXT("else"),   EDirectiveKind::Else },
			{ TEXT("endif"),  EDirectiveKind::Endif },
			{ TEXT("define"), EDirectiveKind::Define },
			{ TEXT("undef"),  EDirectiveKind::Undef },
		};

		/**
		 * The parser's graph directives, which pass through untouched.
		 *
		 * Matched case-INSENSITIVELY, because that is what the parser does: ExtractGraphRegions reaches
		 * them through IsGraphDirective, which compares with ESearchCase::IgnoreCase, so `#Region`,
		 * `#region` and `#REGION` are all region directives today and Docs/language/layout.md says so.
		 * Naming them here is what lets the fallthrough below be an error instead of a shrug.
		 */
		constexpr const TCHAR* GGraphDirectiveKeywords[] = { TEXT("region"), TEXT("endregion") };

		/**
		 * Classifies one physical line, and hands back the keyword and everything after it.
		 *
		 * A line is a directive when its first non-whitespace character is `#`. A line starting with
		 * `//` therefore cannot be one, which is what makes a commented-out `// #if FOO` inert with no
		 * special case.
		 *
		 * The keyword is read as a maximal run of identifier characters, and that one decision settles
		 * three rules at once:
		 *
		 *   - `#if(A)` is legal, because whatever follows the run is by construction not an identifier
		 *     character and the `(` simply starts the expression;
		 *   - `#iffy` is an unknown directive rather than a mangled `#if`, because the run is `iffy`;
		 *   - `#  if FOO` is `#if`, because whitespace after `#` is skipped first.
		 *
		 * Outside a Function body there are then exactly three outcomes, and the third one is the point:
		 * a graph directive passes through, one of the eight is acted on, and ANYTHING ELSE is an error.
		 *
		 * There is deliberately no "unrecognized spellings pass through" fallback. One used to live here,
		 * to let `#Region` by before the graph directives were named explicitly, and it was quietly
		 * expensive: under it `#IF FOO` and `#Endif` were emitted as ordinary source, the parser had
		 * nothing to say about them either, and the author's conditional simply never took effect -- no
		 * error, no warning, and a generated asset that looks fine. That is the exact shape of failure
		 * this whole feature exists to remove, so a `#` line that is none of the ten named spellings is
		 * DSH1035, and DSH1035 suggests what was probably meant.
		 *
		 * Note what this function is NOT asked: whether the line is inside a Function body. That is the
		 * tracker's job, and a line inside one never reaches here -- which is why `#include`, legal and
		 * common at the top of an HLSL body, needs no entry above.
		 */
		EDirectiveKind ClassifyDirectiveLine(const FString& InLine, FString& OutKeyword, FString& OutRest)
		{
			OutKeyword.Reset();
			OutRest.Reset();

			const int32 Length = InLine.Len();
			int32 Index = 0;

			while (Index < Length && FChar::IsWhitespace(InLine[Index]))
			{
				++Index;
			}

			if (Index >= Length || InLine[Index] != TCHAR('#'))
			{
				return EDirectiveKind::None;
			}

			++Index;
			while (Index < Length && FChar::IsWhitespace(InLine[Index]))
			{
				++Index;
			}

			const int32 KeywordStart = Index;
			while (Index < Length && (FChar::IsAlnum(InLine[Index]) || InLine[Index] == TCHAR('_')))
			{
				++Index;
			}

			OutKeyword = InLine.Mid(KeywordStart, Index - KeywordStart);
			OutRest = InLine.Mid(Index);

			for (const TCHAR* const GraphKeyword : GGraphDirectiveKeywords)
			{
				if (OutKeyword.Equals(GraphKeyword, ESearchCase::IgnoreCase))
				{
					return EDirectiveKind::None;
				}
			}

			for (const FDirectiveKeywordEntry& Entry : GDirectiveKeywords)
			{
				if (OutKeyword.Equals(Entry.Keyword, ESearchCase::CaseSensitive))
				{
					return Entry.Kind;
				}
			}

			return EDirectiveKind::Unknown;
		}

		/** Levenshtein, on strings short enough that the obvious two-row table is the whole story. */
		int32 ComputeEditDistance(const FString& InLeft, const FString& InRight)
		{
			const int32 LeftLength = InLeft.Len();
			const int32 RightLength = InRight.Len();

			TArray<int32> Previous;
			TArray<int32> Current;
			Previous.SetNumUninitialized(RightLength + 1);
			Current.SetNumUninitialized(RightLength + 1);

			for (int32 Column = 0; Column <= RightLength; ++Column)
			{
				Previous[Column] = Column;
			}

			for (int32 Row = 1; Row <= LeftLength; ++Row)
			{
				Current[0] = Row;
				for (int32 Column = 1; Column <= RightLength; ++Column)
				{
					const int32 Substitution = Previous[Column - 1]
						+ (InLeft[Row - 1] == InRight[Column - 1] ? 0 : 1);
					Current[Column] = FMath::Min3(Substitution, Previous[Column] + 1, Current[Column - 1] + 1);
				}
				Swap(Previous, Current);
			}

			return Previous[RightLength];
		}

		/**
		 * The advice half of DSH1035, as a sub-FText.
		 *
		 * One format string cannot serve both of the mistakes that reach it. `#include` is someone
		 * reaching for the HLSL they know, and the answer is a different construct entirely; `#endfi` and
		 * `#IF` are someone reaching for the right directive and missing, and the answer is a spelling.
		 * Handing either one the other's advice wastes the diagnostic. So the code keeps its single raise
		 * site and its single format string, and the part that has to differ arrives as this.
		 */
		FText SuggestPreprocessorDirective(const FString& InKeyword)
		{
			const FString Lowered = InKeyword.ToLower();

			if (Lowered.Equals(TEXT("include"), ESearchCase::CaseSensitive))
			{
				return LOCTEXT("UnknownDirectiveSuggestImport", "'#include' is HLSL: it is recognized inside a Function body and nowhere else. At the declaration level, use import \"...\" instead.");
			}

			// Candidates are everything legal on a `#` line: the eight this file acts on, plus the two it
			// hands to the parser. A typo for `#endregion` deserves the same help as one for `#endif`.
			const TCHAR* Nearest = nullptr;
			int32 NearestDistance = MAX_int32;

			auto Consider = [&Lowered, &Nearest, &NearestDistance](const TCHAR* InCandidate)
			{
				const FString Candidate = InCandidate;

				// Scaled, not fixed: two edits from `endif` is still recognizably `endif`, while two edits
				// from `if` is any two-letter word at all, and suggesting it would be noise.
				const int32 MaxDistance = Candidate.Len() <= 3 ? 1 : 2;

				const int32 Distance = ComputeEditDistance(Lowered, Candidate);
				if (Distance <= MaxDistance && Distance < NearestDistance)
				{
					Nearest = InCandidate;
					NearestDistance = Distance;
				}
			};

			for (const FDirectiveKeywordEntry& Entry : GDirectiveKeywords)
			{
				Consider(Entry.Keyword);
			}
			for (const TCHAR* const GraphKeyword : GGraphDirectiveKeywords)
			{
				Consider(GraphKeyword);
			}

			if (Nearest)
			{
				// Distance zero means the spelling was right and only the case was wrong. That IS the
				// diagnosis, and worth saying outright rather than dressing up as a "did you mean".
				return NearestDistance == 0
					? FText::Format(
						LOCTEXT("UnknownDirectiveSuggestCase", "Preprocessor directives are lowercase: write '#{0}'."),
						FText::FromString(Nearest))
					: FText::Format(
						LOCTEXT("UnknownDirectiveSuggestNearest", "Did you mean '#{0}'?"),
						FText::FromString(Nearest));
			}

			return LOCTEXT("UnknownDirectiveSuggestList", "A '#' line must be #if, #ifdef, #ifndef, #elif, #else, #endif, #define or #undef, or one of the parser's #Region / #EndRegion.");
		}

		/** True for the eight real directives -- Unknown is a misspelling, not a directive. */
		bool IsRealDirective(const EDirectiveKind InKind)
		{
			return InKind != EDirectiveKind::None && InKind != EDirectiveKind::Unknown;
		}

		/**
		 * Cuts a trailing `//` comment off a directive's tail.
		 *
		 * String-aware, because `#if DS_PLATFORM == "http://a"` is a condition and not a comment. Block
		 * comments are deliberately not handled here: one may span lines, and a line-oriented scanner
		 * that tried would need the very lexer state this pass runs before. The design records that as a
		 * known limitation, alongside the matching one that a directive inside a block comment is still
		 * a directive.
		 */
		FString StripTrailingLineComment(const FString& InText)
		{
			bool bInString = false;
			for (int32 Index = 0; Index < InText.Len(); ++Index)
			{
				const TCHAR Character = InText[Index];

				if (bInString)
				{
					if (Character == TCHAR('\\') && Index + 1 < InText.Len())
					{
						++Index;
						continue;
					}
					if (Character == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Character == TCHAR('"'))
				{
					bInString = true;
					continue;
				}

				if (Character == TCHAR('/') && Index + 1 < InText.Len() && InText[Index + 1] == TCHAR('/'))
				{
					return InText.Left(Index);
				}
			}

			return InText;
		}

		/**
		 * Splits a `#define` / `#undef` / `#ifdef` / `#ifndef` tail into the name and the remainder.
		 *
		 * The name is the first whitespace-delimited token, taken WHOLE and validated afterwards, so
		 * `#define FOO(x) ...` fails as an invalid name instead of quietly defining `FOO` with the value
		 * `(x)`. There are no function-like macros here, and a source reaching for one should be told
		 * so rather than compiled into something else.
		 *
		 * A `#define` value is everything after the name, trimmed, and runs to the end of the line:
		 * there is no line continuation, which is the other half of what makes line-count conservation
		 * possible at all.
		 */
		void SplitDefineNameAndValue(const FString& InRest, FString& OutName, FString& OutValue)
		{
			const int32 Length = InRest.Len();
			int32 Index = 0;

			while (Index < Length && FChar::IsWhitespace(InRest[Index]))
			{
				++Index;
			}

			const int32 NameStart = Index;
			while (Index < Length && !FChar::IsWhitespace(InRest[Index]))
			{
				++Index;
			}

			OutName = InRest.Mid(NameStart, Index - NameStart);
			OutValue = InRest.Mid(Index).TrimStartAndEnd();
		}

		/**
		 * Reduces a directive tail to the shape DSH1042 wants, and forwards.
		 *
		 * NOT a raise site -- FailDreamShaderPreprocessorTrailingTokens is, and it is the only one. It
		 * sits in the expression translation unit because the evaluator needs it too: `#if 1 2` and
		 * `#ifdef A B` are the same mistake with the same fix, so they must be the same code, and a code
		 * with two wordings is a code whose documentation page is written by whichever site
		 * .skill/gen-diagnostics.ps1 happened to reach first. This wrapper exists only so the four call
		 * sites below can pass the raw remainder and the bare keyword they already have.
		 *
		 * `#define` never reaches here: its value runs to the end of the line, so `#define A B C` has no
		 * trailing tokens, only a value spelled `B C`.
		 */
		bool FailTrailingTokens(
			const FString& InFilePathForDiagnostics,
			const int32 InLineNumber,
			const FString& InKeyword,
			const FString& InRemainder,
			FDreamShaderTextError& OutError)
		{
			FString FirstTrailingToken;
			FString IgnoredRest;
			SplitDefineNameAndValue(InRemainder, FirstTrailingToken, IgnoredRest);

			return Private::FailDreamShaderPreprocessorTrailingTokens(
				InFilePathForDiagnostics,
				InLineNumber,
				FString(TEXT("#")) + InKeyword,
				FirstTrailingToken,
				OutError);
		}
	}

	bool PreprocessDreamShaderSource(
		const FString& InText,
		const FString& InFilePathForDiagnostics,
		const FDreamShaderDefineTable& InDefines,
		FDreamShaderPreprocessResult& OutResult,
		FDreamShaderTextError& OutError)
	{
		OutResult.Text.Reset();
		OutResult.TouchedDefines.Reset();
		OutResult.bHadDirectives = false;

		TArray<FSourceLine> Lines;
		SplitSourceLines(InText, Lines);

		// `#define` is FILE-LOCAL, and this copy is the whole of that implementation: definitions land
		// on it and it dies with this call, so a header cannot leak a macro into the file that imports
		// it. See the public header for why that trade is the right one.
		FDreamShaderDefineTable Defines = InDefines;

		// Names this file has defined or undefined itself. Read by the touched-set rule below.
		FDefineNameSet LocallyOverriddenNames;

		TArray<FConditionalFrame> Stack;
		FOpaqueRegionTracker OpaqueRegion;

		auto IsEmitting = [&Stack]() -> bool
		{
			return Stack.IsEmpty() || Stack.Top().bCurrentActive;
		};

		auto ReadDefine = [&Defines, &LocallyOverriddenNames, &OutResult](const FString& Name, FString& OutValue) -> bool
		{
			const FDreamShaderDefineEntry* Entry = Defines.Find(Name);

			// A read is recorded only when the INJECTED TABLE is what answered it.
			//
			// TouchedDefines exists to answer one question -- which of the table's entries did this
			// file's output depend on, and what were they -- because the build key folds the answer in.
			// A name this file has defined or undefined itself is answered by the file's own text, which
			// the build key already hashes, so recording it would add nothing and would actively lie:
			// the value would depend on where in the file the read happened, the same name could be
			// folded twice with two values, and the union across files in
			// LoadPreparedDreamShaderSourceRecursive would report a contradiction that is not one.
			//
			// The other direction is why an UNDEFINED name still has to be recorded, sentinel and all.
			// `#if defined(FOO)` with no FOO anywhere is false, and that false was decided by the
			// table's silence. Leave it out and adding FOO later changes no hash, rebuilds nothing, and
			// the asset keeps the other branch forever.
			//
			// Once a name is locally overridden the injected value can never be observed again -- there
			// is one flat table, so an `#undef` removes the binding rather than uncovering an outer one
			// -- which is exactly what makes skipping it sound rather than merely tidy. And for a name
			// that is NOT overridden, this table still holds precisely what the injected one holds, so
			// the value recorded here is the injected value.
			if (!LocallyOverriddenNames.Contains(Name))
			{
				OutResult.TouchedDefines.Add(
					Name,
					Entry ? Entry->Value : FString(GDreamShaderUndefinedDefineSentinel));
			}

			if (!Entry)
			{
				return false;
			}

			OutValue = Entry->Value;
			return true;
		};

		FString Output;
		Output.Reserve(InText.Len());

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex].Content;
			const int32 LineNumber = LineIndex + 1;

			// Asked before the line is scanned, so a `Function` declaration line is still ordinary
			// source and the body's closing `}` line is still the last opaque one.
			const bool bOpaque = OpaqueRegion.IsOpaque();

			FString Keyword;
			FString Rest;
			const EDirectiveKind Kind = bOpaque
				? EDirectiveKind::None
				: ClassifyDirectiveLine(Line, Keyword, Rest);

			const bool bEmitting = IsEmitting();

			// LINE-COUNT CONSERVATION LIVES HERE, and it lives here by shape rather than by care: every
			// iteration emits the line's own terminator, and either the line's content or nothing. So
			// the output has exactly the input's line count and exactly its line endings, for any input
			// and any branch taken below -- and a file with no directives comes back byte for byte.
			// Directive lines and cut lines come out EMPTY, never removed, because the diagnostics
			// mapper recovers physical line numbers by counting lines inside each file's Begin/End
			// block, and one missing line silently misplaces every error under it.
			if (Kind == EDirectiveKind::None && bEmitting)
			{
				Output.Append(Line);
			}
			Output.Append(Lines[LineIndex].Terminator);

			// Advanced for every line, opaque or not, emitted or cut. A Function body inside a branch
			// this build cuts still has to be recognized as a body, or its HLSL `#if` lines would be
			// paired against the file's own conditional stack and corrupt it.
			OpaqueRegion.ScanLine(Line);

			if (IsRealDirective(Kind))
			{
				// Taken or not: the Adopt gate asks whether adopting would delete conditionals, and a
				// conditional in a branch this compile cut is still one in the file.
				OutResult.bHadDirectives = true;
			}

			switch (Kind)
			{
			case EDirectiveKind::None:
				break;

			case EDirectiveKind::Unknown:
				// Not reported inside a cut branch, exactly as in C. A branch that was cut is not
				// compiled, and checking it would raise errors about code this build does not contain
				// and make the message depend on which switches happened to be set. The cost is that a
				// dead branch rots unnoticed; the design accepts it and names `-DefineMatrix` as the
				// eventual answer.
				if (bEmitting)
				{
					return FailWith(OutError, TEXT("DSH1035"), FText::Format(
						LOCTEXT("UnknownPreprocessorDirective", "{0}({1}): unknown preprocessor directive '#{2}'. {3}"),
						FText::FromString(InFilePathForDiagnostics),
						FormatPlainNumber(LineNumber),
						FText::FromString(Keyword),
						SuggestPreprocessorDirective(Keyword)));
				}
				break;

			case EDirectiveKind::If:
			case EDirectiveKind::IfDef:
			case EDirectiveKind::IfNDef:
				{
					if (Stack.Num() >= GMaxConditionalDepth)
					{
						return FailWith(OutError, TEXT("DSH1037"), FText::Format(
							LOCTEXT("ConditionalNestingTooDeep", "{0}({1}): '#{2}' nesting is deeper than the limit of {3}."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber),
							FText::FromString(Keyword),
							FormatPlainNumber(GMaxConditionalDepth)));
					}

					FConditionalFrame Frame;
					Frame.bParentActive = bEmitting;
					Frame.DirectiveLine = LineNumber;

					if (bEmitting)
					{
						bool bCondition = false;

						if (Kind == EDirectiveKind::If)
						{
							if (!Private::EvaluateDreamShaderPreprocessorCondition(
								StripTrailingLineComment(Rest),
								TEXT("#if"),
								InFilePathForDiagnostics,
								LineNumber,
								ReadDefine,
								bCondition,
								OutError))
							{
								return false;
							}
						}
						else
						{
							// `#ifdef NAME` is `#if defined(NAME)` and `#ifndef NAME` is its negation.
							// They exist because the decision was to align with HLSL: an author writing
							// from muscle memory would otherwise land on DSH1035 for a spelling the
							// language has no reason to refuse.
							FString Name;
							FString TrailingTokens;
							SplitDefineNameAndValue(StripTrailingLineComment(Rest), Name, TrailingTokens);

							// Nothing at all is a MISSING operand, the same failure as a bare `#if`;
							// something unusable is an invalid NAME. They are different mistakes and a
							// reader chasing one should not be handed the other's code, so `#ifdef`
							// splits here while `#define` -- which has no operand-less form to confuse
							// it with -- reports DSH1038 for both.
							if (Name.IsEmpty())
							{
								return FailWith(OutError, TEXT("DSH1036"), FText::Format(
									LOCTEXT("MissingDefineNameOperand", "{0}({1}): '#{2}' requires a define name."),
									FText::FromString(InFilePathForDiagnostics),
									FormatPlainNumber(LineNumber),
									FText::FromString(Keyword)));
							}

							if (!IsValidDreamShaderDefineName(Name))
							{
								return FailWith(OutError, TEXT("DSH1038"), FText::Format(
									LOCTEXT("InvalidDefineName", "{0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'."),
									FText::FromString(InFilePathForDiagnostics),
									FormatPlainNumber(LineNumber),
									FText::FromString(Keyword),
									FText::FromString(Name)));
							}

							// `#ifdef A B` desugars to `#if defined(A) B`, so it must report what that
							// spelling reports -- and it does, down to the code. The check has to be
							// written out here only because the name is read directly rather than run
							// through the expression grammar that would otherwise have noticed.
							if (!TrailingTokens.IsEmpty())
							{
								return FailTrailingTokens(
									InFilePathForDiagnostics, LineNumber, Keyword, TrailingTokens, OutError);
							}

							FString UnusedValue;
							const bool bDefined = ReadDefine(Name, UnusedValue);
							bCondition = (Kind == EDirectiveKind::IfDef) ? bDefined : !bDefined;
						}

						Frame.bBranchTaken = bCondition;
						Frame.bCurrentActive = bCondition;
					}
					else
					{
						// Inside a branch already cut, the condition is not evaluated at all: no define
						// is read, so none is recorded, and a syntax error in it is not reported. Only
						// the nesting is tracked, which is all that is needed to pair the `#endif`.
						// Marking the chain as already taken is how every `#elif` and `#else` under it
						// stays dead without a second flag to consult.
						Frame.bBranchTaken = true;
						Frame.bCurrentActive = false;
					}

					Stack.Push(Frame);
					break;
				}

			case EDirectiveKind::Elif:
			case EDirectiveKind::Else:
				{
					if (Stack.IsEmpty())
					{
						return FailWith(OutError, TEXT("DSH1032"), FText::Format(
							LOCTEXT("StrayConditionalBranch", "{0}({1}): '#{2}' without a matching '#if'."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber),
							FText::FromString(Keyword)));
					}

					FConditionalFrame& Frame = Stack.Top();

					// Reported even inside a cut branch, unlike everything else here. The SHAPE of the
					// chain is not part of a branch's contents: get it wrong and the `#endif` pairing
					// goes wrong with it, so every line after it is misjudged in a file that still
					// compiles. A malformed chain is never survivable, whichever switches are set.
					if (Frame.bSeenElse)
					{
						return FailWith(OutError, TEXT("DSH1033"), FText::Format(
							LOCTEXT("BranchAfterElse", "{0}({1}): '#{2}' after the '#else' on line {3}, which already closed this chain."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber),
							FText::FromString(Keyword),
							FormatPlainNumber(Frame.ElseLine)));
					}

					if (Kind == EDirectiveKind::Else)
					{
						// `#else` takes no operand at all, so anything left is trailing. Checked whether
						// or not this branch emits, for the same reason DSH1033 is: `#else` and `#endif`
						// belong to the CHAIN, not to the branch they sit in, and a chain is never
						// "inside a region that was cut" the way a branch's contents are.
						const FString ElseRemainder = StripTrailingLineComment(Rest).TrimStartAndEnd();
						if (!ElseRemainder.IsEmpty())
						{
							return FailTrailingTokens(
								InFilePathForDiagnostics, LineNumber, Keyword, ElseRemainder, OutError);
						}

						Frame.bSeenElse = true;
						Frame.ElseLine = LineNumber;
						Frame.bCurrentActive = Frame.bParentActive && !Frame.bBranchTaken;
						Frame.bBranchTaken = true;
						break;
					}

					if (Frame.bParentActive && !Frame.bBranchTaken)
					{
						bool bCondition = false;
						if (!Private::EvaluateDreamShaderPreprocessorCondition(
							StripTrailingLineComment(Rest),
							TEXT("#elif"),
							InFilePathForDiagnostics,
							LineNumber,
							ReadDefine,
							bCondition,
							OutError))
						{
							return false;
						}

						Frame.bBranchTaken = bCondition;
						Frame.bCurrentActive = bCondition;
					}
					else
					{
						// An earlier branch already won, or the whole chain is inside a cut region.
						// Either way this condition cannot change the output, so it is not evaluated --
						// which is what keeps its defines out of the touched set.
						Frame.bCurrentActive = false;
					}

					break;
				}

			case EDirectiveKind::Endif:
				{
					if (Stack.IsEmpty())
					{
						return FailWith(OutError, TEXT("DSH1031"), FText::Format(
							LOCTEXT("StrayEndif", "{0}({1}): '#endif' without a matching '#if'."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber)));
					}

					// `#endif MOONTOON_LEGACY` is the C habit of labelling a long chain, and it is not
					// spelled that way here -- `// MOONTOON_LEGACY` is. Checked unconditionally, as
					// `#else` is above and for the same reason.
					const FString EndifRemainder = StripTrailingLineComment(Rest).TrimStartAndEnd();
					if (!EndifRemainder.IsEmpty())
					{
						return FailTrailingTokens(
							InFilePathForDiagnostics, LineNumber, Keyword, EndifRemainder, OutError);
					}

					Stack.Pop();
					break;
				}

			case EDirectiveKind::Define:
			case EDirectiveKind::Undef:
				{
					// Only in an active region: a `#define` in a branch this build cut must not change
					// what the lines after it see, or a cut branch would still be steering the file.
					if (!bEmitting)
					{
						break;
					}

					FString Name;
					FString Value;
					SplitDefineNameAndValue(StripTrailingLineComment(Rest), Name, Value);

					if (!IsValidDreamShaderDefineName(Name))
					{
						return FailWith(OutError, TEXT("DSH1038"), FText::Format(
							LOCTEXT("InvalidDefineNameOnDefinition", "{0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber),
							FText::FromString(Keyword),
							FText::FromString(Name)));
					}

					if (IsReservedDreamShaderDefineName(Name))
					{
						// `#undef DS_FOO` is refused for the same reason `#define DS_FOO` is: the
						// builtins describe the process doing the compiling, and a source able to erase
						// one could make a Substrate build generate the non-Substrate graph. Reading one
						// is fine, which is why `#ifdef DS_SUBSTRATE` never comes through here.
						return FailWith(OutError, TEXT("DSH1039"), FText::Format(
							LOCTEXT("ReservedDefineName", "{0}({1}): '{3}' is a read-only built-in constant, so '#{2}' cannot change it. The 'DS_' prefix is reserved by DreamShader."),
							FText::FromString(InFilePathForDiagnostics),
							FormatPlainNumber(LineNumber),
							FText::FromString(Keyword),
							FText::FromString(Name)));
					}

					// `#undef` takes a name and nothing else. `#define` is the exception in this pair --
					// its value runs to the end of the line, so there is no such thing as a trailing
					// token after one -- which is why the check is here and not above the switch.
					if (Kind == EDirectiveKind::Undef && !Value.IsEmpty())
					{
						return FailTrailingTokens(
							InFilePathForDiagnostics, LineNumber, Keyword, Value, OutError);
					}

					// Marked before the write, and marked for `#undef` too: from here on, what this name
					// reads is decided by this file's text, so the touched set stops recording it.
					LocallyOverriddenNames.Add(Name);

					if (Kind == EDirectiveKind::Define)
					{
						// SourceFile is the one tier that never takes part in resolution: it is written
						// here, onto the local copy that dies with this call, and no injected tier is
						// ever ranked against it. Whatever it shadows, it shadows only for the rest of
						// this one file.
						//
						// Set refuses reserved names on its own; the check above is what turns that
						// silent refusal into a diagnostic with a file and a line.
						Defines.Set(Name, Value, EDreamShaderDefineSource::SourceFile, InFilePathForDiagnostics);
					}
					else
					{
						// Remove, not "restore what the table had": there is one flat table, so an
						// `#undef` of an injected name leaves it undefined for the rest of the file
						// rather than uncovering an outer definition.
						Defines.Remove(Name);
					}

					break;
				}
			}
		}

		if (!Stack.IsEmpty())
		{
			// Pointed at the innermost `#if` still open, the one whose `#endif` the file ran out before
			// reaching. The count is there because with several open at once the innermost is a guess at
			// which one the author meant to close.
			return FailWith(OutError, TEXT("DSH1030"), FText::Format(
				LOCTEXT("UnterminatedConditional", "{0}({1}): this '#if' is never closed; the file ends with {2} conditional block(s) still open."),
				FText::FromString(InFilePathForDiagnostics),
				FormatPlainNumber(Stack.Top().DirectiveLine),
				FormatPlainNumber(Stack.Num())));
		}

		OutResult.Text = MoveTemp(Output);
		return true;
	}

	bool DreamShaderSourceHasPreprocessorDirectives(const FString& InText)
	{
		TArray<FSourceLine> Lines;
		SplitSourceLines(InText, Lines);

		FOpaqueRegionTracker OpaqueRegion;

		for (const FSourceLine& SourceLine : Lines)
		{
			const FString& Line = SourceLine.Content;
			const bool bOpaque = OpaqueRegion.IsOpaque();
			OpaqueRegion.ScanLine(Line);

			if (bOpaque)
			{
				// The Function-body rule matters here more than anywhere. This answer gates Adopt, and
				// MF_MoonToonTranslucencyShadow.dsf is full of `#if` lines that are HLSL, not
				// DreamShader -- counting them would refuse Adopt on files that have no DreamShader
				// conditional at all.
				continue;
			}

			FString Keyword;
			FString Rest;
			// Through the same classifier and the same tracker the real pass uses, rather than a quick
			// substring search. The two answers must agree -- this one gates Adopt (DSH8149), the other
			// decides what gets cut -- and asking the same code is the only way to guarantee that.
			if (IsRealDirective(ClassifyDirectiveLine(Line, Keyword, Rest)))
			{
				return true;
			}
		}

		return false;
	}

	FString BuildDreamShaderDefineKeyFragment(const FDreamShaderDefineValueMap& TouchedDefines)
	{
		TArray<FString> Names;
		TouchedDefines.GetKeys(Names);

		// Sorted with an explicit case-sensitive comparator, NOT with the default.
		//
		// The map's own key funcs keep `Foo` and `FOO` apart, and this is the other half of that: an
		// FString's relational operators are documented case-insensitive (they run Stricmp), so the
		// default comparator would call the two names equivalent and an unstable sort would then be
		// free to order them either way from one run to the next. The result is a build key that
		// changes without the build changing: every asset stale, every time, for a reason nobody would
		// find by reading the fold.
		Names.Sort([](const FString& Lhs, const FString& Rhs)
		{
			return Lhs.Compare(Rhs, ESearchCase::CaseSensitive) < 0;
		});

		// Length-prefixed rather than delimiter-separated. A define value is arbitrary text from a
		// settings row or a command line, so any separator that can be typed can also appear inside a
		// value -- and two different touched sets folding to one string is a build key that says "still
		// current" about an asset generated from different switches. Prefixing every piece with its
		// length is injective whatever the content is, which no choice of separator can promise.
		// (BuildSourceHash's own "\n--\n" separator is safe for the reason it gives: source text cannot
		// spell it. Values can spell anything.)
		FString Fragment = FString::Printf(TEXT("Defines=%d"), Names.Num()); /* I18N-EXEMPT: build key material, never displayed */
		Fragment.Reserve(Fragment.Len() + Names.Num() * 32);

		for (const FString& Name : Names)
		{
			const FString& Value = TouchedDefines.FindChecked(Name);
			Fragment += FString::Printf( /* I18N-EXEMPT: build key material, never displayed */
				TEXT("|%d:%s=%d:%s"),
				Name.Len(),
				*Name,
				Value.Len(),
				*Value);
		}

		return Fragment;
	}
}

#undef LOCTEXT_NAMESPACE
