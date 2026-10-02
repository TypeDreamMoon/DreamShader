// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `.usf` half of a `.dsp`, read as text: where a `Shader = "..."` reference lands -- the file on disk and its
// virtual shader path -- what the file defines (top-level functions, and the group size of every compute entry),
// and which files it pulls in: the ones it includes by a relative path, which a slot's snapshot copies with their
// layout, and the absolute ones it leaves alone. No compile and no UObject; the reference stage before the bind, the
// snapshot builder of the slot registry and the bridge's `.usf` watch all read shader files through here, so the
// three agree on what one shader file depends on.
//
// The include scan is textual and blind to `#if`, like the dependency graph's scan of DreamShader sources
// (DreamShaderDependencyGraphService.cpp): a file included only from a dead branch still belongs to the snapshot.
// Copying a file that is not compiled costs nothing; leaving out one that is would compile against the live file.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Compiler
{
	/** One `#include "..."` of a shader text: the path as written and the 1-based line it is on. */
	struct FDreamPassShaderInclude
	{
		FString Path;
		int32 Line = 1;
	};

	/** The text with every comment blanked to spaces, newlines kept, so that lines and offsets still match the file. */
	FString StripDreamPassShaderComments(const FString& Text);

	/** Every `#include` of a comment-stripped text, in order. `#if` is not evaluated: the union of every branch. */
	void ScanDreamPassShaderIncludes(const FString& StrippedText, TArray<FDreamPassShaderInclude>& OutIncludes);

	/**
	 * The functions a comment-stripped text defines at file scope. One with a `[numthreads(x, y, z)]` in front of it whose
	 * three arguments are integer literals goes into OutComputeEntries with that group size; every other one -- a pixel
	 * entry, a helper, a compute entry whose group size is spelled with macros -- into OutFunctions.
	 */
	void ScanDreamPassShaderFunctions(const FString& StrippedText, TMap<FString, FIntVector>& OutComputeEntries, TArray<FString>& OutFunctions);

	/** `.usf` or `.ush`, case-insensitively: the only extensions a virtual shader path may have (RC/Private/ShaderCore.cpp). */
	bool IsDreamPassShaderFileExtension(const FString& Path);

	/** Absolute, forward slashes, `..` collapsed: the one spelling shader files are compared in, by every caller. */
	FString NormalizeDreamPassShaderFilePath(const FString& Path);

	/** A file on disk as a virtual shader path, through the mapped directory that holds it (the deepest one). False when none does. */
	bool MapDreamPassShaderFileToVirtualPath(const FString& FilePath, FString& OutVirtualPath);

	/** A virtual shader path as a file on disk, through the longest mapped virtual directory it starts with. False when none matches. */
	bool MapDreamPassShaderVirtualPathToFile(const FString& VirtualPath, FString& OutFilePath);

	/**
	 * A pass's `Shader = "..."`: a path relative to the folder of the `.dsp` that names it, or a virtual shader path
	 * (`/Project/Passes/Blur.usf`). Each output is set when it resolves and left empty when it does not -- a file outside
	 * every mapped directory has no virtual path, a virtual path no mapping covers has no file.
	 */
	void ResolveDreamPassShaderReference(const FString& Reference, const FString& PipelineSourceFile, FString& OutVirtualPath, FString& OutFilePath);

	/** One file of a slot's snapshot. */
	struct FDreamPassShaderClosureFile
	{
		/** Absolute, normalized. */
		FString FilePath;
		/** Relative to the closure's common directory, forward slashes: where the snapshot puts it in the slot directory. */
		FString RelativePath;
		FString Text;
	};

	/** An include the snapshot leaves pointing at a live file: a virtual path that is neither `/Engine/` nor `/Plugin/`. */
	struct FDreamPassLiveInclude
	{
		FString IncludingFile;
		int32 Line = 1;
		FString VirtualPath;
		/** The file it maps to; empty when no mapping covers it. */
		FString FilePath;
		/** Its text, folded into the snapshot hash so a change to it still counts as a change of the slot. */
		FString Text;
	};

	/** A relative include that names no file. */
	struct FDreamPassMissingInclude
	{
		FString IncludingFile;
		int32 Line = 1;
		FString Path;
	};

	/** A shader file and everything it includes by a relative path, transitively: what a slot's snapshot holds. */
	struct FDreamPassShaderClosure
	{
		FString RootFilePath;
		/** The deepest directory that holds every file of the closure; the snapshot keeps the layout below it. */
		FString CommonDirectory;
		/** The root first, then in discovery order. */
		TArray<FDreamPassShaderClosureFile> Files;
		TArray<FDreamPassLiveInclude> LiveIncludes;
		TArray<FDreamPassMissingInclude> MissingIncludes;

		const FDreamPassShaderClosureFile* GetRoot() const { return Files.Num() > 0 ? &Files[0] : nullptr; }
		const FDreamPassShaderClosureFile* FindByRelativePath(const FString& RelativePath) const;

		/** SHA-1 of every file's relative path and text, sorted, and of every live include's path and text. */
		FString ComputeContentHash() const;
	};

	/** Reads RootFilePath and every file it includes by a relative path. False only when the root cannot be read. */
	bool CollectDreamPassShaderClosure(const FString& RootFilePath, FDreamPassShaderClosure& OutClosure);

	/** SHA-1 of a text as UTF-8, hex: the one hash the slot registry stamps. */
	FString HashDreamPassText(const FString& Text);
}
