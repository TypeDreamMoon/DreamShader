// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Diagnostics for the DreamShaderLang front end.
//
// One structure, not two: the 1.x parser carried FText and the 1.x generator carried FString, and
// every hand-off between them lost either the localisation or the code. Here every diagnostic is
// a DSHnnnn code, a severity, an FText message (so the localisation gather sees it) and a span.
// The English wire form for logs and JSON is derived on demand.
//
// Raise helpers are METHODS on the sink and are spelled `Sink.Error(TEXT("DSHnnnn"), Span, LOCTEXT(...))`
// -- code first -- because .skill/gen-diagnostics.ps1 finds raise sites by that shape, the same
// way it finds FailWith(...) in the 1.x code. Do not wrap them in another helper that hides the
// literal code from the scanner.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Lang
{
	enum class ELangSeverity : uint8
	{
		Error,
		Warning,
		Info,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(ELangSeverity Severity);

	struct FLangDiagnostic
	{
		/** DSHnnnn. Never empty: every raise site in this module names its code. */
		FString Code;
		ELangSeverity Severity = ELangSeverity::Error;
		/** Localisable message, formatted at the raise site. */
		FText Message;
		FLangSpan Span;
		/** The source file the span refers to, as the FLangSourceText spelled it. */
		FString FilePath;
	};

	/**
	 * Collects diagnostics for one front-end run. The parser keeps going after an error where it
	 * can, so a sink may hold several; callers ask HasErrors() to decide whether the result is
	 * usable, not whether the sink is empty.
	 */
	class DREAMSHADERLANG_API FLangDiagnosticSink
	{
	public:
		FLangDiagnosticSink() = default;
		explicit FLangDiagnosticSink(FString InFilePath);

		/** Records an error. Returns false so `return Sink.Error(...)` reads as the 1.x FailWith does. */
		bool Error(const TCHAR* Code, const FLangSpan& Span, const FText& Message);
		void Warning(const TCHAR* Code, const FLangSpan& Span, const FText& Message);
		void Info(const TCHAR* Code, const FLangSpan& Span, const FText& Message);

		bool HasErrors() const { return ErrorCount > 0; }
		int32 NumErrors() const { return ErrorCount; }
		int32 Num() const { return Diagnostics.Num(); }

		const TArray<FLangDiagnostic>& GetDiagnostics() const { return Diagnostics; }
		const FString& GetFilePath() const { return FilePath; }

		/** Moves every diagnostic of Other onto the end of this sink. */
		void Append(FLangDiagnosticSink&& Other);

		/** The first error, for callers that can only report one. Null when there is none. */
		const FLangDiagnostic* FirstError() const;

		/**
		 * `DSHnnnn: <message>` with the message in its invariant (source) form, so logs, the
		 * diagnostics JSON and the corpus expectations read English whatever the editor culture is.
		 */
		static FString ToWireString(const FLangDiagnostic& Diagnostic);

	private:
		void Add(ELangSeverity Severity, const TCHAR* Code, const FLangSpan& Span, const FText& Message);

		FString FilePath;
		TArray<FLangDiagnostic> Diagnostics;
		int32 ErrorCount = 0;
	};
}
