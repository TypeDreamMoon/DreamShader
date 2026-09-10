// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The engine-facing half of the preprocessor define system: everything that BUILDS an
// FDreamShaderDefineTable, as opposed to everything that reads one.
//
// The split is a module boundary, not a taste. FDreamShaderDefineTable itself, the name rules and
// the preprocessor that consumes them live in DreamShaderLang, which depends on Core and nothing
// else, so that a table can be constructed by hand and the whole `#if` layer exercised with no
// editor, no UObject and no plugin manager underneath it. Resolution cannot live there, because
// every tier it merges is an engine fact: UDreamShaderSettings read through its CDO, the engine
// version macros, the plugin descriptor via IPluginManager, r.Substrate via IConsoleManager, the
// commandlet's `-Define=` set, and delegates registered by other modules.
//
// The names and signatures are exactly what DreamShaderDefineTable.h published in 1.9.0. A caller
// that used any of them needs one added include and nothing else.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "DreamShaderDefineTable.h"

namespace UE::DreamShader
{
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
