#pragma once

#include "CoreMinimal.h"
#include "DreamShaderVersionCompat.h"
#include "Modules/ModuleManager.h"

class IPlugin;

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
		 * Whether the owning plugin is one this project may generate into -- see
		 * IsProjectContentPlugin. False for an engine or marketplace plugin, whose mount the
		 * plugin-root resolver refuses (gate 3, DSH8090): its sources are still discovered, compiled
		 * and browsable, but the assets they describe ship with that plugin, not with the project
		 * that happens to have it installed. False on the project root, which has no plugin.
		 */
		bool bIsProjectPlugin = false;

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

	/**
	 * Whether assets may be generated into `InPlugin`'s content: it must be a project plugin
	 * (`EPluginType::Project`) whose base directory is under `<Project>/Plugins`.
	 *
	 * Gate 3 of the plugin-root resolver asks exactly this and refuses everything else with DSH8090
	 * (see Docs/generation/asset-paths.md). It is also what separates a source this project owns from
	 * one that merely happens to be enabled: an engine or marketplace plugin's sources describe
	 * assets that plugin ships itself, so a host project reports a failure to generate one instead of
	 * aborting the cook over it -- see FDreamShaderSourceRoot::bIsProjectPlugin.
	 */
	DREAMSHADER_API bool IsProjectContentPlugin(const IPlugin& InPlugin);

	/** Drops the cached root list. Only needed when plugins mount or unmount mid-session. */
	DREAMSHADER_API void RefreshSourceShaderRoots();

	/** True when `InPath` is `InDirectory` itself or sits underneath it. Case-insensitive. */
	DREAMSHADER_API bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);

	/** The project root's directory -- the target of every editor-side write. */
	DREAMSHADER_API FString GetSourceShaderDirectory();

	/** The project root's `Packages` directory. Plugin roots carry their own in `FDreamShaderSourceRoot`. */
	DREAMSHADER_API FString GetPackageShaderDirectory();

	/**
	 * The package `InPath` belongs to: `<Root>/Packages/@scope/name` for a scoped package, `<Root>/Packages/name`
	 * otherwise (absolute, normalized, no trailing slash). Empty for a path under no root's `Packages` tree, and for a
	 * file directly in `Packages` (or directly in an `@scope` folder), which belongs to no package.
	 */
	DREAMSHADER_API FString FindPackageDirectoryForFile(const FString& InPath);

	/** True for a path under any root's `Packages` tree, whether or not it is in a package. */
	DREAMSHADER_API bool IsPackageFilePath(const FString& InPath);

	/**
	 * True for a `.dss`, `.dsf` or `.dsi` under a package's `Assets` folder: a source the plugin builds like the
	 * project's own -- at startup, by `compile -All`, by the cook -- into the package's asset root.
	 */
	DREAMSHADER_API bool IsPackageAssetsSourceFile(const FString& InPath);

	/**
	 * Where a package's assets go when a source names no Root: `/Game/DreamShaderPackages/<scope>/<name>` for a package
	 * of the project's root, `/<Plugin>/DreamShaderPackages/<scope>/<name>` for one under a plugin's root (`@` dropped,
	 * characters a package path cannot hold made `_`). Empty for a path in no package.
	 */
	DREAMSHADER_API FString GetPackageAssetRoot(const FString& InPath);

	/** The folder of `InPath` relative to its package's `Assets` folder (`Noise` for `<pkg>/Assets/Noise/MF_X.dss`); empty otherwise. */
	DREAMSHADER_API FString GetPackageAssetsRelativeDirectory(const FString& InPath);

	DREAMSHADER_API FString GetGeneratedShaderDirectory();
	DREAMSHADER_API FString GetGeneratedShaderVirtualDirectory();
	DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
	DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
	DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
	/** `.dss` -- the 2.0 language. */
	DREAMSHADER_API bool IsDreamShaderLang2File(const FString& InPath);
	/** `.dsi` -- a 2.0 material instance: one `#pragma instance(...)` plus uniform overrides of its parent. */
	DREAMSHADER_API bool IsDreamShaderInstanceFile(const FString& InPath);
	/** `.dsp` -- a Custom Pass pipeline: buffers and passes, one UDreamPassPipeline (the DreamShaderPass module). */
	DREAMSHADER_API bool IsDreamShaderPipelineFile(const FString& InPath);
	/** Any of the six: `.dsm`, `.dsh`, `.dsf`, `.dss`, `.dsi`, `.dsp`. */
	DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
}

class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
