// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A Custom node's Code, read back into the `/// @custom` functions it was built from.
//
// The grammar is the one IRCustomHlsl.cpp writes (FCustomCodeBuilder::Build):
//
//     struct generated_wrapper_<Name>_<CRC>          only when the body calls other @custom functions
//     {
//     <TAB><ret> DreamShaderFn_<Helper>(<params>)    one per embedded helper, dependency first
//     <TAB>{
//     <TAB><TAB><out> = (<T>)0;                       one per `out` parameter
//     <TAB><TAB><body block>
//     <TAB>}
//     <empty>
//     };
//     <WrapperType> __ds_wrapper_<CRC>;
//     <empty>
//     <body block>                                    the node's own function
//
// and a body block is
//
//     [return]                                        a bare expression body
//     [<T> <Out> = (<T>)0;]                           legacy rule L9, the node's own block only
//     // Begin DreamShader source: <file>
//     // DreamShader custom: <Function> line <N>
//     <the body, one emitted line per source line>
//     // End DreamShader source: <file>
//     [;] | [return <fallback>;]
//
// What the builder changed inside a body is undone here: a call to a sibling reads `Name(...)` again without the
// sampler arguments the builder added, and the `#include` lines it blanked are written back over their blanks.
// The sampler sugar stays expanded -- `Texture2DSample(Tex, TexSampler, UV)` is HLSL the builder leaves alone, so
// the text is a fixed point from here on.
//
// Anything else is a node somebody wrote by hand; RecoverCustomCode answers false and the caller keeps the code
// verbatim as the body of a function of its own.

#include "IRToAstInternal.h"

#include "IR/IRCustomHlsl.h"

namespace UE::DreamShader::Lang::DecompileAst
{
	namespace CustomRecover
	{
		static const TCHAR* const WrapperStructPrefix = TEXT("struct generated_wrapper_");
		static const TCHAR* const WrapperVariablePrefix = TEXT("__ds_wrapper_");
		static const TCHAR* const IncludeOpen = TEXT("#include \"");

