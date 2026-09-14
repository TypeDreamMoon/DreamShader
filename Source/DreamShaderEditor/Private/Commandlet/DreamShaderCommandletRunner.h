#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor
{
	class IDreamShaderDecompiler;
}

namespace UE::DreamShader::Editor::Private
{
	const TCHAR* GetDreamShaderCommandletUsage();

	FString NormalizeCommandletValue(FString Value);
	FString NormalizeCommandletKey(FString Key);
	bool TrySplitCommandletAssignment(const FString& Text, FString& OutKey, FString& OutValue);
	bool TryGetCommandletParam(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		const FString& Name,
		FString& OutValue);

	/**
	 * Reads every `-Define=NAME=VALUE` (short form `-D=NAME=VALUE`) off a commandlet command line and
	 * installs them as the command-line tier of the preprocessor define table. Returns how many
	 * survived validation; invalid and reserved names are dropped with a warning, not a failure.
	 *
	 * Takes the WHOLE command-line string rather than the Tokens/Switches/Params triple every other
	 * function here takes, and that is not an oversight -- see the definition. Must be called before
	 * anything compiles: the table is resolved at the top of each compile, so a define applied
	 * afterwards is a define that changed nothing.
	 */
	int32 ApplyDreamShaderCommandletDefines(const FString& CommandLine);

	/**
	 * The `-Source` / `-File` / `-All` triple, resolved into the list a command works on: one file,
	 * or every project source with `.dsf` ranked ahead of `.dsm` so a function asset exists before
	 * the material that calls it is generated.
	 *
	 * Shared by `compile` and `dump-graph` rather than copied, because the two must agree on WHICH
	 * sources a project has: a baseline that covered a different set than the compiler does would
	 * report a missing dump as a parity difference.
	 *
	 * Returns false when none of the three was given, which is the usage-banner case. An empty list
	 * with a true return is the legitimate "this project has no sources" answer.
	 */
	bool ResolveDreamShaderCommandletSourceFiles(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		TArray<FString>& OutSourceFiles);

	bool RunDreamShaderCompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
	/**
	 * `dump-graph`: generate every named source in memory and write one canonical JSON per asset.
	 * A developer tool -- the parity oracle for the 2.0 compiler rewrite. Never writes an asset.
	 */
	bool RunDreamShaderDumpGraphCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
	bool RunDreamShaderDecompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		UE::DreamShader::Editor::IDreamShaderDecompiler& Decompiler);

	// ---------------------------------------------------------------------------- the 2.0 verbs
	//
	// `check`, `dump-ir`, `index` and `export-catalog` work on `.dss` sources through the 2.0
	// pipeline. They are implemented in Compiler/DreamShaderCompilerTools.cpp, not here: they share
	// no code with the 1.x verbs -- a different source selection (`.dss` rather than `.dsm`/`.dsf`),
	// a different driver, and diagnostics that come out of an FLangDiagnosticSink rather than an
	// FDreamShaderError. These four are forwarders, so the dispatcher keeps naming one namespace.

	/** `check <file|-All> [-Shaders] [-Platform=] [-Quality=] [-Timeout=] [-DiagnosticsOut=]`. */
	bool RunDreamShaderCheckCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `dump-ir <file|-All> [-Out=<dir>] [-Json]`. */
	bool RunDreamShaderDumpIRCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `index <file|-All> [-Out=<dir>]`. */
	bool RunDreamShaderIndexCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `export-catalog [-Out=<file>]`. */
	bool RunDreamShaderExportCatalogCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
}
