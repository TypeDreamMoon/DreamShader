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
		static TSet<FString> RebuildAndCollectDependentsForImport(
			const FString& ImportFilePath,
			TMap<FString, TSet<FString>>& InOutHeaderDependentsByFile);
	};
}
