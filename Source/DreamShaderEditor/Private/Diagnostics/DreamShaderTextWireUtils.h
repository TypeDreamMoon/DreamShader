// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Culture-invariant wire string reconstruction for diagnostic FText values.
//
// WHY THIS EXISTS: the diagnostics wire format (diagnostics.json / diagnostics/*.json /
// bridge.db) is parsed by VSCode and Rider extensions and MUST stay English regardless of the
// editor culture. FTextInspector::GetSourceString returns the source-language string for plain
// LOCTEXT/NSLOCTEXT entries and the raw input for FText::FromString dynamic text, so it is safe
// for those. But for an FText::Format result it returns the LOCALIZED substituted display
// string (FTextHistory::GetSourceString defaults to GetDisplayString and format histories do
// not override it), so a zh-Hans editor would leak localized text into the wire the moment a
// message site uses FText::Format(LOCTEXT(...), ...).
//
// ToInvariantWireString reconstructs the invariant English string of any FText by replaying its
// historic format data with invariant-culture rendering of every argument. The result is the
// same under every editor culture, so message sites may be converted to FText::Format without
// changing the wire format.

#pragma once

#ifndef DREAMSHADER_TEXT_WIRE_UTILS_H
#define DREAMSHADER_TEXT_WIRE_UTILS_H

#include "CoreMinimal.h"

#include "DreamShaderVersionCompat.h"
#include "Internationalization/Internationalization.h"
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
#include "Internationalization/Text.h"
#else
#include "Internationalization/TextInspector.h"
#endif

namespace UE::DreamShader::Editor::Private
{
	// Reconstructs the culture-invariant English wire string for Text:
	//  - plain LOCTEXT / FText::FromString dynamic text: the source string as-is.
	//  - FText::Format results: the source pattern replayed with every argument rendered in the
	//    invariant culture (numbers via invariant AsNumber, nested formats recursively), so the
	//    rebuilt text is identical under any editor culture.
	inline FString ToInvariantWireString(const FText& Text)
	{
		TArray<FHistoricTextFormatData> FormatData;
		FTextInspector::GetHistoricFormatData(Text, FormatData);

		// Plain text (LOCTEXT, FromString, or a format whose history did not record arguments):
		// the source string IS the invariant English text.
		if (FormatData.IsEmpty())
		{
			const FString* SourceString = nullptr;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
			SourceString = FTextInspector::GetSourceString(Text);
#else
			FTextInspector::GetSourceString(Text, SourceString);
#endif
			return SourceString != nullptr ? *SourceString : Text.ToString();
		}

		// The outermost historic layer holds the pattern and arguments of the final format
		// operation (nested formats appear as their own inner layers, ordered deepest first).
		const FHistoricTextFormatData& Outermost = FormatData.Last();

		FFormatNamedArguments InvariantArguments;
		for (const TPair<FString, FFormatArgumentValue>& Pair : Outermost.Arguments)
		{
			const FFormatArgumentValue& Argument = Pair.Value;
			FString InvariantValue;

			switch (Argument.GetType())
			{
			case EFormatArgumentType::Text:
				// Nested formatted text (or a dynamic/LOCTEXT argument): recurse so the whole
				// subtree is rebuilt invariantly.
				InvariantValue = ToInvariantWireString(Argument.GetTextValue());
				break;

			case EFormatArgumentType::Int:
			case EFormatArgumentType::UInt:
			case EFormatArgumentType::Float:
			case EFormatArgumentType::Double:
			{
				// Format the RAW numeric value with the invariant culture. FFormatArgumentValue::
				// ToFormattedString must not be used here: it renders with the CURRENT locale
				// (Text.cpp FFormatArgumentValue::ToFormattedString uses
				// FInternationalization::Get().GetCurrentLocale()). The invariant culture
				// already uses '.'/',' like en-US, but grouping is explicitly disabled so a
				// 5+ digit count stays "1234" instead of "1,234" in every culture.
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
				// Gender and any future non-text/non-numeric argument types have no
				// culture-dependent rendering; replay them as-is.
				InvariantValue = Argument.ToFormattedString(false, true);
				break;
			}

			// FFormatArgumentValue has no FString constructor (EFormatArgumentType is only
			// Int/UInt/Float/Double/Text/Gender), so an invariant string travels as dynamic
			// text: FText::FromString's source string IS its display string under any culture.
			InvariantArguments.Add(Pair.Key, FFormatArgumentValue(FText::FromString(InvariantValue)));
		}

		// Replay the OUTERMOST pattern with the invariant arguments. The pattern must come from
		// Fmt.SourceFmt.GetSourceText(): FTextFormatData::GetSourceString() returns
		// SourceText.ToString() for a text-backed format (the LOCALIZED display), while
		// GetSourceText() yields the pattern FText itself (or a dynamic text for a string-backed
		// pattern). Recursing through ToInvariantWireString extracts the English LOCTEXT literal
		// (or the raw pattern string) exactly like the engine's own rebuild-as-source path does
		// with BuildSourceString().
		return FText::Format(
			FTextFormat::FromString(ToInvariantWireString(Outermost.SourceFmt.GetSourceText())),
			InvariantArguments).ToString();
	}
}

#endif // DREAMSHADER_TEXT_WIRE_UTILS_H
