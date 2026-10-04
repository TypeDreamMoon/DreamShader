// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The HLSL pass slots, compiler side.
//
// A `.dsp` pass written in HLSL -- `compute`, or `fullscreen` without a material -- runs in one permutation ("slot") of
// the global shaders FDreamPassCS / FDreamPassPS (DreamShaderPass, Render/DreamPassGlobalShaders.cpp). Those shaders
// include a registry this file writes, `<user shader directory>/RegistryCompute.ush` and `RegistryPixel.ush`, and the
// registry gives each slot its pass: `#if DP_SLOT == 7`, the pass's names #defined onto the fixed parameters of
// Shaders/Pass/DreamPass.ush, the entry renamed, and an `#include` of the pass's SNAPSHOT -- a copy of its `.usf`, and
// of every file that includes by a relative path, taken the moment it passed the pre-check. A pass whose HLSL is in its
// `.dsp` (DreamShader_Plan/10) has a generated root instead of the `.usf`: the file's `hlsl` block with the other passes'
// entries blanked, then the pass's own code (Lang/LangHlslText.h, BuildDreamPassInlineHlslRoot). Never the live file: a
// global shader that fails to compile is fatal (a retry box, then an exit), and the registry is committed, so a typo
// saved in a `.usf` must never reach a teammate's editor or a cook.
//
// `Registry.json` beside the two registry files is the record: slot -> (pipeline object path, pass, kind, the snapshot's
// hash, the shader formats it was pre-checked for, the section text). The registry files are rebuilt from it, which is
// what keeps a slot another pipeline owns intact while this one is compiled. Slots are stable: an existing
// (pipeline, pass) keeps its slot, a new pass takes the lowest free one, and a pass a pipeline no longer has frees its
// slot when that pipeline is compiled again (`dsc pass-registry -Gc` catches the pipelines whose source is gone).
//
// The emit runs it in three steps, so that nothing is written unless every changed slot compiled:
//   PlanDreamPassSlots      slots, snapshots in memory, sections, which slots changed     writes nothing
//   PrecheckDreamPassSlots  each changed slot compiled in-process for every format        writes nothing
//   CommitDreamPassSlots    snapshots, freed slots, Registry.json, the registry files      then the hot reload
//
// Game thread only.

#pragma once

#include "CoreMinimal.h"

#include "Lang/LangDiagnostic.h"
#include "Lang/LangHlslText.h"
#include "Pass/DreamShaderPassShaderText.h"

class UDreamPassPipeline;

namespace UE::DreamShader::Editor::Compiler
{
	/** One slot of Registry.json. */
	struct FDreamPassRegistrySlot
	{
		int32 Slot = INDEX_NONE;
		/** The UDreamPassPipeline's object path. */
		FString Pipeline;
		FString Pass;
		/** `compute` or `fullscreen`. */
		FString Kind;
		/** The `.dsp`, project-relative. */
		FString Source;
		/** The shader file the pass names, project-relative (the reference as written when it names no file). */
		FString Shader;
		FString Entry;
		/** The slot hash: its section text, its snapshot's files and the text of the live includes. Empty: reserved, no snapshot. */
		FString Hash;
		/** The shader formats the pre-check compiled this snapshot for. */
		TArray<FString> Formats;
		/** The snapshot's files, relative to the slot directory, the included one first. */
		TArray<FString> Files;
		/** The registry section, as the registry file holds it. Empty for a reserved slot. */
		FString Section;

		bool HasSnapshot() const { return !Hash.IsEmpty() && !Section.IsEmpty(); }
	};

	struct FDreamPassRegistry
	{
		TArray<FDreamPassRegistrySlot> Compute;
		TArray<FDreamPassRegistrySlot> Pixel;

		TArray<FDreamPassRegistrySlot>& Table(bool bCompute) { return bCompute ? Compute : Pixel; }
		const TArray<FDreamPassRegistrySlot>& Table(bool bCompute) const { return bCompute ? Compute : Pixel; }

