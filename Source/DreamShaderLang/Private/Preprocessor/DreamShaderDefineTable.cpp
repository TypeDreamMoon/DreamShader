// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The PURE half of the define system: the table container, the name rules and nothing else. It is
// built by hand in tests and by ResolveDreamShaderDefines() in the DreamShader module -- see
// DreamShaderDefineResolution.h/.cpp, which owns the builtins, the C++ registry, the provider
// delegates, the command-line tier and the revision counter, because every one of those reads
// something this module is not allowed to see.

#include "DreamShaderDefineTable.h"

namespace UE::DreamShader
{
	/**
	 * File-local helpers, nested one level deeper than the plugin's usual `Private`.
	 *
	 * Unreal compiles this module in unity blobs, which paste several .cpp files into ONE translation
	 * unit -- so a helper at `UE::DreamShader::Private` scope collides with an identically named one
	 * in any sibling file that lands in the same blob, and `static` or an anonymous namespace does
	 * not help because it is all still a single TU. DreamShaderPreprocessor.cpp sits in this very
	 * directory and works on the same domain, so the collision is a question of when, not whether.
	 */
	namespace Private::DefineTableImpl
	{
		// -------------------------------------------------------------------------------------------
		// Name characters.
		//
		// Deliberately NOT FChar::IsAlpha / FChar::IsAlnum: those are Unicode-aware and would happily
		// accept `Café` or a CJK identifier. The grammar in Plan/preprocessor-conditionals.md is the
		// ASCII one C and HLSL use, and everything downstream assumes it -- the preprocessor's own
		// tokenizer, the generated HLSL symbol names, and the VS Code extension's lexer. Accepting a
		// name here that one of those rejects later is the worst outcome: the define resolves, the
		// source compiles, and the failure lands somewhere with no obvious link back to the name.
		// -------------------------------------------------------------------------------------------

		static FORCEINLINE bool IsNameStartChar(const TCHAR Char)
		{
			return (Char >= TEXT('A') && Char <= TEXT('Z'))
				|| (Char >= TEXT('a') && Char <= TEXT('z'))
				|| Char == TEXT('_');
		}

		static FORCEINLINE bool IsNameBodyChar(const TCHAR Char)
		{
			return IsNameStartChar(Char) || (Char >= TEXT('0') && Char <= TEXT('9'));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// FDreamShaderDefineTable
	// ---------------------------------------------------------------------------------------------

	bool FDreamShaderDefineTable::Set(
		const FString& Name,
		const FString& Value,
		const EDreamShaderDefineSource Source,
		const FString& SourceTag)
	{
		// The single choke point for the read-only rule. Every tier -- settings, C++ registration, a
		// provider delegate holding this table by reference, the command line -- has to come through
		// here to change anything, so putting the refusal in the container instead of in each
		// ingestion path is what makes "builtins cannot be overridden" actually hold rather than
		// merely being everyone's intention.
		if (Source != EDreamShaderDefineSource::Builtin && IsReservedDreamShaderDefineName(Name))
		{
			return false;
		}

		// Note the asymmetry with OfferToTable: syntactic validity is NOT checked here. Each ingestion
		// path validates, because each one has a different thing to say about a bad name (a log line,
		// a false return, a DSH1038 with a file and a line). The container's one job is the rule that
		// must never be bypassable.
		FDreamShaderDefineEntry& Entry = Entries.FindOrAdd(Name);
		Entry.Value = Value;
		Entry.Source = Source;
		Entry.SourceTag = SourceTag;
		return true;
	}

	TArray<FString> FDreamShaderDefineTable::GetSortedNames() const
	{
		TArray<FString> Names;
		Names.Reserve(Entries.Num());
		for (const TPair<FString, FDreamShaderDefineEntry>& Pair : Entries)
		{
			Names.Add(Pair.Key);
		}

		// Explicitly case-sensitive. TArray::Sort's default predicate is FString::operator<, which is
		// Stricmp-based: under it two names differing only in case have no defined relative order and
		// the sort result can vary run to run. This array feeds the build key, where an unstable order
		// reads as "every asset is stale, every time".
		Names.Sort([](const FString& A, const FString& B)
		{
			return A.Compare(B, ESearchCase::CaseSensitive) < 0;
		});

		return Names;
	}

	// ---------------------------------------------------------------------------------------------
	// Name rules
	// ---------------------------------------------------------------------------------------------

	bool IsReservedDreamShaderDefineName(const FString& Name)
	{
		// ESearchCase::CaseSensitive is not optional here: FString::StartsWith defaults to IgnoreCase,
		// which would also reserve `ds_`, `Ds_` and every other spelling. Define names are
		// case-sensitive per the header's contract, so the reservation has to be too.
		return Name.StartsWith(GDreamShaderReservedDefinePrefix, ESearchCase::CaseSensitive);
	}

	bool IsValidDreamShaderDefineName(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return false;
		}

		if (!Private::DefineTableImpl::IsNameStartChar(Name[0]))
		{
			return false;
		}

		for (int32 Index = 1; Index < Name.Len(); ++Index)
		{
			if (!Private::DefineTableImpl::IsNameBodyChar(Name[Index]))
			{
				return false;
			}
		}

		return true;
	}
}
