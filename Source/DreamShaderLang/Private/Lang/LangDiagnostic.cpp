// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FLangDiagnosticSink.
//
// The one interesting piece here is ToWireString. The wire format (diagnostics.json, the bridge,
// the corpus goldens, the VSCode/Rider extensions) MUST stay English whatever culture the editor
// runs in. FTextInspector::GetSourceString gives that for a plain LOCTEXT and for
// FText::FromString, but for an FText::Format RESULT it returns the localized, already-substituted
// display string -- so the moment a raise site formats its message (and most of them do, they name
// the token they found) a zh-Hans editor would leak Chinese into the wire.
//
// The 1.x code solves this in Editor/Private/Diagnostics/DreamShaderTextWireUtils.h by replaying
// the historic format data with invariant arguments. That header lives in the Editor module, which
// this module must not depend on, so the same idea is reimplemented here over the same Core API.
// Keep the two in step if either changes.

#include "Lang/LangDiagnostic.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"

namespace UE::DreamShader::Lang
{
	namespace Private
	{
		/**
		 * The culture-invariant English string of an FText.
		 *
		 *  - plain LOCTEXT / FText::FromString: the source string as written.
		 *  - FText::Format results: the source pattern replayed with every argument rendered in the
		 *    invariant culture (numbers via invariant AsNumber with grouping off, nested formats
		 *    recursively), so the rebuilt text is identical under any editor culture.
		 */
		static FString ToLangInvariantWireString(const FText& Text)
		{
			TArray<FHistoricTextFormatData> FormatData;
			FTextInspector::GetHistoricFormatData(Text, FormatData);

			if (FormatData.IsEmpty())
			{
				const FString* SourceString = FTextInspector::GetSourceString(Text);
				return SourceString != nullptr ? *SourceString : Text.ToString();
			}

			// The outermost layer holds the pattern and arguments of the final format operation;
			// nested formats appear as their own inner layers, deepest first.
			const FHistoricTextFormatData& Outermost = FormatData.Last();

			FFormatNamedArguments InvariantArguments;
			for (const TPair<FString, FFormatArgumentValue>& Pair : Outermost.Arguments)
			{
				const FFormatArgumentValue& Argument = Pair.Value;
				FString InvariantValue;

				switch (Argument.GetType())
				{
				case EFormatArgumentType::Text:
					InvariantValue = ToLangInvariantWireString(Argument.GetTextValue());
					break;

				case EFormatArgumentType::Int:
				case EFormatArgumentType::UInt:
				case EFormatArgumentType::Float:
				case EFormatArgumentType::Double:
				{
					// NOT FFormatArgumentValue::ToFormattedString: that renders with the CURRENT
					// locale. Grouping is switched off so a five-digit offset stays "12345" and not
					// "12,345" in en-US or "12 345" elsewhere.
					FNumberFormattingOptions NumberOptions;
					NumberOptions.UseGrouping = false;
					const FCulturePtr InvariantCulture = FInternationalization::Get().GetInvariantCulture();

					switch (Argument.GetType())
					{
					case EFormatArgumentType::Int:
						InvariantValue = FText::AsNumber(Argument.GetIntValue(), &NumberOptions, InvariantCulture).ToString();
						break;
					case EFormatArgumentType::UInt:
						InvariantValue = FText::AsNumber(Argument.GetUIntValue(), &NumberOptions, InvariantCulture).ToString();
						break;
					case EFormatArgumentType::Float:
						InvariantValue = FText::AsNumber(Argument.GetFloatValue(), &NumberOptions, InvariantCulture).ToString();
						break;
					case EFormatArgumentType::Double:
					default:
						InvariantValue = FText::AsNumber(Argument.GetDoubleValue(), &NumberOptions, InvariantCulture).ToString();
						break;
					}
					break;
				}

				default:
					// Gender and anything added later: no culture-dependent rendering to undo.
					InvariantValue = Argument.ToFormattedString(false, true);
					break;
				}

				// FFormatArgumentValue has no FString form, so an invariant string travels as
				// dynamic text -- FText::FromString's source string IS its display string.
				InvariantArguments.Add(Pair.Key, FFormatArgumentValue(FText::FromString(InvariantValue)));
			}

			// The pattern must come from GetSourceText(), not GetSourceString(): the latter returns
			// the LOCALIZED display string of a text-backed format.
			return FText::Format(
				FTextFormat::FromString(ToLangInvariantWireString(Outermost.SourceFmt.GetSourceText())),
				InvariantArguments).ToString();
		}
	}

	const TCHAR* LexToString(const ELangSeverity Severity)
	{
		// Lower case on purpose: this is the spelling the 1.x diagnostics wire already uses
		// (FDreamShaderStoredDiagnostic::Severity defaults to TEXT("error")), and the VSCode and
		// Rider extensions match on it.
		switch (Severity)
		{
		case ELangSeverity::Warning:
			return TEXT("warning");
		case ELangSeverity::Info:
			return TEXT("info");
		case ELangSeverity::Error:
		default:
			return TEXT("error");
		}
	}

	FLangDiagnosticSink::FLangDiagnosticSink(FString InFilePath)
		: FilePath(MoveTemp(InFilePath))
	{
	}

	bool FLangDiagnosticSink::Error(const TCHAR* Code, const FLangSpan& Span, const FText& Message)
	{
		Add(ELangSeverity::Error, Code, Span, Message);
		// Always false, so a raise site reads `return Diagnostics.Error(...)` exactly like the 1.x
		// FailWith does.
		return false;
	}

	void FLangDiagnosticSink::Warning(const TCHAR* Code, const FLangSpan& Span, const FText& Message)
	{
		Add(ELangSeverity::Warning, Code, Span, Message);
	}

	void FLangDiagnosticSink::Info(const TCHAR* Code, const FLangSpan& Span, const FText& Message)
	{
		Add(ELangSeverity::Info, Code, Span, Message);
	}

	void FLangDiagnosticSink::Append(FLangDiagnosticSink&& Other)
	{
		Diagnostics.Reserve(Diagnostics.Num() + Other.Diagnostics.Num());
		for (FLangDiagnostic& Diagnostic : Other.Diagnostics)
		{
			if (Diagnostic.Severity == ELangSeverity::Error)
			{
				++ErrorCount;
			}
			Diagnostics.Add(MoveTemp(Diagnostic));
		}

		// Leave the source empty rather than merely moved-from-but-populated: a sink that was
		// appended has handed its contents over, and its counters must say so.
		Other.Diagnostics.Reset();
		Other.ErrorCount = 0;
	}

	const FLangDiagnostic* FLangDiagnosticSink::FirstError() const
	{
		for (const FLangDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Severity == ELangSeverity::Error)
			{
				return &Diagnostic;
			}
		}
		return nullptr;
	}

	FString FLangDiagnosticSink::ToWireString(const FLangDiagnostic& Diagnostic)
	{
		return Diagnostic.Code + TEXT(": ") + Private::ToLangInvariantWireString(Diagnostic.Message);
	}

	void FLangDiagnosticSink::Add(const ELangSeverity Severity, const TCHAR* Code, const FLangSpan& Span, const FText& Message)
	{
		FLangDiagnostic Diagnostic;
		Diagnostic.Code = Code;
		Diagnostic.Severity = Severity;
		Diagnostic.Message = Message;
		Diagnostic.Span = Span;
		Diagnostic.FilePath = FilePath;

		if (Severity == ELangSeverity::Error)
		{
			++ErrorCount;
		}

		Diagnostics.Add(MoveTemp(Diagnostic));
	}
}
