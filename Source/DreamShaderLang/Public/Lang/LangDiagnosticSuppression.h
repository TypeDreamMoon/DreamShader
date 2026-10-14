// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The warnings and errors a source turns off for itself (after 2.1.2).
//
//   #pragma disable_warning(DSH7224, DSH8237)   the whole file; written in the file being compiled, every diagnostic
//   #pragma disable_error(DSH8219)               of the compile, the headers it includes as well
//   x = f(); // disable_warning DSH5251          that line
//   // disable_error DSH4210                     alone on its line: the next line that is not blank or another such comment
//
// A file of either language takes all four (the 1.x front end accepts the two `#pragma` lines at its top level). The
// lists are read off the text of each file, not off its tree: a `//` comment never reaches a parser, and a header's
// lines are read from the header when one of its diagnostics comes up. `#if` does not hide either spelling.
//
// A disabled diagnostic is not reported at all. For an error that means it does not fail the compile either: the
// sink does not count it, so a stage that asks HasErrors goes on. A stage that cannot go on without what the error was
// about still stops, and when nothing else then says why, the pipeline reports the disabled error after all
// (FLangDiagnosticSink::GetSuppressedDiagnostics). An info is never disabled: the pipeline reads some of them back.
//
// The lists apply while an FLangDiagnosticSuppressionScope is open on the thread that raises: one per pipeline run, a
// nested run (a `.dsi`'s parent) opening its own over its own file.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Lang
{
	/** What one file's text disables. Codes are upper case. Lines are 1-based. */
	struct DREAMSHADERLANG_API FLangDisabledDiagnostics
	{
		TSet<FString> FileWarnings;
		TSet<FString> FileErrors;
		TMap<int32, TSet<FString>> LineWarnings;
		TMap<int32, TSet<FString>> LineErrors;

		/** A word in a list that is not `DSHnnnn`: where it is, for the pipeline to say so (DSH7236). */
		struct FMalformed
		{
			FString Text;
			int32 Line = 1;
			int32 Column = 1;
			/** Into the scanned text, and how much of it. */
			int32 Offset = 0;
			int32 Length = 0;
		};
		TArray<FMalformed> Malformed;

		bool IsEmpty() const;
	};

	/** Reads the four spellings out of a file's text. */
	DREAMSHADERLANG_API void ScanLangDisabledDiagnostics(const FString& Text, FLangDisabledDiagnostics& Out);

	/**
	 * The lists in force while it lives, on the thread that made it. RootFilePath is the file being compiled, and its
	 * file-wide lists cover every diagnostic; RootText is its text as compiled. Any other file's lists are read from
	 * disk the first time one of its diagnostics comes up, and cover that file only.
	 */
	class DREAMSHADERLANG_API FLangDiagnosticSuppressionScope
	{
	public:
		FLangDiagnosticSuppressionScope(const FString& RootFilePath, const FString& RootText);
		~FLangDiagnosticSuppressionScope();

		FLangDiagnosticSuppressionScope(const FLangDiagnosticSuppressionScope&) = delete;
		FLangDiagnosticSuppressionScope& operator=(const FLangDiagnosticSuppressionScope&) = delete;

		/**
		 * One more file whose file-wide lists cover the whole compile, with its own text read from disk: the package header
		 * a synthesized asset unit stands in for.
		 */
		void AddRootFile(const FString& FilePath);

		/** What the root text disables, for the pipeline to report its malformed words. */
		const FLangDisabledDiagnostics& GetRootLists() const { return Root; }

		/** True when a diagnostic of this severity and code, at FilePath's Line (0: no line), is disabled. Never for an info. */
		bool ShouldSuppress(ELangSeverity Severity, const FString& Code, const FString& FilePath, int32 Line) const;

		/** The innermost scope open on this thread, or null. */
		static const FLangDiagnosticSuppressionScope* GetCurrent();

	private:
		const FLangDisabledDiagnostics& FindOrReadFile(const FString& NormalizedPath) const;

		FString RootFile;
		FLangDisabledDiagnostics Root;
		/** Every file-wide list of the extra root files, merged. */
		TSet<FString> RootWideWarnings;
		TSet<FString> RootWideErrors;
		mutable TMap<FString, FLangDisabledDiagnostics> Files;
		FLangDiagnosticSuppressionScope* Outer = nullptr;
	};

	/** FLangDiagnosticSuppressionScope::GetCurrent()->ShouldSuppress, false with no scope open. */
	DREAMSHADERLANG_API bool ShouldSuppressLangDiagnostic(ELangSeverity Severity, const FString& Code, const FString& FilePath, int32 Line);
}
