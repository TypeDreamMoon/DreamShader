#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	/** Whether an enumeration takes the sources a package builds (see FDreamShaderSourceFileUtils::IsBuildablePackageFile). */
	enum class EDreamShaderPackageSources : uint8
	{
		/** Nothing under any root's `Packages` tree: for the features that rewrite sources, which never touch a package. */
		Exclude,
		/** A package's `Assets` sources and its asset headers, as if they were the project's own; nothing else under `Packages`. */
		IncludeBuildable,
	};

	struct DREAMSHADERCOMPILER_API FDreamShaderSourceFileUtils
	{
		static bool IsPathUnderDirectory(const FString& InPath, const FString& InDirectory);

		/**
		 * A `.dsh` in a package (not under its `Examples`) that asks for material functions to be generated: it carries a
		 * `/// @generate`, a `#pragma generate(...)` or a 1.x `Generate = ...` attribute. Decided on the file's text, so it
		 * reads the file; the compile decides which functions, if any, the annotations actually reach.
		 */
		static bool IsPackageAssetHeader(const FString& InPath);

		/** A package file the plugin builds: an `Assets` source (UE::DreamShader::IsPackageAssetsSourceFile) or an asset header. */
		static bool IsBuildablePackageFile(const FString& InPath);

		/** Under a root's `Packages` tree and not buildable: a file every build path leaves alone (it is compiled only through what includes it). */
		static bool IsExcludedPackageFile(const FString& InPath);

		/**
		 * A file the compiler builds on its own: any source but a `.dsh`, and a `.dsh` that is a package's asset header.
		 * What a batch (startup, `compile -All`, the cook) keeps after enumerating.
		 */
		static bool IsCompileUnit(const FString& InPath);

		/** Every source of every root, sorted. `Packages` decides what of the package trees is taken. */
		static void FindProjectDreamShaderSourceFiles(TArray<FString>& OutSourceFiles, EDreamShaderPackageSources Packages = EDreamShaderPackageSources::Exclude);

		/** Every file that generates assets (no `.dsh` but a package's asset headers), with the buildable package files. */
		static void FindProjectMaterialSourceFiles(TArray<FString>& OutSourceFiles);
	};
}
