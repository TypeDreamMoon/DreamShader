// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `dsc fmt` without the editor: one source text in, the same file in the printer's layout out.
//
// The formatter is the printer (LangPrinter.h) over a parse that kept its trivia, and nothing else -- there is no
// second opinion about layout anywhere. What this adds is the refusal to write a file it cannot vouch for: the
// formatted text has to parse, to the same tree, with every comment the source had, and printing it again has to
// give the same text. A file that fails any of those is left alone and the failure is the formatter's (DSH9044),
// which is what lets `fmt -All` run over a tree nobody has read line by line.
//
// Two kinds of file are not formatted, and neither is an error:
//   * a file with 1.x declarations (a `.dsm` / `.dsf`, a `.dsh` that still has `Function` blocks): what the printer
//     writes for those is 2.0 text, and turning 1.x into 2.0 is `dsc migrate`, which checks far more (DSH9042);
//   * a file that uses the preprocessor outside an opaque body: the parser reads preprocessed text, the formatter
//     reads the file as it is on disk, and reprinting either branch of an `#if` would delete the other (DSH9043).
//
// Core-only.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Lang
{
	enum class ELangFormatOutcome : uint8
	{
		/** The text is already what the printer writes. */
		Unchanged,
		/** OutText is the formatted file. */
		Changed,
		/** Not a file `fmt` rewrites; an info in the sink says which kind (DSH9042, DSH9043). Nothing is wrong with it. */
		Skipped,
		/** The file does not parse, or the formatted text failed its own check (DSH9044). Nothing to write. */
		Failed,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(ELangFormatOutcome Outcome);

	struct FLangFormatOptions
	{
		/**
		 * Indent and blank-line policy. NewLine is ignored: the formatted file keeps the line terminator the source
		 * uses (`\r\n` when the source has one anywhere, `\n` otherwise), so a format never shows up as a whole-file diff.
		 */
		FLangPrintOptions Print;
	};

	/**
	 * Formats one source. OutText is set for Unchanged and Changed (the text to have on disk), and left empty
	 * otherwise. Parse errors go to Diagnostics as the parser raised them.
	 */
	DREAMSHADERLANG_API ELangFormatOutcome FormatDreamShaderLangSource(
		const FLangSourceText& Source,
		const FLangFormatOptions& Options,
		FString& OutText,
		FLangDiagnosticSink& Diagnostics);
}