		static bool IsIdentifierStartChar(const TCHAR Character)
		{
			return (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| Character == TEXT('_');
		}

		static bool IsIdentifierPartChar(const TCHAR Character)
		{
			return IsIdentifierStartChar(Character) || (Character >= TEXT('0') && Character <= TEXT('9'));
		}

		static bool IsWhitespaceOnly(const FString& Text)
		{
			for (const TCHAR Character : Text)
			{
				if (!FChar::IsWhitespace(Character))
				{
					return false;
				}
			}
			return true;
		}

		static bool IsTextureSpelling(const FString& TypeSpelling)
		{
			return TypeSpelling.StartsWith(TEXT("Texture"), ESearchCase::CaseSensitive)
				|| TypeSpelling.Equals(TEXT("VolumeTexture"), ESearchCase::CaseSensitive);
		}

		/** At a comment or a string: the index just past it. Anywhere else: Index. */
		static int32 SkipCommentOrString(const FString& Text, const int32 Index)
		{
			if (!Text.IsValidIndex(Index))
			{
				return Index;
			}

			const TCHAR Character = Text[Index];
			if (Character == TEXT('/') && Text.IsValidIndex(Index + 1))
			{
				if (Text[Index + 1] == TEXT('/'))
				{
					int32 End = Index + 2;
					while (End < Text.Len() && Text[End] != TEXT('\n'))
					{
						++End;
					}
					return End;
				}
				if (Text[Index + 1] == TEXT('*'))
				{
					int32 End = Index + 2;
					while (End + 1 < Text.Len() && !(Text[End] == TEXT('*') && Text[End + 1] == TEXT('/')))
					{
						++End;
					}
					return FMath::Min(End + 2, Text.Len());
				}
			}
			if (Character == TEXT('"'))
			{
				int32 End = Index + 1;
				while (End < Text.Len() && Text[End] != TEXT('"') && Text[End] != TEXT('\n'))
				{
					End += (Text[End] == TEXT('\\') && End + 1 < Text.Len()) ? 2 : 1;
				}
				return FMath::Min(End + 1, Text.Len());
			}
			return Index;
		}

		/** The `)` that closes the `(` at OpenIndex, or INDEX_NONE. */
		static int32 FindClosingParenthesis(const FString& Text, const int32 OpenIndex)
		{
			int32 Depth = 0;
			int32 Index = OpenIndex;
			while (Index < Text.Len())
			{
				const int32 Skipped = SkipCommentOrString(Text, Index);
				if (Skipped != Index)
				{
					Index = Skipped;
					continue;
				}

				const TCHAR Character = Text[Index];
				if (Character == TEXT('(') || Character == TEXT('[') || Character == TEXT('{'))
				{
					++Depth;
				}
				else if (Character == TEXT(')') || Character == TEXT(']') || Character == TEXT('}'))
				{
					--Depth;
					if (Depth == 0)
					{
						return Character == TEXT(')') ? Index : INDEX_NONE;
					}
				}
				++Index;
			}
			return INDEX_NONE;
		}

		/** The arguments between a pair of parentheses, split at the commas that belong to that pair. */
		static void SplitArguments(const FString& Text, TArray<FString>& OutArguments)
		{
			OutArguments.Reset();
			if (IsWhitespaceOnly(Text))
			{
				return;
			}

			int32 Depth = 0;
			int32 Start = 0;
			int32 Index = 0;
			while (Index < Text.Len())
			{
				const int32 Skipped = SkipCommentOrString(Text, Index);
				if (Skipped != Index)
				{
					Index = Skipped;
					continue;
				}

				const TCHAR Character = Text[Index];
				if (Character == TEXT('(') || Character == TEXT('[') || Character == TEXT('{'))
				{
					++Depth;
				}
				else if (Character == TEXT(')') || Character == TEXT(']') || Character == TEXT('}'))
				{
					--Depth;
				}
				else if (Character == TEXT(',') && Depth == 0)
				{
					OutArguments.Add(Text.Mid(Start, Index - Start));
					Start = Index + 1;
				}
				++Index;
			}
			OutArguments.Add(Text.Mid(Start));
		}

		/** `<TAB><ret> DreamShaderFn_<Name>(<params>)`. */
		static bool ParseHelperHeader(const FString& Line, FRecoveredCustomFunction& OutFunction, FString& OutSymbol)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			const int32 Open = Trimmed.Find(TEXT("("), ESearchCase::CaseSensitive);
			if (Open == INDEX_NONE || !Trimmed.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
			{
				return false;
			}

			const FString Head = Trimmed.Left(Open).TrimStartAndEnd();
			int32 SymbolStart = Head.Len();
			while (SymbolStart > 0 && IsIdentifierPartChar(Head[SymbolStart - 1]))
			{
				--SymbolStart;
			}
			OutSymbol = Head.Mid(SymbolStart);
			OutFunction.ReturnTypeSpelling = Head.Left(SymbolStart).TrimStartAndEnd();
			if (OutSymbol.IsEmpty() || OutFunction.ReturnTypeSpelling.IsEmpty())
			{
				return false;
			}

			TArray<FString> Params;
			SplitArguments(Trimmed.Mid(Open + 1, Trimmed.Len() - Open - 2), Params);
			for (const FString& ParamText : Params)
			{
				FString Text = ParamText.TrimStartAndEnd();
				FRecoveredCustomParam Param;
				if (Text.StartsWith(TEXT("out "), ESearchCase::CaseSensitive))
				{
					Param.Direction = EParamDirection::Out;
					Text.RightChopInline(4);
					Text.TrimStartInline();
				}

				int32 NameStart = Text.Len();
				while (NameStart > 0 && IsIdentifierPartChar(Text[NameStart - 1]))
				{
					--NameStart;
				}
				Param.Name = Text.Mid(NameStart);
				Param.TypeSpelling = Text.Left(NameStart).TrimStartAndEnd();
				if (Param.Name.IsEmpty() || Param.TypeSpelling.IsEmpty())
				{
					return false;
				}

				// The companion the builder adds after a texture parameter; the source never declared it.
				if (Param.TypeSpelling.Equals(TEXT("SamplerState"), ESearchCase::CaseSensitive)
					&& OutFunction.Params.Num() > 0
					&& IsTextureSpelling(OutFunction.Params.Last().TypeSpelling)
					&& Param.Name.Equals(OutFunction.Params.Last().Name + TEXT("Sampler"), ESearchCase::CaseSensitive))
				{
					continue;
				}
				OutFunction.Params.Add(MoveTemp(Param));
			}

			OutFunction.bHasSignature = true;
			return true;
		}

		/** `<T> <Name> = (<T>)0;` */
		static bool ParseZeroedDeclaration(const FString& Line, FString& OutType, FString& OutName)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			const int32 Assign = Trimmed.Find(TEXT(" = ("), ESearchCase::CaseSensitive);
			if (Assign == INDEX_NONE || !Trimmed.EndsWith(TEXT(")0;"), ESearchCase::CaseSensitive))
			{
				return false;
			}

			const FString Head = Trimmed.Left(Assign).TrimStartAndEnd();
			int32 NameStart = Head.Len();
			while (NameStart > 0 && IsIdentifierPartChar(Head[NameStart - 1]))
			{
				--NameStart;
			}
			OutName = Head.Mid(NameStart);
			OutType = Head.Left(NameStart).TrimStartAndEnd();
			return !OutName.IsEmpty() && !OutType.IsEmpty();
		}

