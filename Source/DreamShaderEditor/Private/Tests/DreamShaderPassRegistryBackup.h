// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The project's HLSL pass slot registry put back after a test that commits to it: the compiler's Registry test and the
// renderer's HLSL pass cases.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderPassModule.h"
#include "DreamShaderPassPipelines.h"
#include "HAL/PlatformFileManager.h"
#include "Lang/LangDiagnostic.h"
#include "Misc/App.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	/**
	 * The project's slot registry folder (<DShader>/.dreampass) copied away, and put back however the scope is left. The
	 * slot shaders the editor has loaded follow the files: with a renderer, RewriteDreamPassRegistryFiles writes the
	 * registry files from the restored Registry.json and hot reloads them, and the copy is laid over them once more so that
	 * every file is byte for byte what it was.
	 */
	struct FScopedDreamPassRegistryBackup
	{
		FString Directory;
		FString Backup;
		bool bHadDirectory = false;
		bool bBackedUp = false;

		FScopedDreamPassRegistryBackup()
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			Directory = FPaths::ConvertRelativePathToFull(::UE::DreamPass::GetUserShaderDirectory());
			Backup = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("DreamShaderTests"),
				FString::Printf(TEXT("DreamPassBackup-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			bHadDirectory = PlatformFile.DirectoryExists(*Directory);
			if (bHadDirectory)
			{
				PlatformFile.CreateDirectoryTree(*Backup);
				bBackedUp = PlatformFile.CopyDirectoryTree(*Backup, *Directory, /*bOverwriteAllExisting*/ true);
			}
		}

		/** False when the folder exists and could not be copied: nothing may be committed then. */
		bool IsValid() const { return !bHadDirectory || bBackedUp; }

		~FScopedDreamPassRegistryBackup()
		{
			if (!IsValid())
			{
				return;
			}
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.DeleteDirectoryRecursively(*Directory);
			if (bHadDirectory)
			{
				PlatformFile.CreateDirectoryTree(*Directory);
				PlatformFile.CopyDirectoryTree(*Directory, *Backup, /*bOverwriteAllExisting*/ true);
				if (FApp::CanEverRender())
				{
					int32 Reserved = 0;
					::UE::DreamShader::Lang::FLangDiagnosticSink Ignored;
					::UE::DreamShader::Editor::Compiler::RewriteDreamPassRegistryFiles(Reserved, Ignored);
					PlatformFile.CopyDirectoryTree(*Directory, *Backup, /*bOverwriteAllExisting*/ true);
				}
				PlatformFile.DeleteDirectoryRecursively(*Backup);
			}
		}

		FScopedDreamPassRegistryBackup(const FScopedDreamPassRegistryBackup&) = delete;
		FScopedDreamPassRegistryBackup& operator=(const FScopedDreamPassRegistryBackup&) = delete;
	};
}
