// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

namespace UE::DreamShader
{
	/**
	 * Where a preprocessor define came from. The enumerator order IS the precedence order: a later
	 * source overwrites an earlier one during ResolveDreamShaderDefines(), so anything added here
	 * must be inserted at the tier it actually outranks, not appended.
	 *
	 * Builtin is the exception that never loses, because it is the one tier that describes objective
	 * facts about the compiling process rather than an intent. A source that lies about the engine
	 * version or about Substrate does not produce a different material -- it produces a graph the
	 * engine cannot compile at all, and the failure surfaces far from the override that caused it.
	 */
	enum class EDreamShaderDefineSource : uint8
	{
		/** DS_-prefixed environment facts. Read-only: every other tier is refused, not merged. */
		Builtin,
		/** UDreamShaderSettings::PreprocessorDefines -- the checked-in, artist-editable tier. */
		Settings,
		/** RegisterDreamShaderDefine() -- another module's compiled-in switch. */
		Registered,
		/** A define provider delegate, pulled fresh at resolve time. Outranks direct registration. */
		Provider,
		/** `dsc -Define=NAME=VALUE`. Most explicit, so it wins -- except over Builtin. */
		CommandLine,

		/**
		 * A `#define` in the source file being preprocessed. OUT OF BAND: it takes no part in the
		 * precedence ordering above, because it never reaches a resolved table -- the preprocessor
		 * writes it into a private copy it discards on return, where it simply overwrites whatever
		 * the injected tiers said for the rest of that one file.
		 *
		 * Listed last so it cannot be mistaken for a rank. It exists so the preprocessor does not
		 * have to label its own definitions with some other tier's name; a `#define` of a reserved
		 * name is still refused by Set(), which is what raises DSH1039.
		 */
		SourceFile,
	};

	/** One resolved define. An empty Value is a bare marker: defined() is true, arithmetic reads 1. */
	struct FDreamShaderDefineEntry
	{
		FString Value;
		EDreamShaderDefineSource Source = EDreamShaderDefineSource::Registered;

		/** Who contributed it -- a module or plugin name. Used by unregistration and diagnostics. */
		FString SourceTag;
	};

