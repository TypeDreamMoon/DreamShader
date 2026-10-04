// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See LangHlslText.h.

#include "Lang/LangHlslText.h"

#include "IR/IR.h"

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

		static int32 CountNewlines(const FString& Text)
		{
			int32 Count = 0;
			for (const TCHAR Character : Text)
			{
				Count += Character == TCHAR('\n') ? 1 : 0;
			}
			return Count;
		}

		/** Appends to a root, recording which lines came from where. */
		struct FRootWriter
		{
			FHlslInlineRoot& Root;
			/** The 1-based line the next character lands on. */
			int32 Line = 1;

			explicit FRootWriter(FHlslInlineRoot& InRoot) : Root(InRoot) {}

			void Generated(const FString& Text)
			{
				Root.Text += Text;
				Line += CountNewlines(Text);
			}

			/** Text from the `.dsp`, whose first character is on FirstSourceLine there. Always ends a line. */
			void Mapped(const FString& Text, const int32 FirstSourceLine, const bool bShared)
			{
				if (Text.IsEmpty())
				{
					return;
				}
				const bool bEndsLine = Text.EndsWith(TEXT("\n"), ESearchCase::CaseSensitive);
				const int32 Newlines = CountNewlines(Text);

				FHlslRootLineRun& Run = Root.Runs.AddDefaulted_GetRef();
				Run.FirstRootLine = Line;
				Run.FirstSourceLine = FirstSourceLine;
				Run.bShared = bShared;
				// A text that ends its last line has no line after it: the next text's first line is not this run's.
				Run.LineCount = Newlines + (bEndsLine ? 0 : 1);

				Root.Text += Text;
				Line += Newlines;
				if (!bEndsLine)
				{
					Root.Text += TEXT("\n");
					++Line;
				}
			}
		};
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

	bool FHlslInlineRoot::MapLine(const int32 RootLine, int32& OutSourceLine, bool& bOutShared) const
	{
		for (const FHlslRootLineRun& Run : Runs)
		{
			if (RootLine >= Run.FirstRootLine && RootLine < Run.FirstRootLine + Run.LineCount)
			{
				OutSourceLine = Run.FirstSourceLine + (RootLine - Run.FirstRootLine);
				bOutShared = Run.bShared;
				return true;
			}
		}
		return false;
	}

	bool BuildDreamPassInlineHlslRoot(const IR::FIRPassPipeline& Pipeline, const int32 PassIndex, FHlslInlineRoot& OutRoot, FString& OutError)
	{
		using namespace LangHlslTextPrivate;

		OutRoot = FHlslInlineRoot();
		OutError.Reset();
		if (!Pipeline.Passes.IsValidIndex(PassIndex))
		{
			OutError = TEXT("no such pass"); /* I18N-EXEMPT: internal, wrapped by the caller's diagnostic */
			return false;
		}

		const IR::FIRPass& Pass = Pipeline.Passes[PassIndex];
		const FString& Source = Pass.HlslSource;
		if (!IR::PassHlslSource::IsInline(Source))
		{
			OutError = TEXT("the pass's HLSL is not inline"); /* I18N-EXEMPT: internal, wrapped by the caller's diagnostic */
			return false;
		}
		const bool bCompute = Pass.Kind.Equals(TEXT("compute"), ESearchCase::CaseSensitive);
		const bool bAtBeginView = Pass.Injection.Equals(TEXT("BeginView"), ESearchCase::CaseSensitive);
		const bool bShared = Source.Equals(IR::PassHlslSource::Shared, ESearchCase::CaseSensitive);

		FRootWriter Writer(OutRoot);
		Writer.Generated(FString::Printf(
			TEXT("// Pass %s: HLSL written inline in its .dsp. Generated by DreamShader from the pre-checked source; do not edit.\n"), /* I18N-EXEMPT: HLSL comment */
			*Pass.Name));

		// ---- the file's block: its shared functions, with every entry a Shared pass names blanked -- this pass's own included,
		// which comes after them below
		FHlslTextScan SharedScan;
		if (Pipeline.bHasSharedHlsl)
		{
			SharedScan = ScanHlslText(Pipeline.SharedHlsl);
			// By exact spelling, as HLSL names are told apart: a TSet<FString> would ignore case.
			TArray<FString> Entries;
			for (const IR::FIRPass& Other : Pipeline.Passes)
			{
				if (Other.HlslSource.Equals(IR::PassHlslSource::Shared, ESearchCase::CaseSensitive) && !Other.Entry.IsEmpty())
				{
					Entries.Add(Other.Entry);
				}
			}
			FString Shared = Pipeline.SharedHlsl;
			for (const FHlslTopLevelFunction& Function : SharedScan.Functions)
			{
				const bool bEntry = Entries.ContainsByPredicate([&Function](const FString& Entry)
				{
					return Entry.Equals(Function.Name, ESearchCase::CaseSensitive);
				});
				if (bEntry)
				{
					Shared = BlankHlslRange(Shared, Function.DeclarationStart, Function.End);
				}
			}
			Writer.Mapped(Shared, Pipeline.SharedHlslLine, /*bShared*/ true);
		}

		// ---- the guard: at BeginView the view uniform buffer does not exist yet, so a use of `View` in this pass's own code
		// is an undeclared name rather than a crash. Not around the shared functions, which other passes may use with it.
		if (bAtBeginView)
		{
			Writer.Generated(TEXT("#define View DP_NoViewAtBeginView\n")); /* I18N-EXEMPT: HLSL */
		}

		// ---- this pass's code
		if (bShared)
		{
			const FHlslTopLevelFunction* Entry = SharedScan.FindFunction(Pass.Entry);
			if (!Pipeline.bHasSharedHlsl || !Entry)
			{
				OutError = FString::Printf(TEXT("the file's hlsl block defines no function '%s'"), *Pass.Entry); /* I18N-EXEMPT: internal, wrapped by the caller's diagnostic */
				return false;
			}
			// From the start of the entry's first line, what came before it on that line blanked: its columns stay its own.
			int32 LineStart = Entry->DeclarationStart;
			while (LineStart > 0 && Pipeline.SharedHlsl[LineStart - 1] != TCHAR('\n'))
			{
				--LineStart;
			}
			FString Code = Pipeline.SharedHlsl.Mid(LineStart, Entry->End - LineStart);
			Code = BlankHlslRange(Code, 0, Entry->DeclarationStart - LineStart);
			const int32 FirstLine = Pipeline.SharedHlslLine + GetHlslLineOfOffset(Pipeline.SharedHlsl, LineStart) - 1;
			Writer.Mapped(Code, FirstLine, /*bShared*/ false);
		}
		else if (Source.Equals(IR::PassHlslSource::Block, ESearchCase::CaseSensitive))
		{
			Writer.Mapped(Pass.InlineHlsl, Pass.InlineHlslLine, /*bShared*/ false);
		}
		else
		{
			// The body form: the statements inside a function the compiler writes, with the names DreamShader_Plan/10 3.2 gives.
			if (bCompute)
			{
				Writer.Generated(FString::Printf(
					TEXT("[numthreads(%d, %d, %d)]\n") /* I18N-EXEMPT: HLSL */
					TEXT("void %s(uint3 Id : SV_DispatchThreadID, uint3 GroupId : SV_GroupID, uint3 LocalId : SV_GroupThreadID, uint LocalIndex : SV_GroupIndex)\n") /* I18N-EXEMPT: HLSL */
					TEXT("{\n") /* I18N-EXEMPT: HLSL */
					TEXT("\tif (any(Id >= DP_DispatchSize.xyz))\n") /* I18N-EXEMPT: HLSL */
					TEXT("\t{\n\t\treturn;\n\t}\n"), /* I18N-EXEMPT: HLSL */
					FMath::Max(Pass.ThreadsX, 1), FMath::Max(Pass.ThreadsY, 1), FMath::Max(Pass.ThreadsZ, 1),
					GetDreamPassMainEntryName(true)));
			}
			else
			{
				TArray<FString> Outputs;
				for (const IR::FIRPassBinding& Write : Pass.Writes)
				{
					Outputs.Add(Write.Slot.IsEmpty() ? Write.Buffer : Write.Slot);
				}
				FString Signature = FString::Printf(TEXT("void %s(float4 SvPosition : SV_POSITION"), GetDreamPassMainEntryName(false)); /* I18N-EXEMPT: HLSL */
				for (int32 Index = 0; Index < Outputs.Num(); ++Index)
				{
					Signature += FString::Printf(TEXT(", out float4 %s : SV_Target%d"), *Outputs[Index], Index); /* I18N-EXEMPT: HLSL */
				}
				Signature += TEXT(")\n{\n"); /* I18N-EXEMPT: HLSL */
				Signature += TEXT("\tconst float2 Pixel = SvPosition.xy - float2(DP_ViewRect.xy);\n"); /* I18N-EXEMPT: HLSL */
				Signature += TEXT("\tconst float2 UV = Pixel / float2(DP_ViewRect.zw - DP_ViewRect.xy);\n"); /* I18N-EXEMPT: HLSL */
				for (const FString& Output : Outputs)
				{
					Signature += FString::Printf(TEXT("\t%s = 0;\n"), *Output); /* I18N-EXEMPT: HLSL */
				}
				Writer.Generated(Signature);
			}
			Writer.Mapped(Pass.InlineHlsl, Pass.InlineHlslLine, /*bShared*/ false);
			Writer.Generated(TEXT("}\n")); /* I18N-EXEMPT: HLSL */
		}

		if (bAtBeginView)
		{
			Writer.Generated(TEXT("#undef View\n")); /* I18N-EXEMPT: HLSL */
		}
		return true;
	}
}
