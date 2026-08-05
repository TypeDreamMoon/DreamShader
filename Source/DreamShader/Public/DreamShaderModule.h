#pragma once

#include "CoreMinimal.h"
#include "DreamShaderVersionCompat.h"
#include "Modules/ModuleManager.h"

DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All);

namespace UE::DreamShader
{
	/**
	 * One directory tree that DreamShader source files are discovered under. The project always
	 * contributes a root; every enabled plugin that ships a `DShader` folder contributes one more.
	 *
	 * A root is the unit of import resolution: an import specifier is only ever resolved against the
	 * bases of the root that owns the importing file, never against another root's. Cross-root
	 * imports need an explicit specifier (not implemented yet -- see Docs/language/import.md).
	 */
	struct FDreamShaderSourceRoot
	{
		/** Absolute, normalized, no trailing slash. */
		FString Directory;

		/** `<Directory>/Packages` -- this root's own third-party package tree. */
		FString PackagesDirectory;

		/** Name for editor UI and log messages: `Project`, or the owning plugin's name. */
		FString DisplayName;

		/** Empty for the project root; the owning plugin's name otherwise. */
		FString PluginName;

		/** True for the project root only. */
		bool bIsProjectRoot = false;

		/**
		 * Whether editor features that rewrite source files in place (VirtualFunction sync) may touch
		 * files under this root. Only the project root is writable: plugin roots are discovered and
		 * compiled, but a plugin ships its sources as-is and the editor must not rewrite them.
		 */
		bool bWritable = false;
	};

	/** The project root first, then one root per enabled plugin that has a `DShader` folder. */
	DREAMSHADER_API const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots();

	/** The root owning `InPath`, or null when the path is outside every root. */
	DREAMSHADER_API const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath);

	/** False for files under a read-only root; true for paths outside every root (ad-hoc sources). */
	DREAMSHADER_API bool IsWritableSourceFilePath(const FString& InPath);

	/** Drops the cached root list. Only needed when plugins mount or unmount mid-session. */
	DREAMSHADER_API void RefreshSourceShaderRoots();

	/** True when `InPath` is `InDirectory` itself or sits underneath it. Case-insensitive. */
	DREAMSHADER_API bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);

	/** The project root's directory -- the target of every editor-side write. */
	DREAMSHADER_API FString GetSourceShaderDirectory();

	/** The project root's `Packages` directory. Plugin roots carry their own in `FDreamShaderSourceRoot`. */
	DREAMSHADER_API FString GetPackageShaderDirectory();
	DREAMSHADER_API FString GetGeneratedShaderDirectory();
	DREAMSHADER_API FString GetGeneratedShaderVirtualDirectory();
	DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
	DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
}

class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