	/**
	 * Case-sensitive key matching for the define table, pinned here instead of inherited.
	 *
	 * Define names are case-sensitive because C and HLSL are, and aligning with HLSL is the whole
	 * spelling decision behind this feature. What a default TMap<FString, ...> does about case is not
	 * something that promise may rest on. FString carries no in-class operator==, its Equals default
	 * has already moved to CaseSensitive in this engine tree, and GetTypeHash(FString) is still the
	 * case-INSENSITIVE Strihash -- so the container's answer is an engine-version detail, and a
	 * plugin built against several engine versions would silently change its LANGUAGE SEMANTICS with
	 * the engine it happened to compile against. A language spec does not get to drift like that.
	 *
	 * FName was never the alternative: it folds case at the identity level, so `Foo` and `FOO` there
	 * are the same key with no way to separate them again.
	 *
	 * Keeping the inherited case-insensitive HASH is deliberate. Equal keys still hash equal -- the
	 * only invariant a hash owes its container -- and `Foo`/`FOO` merely share a bucket, where
	 * Matches then separates them. The cost is one collision between two names that essentially never
	 * coexist; the alternative would be reimplementing a hash to no benefit.
	 */
	template <typename ValueType>
	struct TDreamShaderDefineNameKeyFuncs
		: TDefaultMapKeyFuncs<FString, ValueType, /*bInAllowDuplicateKeys*/ false>
	{
		using Super = TDefaultMapKeyFuncs<FString, ValueType, /*bInAllowDuplicateKeys*/ false>;
		using KeyInitType = typename Super::KeyInitType;

		[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}

		/**
		 * Overridden alongside the non-template form so a heterogeneous lookup cannot quietly fall
		 * back to the inherited `A == B`. Nothing uses one today; this is here so that adding one
		 * later cannot reintroduce case folding through a path no test covers.
		 */
		template <typename ComparableKey>
		[[nodiscard]] static FORCEINLINE bool Matches(KeyInitType A, ComparableKey B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
	};

	/** The resolved define set. */
	using FDreamShaderDefineMap = TMap<
		FString, FDreamShaderDefineEntry, FDefaultSetAllocator,
		TDreamShaderDefineNameKeyFuncs<FDreamShaderDefineEntry>>;

	/**
	 * A plain name -> value map that is case-sensitive on the same terms.
	 *
	 * Used for the preprocessor's touched-define set and for the command-line define set. Both hold
	 * define NAMES, so both are subject to the language's case rule; leaving either as a default TMap
	 * would let `Foo` and `FOO` merge into one entry. For the touched set that is not merely untidy,
	 * it is a build-key soundness hole: two distinct names collapse to one, only one value reaches
	 * the hash, and changing the other rebuilds nothing.
	 */
	using FDreamShaderDefineValueMap = TMap<
		FString, FString, FDefaultSetAllocator,
		TDreamShaderDefineNameKeyFuncs<FString>>;

	/**
	 * The define set a single compile sees.
	 *
	 * Names are CASE-SENSITIVE; see FDreamShaderDefineNameKeyFuncs for why that is pinned rather than
	 * inherited from the container.
	 */
	class DREAMSHADER_API FDreamShaderDefineTable
	{
	public:
		const FDreamShaderDefineEntry* Find(const FString& Name) const { return Entries.Find(Name); }
		bool IsDefined(const FString& Name) const { return Entries.Contains(Name); }

		/**
		 * Adds or overwrites. Returns false and leaves the table untouched when Name is reserved
		 * (see IsReservedDreamShaderDefineName) and Source is not Builtin -- the one refusal this
		 * class makes on its own, so no caller can bypass the read-only rule by holding a table.
		 */
		bool Set(const FString& Name, const FString& Value, EDreamShaderDefineSource Source, const FString& SourceTag);

		void Remove(const FString& Name) { Entries.Remove(Name); }
		void Reset() { Entries.Reset(); }
		int32 Num() const { return Entries.Num(); }

		const FDreamShaderDefineMap& GetEntries() const { return Entries; }

		/**
		 * Every name, ascending, compared CASE-SENSITIVELY.
		 *
		 * Sorted because TMap iteration order is not stable, and an unsorted fold into the build key
		 * would hash differently run to run -- which reads as "every asset is stale, every time".
		 * Case-sensitively because the default FString comparison operators are not, and TArray::Sort
		 * is unstable: `Foo`, `FOO` and `foo` would otherwise come back in insertion order, putting
		 * that same instability back into the key by another door.
		 */
		TArray<FString> GetSortedNames() const;

	private:
		FDreamShaderDefineMap Entries;
	};

	/**
	 * True for names DreamShader owns: the `DS_` prefix. Reserved is a prefix rule rather than a
	 * fixed list so that adding a builtin later cannot silently start losing to a define some
	 * project already registered under that name.
	 */
	DREAMSHADER_API bool IsReservedDreamShaderDefineName(const FString& Name);

	/** True for a syntactically valid define name: [A-Za-z_][A-Za-z0-9_]*. */
	DREAMSHADER_API bool IsValidDreamShaderDefineName(const FString& Name);

	/**
	 * The environment facts, ADDED to OutTable rather than replacing its contents -- a Builtin write
	 * is never refused, so merging is the same as seeding an empty table and also lets the resolver
	 * re-assert the builtins over anything a later tier put in their slots.
	 *
	 * Recomputed on each call.
	 *
	 * HARD RULE for anything added here: it must be invariant for the lifetime of the process.
	 * A define is evaluated once, at generation time, and its effect is then baked into a saved
	 * asset; a value that can change mid-session makes the build unreproducible and the asset's
	 * build key a lie. `r.Substrate` qualifies only because it is a read-only CVar.
	 */
	DREAMSHADER_API void GetBuiltinDreamShaderDefines(FDreamShaderDefineTable& OutTable);

	// -----------------------------------------------------------------------------------------------
	// Registry.
	//
	// Free functions in this namespace rather than an IDreamShaderModule interface, matching how the
	// rest of the plugin's cross-module surface is already shaped (GetSourceShaderRoots and family).
	// FDreamShaderModule is a concrete class with no interface to extend.
	// -----------------------------------------------------------------------------------------------

	/**
	 * Contributes a define from C++. Returns false (and logs an error) for an invalid or reserved
	 * name; the table is unchanged in that case.
	 *
	 * SourceTag identifies the contributor so UnregisterDreamShaderDefinesFrom can withdraw the whole
	 * set when a plugin shuts down. Registering the same name twice from the same tag overwrites.
	 *
	 * Callers must register before the first compile. A module whose value depends on state that is
	 * not ready at StartupModule time should register a provider instead -- resolution order stops
	 * mattering there.
	 */
	DREAMSHADER_API bool RegisterDreamShaderDefine(const FString& Name, const FString& Value, const FString& SourceTag);

	/** Withdraws every define a given contributor registered. Safe for an unknown tag. */
	DREAMSHADER_API void UnregisterDreamShaderDefinesFrom(const FString& SourceTag);

	/**
	 * Pulled during ResolveDreamShaderDefines(), after direct registrations and before the command
	 * line. Write into the table with Set(..., EDreamShaderDefineSource::Provider, YourTag).
	 */
	DECLARE_DELEGATE_OneParam(FDreamShaderDefineProviderDelegate, FDreamShaderDefineTable& /*InOutTable*/);

	DREAMSHADER_API FDelegateHandle RegisterDreamShaderDefineProvider(FDreamShaderDefineProviderDelegate Provider);
	DREAMSHADER_API void UnregisterDreamShaderDefineProvider(FDelegateHandle Handle);

	/** Set once by the commandlet from `-Define=NAME=VALUE`. Replaces any previous command-line set. */
	DREAMSHADER_API void SetDreamShaderCommandLineDefines(const FDreamShaderDefineValueMap& Defines);

	/**
	 * Builds the table one compile will see: Builtin, then Settings, Registered, Provider and
	 * CommandLine in that order, each overwriting the last. Reserved names offered by a non-builtin
	 * tier are dropped with a warning rather than failing the compile -- the offer is a configuration
	 * mistake, not a source error, and it has no file or line to point at.
	 */
	DREAMSHADER_API FDreamShaderDefineTable ResolveDreamShaderDefines();

	/**
	 * Bumped whenever any tier changes (register, unregister, provider add/remove, command-line set,
	 * settings edit). Anything holding compiled output keyed by the define set -- the ThinCustom
	 * in-memory materials, above all -- compares this and invalidates when it moves.
	 */
	DREAMSHADER_API uint32 GetDreamShaderDefineRevision();

	/** Called by the settings object's PostEditChangeProperty. Bumps the revision. */
	DREAMSHADER_API void NotifyDreamShaderDefineSettingsChanged();
}
