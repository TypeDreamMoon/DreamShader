// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Custom Pass half of the compiler as the editor's tools reach it:
//
//   the HLSL slot registry     as `dsc pass-registry` lists it, collects its garbage (`--gc`) and rebuilds it (`--rebuild`)
//   the slot pre-check         as `check -Shaders` runs it for a `.dsp`: every HLSL pass, changed or not
//   the shader files           a `.dsp` compiles from -- each `Shader = "..."` and what it includes -- which the editor
//                              bridge watches wherever they are: a pass's `.usf` may live anywhere, next to its `.dsp`
//                              as often as not, because the engine only ever compiles its snapshot
//
// Nothing here builds an asset. Game thread only.

#pragma once

#include "CoreMinimal.h"

#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Editor::Compiler
{
	/** What one slot of the registry is. */
	enum class EDreamPassSlotState : uint8
	{
		/** Its pipeline's source runs that pass in HLSL in that table, and the snapshot is on disk. */
		Live,
		/** Recorded without a snapshot that passed a pre-check: it compiles to the empty stub. */
		Reserved,
		/** The snapshot's files are gone (a Slots folder that was not committed); the next compile of its pipeline writes them. */
		SnapshotMissing,
		/** No `.dsp` under the source roots builds its pipeline any more. `--gc` frees it. */
		PipelineGone,
		/** Its pipeline's source no longer runs that pass in HLSL in that table. `--gc` frees it. */
		PassGone,
		/** Its pipeline's source does not compile far enough to tell. Kept. */
		Unknown,
	};

	/**
	 * `Live`, `Reserved`, `SnapshotMissing`, `PipelineGone`, `PassGone`, `Unknown`. Not a LexToString overload: one declared
	 * in this namespace would hide the engine's for every unqualified call in it.
	 */
	DREAMSHADERCOMPILER_API const TCHAR* LexDreamPassSlotState(EDreamPassSlotState State);

	/** One slot, as Registry.json records it and as DescribeDreamPassRegistry judged it. */
	struct FDreamPassSlotReport
	{
		bool bCompute = true;
		int32 Slot = INDEX_NONE;
		/** The UDreamPassPipeline's object path. */
		FString Pipeline;
		FString Pass;
		/** The `.dsp`, project-relative. */
		FString Source;
		/** The shader file, project-relative. */
		FString Shader;
		FString Entry;
		FString Hash;
		/** The shader formats its snapshot passed the pre-check for. */
		TArray<FString> Formats;
		EDreamPassSlotState State = EDreamPassSlotState::Live;
		/** PipelineGone: the pipeline asset still exists, so its pass still points at this slot. */
		bool bPipelineAssetExists = false;
	};

	struct FDreamPassRegistryReport
	{
		FString RegistryJsonPath;
		int32 ComputeSlotCount = 0;
		int32 PixelSlotCount = 0;
		/** Compute slots first, then pixel ones, each in slot order. */
		TArray<FDreamPassSlotReport> Slots;
	};

	/**
	 * Reads Registry.json and judges every slot. bClassify runs the front end of every `.dsp` that owns a slot -- nothing is
	 * built or written -- to tell Live from PipelineGone, PassGone and Unknown; without it a slot is Live, Reserved or
	 * SnapshotMissing. False with OutError when Registry.json does not parse.
	 */
	DREAMSHADERCOMPILER_API bool DescribeDreamPassRegistry(bool bClassify, FDreamPassRegistryReport& OutReport, FString& OutError);

	/**
	 * `pass-registry --gc`: frees every PipelineGone and PassGone slot -- its entry, its registry section, its snapshot -- and
	 * deletes the slot directories nothing names. Hot reloads the slot shaders in the editor. OutFreed lists what was freed.
	 * False with diagnostics when Registry.json does not parse or a file cannot be written.
	 */
	DREAMSHADERCOMPILER_API bool CollectDreamPassRegistryGarbage(TArray<FDreamPassSlotReport>& OutFreed, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The first step of `pass-registry --rebuild` when Registry.json does not parse -- a merge conflict left in it: moves it
	 * aside to `Registry.json.unreadable` and writes an empty registry, so the compiles that follow assign every slot afresh.
	 * True without touching anything when the file parses; OutMovedTo is then empty. False with diagnostics when a file
	 * cannot be moved or written.
	 */
	DREAMSHADERCOMPILER_API bool ResetUnreadableDreamPassRegistry(FString& OutMovedTo, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The last step of `pass-registry --rebuild`, after every `.dsp` was compiled again: writes RegistryCompute.ush and
	 * RegistryPixel.ush from Registry.json as it stands, turning a slot whose snapshot files are missing into a reserved one
	 * (a registry that includes a missing file fails the global shader compile at the next start), deletes the slot
	 * directories it does not name, and hot reloads the slot shaders in the editor. OutReserved counts the slots it turned.
	 */
	DREAMSHADERCOMPILER_API bool RewriteDreamPassRegistryFiles(int32& OutReserved, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * `check -Shaders` on a `.dsp`: runs it to IR -- nothing built, nothing written -- stages each pipeline, plans its slots
	 * against the registry as it stands and pre-checks every HLSL pass, changed or not, for Formats (empty: the formats a
	 * compile pre-checks for). OutPassesChecked counts the HLSL passes. False with diagnostics on any error.
	 */
	DREAMSHADERCOMPILER_API bool CheckDreamShaderPipelineSlots(
		const FString& SourceFilePath,
		const TArray<FName>& Formats,
		Lang::FLangDiagnosticSink& Diagnostics,
		int32& OutPassesChecked);

	/** `.usf` or `.ush`: a file a `.dsp`'s HLSL pass can compile from. */
	DREAMSHADERCOMPILER_API bool IsDreamShaderPassShaderFile(const FString& Path);

	/**
	 * Every file the HLSL passes of a `.dsp` compile from: each `Shader = "..."` file, everything it includes by a relative
	 * path (a file such an include names that does not exist yet as well -- creating it changes the pass), and the files
	 * behind the user virtual includes its snapshot keeps live. Absolute, normalized, each once.
	 */
	DREAMSHADERCOMPILER_API void CollectDreamShaderPipelineShaderFiles(const FString& PipelineSourceFile, TArray<FString>& OutShaderFiles);

	/** The `.dsp` sources one of whose HLSL passes compiles from ShaderFile (see CollectDreamShaderPipelineShaderFiles). */
	DREAMSHADERCOMPILER_API void FindDreamShaderPipelinesUsingShaderFile(const FString& ShaderFile, TArray<FString>& OutPipelineSourceFiles);

	/** Every directory holding a file some `.dsp` compiles from: what the bridge watches for `.usf` / `.ush` edits. */
	DREAMSHADERCOMPILER_API void CollectDreamShaderPipelineShaderDirectories(TArray<FString>& OutDirectories);
}