		struct FLineReader
		{
			const TArray<FString>& Lines;
			int32 Index = 0;

			bool AtEnd() const { return Index >= Lines.Num(); }
			const FString& Current() const
			{
				static const FString Empty;
				return Lines.IsValidIndex(Index) ? Lines[Index] : Empty;
			}
			bool ConsumeExact(const FString& Expected)
			{
				if (!AtEnd() && Current().Equals(Expected, ESearchCase::CaseSensitive))
				{
					++Index;
					return true;
				}
				return false;
			}
		};

		static bool ParseBodyBlock(FLineReader& Reader, const FString& TailIndent, const bool bIsRoot, FRecoveredCustomFunction& OutFunction)
		{
			const bool bBareExpression = Reader.ConsumeExact(TailIndent + TEXT("return"));

			if (bIsRoot && !Reader.Current().StartsWith(IR::CustomCodeMarker::BeginPrefix, ESearchCase::CaseSensitive))
			{
				if (!ParseZeroedDeclaration(Reader.Current(), OutFunction.PrimaryOutType, OutFunction.PrimaryOutName))
				{
					return false;
				}
				++Reader.Index;
			}

			if (!Reader.Current().StartsWith(IR::CustomCodeMarker::BeginPrefix, ESearchCase::CaseSensitive))
			{
				return false;
			}
			++Reader.Index;

			int32 SourceLine = 0;
			if (!IR::TryParseCustomCodeBodyMarker(Reader.Current(), OutFunction.Name, SourceLine) || OutFunction.Name.IsEmpty())
			{
				return false;
			}
			++Reader.Index;

			TArray<FString> BodyLines;
			while (!Reader.AtEnd() && !Reader.Current().StartsWith(IR::CustomCodeMarker::EndPrefix, ESearchCase::CaseSensitive))
			{
				BodyLines.Add(Reader.Current());
				++Reader.Index;
			}
			if (Reader.AtEnd())
			{
				return false;
			}
			++Reader.Index;

			// The builder ends the body with a line break when it has none. A last line of nothing but the closing
			// brace's indentation is where the source had none, so nothing is added back after it.
			OutFunction.RawBody = FString::Join(BodyLines, TEXT("\n"));
			if (BodyLines.Num() > 0 && !IsWhitespaceOnly(BodyLines.Last()))
			{
				OutFunction.RawBody += TEXT("\n");
			}

			if (bBareExpression)
			{
				return Reader.ConsumeExact(TailIndent + TEXT(";"));
			}

			const FString ReturnPrefix = TailIndent + TEXT("return ");
			if (!Reader.AtEnd() && Reader.Current().StartsWith(ReturnPrefix, ESearchCase::CaseSensitive) && Reader.Current().EndsWith(TEXT(";"), ESearchCase::CaseSensitive))
			{
				const FString Returned = Reader.Current().Mid(ReturnPrefix.Len(), Reader.Current().Len() - ReturnPrefix.Len() - 1);
				OutFunction.bZeroFallback = Returned.Equals(TEXT("0.0"), ESearchCase::CaseSensitive);
				++Reader.Index;
			}
			return true;
		}

