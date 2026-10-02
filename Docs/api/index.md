# C++ API

> [DreamShader](../index.md) » **C++ API**

The plugin's public C++ surface: five modules, 69 public headers, and one compile interface every
caller goes through.

| | |
| :-- | :-- |
| Modules | `DreamShaderLang` (Runtime) · `DreamShader` (Runtime) · `DreamShaderPass` (Runtime) · `DreamShaderCompiler` (Editor) · `DreamShaderEditor` (Editor) |
| Public headers | 30 + 8 + 11 + 20 + 0 |
| Export macros | `DREAMSHADERLANG_API`, `DREAMSHADER_API`, `DREAMSHADERPASS_API`, `DREAMSHADERCOMPILER_API` |
| Reflected types in public headers | `DreamShader`: 2 `UCLASS`, 2 `UENUM` · `DreamShaderPass` *(since 2.1.0)*: 9 `UCLASS`, 18 `USTRUCT`, 17 `UENUM` |
| Delegates | three — `FDreamShaderDefineProviderDelegate`, a `DECLARE_DELEGATE_OneParam` in `DreamShaderDefineResolution.h` *(since 1.9.0; moved out of `DreamShaderDefineTable.h` in 2.0)*; `FOnDreamShaderSourceGenerated`, a multicast delegate in `DreamShaderCompilerService.h` *(public since 2.0)*; and `FOnDreamPassPipelineChanged`, a `DECLARE_MULTICAST_DELEGATE_OneParam` in `DreamPassPipeline.h` *(since 2.1.0)*. No `DECLARE_EVENT*` and no `DECLARE_DYNAMIC*` |
| Plugin version | `2.0.2` (`"Version": 202`) |

## Modules