		const FDreamPassRegistrySlot* Find(bool bCompute, const FString& Pipeline, const FString& Pass) const;
		const FDreamPassRegistrySlot* FindSlot(bool bCompute, int32 Slot) const;
		/** Every slot of either table that Pipeline owns. */
		void FindPipeline(const FString& Pipeline, TArray<const FDreamPassRegistrySlot*>& OutSlots) const;
		/** Slot order, so the files written from it never depend on insertion order. */
		void Sort();
	};

	/** `<user shader directory>/Registry.json`. */
	FString GetDreamPassRegistryJsonPath();

	/**
	 * Reads Registry.json. A missing file is an empty registry and true; a file that exists but does not parse is false
	 * with OutError, because writing over it would lose every slot it records.
	 */
	bool LoadDreamPassRegistry(FDreamPassRegistry& OutRegistry, FString& OutError);

	FString SerializeDreamPassRegistry(const FDreamPassRegistry& Registry);

	/** The text of RegistryCompute.ush / RegistryPixel.ush for a registry. */
	FString BuildDreamPassRegistryShaderText(const FDreamPassRegistry& Registry, bool bCompute);

	/** One HLSL pass of the pipeline being compiled, as the emitter hands it over. */
	struct FDreamPassSlotCandidate
	{
		bool bCompute = true;
		/** Index into the staged pipeline's Passes. */
		int32 PassIndex = INDEX_NONE;
		FString PassName;
		/**
		 * `Shader = "..."` as written, its virtual path, the file on disk. The file may lie anywhere -- next to the `.dsp` is the
		 * usual place -- so the virtual path is empty for one outside every mapped directory: the slot compiles the snapshot,
		 * under the mapped user directory, never the file itself.
		 */
		FString ShaderReference;
		FString ShaderVirtualPath;
		FString ShaderFilePath;
		FString Entry;
		/** Where messages about this pass go: the pass's span in the `.dsp`. */
		Lang::FLangSpan Span;

		/**
		 * The pass's HLSL is in its `.dsp`: the snapshot's root is generated -- Inline.Text, with the map of its lines back to
		 * the `.dsp` that the pre-check reports through -- and ShaderFilePath is the `.dsp`. Entry is the slot's own entry
		 * point for a body form, whose function the root names so. At BeginView the root guards the pass's own code against
		 * `View` itself (the section does not), and the pre-check refuses a slot that binds the view through shared code.
		 */
		bool bInline = false;
		Lang::FHlslInlineRoot Inline;
		bool bAtBeginView = false;

		// --- filled by PlanDreamPassSlots ---
		int32 Slot = INDEX_NONE;
		FDreamPassShaderClosure Closure;
		/** The section up to its `#include`, and from the `#undef`s on. */
		FString SectionHead;
		FString SectionTail;
		/** The included file, relative to the slot directory. */
		FString IncludeRelativePath;
		/** Inline: where the generated root sits, `<.dsp folder>/<pipeline>.<pass>.usf` -- no file is there. */
		FString InlineRootFile;
		/** The section as the registry file will hold it. */
		FString Section;
		FString Hash;
		/** The registry already holds this hash and the snapshot is on disk: nothing to check, nothing to write. */
		bool bUnchanged = false;
		/** The formats the pre-check compiled it for (unchanged: the registry's). */
		TArray<FString> CheckedFormats;
	};

	/** A slot a pipeline gives back: a pass it no longer has, or one that moved to the other table. */
	struct FDreamPassFreedSlot
	{
		bool bCompute = true;
		int32 Slot = INDEX_NONE;
		FString Pass;
	};

	/** What one pipeline's compile does to the registry. */
	struct FDreamPassSlotPlan
	{
		FString PipelineObjectPath;
		/** The asset's name, for the section comments. */
		FString PipelineName;
		FString PipelineSourceFile;
		/** The registry as it will be. */
		FDreamPassRegistry Registry;
		TArray<FDreamPassSlotCandidate> Candidates;
		TArray<FDreamPassFreedSlot> FreedSlots;
		bool bComputeChanged = false;
		bool bPixelChanged = false;

		bool HasChanges() const { return bComputeChanged || bPixelChanged; }
	};

