// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCatalogManifest.h. The cache half of this file moved to the compiler module
// (Private/Pipeline/DreamShaderBuiltinCatalog.cpp) in the compiler relocation.

#include "Tools/DreamShaderCatalogManifest.h"

#include "Workspace/DreamShaderWorkspaceService.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Compiler
{
	FString GetDreamShaderBuiltinCatalogManifestFilePath()
	{
		// Beside the other manifests rather than a second spelling of the directory: the bridge
		// moves its manifest folder as one thing, and a path written out again here would be the
		// one file left behind.
		const FString ManifestDirectory =
			FPaths::GetPath(Private::FDreamShaderWorkspaceService::GetMaterialExpressionManifestFilePath());
		return FPaths::Combine(ManifestDirectory, TEXT("dreamshader-builtin-catalog.json"));
	}

	bool ExportDreamShaderBuiltinCatalogManifest(
		const FString& OutputFilePath,
		FString& OutWrittenPath,
		FString& OutError)
	{
		OutError.Reset();
		OutWrittenPath = OutputFilePath.IsEmpty()
			? GetDreamShaderBuiltinCatalogManifestFilePath()
			: FPaths::ConvertRelativePathToFull(OutputFilePath);

		// Forced: the manifest is what an out-of-editor tool binds against, so writing a cached copy
		// from before the last plugin mounted would publish an answer this process no longer gives.
		InvalidateDreamShaderBuiltinCatalog();
		const UE::DreamShader::IR::FBuiltinCatalog& Catalog = GetDreamShaderBuiltinCatalog();

		const FString Directory = FPaths::GetPath(OutWrittenPath);
		if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				TEXT("could not create directory '%s'"),
				*Directory);
			return false;
		}

		const FString Json = UE::DreamShader::IR::SaveBuiltinCatalogToJson(Catalog);
		if (!FFileHelper::SaveStringToFile(Json, *OutWrittenPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				TEXT("could not write '%s'"),
				*OutWrittenPath);
			return false;
		}

		return true;
	}
}
