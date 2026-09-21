// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Which DreamShader source builds which asset, for the `.dsi` parent chain: parent resolution, instance dependency
// edges, and the instances to rebuild when a parent's source changes.
//
// The index covers the `.dss` and `.dsi` files under every source root, refreshed lazily by file timestamp. A `.dss`
// is indexed through product resolution (ResolveDreamShaderSourceProducts), so its object paths are exactly the ones
// a build writes; a `.dsi` is indexed by a parse alone -- its one product is named by the binder's rule (file stem,
// or `/// @name` above the pragma) -- because resolving a `.dsi` would need its parent, which is what the index is
// for. A legacy `.dsm` product is not in the index: an instance of one names it by path and resolves as a foreign
// parent (its schema then comes from the loaded asset).

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
}