		/**
		 * Sibling calls as the source spelled them: `DreamShaderFn_X(` and `__ds_wrapper_N.DreamShaderFn_X(` become
		 * `X(`, and the ` <Tex>Sampler` argument the builder put after every texture argument goes away.
		 */
		static FString RestoreSiblingCalls(const FString& Text, const TMap<FString, const FRecoveredCustomFunction*>& BySymbol, bool& bInOutOk)
		{
			FString Result;
			Result.Reserve(Text.Len());

			int32 Index = 0;
			while (Index < Text.Len())
			{
				const int32 Skipped = SkipCommentOrString(Text, Index);
				if (Skipped != Index)
				{
					Result += Text.Mid(Index, Skipped - Index);
					Index = Skipped;
					continue;
				}

				if (!IsIdentifierStartChar(Text[Index]) || (Index > 0 && IsIdentifierPartChar(Text[Index - 1])))
				{
					Result.AppendChar(Text[Index]);
					++Index;
					continue;
				}

				int32 IdentifierEnd = Index;
				while (IdentifierEnd < Text.Len() && IsIdentifierPartChar(Text[IdentifierEnd]))
				{
					++IdentifierEnd;
				}
				FString Identifier = Text.Mid(Index, IdentifierEnd - Index);
				const bool bIsMember = Index > 0 && Text[Index - 1] == TEXT('.');

				int32 SymbolEnd = IdentifierEnd;
				if (!bIsMember
					&& Identifier.StartsWith(WrapperVariablePrefix, ESearchCase::CaseSensitive)
					&& Text.IsValidIndex(IdentifierEnd)
					&& Text[IdentifierEnd] == TEXT('.'))
				{
					int32 MemberEnd = IdentifierEnd + 1;
					while (MemberEnd < Text.Len() && IsIdentifierPartChar(Text[MemberEnd]))
					{
						++MemberEnd;
					}
					Identifier = Text.Mid(IdentifierEnd + 1, MemberEnd - IdentifierEnd - 1);
					SymbolEnd = MemberEnd;
				}
				else if (bIsMember)
				{
					Result += Identifier;
					Index = IdentifierEnd;
					continue;
				}

				const FRecoveredCustomFunction* const* Callee = BySymbol.Find(Identifier);
				if (!Callee || !Text.IsValidIndex(SymbolEnd) || Text[SymbolEnd] != TEXT('('))
				{
					Result += Text.Mid(Index, IdentifierEnd - Index);
					Index = IdentifierEnd;
					continue;
				}

				const int32 Close = FindClosingParenthesis(Text, SymbolEnd);
				if (Close == INDEX_NONE)
				{
					bInOutOk = false;
					Result += Text.Mid(Index);
					break;
				}

				TArray<FString> Arguments;
				SplitArguments(Text.Mid(SymbolEnd + 1, Close - SymbolEnd - 1), Arguments);

				TArray<FString> Kept;
				int32 ArgumentIndex = 0;
				for (const FRecoveredCustomParam& Param : (*Callee)->Params)
				{
					if (!Arguments.IsValidIndex(ArgumentIndex))
					{
						bInOutOk = false;
						break;
					}
					Kept.Add(RestoreSiblingCalls(Arguments[ArgumentIndex++], BySymbol, bInOutOk));

					if (IsTextureSpelling(Param.TypeSpelling))
					{
						if (Arguments.IsValidIndex(ArgumentIndex) && Arguments[ArgumentIndex].TrimStartAndEnd().EndsWith(TEXT("Sampler"), ESearchCase::CaseSensitive))
						{
							++ArgumentIndex;
						}
						else
						{
							bInOutOk = false;
						}
					}
				}
				if (ArgumentIndex != Arguments.Num())
				{
					bInOutOk = false;
				}

				Result += (*Callee)->Name;
				Result += TEXT("(");
				Result += FString::Join(Kept, TEXT(","));
				Result += TEXT(")");
				Index = Close + 1;
			}

			return Result;
		}

