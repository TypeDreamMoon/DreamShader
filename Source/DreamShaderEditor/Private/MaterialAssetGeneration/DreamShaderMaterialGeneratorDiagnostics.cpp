// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Diagnostic-formatting helpers for the DreamShader material generator (prepared-source index ->
// file/line/column mapping + parse/generate/code-block error formatting). Pure string processing,
// no asset or graph state. Extracted from DreamShaderMaterialGenerator.cpp's anonymous namespace
// and promoted to external linkage via DreamShaderMaterialGeneratorDiagnostics.h.

#include "DreamShaderMaterialGeneratorDiagnostics.h"

namespace UE::DreamShader::Editor
{
	bool TryExtractPreparedSourceIndexFromError(const FString& Error, int32& OutIndex)
	{
		const FString Needle = TEXT("near index ");
		const int32 NeedleIndex = Error.Find(Needle, ESearchCase::IgnoreCase);
		if (NeedleIndex == INDEX_NONE)
		{
			return false;
		}

		int32 Cursor = NeedleIndex + Needle.Len();
		while (Cursor < Error.Len() && FChar::IsWhitespace(Error[Cursor]))
		{
			++Cursor;
		}

		const int32 NumberStart = Cursor;
		while (Cursor < Error.Len() && FChar::IsDigit(Error[Cursor]))
		{
			++Cursor;
		}

		if (Cursor == NumberStart)
		{
			return false;
		}

		OutIndex = FCString::Atoi(*Error.Mid(NumberStart, Cursor - NumberStart));
		return OutIndex >= 0;
	}

	bool TryMapPreparedSourceIndexToLocation(
		const FString& PreparedSource,
		const int32 SourceIndex,
		FString& OutFilePath,
		int32& OutLine,
		int32& OutColumn)
	{
		const FString BeginMarker = TEXT("// Begin DreamShader source: ");
		const FString EndMarker = TEXT("// End DreamShader source: ");

		FString CurrentFilePath;
		int32 CurrentSourceLine = 1;
		int32 Cursor = 0;
		while (Cursor <= PreparedSource.Len())
		{
			int32 LineEnd = PreparedSource.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (LineEnd == INDEX_NONE)
			{
				LineEnd = PreparedSource.Len();
			}

			const FString LineText = PreparedSource.Mid(Cursor, LineEnd - Cursor);
			if (LineText.StartsWith(BeginMarker))
			{
				CurrentFilePath = LineText.RightChop(BeginMarker.Len()).TrimStartAndEnd();
				CurrentSourceLine = 1;
			}
			else if (LineText.StartsWith(EndMarker))
			{
				CurrentFilePath.Reset();
				CurrentSourceLine = 1;
			}
			else
			{
				const int32 LineEndInclusive = LineEnd < PreparedSource.Len() ? LineEnd + 1 : LineEnd;
				if (SourceIndex >= Cursor && SourceIndex <= LineEndInclusive && !CurrentFilePath.IsEmpty())
				{
					OutFilePath = CurrentFilePath;
					OutLine = FMath::Max(1, CurrentSourceLine);
					OutColumn = FMath::Max(1, SourceIndex - Cursor + 1);
					return true;
				}

				if (!CurrentFilePath.IsEmpty())
				{
					++CurrentSourceLine;
				}
			}

			if (LineEnd >= PreparedSource.Len())
			{
				break;
			}
			Cursor = LineEnd + 1;
		}

		return false;
	}

	FString FormatParseErrorWithSourceLocation(
		const FString& FallbackSourceFilePath,
		const FString& PreparedSource,
		const FString& ParseError)
	{
		int32 SourceIndex = INDEX_NONE;
		FString MappedFilePath;
		int32 MappedLine = 1;
		int32 MappedColumn = 1;
		if (TryExtractPreparedSourceIndexFromError(ParseError, SourceIndex)
			&& TryMapPreparedSourceIndexToLocation(PreparedSource, SourceIndex, MappedFilePath, MappedLine, MappedColumn))
		{
			return FString::Printf(TEXT("%s(%d,%d): %s"), *MappedFilePath, MappedLine, MappedColumn, *ParseError); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return FString::Printf(TEXT("%s: %s"), *FallbackSourceFilePath, *ParseError); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	bool LooksLikeLocatedDiagnostic(const FString& Error)
	{
		int32 CloseMarkerIndex = INDEX_NONE;
		if (!Error.FindChar(TCHAR(')'), CloseMarkerIndex))
		{
			return false;
		}

		const int32 OpenMarkerIndex = Error.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CloseMarkerIndex);
		return OpenMarkerIndex != INDEX_NONE
			&& Error.Left(OpenMarkerIndex).Find(TEXT(": ")) == INDEX_NONE
			&& Error.Find(TEXT(": "), ESearchCase::CaseSensitive, ESearchDir::FromStart, CloseMarkerIndex) != INDEX_NONE;
	}

	FString FormatGenerateError(const FString& SourceFilePath, const FString& Error)
	{
		return LooksLikeLocatedDiagnostic(Error)
			? Error
			: FString::Printf(TEXT("%s: %s"), *SourceFilePath, *Error); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	void ResolveCodeBlockLocation(
		const FString& FallbackSourceFilePath,
		const FString& PreparedSource,
		const int32 CodeStartIndex,
		FString& OutFilePath,
		int32& OutLine,
		int32& OutColumn)
	{
		OutFilePath = FallbackSourceFilePath;
		OutLine = 1;
		OutColumn = 1;
		if (CodeStartIndex != INDEX_NONE)
		{
			TryMapPreparedSourceIndexToLocation(PreparedSource, CodeStartIndex, OutFilePath, OutLine, OutColumn);
		}
	}

	FString FormatCodeBlockError(
		const FString& FallbackSourceFilePath,
		const FString& CodeSourceFilePath,
		const int32 CodeStartLine,
		const int32 CodeStartColumn,
		const FString& Error,
		const int32 ErrorLine,
		const int32 ErrorColumn)
	{
		if (LooksLikeLocatedDiagnostic(Error) || ErrorLine <= 0 || ErrorColumn <= 0)
		{
			return FormatGenerateError(FallbackSourceFilePath, Error);
		}

		const int32 Line = CodeStartLine + ErrorLine - 1;
		const int32 Column = ErrorLine <= 1
			? CodeStartColumn + ErrorColumn - 1
			: ErrorColumn;
		return FString::Printf(TEXT("%s(%d,%d): %s"), *CodeSourceFilePath, Line, Column, *Error); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}
}
