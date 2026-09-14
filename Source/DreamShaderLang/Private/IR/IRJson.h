// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A small JSON reader and writer for the IR module.
//
// Why hand-written: DreamShaderLang depends on Core and on nothing else (CONTRACT §0.9), and the
// `Json` module is not Core. Two things in this module are JSON on the wire -- the builtin catalog
// manifest (IRCatalog.cpp) and the IR dump (IRDump.cpp) -- and both are read or diffed by tools and
// by the corpus tests, so the output has to be byte-stable, not merely valid.
//
// What the writer guarantees:
//   * two-space indentation, one member per line, `": "` between key and value, no trailing commas;
//   * a trailing newline after the root value;
//   * numbers rendered by FormatIRNumber: integral values as integers, everything else `%.9g`,
//     with -0.0, NaN and the infinities normalised so a dump never differs between two runs;
//   * strings escaped minimally and deterministically (`"` `\` and the C0 range, nothing else), so
//     re-reading and re-writing reproduces the same bytes.
//
// The reader accepts standard JSON (RFC 8259) and nothing more: no comments, no trailing commas,
// no single quotes. Object members keep their file order, which is what makes round-tripping a
// canonical file textually stable.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::IR::Private
{
	enum class EIRJsonKind : uint8
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	struct FIRJsonValue
	{
		EIRJsonKind Kind = EIRJsonKind::Null;
		bool bBool = false;
		double Number = 0.0;
		FString String;
		/** Kind == Array. */
		TArray<FIRJsonValue> Items;
		/** Kind == Object, in file order. */
		TArray<TPair<FString, FIRJsonValue>> Members;

		bool IsNull() const { return Kind == EIRJsonKind::Null; }
		bool IsBool() const { return Kind == EIRJsonKind::Bool; }
		bool IsNumber() const { return Kind == EIRJsonKind::Number; }
		bool IsString() const { return Kind == EIRJsonKind::String; }
		bool IsArray() const { return Kind == EIRJsonKind::Array; }
		bool IsObject() const { return Kind == EIRJsonKind::Object; }

		/** Object member by name, case-sensitive. Null when this is not an object or has no such member. */
		const FIRJsonValue* Find(const TCHAR* Name) const;
		/** Member that is an array / an object, or null. */
		const FIRJsonValue* FindArray(const TCHAR* Name) const;
		const FIRJsonValue* FindObject(const TCHAR* Name) const;

		FString GetString(const TCHAR* Name, const FString& Fallback = FString()) const;
		bool GetBool(const TCHAR* Name, bool bFallback = false) const;
		double GetNumber(const TCHAR* Name, double Fallback = 0.0) const;
		int64 GetInt(const TCHAR* Name, int64 Fallback = 0) const;

		/** Every element of a string array member, in order; empty when the member is absent. */
		void GetStringArray(const TCHAR* Name, TArray<FString>& OutValues) const;
	};

	/** Parses one JSON document. On failure OutError is `line:column: what`, never empty. */
	bool ParseIRJson(const FString& Text, FIRJsonValue& OutValue, FString& OutError);

	/** Culture-invariant, run-stable rendering of a double. See the header comment. */
	FString FormatIRNumber(double Value);

	/** Appends Value to Out with the JSON string escapes applied; does NOT add the quotes. */
	void AppendIRJsonEscaped(FString& Out, const FString& Value);

	/**
	 * Streaming writer. The caller drives the structure; the writer owns the whitespace so every
	 * emitter in this module produces the same layout.
	 */
	class FIRJsonWriter
	{
	public:
		FIRJsonWriter() = default;

		void BeginObject();
		void EndObject();
		void BeginArray();
		void EndArray();

		/** `"Name": ` -- the next value written lands on the same line. */
		void Key(const TCHAR* Name);

		void ValueString(const FString& Value);
		void ValueNumber(double Value);
		void ValueInt(int64 Value);
		void ValueBool(bool bValue);
		void ValueNull();

		void KeyString(const TCHAR* Name, const FString& Value);
		void KeyNumber(const TCHAR* Name, double Value);
		void KeyInt(const TCHAR* Name, int64 Value);
		void KeyBool(const TCHAR* Name, bool bValue);
		void KeyStringArray(const TCHAR* Name, const TArray<FString>& Values);

		/** The document with its trailing newline. Call once, at the end. */
		FString Release();

	private:
		struct FLevel
		{
			bool bAnyItem = false;
		};

		void AppendIndent();
		void WriteMemberPrefix();
		void WriteValue(const TCHAR* Text);
		void WriteValue(const FString& Text);

		FString Out;
		TArray<FLevel> Levels;
		bool bAfterKey = false;
	};
}
