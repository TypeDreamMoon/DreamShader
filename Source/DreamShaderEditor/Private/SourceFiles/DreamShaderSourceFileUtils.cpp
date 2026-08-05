#include "DreamShaderSourceFileUtils.h"

#include "DreamShaderModule.h"

#include "HAL/FileManager.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		void AppendSourceFilesWithExtension(
			const FString& Directory,
			const TCHAR* Wildcard,
			TArray<FString>& OutSourceFiles)
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(
				Files,
				*Directory,
				Wildcard,
				true,
				false,
				false);

			OutSourceFiles.Reserve(OutSourceFiles.Num() + Files.Num());
			for (const FString& File : Files)
			{
				OutSourceFiles.Add(UE::DreamShader::NormalizeSourceFilePath(File));
			}
		}
	}

	bool FDreamShaderSourceFileUtils::IsPathUnderDirectory(const FString& InPath, const FString& InDirectory)
	{
		return UE::DreamShader::IsPathUnderSourceDirectory(InPath, InDirectory);
	}

	bool FDreamShaderSourceFileUtils::IsPackageMaterialFile(const FString& InPath)
	{
		if (!UE::DreamShader::IsDreamShaderMaterialFile(InPath))
		{
			return false;
		}

		// Every root carries its own Packages tree, so "is this a package file" is a question about
		// the root that owns the path, not about the project's Packages folder.
		const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(InPath);
		return Root != nullptr
			&& UE::DreamShader::IsPathUnderSourceDirectory(InPath, Root->PackagesDirectory);
	}

	void FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(TArray<FString>& OutSourceFiles)
	{
		OutSourceFiles.Reset();

		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			TArray<FString> RootSourceFiles;
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsm"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsh"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsf"), RootSourceFiles);

			// This scan drops every file under Packages; the generatable scan below drops only the
			// package .dsm files. The asymmetry is deliberate -- see Docs/language/source-files.md.
			RootSourceFiles.RemoveAll([&Root](const FString& SourceFile)
			{
				return UE::DreamShader::IsPathUnderSourceDirectory(SourceFile, Root.PackagesDirectory);
			});

			OutSourceFiles.Append(MoveTemp(RootSourceFiles));
		}

		OutSourceFiles.Sort();
	}

	void FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(TArray<FString>& OutSourceFiles)
	{
		OutSourceFiles.Reset();

		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsm"), OutSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsf"), OutSourceFiles);
		}

		OutSourceFiles.RemoveAll([](const FString& SourceFile)
		{
			return FDreamShaderSourceFileUtils::IsPackageMaterialFile(SourceFile);
		});
	}
}
