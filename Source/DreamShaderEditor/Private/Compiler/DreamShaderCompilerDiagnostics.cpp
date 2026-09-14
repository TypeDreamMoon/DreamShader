// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCompilerDiagnostics.h for what this is for and why the multi-line error message
// is load-bearing rather than verbose.

#include "DreamShaderCompilerDiagnostics.h"

#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "DreamShaderModule.h"

#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UE::DreamShader::Editor::Compiler
{
	using UE::DreamShader::Lang::ELangSeverity;
	using UE::DreamShader::Lang::FLangDiagnostic;
	using UE::DreamShader::Lang::FLangDiagnosticSink;

	namespace
	{
		/** The `source` field: what produced the diagnostic, for a client that groups by producer. */
		const TCHAR* const Lang2DiagnosticSource = TEXT("DreamShader Lang2");

		/**
		 * The numeric part of a DSHnnnn code, or INDEX_NONE.
		 *
		 * Tolerant on purpose: a code that is not of that shape is not a reason to lose the
		 * diagnostic, only a reason not to guess its stage.
		 */
		int32 ParseDiagnosticCodeNumber(const FString& Code)
		{
			if (!Code.StartsWith(TEXT("DSH"), ESearchCase::CaseSensitive))
			{
				return INDEX_NONE;
			}

			const FString Digits = Code.Mid(3);
			if (Digits.IsEmpty() || !Digits.IsNumeric())
			{
				return INDEX_NONE;
			}

			return FCString::Atoi(*Digits);
		}
	}

	const TCHAR* Lang2SeverityWireName(const ELangSeverity Severity)
	{
		// Not LexToString(ELangSeverity): that is the language's own spelling and may change with
		// the language. These three strings are the wire, shared with the 1.x records and with the
		// VS Code extension's severity mapping, so they are pinned here.
		switch (Severity)
		{
		case ELangSeverity::Error:   return TEXT("error");
		case ELangSeverity::Warning: return TEXT("warning");
		case ELangSeverity::Info:    return TEXT("info");
		}

		return TEXT("error");
	}

	FString Lang2StageForCode(const FString& Code)
	{
		const int32 Number = ParseDiagnosticCodeNumber(Code);
		if (Number == INDEX_NONE)
		{
			return TEXT("compile");
		}

		// The ranges are CONTRACT §7 plus the 1.x allocation that predates it. Written as explicit
		// bands rather than as `Number / 1000` because two of them are split inside one thousand:
		// DSH4200-4299 is the binder while DSH4300-4399 is the IR, and DSH6200-6219 is the binder
		// while DSH6220-6299 is lowering and custom HLSL.
		if (Number < 2000) { return TEXT("preprocess"); }
		if (Number < 4000) { return TEXT("parse"); }
		if (Number < 4300) { return TEXT("bind"); }
		if (Number < 4400) { return TEXT("ir"); }
		if (Number < 6000) { return TEXT("bind"); }   // DSH5xxx: reflected calls and the catalog
		if (Number < 6220) { return TEXT("bind"); }   // DSH6200-6219: function kinds, entry, linkage
		if (Number < 6300) { return TEXT("ir"); }     // DSH6220-6249 inlining, DSH6250-6299 custom HLSL
		if (Number < 8000) { return TEXT("bind"); }   // DSH7xxx: uniforms and directives
		if (Number < 9000) { return TEXT("generate"); }
		return TEXT("tools");
	}

	FString FormatLang2DiagnosticWireLine(const FLangDiagnostic& Diagnostic, const FString& FallbackFilePath)
	{
		const FString& File = Diagnostic.FilePath.IsEmpty() ? FallbackFilePath : Diagnostic.FilePath;

		// ToWireString gives `DSHnnnn: <invariant message>`; the located prefix in front of it is
		// the 1.x shape FDreamShaderDiagnosticsStore::TryParseErrorLocation knows how to undo.
		const FString Body = FLangDiagnosticSink::ToWireString(Diagnostic);
		if (File.IsEmpty())
		{
			return Body;
		}

		return FString::Printf(
			TEXT("%s(%d,%d): %s"), /* I18N-EXEMPT: wire form, parsed by the diagnostics store */
			*File,
			FMath::Max(1, Diagnostic.Span.Line),
			FMath::Max(1, Diagnostic.Span.Column),
			*Body);
	}

	void BuildLang2DiagnosticRecords(
		const FLangDiagnosticSink& Sink,
		const FString& SourceFilePath,
		TArray<FLang2DiagnosticRecord>& OutRecords)
	{
		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		OutRecords.Reset();
		OutRecords.Reserve(Sink.Num());

		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			FLang2DiagnosticRecord Entry;
			Private::FDreamShaderDiagnosticRecord& Record = Entry.Record;

			// FilePath is set EXPLICITLY, even when it equals the compiled source. The store buckets
			// a record by its FilePath and falls back to the compiled source only when it is empty,
			// so a diagnostic raised inside an included `.dsh` reaches that header's bucket only if
			// the path travels with it.
			Record.FilePath = Diagnostic.FilePath.IsEmpty()
				? NormalizedSource
				: UE::DreamShader::NormalizeSourceFilePath(Diagnostic.FilePath);
			Record.Message = Diagnostic.Message;
			Record.Detail = FText::FromString(FormatLang2DiagnosticWireLine(Diagnostic, NormalizedSource));
			Record.Stage = Lang2StageForCode(Diagnostic.Code);
			Record.Code = Diagnostic.Code;
			Record.Line = FMath::Max(1, Diagnostic.Span.Line);
			Record.Column = FMath::Max(1, Diagnostic.Span.Column);
			Record.Severity = Lang2SeverityWireName(Diagnostic.Severity);
			Record.Source = Lang2DiagnosticSource;

			Entry.Length = FMath::Max(0, Diagnostic.Span.Length);

			OutRecords.Add(MoveTemp(Entry));
		}
	}

	bool BuildLang2CompileError(
		const FLangDiagnosticSink& Sink,
		const FString& SourceFilePath,
		UE::DreamShader::FDreamShaderError& OutError)
	{
		const FLangDiagnostic* First = Sink.FirstError();
		if (!First)
		{
			return false;
		}

		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		TArray<FString> Lines;
		Lines.Add(FormatLang2DiagnosticWireLine(*First, NormalizedSource));

		// Errors first, then warnings, then infos -- and within each, the order they were raised.
		// The reader of line 1 wants the first ERROR, not the first diagnostic, and a warning raised
		// during parsing must not push the error that stopped the compile off the top.
		const ELangSeverity Order[] = { ELangSeverity::Error, ELangSeverity::Warning, ELangSeverity::Info };
		for (const ELangSeverity Severity : Order)
		{
			for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (&Diagnostic == First || Diagnostic.Severity != Severity)
				{
					continue;
				}

				Lines.Add(FormatLang2DiagnosticWireLine(Diagnostic, NormalizedSource));
			}
		}

		OutError.Code = First->Code;
		OutError.Message = FString::Join(Lines, TEXT("\n"));
		return true;
	}

	FString BuildLang2DiagnosticsWireJson(const FString& SourceFilePath, const TArray<FLang2DiagnosticRecord>& Records)
	{
		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		// Grouped by the record's own file, so a diagnostic raised in an included header appears
		// under that header rather than under the file that included it -- the same bucketing the
		// store does, done here because this writer does not go through the store.
		TArray<FString> FileOrder;
		TMap<FString, TArray<const FLang2DiagnosticRecord*>> ByFile;
		for (const FLang2DiagnosticRecord& Record : Records)
		{
			const FString Key = Record.Record.FilePath.IsEmpty() ? NormalizedSource : Record.Record.FilePath;
			if (!ByFile.Contains(Key))
			{
				FileOrder.Add(Key);
			}
			ByFile.FindOrAdd(Key).Add(&Record);
		}

		if (FileOrder.IsEmpty())
		{
			FileOrder.Add(NormalizedSource);
			ByFile.FindOrAdd(NormalizedSource);
		}

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schema"), TEXT("dreamshader-diagnostics"));
		Writer->WriteValue(TEXT("version"), 1);
		Writer->WriteValue(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		Writer->WriteArrayStart(TEXT("files"));
		for (const FString& File : FileOrder)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("path"), File);
			Writer->WriteArrayStart(TEXT("diagnostics"));
			for (const FLang2DiagnosticRecord* Record : ByFile.FindChecked(File))
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("message"), Private::ToInvariantWireString(Record->Record.Message));
				if (!Record->Record.Detail.IsEmpty())
				{
					Writer->WriteValue(TEXT("detail"), Private::ToInvariantWireString(Record->Record.Detail));
				}
				if (!Record->Record.Stage.IsEmpty())
				{
					Writer->WriteValue(TEXT("stage"), Record->Record.Stage);
				}
				if (!Record->Record.AssetPath.IsEmpty())
				{
					Writer->WriteValue(TEXT("assetPath"), Record->Record.AssetPath);
				}
				if (!Record->Record.ShaderPlatform.IsEmpty())
				{
					Writer->WriteValue(TEXT("shaderPlatform"), Record->Record.ShaderPlatform);
				}
				if (!Record->Record.QualityLevel.IsEmpty())
				{
					Writer->WriteValue(TEXT("qualityLevel"), Record->Record.QualityLevel);
				}
				if (!Record->Record.Code.IsEmpty())
				{
					Writer->WriteValue(TEXT("code"), Record->Record.Code);
				}
				Writer->WriteValue(TEXT("line"), FMath::Max(1, Record->Record.Line));
				Writer->WriteValue(TEXT("column"), FMath::Max(1, Record->Record.Column));
				// Omitted at 0: that is what makes `length` an addition to the schema rather than a
				// break of it. A consumer that does not know the field sees the same object it saw
				// before, and one that does gets a range only where there really is one.
				if (Record->Length > 0)
				{
					Writer->WriteValue(TEXT("length"), Record->Length);
				}
				Writer->WriteValue(TEXT("severity"), Record->Record.Severity);
				Writer->WriteValue(TEXT("source"), Record->Record.Source);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();

		return Json;
	}

	bool WriteLang2DiagnosticsWireJson(
		const FString& OutputFilePath,
		const FString& SourceFilePath,
		const TArray<FLang2DiagnosticRecord>& Records,
		FString& OutError)
	{
		const FString Directory = FPaths::GetPath(OutputFilePath);
		if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				TEXT("could not create directory '%s'"),
				*Directory);
			return false;
		}

		const FString Json = BuildLang2DiagnosticsWireJson(SourceFilePath, Records);
		if (!FFileHelper::SaveStringToFile(Json, *OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				TEXT("could not write '%s'"),
				*OutputFilePath);
			return false;
		}

		return true;
	}

	void LogLang2Diagnostics(const FLangDiagnosticSink& Sink, const FString& SourceFilePath)
	{
		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			const FString Line = FormatLang2DiagnosticWireLine(Diagnostic, NormalizedSource);
			switch (Diagnostic.Severity)
			{
			case ELangSeverity::Error:
				UE_LOG(LogDreamShader, Error, TEXT("%s"), *Line);
				break;
			case ELangSeverity::Warning:
				UE_LOG(LogDreamShader, Warning, TEXT("%s"), *Line);
				break;
			default:
				UE_LOG(LogDreamShader, Display, TEXT("%s"), *Line);
				break;
			}
		}
	}
}
