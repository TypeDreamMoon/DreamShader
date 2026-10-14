// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The text scanner of IRCustomHlsl.cpp, for the one other place that reads a `@custom` body as HLSL: `dsc migrate`
// (Migrate/LangMigrate.cpp), which rewrites the calls in a 1.x body the way the Custom code builder reads them.
// Nothing here knows about a bound module.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::IR::CustomHlslPrivate
{
	bool IsIdentifierStart(TCHAR Char);
	bool IsIdentifierPart(TCHAR Char);
	/** Past any run of whitespace, newlines included. */
	int32 SkipInlineWhitespace(const FString& Source, int32 Index);
	/** Past whitespace AND `//` / block comments. */
	int32 SkipWhitespaceAndComments(const FString& Source, int32 Index);
	/** Reads `[A-Za-z_][A-Za-z0-9_]*` at InOutIndex. */
	bool TryReadIdentifier(const FString& Source, int32& InOutIndex, FString& OutIdentifier);

	enum class EPieceKind : uint8
	{
		Code,
		Comment,
		Literal,
	};

	struct FPiece
	{
		EPieceKind Kind = EPieceKind::Code;
		int32 Start = 0;
		int32 End = 0;
	};

	/** The text as code, comment and literal runs that tile it completely. */
	void SplitHlslPieces(const FString& Source, TArray<FPiece>& OutPieces);
	/** Half-open ranges of the top-level arguments of what stands between a call's parentheses; none for a blank block. */
	void SplitTopLevelArguments(const FString& Text, TArray<TPair<int32, int32>>& OutRanges);
	/** The index of the `)` matching the `(` at OpenIndex. */
	bool TryFindMatchingParenthesis(const FString& Source, int32 OpenIndex, int32& OutCloseIndex);

	/** What a call does with what it returns, as far as the text in front of it says. */
	enum class ECustomCallUse : uint8
	{
		/** It starts a statement: after `;`, `{`, `}`, `else`, `do`, or at the start of the body. */
		Statement,
		/** Its value is read: after `=`, `(`, `,`, an operator, or `return`. */
		Value,
		/** After `)` (a condition or a cast), `:` (a label or a branch) or any other word: not said. */
		Unknown,
	};

	/** Pieces are Text's (SplitHlslPieces); CallStart is where the callee's name starts. */
	ECustomCallUse ClassifyCustomCallUse(const FString& Text, const TArray<FPiece>& Pieces, int32 CallStart);
}