		/**
		 * The `#include` lines the builder blanked, written back over their blanks: it replaced each with spaces of the
		 * same length (HoistLeadingIncludes), so a leading line of nothing but whitespace that is long enough is where
		 * one stood. Paths are taken from Remaining in order and removed from it.
		 */
		static void RestoreIncludes(FString& InOutBody, TArray<FString>& Remaining)
		{
			if (Remaining.IsEmpty())
			{
				return;
			}

			TArray<FString> Lines;
			InOutBody.ParseIntoArray(Lines, TEXT("\n"), /* bCullEmpty */ false);

			for (int32 LineIndex = 0; LineIndex < Lines.Num() && Remaining.Num() > 0; ++LineIndex)
			{
				FString& Line = Lines[LineIndex];
				const FString Trimmed = Line.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					if (Trimmed.StartsWith(TEXT("//"), ESearchCase::CaseSensitive))
					{
						// A comment above the includes: the builder skips it too.
						continue;
					}
					break;
				}

				int32 TrailingSpaces = 0;
				while (TrailingSpaces < Line.Len() && Line[Line.Len() - 1 - TrailingSpaces] == TEXT(' '))
				{
					++TrailingSpaces;
				}

				int32 Chosen = INDEX_NONE;
				for (int32 Candidate = 0; Candidate < Remaining.Num(); ++Candidate)
				{
					const int32 Needed = FCString::Strlen(IncludeOpen) + Remaining[Candidate].Len() + 1;
					if (Needed == TrailingSpaces)
					{
						Chosen = Candidate;
						break;
					}
					if (Chosen == INDEX_NONE && Needed <= TrailingSpaces)
					{
						Chosen = Candidate;
					}
				}
				if (Chosen == INDEX_NONE)
				{
					continue;
				}

				const FString Spelled = FString(IncludeOpen) + Remaining[Chosen] + TEXT("\"");
				Line = Line.Left(Line.Len() - Spelled.Len()) + Spelled;
				Remaining.RemoveAt(Chosen);
			}

			InOutBody = FString::Join(Lines, TEXT("\n"));
		}

