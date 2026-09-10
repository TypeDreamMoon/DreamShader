// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Source text and positions for the DreamShaderLang front end. Everything the lexer, the parsers
// and the diagnostics say about "where" is an FLangSpan: a byte range into one source text, with
// the 1-based line and column of its start already resolved so no consumer has to count newlines.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Lang
{
	/**
	 * A half-open range [Offset, Offset + Length) of TCHARs into one FLangSourceText, plus the
	 * 1-based physical line and column of Offset.
	 *
	 * Line and Column are cached, not derived: a diagnostic is built long after the text that
	 * produced it is out of scope, and the bridge's JSON wants line/column, not offsets. Offset and
	 * Length stay authoritative -- they are what the printer, the raw-body capture and a future
	 * language service slice the text with.
	 */
	struct FLangSpan
	{
		int32 Offset = 0;
		int32 Length = 0;
		int32 Line = 1;
		int32 Column = 1;

		int32 End() const { return Offset + Length; }
		bool IsEmpty() const { return Length <= 0; }

		/** The smallest span covering both. Line/Column follow whichever starts first. */
		static FLangSpan Join(const FLangSpan& A, const FLangSpan& B)
		{
			const FLangSpan& First = (B.Offset < A.Offset) ? B : A;
			FLangSpan Result;
			Result.Offset = First.Offset;
			Result.Length = FMath::Max(A.End(), B.End()) - Result.Offset;
			Result.Line = First.Line;
			Result.Column = First.Column;
			return Result;
		}
	};

	/** Which kind of DreamShaderLang file a text is, decided from its extension. */
	enum class ELangFileKind : uint8
	{
		/** 2.0 compilation unit: at most one material entry, any number of exported functions. */
		Dss,
		/** Shared header, both syntaxes mixed, never produces an asset. */
		Dsh,
		/** 1.x material (frozen syntax, second front end). */
		Dsm,
		/** 1.x material function / layer / layer blend (frozen syntax, second front end). */
		Dsf,
		Unknown,
	};

	/** `.dss` / `.dsh` / `.dsm` / `.dsf`, case-insensitively; anything else is Unknown. */
	DREAMSHADERLANG_API ELangFileKind GetLangFileKindFromPath(const FString& Path);
	DREAMSHADERLANG_API const TCHAR* LexToString(ELangFileKind Kind);

	/**
	 * One source text with its line table.
	 *
	 * Built once per parse; the lexer asks it for line/column as it emits tokens, and the printer
	 * and the raw-body capture slice it by span. Line terminators are `\n`, `\r\n` and a lone `\r`,
	 * all counting as one line, which matches how the preprocessor conserves line counts.
	 */
	class DREAMSHADERLANG_API FLangSourceText
	{
	public:
		FLangSourceText() = default;
		FLangSourceText(FString InPath, FString InText);

		const FString& GetPath() const { return Path; }
		const FString& GetText() const { return Text; }
		int32 Len() const { return Text.Len(); }
		int32 GetLineCount() const { return LineStarts.Num(); }

		/** 1-based line and column of an offset; an offset past the end maps to the last position. */
		void GetLineAndColumn(int32 Offset, int32& OutLine, int32& OutColumn) const;

		/** The text of a 1-based line, without its terminator. Empty for a line out of range. */
		FString GetLineText(int32 Line) const;

		/** A span with its line and column resolved. */
		FLangSpan MakeSpan(int32 Offset, int32 Length) const;

		/** The text a span covers. */
		FString Slice(const FLangSpan& Span) const;

	private:
		FString Path;
		FString Text;
		/** Offset of the first TCHAR of every line; LineStarts[0] is always 0. */
		TArray<int32> LineStarts;
	};
}