| Module | Type | Loading phase | Public headers | Export macro | Purpose |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | `Runtime` | `PostConfigInit` | 30 | `DREAMSHADERLANG_API` | The front end and the middle end, and the only module that depends on `Core` alone: source text and spans, diagnostics, the lexer, the AST, **both** parsers (2.0 and the 1.x legacy front end), the printer, the preprocessor with its define table, the binder (`Semantic/`), the engine-free graph IR with its builder, passes, validator and comparator (`IR/`), the way back from IR to source (`Decompile/`), and the 1.x → 2.0 migrator (`Migrate/`). Knows nothing about the engine — engine facts arrive as parameters, or as data in an `FBuiltinCatalog`. |
| `DreamShader` | `Runtime` | `PostConfigInit` | 8 | `DREAMSHADER_API` | Log category, canonical path helpers, the 1.x source data model, the define-resolution tiers that feed the preprocessor its engine-side values, the project settings object, the generated instance class, the engine-version macros, and the **compile interface** with its registry (`GetDreamShaderCompiler`). The 1.x parser that lived here retired in 2.0. |
| `DreamShaderPass` *(since 2.1.0)* | `Runtime` | `PostConfigInit` | 11 | `DREAMSHADERPASS_API` | The Custom Pass runtime: the `UDreamPassPipeline` asset a `.dsp` compiles to, the settings, the subsystem that decides which pipelines apply to a view, the volume and components, the two material expressions, and the scene view extension that runs the passes — the last only on UE 5.8 and later ([`DREAMSHADER_WITH_CUSTOM_PASS`](version-compat.md#dreamshader_with_custom_pass)). See [DreamShaderPass](pass-module.md). |
| `DreamShaderCompiler` | **`Editor`** | `Default` | 20 | `DREAMSHADERCOMPILER_API` | The compiler's back half *(since 2.0; a Runtime interface-only module through 1.9.x)*: the pipeline, the IR emitter, the asset layer, the builtin catalog built from reflection, the product index, `.dsi` parent schemas and settings, and *(since 2.1.0)* the `.dsp` emit with the Custom Pass HLSL slot registry and its pre-check. Registers the one `IDreamShaderCompiler`. |
| `DreamShaderEditor` | `Editor` | `Default` | **0** | *(none used)* | Everything a user touches: the decompilers, `dsc migrate`, the commandlet, the bridge, the preview, the Material Content Browser, provenance actions, the workspace exporter, the pass pipeline details panel, the automation tests. |

All five are declared in `DreamShader.uplugin`. `DreamShaderLang`, `DreamShader` and `DreamShaderPass`
load at `PostConfigInit` — early enough that the settings object and the shader-directory mappings exist
before anything asks for them, and, for `DreamShaderPass`, before the engine commits its shader type
list, which its global shader types join when the module loads — and the two Editor modules at
`Default`. `DreamShaderLang` is listed first in the descriptor: every other module depends on it,
directly or through `DreamShader`, and the phase alone does not order them. `DreamShaderPass` follows
`DreamShader`, and `DreamShaderCompiler` is listed before `DreamShaderEditor`, for the same reason; a
caller that runs before the compiler has started still gets one: `GetDreamShaderCompiler()` loads the
module on demand.
The plugin is `EnabledByDefault` and `CanContainContent`; `IsBetaVersion` is `false`.

## Public headers

| Header | Module | Include | Purpose |
| :-- | :-- | :-- | :-- |
| `DreamShaderModule.h` | `DreamShader` | `#include "DreamShaderModule.h"` | `LogDreamShader`, `FDreamShaderModule`, `FDreamShaderSourceRoot`, eighteen exported free functions (source roots, paths, identifier sanitizing, file classification — `IsDreamShaderPipelineFile` among them *(since 2.1.0)*). |
| `DreamShaderTypes.h` | `DreamShader` | `#include "DreamShaderTypes.h"` | The parsed-AST data model: 13 structs, 5 enums, `LexToString`, `NormalizeSettingKey`. |
| `DreamShaderAssetReferenceText.h` *(since 1.9.0)* | `DreamShader` | `#include "DreamShaderAssetReferenceText.h"` | Header-only helpers for asset references in Unreal's export form — `Class'/Game/Path/Asset.Asset'`, what *Copy Reference* puts on the clipboard: `FDreamShaderReferenceShell` and the functions that strip the shell. |
| `DreamShaderCompilerInterface.h` *(since 2.0)* | `DreamShader` | `#include "DreamShaderCompilerInterface.h"` | `EThinCustomPersistence`, `FDreamShaderCompileRequest`, `FDreamShaderCompileResult` (`Message` is `FText`, `Code` the first `DSHnnnn`), `IDreamShaderCompiler`, `GetDreamShaderCompiler`, `RegisterDreamShaderCompiler`. See [DreamShaderCompiler](compiler-module.md#the-interface-lives-in-dreamshader). |
| `DreamShaderDefineTable.h` *(since 1.9.0; `DreamShaderLang` since 2.0)* | `DreamShaderLang` | `#include "DreamShaderDefineTable.h"` | The [preprocessor](../language/preprocessor.md) define table, pure text and no engine: `EDreamShaderDefineSource`, `FDreamShaderDefineEntry`, the case-sensitive `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap` aliases, `FDreamShaderDefineTable`, `IsReservedDreamShaderDefineName`, `IsValidDreamShaderDefineName`. |
| `DreamShaderDefineResolution.h` *(since 2.0)* | `DreamShader` | `#include "DreamShaderDefineResolution.h"` | The engine half that BUILDS one of those tables: `GetBuiltinDreamShaderDefines`, `RegisterDreamShaderDefine`, `UnregisterDreamShaderDefinesFrom`, `FDreamShaderDefineProviderDelegate`, `RegisterDreamShaderDefineProvider`, `UnregisterDreamShaderDefineProvider`, `SetDreamShaderCommandLineDefines`, `ResolveDreamShaderDefines`, `GetDreamShaderDefineRevision`, `NotifyDreamShaderDefineSettingsChanged`. |
| `DreamShaderPreprocessor.h` *(since 1.9.0; `DreamShaderLang` since 2.0)* | `DreamShaderLang` | `#include "DreamShaderPreprocessor.h"` | `PreprocessDreamShaderSource`, `FDreamShaderPreprocessResult`, `DreamShaderSourceHasPreprocessorDirectives`, `BuildDreamShaderDefineKeyFragment`. |
| `DreamShaderSettings.h` | `DreamShader` | `#include "DreamShaderSettings.h"` | `EDreamShaderDefaultBackend`, `UDreamShaderSettings`, the enum-alias resolvers. |
| `DreamShaderMaterialInstance.h` | `DreamShader` | `#include "DreamShaderMaterialInstance.h"` | `UDreamShaderMaterialInstance` — the asset the ThinCustom backend produces. |
| `DreamShaderVersionCompat.h` | `DreamShader` | `#include "DreamShaderVersionCompat.h"` | Six engine-version macros. No types, no functions. |
| `DreamPassPipeline.h` · `DreamPassTypes.h` · `DreamPassSubsystem.h` · `DreamPassSettings.h` · `DreamShaderPassModule.h` · 6 more *(since 2.1.0)* | `DreamShaderPass` | `#include "DreamPassPipeline.h"` | The Custom Pass asset and its descriptor types, the subsystem, the settings, the volume and components, the Blueprint library, the two material expressions, the slot registry paths. Listed on the [DreamShaderPass page](pass-module.md#public-headers). |
| `DreamShaderCompilerService.h` · `DreamShaderCompilePipeline.h` · `DreamShaderIREmitter.h` · `DreamShaderBuiltinCatalog.h` · `DreamShaderProductIndex.h` · `DreamShaderPassPipelines.h` *(since 2.1.0)* · 14 more | `DreamShaderCompiler` | editor-only | The compiler service, the pipeline with its intermediate products, the emitter, the catalog, the product index, the Custom Pass slot registry tools. Listed on the [DreamShaderCompiler page](compiler-module.md#public-headers). |
| `Lang/*.h` · `Semantic/LangBound.h` · `IR/*.h` · `Decompile/IRToAst.h` · `Migrate/LangMigrate.h` | `DreamShaderLang` | keep the folder prefix: `#include "Lang/LangParser.h"` | The language itself. Listed on the [DreamShaderLang page](lang-module.md#public-headers). |

Namespaces: everything in `DreamShaderModule.h`, `DreamShaderTypes.h`,
`DreamShaderCompilerInterface.h`, `DreamShaderDefineResolution.h`, `DreamShaderDefineTable.h` and
`DreamShaderPreprocessor.h` lives in `UE::DreamShader` — the two that moved to `DreamShaderLang` in
2.0 kept that namespace, so a caller of theirs needs no source change. The language is
`UE::DreamShader::Lang` and `UE::DreamShader::IR`; the compiler module is
`UE::DreamShader::Editor::Compiler`. `FDreamShaderModule`, `FDreamShaderCompilerModule`,
`UDreamShaderSettings`, `UDreamShaderMaterialInstance` and `EDreamShaderDefaultBackend` are at global
scope. `DreamShaderPass` *(since 2.1.0)* puts its reflected types — `UDreamPassPipeline`,
`UDreamPassSubsystem`, `ADreamPassVolume`, the `EDreamPass*` enums and `FDreamPass*` structs — at global
scope, as reflected types must be, and its free functions in `UE::DreamPass`.

## Linkage

> [!WARNING]
> **`DreamShaderEditor` exports nothing and cannot be linked against.** It has no `Public/` folder at
> all, its `Build.cs` declares no `PublicDependencyModuleNames`, and `DREAMSHADEREDITOR_API` — which
> UnrealBuildTool defines for every module — has zero occurrences anywhere in the plugin's source.
> Its module class `FDreamShaderEditorModule` is declared inside the `.cpp`, not in a header, so it
> cannot be named from outside either. Everything the editor module does is reachable only through
> UE's module system, through UI actions, or through the [commandlet](../tools/commandlet.md).
> Compiling is no longer among those things: since 2.0 the compiler is its own module, reached
> through [`IDreamShaderCompiler`](compiler-module.md#idreamshadercompiler).

| Module | `DREAMSHADERLANG_API` | `DREAMSHADER_API` | `DREAMSHADERPASS_API` | `DREAMSHADERCOMPILER_API` | `DREAMSHADEREDITOR_API` |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | `DLLEXPORT` | — | — | — | — |
| `DreamShader` | `DLLIMPORT` | `DLLEXPORT` | — | — | — |
| `DreamShaderPass` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` | — | — |
| `DreamShaderCompiler` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` | — |
| `DreamShaderEditor` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` *(defined, never used)* |

`UE_PLUGIN_NAME` is `"DreamShader"` for all five modules.

None of the headers guards its export macro with an `#ifndef`; all of them require the UBT-generated
definitions.

### There are no module singletons

The plugin declares no `Get()`, `IsAvailable()` or `GetChecked()` accessor on any module class. Use
the stock module manager — or, for the compiler, `GetDreamShaderCompiler()`, which is the one
registry the plugin has:

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
| `PublicDependencyModuleNames` | `Core`, `CoreUObject`, `DeveloperSettings`, `DreamShaderLang`, `Engine`, `Projects`, `RenderCore` |
| `PrivateDependencyModuleNames` | *(none declared)* |
| `PublicDefinitions` | `DREAMSHADER_WITH_MOON_ENGINE=0\|1`, probed from the engine's `SceneTypes.h`; `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS=0\|1`, probed from the header that declares `EMaterialParameterType` |

| Dependency | Needed for |
| :-- | :-- |
| `Core` | `FString`, `FPaths`, `IFileManager`, the log category |
| `CoreUObject` | `UObject`, `UObjectInitialized()`, the two `UCLASS`es |
| `DeveloperSettings` | `UDeveloperSettings`, the base of `UDreamShaderSettings` |
| `DreamShaderLang` | the define table `DreamShaderDefineResolution.h` fills, `FDreamShaderError` |
| `Engine` | `Engine/EngineTypes.h`, `MaterialDomain.h`, `UMaterialInstanceConstant` |
| `Projects` | `IPluginManager` — shader-mount lookup and `Path(Plugin.X, …)` resolution |
| `RenderCore` | `ShaderCore.h`'s `AllShaderSourceDirectoryMappings` / `AddShaderSourceDirectoryMapping` |

### `DreamShaderPass.Build.cs`

*(since 2.1.0)*

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`, `CoreUObject`, `DeveloperSettings`, `DreamShader`, `Engine`, `RenderCore`, `RHI` |
| `PrivateDependencyModuleNames` | `Projects`, `Renderer` |
| `PublicDefinitions` | `DREAMSHADER_WITH_CUSTOM_PASS=1` when the target engine is 5.8 or later (`Target.Version`), `=0` below — see [`DREAMSHADER_WITH_CUSTOM_PASS`](version-compat.md#dreamshader_with_custom_pass) |
| `PrivateIncludePaths` | on 5.8 and later only, the Renderer module's `Internal` folder, for `FPostProcessingInputs` — UBT hands an `Internal` folder to modules of the same rules scope only, which an engine plugin is and a project plugin is not |

The engine gate is in the build rules rather than in the descriptor, which has no per-module engine
field, and the define is public so that `DreamShaderCompiler` and `DreamShaderEditor` ask it the same
question instead of deriving it again.

### `DreamShaderCompiler.Build.cs`

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`, `CoreUObject`, `DreamShader`, `DreamShaderLang`, `Engine` |
| `PrivateDependencyModuleNames` | `AssetRegistry`, `AssetTools`, `DreamShaderPass`, `Json`, `MaterialEditor`, `Projects`, `RenderCore`, `RHI`, `TargetPlatform`, `UnrealEd` |

The public dependencies are public because the public headers expose them: `UMaterial*` and
`EMaterialProperty` (`Engine`), `FTextShaderDefinition` and the compile interface (`DreamShader`),
`FLangDiagnosticSink` / `FModule` / `FIRModule` (`DreamShaderLang`). An Editor-type module cannot be
a dependency of a Runtime one, which is why the interface itself lives in `DreamShader`.

`DreamShaderPass`, `RenderCore`, `RHI` and `TargetPlatform` are the `.dsp` half *(since 2.1.0)*:
`DreamShaderPass` for the asset a `.dsp` compiles to, its settings and its slot paths; the other three for
the HLSL slots — the shader source mappings, the pre-check's in-process compile, the global shader types it
finds by name, the shader platforms and the formats the target platforms cook. No public header of the
module names them, so they are private.

### `DreamShaderEditor.Build.cs`

| Setting | Value |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | **none** |
| `PrivateDependencyModuleNames` | 31, listed below |
| `PrivateDefinitions` | `MOON_ENGINE=1`, when the engine's `SceneTypes.h` declares `MP_MoonEncodedAttribute0` |

| # | Module | Used for |
| :-: | :-- | :-- |
| 1 | `ApplicationCore` | external-editor launch and clipboard, from the workspace service |
| 2 | `AssetRegistry` | `FAssetRegistryModule` — asset creation/removal broadcasts from the bridge |
| 3 | `AssetTools` | `FAssetToolsModule` — the material-instance factory |
| 4 | `ContentBrowser` | `FContentBrowserModule` — the Project page and the folder picker |
| 5 | `Core` | — |
| 6 | `CoreUObject` | — |
| 7 | `DesktopPlatform` | file and folder dialogs |
| 8 | `DirectoryWatcher` | `FDirectoryWatcherModule` — the source-directory watcher behind auto-compile-on-save, and the watches on Custom Pass shader files |
| 9 | `DreamShader` | types, settings, log category, the compile interface |
| 10 | `DreamShaderCompiler` | the pipeline, the emitter's asset layer, the catalog, the product index |
| 11 | `DreamShaderLang` | both front ends, the IR, IR → source, the migrator |
| 12 | `DreamShaderPass` *(since 2.1.0)* | `UDreamPassPipeline` and its types: the `.dsp` decompiler, Adopt, the browser, the details panel and `dump-graph` read the asset |
| 13 | `Engine` | materials, packages |
| 14 | `InputCore` | Slate input |
| 15 | `Json` | bridge manifests and diagnostics files |
| 16 | `MaterialEditor` | `UMaterialEditingLibrary`, node navigation |
| 17 | `Projects` | `IPluginManager` |
| 18 | `PropertyEditor` *(since 2.1.0)* | the details panel of a pipeline asset |
| 19 | `RHI` | preview render target, `EShaderPlatform` enumeration |
| 20 | `RenderCore` | shader directory mappings |
| 21 | `Renderer` | preview rendering |
| 22 | `Settings` | opening the project settings page |
| 23 | `Slate` | UI |
| 24 | `SlateCore` | UI |
| 25 | `SQLiteCore` | the bridge database — see below |
| 26 | `TargetPlatform` | `check -Shaders`: the platforms shaders are compiled for |
| 27 | `ToolMenus` | `UToolMenus` menu and toolbar extensions |
| 28 | `ToolWidgets` | search box and filter widgets |
| 29 | `UnrealEd` | commandlet base, editor subsystems, `FScopedSlowTask` |
| 30 | `WebSocketNetworking` | the live-preview server — see below |
| 31 | `WorkspaceMenuStructure` | the tab-spawner category for the Material Content Browser |

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
| [`DreamShaderTypes.h`](types.md) | Every struct and enum in the 1.x source data model, which the asset layer still speaks |
| [`DreamShaderParser.h`](parser.md) | **Retired in 2.0.** What replaced `FTextShaderParser::Parse` |
| [`DreamShaderSettings.h`](settings.md) | `UDreamShaderSettings`, `EDreamShaderDefaultBackend`, and the alias catalogues |
| [`DreamShaderMaterialInstance.h`](material-instance.md) | `UDreamShaderMaterialInstance` and its two overrides |
| [`DreamShaderVersionCompat.h`](version-compat.md) | The compat macros and every version-gated behaviour they select |
| [`DreamShaderCompiler`](compiler-module.md) | The compile interface and its registry, the compiler service, the pipeline, and the module's other public headers |
| [`DreamShaderLang`](lang-module.md) | The front end and middle end: the `Lang/`, `Semantic/`, `IR/`, `Decompile/` and `Migrate/` headers, the two preprocessor headers that moved into it, every entry point from preprocess to IR validation and back to source, and the `Lang` fixture corpus |
| [`DreamShaderPass`](pass-module.md) *(since 2.1.0)* | The Custom Pass runtime's C++ and Blueprint surface: the pipeline asset, the subsystem and activation sources, the volume and components, the slot registry paths |
| `DreamShaderDefineTable.h` · `DreamShaderPreprocessor.h` · `DreamShaderDefineResolution.h` | No page of their own yet; the whole surface is documented from the language side, on [Preprocessor](../language/preprocessor.md). The first two live in `DreamShaderLang` — see the [DreamShaderLang page](lang-module.md) |

## Notes

- **Asking for a compile needs only the runtime module.** `GetDreamShaderCompiler()` hands back the
  compiler the editor side registered, or null where none exists (a game target). Generation itself
  is editor-only and lives in [`DreamShaderCompiler`](compiler-module.md).
- **The language is engine-independent.** Everything in `DreamShaderLang` — parsing either syntax,
  binding, lowering to IR, printing, migrating — is string and data processing over `Core`; it creates
  no `UObject`s and loads no assets. Binding needs an `FBuiltinCatalog`, which `dsc export-catalog`
  writes as JSON for tools that run outside the editor.
- `DreamShader` exports two `UCLASS`es, `UDreamShaderSettings` and `UDreamShaderMaterialInstance`.
  `DreamShaderPass` *(since 2.1.0)* exports seven with `DREAMSHADERPASS_API` — the pipeline, the
  settings, the subsystem, the volume, the two components and the Blueprint library — and declares its
  two material expressions `MinimalAPI`. `UDreamShaderCommandlet` exists in the editor module's
  `Private/` folder with **no** API macro, so it is reachable only through UE's reflection system and
  `-run=DreamShader`.
- `Config/FilterPlugin.ini` contains only the stock commented template — the plugin declares no
  extra packaged files.

## Example

A consuming module that wants to parse DreamShaderLang and request compiles:

```csharp
// MyTooling.Build.cs
PrivateDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "DreamShaderLang",      // the language: parse, print, bind, IR
    "DreamShader"           // paths, settings, the compile interface
});
```

```cpp
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderModule.h"
#include "Lang/LangParser.h"

using namespace UE::DreamShader;

void InspectSource(const FString& InPath)
{
    if (!IsDreamShaderSourceFile(InPath))
    {
        return;
    }

    FString SourceText;
    const FString Path = NormalizeSourceFilePath(InPath);
    if (!FFileHelper::LoadFileToString(SourceText, *Path))
    {
        return;
    }

    // Auto picks the front end from the extension: .dss / .dsi / .dsp / .dsh are 2.0, .dsm / .dsf are 1.x.
    const Lang::FLangSourceText Source(Path, SourceText);
    const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Source);
    UE_LOG(LogDreamShader, Display, TEXT("%s: %d declaration(s), %d diagnostic(s)."),
        *Path,
        Parsed.Module.IsValid() ? Parsed.Module->Declarations.Num() : 0,
        Parsed.Diagnostics.GetDiagnostics().Num());

    if (Parsed.Succeeded())
    {
        if (IDreamShaderCompiler* Compiler = GetDreamShaderCompiler())
        {
            FDreamShaderCompileRequest Request;
            Request.SourceFilePath = Path;
            Compiler->CompileAssets(Request);
        }
    }
}
```

`ParseDreamShaderLang` does not read files and does not follow `#include` / `import`: includes are
resolved at bind time, by the resolver the host hands the binder.

## See also

- [`DreamShaderModule.h`](dreamshader-module.md) — the module entry point and its exported helpers
- [`DreamShaderCompiler`](compiler-module.md) — the compile interface, the service and the pipeline
- [`DreamShaderPass`](pass-module.md) — the Custom Pass runtime
- [Project settings](../settings/project.md) — `UDreamShaderSettings` from the user's side
- [Generation](../generation/index.md) — what an emitted asset looks like
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the headless entry point
- [Editor bridge](../tools/bridge.md) — the WebSocket server and request-file protocol
- [Workspace](../tools/workspace.md) — the `bridge.db` cache and the exported manifests
- [Contributing](../contributing/index.md) — building the plugin from source
