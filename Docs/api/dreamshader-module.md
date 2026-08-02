# DreamShaderModule.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderModule.h**

The runtime module entry point, the plugin's log category, and the ten path, identifier and
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
    DREAMSHADER_API FString GetSourceShaderDirectory();
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
```

## `LogDreamShader`

| Aspect | Value |
| :-- | :-- |
| Declaration | `DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All)` |
| Default runtime verbosity | `Log` |
| Compile-time maximum verbosity | `All` |
| Defined in | `DreamShader` (Runtime) |
| Used by | all three modules — `DreamShaderEditor` includes this header solely for the category |

Every message the plugin emits — parse failures surfaced to the log, generation results, cook
progress, bridge and SQLite warnings — goes through this one category. Raise it from a config file
or the console:

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
| `GetSourceShaderDirectory()` | The absolute, normalized source root — `UDreamShaderSettings::SourceDirectory.Path` resolved against the project directory | `<Project>/DShader` |
| `GetPackageShaderDirectory()` | `<source root>/Packages`, always derived from `GetSourceShaderDirectory()`; there is no separate setting | `<Project>/DShader/Packages` |
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
getters return the hard-coded defaults without touching `GetDefault<UDreamShaderSettings>()`.

> [!WARNING]
> `GetGeneratedShaderDirectory()` is not simply the *Generated Shader Directory* setting.
> `StartupModule` registers the `/DreamShaderGenerated` mapping only if it is absent, and never
> re-points it. Changing the setting mid-session therefore changes where `StartupModule` *would*
> have mounted, and changes the internal cached value, but `GetGeneratedShaderDirectory()` keeps
> returning the directory that was mounted at startup. Restart the editor to move the generated
> `.ush` output. See [Generated HLSL](../generation/generated-hlsl.md).

> [!NOTE]
> The cache is a plain static with no lock. Treat all four functions as game-thread APIs even
> though nothing in them is intrinsically thread-affine.

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

Callers inside the plugin: namespace-qualified identifier rewriting in the parser, transient
ThinCustom base-material naming in the generator, and the VirtualFunction service.

## `NormalizeSourceFilePath`

```cpp
DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
```

Applies `FPaths::ConvertRelativePathToFull`, then `FPaths::NormalizeFilename`, then
`FPaths::MakeStandardFilename`. The result is an absolute path with `/` separators.

This is **the** canonical key for a source file. The bridge, the diagnostics store, the generator and
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
DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
```

| Function | Returns `true` when the extension is |
| :-- | :-- |
| `IsDreamShaderMaterialFile` | `.dsm` |
| `IsDreamShaderHeaderFile` | `.dsh` |
| `IsDreamShaderFunctionFile` | `.dsf` |
| `IsDreamShaderSourceFile` | `.dsm`, `.dsh` **or** `.dsf` |

All four compare the result of `FPaths::GetExtension(InPath, /*bIncludeDot*/ true)` with
`ESearchCase::IgnoreCase`, so **extension matching is case-insensitive**: `Foo.DSM` classifies as a
material file. Only the extension is examined — the file need not exist and its contents are never
read. Pure; any thread.

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

Runs on the game thread at module-load time and performs five side effects in order.

| # | Action | Notes |
| :-- | :-- | :-- |
| 1 | Create `GetSourceShaderDirectory()` as a directory tree | `<Project>/DShader` by default |
| 2 | Create `GetPackageShaderDirectory()` as a directory tree | `<Project>/DShader/Packages` by default |
| 3 | Create the **configured** generated-shader directory as a directory tree | uses the configured path, not `GetGeneratedShaderDirectory()` |
| 4 | If `/DreamShaderGenerated` is absent from `AllShaderSourceDirectoryMappings()`, map it to the configured generated-shader directory | never re-points an existing mapping |
| 5 | If the `DreamShader` plugin is found through `IPluginManager` and `/Plugin/DreamShader` is absent from the mappings, map it to `<PluginBaseDir>/Shaders` | never re-points an existing mapping |

The plugin's `Shaders/` folder holds exactly one file, `DreamShaderBuiltins.ush`, which becomes
addressable as `/Plugin/DreamShader/DreamShaderBuiltins.ush`. See
[`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md).

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
  `DreamShaderModule.h` also gets the six version macros. See
  [`DreamShaderVersionCompat.h`](version-compat.md).
- None of the free functions logs, throws or asserts. Failures are expressed as a fallback value —
  the default path, `DreamShaderSymbol`, or `false`.
- The module declares no delegates, so there is no notification when a compile finishes. Editor code
  observes results through the bridge's diagnostics files instead. See
  [Editor bridge](../tools/bridge.md).

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

The generated `.ush` helper include written by a compile of any of those files lands under
`GetGeneratedShaderDirectory()` and is referenced from generated Custom nodes as
`/DreamShaderGenerated/<Name>_<hash>.ush`.

## See also

- [C++ API](index.md) — modules, headers, and linkage
- [`DreamShaderTypes.h`](types.md) — `NormalizeSettingKey` and the parsed-source data model
- [`DreamShaderSettings.h`](settings.md) — `SourceDirectory`, `GeneratedShaderDirectory`, `NormalizeMappingKey`
- [`DreamShaderVersionCompat.h`](version-compat.md) — the macros this header pulls in
- [`DreamShaderCompiler`](compiler-module.md) — requesting a compile from C++
- [Project settings](../settings/project.md) — the two directory settings from the user's side
- [Generated HLSL](../generation/generated-hlsl.md) — what `/DreamShaderGenerated` receives
- [Packages](../tools/packages.md) — the `DShader/Packages` directory
- [Source files](../language/source-files.md) — what the three extensions may contain
- [`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md) — the header mounted at `/Plugin/DreamShader`
