#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderDependencyGraphService
	{
		static bool TryExtractImportPathFromLine(const FString& Line, FString& OutPath);
		static FString NormalizeImportSpecifier(const FString& ImportSpecifier);
		/**
		 * `OutError`, when given, is filled only for a failure the caller could not have described
		 * itself — currently a root-qualified specifier naming a root that does not exist. It stays
		 * empty for an ordinary "no candidate existed" miss, which the caller words on its own.
		 */
		static bool ResolveImportPath(
			const FString& CurrentFilePath,
			const FString& ImportSpecifier,
			FString& OutResolvedPath,
			FString* OutError = nullptr);
		static void CollectHeaderDependenciesRecursive(
			const FString& SourceFilePath,
			TSet<FString>& OutHeaders,
			TSet<FString>& InOutVisitedFiles);
		static void RebuildMaterialDependencyGraph(TMap<FString, TSet<FString>>& OutHeaderDependentsByFile);
		/**
		 * Reorder a batch of source files so that each one is compiled after everything it imports.
		 *
		 * Compilation order is not a preference. A `.dsm` that calls a `ShaderFunction` binds its call
		 * node against the LIVE `UMaterialFunction` asset -- `SetMaterialFunction` reads the pins off
		 * the object, not off the source -- so compiling the caller before the callee binds it against
		 * the previous version of the function's interface. Renaming or retyping a function input and
		 * saving both files is enough to hit it; which one wins was, until this existed, whichever order
		 * the pending-file map happened to iterate in.
		 *
		 * Only edges inside the batch are honoured, and a cycle is left in traversal order for the
		 * import loader to reject with its own diagnostic.
		 */
		static void SortByDependencyOrder(TArray<FString>& InOutSourceFiles);
		static TSet<FString> RebuildAndCollectDependentsForImport(
			const FString& ImportFilePath,
			TMap<FString, TSet<FString>>& InOutHeaderDependentsByFile);
	};
}
