// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Graph dump: a canonical, byte-stable JSON description of the material graph a DreamShader source
// generates. This is a DEVELOPER TOOL, not a shipping feature -- it exists to be the parity oracle
// for the 2.0 compiler rewrite (Plan/v2-architecture.md section 8, item 1). The current generator is
// going to be deleted; a JSON capture taken on the current tree is the only ground truth the
// replacement can be compared against, source file by source file.
//
// Everything here is subordinated to determinism. A fingerprint that changes between two runs of the
// SAME compiler cannot say anything about two DIFFERENT compilers, so anything that is not a
// property of the graph is excluded rather than merely normalized: node coordinates, node colours,
// the numeric suffix engines append to a recreated UObject's name, MaterialExpressionGuid, the
// UEdGraphNode back-pointer, the Material/Function back-pointers, Desc, and the comment-bubble /
// collapsed flags. Node identity is positional -- "n0, n1, ..." in a canonical traversal order --
// precisely so that no engine-assigned identifier can leak into the output.

#pragma once

#include "CoreMinimal.h"

// FDreamShaderError: the DSH9030-DSH9034 failures below carry a code alongside their message, the
// same way every other stage of the pipeline does.
#include "DreamShaderDiagnostic.h"

class UObject;

namespace UE::DreamShader::Editor::Private
{
	/** One asset a source produced, and where its dump went. */
	struct FDreamShaderGraphDumpEntry
	{
		/** `/Game/Path/Asset.Asset`. */
		FString ObjectPath;

		/** `Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` / `ThinCustomInstance`. */
		FString Kind;

		/** Absolute path of the `.graph.json` that was written. */
		FString OutputFilePath;

		/** Nodes in the dump -- the whole graph, not just the reachable part. */
		int32 NodeCount = 0;

		/**
		 * True when this asset already had a `.uasset` behind it, so generation was refused (see
		 * FScopedDreamShaderGraphDumpWriteGuard) and the dump describes the asset as it stands on
		 * disk rather than a freshly built graph.
		 */
		bool bReadFromDisk = false;
	};

	/**
	 * Refuses every write to a generated asset for its lifetime.
	 *
	 * A baseline capture must not be a rebuild-and-save of the whole project: that would rewrite
	 * every generated `.uasset` in the working tree just to read them. The guard is the generator's
	 * own SetMayWriteGeneratedAssetsToDisk switch, which makes ShouldDeferPersistedAssetToWriteOwner
	 * refuse any asset that would persist -- BEFORE the old graph is torn down, so nothing is
	 * modified in memory either.
	 *
	 * The consequence is the whole reason `dump-graph` documents a two-step workflow: an asset that
	 * exists on disk is dumped as it stands, not regenerated. Compile the tree first
	 * (`dsc compile -All -Force`) and the two are the same thing; skip that step with stale sources
	 * and the dump describes the old graph. The commandlet counts these and says so.
	 */
	struct FScopedDreamShaderGraphDumpWriteGuard
	{
		FScopedDreamShaderGraphDumpWriteGuard();
		~FScopedDreamShaderGraphDumpWriteGuard();

		FScopedDreamShaderGraphDumpWriteGuard(const FScopedDreamShaderGraphDumpWriteGuard&) = delete;
		FScopedDreamShaderGraphDumpWriteGuard& operator=(const FScopedDreamShaderGraphDumpWriteGuard&) = delete;

	private:
		bool bSavedMayWrite = true;
	};

	/** `<Project>/Saved/DreamShader/GraphBaseline`, the default `-Out`. */
	FString GetDefaultDreamShaderGraphDumpDirectory();

	/**
	 * The canonical JSON for one generated asset: pretty-printed with two-space indents, keys sorted
	 * case-sensitively at every level, LF line endings and a trailing newline. Returns an empty
	 * string for an asset class the dump does not cover.
	 *
	 * SourceFilePath is recorded in the `source` object (root name + path relative to that root) and
	 * is otherwise unused, so a test can pass an empty string. OutNodeCount, when given, receives the
	 * length of the `nodes` array -- the summary line wants it and re-parsing the JSON to get it back
	 * would be the only other way.
	 */
	FString BuildDreamShaderGraphDumpJson(UObject* Asset, const FString& SourceFilePath, int32* OutNodeCount = nullptr);

	/** `<OutputDirectory>/<root>/<source path relative to that root>.<asset leaf>.graph.json`. */
	FString MakeDreamShaderGraphDumpFilePath(
		const FString& OutputDirectory,
		const FString& SourceFilePath,
		const FString& ObjectPath);

	/**
	 * Generates one source in memory and writes one JSON file per asset it produces.
	 *
	 * Callers must already hold an FScopedDreamShaderGraphDumpWriteGuard: this function does not
	 * install one, because a `-All` run needs a single guard around the whole sweep rather than one
	 * per file that another thread could interleave with.
	 */
	bool DumpDreamShaderGraphsForSource(
		const FString& SourceFilePath,
		const FString& OutputDirectory,
		TArray<FDreamShaderGraphDumpEntry>& OutEntries,
		UE::DreamShader::FDreamShaderError& OutError);
}