	/**
	 * Plans the slots of one pipeline. Staged is the pipeline as the emit is about to make it (the param types come off it,
	 * through UE::DreamPass::GetParamBindingType, as the runtime reads them). Fills each candidate's slot, snapshot,
	 * section and hash, and says which slots changed. Writes nothing. False with diagnostics when a snapshot cannot be built,
	 * a section cannot be written or the slots are used up.
	 */
	bool PlanDreamPassSlots(
		const UDreamPassPipeline& Staged,
		const FString& PipelineObjectPath,
		const FString& PipelineSourceFile,
		TArray<FDreamPassSlotCandidate>&& Candidates,
		FDreamPassSlotPlan& OutPlan,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Compiles every changed candidate of the plan in-process, for every format, without the global shader map ever seeing
	 * it: a failure is a diagnostic at the line of the `.usf` that caused it, never the fatal a global shader error is.
	 * Formats empty: ResolveDreamPassPrecheckFormats. bRecheckUnchanged checks the unchanged ones as well (`check -Shaders`).
	 */
	bool PrecheckDreamPassSlots(
		FDreamPassSlotPlan& Plan,
		const TArray<FName>& Formats,
		bool bRecheckUnchanged,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Writes the changed snapshots, deletes the freed slots (an Info each), then Registry.json and the registry files that
	 * changed -- all of it or, when one of those files cannot be written or deleted, none of it (DSH8326).
	 */
	bool CommitDreamPassSlots(FDreamPassSlotPlan& Plan, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Writes a registry as it stands -- Registry.json and both registry files -- and deletes the slot directories it no
	 * longer names. What `pass-registry -Gc` and `-Rebuild` end with. All or nothing, as CommitDreamPassSlots is.
	 */
	bool WriteDreamPassRegistry(const FDreamPassRegistry& Registry, const FDreamPassRegistry* Previous, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * Before a registry write touches the disk: whether every file in Files can be written and every directory in
	 * DeletedDirectories deleted with all it holds. False with DSH8326 naming the first that cannot -- an existing file
	 * that is read-only, the usual state of a committed file that is not checked out -- and then the caller writes and
	 * deletes nothing, so that Registry.json, the registry files and the snapshots never disagree.
	 */
	bool CheckDreamPassRegistryWritable(const TArray<FString>& Files, const TArray<FString>& DeletedDirectories, Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * In the editor (never a commandlet, never without a renderer): flushes the shader file cache and recompiles
	 * FDreamPassCS and/or FDreamPassPS, found by name, for every active feature level, so the next frame runs the new
	 * snapshots. Synchronous. The pre-check is what makes this safe; nothing here can catch a failure.
	 */
	void HotReloadDreamPassShaders(bool bCompute, bool bPixel);

	/**
	 * The shader formats a pre-check compiles for: those of the active feature levels -- what the hot reload compiles -- and
	 * every format the active target platforms target -- what a cook compiles. OutUnavailable gets the ones this machine has
	 * no shader compiler for.
	 */
	void ResolveDreamPassPrecheckFormats(TArray<FName>& OutFormats, TArray<FName>& OutUnavailable);

	/**
	 * Whether every HLSL pass of Pipeline (an asset as it stands) has its slot in the registry, a snapshot that passed the
	 * pre-check, and the snapshot's files on disk. A compile may skip on an unchanged source hash only when it does: a
	 * registry somebody deleted or a merge that lost a slot is rebuilt by the next compile, not hidden by the hash.
	 */
	bool IsDreamPassRegistryCurrentFor(const UDreamPassPipeline& Pipeline, const FString& PipelineObjectPath);

	/** `C07` / `P03`: the slot directory's leaf. */
	FString MakeDreamPassSlotLeaf(bool bCompute, int32 Slot);

	/** Whether every file of a slot's snapshot is on disk. False for a slot that records none. */
	bool AreDreamPassSnapshotFilesPresent(bool bCompute, const FDreamPassRegistrySlot& Slot);

	/**
	 * Deletes the slot directories under `<user shader directory>/Slots` that no slot of Registry names -- leftovers of a
	 * reset or of a merge. Only directories spelled as slot directories are touched. Returns how many went.
	 */
	int32 DeleteStrayDreamPassSlotDirectories(const FDreamPassRegistry& Registry);
}
