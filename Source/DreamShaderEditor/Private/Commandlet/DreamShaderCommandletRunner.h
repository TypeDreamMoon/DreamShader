#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Lang
{
	class FLangDiagnosticSink;
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
	 * Whether the boolean flag Name is on: `-Name` alone, or `-Name=` true / 1 / yes / on (off for false / 0 / no / off),
	 * case-insensitively. Any other value, an empty one included, raises DSH9110 into Diagnostics and reads as off -- a
	 * value that says neither is not guessed at, and a verb that finds an error there runs nothing.
	 *
	 * Every verb reads its flags through this one function, Params included, because UCommandlet::ParseCommandLine MOVES
	 * each `-X=Y` out of the switch list into that map: `-Force=true` is never in Switches, and a check of the switches
	 * alone reads it as absent. Searched in TryGetCommandletParam's order -- the map, the switches, the bare tokens -- and
	 * the first that names the flag decides.
	 */
	bool HasCommandletFlag(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		const FString& Name,
		::UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Logs what HasCommandletFlag raised into FlagSink, a `DSH9110: ...` Error line each, and returns whether there was
	 * any: a verb that gets true back runs nothing. The lines carry no location: a flag is on the command line, not in
	 * a file.
	 */
	bool LogCommandletFlagErrors(const ::UE::DreamShader::Lang::FLangDiagnosticSink& FlagSink);

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
	 * Returns false when none of the three was given, which is the usage-banner case -- or when `-All` has a value that is
	 * not a boolean, which is DSH9110 in Diagnostics; a caller asks the sink first. An empty list with a true return is the
	 * legitimate "this project has no sources" answer.
	 */
	bool ResolveDreamShaderCommandletSourceFiles(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		TArray<FString>& OutSourceFiles,
		::UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

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
	/**
	 * `decompile { -Asset=<path> | -SourceFile=<file> } [-Out=<file>] [-Format=Dss|Legacy|Auto] [-KeepAssetPath] [-Readable]
	 * [-DiagnosticsOut=<file>]`: one decompile through the editor's decompile seam (Tools/DreamShaderDecompileTools.h), which
	 * picks the 1.x or the 2.0 decompiler by the format, written to disk. `-SourceFile` alone decompiles every asset that source
	 * builds into one file. False when the decompile, the write or the diagnostics file failed.
	 */
	bool RunDreamShaderDecompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	// ---------------------------------------------------------------------------- the 2.0 verbs
	//
	// `check`, `dump-ir`, `dump-layout`, `index`, `export-catalog`, `fmt` and `list-generated` run the 2.0
	// pipeline -- or, for `fmt`, the front end alone -- themselves. They are implemented in
	// Tools/DreamShaderCompilerTools.cpp, not here: they drive the pipeline directly rather than through the
	// compiler service `compile` calls, and their diagnostics come out of an FLangDiagnosticSink rather than an
	// FDreamShaderError. These are forwarders, so the dispatcher keeps naming one namespace.

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

	/** `dump-layout <file|-All> [-Style=Blocks|SourceBands|Layered|All] [-Out=<dir>] [-Json]`. */
	bool RunDreamShaderDumpLayoutCommandlet(
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

	/** `fmt <file|-All> [-Check] [-Out=<dir>]`. */
	bool RunDreamShaderFormatCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `list-generated <file|-All> [-As=Packages|Files|GitIgnore|Json] [-Out=<file>] [-IncludeEphemeral]`. */
	bool RunDreamShaderListGeneratedCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
}