		/** Includes that found no blank to go back into: on lines of their own at the top of the body. */
		static void PrependIncludes(FString& InOutBody, const TArray<FString>& Includes)
		{
			if (Includes.IsEmpty())
			{
				return;
			}

			FString Block;
			for (const FString& Path : Includes)
			{
				Block += FString(IncludeOpen) + Path + TEXT("\"\n");
			}

			const int32 FirstBreak = InOutBody.Find(TEXT("\n"), ESearchCase::CaseSensitive);
			if (FirstBreak != INDEX_NONE && IsWhitespaceOnly(InOutBody.Left(FirstBreak)))
			{
				InOutBody.InsertAt(FirstBreak + 1, Block);
			}
			else
			{
				InOutBody = FString(TEXT("\n")) + Block + InOutBody;
			}
		}
	}

	bool RecoverCustomCode(const FString& Code, const TArray<FString>& IncludeFilePaths, FRecoveredCustomCode& Out)
	{
		using namespace CustomRecover;

		Out = FRecoveredCustomCode();

		const FString Normalized = Code.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\r"), TEXT("\n"));
		if (!Normalized.Contains(IR::CustomCodeMarker::BodyPrefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		TArray<FString> Lines;
		Normalized.ParseIntoArray(Lines, TEXT("\n"), /* bCullEmpty */ false);
		FLineReader Reader{ Lines, 0 };

		TArray<FString> Symbols;
		if (Reader.Current().StartsWith(WrapperStructPrefix, ESearchCase::CaseSensitive))
		{
			++Reader.Index;
			if (!Reader.ConsumeExact(TEXT("{")))
			{
				return false;
			}

			while (!Reader.AtEnd() && !Reader.Current().Equals(TEXT("};"), ESearchCase::CaseSensitive))
			{
				FRecoveredCustomFunction Helper;
				FString Symbol;
				if (!ParseHelperHeader(Reader.Current(), Helper, Symbol))
				{
					return false;
				}
				++Reader.Index;
				if (!Reader.ConsumeExact(TEXT("\t{")))
				{
					return false;
				}

				// A real HLSL `out` parameter arrives uninitialised, so the builder zeroes each one first.
				for (const FRecoveredCustomParam& Param : Helper.Params)
				{
					if (Param.Direction != EParamDirection::Out)
					{
						continue;
					}
					const FString Expected = FString::Printf(TEXT("\t\t%s = (%s)0;"), *Param.Name, *Param.TypeSpelling);
					if (!Reader.ConsumeExact(Expected))
					{
						return false;
					}
				}

				// The header named the function by its symbol; the body marker carries the name the source used.
				if (!ParseBodyBlock(Reader, TEXT("\t\t"), /* bIsRoot */ false, Helper))
				{
					return false;
				}
				if (!Reader.ConsumeExact(TEXT("\t}")) || !Reader.ConsumeExact(FString()))
				{
					return false;
				}

				Symbols.Add(Symbol);
				Out.Helpers.Add(MoveTemp(Helper));
			}

			// `};`, the instance, and the blank line under it.
			if (!Reader.ConsumeExact(TEXT("};")))
			{
				return false;
			}
			if (Reader.AtEnd() || !Reader.Current().Contains(WrapperVariablePrefix, ESearchCase::CaseSensitive))
			{
				return false;
			}
			++Reader.Index;
			if (!Reader.ConsumeExact(FString()))
			{
				return false;
			}
		}

		if (!ParseBodyBlock(Reader, FString(), /* bIsRoot */ true, Out.Root))
		{
			return false;
		}
		// Nothing but the text's final line break may follow.
		while (!Reader.AtEnd())
		{
			if (!IsWhitespaceOnly(Reader.Current()))
			{
				return false;
			}
			++Reader.Index;
		}

		TMap<FString, const FRecoveredCustomFunction*> BySymbol;
		for (int32 Index = 0; Index < Out.Helpers.Num(); ++Index)
		{
			BySymbol.Add(Symbols[Index], &Out.Helpers[Index]);
		}

		bool bOk = true;
		TArray<FString> RemainingIncludes = IncludeFilePaths;
		for (FRecoveredCustomFunction& Helper : Out.Helpers)
		{
			Helper.RawBody = RestoreSiblingCalls(Helper.RawBody, BySymbol, bOk);
			RestoreIncludes(Helper.RawBody, RemainingIncludes);
		}
		Out.Root.RawBody = RestoreSiblingCalls(Out.Root.RawBody, BySymbol, bOk);
		RestoreIncludes(Out.Root.RawBody, RemainingIncludes);
		PrependIncludes(Out.Root.RawBody, RemainingIncludes);

		return bOk;
	}
}
