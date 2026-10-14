// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See LangDiagnosticSuppression.h.

#include "Lang/LangDiagnosticSuppression.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Lang
{
	namespace LangDiagnosticSuppressionDetail
	{
		/** One scope per thread at most is innermost; a pipeline run opens and closes its own on the game thread. */
		thread_local FLangDiagnosticSuppressionScope* GCurrentScope = nullptr;

		FString NormalizeSuppressionPath(const FString& FilePath)
		{
			FString Path = FPaths::ConvertRelativePathToFull(FilePath);
			FPaths::NormalizeFilename(Path);
			FPaths::CollapseRelativeDirectories(Path);
			return Path;
		}

		/** Where a `//` comment starts on the line, outside a string literal; INDEX_NONE when there is none. */
		int32 FindLineComment(const FString& Line)
		{
			bool bInString = false;
			for (int32 Index = 0; Index + 1 < Line.Len(); ++Index)
			{
				const TCHAR Char = Line[Index];
				if (bInString)
				{
					if (Char == TEXT('\\'))
					{
						++Index;
					}
					else if (Char == TEXT('"'))
					{
						bInString = false;
					}
					continue;
				}
				if (Char == TEXT('"'))
				{
					bInString = true;
				}
				else if (Char == TEXT('/') && Line[Index + 1] == TEXT('/'))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** `disable_warning` / `disable_error` at the start of Text, followed by the end or a separator: which one, and the rest. */
		bool MatchDisableWord(const FString& Text, bool& bOutError, FString& OutRest)
		{
			static const FString WarningWord = TEXT("disable_warning");
			static const FString ErrorWord = TEXT("disable_error");
			for (const bool bError : { false, true })
			{
				const FString& Word = bError ? ErrorWord : WarningWord;
				if (!Text.StartsWith(Word, ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Text.Len() > Word.Len())
				{
					const TCHAR Next = Text[Word.Len()];
					if (FChar::IsAlnum(Next) || Next == TEXT('_'))
					{
						continue;
					}
				}
				bOutError = bError;
				OutRest = Text.RightChop(Word.Len());
				return true;
			}
			return false;
		}

		/** The codes of a list -- `DSH7224, DSH8237`, `(DSH7224)`, `: DSH7224` -- upper case; whatever is not one goes to Malformed. */
		void ReadCodeList(const FString& List, const FString& Line, const int32 LineNumber, const int32 LineOffset, TArray<FString>& OutCodes, TArray<FLangDisabledDiagnostics::FMalformed>& Malformed)
		{
			FString Words = List;
			for (const TCHAR* Separator : { TEXT("("), TEXT(")"), TEXT(","), TEXT(";"), TEXT(":"), TEXT("\t") })
			{
				Words.ReplaceInline(Separator, TEXT(" "), ESearchCase::CaseSensitive);
			}

			TArray<FString> Tokens;
			Words.ParseIntoArrayWS(Tokens);
			for (const FString& Token : Tokens)
			{
				const bool bCode = Token.Len() == 7
					&& Token.Left(3).Equals(TEXT("DSH"), ESearchCase::IgnoreCase)
					&& FChar::IsDigit(Token[3]) && FChar::IsDigit(Token[4]) && FChar::IsDigit(Token[5]) && FChar::IsDigit(Token[6]);
				if (bCode)
				{
					OutCodes.AddUnique(Token.ToUpper());
					continue;
				}
				FLangDisabledDiagnostics::FMalformed& Entry = Malformed.AddDefaulted_GetRef();
				Entry.Text = Token;
				Entry.Line = LineNumber;
				const int32 Found = Line.Find(Token, ESearchCase::CaseSensitive);
				Entry.Column = Found == INDEX_NONE ? 1 : Found + 1;
				Entry.Offset = LineOffset + Entry.Column - 1;
				Entry.Length = Token.Len();
			}
			if (Tokens.IsEmpty())
			{
				// `// disable_warning` with nothing after it: said, because it reads as "all of them", which it is not.
				FLangDisabledDiagnostics::FMalformed& Entry = Malformed.AddDefaulted_GetRef();
				Entry.Line = LineNumber;
				Entry.Column = 1;
				Entry.Offset = LineOffset;
				Entry.Length = FMath::Max(1, Line.Len());
			}
		}

		void AddCodes(TMap<int32, TSet<FString>>& Map, const int32 Line, const TArray<FString>& Codes)
		{
			TSet<FString>& Set = Map.FindOrAdd(Line);
			Set.Append(Codes);
		}
	}

	bool FLangDisabledDiagnostics::IsEmpty() const
	{
		return FileWarnings.IsEmpty() && FileErrors.IsEmpty() && LineWarnings.IsEmpty() && LineErrors.IsEmpty();
	}

	void ScanLangDisabledDiagnostics(const FString& Text, FLangDisabledDiagnostics& Out)
	{
		using namespace LangDiagnosticSuppressionDetail;

		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty*/ false);

		// Comments alone on their line wait here for the next line that holds code.
		TArray<TPair<bool, TArray<FString>>> Pending;

		int32 LineOffset = 0;
		for (int32 Index = 0; Index < Lines.Num(); LineOffset += Lines[Index].Len() + 1, ++Index)
		{
			const int32 LineNumber = Index + 1;
			FString Line = Lines[Index];
			Line.RemoveFromEnd(TEXT("\r"), ESearchCase::CaseSensitive);
			const FString Trimmed = Line.TrimStartAndEnd();

			// `#pragma disable_warning(...)` / `#pragma disable_error(...)`: the whole file.
			if (Trimmed.StartsWith(TEXT("#"), ESearchCase::CaseSensitive))
			{
				FString Payload = Trimmed.RightChop(1).TrimStart();
				if (Payload.StartsWith(TEXT("pragma"), ESearchCase::CaseSensitive) && Payload.Len() > 6 && FChar::IsWhitespace(Payload[6]))
				{
					Payload = Payload.RightChop(6).TrimStart();
					const int32 CommentAt = FindLineComment(Payload);
					if (CommentAt != INDEX_NONE)
					{
						Payload = Payload.Left(CommentAt);
					}
					bool bError = false;
					FString Rest;
					if (MatchDisableWord(Payload, bError, Rest))
					{
						TArray<FString> Codes;
						ReadCodeList(Rest, Line, LineNumber, LineOffset, Codes, Out.Malformed);
						(bError ? Out.FileErrors : Out.FileWarnings).Append(Codes);
					}
				}
			}

			const int32 CommentAt = FindLineComment(Line);
			// A `///` line is source too: a directive in it is where a diagnostic about the directive points.
			const bool bDocLine = CommentAt != INDEX_NONE && CommentAt + 2 < Line.Len() && Line[CommentAt + 2] == TEXT('/');
			const bool bHasCode = bDocLine || !(CommentAt == INDEX_NONE ? Trimmed : Line.Left(CommentAt).TrimStartAndEnd()).IsEmpty();

			// `// disable_warning DSH...`: this line when code stands before it, else the next line that holds code. A `///`
			// line is documentation, never one of these.
			if (CommentAt != INDEX_NONE && !bDocLine)
			{
				const FString Body = Line.Mid(CommentAt + 2).TrimStartAndEnd();
				bool bError = false;
				FString Rest;
				if (MatchDisableWord(Body, bError, Rest))
				{
					TArray<FString> Codes;
					ReadCodeList(Rest, Line, LineNumber, LineOffset, Codes, Out.Malformed);
					if (bHasCode)
					{
						AddCodes(bError ? Out.LineErrors : Out.LineWarnings, LineNumber, Codes);
					}
					else
					{
						Pending.Emplace(bError, MoveTemp(Codes));
					}
				}
			}

			if (bHasCode)
			{
				for (const TPair<bool, TArray<FString>>& Entry : Pending)
				{
					AddCodes(Entry.Key ? Out.LineErrors : Out.LineWarnings, LineNumber, Entry.Value);
				}
				Pending.Reset();
			}
		}
	}

	FLangDiagnosticSuppressionScope::FLangDiagnosticSuppressionScope(const FString& RootFilePath, const FString& RootText)
		: RootFile(LangDiagnosticSuppressionDetail::NormalizeSuppressionPath(RootFilePath))
		, Outer(LangDiagnosticSuppressionDetail::GCurrentScope)
	{
		ScanLangDisabledDiagnostics(RootText, Root);
		LangDiagnosticSuppressionDetail::GCurrentScope = this;
	}

	FLangDiagnosticSuppressionScope::~FLangDiagnosticSuppressionScope()
	{
		LangDiagnosticSuppressionDetail::GCurrentScope = Outer;
	}

	void FLangDiagnosticSuppressionScope::AddRootFile(const FString& FilePath)
	{
		const FString Normalized = LangDiagnosticSuppressionDetail::NormalizeSuppressionPath(FilePath);
		if (Normalized.Equals(RootFile, ESearchCase::IgnoreCase))
		{
			return;
		}
		const FLangDisabledDiagnostics& Lists = FindOrReadFile(Normalized);
		RootWideWarnings.Append(Lists.FileWarnings);
		RootWideErrors.Append(Lists.FileErrors);
	}

	const FLangDisabledDiagnostics& FLangDiagnosticSuppressionScope::FindOrReadFile(const FString& NormalizedPath) const
	{
		if (const FLangDisabledDiagnostics* Found = Files.Find(NormalizedPath))
		{
			return *Found;
		}

		FLangDisabledDiagnostics& Lists = Files.Add(NormalizedPath);
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *NormalizedPath))
		{
			ScanLangDisabledDiagnostics(Text, Lists);
		}
		return Lists;
	}

	bool FLangDiagnosticSuppressionScope::ShouldSuppress(const ELangSeverity Severity, const FString& Code, const FString& FilePath, const int32 Line) const
	{
		if (Severity == ELangSeverity::Info || Code.IsEmpty())
		{
			return false;
		}

		const bool bError = Severity == ELangSeverity::Error;
		const FString Upper = Code.ToUpper();
		if ((bError ? Root.FileErrors : Root.FileWarnings).Contains(Upper)
			|| (bError ? RootWideErrors : RootWideWarnings).Contains(Upper))
		{
			return true;
		}

		const FString Normalized = FilePath.IsEmpty() ? RootFile : LangDiagnosticSuppressionDetail::NormalizeSuppressionPath(FilePath);
		const bool bRoot = Normalized.Equals(RootFile, ESearchCase::IgnoreCase);
		const FLangDisabledDiagnostics& Lists = bRoot ? Root : FindOrReadFile(Normalized);
		if (!bRoot && (bError ? Lists.FileErrors : Lists.FileWarnings).Contains(Upper))
		{
			return true;
		}
		if (Line > 0)
		{
			if (const TSet<FString>* Codes = (bError ? Lists.LineErrors : Lists.LineWarnings).Find(Line))
			{
				return Codes->Contains(Upper);
			}
		}
		return false;
	}

	const FLangDiagnosticSuppressionScope* FLangDiagnosticSuppressionScope::GetCurrent()
	{
		return LangDiagnosticSuppressionDetail::GCurrentScope;
	}

	bool ShouldSuppressLangDiagnostic(const ELangSeverity Severity, const FString& Code, const FString& FilePath, const int32 Line)
	{
		const FLangDiagnosticSuppressionScope* const Scope = FLangDiagnosticSuppressionScope::GetCurrent();
		return Scope && Scope->ShouldSuppress(Severity, Code, FilePath, Line);
	}
}
