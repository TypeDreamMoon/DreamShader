# DreamShaderModule.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderModule.h**

The runtime module entry point, the plugin's log category, and the path, identifier and
file-classification helpers every other module builds on.

Defined in header `DreamShaderModule.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderModule.h"` |
| Namespace | `UE::DreamShader` (free functions) · global scope (`LogDreamShader`, `FDreamShaderModule`) |
| Export macro | `DREAMSHADER_API` |
| Pulls in | `CoreMinimal.h`, `DreamShaderVersionCompat.h`, `Modules/ModuleManager.h` |

## Synopsis

```cpp
#include "CoreMinimal.h"
#include "DreamShaderVersionCompat.h"
#include "Modules/ModuleManager.h"

DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All);

namespace UE::DreamShader
{
    struct FDreamShaderSourceRoot
    {
        FString Directory;          // absolute, normalized, no trailing slash
        FString PackagesDirectory;  // <Directory>/Packages
        FString DisplayName;        // "Project", or the owning plugin's name
        FString PluginName;         // empty for the project root
        bool bIsProjectRoot = false;
        bool bWritable = false;     // true for the project root only
    };

    DREAMSHADER_API const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots();
    DREAMSHADER_API const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath);
    DREAMSHADER_API bool IsWritableSourceFilePath(const FString& InPath);
    DREAMSHADER_API void RefreshSourceShaderRoots();
    DREAMSHADER_API bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);

    DREAMSHADER_API FString GetSourceShaderDirectory();
    DREAMSHADER_API FString GetPackageShaderDirectory();
    DREAMSHADER_API FString GetGeneratedShaderDirectory();
    DREAMSHADER_API FString GetGeneratedShaderVirtualDirectory();
    DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
    DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderLang2File(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderInstanceFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderPipelineFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
}

class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

## `LogDreamShader`

| Aspect | Value |
| :-- | :-- |
| Declaration | `DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All)` |
| Default runtime verbosity | `Log` |
| Compile-time maximum verbosity | `All` |
| Defined in | `DreamShader` (Runtime) |
| Used by | `DreamShader`, `DreamShaderCompiler` and `DreamShaderEditor`. **Not** `DreamShaderLang`: it depends on `Core` alone and never logs, it reports through `FLangDiagnosticSink` instead. **Not** `DreamShaderPass`, which has its own `LogDreamPass` |

Every message the compiler and the editor log — diagnostics surfaced to the log, generation results,
cook progress, bridge and SQLite warnings — goes through this one category. Raise it from a config
file or the console:

```text
[Core.Log]
LogDreamShader=Verbose
```

## Directory functions

```cpp
FString GetSourceShaderDirectory();
FString GetPackageShaderDirectory();
FString GetGeneratedShaderDirectory();
FString GetGeneratedShaderVirtualDirectory();
```

| Function | Returns | Value when the setting is unset |
| :-- | :-- | :-- |
| `GetSourceShaderDirectory()` | The absolute, normalized **project** source root — `UDreamShaderSettings::SourceDirectory.Path` resolved against the project directory. The target of every editor-side write; plugin roots are under [Source roots](#source-roots) | `<Project>/DShader` |
| `GetPackageShaderDirectory()` | `<source root>/Packages` of the project root, always derived from `GetSourceShaderDirectory()`; there is no separate setting | `<Project>/DShader/Packages` |
| `GetGeneratedShaderDirectory()` | The real directory currently registered for the virtual mount `/DreamShaderGenerated`; **only when no such mapping exists** does it fall back to the configured `GeneratedShaderDirectory` | `<Project>/Intermediate/DreamShader/GeneratedShaders` |
| `GetGeneratedShaderVirtualDirectory()` | The compile-time constant `TEXT("/DreamShaderGenerated")` | n/a |

Resolution of a configured path, in order:

| Step | Rule |
| :-- | :-- |
| 1 | Trim the configured path. If it is empty, substitute the hard-coded default. |
| 2 | If the result is relative, combine it with `FPaths::ProjectDir()`. |
| 3 | `FPaths::NormalizeFilename`, then `FPaths::MakeStandardFilename`. |

The three configured paths are held in one internal cache. It is refreshed when it has not been
initialized **or** when the settings object is currently readable — that is,
`UObjectInitialized() && !GExitPurge && !IsEngineExitRequested()`. Once the UObject system is live
this condition is always true, so the settings are re-read on **every** call and a mid-session change
in Project Settings takes effect immediately. Before UObjects exist, and during exit purge, the
getters do not touch `GetDefault<UDreamShaderSettings>()`: they read `SourceDirectory` and
`GeneratedShaderDirectory` straight from `[/Script/DreamShader.DreamShaderSettings]` in the engine
ini — the values the settings object will load — and fall back to the defaults when a key is absent.
The module starts at `PostConfigInit`, before the settings object can exist, which is why.

> [!WARNING]
> `GetGeneratedShaderDirectory()` is not simply the *Generated Shader Directory* setting.
> `StartupModule` registers the `/DreamShaderGenerated` mapping only if it is absent, and never
> re-points it. Changing the setting mid-session therefore changes where `StartupModule` *would*
> have mounted, and changes the internal cached value, but `GetGeneratedShaderDirectory()` keeps
> returning the directory that was mounted at startup. Restart the editor to move the mount. See
> [Generated HLSL](../generation/generated-hlsl.md).

> [!NOTE]
> The cache is a plain static with no lock. Treat all four functions as game-thread APIs even
> though nothing in them is intrinsically thread-affine.

## Source roots

*(since 1.6.0)*

```cpp
const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots();
const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath);
bool IsWritableSourceFilePath(const FString& InPath);
void RefreshSourceShaderRoots();
bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);
```

A source root is one directory tree sources are discovered under, and the unit of import
resolution: a file's imports resolve against its own root and that root's `Packages` folder only.

| Function | Behaviour |
| :-- | :-- |
| `GetSourceShaderRoots()` | The project root first, then — when *Scan Plugin Source Directories* is on — one root per enabled plugin that has a `DShader` folder. A plugin root that overlaps an earlier root (one contains the other) is dropped with a `LogDreamShader` warning. Cached; rebuilt when the project source directory or the scan toggle changes. The reference is invalidated by the next rebuild, so use it on the game thread and do not keep it |
| `FindSourceRootForFile(InPath)` | The root that contains `InPath`, or `nullptr` when it is under none |
| `IsWritableSourceFilePath(InPath)` | `false` for a file under a plugin root; `true` under the project root **and** for a path under no root (an ad-hoc source named explicitly, such as a commandlet `-Source`) |
| `RefreshSourceShaderRoots()` | Drops the cache. Needed only when plugins mount or unmount mid-session |
| `IsPathUnderSourceDirectory(InPath, InDirectory)` | `true` when the path is the directory itself or below it; both are normalized with `NormalizeSourceFilePath` and compared case-insensitively |

Only the project root is writable: editor features that rewrite sources in place (VirtualFunction
sync, [asset rename sync](../tools/asset-rename-sync.md)) never touch a plugin root.

## `SanitizeIdentifier`

```cpp
DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
```

Rewrites arbitrary text into something usable as an HLSL or asset identifier. Pure; touches no
engine state; callable from any thread.

| Step | Rule |
| :-- | :-- |
| 1 | Replace every character outside `[A-Za-z0-9_]` with `_`. |
| 2 | If the result is empty, return `DreamShaderSymbol`. |
| 3 | If the result consists only of underscores, return `DreamShaderSymbol`. |
| 4 | If the first character is not in `[A-Za-z_]` — i.e. it is a digit — prepend `_`. |
| 5 | Collapse every run of consecutive underscores to a single `_`, scanning right to left. |

| Input | Result |
| :-- | :-- |
| `Noise::Perlin` | `Noise_Perlin` |
| `2Fast` | `_2Fast` |
| `a  b` | `a_b` |
| `___` | `DreamShaderSymbol` |
| `""` | `DreamShaderSymbol` |
| `Ünlit` | `_nlit` |

> [!WARNING]
> Character classification is done against raw ASCII ranges. Non-ASCII letters are **replaced**, not
> preserved — `Ünlit` becomes `_nlit`, not `Ünlit`. Use ASCII identifiers in namespace and function
> names.

Callers inside the plugin: the transient ThinCustom base-material name in the emitter
(`MB_DreamThinBase_<package>`), the output named-reroute names (`DS_<Name>_<Index>`), and the
`MI_<Name>` stem of a new instance's `.dsi` file in the instance factory and the provenance actions.
`DreamShaderLang` cannot
link this module, so the legacy front end (flattening `A::B` to `A_B`) and the custom-HLSL builder
(`DreamShaderFn_<Name>`) each keep a copy of the same rule *(since 2.0.0)*.

## `NormalizeSourceFilePath`

```cpp
DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
```

Applies `FPaths::ConvertRelativePathToFull`, then `FPaths::NormalizeFilename`, then
`FPaths::MakeStandardFilename`. The result is an absolute path with `/` separators.

This is **the** canonical key for a source file. The bridge, the diagnostics store, the compiler and
the commandlet all normalize before comparing paths, so a path produced by this function compares
equal to the one the plugin stores internally.

## The four normalizers compared

Four differently-behaving functions have similar names. They are not interchangeable.

| Function | Header | Operation | Typical subject |
| :-- | :-- | :-- | :-- |
| `UE::DreamShader::NormalizeSourceFilePath` | `DreamShaderModule.h` | absolute path, `/` separators | a `.dsm` / `.dsf` / `.dsh` file path |
| `UE::DreamShader::SanitizeIdentifier` | `DreamShaderModule.h` | non-`[A-Za-z0-9_]` → `_`, digit-leading fix, underscore-run collapse | an HLSL or asset identifier |
| `UE::DreamShader::NormalizeSettingKey` | [`DreamShaderTypes.h`](types.md#normalizesettingkey) | trim, then lower-case — **nothing else** | a `Settings` / `Options` key |
| `UDreamShaderSettings::NormalizeMappingKey` | [`DreamShaderSettings.h`](settings.md#normalizemappingkey) | trim, lower-case, then strip every space, `_` and `-` | a `ShadingModel` / `BlendMode` / `Domain` **value** alias |

The last two are the pair most often confused. `NormalizeSettingKey` keeps spaces and punctuation,
so the setting keys `Two Sided` and `TwoSided` are **different** keys. `NormalizeMappingKey` removes
them, so the values `Two Sided Foliage`, `TwoSidedFoliage` and `two-sided_foliage` all resolve to
the same shading model.

## File-classification predicates

```cpp
DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderLang2File(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderInstanceFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderPipelineFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
```

| Function | Returns `true` when the extension is |
| :-- | :-- |
| `IsDreamShaderMaterialFile` | `.dsm` |
| `IsDreamShaderHeaderFile` | `.dsh` |
| `IsDreamShaderFunctionFile` | `.dsf` |
| `IsDreamShaderLang2File` | `.dss` — a 2.0 compilation unit |
| `IsDreamShaderInstanceFile` | `.dsi` — a 2.0 material instance |
| `IsDreamShaderPipelineFile` *(since 2.1.0)* | `.dsp` — a [Custom Pass pipeline](../language-v2/passes.md) |
| `IsDreamShaderSourceFile` | any of the six: `.dsm`, `.dsh`, `.dsf`, `.dss`, `.dsi` **or** `.dsp` *(`.dsp` since 2.1.0)* |

`IsDreamShaderSourceFile` is the one predicate everything that scans, watches or lists sources asks — the
startup scan, the source-directory watcher, `compile -All`, the Material Content Browser — so a `.dsp` is a
source to all of them; only the compiler tells the kinds apart. All seven compare the result of
`FPaths::GetExtension(InPath, /*bIncludeDot*/ true)` with `ESearchCase::IgnoreCase`, so **extension
matching is case-insensitive**: `Foo.DSM` classifies as a material file. Only the extension is examined —
the file need not exist and its contents are never read. Pure; any thread.

## `FDreamShaderModule`

```cpp
class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

Registered with `IMPLEMENT_MODULE(FDreamShaderModule, DreamShader)`. The class exposes no state and
no accessors; obtain it, if you need to at all, with
`FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"))`.

### `StartupModule()`

Runs on the game thread when the module loads, in the `PostConfigInit` phase — before any material
loads, so that a material whose cached include paths name `/DreamShaderGenerated` finds the mapping
registered. It performs five side effects in order.

| # | Action | Notes |
| :-- | :-- | :-- |
| 1 | Create `GetSourceShaderDirectory()` as a directory tree | `<Project>/DShader` by default |
| 2 | Create `GetPackageShaderDirectory()` as a directory tree | `<Project>/DShader/Packages` by default |
| 3 | Create the **configured** generated-shader directory as a directory tree | uses the configured path, not `GetGeneratedShaderDirectory()` |
| 4 | If `/DreamShaderGenerated` is absent from `AllShaderSourceDirectoryMappings()`, map it to the configured generated-shader directory | never re-points an existing mapping |
| 5 | If the `DreamShader` plugin is found through `IPluginManager` and `/Plugin/DreamShader` is absent from the mappings, map it to `<PluginBaseDir>/Shaders` | never re-points an existing mapping |

The plugin's `Shaders/` folder holds `DreamShaderBuiltins.ush`, addressable as
`/Plugin/DreamShader/DreamShaderBuiltins.ush` (see
[`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md)), and, *(since 2.1.0)*, the Custom Pass
shaders under `Pass/` (`/Plugin/DreamShader/Pass/…`).

Steps 1–3 mean the source and package directories exist from the first editor launch, whether or not
the user has authored anything.

### `ShutdownModule()`

Empty.

> [!NOTE]
> The two virtual shader directory mappings registered by `StartupModule` are **never
> unregistered**. In an editor session that unloads and reloads the module, step 4 and step 5 are
> both no-ops the second time round, and the mounts keep pointing at whatever they were first given.

## Notes

- `DreamShaderVersionCompat.h` is included by this header, so anything that includes
  `DreamShaderModule.h` also gets the thirteen version macros. See
  [`DreamShaderVersionCompat.h`](version-compat.md).
- None of the free functions logs, throws or asserts. Failures are expressed as a fallback value —
  the default path, `DreamShaderSymbol`, or `false`.
- This header declares no delegates. The notification that a compile finished is
  `OnDreamShaderSourceGenerated` in the `DreamShaderCompiler` module — see
  [Compiler module](compiler-module.md); tools outside the editor read the bridge's diagnostics
  files instead — see [Editor bridge](../tools/bridge.md).

## Example

```cpp
#include "DreamShaderModule.h"
#include "HAL/FileManager.h"

using namespace UE::DreamShader;

/** Enumerate every DreamShaderLang source under the configured source root. */
void CollectProjectSources(TArray<FString>& OutFiles)
{
    const FString Root = GetSourceShaderDirectory();

    TArray<FString> Found;
    IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("*.*"), /*Files*/ true, /*Dirs*/ false);

    for (const FString& File : Found)
    {
        if (IsDreamShaderSourceFile(File))
        {
            OutFiles.Add(NormalizeSourceFilePath(File));
        }
    }

    UE_LOG(LogDreamShader, Display, TEXT("Found %d DreamShader source(s) under %s"), OutFiles.Num(), *Root);
}
```

Typical output with default settings:

```text
LogDreamShader: Found 3 DreamShader source(s) under I:/Project/DShader
```

A compile writes no helper include *(since 2.0.0)*: an HLSL function becomes the code of a Custom
node, with the HLSL functions it calls embedded in that code. The `/DreamShaderGenerated` mapping is still registered at startup, and
*Clean Generated Shaders* still empties `GetGeneratedShaderDirectory()` of `*.ush` files.

## See also

- [C++ API](index.md) — modules, headers, and linkage
- [`DreamShaderTypes.h`](types.md) — `NormalizeSettingKey` and the parsed-source data model
- [`DreamShaderSettings.h`](settings.md) — `SourceDirectory`, `GeneratedShaderDirectory`, `NormalizeMappingKey`
- [`DreamShaderVersionCompat.h`](version-compat.md) — the macros this header pulls in
- [`DreamShaderCompiler`](compiler-module.md) — requesting a compile from C++
- [`DreamShaderLang`](lang-module.md) — the Core-only front end. `DreamShaderPreprocessor.h`,
  `DreamShaderDefineTable.h` and `DreamShaderDiagnostic.h` moved into it in 2.0; the include
  spellings did not change. Only `DreamShaderDefineResolution.h` — the tier merge that reads
  project settings, the engine version, the plugin descriptor and the command line — stayed in
  this module
- [Project settings](../settings/project.md) — the two directory settings from the user's side
- [Generated HLSL](../generation/generated-hlsl.md) — what `/DreamShaderGenerated` receives
- [Packages](../tools/packages.md) — the `DShader/Packages` directory
- [Source files](../language/source-files.md) — what the three extensions may contain
- [`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md) — the header mounted at `/Plugin/DreamShader`
