// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The four 2.0 commandlet verbs -- `check`, `dump-ir`, `index`, `export-catalog` -- and the source
// selection they share. They take the same (Tokens, Switches, Params) triple UCommandlet hands its
// Main and that every existing verb already takes, so the dispatcher stays a dispatcher.
//
// Until the compiler relocation this header also carried the pipeline's rich entry point
// (FDreamShaderLang2PipelineOptions / FDreamShaderLang2PipelineResult / RunDreamShaderLang2Pipeline).
// That half moved into the compiler module as DreamShaderCompilePipeline.h, which is included below so
// that every caller of the verbs still sees the driver they run.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilePipeline.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Every compilable source -- `.dss`, `.dsi`, `.dsm`, `.dsf` -- under every source root, minus the `Packages`
	 * trees, sorted. The `.dsh` headers FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles also yields
	 * are left out: a header is checked, dumped and indexed through a source that includes it.
	 */
	void FindProjectDreamShaderLang2Sources(TArray<FString>& OutSourceFiles);

	/**
	 * The `-Source` / `-File` / `-All` triple for the 2.0 verbs, resolved the way `compile` resolves
	 * it, over every compilable source.
	 *
	 * Returns false when none of the three was given -- the usage-banner case. An empty list with a
	 * true return is the legitimate "this project has no 2.0 sources yet" answer.
	 */
	bool ResolveDreamShaderLang2CommandletSourceFiles(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		TArray<FString>& OutSourceFiles);

	/** The usage lines for the four 2.0 verbs, appended to the commandlet's own banner. */
	const TCHAR* GetDreamShaderLang2CommandletUsage();

	/**
	 * `check <file|-All> [-Shaders] [-Platform=SM6[,SM5]] [-Quality=High] [-Timeout=120] [-DiagnosticsOut=<file>]`
	 *
	 * Runs the pipeline to IR validation and writes NOTHING -- that is the gate, and it is safe to
	 * run against a tree you do not want touched.
	 *
	 * `-Shaders` is the exception, and it is a real one: a shader compile needs a real material, and
	 * 2.0 has no transient asset to build one into (plan §5), so `check -Shaders` builds and saves
	 * the products exactly as `compile` does before compiling their shaders. It says so in the log
	 * before it starts. See DreamShaderShaderCheck.h and plan §13.3.
	 */
	bool RunDreamShaderCheckCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `dump-ir <file|-All> [-Out=<dir>] [-Json]` -- DumpDreamShaderIRText, and `.json` as well with `-Json`. */
	bool RunDreamShaderDumpIRCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/**
	 * `dump-layout <file|-All> [-Style=Blocks|SourceBands|Layered|All] [-Out=<dir>] [-Json]` -- the graph layouts
	 * computed on the IR, of every product of a source, as an SVG per style, without building an asset. Node sizes are
	 * the IR's own estimate, so the picture shows the arrangement, not the exact footprint a live node has.
	 */
	bool RunDreamShaderDumpLayoutCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `index <file|-All> [-Out=<dir>]` -- BuildDreamShaderSymbolIndexJson per file (plan §13.4). */
	bool RunDreamShaderIndexCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `export-catalog [-Out=<path>]` -- the builtin catalog as JSON, beside the other manifests. */
	bool RunDreamShaderExportCatalogCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/**
	 * `fmt <file|-All> [-Check] [-Out=<dir>]` -- FormatDreamShaderLangSource over 2.0 sources (`.dss`, `.dsi`, and a
	 * `.dsh` with no 1.x declarations), rewriting each in place. `-All` takes the writable source roots only, as
	 * `migrate -All` does: a plugin ships its sources as they are. `-Check` writes nothing and fails when a file would
	 * change, which is the CI form; `-Out` writes the formatted copies under a directory instead of over the sources.
	 * Reads no asset and builds nothing (plan section 13.4).
	 */
	bool RunDreamShaderFormatCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

}
