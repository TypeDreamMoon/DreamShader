// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The JSON reader/writer declared in IRJson.h. See that header for why this exists rather than a
// dependency on the engine's Json module.

#include "IRJson.h"

#include "Math/NumericLimits.h"
#include "Misc/Char.h"

namespace UE::DreamShader::IR::Private
{
	// ------------------------------------------------------------------------------- values

	const FIRJsonValue* FIRJsonValue::Find(const TCHAR* Name) const
	{
		if (Kind != EIRJsonKind::Object || Name == nullptr)
		{
			return nullptr;
		}

		for (const TPair<FString, FIRJsonValue>& Member : Members)
		{
			// Case-sensitive: JSON keys are keys, and FString::operator== is not.
			if (Member.Key.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Member.Value;
			}
		}

		return nullptr;
	}

	const FIRJsonValue* FIRJsonValue::FindArray(const TCHAR* Name) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsArray()) ? Value : nullptr;
	}

	const FIRJsonValue* FIRJsonValue::FindObject(const TCHAR* Name) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsObject()) ? Value : nullptr;
	}

	FString FIRJsonValue::GetString(const TCHAR* Name, const FString& Fallback) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsString()) ? Value->String : Fallback;
	}

	bool FIRJsonValue::GetBool(const TCHAR* Name, const bool bFallback) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsBool()) ? Value->bBool : bFallback;
	}

	double FIRJsonValue::GetNumber(const TCHAR* Name, const double Fallback) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsNumber()) ? Value->Number : Fallback;
	}

	int64 FIRJsonValue::GetInt(const TCHAR* Name, const int64 Fallback) const
	{
		const FIRJsonValue* Value = Find(Name);
		return (Value != nullptr && Value->IsNumber()) ? static_cast<int64>(Value->Number) : Fallback;
	}

	void FIRJsonValue::GetStringArray(const TCHAR* Name, TArray<FString>& OutValues) const
	{
		OutValues.Reset();

		const FIRJsonValue* Array = FindArray(Name);
		if (Array == nullptr)
		{
			return;
		}

		OutValues.Reserve(Array->Items.Num());
		for (const FIRJsonValue& Item : Array->Items)
		{
			if (Item.IsString())
			{
				OutValues.Add(Item.String);
			}
		}
	}

	// ------------------------------------------------------------------------------ numbers

	FString FormatIRNumber(const double Value)
	{
		// NaN never equals itself. Testing it this way avoids depending on which FMath::IsNaN
		// overloads this engine version happens to have.
		if (!(Value == Value))
		{
			return TEXT("nan");
		}
		if (Value > TNumericLimits<double>::Max())
		{
			return TEXT("inf");
		}
		if (Value < TNumericLimits<double>::Lowest())
		{
			return TEXT("-inf");
		}
		if (Value == 0.0)
		{
			// Folds -0.0 onto 0 so two runs that differ only in the sign of a zero produce the same
			// dump and the same dedupe key.
			return TEXT("0");
		}

		// An integral value prints without a decimal point: `2`, not `2.0000000`. That is what the
		// 1.x dumps look like and what a hand-written expectation file reads like.
		if (Value >= -9.007199254740992e15 && Value <= 9.007199254740992e15)
		{
			const int64 AsInt = static_cast<int64>(Value);
			if (static_cast<double>(AsInt) == Value)
			{
				return FString::Printf(TEXT("%lld"), AsInt);
			}
		}

		// 9 significant digits round-trips a float exactly and is far short of a double's 17, which
		// keeps `0.1f` from printing as 0.100000001490116. Shader constants do not need more.
		return FString::Printf(TEXT("%.9g"), Value);
	}

	// ------------------------------------------------------------------------------ escaping

	void AppendIRJsonEscaped(FString& Out, const FString& Value)
	{
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			switch (Character)
			{
			case TEXT('\"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\b'): Out += TEXT("\\b");  break;
			case TEXT('\f'): Out += TEXT("\\f");  break;
			case TEXT('\n'): Out += TEXT("\\n");  break;
			case TEXT('\r'): Out += TEXT("\\r");  break;
			case TEXT('\t'): Out += TEXT("\\t");  break;
			default:
				if (Character < TEXT(' '))
				{
					// The rest of the C0 range. `\u00XX` is the only legal spelling for it and the
					// lower-case hex keeps the output stable whatever the platform's printf does.
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Character));
				}
				else
				{
					// Everything else, non-ASCII included, is written through: the file is UTF-8 and
					// escaping it would only make it harder to read and to diff.
					Out.AppendChar(Character);
				}
				break;
			}
		}
	}

	// -------------------------------------------------------------------------------- writer

	void FIRJsonWriter::AppendIndent()
	{
		const int32 Spaces = Levels.Num() * 2;
		for (int32 Index = 0; Index < Spaces; ++Index)
		{
			Out.AppendChar(TEXT(' '));
		}
	}

	void FIRJsonWriter::WriteMemberPrefix()
	{
		if (Levels.Num() == 0)
		{
			return;
		}

		FLevel& Level = Levels.Last();
		if (Level.bAnyItem)
		{
			Out.AppendChar(TEXT(','));
		}
		Out.AppendChar(TEXT('\n'));
		AppendIndent();
		Level.bAnyItem = true;
	}

	void FIRJsonWriter::WriteValue(const TCHAR* Text)
	{
		if (bAfterKey)
		{
			bAfterKey = false;
		}
		else
		{
			WriteMemberPrefix();
		}
		Out += Text;
	}

	void FIRJsonWriter::WriteValue(const FString& Text)
	{
		if (bAfterKey)
		{
			bAfterKey = false;
		}
		else
		{
			WriteMemberPrefix();
		}
		Out += Text;
	}

	void FIRJsonWriter::BeginObject()
	{
		WriteValue(TEXT("{"));
		Levels.Add(FLevel());
	}

	void FIRJsonWriter::EndObject()
	{
		bool bAnyItem = false;
		if (Levels.Num() > 0)
		{
			bAnyItem = Levels.Last().bAnyItem;
			Levels.SetNum(Levels.Num() - 1);
		}

		// An empty object stays on one line as `{}`; anything else closes on its own line at the
		// indentation of the key that opened it.
		if (bAnyItem)
		{
			Out.AppendChar(TEXT('\n'));
			AppendIndent();
		}
		Out.AppendChar(TEXT('}'));
	}

	void FIRJsonWriter::BeginArray()
	{
		WriteValue(TEXT("["));
		Levels.Add(FLevel());
	}

	void FIRJsonWriter::EndArray()
	{
		bool bAnyItem = false;
		if (Levels.Num() > 0)
		{
			bAnyItem = Levels.Last().bAnyItem;
			Levels.SetNum(Levels.Num() - 1);
		}

		if (bAnyItem)
		{
			Out.AppendChar(TEXT('\n'));
			AppendIndent();
		}
		Out.AppendChar(TEXT(']'));
	}

	void FIRJsonWriter::Key(const TCHAR* Name)
	{
		WriteMemberPrefix();
		Out.AppendChar(TEXT('\"'));
		AppendIRJsonEscaped(Out, Name != nullptr ? FString(Name) : FString());
		Out += TEXT("\": ");
		bAfterKey = true;
	}

	void FIRJsonWriter::ValueString(const FString& Value)
	{
		FString Text;
		Text.AppendChar(TEXT('\"'));
		AppendIRJsonEscaped(Text, Value);
		Text.AppendChar(TEXT('\"'));
		WriteValue(Text);
	}

	void FIRJsonWriter::ValueNumber(const double Value)
	{
		WriteValue(FormatIRNumber(Value));
	}

	void FIRJsonWriter::ValueInt(const int64 Value)
	{
		WriteValue(FString::Printf(TEXT("%lld"), Value));
	}

	void FIRJsonWriter::ValueBool(const bool bValue)
	{
		WriteValue(bValue ? TEXT("true") : TEXT("false"));
	}

	void FIRJsonWriter::ValueNull()
	{
		WriteValue(TEXT("null"));
	}

	void FIRJsonWriter::KeyString(const TCHAR* Name, const FString& Value)
	{
		Key(Name);
		ValueString(Value);
	}

	void FIRJsonWriter::KeyNumber(const TCHAR* Name, const double Value)
	{
		Key(Name);
		ValueNumber(Value);
	}

	void FIRJsonWriter::KeyInt(const TCHAR* Name, const int64 Value)
	{
		Key(Name);
		ValueInt(Value);
	}

	void FIRJsonWriter::KeyBool(const TCHAR* Name, const bool bValue)
	{
		Key(Name);
		ValueBool(bValue);
	}

	void FIRJsonWriter::KeyStringArray(const TCHAR* Name, const TArray<FString>& Values)
	{
		Key(Name);
		BeginArray();
		for (const FString& Value : Values)
		{
			ValueString(Value);
		}
		EndArray();
	}

	FString FIRJsonWriter::Release()
	{
		FString Result = MoveTemp(Out);
		Out.Reset();
		Levels.Reset();
		bAfterKey = false;

		// One trailing newline, always: a file that ends without one is a diff hazard, and a file
		// that ends with two would not round-trip.
		if (!Result.EndsWith(TEXT("\n"), ESearchCase::CaseSensitive))
		{
			Result.AppendChar(TEXT('\n'));
		}
		return Result;
	}

	// -------------------------------------------------------------------------------- reader

	namespace
	{
		/** Recursion guard: a pathological file must fail, not overflow the stack. */
		constexpr int32 GMaxJsonDepth = 64;

		class FIRJsonReader
		{
		public:
			explicit FIRJsonReader(const FString& InText)
				: Text(InText)
			{
			}

			bool ParseDocument(FIRJsonValue& OutValue)
			{
				SkipWhitespace();
				if (!ParseValue(OutValue, 0))
				{
					return false;
				}
				SkipWhitespace();
				if (Index < Text.Len())
				{
					return Fail(TEXT("expected the end of the document"));
				}
				return true;
			}

			const FString& GetError() const { return Error; }

		private:
			bool Fail(const TCHAR* What)
			{
				if (!Error.IsEmpty())
				{
					// Keep the first, innermost failure: it names the actual problem.
					return false;
				}

				int32 Line = 1;
				int32 Column = 1;
				const int32 Limit = FMath::Clamp(Index, 0, Text.Len());
				for (int32 Scan = 0; Scan < Limit; ++Scan)
				{
					if (Text[Scan] == TEXT('\n'))
					{
						++Line;
						Column = 1;
					}
					else
					{
						++Column;
					}
				}

				Error = FString::Printf(TEXT("%d:%d: %s"), Line, Column, What); /* I18N-EXEMPT: parser OutError text, never displayed to an end user */
				return false;
			}

			void SkipWhitespace()
			{
				while (Index < Text.Len())
				{
					const TCHAR Character = Text[Index];
					if (Character == TEXT(' ') || Character == TEXT('\t') || Character == TEXT('\n') || Character == TEXT('\r'))
					{
						++Index;
						continue;
					}
					break;
				}
			}

			bool Peek(const TCHAR Character) const
			{
				return Index < Text.Len() && Text[Index] == Character;
			}

			bool ParseValue(FIRJsonValue& OutValue, const int32 Depth)
			{
				if (Depth > GMaxJsonDepth)
				{
					return Fail(TEXT("the document nests too deeply"));
				}
				if (Index >= Text.Len())
				{
					return Fail(TEXT("expected a value"));
				}

				const TCHAR Character = Text[Index];
				switch (Character)
				{
				case TEXT('{'): return ParseObject(OutValue, Depth);
				case TEXT('['): return ParseArray(OutValue, Depth);
				case TEXT('\"'):
					OutValue.Kind = EIRJsonKind::String;
					return ParseString(OutValue.String);
				case TEXT('t'):
					if (!Consume(TEXT("true")))
					{
						return Fail(TEXT("expected a value"));
					}
					OutValue.Kind = EIRJsonKind::Bool;
					OutValue.bBool = true;
					return true;
				case TEXT('f'):
					if (!Consume(TEXT("false")))
					{
						return Fail(TEXT("expected a value"));
					}
					OutValue.Kind = EIRJsonKind::Bool;
					OutValue.bBool = false;
					return true;
				case TEXT('n'):
					if (!Consume(TEXT("null")))
					{
						return Fail(TEXT("expected a value"));
					}
					OutValue.Kind = EIRJsonKind::Null;
					return true;
				default:
					break;
				}

				if (Character == TEXT('-') || (Character >= TEXT('0') && Character <= TEXT('9')))
				{
					OutValue.Kind = EIRJsonKind::Number;
					return ParseNumber(OutValue.Number);
				}

				return Fail(TEXT("expected a value"));
			}

			bool Consume(const TCHAR* Literal)
			{
				int32 Offset = 0;
				while (Literal[Offset] != TEXT('\0'))
				{
					if (Index + Offset >= Text.Len() || Text[Index + Offset] != Literal[Offset])
					{
						return false;
					}
					++Offset;
				}
				Index += Offset;
				return true;
			}

			bool ParseObject(FIRJsonValue& OutValue, const int32 Depth)
			{
				OutValue.Kind = EIRJsonKind::Object;
				++Index; // '{'
				SkipWhitespace();

				if (Peek(TEXT('}')))
				{
					++Index;
					return true;
				}

				for (;;)
				{
					SkipWhitespace();
					if (!Peek(TEXT('\"')))
					{
						return Fail(TEXT("expected a member name in quotes"));
					}

					FString Name;
					if (!ParseString(Name))
					{
						return false;
					}

					SkipWhitespace();
					if (!Peek(TEXT(':')))
					{
						return Fail(TEXT("expected ':' after a member name"));
					}
					++Index;
					SkipWhitespace();

					FIRJsonValue Member;
					if (!ParseValue(Member, Depth + 1))
					{
						return false;
					}
					OutValue.Members.Emplace(MoveTemp(Name), MoveTemp(Member));

					SkipWhitespace();
					if (Peek(TEXT(',')))
					{
						++Index;
						continue;
					}
					if (Peek(TEXT('}')))
					{
						++Index;
						return true;
					}
					return Fail(TEXT("expected ',' or '}' in an object"));
				}
			}

			bool ParseArray(FIRJsonValue& OutValue, const int32 Depth)
			{
				OutValue.Kind = EIRJsonKind::Array;
				++Index; // '['
				SkipWhitespace();

				if (Peek(TEXT(']')))
				{
					++Index;
					return true;
				}

				for (;;)
				{
					SkipWhitespace();

					FIRJsonValue Item;
					if (!ParseValue(Item, Depth + 1))
					{
						return false;
					}
					OutValue.Items.Emplace(MoveTemp(Item));

					SkipWhitespace();
					if (Peek(TEXT(',')))
					{
						++Index;
						continue;
					}
					if (Peek(TEXT(']')))
					{
						++Index;
						return true;
					}
					return Fail(TEXT("expected ',' or ']' in an array"));
				}
			}

			bool ParseHex4(int32& OutValue)
			{
				if (Index + 4 > Text.Len())
				{
					return false;
				}

				int32 Result = 0;
				for (int32 Offset = 0; Offset < 4; ++Offset)
				{
					const TCHAR Character = Text[Index + Offset];
					int32 Digit;
					if (Character >= TEXT('0') && Character <= TEXT('9'))
					{
						Digit = Character - TEXT('0');
					}
					else if (Character >= TEXT('a') && Character <= TEXT('f'))
					{
						Digit = 10 + (Character - TEXT('a'));
					}
					else if (Character >= TEXT('A') && Character <= TEXT('F'))
					{
						Digit = 10 + (Character - TEXT('A'));
					}
					else
					{
						return false;
					}
					Result = (Result << 4) | Digit;
				}

				Index += 4;
				OutValue = Result;
				return true;
			}

			void AppendCodePoint(FString& Out, const int32 CodePoint)
			{
				// TCHAR is 16 bits on Windows and 32 on the other platforms this builds for, so a
				// non-BMP code point is either a surrogate pair or one unit. Writing it out here
				// keeps every caller from having to know which. `if constexpr` rather than `if`:
				// the condition is a compile-time constant and MSVC warns (C4127) about those.
				if constexpr (sizeof(TCHAR) == 2)
				{
					if (CodePoint > 0xFFFF)
					{
						const int32 Shifted = CodePoint - 0x10000;
						Out.AppendChar(static_cast<TCHAR>(0xD800 + (Shifted >> 10)));
						Out.AppendChar(static_cast<TCHAR>(0xDC00 + (Shifted & 0x3FF)));
						return;
					}
				}
				Out.AppendChar(static_cast<TCHAR>(CodePoint));
			}

			bool ParseString(FString& OutValue)
			{
				OutValue.Reset();
				++Index; // opening quote

				for (;;)
				{
					if (Index >= Text.Len())
					{
						return Fail(TEXT("a string is not closed before the end of the document"));
					}

					const TCHAR Character = Text[Index];
					if (Character == TEXT('\"'))
					{
						++Index;
						return true;
					}
					if (Character != TEXT('\\'))
					{
						if (Character < TEXT(' '))
						{
							return Fail(TEXT("a raw control character inside a string; write it as an escape"));
						}
						OutValue.AppendChar(Character);
						++Index;
						continue;
					}

					++Index; // backslash
					if (Index >= Text.Len())
					{
						return Fail(TEXT("a string is not closed before the end of the document"));
					}

					const TCHAR Escape = Text[Index++];
					switch (Escape)
					{
					case TEXT('\"'): OutValue.AppendChar(TEXT('\"')); break;
					case TEXT('\\'): OutValue.AppendChar(TEXT('\\')); break;
					case TEXT('/'):  OutValue.AppendChar(TEXT('/'));  break;
					case TEXT('b'):  OutValue.AppendChar(TEXT('\b')); break;
					case TEXT('f'):  OutValue.AppendChar(TEXT('\f')); break;
					case TEXT('n'):  OutValue.AppendChar(TEXT('\n')); break;
					case TEXT('r'):  OutValue.AppendChar(TEXT('\r')); break;
					case TEXT('t'):  OutValue.AppendChar(TEXT('\t')); break;
					case TEXT('u'):
					{
						int32 CodeUnit = 0;
						if (!ParseHex4(CodeUnit))
						{
							return Fail(TEXT("'\\u' needs four hexadecimal digits"));
						}

						// A high surrogate followed by its low half is one code point.
						if (CodeUnit >= 0xD800 && CodeUnit <= 0xDBFF
							&& Index + 1 < Text.Len() && Text[Index] == TEXT('\\') && Text[Index + 1] == TEXT('u'))
						{
							const int32 Saved = Index;
							Index += 2;
							int32 LowUnit = 0;
							if (ParseHex4(LowUnit) && LowUnit >= 0xDC00 && LowUnit <= 0xDFFF)
							{
								AppendCodePoint(OutValue, 0x10000 + ((CodeUnit - 0xD800) << 10) + (LowUnit - 0xDC00));
								break;
							}
							Index = Saved;
						}

						AppendCodePoint(OutValue, CodeUnit);
						break;
					}
					default:
						return Fail(TEXT("unknown string escape"));
					}
				}
			}

			bool ParseNumber(double& OutValue)
			{
				const int32 Start = Index;

				if (Peek(TEXT('-')))
				{
					++Index;
				}

				const int32 IntegerStart = Index;
				while (Index < Text.Len() && Text[Index] >= TEXT('0') && Text[Index] <= TEXT('9'))
				{
					++Index;
				}
				if (Index == IntegerStart)
				{
					return Fail(TEXT("expected a digit in a number"));
				}

				if (Peek(TEXT('.')))
				{
					++Index;
					const int32 FractionStart = Index;
					while (Index < Text.Len() && Text[Index] >= TEXT('0') && Text[Index] <= TEXT('9'))
					{
						++Index;
					}
					if (Index == FractionStart)
					{
						return Fail(TEXT("expected a digit after '.'"));
					}
				}

				if (Peek(TEXT('e')) || Peek(TEXT('E')))
				{
					++Index;
					if (Peek(TEXT('+')) || Peek(TEXT('-')))
					{
						++Index;
					}
					const int32 ExponentStart = Index;
					while (Index < Text.Len() && Text[Index] >= TEXT('0') && Text[Index] <= TEXT('9'))
					{
						++Index;
					}
					if (Index == ExponentStart)
					{
						return Fail(TEXT("expected a digit in an exponent"));
					}
				}

				OutValue = FCString::Atod(*Text.Mid(Start, Index - Start));
				return true;
			}

			const FString& Text;
			int32 Index = 0;
			FString Error;
		};
	}

	bool ParseIRJson(const FString& Text, FIRJsonValue& OutValue, FString& OutError)
	{
		OutValue = FIRJsonValue();
		OutError.Reset();

		FIRJsonReader Reader(Text);
		if (Reader.ParseDocument(OutValue))
		{
			return true;
		}

		OutError = Reader.GetError();
		if (OutError.IsEmpty())
		{
			OutError = TEXT("1:1: the text is not JSON"); /* I18N-EXEMPT: parser OutError text, never displayed to an end user */
		}
		OutValue = FIRJsonValue();
		return false;
	}
}
