// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShaderLang HLSL function codegen: identifier tokenizing, call rewriting, Custom-node code
// assembly (PrepareCustomNodeCode), and generated .ush include emission (WriteGeneratedInclude).
// Pure string/FTextShaderDefinition processing, no UMaterialExpression graph state. Extracted from
// DreamShaderMaterialGeneratorSupport.cpp; header-declared entry points stay in the private header.

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private
{
	FString EnsureTopLevelReturn(const FString& InHLSL)
	{
		const FString Sanitized = InHLSL.Replace(TEXT("\r\n"), TEXT("\n"));

		// Token-aware scan: detect a real top-level `return` statement while ignoring
		// occurrences inside // line comments, /* */ block comments, string/char literals,
		// or identifiers (e.g. returnValue). Only suppress injection for a genuine
		// brace-depth-0 `return` keyword bounded by non-identifier characters.
		auto IsReturnIdentifierPart = [](const TCHAR Character)
		{
			return FChar::IsAlnum(Character) || Character == TCHAR('_');
		};

		bool bHasTopLevelReturn = false;
		int32 BraceDepth = 0;
		bool bInString = false;
		bool bInChar = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Sanitized.Len() && !bHasTopLevelReturn;)
		{
			const TCHAR Char = Sanitized[Index];
			const TCHAR Next = Sanitized.IsValidIndex(Index + 1) ? Sanitized[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString || bInChar)
			{
				if (Char == TCHAR('\\') && Sanitized.IsValidIndex(Index + 1))
				{
					Index += 2;
					continue;
				}
				if (bInString && Char == TCHAR('"'))
				{
					bInString = false;
				}
				else if (bInChar && Char == TCHAR('\''))
				{
					bInChar = false;
				}
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Index += 2;
				continue;
			}
			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Index += 2;
				continue;
			}
			if (Char == TCHAR('"'))
			{
				bInString = true;
				++Index;
				continue;
			}
			if (Char == TCHAR('\''))
			{
				bInChar = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('{'))
			{
				++BraceDepth;
				++Index;
				continue;
			}
			if (Char == TCHAR('}'))
			{
				if (BraceDepth > 0)
				{
					--BraceDepth;
				}
				++Index;
				continue;
			}

			if (BraceDepth == 0 && Char == TCHAR('r')
				&& Sanitized.Mid(Index, 6) == TEXT("return"))
			{
				const bool bLeftBoundary = (Index == 0) || !IsReturnIdentifierPart(Sanitized[Index - 1]);
				const TCHAR After = Sanitized.IsValidIndex(Index + 6) ? Sanitized[Index + 6] : TCHAR('\0');
				const bool bRightBoundary = !IsReturnIdentifierPart(After);
				if (bLeftBoundary && bRightBoundary)
				{
					bHasTopLevelReturn = true;
					break;
				}
			}

			++Index;
		}

		if (bHasTopLevelReturn)
		{
			return Sanitized;
		}

		return Sanitized + TEXT("\nreturn 0.0;");
	}

	bool IsTextureFunctionParameterType(const FString& InTypeName)
	{
		return InTypeName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("Texture3D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase);
	}

	static FString GetGeneratedHLSLTypeName(const FString& InTypeName)
	{
		return InTypeName.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase)
			? TEXT("Texture3D")
			: InTypeName;
	}

	FString BuildGeneratedFunctionSymbolName(const FTextShaderFunctionDefinition& Function)
	{
		// Prefix every generated HLSL function so a DreamShaderLang function name can never collide
		// with one of Unreal's global shader intrinsics. Without this, a user function named e.g.
		// "Luminance" emits `float Luminance(float3)` into the material's Custom-node include, which
		// redefines /Engine/Private/Common.ush's built-in Luminance and fails shader compilation with
		// "redefinition of 'Luminance'". This name is the single source of truth for the function's
		// definition, its inter-function call rewriting, and the Custom-node call site, so prefixing
		// here keeps all three consistent.
		return TEXT("DreamShaderFn_") + UE::DreamShader::SanitizeIdentifier(Function.Name);
	}

	static uint32 GetSourcePathHash(const FString& SourceFilePath)
	{
		// Hash the project-relative path, not the absolute path, so the generated include's file name
		// and header guard are identical across machines and checkouts. An absolute path made both
		// machine-specific, fragmenting shared DDC / build-cache hits and producing per-machine
		// generated-shader file names for the same source. Sources outside the project (rare) keep
		// their absolute path.
		FString Path = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		FPaths::MakePathRelativeTo(Path, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return FCrc::StrCrc32(*Path);
	}

	static FString BuildGeneratedIncludeGuardMacro(const FString& SourceFilePath)
	{
		return FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("DREAMSHADER_GENERATED_%s_%08X"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)).ToUpper(),
			GetSourcePathHash(SourceFilePath));
	}

	static bool IsFunctionIdentifierStart(const TCHAR Char)
	{
		return FChar::IsAlpha(Char) || Char == TCHAR('_');
	}

	static bool IsFunctionIdentifierPart(const TCHAR Char)
	{
		return FChar::IsAlnum(Char) || Char == TCHAR('_');
	}

	static int32 SkipInlineWhitespace(const FString& Source, int32 Index)
	{
		while (Source.IsValidIndex(Index) && FChar::IsWhitespace(Source[Index]))
		{
			++Index;
		}
		return Index;
	}

	static bool TryReadQualifiedIdentifierToken(const FString& Source, int32& InOutIndex, FString& OutIdentifier)
	{
		if (!Source.IsValidIndex(InOutIndex) || !IsFunctionIdentifierStart(Source[InOutIndex]))
		{
			return false;
		}

		const int32 IdentifierStart = InOutIndex++;
		while (Source.IsValidIndex(InOutIndex) && IsFunctionIdentifierPart(Source[InOutIndex]))
		{
			++InOutIndex;
		}

		OutIdentifier = Source.Mid(IdentifierStart, InOutIndex - IdentifierStart);
		while (Source.IsValidIndex(InOutIndex + 2)
			&& Source[InOutIndex] == TCHAR(':')
			&& Source[InOutIndex + 1] == TCHAR(':')
			&& IsFunctionIdentifierStart(Source[InOutIndex + 2]))
		{
			const int32 NextIdentifierStart = InOutIndex + 2;
			int32 NextIdentifierEnd = NextIdentifierStart + 1;
			while (Source.IsValidIndex(NextIdentifierEnd) && IsFunctionIdentifierPart(Source[NextIdentifierEnd]))
			{
				++NextIdentifierEnd;
			}

			OutIdentifier += TEXT("::") + Source.Mid(NextIdentifierStart, NextIdentifierEnd - NextIdentifierStart);
			InOutIndex = NextIdentifierEnd;
		}

		return true;
	}

	static bool TryFindMatchingCallParenthesis(const FString& Source, const int32 OpenParenthesisIndex, int32& OutCloseParenthesisIndex)
	{
		if (!Source.IsValidIndex(OpenParenthesisIndex) || Source[OpenParenthesisIndex] != TCHAR('('))
		{
			return false;
		}

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;
		int32 ParenthesisDepth = 0;
		for (int32 Index = OpenParenthesisIndex; Index < Source.Len(); ++Index)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					++Index;
				}
				else if (Char == TCHAR('"'))
				{
					bInString = false;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
			}
			else if (Char == TCHAR(')'))
			{
				--ParenthesisDepth;
				if (ParenthesisDepth == 0)
				{
					OutCloseParenthesisIndex = Index;
					return true;
				}
			}
		}

		return false;
	}

	static TArray<FString> SplitTopLevelCallArguments(const FString& ArgumentBlock)
	{
		TArray<FString> Arguments;
		int32 SegmentStart = 0;
		int32 ParenthesisDepth = 0;
		int32 BraceDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < ArgumentBlock.Len(); ++Index)
		{
			const TCHAR Char = ArgumentBlock[Index];
			const TCHAR Next = ArgumentBlock.IsValidIndex(Index + 1) ? ArgumentBlock[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				if (Char == TCHAR('\\') && ArgumentBlock.IsValidIndex(Index + 1))
				{
					++Index;
				}
				else if (Char == TCHAR('"'))
				{
					bInString = false;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				++Index;
				continue;
			}

			switch (Char)
			{
			case TCHAR('('): ++ParenthesisDepth; break;
			case TCHAR(')'): ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1); break;
			case TCHAR('{'): ++BraceDepth; break;
			case TCHAR('}'): BraceDepth = FMath::Max(0, BraceDepth - 1); break;
			case TCHAR('['): ++BracketDepth; break;
			case TCHAR(']'): BracketDepth = FMath::Max(0, BracketDepth - 1); break;
			case TCHAR(','):
				if (ParenthesisDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
				{
					Arguments.Add(ArgumentBlock.Mid(SegmentStart, Index - SegmentStart).TrimStartAndEnd());
					SegmentStart = Index + 1;
				}
				break;
			default:
				break;
			}
		}

		const FString Tail = ArgumentBlock.Mid(SegmentStart).TrimStartAndEnd();
		if (!Tail.IsEmpty() || !ArgumentBlock.TrimStartAndEnd().IsEmpty())
		{
			Arguments.Add(Tail);
		}
		return Arguments;
	}

	static FString BuildTextureSamplerArgumentName(const FString& TextureArgument)
	{
		return TextureArgument.TrimStartAndEnd() + TEXT("Sampler");
	}

	static bool TryRewriteExplicitOutFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const FString& GeneratedFunctionName,
		const FString& ArgumentBlock,
		FString& OutCall)
	{
		const TArray<FString> Arguments = SplitTopLevelCallArguments(ArgumentBlock);
		const int32 ExpectedArgumentCount = Function.Inputs.Num() + Function.Results.Num();
		if (Arguments.Num() != ExpectedArgumentCount || Function.Results.IsEmpty())
		{
			return false;
		}

		TArray<FString> Parameters;
		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			Parameters.Add(Arguments[InputIndex]);
			if (IsTextureFunctionParameterType(Function.Inputs[InputIndex].Type))
			{
				Parameters.Add(BuildTextureSamplerArgumentName(Arguments[InputIndex]));
			}
		}

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			Parameters.Add(Arguments[Function.Inputs.Num() + ResultIndex]);
		}

		const FString PrimaryResultTarget = Arguments[Function.Inputs.Num()].TrimStartAndEnd();
		if (PrimaryResultTarget.IsEmpty())
		{
			return false;
		}

		OutCall = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("%s = %s(%s)"),
			*PrimaryResultTarget,
			*GeneratedFunctionName,
			*FString::Join(Parameters, TEXT(", ")));
		return true;
	}

	static bool TryRewriteValueFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const FString& GeneratedFunctionName,
		const FString& ArgumentBlock,
		FString& OutCall)
	{
		const TArray<FString> Arguments = SplitTopLevelCallArguments(ArgumentBlock);
		if (Function.Results.Num() != 1 || Arguments.Num() != Function.Inputs.Num())
		{
			return false;
		}

		TArray<FString> Parameters;
		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			Parameters.Add(Arguments[InputIndex]);
			if (IsTextureFunctionParameterType(Function.Inputs[InputIndex].Type))
			{
				Parameters.Add(BuildTextureSamplerArgumentName(Arguments[InputIndex]));
			}
		}

		OutCall = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("%s(%s)"),
			*GeneratedFunctionName,
			*FString::Join(Parameters, TEXT(", ")));
		return true;
	}

	static void AddFunctionLookupEntries(
		const FTextShaderFunctionDefinition& Function,
		TMap<FString, const FTextShaderFunctionDefinition*>& OutFunctionsBySpelling,
		TMap<FString, FString>& OutGeneratedNamesBySpelling)
	{
		const FString GeneratedName = BuildGeneratedFunctionSymbolName(Function);

		OutFunctionsBySpelling.Add(Function.Name, &Function);
		OutGeneratedNamesBySpelling.Add(Function.Name, GeneratedName);

		if (!GeneratedName.Equals(Function.Name, ESearchCase::CaseSensitive))
		{
			OutFunctionsBySpelling.Add(GeneratedName, &Function);
			OutGeneratedNamesBySpelling.Add(GeneratedName, GeneratedName);
		}
	}

	static void CollectDreamShaderFunctionCalls(
		const FString& Source,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		TArray<const FTextShaderFunctionDefinition*>& OutFunctions)
	{
		TSet<const FTextShaderFunctionDefinition*> SeenFunctions;
		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					Index += 2;
				}
				else
				{
					if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					++Index;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Index += 2;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Index += 2;
				continue;
			}

			if (IsFunctionIdentifierStart(Char))
			{
				int32 IdentifierEnd = Index;
				FString Identifier;
				if (TryReadQualifiedIdentifierToken(Source, IdentifierEnd, Identifier))
				{
					const int32 PostIdentifier = SkipInlineWhitespace(Source, IdentifierEnd);
					if (Source.IsValidIndex(PostIdentifier) && Source[PostIdentifier] == TCHAR('('))
					{
						if (const FTextShaderFunctionDefinition* const* Function = FunctionsBySpelling.Find(Identifier))
						{
							if (!SeenFunctions.Contains(*Function))
							{
								SeenFunctions.Add(*Function);
								OutFunctions.Add(*Function);
							}
						}
					}

					Index = IdentifierEnd;
					continue;
				}
			}

			++Index;
		}
	}

	static FString RewriteDreamShaderFunctionReferences(
		const FString& Source,
		const TMap<FString, FString>& ReplacementBySpelling)
	{
		FString Result;
		Result.Reserve(Source.Len() + 128);

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					Result.AppendChar(Next);
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					Result.AppendChar(Source[Index + 1]);
					Index += 2;
				}
				else
				{
					if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					++Index;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				Result.AppendChar(Char);
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (IsFunctionIdentifierStart(Char))
			{
				int32 IdentifierEnd = Index;
				FString Identifier;
				if (TryReadQualifiedIdentifierToken(Source, IdentifierEnd, Identifier))
				{
					const int32 PostIdentifier = SkipInlineWhitespace(Source, IdentifierEnd);
					if (Source.IsValidIndex(PostIdentifier) && Source[PostIdentifier] == TCHAR('('))
					{
						if (const FString* Replacement = ReplacementBySpelling.Find(Identifier))
						{
							Result += *Replacement;
							Index = IdentifierEnd;
							continue;
						}
					}

					Result += Source.Mid(Index, IdentifierEnd - Index);
					Index = IdentifierEnd;
					continue;
				}
			}

			Result.AppendChar(Char);
			++Index;
		}

		return Result;
	}

	static FString RewriteDreamShaderFunctionBodyCalls(
		const FString& Source,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		const TMap<FString, FString>& ReplacementBySpelling)
	{
		FString Result;
		Result.Reserve(Source.Len() + 128);

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
				}
				++Index;
				continue;
			}

			if (bInBlockComment)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					Result.AppendChar(Next);
					bInBlockComment = false;
					Index += 2;
				}
				else
				{
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				Result.AppendChar(Char);
				if (Char == TCHAR('\\') && Source.IsValidIndex(Index + 1))
				{
					Result.AppendChar(Source[Index + 1]);
					Index += 2;
				}
				else
				{
					if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					++Index;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				Result.AppendChar(Char);
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				Result.AppendChar(Char);
				Result.AppendChar(Next);
				Index += 2;
				continue;
			}

			if (IsFunctionIdentifierStart(Char))
			{
				int32 IdentifierEnd = Index;
				FString Identifier;
				if (TryReadQualifiedIdentifierToken(Source, IdentifierEnd, Identifier))
				{
					const int32 PostIdentifier = SkipInlineWhitespace(Source, IdentifierEnd);
					const FString* Replacement = ReplacementBySpelling.Find(Identifier);
					if (Source.IsValidIndex(PostIdentifier) && Source[PostIdentifier] == TCHAR('(') && Replacement)
					{
						int32 CloseParenthesisIndex = INDEX_NONE;
						if (const FTextShaderFunctionDefinition* const* Function = FunctionsBySpelling.Find(Identifier);
							Function && TryFindMatchingCallParenthesis(Source, PostIdentifier, CloseParenthesisIndex))
						{
							const FString ArgumentBlock = Source.Mid(PostIdentifier + 1, CloseParenthesisIndex - PostIdentifier - 1);
							FString RewrittenCall;
							if (TryRewriteExplicitOutFunctionCall(**Function, *Replacement, ArgumentBlock, RewrittenCall))
							{
								Result += RewrittenCall;
								Index = CloseParenthesisIndex + 1;
								continue;
							}
							if (TryRewriteValueFunctionCall(**Function, *Replacement, ArgumentBlock, RewrittenCall))
							{
								Result += RewrittenCall;
								Index = CloseParenthesisIndex + 1;
								continue;
							}
						}

						Result += *Replacement;
						Index = IdentifierEnd;
						continue;
					}

					Result += Source.Mid(Index, IdentifierEnd - Index);
					Index = IdentifierEnd;
					continue;
				}
			}

			Result.AppendChar(Char);
			++Index;
		}

		return Result;
	}

	static void AppendIndentedCode(FString& OutSource, const FString& Source, const FString& Indent)
	{
		int32 LineStart = 0;
		while (LineStart < Source.Len())
		{
			const int32 NewLineIndex = Source.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
			const bool bHasNewLine = NewLineIndex != INDEX_NONE;
			const int32 LineEnd = bHasNewLine ? NewLineIndex : Source.Len();

			OutSource += Indent;
			OutSource += Source.Mid(LineStart, LineEnd - LineStart);
			OutSource += TEXT("\n");

			if (!bHasNewLine)
			{
				break;
			}

			LineStart = NewLineIndex + 1;
		}
	}

	static FString BuildGeneratedFunctionParameterList(const FTextShaderFunctionDefinition& Function)
	{
		TArray<FString> Parameters;
		for (const FTextShaderFunctionParameter& Input : Function.Inputs)
		{
			Parameters.Add(FString::Printf(TEXT("%s %s"), *GetGeneratedHLSLTypeName(Input.Type), *Input.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			if (IsTextureFunctionParameterType(Input.Type))
			{
				Parameters.Add(FString::Printf(TEXT("SamplerState %sSampler"), *Input.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}
		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FTextShaderFunctionParameter& Output = Function.Results[ResultIndex];
			Parameters.Add(FString::Printf(TEXT("out %s %s"), *GetGeneratedHLSLTypeName(Output.Type), *Output.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return FString::Join(Parameters, TEXT(", "));
	}

	static void AppendGeneratedFunctionDefinition(
		const FTextShaderFunctionDefinition& Function,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		const TMap<FString, FString>& ReplacementBySpelling,
		const FString& Indent,
		FString& OutSource)
	{
		const FString ReturnType = Function.Results.IsEmpty() ? TEXT("void") : GetGeneratedHLSLTypeName(Function.Results[0].Type);
		const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(Function);

		OutSource += FString::Printf(TEXT("%s%s %s(%s)\n%s{\n"), *Indent, *ReturnType, *GeneratedFunctionName, *BuildGeneratedFunctionParameterList(Function), *Indent); /* I18N-EXEMPT: deferred codegen or compatibility path */

		if (!Function.Results.IsEmpty())
		{
			const FString PrimaryResultType = GetGeneratedHLSLTypeName(Function.Results[0].Type);
			OutSource += FString::Printf(TEXT("%s\t%s %s = (%s)0;\n"), *Indent, *PrimaryResultType, *Function.Results[0].Name, *PrimaryResultType); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FTextShaderFunctionParameter& Output = Function.Results[ResultIndex];
			const FString OutputType = GetGeneratedHLSLTypeName(Output.Type);
			OutSource += FString::Printf(TEXT("%s\t%s = (%s)0;\n"), *Indent, *Output.Name, *OutputType); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		const FString RewrittenFunctionHLSL = RewriteDreamShaderFunctionBodyCalls(Function.HLSL, FunctionsBySpelling, ReplacementBySpelling);
		AppendIndentedCode(OutSource, RewrittenFunctionHLSL, Indent + TEXT("\t"));

		if (!Function.Results.IsEmpty())
		{
			OutSource += FString::Printf(TEXT("%s\treturn %s;\n"), *Indent, *Function.Results[0].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		OutSource += FString::Printf(TEXT("%s}\n"), *Indent); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	static FString BuildSelfContainedWrapperTypeName(const FString& WrapperNameHint)
	{
		const FString SanitizedHint = UE::DreamShader::SanitizeIdentifier(WrapperNameHint.IsEmpty() ? TEXT("Generated") : WrapperNameHint);
		return FString::Printf(TEXT("generated_wrapper_%s_%08X"), *SanitizedHint, FCrc::StrCrc32(*WrapperNameHint)); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	static FString BuildSelfContainedWrapperVariableName(const FString& WrapperNameHint)
	{
		return FString::Printf(TEXT("__ds_wrapper_%08X"), FCrc::StrCrc32(*WrapperNameHint)); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	static bool CollectEmbeddedFunctionClosure(
		const FTextShaderDefinition& Definition,
		const TArray<const FTextShaderFunctionDefinition*>& RootFunctions,
		const TMap<FString, const FTextShaderFunctionDefinition*>& FunctionsBySpelling,
		TArray<const FTextShaderFunctionDefinition*>& OutOrderedFunctions,
		FDreamShaderError& OutError)
	{
		TMap<const FTextShaderFunctionDefinition*, TArray<const FTextShaderFunctionDefinition*>> DependenciesByFunction;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			TArray<const FTextShaderFunctionDefinition*> Dependencies;
			CollectDreamShaderFunctionCalls(Function.HLSL, FunctionsBySpelling, Dependencies);
			DependenciesByFunction.Add(&Function, MoveTemp(Dependencies));
		}

		enum class EVisitState : uint8
		{
			Unvisited,
			Visiting,
			Visited
		};

		TMap<const FTextShaderFunctionDefinition*, EVisitState> VisitStates;
		TArray<const FTextShaderFunctionDefinition*> VisitStack;
		TFunction<bool(const FTextShaderFunctionDefinition*)> VisitFunction;
		VisitFunction = [&](const FTextShaderFunctionDefinition* Function) -> bool
		{
			const EVisitState VisitState = VisitStates.FindRef(Function);
			if (VisitState == EVisitState::Visited)
			{
				return true;
			}

			if (VisitState == EVisitState::Visiting)
			{
				int32 CycleStartIndex = VisitStack.IndexOfByKey(Function);
				if (CycleStartIndex == INDEX_NONE)
				{
					CycleStartIndex = 0;
				}

				TArray<FString> CycleNames;
				for (int32 Index = CycleStartIndex; Index < VisitStack.Num(); ++Index)
				{
					CycleNames.Add(VisitStack[Index]->Name);
				}
				CycleNames.Add(Function->Name);

				return FailWith(OutError, TEXT("DSH6086"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("SelfContained Function cycle detected: %s. HLSL Custom nodes cannot compile recursive DreamShader functions."), *FString::Join(CycleNames, TEXT(" -> "))));
			}

			VisitStates.Add(Function, EVisitState::Visiting);
			VisitStack.Add(Function);

			const TArray<const FTextShaderFunctionDefinition*>* Dependencies = DependenciesByFunction.Find(Function);
			if (Dependencies)
			{
				for (const FTextShaderFunctionDefinition* Dependency : *Dependencies)
				{
					if (!VisitFunction(Dependency))
					{
						return false;
					}
				}
			}

			VisitStack.Pop();
			VisitStates.Add(Function, EVisitState::Visited);
			OutOrderedFunctions.Add(Function);
			return true;
		};

		for (const FTextShaderFunctionDefinition* RootFunction : RootFunctions)
		{
			if (!RootFunction || !VisitFunction(RootFunction))
			{
				return false;
			}
		}

		return true;
	}

	bool PrepareCustomNodeCode(
		const FTextShaderDefinition& Definition,
		const FString& SourceCode,
		const TArray<FString>& RequestedEmbeddedFunctionNames,
		const FString& WrapperNameHint,
		FString& OutCode,
		bool& bOutUsesGeneratedInclude,
		TArray<FString>& OutEmbeddedIncludePaths,
		FDreamShaderError& OutError)
	{
		OutCode = SourceCode;
		bOutUsesGeneratedInclude = false;
		OutEmbeddedIncludePaths.Reset();
		OutError.Reset();

		if (Definition.Functions.IsEmpty())
		{
			return true;
		}

		TMap<FString, const FTextShaderFunctionDefinition*> FunctionsBySpelling;
		TMap<FString, FString> GeneratedNamesBySpelling;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			AddFunctionLookupEntries(Function, FunctionsBySpelling, GeneratedNamesBySpelling);
		}

		TArray<const FTextShaderFunctionDefinition*> DirectCalls;
		CollectDreamShaderFunctionCalls(SourceCode, FunctionsBySpelling, DirectCalls);

		TArray<const FTextShaderFunctionDefinition*> RootFunctions;
		TSet<const FTextShaderFunctionDefinition*> SeenRoots;
		if (!RequestedEmbeddedFunctionNames.IsEmpty())
		{
			for (const FString& RequestedName : RequestedEmbeddedFunctionNames)
			{
				const FTextShaderFunctionDefinition* const* Function = FunctionsBySpelling.Find(RequestedName);
				if (!Function)
				{
					return FailWith(OutError, TEXT("DSH6087"), FString::Printf(TEXT("Unknown SelfContained DreamShader Function '%s'."), *RequestedName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				if (!SeenRoots.Contains(*Function))
				{
					SeenRoots.Add(*Function);
					RootFunctions.Add(*Function);
				}
			}
		}
		else
		{
			for (const FTextShaderFunctionDefinition* Function : DirectCalls)
			{
				if (Function && Function->bSelfContained && !SeenRoots.Contains(Function))
				{
					SeenRoots.Add(Function);
					RootFunctions.Add(Function);
				}
			}
		}

		if (RootFunctions.IsEmpty())
		{
			OutCode = RewriteDreamShaderFunctionBodyCalls(SourceCode, FunctionsBySpelling, GeneratedNamesBySpelling);
			bOutUsesGeneratedInclude = !DirectCalls.IsEmpty();
			return true;
		}

		TArray<const FTextShaderFunctionDefinition*> EmbeddedFunctions;
		if (!CollectEmbeddedFunctionClosure(Definition, RootFunctions, FunctionsBySpelling, EmbeddedFunctions, OutError))
		{
			return false;
		}

		TSet<const FTextShaderFunctionDefinition*> EmbeddedFunctionSet;
		TMap<FString, FString> EmbeddedReferenceReplacements;
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			EmbeddedFunctionSet.Add(Function);
		}

		const FString WrapperTypeName = BuildSelfContainedWrapperTypeName(WrapperNameHint);
		const FString WrapperVariableName = BuildSelfContainedWrapperVariableName(WrapperNameHint);
		TMap<FString, FString> CustomNodeReferenceReplacements = GeneratedNamesBySpelling;
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(*Function);
			EmbeddedReferenceReplacements.Add(Function->Name, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
			CustomNodeReferenceReplacements.Add(Function->Name, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
			if (!GeneratedFunctionName.Equals(Function->Name, ESearchCase::CaseSensitive))
			{
				EmbeddedReferenceReplacements.Add(GeneratedFunctionName, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
				CustomNodeReferenceReplacements.Add(GeneratedFunctionName, WrapperVariableName + TEXT(".") + GeneratedFunctionName);
			}
		}

		// The embedded bodies do not see the generated include, so their hoisted includes ride on the node.
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			for (const FString& IncludePath : Function->IncludePaths)
			{
				OutEmbeddedIncludePaths.AddUnique(IncludePath);
			}
		}

		FString WrapperSource;
		WrapperSource += FString::Printf(TEXT("struct %s\n{\n"), *WrapperTypeName); /* I18N-EXEMPT: deferred codegen or compatibility path */
		for (const FTextShaderFunctionDefinition* Function : EmbeddedFunctions)
		{
			AppendGeneratedFunctionDefinition(*Function, FunctionsBySpelling, GeneratedNamesBySpelling, TEXT("\t"), WrapperSource);
			WrapperSource += TEXT("\n");
		}
		WrapperSource += FString::Printf(TEXT("};\n%s %s;\n"), *WrapperTypeName, *WrapperVariableName); /* I18N-EXEMPT: deferred codegen or compatibility path */

		OutCode = WrapperSource + TEXT("\n") + RewriteDreamShaderFunctionBodyCalls(SourceCode, FunctionsBySpelling, CustomNodeReferenceReplacements);

		for (const FTextShaderFunctionDefinition* DirectCall : DirectCalls)
		{
			if (!EmbeddedFunctionSet.Contains(DirectCall))
			{
				bOutUsesGeneratedInclude = true;
				break;
			}
		}

		return true;
	}

	static bool BuildFunctionIncludeSource(
		const FString& SourceFilePath,
		const FTextShaderDefinition& Definition,
		FString& OutSource,
		FDreamShaderError& OutError)
	{
		OutSource.Reset();
		OutSource += TEXT("// Auto-generated by DreamShader.\n");
		OutSource += TEXT("// Changes will be overwritten the next time the source file is saved.\n\n");

		const FString IncludeGuard = BuildGeneratedIncludeGuardMacro(SourceFilePath);
		OutSource += FString::Printf(TEXT("#ifndef %s\n#define %s\n\n"), *IncludeGuard, *IncludeGuard); /* I18N-EXEMPT: deferred codegen or compatibility path */

		// `#include`s written at the top of Function bodies land here, at file scope, in first-seen order.
		{
			TArray<FString> HoistedIncludes;
			for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
			{
				for (const FString& IncludePath : Function.IncludePaths)
				{
					HoistedIncludes.AddUnique(IncludePath);
				}
			}
			for (const FString& IncludePath : HoistedIncludes)
			{
				OutSource += FString::Printf(TEXT("#include \"%s\"\n"), *IncludePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			if (!HoistedIncludes.IsEmpty())
			{
				OutSource += TEXT("\n");
			}
		}

		TSet<FString> SeenFunctionNames;
		TSet<FString> SeenGeneratedFunctionNames;
		TMap<FString, const FTextShaderFunctionDefinition*> FunctionsBySpelling;
		TMap<FString, FString> GeneratedNamesBySpelling;
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			AddFunctionLookupEntries(Function, FunctionsBySpelling, GeneratedNamesBySpelling);
		}

		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			const FString NormalizedFunctionName = UE::DreamShader::NormalizeSettingKey(Function.Name);
			if (SeenFunctionNames.Contains(NormalizedFunctionName))
			{
				return FailWith(OutError, TEXT("DSH6088"), FString::Printf(TEXT("DreamShader Function '%s' is declared more than once."), *Function.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			SeenFunctionNames.Add(NormalizedFunctionName);

			const FString GeneratedFunctionName = BuildGeneratedFunctionSymbolName(Function);
			const FString NormalizedGeneratedFunctionName = UE::DreamShader::NormalizeSettingKey(GeneratedFunctionName);
			if (SeenGeneratedFunctionNames.Contains(NormalizedGeneratedFunctionName))
			{
				return FailWith(OutError, TEXT("DSH6089"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("DreamShader Function '%s' collides with another generated helper symbol '%s'. Rename the Function or Namespace."), *Function.Name, *GeneratedFunctionName));
			}
			SeenGeneratedFunctionNames.Add(NormalizedGeneratedFunctionName);

			AppendGeneratedFunctionDefinition(Function, FunctionsBySpelling, GeneratedNamesBySpelling, FString(), OutSource);
			OutSource += TEXT("\n");
		}

		OutSource += FString::Printf(TEXT("#endif // %s\n"), *IncludeGuard); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return true;
	}

	FString BuildGeneratedIncludeVirtualPath(const FString& SourceFilePath)
	{
		const FString BaseName = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("%s_%08x.ush"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)),
			GetSourcePathHash(SourceFilePath));
		return FString::Printf(TEXT("%s/%s"), *UE::DreamShader::GetGeneratedShaderVirtualDirectory(), *BaseName); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	static FString BuildGeneratedIncludeRealPath(const FString& SourceFilePath)
	{
		const FString BaseName = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("%s_%08x.ush"),
			*UE::DreamShader::SanitizeIdentifier(FPaths::GetBaseFilename(SourceFilePath)),
			GetSourcePathHash(SourceFilePath));
		return FPaths::Combine(UE::DreamShader::GetGeneratedShaderDirectory(), BaseName);
	}

	bool WriteGeneratedInclude(const FString& SourceFilePath, const FTextShaderDefinition& Definition, FDreamShaderError& OutError)
	{
		const FString IncludePath = BuildGeneratedIncludeRealPath(SourceFilePath);
		FString IncludeSource;
		if (!BuildFunctionIncludeSource(SourceFilePath, Definition, IncludeSource, OutError))
		{
			return false;
		}

		if (!FFileHelper::SaveStringToFile(IncludeSource, *IncludePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return FailWith(OutError, TEXT("DSH6090"), FString::Printf(TEXT("Failed to write generated helper include '%s'."), *IncludePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}
		return true;
	}
}
