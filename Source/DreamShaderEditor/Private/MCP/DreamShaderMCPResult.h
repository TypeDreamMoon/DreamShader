// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The result body every DreamMCP action answers with, and the conversions into it.
//
// One shape for every action, short or long, success or refusal: `ok`, `outcome`, `reason`, `summary`,
// `diagnostics`, `writes`, `artifacts`, `data`, `next`. The MCP server reads only these fields -- never
// the wording -- so `outcome` and `reason` carry the decision and the text is free to change. Every
// string here is wire text: invariant English (ToInvariantWireString for an FText from the compiler),
// and every path is relative to the project root with forward slashes, never absolute.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace UE::DreamShader::Lang
{
	class FLangDiagnosticSink;
}

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderDiagnosticRecord;
}

namespace UE::DreamShader::Editor::Compiler
{
	struct FLang2DiagnosticRecord;
}

namespace UE::DreamShader::Editor::MCP
{
	/** `ok` / `source-error` / `refused` / `engine-error`. */
	enum class EOutcome : uint8
	{
		/** The action did what it was asked. */
		Ok,
		/** The source has an error: the diagnostics say where; the fix is an edit. */
		SourceError,
		/** Not the source's fault: bad arguments, a missing file, an asset that may not be rebuilt. `reason` is a stable code. */
		Refused,
		/** Something inside the engine or the plugin failed. */
		EngineError,
	};

	const TCHAR* LexOutcome(EOutcome Outcome);

	/** One diagnostic, as the result body carries it. */
	struct FDiagnostic
	{
		FString Code;
		/** `error` / `warning` / `info`. */
		FString Severity = TEXT("error");
		/** Relative to the project root. Empty when the diagnostic names no file. */
		FString File;
		int32 Line = 0;
		int32 Column = 0;
		/** The span's length in characters; 0 when it has none. */
		int32 Length = 0;
		FString Message;
		/** The record's detail, when it has one: what to do about it. */
		FString Hint;
		/** The pipeline stage the code belongs to (`parse`, `bind`, `shader`...). */
		FString Stage;
		/** The documentation page of the code. Empty when there is no code. */
		FString Doc;
	};

	/** One asset the action wrote. `state`: `memory` / `saved` / `created` / `deleted`. */
	struct FWrite
	{
		FString Asset;
		FString State;
	};

	/** A file the action produced for the caller to read: an image, a source text. */
	struct FArtifact
	{
		FString Kind;
		/** Relative to the project root. */
		FString Path;
		FString Title;
	};

	struct FResult
	{
		EOutcome Outcome = EOutcome::Ok;
		/** A stable machine code for a refusal or an engine error, `kebab-case`. Empty for `ok` and `source-error`. */
		FString Reason;
		/** One line for the model: what happened. */
		FString Summary;
		/**
		 * Optional: preformatted lines for the model, which a server prints as they are, after the summary. For content
		 * too long for one line that a model reads rather than parses -- `lookup`'s entry. Not written when empty.
		 */
		FString Text;
		TArray<FDiagnostic> Diagnostics;
		TArray<FWrite> Writes;
		TArray<FArtifact> Artifacts;
		/** Action-specific payload. Null serializes as an empty object. */
		TSharedPtr<FJsonObject> Data;
		/** What the model should do next, one sentence each. */
		TArray<FString> Next;

		bool IsOk() const { return Outcome == EOutcome::Ok; }

		static FResult MakeOk(const FString& InSummary);
		static FResult MakeRefused(const FString& InReason, const FString& InSummary, const FString& InNext = FString());
		static FResult MakeEngineError(const FString& InReason, const FString& InSummary, const FString& InNext = FString());

		TSharedRef<FJsonObject> ToJson() const;
	};

	/** The body serialized, condensed. */
	FString SerializeJson(const TSharedRef<FJsonObject>& Object);

	/** Parses a JSON object. Null when Text is not one. */
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Text);

	/**
	 * A path as the wire carries it: relative to the project directory, forward slashes. A path outside the project keeps its
	 * relative spelling (`../...`), so no absolute path ever leaves the process; an empty path stays empty.
	 */
	FString MakeProjectRelativePath(const FString& Path);

	/**
	 * A `file` argument resolved: an absolute path as given; a relative one against the project directory first, then
	 * against the DreamShader source directory -- whichever exists, the project spelling when neither does.
	 */
	FString ResolveProjectFileArgument(const FString& Argument);

	/** Text with every spelling of the absolute project directory taken out, so a path in it reads project-relative. */
	FString ScrubAbsolutePaths(const FString& Text);

	/** The documentation page of a DSHnnnn code, from the plugin's DocsURL. Empty for anything that is not a code. */
	FString MakeDiagnosticDocUrl(const FString& Code);

	/** A diagnostics-store record on the wire. FallbackFile stands in for a record that names no file. */
	FDiagnostic MakeDiagnostic(const Private::FDreamShaderDiagnosticRecord& Record, int32 Length, const FString& FallbackFile);

	/** Appends every record. */
	void AppendDiagnostics(TArray<FDiagnostic>& Out, const TArray<Compiler::FLang2DiagnosticRecord>& Records, const FString& FallbackFile);
	void AppendDiagnostics(TArray<FDiagnostic>& Out, const TArray<Private::FDreamShaderDiagnosticRecord>& Records, const FString& FallbackFile);

	/** Every diagnostic of a sink, in the order raised. */
	void AppendDiagnostics(TArray<FDiagnostic>& Out, const UE::DreamShader::Lang::FLangDiagnosticSink& Sink, const FString& SourceFile);

	/** How many diagnostics have the severity `error` / `warning`. */
	int32 CountSeverity(const TArray<FDiagnostic>& Diagnostics, const TCHAR* Severity);
}
