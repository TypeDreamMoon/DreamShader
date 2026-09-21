// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FormatDreamShaderLangSource: see LangFormat.h for what it promises and what it leaves alone.
//
// Diagnostics owned by this file: DSH9042 (info, 1.x declarations), DSH9043 (info, the preprocessor), DSH9044 (the
// formatted text failed its own check).

#include "Lang/LangFormat.h"

#include "Lang/LangParser.h"
#include "Migrate/LangMigrate.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "DreamShader.Format"

namespace UE::DreamShader::Lang
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangFormatPrivate
	{
		/** The tree alone, in the canonical layout: two texts that print the same here are the same declarations. */
		static FString PrintTreeOnly(const FModule& Module)
		{
			FLangPrintOptions Options;
			Options.bPrintTrivia = false;
			return PrintDreamShaderLang(Module, Options);
		}

		/** Every comment of Before is in After, counted: two equal comments need two. */
		static bool HasEveryComment(TArray<FString> After, const TArray<FString>& Before, FString& OutMissing)
		{
			for (const FString& Comment : Before)
			{
				const int32 Found = After.IndexOfByPredicate([&Comment](const FString& Candidate)
				{
					return Candidate.Equals(Comment, ESearchCase::CaseSensitive);
				});
				if (Found == INDEX_NONE)
				{
					OutMissing = Comment;
					return false;
				}
				After.RemoveAtSwap(Found);
			}
			return true;
		}

		static bool HasErrorCode(const FLangDiagnosticSink& Sink, const TCHAR* Code)
		{
			for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Error && Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}
	}

	const TCHAR* LexToString(const ELangFormatOutcome Outcome)
	{
		switch (Outcome)
		{
		case ELangFormatOutcome::Unchanged: return TEXT("Unchanged");
		case ELangFormatOutcome::Changed:   return TEXT("Changed");
		case ELangFormatOutcome::Skipped:   return TEXT("Skipped");
		case ELangFormatOutcome::Failed:    return TEXT("Failed");
		}
		return TEXT("Failed");
	}

	ELangFormatOutcome FormatDreamShaderLangSource(
		const FLangSourceText& Source,
		const FLangFormatOptions& Options,
		FString& OutText,
		FLangDiagnosticSink& Diagnostics)
	{
		using namespace LangFormatPrivate;

		OutText.Reset();
		const FLangSpan NoSpan;

		FLangParseOptions ParseOptions;
		ParseOptions.bKeepTrivia = true;
		FLangParseResult Parsed = ParseDreamShaderLang(Source, ParseOptions);

		// The preprocessor first, because it is also why the parse failed: a `#if` line is DSH3201 to a parser that
		// expects preprocessed text, and what follows it is whatever recovery made of two branches read as one.
		if (HasErrorCode(Parsed.Diagnostics, TEXT("DSH3201")))
		{
			Diagnostics.Info(TEXT("DSH9043"), NoSpan, FText::Format(
				LOCTEXT("FormatPreprocessor", "'{0}' uses the preprocessor outside a custom body; 'fmt' reads the file as it is on disk and would have to drop one side of every '#if', so it leaves the file as it is."),
				FText::FromString(Source.GetPath())));
			return ELangFormatOutcome::Skipped;
		}

		if (!Parsed.Succeeded())
		{
			Diagnostics.Append(MoveTemp(Parsed.Diagnostics));
			return ELangFormatOutcome::Failed;
		}

		if (Parsed.Legacy.IsValid())
		{
			Diagnostics.Info(TEXT("DSH9042"), NoSpan, FText::Format(
				LOCTEXT("FormatLegacy", "'{0}' has 1.x declarations, and what the printer writes for those is 2.0 text; rewriting 1.x as 2.0 is 'dsc migrate', so 'fmt' leaves the file as it is."),
				FText::FromString(Source.GetPath())));
			return ELangFormatOutcome::Skipped;
		}

		FLangPrintOptions PrintOptions = Options.Print;
		PrintOptions.bPrintTrivia = true;
		// The source's own line terminator, so that a format is never a whole-file diff.
		PrintOptions.NewLine = Source.GetText().Contains(TEXT("\r\n"), ESearchCase::CaseSensitive) ? TEXT("\r\n") : TEXT("\n");

		const FString Formatted = PrintDreamShaderLang(*Parsed.Module, PrintOptions);

		if (Formatted.Equals(Source.GetText(), ESearchCase::CaseSensitive))
		{
			OutText = Formatted;
			return ELangFormatOutcome::Unchanged;
		}

		// ----- the check: the text parses, to the same tree, with the same comments, and prints as itself
		const auto Refuse = [&Diagnostics, &Source, &NoSpan](const FText& Why)
		{
			Diagnostics.Error(TEXT("DSH9044"), NoSpan, FText::Format(
				LOCTEXT("FormatCheckFailed", "The formatted text of '{0}' failed its own check -- {1} -- so nothing was written. This is a fault of the formatter, not of the file."),
				FText::FromString(Source.GetPath()),
				Why));
			return ELangFormatOutcome::Failed;
		};

		const FLangSourceText FormattedSource(Source.GetPath(), Formatted);
		FLangParseResult Reparsed = ParseDreamShaderLang(FormattedSource, ParseOptions);
		if (!Reparsed.Succeeded())
		{
			FString FirstError;
			for (const FLangDiagnostic& Diagnostic : Reparsed.Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Error)
				{
					// The wire form: English whatever the editor's culture is, as a log line has to be.
					FirstError = FLangDiagnosticSink::ToWireString(Diagnostic);
					break;
				}
			}
			return Refuse(FText::Format(LOCTEXT("FormatCheckParse", "it does not parse ({0})"), FText::FromString(FirstError)));
		}

		if (!PrintTreeOnly(*Reparsed.Module).Equals(PrintTreeOnly(*Parsed.Module), ESearchCase::CaseSensitive))
		{
			return Refuse(LOCTEXT("FormatCheckTree", "it parses to other declarations than the file does"));
		}

		// `///` lines are part of the tree (the printer writes one directive per line), so they were compared above;
		// these are the comments that are nothing but trivia.
		TArray<FString> Before;
		TArray<FString> After;
		CollectDreamShaderComments(Source, /* bIncludeDocComments */ false, Before);
		CollectDreamShaderComments(FormattedSource, /* bIncludeDocComments */ false, After);
		FString Missing;
		if (!HasEveryComment(MoveTemp(After), Before, Missing))
		{
			return Refuse(FText::Format(LOCTEXT("FormatCheckComment", "the comment '{0}' is not in it"), FText::FromString(Missing)));
		}

		if (!PrintDreamShaderLang(*Reparsed.Module, PrintOptions).Equals(Formatted, ESearchCase::CaseSensitive))
		{
			return Refuse(LOCTEXT("FormatCheckStable", "formatting it again gives another text"));
		}

		OutText = Formatted;
		return ELangFormatOutcome::Changed;
	}
}

#undef LOCTEXT_NAMESPACE
