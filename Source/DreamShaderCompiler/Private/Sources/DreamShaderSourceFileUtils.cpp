#include "DreamShaderSourceFileUtils.h"

#include "DreamShaderModule.h"

#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"

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

	bool FDreamShaderSourceFileUtils::IsPackageAssetHeader(const FString& InPath)
	{
		if (!UE::DreamShader::IsDreamShaderHeaderFile(InPath))
		{
			return false;
		}
		const FString PackageDirectory = UE::DreamShader::FindPackageDirectoryForFile(InPath);
		if (PackageDirectory.IsEmpty() || UE::DreamShader::IsPathUnderSourceDirectory(InPath, PackageDirectory + TEXT("/Examples")))
		{
			return false;
		}

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *InPath))
		{
			return false;
		}
		// The three spellings of the annotation. A match inside a comment or a string makes the header a compile unit
		// that generates nothing, which is harmless; the compile is what decides.
		static const FRegexPattern Pattern(TEXT("///[ \\t]*@generate\\b|#[ \\t]*pragma[ \\t]+generate\\b|\\bGenerate[ \\t]*="));
		FRegexMatcher Matcher(Pattern, Text);
		return Matcher.FindNext();
	}

	bool FDreamShaderSourceFileUtils::IsBuildablePackageFile(const FString& InPath)
	{
		return UE::DreamShader::IsPackageAssetsSourceFile(InPath) || IsPackageAssetHeader(InPath);
	}

	bool FDreamShaderSourceFileUtils::IsExcludedPackageFile(const FString& InPath)
	{
		return UE::DreamShader::IsPackageFilePath(InPath) && !IsBuildablePackageFile(InPath);
	}

	bool FDreamShaderSourceFileUtils::IsCompileUnit(const FString& InPath)
	{
		if (!UE::DreamShader::IsDreamShaderSourceFile(InPath))
		{
			return false;
		}
		return !UE::DreamShader::IsDreamShaderHeaderFile(InPath) || IsPackageAssetHeader(InPath);
	}

	void FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(TArray<FString>& OutSourceFiles, const EDreamShaderPackageSources Packages)
	{
		OutSourceFiles.Reset();

		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			TArray<FString> RootSourceFiles;
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsm"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsh"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsf"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dss"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsi"), RootSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsp"), RootSourceFiles);

			// A package is somebody else's source: only what it asks to have built joins in, and only for the callers
			// that build. See Docs/tools/packages.md.
			RootSourceFiles.RemoveAll([&Root, Packages](const FString& SourceFile)
			{
				if (!UE::DreamShader::IsPathUnderSourceDirectory(SourceFile, Root.PackagesDirectory))
				{
					return false;
				}
				return Packages == EDreamShaderPackageSources::Exclude || !IsBuildablePackageFile(SourceFile);
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
			// A `.dss`, a `.dsi` or a `.dsp` generates assets like a `.dsm`/`.dsf` does. A `.dsh` does only as a package's
			// asset header, so headers are looked for in the package tree alone.
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dss"), OutSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsi"), OutSourceFiles);
			AppendSourceFilesWithExtension(Root.Directory, TEXT("*.dsp"), OutSourceFiles);
			if (IFileManager::Get().DirectoryExists(*Root.PackagesDirectory))
			{
				AppendSourceFilesWithExtension(Root.PackagesDirectory, TEXT("*.dsh"), OutSourceFiles);
			}
		}

		OutSourceFiles.RemoveAll([](const FString& SourceFile)
		{
			return FDreamShaderSourceFileUtils::IsExcludedPackageFile(SourceFile);
		});
	}
}
