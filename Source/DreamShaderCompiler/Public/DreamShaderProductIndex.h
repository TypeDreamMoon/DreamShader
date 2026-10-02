// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Which DreamShader source builds which asset, for the cross-source references: the `.dsi` parent chain, the materials
// a `.dsp` pipeline's passes name, and the pipelines a `.dss` reads exported buffers from. Reference resolution,
// dependency edges, and the sources to rebuild when one they depend on changes.
//
// The index covers the `.dss`, `.dsi` and `.dsp` files under every source root, refreshed lazily by file timestamp. A
// `.dss` is indexed through product resolution (ResolveDreamShaderSourceProducts), so its object paths are exactly the
// ones a build writes; a `.dsi` and a `.dsp` are indexed by a parse alone -- each has one product, named by the binder's
// rule (the file stem, or `/// @name` above a `.dsi`'s pragma) -- because resolving either would need the very
// references the index is for. A legacy `.dsm` product is not in the index: an instance of one names it by path and
// resolves as a foreign parent (its schema then comes from the loaded asset).

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Editor::Compiler
{
	struct FDreamShaderProductRecord
	{
		/** Absolute, normalized. */
		FString SourceFilePath;
		/** The asset's object name: what a bare-name Parent matches. */
		FString ProductName;
		/** `/Game/FX/M_Glow.M_Glow`. */
		FString ObjectPath;
		IR::EIRProductKind Kind = IR::EIRProductKind::Material;
		/** `.dsi` only: Parent as written. */
		FString ParentReference;
		/** `.dsp` only: every `Material = "..."` its passes name, as written. */
		TArray<FString> MaterialReferences;
		/** `.dsp` only: every `Shader = "..."` its passes name, as the file on disk (absolute); one that resolves to no file is left out. */
		TArray<FString> ShaderFiles;
		/** `.dss` only: the `Pipeline = "..."` of every UE.DreamPassBuffer it reads, as written. */
		TArray<FString> PassPipelineReferences;
	};

	class FDreamShaderProductIndex
	{
	public:
		/** Process-wide. Game thread only. */
		static DREAMSHADERCOMPILER_API FDreamShaderProductIndex& Get();

		/** Re-indexes every source whose timestamp changed and forgets deleted ones. Pointers handed out before are invalid afterwards. */
		DREAMSHADERCOMPILER_API void Refresh();

		/** The record building ObjectPath (an object path, or a package name for an asset named after its package), or null. */
		DREAMSHADERCOMPILER_API const FDreamShaderProductRecord* FindByObjectPath(const FString& ObjectPath) const;

		/** Every record named ProductName (case-sensitive) whose source lies under RootDirectory; every root when RootDirectory is empty. */
		DREAMSHADERCOMPILER_API void FindByName(const FString& RootDirectory, const FString& ProductName, TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/** The records a source builds. */
		DREAMSHADERCOMPILER_API void FindBySource(const FString& SourceFilePath, TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/** The `.dsi` records whose Parent resolves to a product of ParentSourceFile (one level). */
		DREAMSHADERCOMPILER_API void FindInstancesOfSource(const FString& ParentSourceFile, TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/** The `.dsp` records one of whose material references resolves to a product of MaterialSourceFile. */
		DREAMSHADERCOMPILER_API void FindPipelinesUsingSource(const FString& MaterialSourceFile, TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/** The `.dss` records one of whose UE.DreamPassBuffer pipeline references resolves to the product of PipelineSourceFile. */
		DREAMSHADERCOMPILER_API void FindPassBufferReadersOfSource(const FString& PipelineSourceFile, TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/** Every `.dsp` record. */
		DREAMSHADERCOMPILER_API void FindPipelines(TArray<const FDreamShaderProductRecord*>& OutRecords) const;

		/**
		 * True while Refresh runs. Indexing a `.dss` runs its front half, and a stage in there that would look references up
		 * in the index -- the `.dss` pass-buffer stage -- must not read a half-built index; it asks this and stands down.
		 */
		bool IsRefreshing() const { return bRefreshing; }

	private:
		struct FIndexedFile
		{
			FDateTime Timestamp;
			TArray<FDreamShaderProductRecord> Records;
		};

		TMap<FString, FIndexedFile> Files;
		bool bRefreshing = false;
	};

	/**
	 * A `.dsi`'s Parent reference -> the parent's object path, plus the source that builds it (empty for a foreign
	 * parent). A path form (`"/Game/X/M"`, `"/Game/X/M.M"`, a `Class'...'` shell, `Path(...)`) resolves through
	 * TryResolveDreamShaderAssetReference, then the index; a foreign parent must exist (loaded or on disk). A bare name
	 * is looked up among the products built under the instance's own source root. DSH8260 unresolvable or missing,
	 * DSH8261 no product of that name, DSH8262 several, DSH8263 the instance itself -- each at ParentSpan.
	 */
	DREAMSHADERCOMPILER_API bool ResolveInstanceParent(
		const FString& InstanceSourceFile,
		const FString& ParentReference,
		const Lang::FLangSpan& ParentSpan,
		FString& OutObjectPath,
		FString& OutParentSourceFile,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The `.dsi` sources to rebuild after SourceFilePath rebuilt: every instance whose Parent resolves to one of its
	 * products, and the instances of those, transitively, each once. The bridge queues them,
	 * so a renamed or retyped parent parameter surfaces as the child's error without a child edit.
	 */
	DREAMSHADERCOMPILER_API void CollectInstanceDependents(const FString& SourceFilePath, TArray<FString>& OutInstanceSourceFiles);

	/** The source that builds a `.dsi`'s parent, or empty when it does not resolve quietly or the parent is foreign. The dependency sort's instance edge. */
	DREAMSHADERCOMPILER_API FString FindInstanceParentSourceFile(const FString& InstanceSourceFile);

	/**
	 * The `.dsp` sources to rebuild after SourceFilePath (a `.dss` or a `.dsi`) rebuilt: every pipeline one of whose passes
	 * names a product of it as its material. One level: a pipeline is nobody's material.
	 */
	DREAMSHADERCOMPILER_API void CollectPipelineDependents(const FString& SourceFilePath, TArray<FString>& OutPipelineSourceFiles);

	/**
	 * The `.dss` sources to rebuild after PipelineSourceFile (a `.dsp`) rebuilt: every source whose UE.DreamPassBuffer reads
	 * that pipeline -- its exports may have changed, and the materials list its render targets among their textures.
	 */
	DREAMSHADERCOMPILER_API void CollectPassBufferDependents(const FString& PipelineSourceFile, TArray<FString>& OutSourceFiles);

	/** The sources that build the materials a `.dsp` names: the dependency sort's `.dsp` -> `.dss` edge. */
	DREAMSHADERCOMPILER_API void FindPipelineMaterialSourceFiles(const FString& PipelineSourceFile, TArray<FString>& OutSourceFiles);

	/** The `.dsp` sources whose buffers a `.dss` reads: the dependency sort's `.dss` -> `.dsp` edge. */
	DREAMSHADERCOMPILER_API void FindPassBufferPipelineSourceFiles(const FString& SourceFilePath, TArray<FString>& OutPipelineSourceFiles);
}
