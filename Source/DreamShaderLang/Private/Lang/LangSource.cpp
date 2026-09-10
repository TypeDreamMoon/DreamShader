// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FLangSourceText: the text plus its line table.
//
// The line table is built ONCE, in the constructor, and every position question is answered from
// it by binary search. That is deliberate: the lexer asks for a line/column per token, so anything
// that counts newlines per query turns lexing into O(n^2) on a big .dsh. Line terminators are
// `\n`, `\r\n` and a lone `\r`, each counting as exactly one line break -- the same three the 1.x
// preprocessor conserves, so a diagnostic raised here lands on the same physical line the
// preprocessor reported.

#include "Lang/LangSource.h"

#include "Misc/Paths.h"

namespace UE::DreamShader::Lang
{
	ELangFileKind GetLangFileKindFromPath(const FString& Path)
	{
		// GetExtension returns the text after the LAST dot, without the dot, or empty when there is
		// none. "Foo.dsm.bak" is therefore "bak" -> Unknown, which is what we want: only the real
		// extension selects a front end.
		const FString Extension = FPaths::GetExtension(Path);

		if (Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			return ELangFileKind::Dss;
		}
		if (Extension.Equals(TEXT("dsh"), ESearchCase::IgnoreCase))
		{
			return ELangFileKind::Dsh;
		}
		if (Extension.Equals(TEXT("dsm"), ESearchCase::IgnoreCase))
		{
			return ELangFileKind::Dsm;
		}
		if (Extension.Equals(TEXT("dsf"), ESearchCase::IgnoreCase))
		{
			return ELangFileKind::Dsf;
		}

		return ELangFileKind::Unknown;
	}

	const TCHAR* LexToString(const ELangFileKind Kind)
	{
		switch (Kind)
		{
		case ELangFileKind::Dss:
			return TEXT("Dss");
		case ELangFileKind::Dsh:
			return TEXT("Dsh");
		case ELangFileKind::Dsm:
			return TEXT("Dsm");
		case ELangFileKind::Dsf:
			return TEXT("Dsf");
		case ELangFileKind::Unknown:
		default:
			return TEXT("Unknown");
		}
	}

	FLangSourceText::FLangSourceText(FString InPath, FString InText)
		: Path(MoveTemp(InPath))
		, Text(MoveTemp(InText))
	{
		// LineStarts[0] is always 0, even for empty text: an empty file still has line 1, and every
		// query below relies on the array being non-empty.
		LineStarts.Add(0);

		const int32 Length = Text.Len();
		for (int32 Index = 0; Index < Length; ++Index)
		{
			const TCHAR Character = Text[Index];
			if (Character == TEXT('\n'))
			{
				LineStarts.Add(Index + 1);
			}
			else if (Character == TEXT('\r'))
			{
				// `\r\n` is ONE break. Consuming the `\n` here (rather than letting the loop see it)
				// is what keeps a CRLF file from reporting twice as many lines as an LF one.
				if (Index + 1 < Length && Text[Index + 1] == TEXT('\n'))
				{
					++Index;
				}
				LineStarts.Add(Index + 1);
			}
		}
	}

	void FLangSourceText::GetLineAndColumn(const int32 Offset, int32& OutLine, int32& OutColumn) const
	{
		// A span may legitimately point AT the end of the text (the EndOfFile token), and a caller
		// recovering from a truncated file may point past it; both map to the last position.
		const int32 Clamped = FMath::Clamp(Offset, 0, Text.Len());

		if (LineStarts.Num() == 0)
		{
			// Only a default-constructed FLangSourceText gets here (no constructor ran, so no line
			// table). Answer as if the whole text were one line rather than indexing an empty array.
			OutLine = 1;
			OutColumn = Clamped + 1;
			return;
		}

		// The greatest Index with LineStarts[Index] <= Clamped. LineStarts is strictly increasing
		// and LineStarts[0] == 0 <= Clamped, so the invariant "Low is a valid answer" holds from
		// the start and the loop always terminates with Low == High.
		int32 Low = 0;
		int32 High = LineStarts.Num() - 1;
		while (Low < High)
		{
			const int32 Mid = Low + ((High - Low) + 1) / 2;
			if (LineStarts[Mid] <= Clamped)
			{
				Low = Mid;
			}
			else
			{
				High = Mid - 1;
			}
		}

		OutLine = Low + 1;
		// One column per TCHAR: a tab counts as one, exactly like the 1.x diagnostics, so an editor
		// that renders tabs as four spaces still puts the caret on the right character.
		OutColumn = (Clamped - LineStarts[Low]) + 1;
	}

	FString FLangSourceText::GetLineText(const int32 Line) const
	{
		if (Line < 1 || Line > LineStarts.Num())
		{
			return FString();
		}

		const int32 Start = LineStarts[Line - 1];
		int32 End = (Line < LineStarts.Num()) ? LineStarts[Line] : Text.Len();

		// Drop the terminator this line was cut at: `\n`, then a `\r` in front of it (CRLF), or a
		// lone `\r`.
		while (End > Start && (Text[End - 1] == TEXT('\n') || Text[End - 1] == TEXT('\r')))
		{
			--End;
		}

		return Text.Mid(Start, End - Start);
	}

	FLangSpan FLangSourceText::MakeSpan(const int32 Offset, const int32 Length) const
	{
		FLangSpan Span;
		Span.Offset = FMath::Clamp(Offset, 0, Text.Len());
		Span.Length = FMath::Clamp(Length, 0, Text.Len() - Span.Offset);
		GetLineAndColumn(Span.Offset, Span.Line, Span.Column);
		return Span;
	}

	FString FLangSourceText::Slice(const FLangSpan& Span) const
	{
		const int32 Start = FMath::Clamp(Span.Offset, 0, Text.Len());
		const int32 Length = FMath::Clamp(Span.Length, 0, Text.Len() - Start);
		return Text.Mid(Start, Length);
	}
}
