// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See LangHlslText.h.

#include "Lang/LangHlslText.h"

namespace UE::DreamShader::Lang
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangHlslTextPrivate
	{
		static bool IsIdentifierStart(const TCHAR Character)
		{
			return (Character >= TCHAR('A') && Character <= TCHAR('Z'))
				|| (Character >= TCHAR('a') && Character <= TCHAR('z'))
				|| Character == TCHAR('_');
		}

		static bool IsDigit(const TCHAR Character)
		{
			return Character >= TCHAR('0') && Character <= TCHAR('9');
		}

		static bool IsIdentifierCharacter(const TCHAR Character)
		{
			return IsIdentifierStart(Character) || IsDigit(Character);
		}

		/** Past a string literal opened at Open; a literal the line does not close ends at its newline. */
		static int32 SkipStringLiteral(const FString& Text, const int32 Open)
		{
			int32 Index = Open + 1;
			while (Index < Text.Len())
			{
				const TCHAR Character = Text[Index];
				if (Character == TCHAR('\\'))
				{
					Index += 2;
					continue;
				}
				if (Character == TCHAR('"'))
				{
					return Index + 1;
				}
				if (Character == TCHAR('\n'))
				{
					return Index;
				}
				++Index;
			}
			return Text.Len();
		}

		/** Past a number starting at Start: digits, a fraction, an exponent, a hex prefix, a suffix -- `2.0f`, `0x1F`, `1e-3`. */
		static int32 SkipNumber(const FString& Text, int32 Index)
		{
			while (Index < Text.Len())
			{
				const TCHAR Character = Text[Index];
				if (IsIdentifierCharacter(Character) || Character == TCHAR('.'))
				{
					++Index;
					continue;
				}
				// The sign of an exponent: `1e-3`, `2.5E+2`.
				if ((Character == TCHAR('-') || Character == TCHAR('+')) && Index > 0 && (Text[Index - 1] == TCHAR('e') || Text[Index - 1] == TCHAR('E')))
				{
					++Index;
					continue;
				}
				break;
			}
			return Index;
		}

		static int32 SkipWhitespace(const FString& Text, int32 Index)
		{
			while (Index < Text.Len() && FChar::IsWhitespace(Text[Index]))
			{
				++Index;
			}
			return Index;
		}

		/** Past the bracket that closes the one at Open (`(`, `[` or `{`), strings skipped; INDEX_NONE when there is none. */
		static int32 FindClosing(const FString& Text, const int32 Open, const TCHAR OpenCharacter, const TCHAR CloseCharacter)
		{
			int32 Depth = 0;
			for (int32 Index = Open; Index < Text.Len(); )
			{
				const TCHAR Character = Text[Index];
				if (Character == TCHAR('"'))
				{
					Index = SkipStringLiteral(Text, Index);
					continue;
				}
				if (Character == OpenCharacter)
				{
					++Depth;
				}
				else if (Character == CloseCharacter && --Depth == 0)
				{
					return Index + 1;
				}
				++Index;
			}
			return INDEX_NONE;
		}

		/**
		 * The text with every preprocessor line, and its continuation lines, blanked; newlines kept. A `#if` around part of a
		 * block is the shader compiler's to evaluate: the scan sees both branches, which is what a scan for names should see.
		 */
		static FString BlankPreprocessorLines(const FString& Text)
		{
			FString Result = Text;
			bool bAtLineStart = true;
			bool bInDirective = false;
			for (int32 Index = 0; Index < Result.Len(); ++Index)
			{
				const TCHAR Character = Result[Index];
				if (Character == TCHAR('\n'))
				{
					// A directive goes on past its newline only when the line ended in a backslash.
					const bool bContinued = bInDirective && Index > 0
						&& (Result[Index - 1] == TCHAR('\\') || (Result[Index - 1] == TCHAR('\r') && Index > 1 && Result[Index - 2] == TCHAR('\\')));
					bInDirective = bContinued;
					bAtLineStart = true;
					continue;
				}
				if (bAtLineStart && !FChar::IsWhitespace(Character))
				{
					bAtLineStart = false;
					bInDirective = Character == TCHAR('#');
				}
				if (bInDirective && Character != TCHAR('\r'))
				{
					Result[Index] = TCHAR(' ');
				}
			}
			return Result;
		}

		/** `numthreads(8, 8, 1)` inside an attribute: the group size when all three are positive integer literals. */
		static bool TryParseNumThreads(const FString& AttributeText, FIntVector& OutGroupSize)
		{
			const FString Trimmed = AttributeText.TrimStartAndEnd();
			if (!Trimmed.StartsWith(TEXT("numthreads"), ESearchCase::IgnoreCase))
			{
				return false;
			}
			int32 Open = INDEX_NONE;
			int32 Close = INDEX_NONE;
			if (!Trimmed.FindChar(TCHAR('('), Open) || !Trimmed.FindLastChar(TCHAR(')'), Close) || Close <= Open)
			{
				return false;
			}
			TArray<FString> Arguments;
			Trimmed.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Arguments, TEXT(","), /*InCullEmpty*/ false);
			if (Arguments.Num() != 3)
			{
				return false;
			}
			int32 Values[3] = { 0, 0, 0 };
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				FString Argument = Arguments[Axis].TrimStartAndEnd();
				Argument.RemoveFromEnd(TEXT("u"), ESearchCase::IgnoreCase);
				if (Argument.IsEmpty() || !Argument.IsNumeric() || Argument.Contains(TEXT(".")) || Argument.Contains(TEXT("-")))
				{
					return false;
				}
				Values[Axis] = FCString::Atoi(*Argument);
				if (Values[Axis] <= 0)
				{
					return false;
				}
			}
			OutGroupSize = FIntVector(Values[0], Values[1], Values[2]);
			return true;
		}

		/** Words after which a parenthesized part and a brace are a statement, not a function's parameters and body. */
		static bool IsStatementKeyword(const FString& Word)
		{
			static const TCHAR* const Keywords[] = { TEXT("if"), TEXT("for"), TEXT("while"), TEXT("switch"), TEXT("do"), TEXT("return"), TEXT("else") };
			for (const TCHAR* Keyword : Keywords)
			{
				if (Word.Equals(Keyword, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}

		/** Words that start a declaration no function body can hold. */
		static bool IsDeclarationKeyword(const FString& Word)
		{
			static const TCHAR* const Keywords[] = { TEXT("groupshared"), TEXT("struct"), TEXT("cbuffer"), TEXT("tbuffer") };
			for (const TCHAR* Keyword : Keywords)
			{
				if (Word.Equals(Keyword, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}
	}

	const FHlslTopLevelFunction* FHlslTextScan::FindFunction(const FString& Name) const
	{
		return Functions.FindByPredicate([&Name](const FHlslTopLevelFunction& Function)
		{
			return Function.Name.Equals(Name, ESearchCase::CaseSensitive);
		});
	}

	FString BlankHlslComments(const FString& Text)
	{
		using namespace LangHlslTextPrivate;

		FString Result = Text;
		const int32 Length = Result.Len();
		for (int32 Index = 0; Index < Length; )
		{
			const TCHAR Character = Result[Index];
			if (Character == TCHAR('"'))
			{
				Index = SkipStringLiteral(Result, Index);
				continue;
			}
			if (Character == TCHAR('/') && Index + 1 < Length && Result[Index + 1] == TCHAR('/'))
			{
				while (Index < Length && Result[Index] != TCHAR('\n'))
				{
					if (Result[Index] != TCHAR('\r'))
					{
						Result[Index] = TCHAR(' ');
					}
					++Index;
				}
				continue;
			}
			if (Character == TCHAR('/') && Index + 1 < Length && Result[Index + 1] == TCHAR('*'))
			{
				Result[Index] = TCHAR(' ');
				Result[Index + 1] = TCHAR(' ');
				Index += 2;
				while (Index < Length && !(Result[Index] == TCHAR('*') && Index + 1 < Length && Result[Index + 1] == TCHAR('/')))
				{
					if (Result[Index] != TCHAR('\n') && Result[Index] != TCHAR('\r'))
					{
						Result[Index] = TCHAR(' ');
					}
					++Index;
				}
				if (Index + 1 < Length)
				{
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					Index += 2;
				}
				continue;
			}
			++Index;
		}
		return Result;
	}

	FString BlankHlslRange(const FString& Text, const int32 Start, const int32 End)
	{
		FString Result = Text;
		const int32 First = FMath::Clamp(Start, 0, Result.Len());
		const int32 Last = FMath::Clamp(End, First, Result.Len());
		for (int32 Index = First; Index < Last; ++Index)
		{
			if (Result[Index] != TCHAR('\n') && Result[Index] != TCHAR('\r'))
			{
				Result[Index] = TCHAR(' ');
			}
		}
		return Result;
	}

	FHlslTextScan ScanHlslText(const FString& Text)
	{
		using namespace LangHlslTextPrivate;

		FHlslTextScan Scan;
		const FString NoComments = BlankHlslComments(Text);

		// The `#include` lines, read before the preprocessor lines are blanked.
		{
			int32 Line = 1;
			bool bAtLineStart = true;
			for (int32 Index = 0; Index < NoComments.Len(); ++Index)
			{
				const TCHAR Character = NoComments[Index];
				if (Character == TCHAR('\n'))
				{
					++Line;
					bAtLineStart = true;
					continue;
				}
				if (!bAtLineStart || FChar::IsWhitespace(Character))
				{
					continue;
				}
				bAtLineStart = false;
				if (Character == TCHAR('#'))
				{
					int32 Word = Index + 1;
					while (Word < NoComments.Len() && (NoComments[Word] == TCHAR(' ') || NoComments[Word] == TCHAR('\t')))
					{
						++Word;
					}
					if (NoComments.Mid(Word, 7).Equals(TEXT("include"), ESearchCase::CaseSensitive))
					{
						Scan.IncludeLines.Add(Line);
					}
				}
			}
		}

		const FString Code = BlankPreprocessorLines(NoComments);
		const int32 Length = Code.Len();

		int32 Depth = 0;
		// The first character of the top-level declaration (or statement) being read; INDEX_NONE between two of them.
		int32 DeclarationStart = INDEX_NONE;
		// The attributes in front of it: `[numthreads(8, 8, 1)]` belongs to the function that follows.
		TArray<FString> PendingAttributes;
		FString LastIdentifier;
		int32 LastIdentifierStart = INDEX_NONE;
		int32 LastIdentifierEnd = INDEX_NONE;

		const auto EndDeclaration = [&DeclarationStart, &PendingAttributes, &LastIdentifier]()
		{
			DeclarationStart = INDEX_NONE;
			PendingAttributes.Reset();
			LastIdentifier.Reset();
		};

		for (int32 Index = 0; Index < Length; )
		{
			const TCHAR Character = Code[Index];
			if (FChar::IsWhitespace(Character))
			{
				++Index;
				continue;
			}
			if (Depth == 0 && DeclarationStart == INDEX_NONE)
			{
				DeclarationStart = Index;
			}

			if (Character == TCHAR('"'))
			{
				Index = SkipStringLiteral(Code, Index);
				LastIdentifier.Reset();
				continue;
			}
			if (Character == TCHAR('{'))
			{
				++Depth;
				++Index;
				LastIdentifier.Reset();
				continue;
			}
			if (Character == TCHAR('}'))
			{
				++Index;
				if (Depth == 0)
				{
					Scan.bBalanced = false;
					EndDeclaration();
					continue;
				}
				if (--Depth == 0)
				{
					// The end of a statement block, a struct or a cbuffer at the top level.
					EndDeclaration();
				}
				continue;
			}
			if (Depth > 0)
			{
				// Inside braces nothing but strings and braces matters to the top level.
				++Index;
				continue;
			}

			if (Character == TCHAR(';'))
			{
				EndDeclaration();
				++Index;
				continue;
			}
			if (Character == TCHAR('['))
			{
				// An attribute in front of a declaration (`[numthreads(8, 8, 1)]`), or a statement's (`[unroll]`).
				const int32 AfterBracket = FindClosing(Code, Index, TCHAR('['), TCHAR(']'));
				if (AfterBracket == INDEX_NONE)
				{
					Scan.bBalanced = false;
					break;
				}
				const FString Attribute = Code.Mid(Index + 1, AfterBracket - Index - 2);
				PendingAttributes.Add(Attribute);
				// Only a compute entry has one, whatever its sizes are written with: a block that carries it is declarations.
				if (Attribute.TrimStart().StartsWith(TEXT("numthreads"), ESearchCase::IgnoreCase))
				{
					Scan.bHasDeclarations = true;
				}
				Index = AfterBracket;
				LastIdentifier.Reset();
				continue;
			}
			if (IsDigit(Character) || (Character == TCHAR('.') && Index + 1 < Length && IsDigit(Code[Index + 1])))
			{
				Index = SkipNumber(Code, Index);
				LastIdentifier.Reset();
				continue;
			}
			if (IsIdentifierStart(Character))
			{
				int32 End = Index + 1;
				while (End < Length && IsIdentifierCharacter(Code[End]))
				{
					++End;
				}
				LastIdentifier = Code.Mid(Index, End - Index);
				LastIdentifierStart = Index;
				LastIdentifierEnd = End;
				if (IsDeclarationKeyword(LastIdentifier))
				{
					Scan.bHasDeclarations = true;
				}
				Index = End;
				continue;
			}
			if (Character == TCHAR('(') && !LastIdentifier.IsEmpty() && SkipWhitespace(Code, LastIdentifierEnd) == Index)
			{
				const int32 AfterParameters = FindClosing(Code, Index, TCHAR('('), TCHAR(')'));
				if (AfterParameters == INDEX_NONE)
				{
					Scan.bBalanced = false;
					break;
				}

				// `) : SV_Target0 {` -- a semantic between the parameter list and the body is allowed.
				int32 Cursor = SkipWhitespace(Code, AfterParameters);
				if (Cursor < Length && Code[Cursor] == TCHAR(':'))
				{
					Cursor = SkipWhitespace(Code, Cursor + 1);
					while (Cursor < Length && IsIdentifierCharacter(Code[Cursor]))
					{
						++Cursor;
					}
					Cursor = SkipWhitespace(Code, Cursor);
				}

				if (Cursor < Length && Code[Cursor] == TCHAR('{') && !IsStatementKeyword(LastIdentifier))
				{
					const int32 AfterBody = FindClosing(Code, Cursor, TCHAR('{'), TCHAR('}'));
					FHlslTopLevelFunction& Function = Scan.Functions.AddDefaulted_GetRef();
					Function.Name = LastIdentifier;
					Function.DeclarationStart = DeclarationStart != INDEX_NONE ? DeclarationStart : LastIdentifierStart;
					Function.NameOffset = LastIdentifierStart;
					Function.BodyStart = Cursor;
					Function.End = AfterBody != INDEX_NONE ? AfterBody : Length;
					for (const FString& Attribute : PendingAttributes)
					{
						if (TryParseNumThreads(Attribute, Function.GroupSize))
						{
							Function.bComputeEntry = true;
							break;
						}
					}
					Scan.bHasDeclarations = true;
					if (AfterBody == INDEX_NONE)
					{
						Scan.bBalanced = false;
						break;
					}
					Index = AfterBody;
					EndDeclaration();
					continue;
				}

				// A call, or the head of a statement (`if (...)`, `for (...)`): the parentheses are skipped, and a brace after
				// them is counted as any other.
				Index = AfterParameters;
				LastIdentifier.Reset();
				continue;
			}

			// Anything else between a name and a `(` -- `=`, `,`, an operator -- means the name was not a function's.
			LastIdentifier.Reset();
			++Index;
		}

		if (Depth != 0)
		{
			Scan.bBalanced = false;
		}
		return Scan;
	}

	void FindHlslIdentifiers(const FString& Text, TArray<FHlslIdentifier>& OutIdentifiers)
	{
		using namespace LangHlslTextPrivate;

		OutIdentifiers.Reset();
		const FString Code = BlankHlslComments(Text);
		const int32 Length = Code.Len();
		for (int32 Index = 0; Index < Length; )
		{
			const TCHAR Character = Code[Index];
			if (Character == TCHAR('"'))
			{
				Index = SkipStringLiteral(Code, Index);
				continue;
			}
			if (IsDigit(Character) || (Character == TCHAR('.') && Index + 1 < Length && IsDigit(Code[Index + 1])))
			{
				Index = SkipNumber(Code, Index);
				continue;
			}
			if (IsIdentifierStart(Character))
			{
				int32 End = Index + 1;
				while (End < Length && IsIdentifierCharacter(Code[End]))
				{
					++End;
				}
				FHlslIdentifier& Identifier = OutIdentifiers.AddDefaulted_GetRef();
				Identifier.Name = Code.Mid(Index, End - Index);
				Identifier.Offset = Index;
				const int32 Next = SkipWhitespace(Code, End);
				Identifier.bFollowedByParenthesis = Next < Length && Code[Next] == TCHAR('(');
				Index = End;
				continue;
			}
			++Index;
		}
	}

	int32 GetHlslLineOfOffset(const FString& Text, const int32 Offset)
	{
		int32 Line = 1;
		const int32 Last = FMath::Clamp(Offset, 0, Text.Len());
		for (int32 Index = 0; Index < Last; ++Index)
		{
			Line += Text[Index] == TCHAR('\n') ? 1 : 0;
		}
		return Line;
	}

	const TCHAR* GetDreamPassMainEntryName(const bool bCompute)
	{
		return bCompute ? TEXT("DreamPassMainCS") : TEXT("DreamPassMainPS");
	}

	TConstArrayView<const TCHAR*> GetDreamPassBodyFormNames(const bool bCompute)
	{
		static const TCHAR* const ComputeNames[] = { TEXT("Id"), TEXT("GroupId"), TEXT("LocalId"), TEXT("LocalIndex") };
		static const TCHAR* const PixelNames[] = { TEXT("SvPosition"), TEXT("Pixel"), TEXT("UV") };
		return bCompute ? TConstArrayView<const TCHAR*>(ComputeNames) : TConstArrayView<const TCHAR*>(PixelNames);
	}
}
