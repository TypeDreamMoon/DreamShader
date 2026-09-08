// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The pure half of generation progress reporting: the shader-compile stall threshold, the
// Custom-node "this will unroll forever" heuristic, and the seam that lets a headless test pretend
// the user pressed Cancel.
//
// Everything here is a free function over plain strings and numbers on purpose. The generator's
// slow-task code cannot be exercised without a UI (FSlowTask::ShouldCancel answers false unless
// GIsSlowTask is set), so the parts worth testing are the parts that do not need one -- which is
// exactly what a scanner over HLSL text and a threshold over elapsed seconds are.

#pragma once

#include "CoreMinimal.h"

#include "Templates/Function.h"

namespace UE::DreamShader::Editor::Private
{
	// ---------------------------------------------------------------------------------------------
	// DSH9011 -- shader compilation took long enough to look like a hang
	// ---------------------------------------------------------------------------------------------

	/**
	 * How long one asset's shader compilation may take before the generator explains itself.
	 *
	 * Thirty seconds is not a performance budget -- plenty of legitimate materials pass it on a cold
	 * shader cache. It is the point past which a still progress bar stops reading as "working" and
	 * starts reading as "hung", which is the actual failure being reported (issue #29: users killed
	 * the editor because nothing on screen said the compile was still running).
	 */
	inline constexpr double GDreamShaderShaderCompileStallSeconds = 30.0;

	/** True when this stage has run past the threshold and has not already said so. */
	inline bool ShouldWarnOnShaderCompileStall(const double ElapsedSeconds, const bool bAlreadyWarned)
	{
		return !bAlreadyWarned && ElapsedSeconds >= GDreamShaderShaderCompileStallSeconds;
	}

	// ---------------------------------------------------------------------------------------------
	// DSH9012 -- dynamic loop bound + implicit-mip sampling in a Custom node
	// ---------------------------------------------------------------------------------------------

	/** What the scan found, so the diagnostic can name the loop bound and the sampler it objects to. */
	struct FDreamShaderDynamicLoopSampleFinding
	{
		bool bFound = false;

		/** The identifier that bounds the loop, which is also one of the node's input pins. */
		FString LoopBoundName;

		/** The implicit-mip sampling call that makes the unrolled loop expensive, e.g. `Texture3DSample`. */
		FString SampleCall;
	};

	/**
	 * Text-level helpers for the scan. Split out so each step is testable on its own and so the
	 * scanner below reads as the rule it implements rather than as a character loop.
	 */
	namespace CustomCodeScan
	{
		inline bool IsIdentifierChar(const TCHAR Char)
		{
			return FChar::IsAlnum(Char) || Char == TCHAR('_');
		}

		/**
		 * Replaces comment and string-literal content with spaces, preserving length and line breaks.
		 *
		 * Length preservation matters: every offset the scanner computes afterwards still points at
		 * the same character of the original text, so a finding can be reported against the source
		 * the user wrote.
		 */
		inline FString BlankCommentsAndStrings(const FString& Text)
		{
			FString Result = Text;
			bool bInLineComment = false;
			bool bInBlockComment = false;
			bool bInString = false;

			for (int32 Index = 0; Index < Result.Len(); ++Index)
			{
				const TCHAR Char = Result[Index];
				const TCHAR Next = Result.IsValidIndex(Index + 1) ? Result[Index + 1] : TCHAR('\0');

				if (bInLineComment)
				{
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					else
					{
						Result[Index] = TCHAR(' ');
					}
					continue;
				}

				if (bInBlockComment)
				{
					if (Char == TCHAR('*') && Next == TCHAR('/'))
					{
						Result[Index] = TCHAR(' ');
						Result[Index + 1] = TCHAR(' ');
						++Index;
						bInBlockComment = false;
					}
					else if (Char != TCHAR('\n'))
					{
						Result[Index] = TCHAR(' ');
					}
					continue;
				}

				if (bInString)
				{
					if (Char == TCHAR('\\') && Result.IsValidIndex(Index + 1))
					{
						Result[Index] = TCHAR(' ');
						Result[Index + 1] = TCHAR(' ');
						++Index;
					}
					else
					{
						if (Char == TCHAR('"'))
						{
							bInString = false;
						}
						Result[Index] = TCHAR(' ');
					}
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('/'))
				{
					bInLineComment = true;
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					++Index;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('*'))
				{
					bInBlockComment = true;
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					++Index;
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					Result[Index] = TCHAR(' ');
				}
			}

			return Result;
		}

