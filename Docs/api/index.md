# C++ API

> [DreamShader](../index.md) » **C++ API**

The plugin's public C++ surface: three modules, nine public headers, and one interface a third party
can implement.

| | |
| :-- | :-- |
| Modules | `DreamShader` (Runtime) · `DreamShaderCompiler` (Runtime) · `DreamShaderEditor` (Editor) |
| Public headers | 6 + 3 + 0 |
| Export macros | `DREAMSHADER_API`, `DREAMSHADERCOMPILER_API` |
| Reflected types in public headers | 2 `UCLASS`, 1 `UENUM` |
| Delegates | none — the plugin declares no `DECLARE_DELEGATE*`, `DECLARE_EVENT*` or `DECLARE_DYNAMIC*` |
| Plugin version | `1.5.1` (`"Version": 151`) |

## Modules

| Module | Type | Loading phase | Public headers | Export macro | Purpose |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader` | `Runtime` | `Default` | 6 | `DREAMSHADER_API` | Log category, canonical path helpers, the parsed-source data model, the parser, the project settings object, the generated instance class, and the engine-version macros. |
| `DreamShaderCompiler` | `Runtime` | `Default` | 3 | `DREAMSHADERCOMPILER_API` | A pure abstraction layer: the compile request/result structs, the `IDreamShaderCompiler` interface, and a thin service wrapper. Contains no material-generation code. |
| `DreamShaderEditor` | `Editor` | `Default` | **0** | *(none used)* | Everything that actually builds assets: the generator, the decompiler, the bridge, the preview, the Material Content Browser, the commandlet, the workspace exporter. |

All three are declared in `DreamShader.uplugin` and load at the `Default` phase. The plugin is
`EnabledByDefault` and `CanContainContent`; `IsBetaVersion` is `false`.

## Public headers

| Header | Module | Include | Purpose |
| :-- | :-- | :-- | :-- |
| `DreamShaderModule.h` | `DreamShader` | `#include "DreamShaderModule.h"` | `LogDreamShader`, `FDreamShaderModule`, ten exported free functions (paths, identifier sanitizing, file classification). |
| `DreamShaderTypes.h` | `DreamShader` | `#include "DreamShaderTypes.h"` | The parsed-AST data model: 13 structs, 5 enums, `LexToString`, `NormalizeSettingKey`. |
| `DreamShaderParser.h` | `DreamShader` | `#include "DreamShaderParser.h"` | `FTextShaderParser::Parse` — the one entry point into the DreamShaderLang front end. |
| `DreamShaderSettings.h` | `DreamShader` | `#include "DreamShaderSettings.h"` | `EDreamShaderDefaultBackend`, `UDreamShaderSettings`, the enum-alias resolvers. |
| `DreamShaderMaterialInstance.h` | `DreamShader` | `#include "DreamShaderMaterialInstance.h"` | `UDreamShaderMaterialInstance` — the asset the ThinCustom backend produces. |
| `DreamShaderVersionCompat.h` | `DreamShader` | `#include "DreamShaderVersionCompat.h"` | Six engine-version macros. No types, no functions. |
| `DreamShaderCompilerInterfaces.h` | `DreamShaderCompiler` | `#include "DreamShaderCompilerInterfaces.h"` | `FDreamShaderCompileRequest`, `FDreamShaderCompileResult`, `IDreamShaderCompiler`. |
| `DreamShaderCompileService.h` | `DreamShaderCompiler` | `#include "DreamShaderCompileService.h"` | `FDreamShaderCompileService` — argument-packing wrapper over an `IDreamShaderCompiler&`. |
| `DreamShaderCompilerModule.h` | `DreamShaderCompiler` | `#include "DreamShaderCompilerModule.h"` | `FDreamShaderCompilerModule`. Both lifecycle methods are empty. |

Namespaces: everything in `DreamShaderModule.h`, `DreamShaderTypes.h` and `DreamShaderParser.h` lives
in `UE::DreamShader`. `DreamShaderCompilerInterfaces.h` and `DreamShaderCompileService.h` live in
`UE::DreamShader::Compiler`. `FDreamShaderModule`, `FDreamShaderCompilerModule`,
`UDreamShaderSettings`, `UDreamShaderMaterialInstance` and `EDreamShaderDefaultBackend` are at global
scope.

## Linkage

