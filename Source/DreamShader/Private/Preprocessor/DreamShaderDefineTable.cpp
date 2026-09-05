// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderDefineTable.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ScopeLock.h"

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
		/**
		 * Contributor tags for the tiers DreamShader itself feeds. A tier the plugin owns still gets a
		 * tag, because every diagnostic about a define says where it came from and "" reads as a bug.
		 */
		static const TCHAR* const BuiltinSourceTag = TEXT("DreamShader");
		static const TCHAR* const SettingsSourceTag = TEXT("ProjectSettings");
		static const TCHAR* const CommandLineSourceTag = TEXT("CommandLine");

		/** The reserved prefix. Spelled once so the check and the diagnostics cannot drift apart. */
		static const TCHAR* const ReservedNamePrefix = TEXT("DS_");

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

		/** Human-readable "who offered this", for the one-line warnings the resolve path emits. */
		static FString DescribeSource(const EDreamShaderDefineSource Source, const FString& SourceTag)
		{
			switch (Source)
			{
			case EDreamShaderDefineSource::Builtin:
				return TEXT("a builtin");
			case EDreamShaderDefineSource::Settings:
				return TEXT("Project Settings > DreamShader > Preprocessor Defines");
			case EDreamShaderDefineSource::Registered:
				return FString::Printf(TEXT("RegisterDreamShaderDefine from '%s'"), *SourceTag);
			case EDreamShaderDefineSource::Provider:
				return FString::Printf(TEXT("a define provider ('%s')"), *SourceTag);
			case EDreamShaderDefineSource::CommandLine:
				return TEXT("the command line (-Define=)");
			default:
				return TEXT("an unknown source");
			}
		}

		/**
		 * Byte-exact entry comparison, used to decide whether a registry write is a real change.
		 *
		 * FString::operator== is CASE-INSENSITIVE in Unreal (it forwards to Equals with
		 * ESearchCase::IgnoreCase), so the obvious `A.Value == B.Value` would call a change from `on`
		 * to `ON` a no-op and skip the revision bump -- leaving every cache keyed on the revision
		 * holding output built from the old value. Compare case-sensitively, always.
		 */
		static bool AreEntriesIdentical(const FDreamShaderDefineEntry& A, const FDreamShaderDefineEntry& B)
		{
			return A.Source == B.Source
				&& A.Value.Equals(B.Value, ESearchCase::CaseSensitive)
				&& A.SourceTag.Equals(B.SourceTag, ESearchCase::CaseSensitive);
		}

		/** Same reasoning as AreEntriesIdentical, for the flat command-line map. */
		static bool AreMapsIdentical(const UE::DreamShader::FDreamShaderDefineValueMap& A, const UE::DreamShader::FDreamShaderDefineValueMap& B)
		{
			if (A.Num() != B.Num())
			{
				return false;
			}

			for (const TPair<FString, FString>& Pair : A)
			{
				const FString* Other = B.Find(Pair.Key);
				if (!Other || !Other->Equals(Pair.Value, ESearchCase::CaseSensitive))
				{
					return false;
				}
			}

			return true;
		}

		/** One registered provider. A struct rather than a TPair so the two fields read at call sites. */
		struct FProviderRecord
		{
			FDelegateHandle Handle;
			FDreamShaderDefineProviderDelegate Delegate;
		};

		/**
		 * The process-wide define registry.
		 *
		 * THREAD SAFETY: guarded by a critical section rather than asserted onto the game thread.
		 *
		 * A compile is not a game-thread-only event. Source generation is driven from the editor
		 * bridge's file watcher and from commandlets, and the asset-compilation machinery around it
		 * moves work onto worker threads; a check(IsInGameThread()) here would convert a read that is
		 * harmless today into a crash the first time any of that is reparented. The cost of being
		 * wrong in the other direction is nothing: the registry is written a handful of times per
		 * session (module startup, a settings edit, one commandlet call) and read once per compiled
		 * file, so an uncontended FScopeLock is invisible next to building a material graph.
		 *
		 * Provider delegates are invoked OUTSIDE this lock -- see ResolveDreamShaderDefines.
		 *
		 * Two pieces of state deliberately live outside the lock because they are not ours to guard:
		 * UDreamShaderSettings (read through its CDO, treated as read-mostly config) and the engine
		 * singletons GetBuiltinDreamShaderDefines consults (IPluginManager, IConsoleManager). Both are
		 * effectively immutable once startup is done, which is the same assumption the rest of the
		 * plugin already makes about them.
		 */
		struct FRegistry
		{
			FCriticalSection Mutex;

			/** The Registered tier. Same name from a different tag is last-writer-wins, by design. */
			TMap<FString, FDreamShaderDefineEntry> Registered;

			TArray<FProviderRecord> Providers;

			/** The whole `-Define=` set, replaced wholesale by SetDreamShaderCommandLineDefines. */
			UE::DreamShader::FDreamShaderDefineValueMap CommandLine;

			/**
			 * Starts at 1, not 0, so a cache can spell "never resolved anything yet" as a stored 0 and
			 * not have to carry a separate bool. Wrapping after 2^32 edits is not a scenario.
			 */
			uint32 Revision = 1;
		};

		static FRegistry& GetRegistry()
		{
			// Function-local static: C++11 guarantees the initialization itself is race-free, which
			// matters because the first toucher may well not be the game thread (see above).
			static FRegistry Registry;
			return Registry;
		}

		/**
		 * Offers one name/value pair from a non-builtin tier into the table being resolved, logging
		 * and dropping anything the tier is not allowed to say.
		 *
		 * Dropping rather than failing is the decision from the design doc: a bad define in the
		 * settings table or on the command line is a CONFIGURATION mistake. It has no file and no line
		 * number to point at, so turning it into a compile error would produce a diagnostic nobody can
		 * act on, and would take down every material in the project at once.
		 */
		static void OfferToTable(
			FDreamShaderDefineTable& InOutTable,
			const FString& Name,
			const FString& Value,
			const EDreamShaderDefineSource Source,
			const FString& SourceTag)
		{
			if (!IsValidDreamShaderDefineName(Name))
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader preprocessor define '%s' from %s is not a valid identifier ")
					TEXT("([A-Za-z_][A-Za-z0-9_]*) and was ignored."),
					*Name, *DescribeSource(Source, SourceTag));
				return;
			}

			// The reserved refusal lives in FDreamShaderDefineTable::Set and nowhere else, so this
			// path cannot drift out of agreement with a caller that holds a table directly.
			if (!InOutTable.Set(Name, Value, Source, SourceTag))
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader preprocessor define '%s' from %s uses the reserved '%s' prefix ")
					TEXT("and was ignored. That prefix is owned by the plugin's builtin environment ")
					TEXT("facts, which are read-only."),
					*Name, *DescribeSource(Source, SourceTag), ReservedNamePrefix);
			}
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
		return Name.StartsWith(Private::DefineTableImpl::ReservedNamePrefix, ESearchCase::CaseSensitive);
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

	// ---------------------------------------------------------------------------------------------
	// Builtins
	// ---------------------------------------------------------------------------------------------

	void GetBuiltinDreamShaderDefines(FDreamShaderDefineTable& OutTable)
	{
		// Adds to OutTable rather than resetting it. Builtins always win, and Set() with the Builtin
		// source can never be refused, so layering them onto a populated table is the same thing as
		// seeding an empty one -- and it lets ResolveDreamShaderDefines re-assert them at the end
		// without having to rebuild anything.
		const FString SourceTag(Private::DefineTableImpl::BuiltinSourceTag);

		// Engine version. Read from the DREAMSHADER_UE_* macros rather than ENGINE_*_VERSION so that a
		// fork overriding them for compat testing is reflected in what the sources see, and so that
		// `#if DS_ENGINE_MINOR >= 7` agrees with the `#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)` that
		// guards the C++ it is usually paired with.
		OutTable.Set(TEXT("DS_ENGINE_MAJOR"), FString::FromInt(DREAMSHADER_UE_MAJOR), EDreamShaderDefineSource::Builtin, SourceTag);
		OutTable.Set(TEXT("DS_ENGINE_MINOR"), FString::FromInt(DREAMSHADER_UE_MINOR), EDreamShaderDefineSource::Builtin, SourceTag);
		OutTable.Set(TEXT("DS_ENGINE_PATCH"), FString::FromInt(DREAMSHADER_UE_PATCH), EDreamShaderDefineSource::Builtin, SourceTag);

		// Platform. IniPlatformName is the stable, configuration-independent spelling ("Windows",
		// "Mac", "Android"); PlatformName is not -- it folds in editor/server/client and would report
		// "WindowsEditor" in the editor against "Windows" in a cooked build, so the same source would
		// preprocess differently on the two sides of a cook. It returns a narrow string.
		OutTable.Set(
			TEXT("DS_PLATFORM"),
			FString(ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())),
			EDreamShaderDefineSource::Builtin,
			SourceTag);

		// Plugin version, from the descriptor. FindPlugin can legitimately return null -- during very
		// early startup, or in a target that links the module without mounting the plugin -- and the
		// name stays defined in that case, with a value no real version string can be confused for.
		// Keeping the name always present matters: the build key folds the set of touched names, and a
		// builtin that intermittently disappears would make `defined(DS_PLUGIN_VERSION)` flip for
		// reasons that have nothing to do with the source. The spelling matches the editor module's
		// GetDreamShaderPluginVersion, which is not reachable from here (wrong module).
		FString PluginVersion = TEXT("unknown");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
		{
			PluginVersion = Plugin->GetDescriptor().VersionName;
		}
		OutTable.Set(TEXT("DS_PLUGIN_VERSION"), PluginVersion, EDreamShaderDefineSource::Builtin, SourceTag);

		// Substrate, read through IConsoleManager and NOT through Substrate::IsSubstrateEnabled().
		//
		// This change cannot be compile-verified right now (the engine it builds against is being
		// modified), so the choice is made on link risk rather than on taste. Substrate::* has only
		// ever been called from the plugin's EDITOR module; whether the symbol resolves from this
		// runtime module -- and on every engine version the plugin supports -- is unproven, and an
		// unproven link error is exactly the failure that surfaces as "the whole plugin stopped
		// building". IConsoleManager is Core, already linked by every module here, and cannot fail to
		// resolve.
		//
		// Reading the CVar is also legitimate under the hard rule above: r.Substrate is declared
		// ECVF_ReadOnly (RenderCore, RenderUtils.cpp), so its value is fixed once configs are loaded
		// and cannot drift mid-session the way an ordinary CVar would. A missing CVar -- an engine too
		// old to have Substrate at all -- reads as 0, which is the correct answer for it.
		int32 SubstrateEnabled = 0;
		if (const IConsoleVariable* SubstrateVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Substrate")))
		{
			SubstrateEnabled = (SubstrateVar->GetInt() != 0) ? 1 : 0;
		}
		OutTable.Set(TEXT("DS_SUBSTRATE"), FString::FromInt(SubstrateEnabled), EDreamShaderDefineSource::Builtin, SourceTag);
	}

	// ---------------------------------------------------------------------------------------------
	// Registry
	// ---------------------------------------------------------------------------------------------

	bool RegisterDreamShaderDefine(const FString& Name, const FString& Value, const FString& SourceTag)
	{
		if (!IsValidDreamShaderDefineName(Name))
		{
			UE_LOG(LogDreamShader, Error,
				TEXT("DreamShader define '%s' registered by '%s' is not a valid identifier ")
				TEXT("([A-Za-z_][A-Za-z0-9_]*); the registration was rejected."),
				*Name, *SourceTag);
			return false;
		}

		if (IsReservedDreamShaderDefineName(Name))
		{
			UE_LOG(LogDreamShader, Error,
				TEXT("DreamShader define '%s' registered by '%s' uses the reserved '%s' prefix; the ")
				TEXT("registration was rejected. Builtin environment defines are read-only -- pick a ")
				TEXT("name outside that prefix."),
				*Name, *SourceTag, Private::DefineTableImpl::ReservedNamePrefix);
			return false;
		}

		// Both refusals return before anything is touched: the header promises the table is unchanged
		// on failure, so a caller retrying with a corrected name cannot find a half-applied first try.
		FDreamShaderDefineEntry NewEntry;
		NewEntry.Value = Value;
		NewEntry.Source = EDreamShaderDefineSource::Registered;
		NewEntry.SourceTag = SourceTag;

		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);

		if (const FDreamShaderDefineEntry* Existing = Registry.Registered.Find(Name))
		{
			// Re-registering an identical entry is a no-op, and must not bump the revision: a
			// StartupModule path gets run again on a hot reload, and a spurious bump throws away every
			// ThinCustom in-memory material for nothing.
			if (Private::DefineTableImpl::AreEntriesIdentical(*Existing, NewEntry))
			{
				return true;
			}
		}

		Registry.Registered.Add(Name, MoveTemp(NewEntry));
		++Registry.Revision;
		return true;
	}

	void UnregisterDreamShaderDefinesFrom(const FString& SourceTag)
	{
		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);

		int32 RemovedCount = 0;
		for (auto It = Registry.Registered.CreateIterator(); It; ++It)
		{
			// Case-sensitive: a tag is an identifier the contributor chose, and matching `MoonToon`
			// against `moontoon` would let one plugin withdraw another's defines by accident.
			if (It.Value().SourceTag.Equals(SourceTag, ESearchCase::CaseSensitive))
			{
				It.RemoveCurrent();
				++RemovedCount;
			}
		}

		// An unknown tag is explicitly safe (the header says so) -- it just removes nothing, and with
		// nothing removed there is no change for the revision to report.
		if (RemovedCount > 0)
		{
			++Registry.Revision;
		}
	}

	FDelegateHandle RegisterDreamShaderDefineProvider(FDreamShaderDefineProviderDelegate Provider)
	{
		if (!Provider.IsBound())
		{
			UE_LOG(LogDreamShader, Error,
				TEXT("DreamShader ignored an unbound define provider. Bind the delegate before ")
				TEXT("registering it; an unbound one would silently contribute nothing."));
			return FDelegateHandle();
		}

		// A freshly generated handle rather than the delegate's own: two providers bound to the same
		// method of the same object share a delegate handle, and unregistering one would then remove
		// the other. Generated handles are unique per registration by construction.
		Private::DefineTableImpl::FProviderRecord Record;
		Record.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
		Record.Delegate = MoveTemp(Provider);

		// Copied out before the move below leaves Record in a moved-from state.
		const FDelegateHandle Handle = Record.Handle;

		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);
		Registry.Providers.Add(MoveTemp(Record));
		++Registry.Revision;

		return Handle;
	}

	void UnregisterDreamShaderDefineProvider(FDelegateHandle Handle)
	{
		if (!Handle.IsValid())
		{
			return;
		}

		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);

		const int32 RemovedCount = Registry.Providers.RemoveAll(
			[Handle](const Private::DefineTableImpl::FProviderRecord& Record)
			{
				return Record.Handle == Handle;
			});

		if (RemovedCount > 0)
		{
			++Registry.Revision;
		}
	}

	void SetDreamShaderCommandLineDefines(const UE::DreamShader::FDreamShaderDefineValueMap& Defines)
	{
		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);

		// Names are not validated here. The command line is one of the two tiers whose bad entries are
		// dropped with a warning at resolve time (the settings table is the other), and doing it there
		// rather than here means the warning is emitted once per compile, next to the compile it
		// actually affected, instead of once at startup where nobody is looking.
		if (Private::DefineTableImpl::AreMapsIdentical(Registry.CommandLine, Defines))
		{
			return;
		}

		Registry.CommandLine = Defines;
		++Registry.Revision;
	}

	FDreamShaderDefineTable ResolveDreamShaderDefines()
	{
		using namespace Private::DefineTableImpl;

		FDreamShaderDefineTable Table;

		// --- 1. Builtin -------------------------------------------------------------------------
		// Kept as a separate table as well as merged, so step 7 can compare against the truth without
		// recomputing it -- and so that a value which somehow moved mid-resolve would be caught rather
		// than silently adopted.
		FDreamShaderDefineTable Builtins;
		GetBuiltinDreamShaderDefines(Builtins);
		for (const TPair<FString, FDreamShaderDefineEntry>& Pair : Builtins.GetEntries())
		{
			Table.Set(Pair.Key, Pair.Value.Value, EDreamShaderDefineSource::Builtin, Pair.Value.SourceTag);
		}

		// --- 2. Project settings ----------------------------------------------------------------
		if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>())
		{
			for (const TPair<FString, FString>& Pair : Settings->PreprocessorDefines)
			{
				OfferToTable(
					Table, Pair.Key, Pair.Value,
					EDreamShaderDefineSource::Settings, FString(SettingsSourceTag));
			}
		}

		// --- 3. Snapshot the registry -----------------------------------------------------------
		// Everything is copied out under one short lock and applied afterwards. Two reasons, and the
		// second is the load-bearing one:
		//   * the merge logs, and logging inside a lock we own widens the window for a lock-order
		//     surprise with the log sink for no benefit;
		//   * a provider is arbitrary third-party code, and may perfectly reasonably call
		//     RegisterDreamShaderDefine from inside itself. Invoking it while holding the mutex would
		//     deadlock on any non-recursive implementation, and FCriticalSection's recursiveness is a
		//     platform detail, not a contract.
		TMap<FString, FDreamShaderDefineEntry> RegisteredSnapshot;
		TArray<FProviderRecord> ProviderSnapshot;
		UE::DreamShader::FDreamShaderDefineValueMap CommandLineSnapshot;
		{
			FRegistry& Registry = GetRegistry();
			FScopeLock Lock(&Registry.Mutex);
			RegisteredSnapshot = Registry.Registered;
			ProviderSnapshot = Registry.Providers;
			CommandLineSnapshot = Registry.CommandLine;
		}

		// --- 4. C++ registrations ---------------------------------------------------------------
		for (const TPair<FString, FDreamShaderDefineEntry>& Pair : RegisteredSnapshot)
		{
			OfferToTable(
				Table, Pair.Key, Pair.Value.Value,
				EDreamShaderDefineSource::Registered, Pair.Value.SourceTag);
		}

		// --- 5. Providers -----------------------------------------------------------------------
		// ExecuteIfBound, not Execute: a provider bound with CreateSP or CreateUObject goes unbound on
		// its own when its owner dies, and a compile is not the place to assert about that.
		for (const FProviderRecord& Record : ProviderSnapshot)
		{
			Record.Delegate.ExecuteIfBound(Table);
		}

		// --- 6. Command line --------------------------------------------------------------------
		for (const TPair<FString, FString>& Pair : CommandLineSnapshot)
		{
			OfferToTable(
				Table, Pair.Key, Pair.Value,
				EDreamShaderDefineSource::CommandLine, FString(CommandLineSourceTag));
		}

		// --- 7. Re-assert the builtins ----------------------------------------------------------
		// "Builtin is the exception that never loses" (see EDreamShaderDefineSource). Set() enforces
		// that for any tier offering a name spelled with the DS_ prefix -- but the table's storage is
		// a TMap<FString, ...>, and Unreal's FString key funcs hash and compare CASE-INSENSITIVELY.
		// So a define named `ds_substrate` passes IsReservedDreamShaderDefineName (which is, and has
		// to be, case-sensitive), is accepted by Set, and then lands on the very slot DS_SUBSTRATE
		// occupies. A provider can also simply Remove() a builtin outright.
		//
		// Rather than leave a hole in an invariant the whole design leans on, the last thing resolve
		// does is put the builtins back and say so out loud. Under the stated precedence this is the
		// correct end state either way, so it costs nothing when nobody is misbehaving.
		for (const TPair<FString, FDreamShaderDefineEntry>& Pair : Builtins.GetEntries())
		{
			const FDreamShaderDefineEntry* Current = Table.Find(Pair.Key);
			if (Current
				&& Current->Source == EDreamShaderDefineSource::Builtin
				&& Current->Value.Equals(Pair.Value.Value, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Current)
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader builtin define '%s' was overwritten by %s; restoring the builtin ")
					TEXT("value '%s'. Builtin defines describe the compiling process and are read-only ")
					TEXT("-- note that define names are case-sensitive while the underlying table is ")
					TEXT("not, so a lowercase spelling of a DS_ name lands on the builtin's slot."),
					*Pair.Key, *DescribeSource(Current->Source, Current->SourceTag), *Pair.Value.Value);
			}
			else
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader builtin define '%s' was removed by a define provider; restoring ")
					TEXT("the builtin value '%s'. Builtin defines are read-only."),
					*Pair.Key, *Pair.Value.Value);
			}

			Table.Set(Pair.Key, Pair.Value.Value, EDreamShaderDefineSource::Builtin, Pair.Value.SourceTag);
		}

		return Table;
	}

	uint32 GetDreamShaderDefineRevision()
	{
		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);
		return Registry.Revision;
	}

	void NotifyDreamShaderDefineSettingsChanged()
	{
		// The settings tier is read live from the CDO in ResolveDreamShaderDefines, so there is
		// nothing to copy across -- the bump IS the entire effect. It is what tells whatever holds
		// compiled output (the ThinCustom in-memory materials above all) that the material built from
		// the previous define set no longer describes what the source says.
		//
		// Unconditional, unlike the registry writers: by the time PostEditChangeProperty runs the
		// property already holds its new value and the old one is gone, so there is nothing left to
		// compare against. An over-bump costs one regeneration; a missed one costs a session spent
		// looking at a material that does not match its source.
		Private::DefineTableImpl::FRegistry& Registry = Private::DefineTableImpl::GetRegistry();
		FScopeLock Lock(&Registry.Mutex);
		++Registry.Revision;
	}
}