		/** `Keyword` occurs at `Index` and is not part of a longer identifier (`for` vs `format`). */
		inline bool MatchKeyword(const FString& Text, const int32 Index, const TCHAR* Keyword)
		{
			const int32 Length = FCString::Strlen(Keyword);
			if (Index < 0 || Index + Length > Text.Len())
			{
				return false;
			}

			for (int32 Offset = 0; Offset < Length; ++Offset)
			{
				if (Text[Index + Offset] != Keyword[Offset])
				{
					return false;
				}
			}

			if (Index > 0 && IsIdentifierChar(Text[Index - 1]))
			{
				return false;
			}

			return !Text.IsValidIndex(Index + Length) || !IsIdentifierChar(Text[Index + Length]);
		}

		/** Names given a value by a `#define` in this body. A loop bounded by one of these is static. */
		inline void CollectDefinedMacros(const FString& Text, TSet<FString>& OutMacros)
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
			for (const FString& Line : Lines)
			{
				FString Rest = Line;
				Rest.TrimStartInline();
				if (!Rest.StartsWith(TEXT("#"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				Rest = Rest.RightChop(1);
				Rest.TrimStartInline();
				if (!Rest.StartsWith(TEXT("define"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				Rest = Rest.RightChop(6);
				if (!Rest.IsEmpty() && IsIdentifierChar(Rest[0]))
				{
					// `#defineFoo` is not a directive.
					continue;
				}

				Rest.TrimStartInline();
				int32 End = 0;
				while (End < Rest.Len() && IsIdentifierChar(Rest[End]))
				{
					++End;
				}

				if (End > 0)
				{
					OutMacros.Add(Rest.Left(End));
				}
			}
		}

		/** Every identifier in `Text`, numeric literals (and their `u`/`f` suffixes) excluded. */
		inline void CollectIdentifiers(const FString& Text, TArray<FString>& OutIdentifiers)
		{
			int32 Index = 0;
			while (Index < Text.Len())
			{
				const TCHAR Char = Text[Index];
				if (FChar::IsDigit(Char))
				{
					// Consume the whole literal, suffix included, so `64u` does not yield `u`.
					while (Index < Text.Len() && (IsIdentifierChar(Text[Index]) || Text[Index] == TCHAR('.')))
					{
						++Index;
					}
					continue;
				}

				if (IsIdentifierChar(Char))
				{
					const int32 Start = Index;
					while (Index < Text.Len() && IsIdentifierChar(Text[Index]))
					{
						++Index;
					}
					OutIdentifiers.AddUnique(Text.Mid(Start, Index - Start));
					continue;
				}

				++Index;
			}
		}

		/**
		 * Reads the `(...)` that follows a loop keyword and hands back what is between the outer
		 * parentheses. False when the next non-space character is not `(` or the parentheses never
		 * balance -- both of which mean this is not a loop header the scanner understands.
		 */
		inline bool TryReadLoopHeader(const FString& Text, const int32 KeywordEnd, FString& OutHeader, int32& OutHeaderEnd)
		{
			int32 Index = KeywordEnd;
			while (Index < Text.Len() && FChar::IsWhitespace(Text[Index]))
			{
				++Index;
			}

			if (!Text.IsValidIndex(Index) || Text[Index] != TCHAR('('))
			{
				return false;
			}

			const int32 Start = Index + 1;
			int32 Depth = 0;
			for (; Index < Text.Len(); ++Index)
			{
				if (Text[Index] == TCHAR('('))
				{
					++Depth;
				}
				else if (Text[Index] == TCHAR(')'))
				{
					--Depth;
					if (Depth == 0)
					{
						OutHeader = Text.Mid(Start, Index - Start);
						OutHeaderEnd = Index + 1;
						return true;
					}
				}
			}

			return false;
		}

		/** The middle clause of a `for` header. Falls back to the whole header when it has no `;`. */
		inline FString ExtractForCondition(const FString& Header)
		{
			TArray<FString> Clauses;
			int32 Depth = 0;
			int32 Start = 0;
			for (int32 Index = 0; Index < Header.Len(); ++Index)
			{
				const TCHAR Char = Header[Index];
				if (Char == TCHAR('(') || Char == TCHAR('['))
				{
					++Depth;
				}
				else if (Char == TCHAR(')') || Char == TCHAR(']'))
				{
					--Depth;
				}
				else if (Char == TCHAR(';') && Depth == 0)
				{
					Clauses.Add(Header.Mid(Start, Index - Start));
					Start = Index + 1;
				}
			}
			Clauses.Add(Header.Mid(Start));

			return Clauses.IsValidIndex(1) ? Clauses[1] : Header;
		}

		/**
		 * The first implicit-mip sampling call in `Text`, or false when there is none.
		 *
		 * "Implicit mip" is the whole point: `Texture2DSample` and `.Sample` derive their mip level
		 * from screen-space derivatives, which are undefined inside divergent control flow, so the
		 * compiler has to flatten the surrounding branch or unroll the surrounding loop to keep them
		 * defined. `SampleLevel`, `SampleGrad`, `SampleBias` and `SampleCmp*` take the level (or the
		 * gradients) as an argument and therefore cost nothing here -- and they are excluded for free,
		 * because a match requires the `(` to come immediately after the name.
		 */
		inline bool FindImplicitMipSampleCall(const FString& Text, FString& OutCall)
		{
			static const TCHAR* const SampleFunctions[] =
			{
				TEXT("Texture1DSample"),
				TEXT("Texture2DSample"),
				TEXT("Texture2DArraySample"),
				TEXT("Texture3DSample"),
				TEXT("TextureCubeSample"),
				TEXT("TextureCubeArraySample"),
				TEXT("TextureExternalSample"),
				TEXT(".Sample"),
			};

			for (const TCHAR* const Name : SampleFunctions)
			{
				const FString Needle(Name);
				const bool bIsMemberCall = Needle.StartsWith(TEXT("."), ESearchCase::CaseSensitive);

				int32 From = 0;
				while (true)
				{
					const int32 Found = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
					if (Found == INDEX_NONE)
					{
						break;
					}

					From = Found + Needle.Len();

					// A free function must start an identifier; a member call already has its `.`.
					if (!bIsMemberCall && Found > 0 && IsIdentifierChar(Text[Found - 1]))
					{
						continue;
					}

					int32 After = From;
					while (After < Text.Len() && FChar::IsWhitespace(Text[After]))
					{
						++After;
					}

					// `SampleLevel` / `SampleGrad` / `SampleBias` fail here: the next character is a
					// letter, not the open parenthesis of a call.
					if (Text.IsValidIndex(After) && Text[After] == TCHAR('('))
					{
						OutCall = bIsMemberCall ? FString(TEXT("Sample")) : Needle;
						return true;
					}
				}
			}

			return false;
		}
	}

	/**
	 * The DSH9012 rule: a loop whose bound is one of this Custom node's inputs, next to a sampling
	 * call whose mip level the compiler has to derive.
	 *
	 * Why the pair and not either half: a dynamic loop on its own compiles (the driver picks a real
	 * unroll factor or emits a real loop), and implicit-mip sampling on its own compiles (the loop
	 * around it is bounded, so unrolling terminates). Together they force the compiler to fully
	 * unroll an iteration count it cannot know, which is the shape that pins ShaderCompileWorker for
	 * minutes and looks, from the editor, exactly like a hung plugin.
	 *
	 * Deliberately loose in two places. The sampling call is looked for anywhere in the body rather
	 * than only inside the loop, because tracking the loop's braces through preprocessor directives
	 * and nested scopes buys precision the warning does not need -- it is advisory, never blocking.
	 * And the bound is matched case-insensitively, because DreamShaderLang resolves property and
	 * input names that way everywhere else.
	 *
	 * `InputNames` are the node's input pin names. A bound naming anything else -- a local, a literal,
	 * a `#define`d constant -- is static as far as the shader compiler is concerned and is not
	 * reported.
	 */
	inline bool ScanCustomCodeForDynamicLoopSampling(
		const FString& CustomCode,
		const TArray<FString>& InputNames,
		FDreamShaderDynamicLoopSampleFinding& OutFinding)
	{
		OutFinding = FDreamShaderDynamicLoopSampleFinding();
		if (CustomCode.IsEmpty() || InputNames.IsEmpty())
		{
			return false;
		}

		const FString Scannable = CustomCodeScan::BlankCommentsAndStrings(CustomCode);

		// The cheap half first: no implicit-mip sample means nothing to warn about, whatever the
		// loops look like.
		FString SampleCall;
		if (!CustomCodeScan::FindImplicitMipSampleCall(Scannable, SampleCall))
		{
			return false;
		}

		TSet<FString> DefinedMacros;
		CustomCodeScan::CollectDefinedMacros(Scannable, DefinedMacros);

		for (int32 Index = 0; Index < Scannable.Len(); ++Index)
		{
			int32 KeywordEnd = INDEX_NONE;
			bool bIsForLoop = false;
			if (CustomCodeScan::MatchKeyword(Scannable, Index, TEXT("for")))
			{
				KeywordEnd = Index + 3;
				bIsForLoop = true;
			}
			else if (CustomCodeScan::MatchKeyword(Scannable, Index, TEXT("while")))
			{
				KeywordEnd = Index + 5;
			}
			else
			{
				continue;
			}

			FString Header;
			int32 HeaderEnd = KeywordEnd;
			if (!CustomCodeScan::TryReadLoopHeader(Scannable, KeywordEnd, Header, HeaderEnd))
			{
				continue;
			}

			TArray<FString> Identifiers;
			CustomCodeScan::CollectIdentifiers(
				bIsForLoop ? CustomCodeScan::ExtractForCondition(Header) : Header,
				Identifiers);

			for (const FString& Identifier : Identifiers)
			{
				if (DefinedMacros.Contains(Identifier))
				{
					continue;
				}

				for (const FString& InputName : InputNames)
				{
					if (!InputName.IsEmpty() && InputName.Equals(Identifier, ESearchCase::IgnoreCase))
					{
						OutFinding.bFound = true;
						OutFinding.LoopBoundName = Identifier;
						OutFinding.SampleCall = SampleCall;
						return true;
					}
				}
			}

			// Resume after this header; the body is scanned too, so nested loops are still seen.
			Index = FMath::Max(Index, HeaderEnd - 1);
		}

		return false;
	}

	// ---------------------------------------------------------------------------------------------
	// Cancellation test seam
	// ---------------------------------------------------------------------------------------------

	using FDreamShaderGenerationCancelPredicate = TFunction<bool()>;

	/**
	 * Consulted by the generator in place of FScopedSlowTask::ShouldCancel when it is set.
	 *
	 * It exists because ShouldCancel cannot answer true without a UI: it is gated on `GIsSlowTask`,
	 * which only the progress dialog sets, so an automation run has no way to reach the cancel path
	 * at all. Left unset in every non-test build, and the generator falls through to the real one.
	 */
	inline FDreamShaderGenerationCancelPredicate& GetDreamShaderGenerationCancelOverride()
	{
		static FDreamShaderGenerationCancelPredicate Override;
		return Override;
	}

	/** Arms the override for a scope and puts back whatever was there before. */
	struct FScopedDreamShaderGenerationCancelOverride
	{
		explicit FScopedDreamShaderGenerationCancelOverride(FDreamShaderGenerationCancelPredicate InPredicate)
			: Saved(MoveTemp(GetDreamShaderGenerationCancelOverride()))
		{
			GetDreamShaderGenerationCancelOverride() = MoveTemp(InPredicate);
		}

		~FScopedDreamShaderGenerationCancelOverride()
		{
			GetDreamShaderGenerationCancelOverride() = MoveTemp(Saved);
		}

		FScopedDreamShaderGenerationCancelOverride(const FScopedDreamShaderGenerationCancelOverride&) = delete;
		FScopedDreamShaderGenerationCancelOverride& operator=(const FScopedDreamShaderGenerationCancelOverride&) = delete;

	private:
		FDreamShaderGenerationCancelPredicate Saved;
	};
}
