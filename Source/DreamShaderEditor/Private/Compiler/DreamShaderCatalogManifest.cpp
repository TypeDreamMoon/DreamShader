// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCatalogManifest.h.

#include "DreamShaderCatalogManifest.h"

// Unit E's reflection producer, BuildBuiltinCatalogFromReflection. It is declared alongside the
// emitter rather than in a header of its own; see the unit report's contract notes.
#include "DreamShaderIREmitter.h"

#include "DreamShaderModule.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		TUniquePtr<UE::DreamShader::IR::FBuiltinCatalog> GCachedCatalog;

		/**
		 * Bumped whenever a module loads or unloads. That is the coarse but honest trigger: a
		 * plugin mounting brings UMaterialExpression classes with it, and a catalog built before it
		 * reports every one of them as an unknown name with a "did you mean" that names nothing.
		 * Reflection is cheap enough that over-invalidating costs a second at worst.
		 */
		uint32 GCatalogRevision = 0;
		uint32 GCachedCatalogRevision = MAX_uint32;
		bool bGModulesDelegateRegistered = false;

		void EnsureModulesChangedHook()
		{
			if (bGModulesDelegateRegistered)
			{
				return;
			}

			bGModulesDelegateRegistered = true;
			FModuleManager::Get().OnModulesChanged().AddLambda([](FName, EModuleChangeReason)
			{
				++GCatalogRevision;
			});
		}
	}

	const UE::DreamShader::IR::FBuiltinCatalog& GetDreamShaderBuiltinCatalog()
	{
		EnsureModulesChangedHook();

		if (!GCachedCatalog.IsValid() || GCachedCatalogRevision != GCatalogRevision)
		{
			GCachedCatalog = MakeUnique<UE::DreamShader::IR::FBuiltinCatalog>();
			BuildBuiltinCatalogFromReflection(*GCachedCatalog);
			GCachedCatalogRevision = GCatalogRevision;

			UE_LOG(
				LogDreamShader,
				Verbose,
				TEXT("DreamShader built the builtin expression catalog from reflection: %d expression(s), %d material attribute(s)."),
				GCachedCatalog->Expressions.Num(),
				GCachedCatalog->MaterialAttributes.Num());
		}

		return *GCachedCatalog;
	}

	void InvalidateDreamShaderBuiltinCatalog()
	{
		++GCatalogRevision;
	}

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