> [!WARNING]
> **`DreamShaderEditor` exports nothing and cannot be linked against.** It has no `Public/` folder at
> all, its `Build.cs` declares no `PublicDependencyModuleNames`, and `DREAMSHADEREDITOR_API` — which
> UnrealBuildTool defines for every module — has zero occurrences anywhere in the plugin's source.
> Its module class `FDreamShaderEditorModule` is declared inside the `.cpp`, not in a header, so it
> cannot be named from outside either. Everything the editor module does is reachable only through
> UE's module system, through UI actions, through the [commandlet](../tools/commandlet.md), or
> through the [`IDreamShaderCompiler`](compiler-module.md#idreamshadercompiler) abstraction it
> registers itself against.

| Module | `DREAMSHADER_API` | `DREAMSHADERCOMPILER_API` | `DREAMSHADEREDITOR_API` |
| :-- | :-- | :-- | :-- |
| `DreamShader` | `DLLEXPORT` | — | — |
| `DreamShaderCompiler` | `DLLIMPORT` | `DLLEXPORT` | — |
| `DreamShaderEditor` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` *(defined, never used)* |

`UE_PLUGIN_NAME` is `"DreamShader"` for all three modules.

Both `DreamShaderCompilerInterfaces.h` and `DreamShaderCompilerModule.h` guard their export macro
with `#ifndef DREAMSHADERCOMPILER_API` / `#define DREAMSHADERCOMPILER_API` / `#endif`, so those two
headers compile outside a UnrealBuildTool context. The `DreamShader` headers do not do this and
require the UBT-generated definitions.

### There are no module singletons

The plugin declares no `Get()`, `IsAvailable()` or `GetChecked()` accessor on either module class.
Use the stock module manager:

```cpp
FDreamShaderModule& Module = FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"));
```

In practice nothing is needed from the module object itself — the exported free functions in
`UE::DreamShader` are usable as soon as the module is loaded.

## Build dependencies

### `DreamShader.Build.cs`

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`, `CoreUObject`, `DeveloperSettings`, `Engine`, `Projects`, `RenderCore` |
| `PrivateDependencyModuleNames` | *(none declared)* |
| `PublicDefinitions` / `PrivateDefinitions` | *(none)* |

| Dependency | Needed for |
| :-- | :-- |
| `Core` | `FString`, `FPaths`, `IFileManager`, the log category |
| `CoreUObject` | `UObject`, `UObjectInitialized()`, the two `UCLASS`es |
| `DeveloperSettings` | `UDeveloperSettings`, the base of `UDreamShaderSettings` |
| `Engine` | `Engine/EngineTypes.h`, `MaterialDomain.h`, `UMaterialInstanceConstant` |
| `Projects` | `IPluginManager` — shader-mount lookup and `Path(Plugin.X, …)` resolution |
| `RenderCore` | `ShaderCore.h`'s `AllShaderSourceDirectoryMappings` / `AddShaderSourceDirectoryMapping` |

### `DreamShaderCompiler.Build.cs`

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`, `CoreUObject`, `DreamShader`, `Engine` |
| `PrivateDependencyModuleNames` | *(none declared)* |

`DreamShader` is a **public** dependency, so a module that lists `DreamShaderCompiler` transitively
gets the runtime headers as well. `CoreUObject` and `Engine` are declared but not required by the
three public headers, which use only `FString`.

### `DreamShaderEditor.Build.cs`

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | **none** |
| `PrivateDependencyModuleNames` | 25, listed below |

| # | Module | Used for |
| :-: | :-- | :-- |
| 1 | `ApplicationCore` | external-editor launch and clipboard, from the workspace service |
| 2 | `AssetRegistry` | `FAssetRegistryModule` — asset creation/removal broadcasts from the bridge |
| 3 | `AssetTools` | `FAssetToolsModule` — the material-instance factory |
| 4 | `ContentBrowser` | `FContentBrowserModule` — the Project page and the folder picker |
| 5 | `Core` | — |
| 6 | `CoreUObject` | — |
| 7 | `DirectoryWatcher` | `FDirectoryWatcherModule` — the source-directory watcher behind auto-compile-on-save |
| 8 | `DreamShader` | parser, types, settings, log category |
| 9 | `DreamShaderCompiler` | `IDreamShaderCompiler`, `FDreamShaderCompileService` |
| 10 | `Engine` | materials, packages |
| 11 | `InputCore` | Slate input |
| 12 | `Json` | bridge manifests and diagnostics files |
| 13 | `MaterialEditor` | `UMaterialEditingLibrary`, graph layout |
| 14 | `Projects` | `IPluginManager` |
| 15 | `RHI` | preview render target, `EShaderPlatform` enumeration |
| 16 | `RenderCore` | shader directory mappings |
| 17 | `Renderer` | preview rendering |
| 18 | `Slate` | UI |
| 19 | `SlateCore` | UI |
| 20 | `SQLiteCore` | the bridge database — see below |
| 21 | `ToolMenus` | `UToolMenus` menu and toolbar extensions |
| 22 | `ToolWidgets` | search box and filter widgets |
| 23 | `UnrealEd` | commandlet base, editor subsystems, `FScopedSlowTask` |
| 24 | `WebSocketNetworking` | the live-preview server — see below |
| 25 | `WorkspaceMenuStructure` | the tab-spawner category for the Material Content Browser |

## Plugin dependencies

`DreamShader.uplugin` enables exactly two other plugins. Both are used only by `DreamShaderEditor`.

| Plugin | Enabled | Where it is actually used |
| :-- | :-- | :-- |
| `WebSocketNetworking` | yes | One place: the live-preview WebSocket server. The editor bridge loads `IWebSocketNetworkingModule` by name, starts a server on port **17864**, and streams preview frames as raw binary WebSocket frames carrying a `[4-byte length][1-byte type tag][payload]` envelope. See [Editor bridge](../tools/bridge.md). |
| `SQLiteCore` | yes | Two translation units, one file: `<Project>/Saved/DreamShader/Bridge/bridge.db`. The workspace service creates the schema and writes the settings-alias, material-expression and Substrate-builtin tables; the diagnostics store writes the `diagnostics` table. The database is a **cache for the VSCode and Rider language extensions** — nothing in the plugin reads it back. See [Workspace](../tools/workspace.md). |

## Pages

| Page | Covers |
| :-- | :-- |
| [`DreamShaderModule.h`](dreamshader-module.md) | The module class, `LogDreamShader`, startup shader mounts, and every exported free function |
| [`DreamShaderTypes.h`](types.md) | Every struct and enum in the parsed-source data model |
| [`DreamShaderParser.h`](parser.md) | `FTextShaderParser::Parse` — contract, diagnostics, thread requirements |
| [`DreamShaderSettings.h`](settings.md) | `UDreamShaderSettings`, `EDreamShaderDefaultBackend`, and the alias catalogues |
| [`DreamShaderMaterialInstance.h`](material-instance.md) | `UDreamShaderMaterialInstance` and its two overrides |
| [`DreamShaderVersionCompat.h`](version-compat.md) | The six macros and every version-gated behaviour they select |
| [`DreamShaderCompiler`](compiler-module.md) | The compile interface, request/result structs, service, and module |

## Notes

- **Nothing in the public API generates assets.** Generation is editor-only and lives entirely in
  `DreamShaderEditor`. The supported way to trigger it from C++ is
  [`FDreamShaderCompileService`](compiler-module.md) over the adapter the editor module implements.
- **The parser is engine-independent.** `FTextShaderParser::Parse` is string processing; it creates
  no `UObject`s and loads no assets. It is the only part of the pipeline usable outside the editor.
- Only two `UCLASS`es are exported: `UDreamShaderSettings` and `UDreamShaderMaterialInstance`. A
  third, `UDreamShaderCommandlet`, exists in the editor module's `Private/` folder with **no** API
  macro, so it is reachable only through UE's reflection system and `-run=DreamShader`.
- `Config/FilterPlugin.ini` contains only the stock commented template — the plugin declares no
  extra packaged files.

## Example

A consuming game or plugin module that wants to parse DreamShaderLang and request compiles:

```csharp
// MyTooling.Build.cs
PrivateDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "DreamShader",          // parser, types, settings
    "DreamShaderCompiler"   // IDreamShaderCompiler, FDreamShaderCompileService
});
```

```cpp
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"

using namespace UE::DreamShader;

void InspectSource(const FString& InPath)
{
    if (!IsDreamShaderSourceFile(InPath))
    {
        return;
    }

    FString SourceText;
    if (!FFileHelper::LoadFileToString(SourceText, *NormalizeSourceFilePath(InPath)))
    {
        return;
    }

    FTextShaderDefinition Definition;
    FString Error;
    if (FTextShaderParser::Parse(SourceText, Definition, Error))
    {
        UE_LOG(LogDreamShader, Display, TEXT("%s declares material '%s' and %d function asset(s)."),
            *InPath, *Definition.Name, Definition.MaterialFunctions.Num());
    }
    else
    {
        UE_LOG(LogDreamShader, Error, TEXT("%s: %s"), *InPath, *Error);
    }
}
```

`Parse` receives text that has **not** had its `import` directives expanded here; a file that imports
anything must be assembled first. See [`import`](../language/import.md).

## See also

- [`DreamShaderModule.h`](dreamshader-module.md) — the module entry point and its exported helpers
- [`DreamShaderCompiler`](compiler-module.md) — the extension point for a custom compiler backend
- [Project settings](../settings/project.md) — `UDreamShaderSettings` from the user's side
- [Generation](../generation/index.md) — what the editor module does with a parsed definition
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the headless entry point
- [Editor bridge](../tools/bridge.md) — the WebSocket server and request-file protocol
- [Workspace](../tools/workspace.md) — the `bridge.db` cache and the exported manifests
- [Contributing](../contributing/index.md) — building the plugin from source
