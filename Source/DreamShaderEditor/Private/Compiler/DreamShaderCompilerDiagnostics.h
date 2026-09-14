// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The seam between the 2.0 front end's diagnostics and the 1.x reporting surfaces.
//
// The front end raises FLangDiagnostics: a DSHnnnn code, a severity, an FText and an FLangSpan
// that knows its file, line, column AND length. Everything downstream of the pipeline was built
// for the 1.x shape instead -- an FDreamShaderError carrying one code and one string, and an
// FDreamShaderDiagnosticRecord whose location is a point, not a range. This file is the one place
// the two shapes meet, so that no other translation unit has to know that they differ.
//
// Three products, from one sink:
//
//   * FDreamShaderError for the pipeline's OutError. Its first line is the first ERROR in the 1.x
//     located wire form `<file>(<line>,<column>): DSHnnnn: <message>`, which is exactly what the
//     frozen DreamShaderCompilerPipeline.h promises its caller. Every remaining diagnostic follows
//     on its own line in the same form -- see BuildLang2CompileError for why that is not padding.
//   * FDreamShaderDiagnosticRecord + span length, for the diagnostics store the bridge reads.
//   * The wire JSON, `version: 1` with the new optional `length` field (plan §3.7).
//
// Messages are converted to their INVARIANT form on the way out (ToInvariantWireString), so a log
// line, a JSON file and a corpus expectation read the same English whatever the editor culture is.
// The code is the stable half; the text is free to be reworded and translated.

#pragma once

#include "CoreMinimal.h"

// FDreamShaderDiagnosticRecord: the shape the store holds. Included rather than forward declared
// because FLang2DiagnosticRecord has one by value.
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
// FDreamShaderError, the 1.x carrier the pipeline's OutError is.
#include "DreamShaderDiagnostic.h"
// FLangDiagnostic / FLangDiagnosticSink / ELangSeverity.
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * One 2.0 diagnostic in the shape the 1.x diagnostics store holds, plus the one thing that shape
	 * has no room for.
	 *
	 * FDreamShaderDiagnosticRecord's location is a point -- line and column, both 1-based -- because
	 * the 1.x generator recovered it by re-parsing `path(line,column):` out of a message string and
	 * there was never a length to recover. The 2.0 front end has the span, and an editor that is
	 * handed a length can underline the offending token rather than a single character. Carrying it
	 * beside the record rather than inside it keeps the store's own struct untouched (it is not this
	 * unit's to change); adding `int32 Length` there and dropping this wrapper is recorded in the
	 * unit report as the follow-up.
	 */
	struct FLang2DiagnosticRecord
	{
		Private::FDreamShaderDiagnosticRecord Record;

		/** The span's length in TCHARs. 0 when the diagnostic has no range (a whole-file failure). */
		int32 Length = 0;
	};

	/** `error` / `warning` / `info` -- the spellings FDreamShaderDiagnosticRecord::Severity already uses. */
	const TCHAR* Lang2SeverityWireName(UE::DreamShader::Lang::ELangSeverity Severity);

	/**
	 * Which stage of the pipeline a DSHnnnn code belongs to, for the `stage` field the extension
	 * groups by: `preprocess`, `parse`, `bind`, `ir`, `generate`, `shader`, `tools`.
	 *
	 * Derived from the code rather than passed down from the raise site on purpose. A sink is
	 * merged and moved between stages (the include resolver's sub-sink joins the binder's), so by
	 * the time a diagnostic is reported nobody still knows which call raised it -- but its code
	 * range says, and the ranges are the contract (CONTRACT §7). An unrecognised code answers
	 * `compile`, never an empty string, so the field is never written blank.
	 */
	FString Lang2StageForCode(const FString& Code);

	/**
	 * `<file>(<line>,<column>): DSHnnnn: <message>` -- the located wire form the 1.x generator
	 * produces and FDreamShaderDiagnosticsStore::TryParseErrorLocation parses back.
	 *
	 * FallbackFilePath is used when the diagnostic carries none, which happens for a failure that
	 * belongs to the compile rather than to a position in it (an unreadable file).
	 */
	FString FormatLang2DiagnosticWireLine(
		const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic,
		const FString& FallbackFilePath);

	/** Every diagnostic of the sink, in the order they were raised, as store records. */
	void BuildLang2DiagnosticRecords(
		const UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
		const FString& SourceFilePath,
		TArray<FLang2DiagnosticRecord>& OutRecords);

	/**
	 * The sink as one FDreamShaderError.
	 *
	 * Line 1 is the first ERROR, in the located wire form; OutError.Code is that error's DSHnnnn, so
	 * the code survives into everything keyed on it. Every OTHER diagnostic -- the remaining errors,
	 * then the warnings, then the infos -- follows on its own line in the same form.
	 *
	 * The extra lines are load-bearing, not decoration. The only route into the bridge's diagnostics
	 * store is FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics, which splits the compile
	 * message into lines and turns each located one into its own record. Report only the first error
	 * and the store holds only the first error, so the editor underlines one mistake at a time even
	 * though the front end found five. (The store cannot be written to directly: the bridge's
	 * SetDiagnostics is private and Bridge/ is not this unit's to edit -- see the unit report.)
	 *
	 * Returns false when the sink holds no error at all, leaving OutError untouched; a caller that
	 * is failing for another reason keeps its own message.
	 */
	bool BuildLang2CompileError(
		const UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
		const FString& SourceFilePath,
		UE::DreamShader::FDreamShaderError& OutError);

	/**
	 * The wire JSON, one file's worth:
	 *
	 *   { "schema": "dreamshader-diagnostics", "version": 1, "updatedAtUtc": "...",
	 *     "files": [ { "path": "<source>", "diagnostics": [
	 *        { "message", "detail", "stage", "code", "line", "column", "length", "severity", "source" } ] } ] }
	 *
	 * The envelope is deliberately the one FDreamShaderDiagnosticsStore::WriteToFile writes, so a
	 * consumer that already reads `diagnostics.json` reads this with no change; `length` is the one
	 * addition and is omitted when it is 0, which is what makes it optional rather than a schema
	 * break.
	 */
	FString BuildLang2DiagnosticsWireJson(const FString& SourceFilePath, const TArray<FLang2DiagnosticRecord>& Records);

	/**
	 * Writes BuildLang2DiagnosticsWireJson to a file, creating its directory.
	 *
	 * OutError is a plain string rather than a sink or an FDreamShaderError because the caller is
	 * the one that owns a code here: this function serialises a sink, so raising into a sink it is
	 * in the middle of reading would be circular. The `check` verb wraps a false return in DSH9036.
	 */
	bool WriteLang2DiagnosticsWireJson(
		const FString& OutputFilePath,
		const FString& SourceFilePath,
		const TArray<FLang2DiagnosticRecord>& Records,
		FString& OutError);

	/**
	 * Logs every diagnostic through LogDreamShader in the 1.x format -- errors as Error, warnings as
	 * Warning, infos as Display -- so a commandlet run reads the same as a 1.x one and `dsc.ps1`'s
	 * colouring keeps working.
	 */
	void LogLang2Diagnostics(
		const UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
		const FString& SourceFilePath);
}
