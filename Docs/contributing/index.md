# Contributing

> [DreamShader](../index.md) » **Contributing**

Building the plugin itself, the layout of its three C++ modules, and where each subsystem's code
lives.

| | |
| :-- | :-- |
| Repository | <https://github.com/TypeDreamMoon/DreamShader> |
| Kind | contributor reference |
| Plugin version | `1.5.1` — descriptor `Version` `151`, `IsBetaVersion` `false` |
| Engines | Unreal Engine `5.3` – `5.8`, Win64 verified |
| License | MIT |

## Synopsis

Validate the plugin standalone, without building a full project target:

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<PluginDir>\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

`<PluginDir>` is the directory holding `DreamShader.uplugin`. `-Package` must point at a directory
that does not overlap the source tree — UAT stages a clean copy there.

## Repository layout

| Path | Contents | In the [release archive](release.md#archive-contents) |
| :-- | :-- | :-- |
| `Source/` | The three C++ modules | yes |
| `Docs/` | This manual | yes |
| `Resources/` | `Icon128.png`, the plugin icon | yes |
| `DreamShader.uplugin` | Plugin descriptor | yes |
| `README.md` | English readme | yes |
| `CHANGELOG.md` | Version history; the release workflow reads its `## <VersionName>` section | yes |
| `LICENSE` | MIT | yes |
| `Shaders/` | `DreamShaderBuiltins.ush`, mounted at `/Plugin/DreamShader` | **no** |
| `Tests/Corpus/` | Data-driven fixtures and `.expected.json` goldens | **no** |
| `Config/` | `FilterPlugin.ini` — the stock commented template; it declares no extra packaged files | **no** |
| `Images/` | Readme artwork | **no** |
| `README.zh-CN.md` | Chinese readme | **no** |
| `.github/workflows/release.yml` | The only workflow in the repository | **no** |
| `Binaries/`, `Intermediate/` | Build output; git-ignored | **no** |

## Modules

| Module | Type | Loading phase | Public headers | Export macro |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader` | Runtime | Default | 6 | `DREAMSHADER_API` |
| `DreamShaderCompiler` | Runtime | Default | 3 | `DREAMSHADERCOMPILER_API` |
| `DreamShaderEditor` | Editor | Default | **0** — there is no `Public/` folder | `DREAMSHADEREDITOR_API` is defined by UBT and never used in source |

The descriptor declares two enabled plugin dependencies: `WebSocketNetworking` (the preview
WebSocket server) and `SQLiteCore` (the bridge database). Both are engine plugins.

### Build rules

All three modules set `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` and declare no
`PublicDefinitions` or `PrivateDefinitions`.

| Module | Dependency list |
| :-- | :-- |
| `DreamShader` | **Public (6)**: `Core`, `CoreUObject`, `DeveloperSettings`, `Engine`, `Projects`, `RenderCore`. No private dependencies. |
| `DreamShaderCompiler` | **Public (4)**: `Core`, `CoreUObject`, `DreamShader`, `Engine`. No private dependencies. |
| `DreamShaderEditor` | **Private (25)**: `ApplicationCore`, `AssetRegistry`, `AssetTools`, `ContentBrowser`, `Core`, `CoreUObject`, `DirectoryWatcher`, `DreamShader`, `DreamShaderCompiler`, `Engine`, `InputCore`, `Json`, `MaterialEditor`, `Projects`, `RHI`, `RenderCore`, `Renderer`, `Slate`, `SlateCore`, `SQLiteCore`, `ToolMenus`, `ToolWidgets`, `UnrealEd`, `WebSocketNetworking`, `WorkspaceMenuStructure`. No public dependencies. |

`DreamShader` is a **public** dependency of `DreamShaderCompiler`, so anything that links
`DreamShaderCompiler` transitively gets the runtime headers.

Why the runtime module needs each of its dependencies: `DeveloperSettings` for `UDeveloperSettings`;
`Engine` for `EngineTypes.h`, `MaterialDomain.h` and `UMaterialInstanceConstant`; `RenderCore` for
`AllShaderSourceDirectoryMappings` / `AddShaderSourceDirectoryMapping`; `Projects` for
`IPluginManager`.

## Source tree

### `Source/DreamShader` — Runtime

| Path | Responsibility |
| :-- | :-- |
| `Public/` | The six exported headers. Documented one page each under [C++ API](../api/index.md). |
| `Private/DreamShaderModule.cpp` | Module bootstrap: creates the source, package and generated-shader directories, registers the `/DreamShaderGenerated` and `/Plugin/DreamShader` shader mappings, and implements the directory getters, `SanitizeIdentifier`, `NormalizeSourceFilePath` and the four file-classification predicates. |
| `Private/DreamShaderSettings.cpp` | `UDreamShaderSettings`: constructor defaults, `NormalizeMappingKey`, the three `TryResolve*` methods and the three `BuildDefault*Mappings` alias catalogues. |
| `Private/DreamShaderTypes.cpp` | `NormalizeSettingKey`, `FTextShaderDefinition::TryGetSetting` / `GetSetting`. |
| `Private/DreamShaderMaterialInstance.cpp` | `UDreamShaderMaterialInstance::HasOverridenBaseProperties` and `IsAsset`. |
| `Private/Parser/DreamShaderParser.cpp` | Top-level dispatcher: `Shader`, `Function`, `GraphFunction`, `Namespace`, `VirtualFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` and the deprecated `MaterialLayer*` aliases; function-signature parsing and return-type lowering. |
| `Private/Parser/DreamShaderParserScanner.cpp` | Token/block scanning, string and comment skipping, `Path(...)` resolution. |
| `Private/Parser/DreamShaderParserSections.cpp` | `Properties`, `Settings`, `Inputs`/`Outputs`/`Results`, `Options`, `Layout` and the `[ … ]` metadata block. |
| `Private/Parser/DreamShaderParserInternal.h` | Shared declarations for the three parser translation units. |

### `Source/DreamShaderCompiler` — Runtime

| Path | Responsibility |
| :-- | :-- |
| `Public/DreamShaderCompilerInterfaces.h` | `FDreamShaderCompileRequest`, `FDreamShaderCompileResult`, `IDreamShaderCompiler`. |
| `Public/DreamShaderCompileService.h` | `FDreamShaderCompileService` — argument packing over an `IDreamShaderCompiler&`. |
| `Public/DreamShaderCompilerModule.h` | `FDreamShaderCompilerModule`. |
| `Private/` | The service forwarders and the module implementation; both module methods are empty. |

This module contains no material-generation code. It exists so non-editor code can request a compile
without linking `DreamShaderEditor`. See [Compiler module](../api/compiler-module.md).

### `Source/DreamShaderEditor` — Editor

Everything is under `Private/`; nothing is exported.

| Path | Responsibility |
| :-- | :-- |
| `DreamShaderEditorModule.cpp` | Module entry. Gates the bridge on `IsRunningCommandlet()`, the cook-director check and `-NoDreamShaderEditorBridge`; owns the cook-time materialization pass. |
| `DreamShaderEditorPersistenceUtils.h` | `BindAndExecute` — the shared prepared-statement helper both SQLite writers use. |
| `Bridge/` | The [editor bridge](../tools/bridge.md): request-file polling, directory watcher, debounce and compile queue, material-compile diagnostics, and the preview WebSocket server. |
| `Commandlet/` | `UDreamShaderCommandlet` and the `compile` / `decompile` runners plus the shared argument helpers. See [Commandlet](../tools/commandlet.md). |
| `Compile/` | `FEditorCompileAdapter` — the editor's implementation of `IDreamShaderCompiler`, and its process-wide accessor. |
| `Decompiler/` | `UMaterial` / `UMaterialFunction` → `.dsm` / `.dsf` export: the decompile service, the graph decompiler and its helpers, and layout emission. See [Decompiler](../tools/decompiler.md). |
| `DependencyGraph/` | `import` dependency tracking: `TryExtractImportPathFromLine`, `NormalizeImportSpecifier`, `ResolveImportPath` and the recursive header-dependency collection that decides which sources a `.dsh` or `.dsf` change requeues. |
| `Diagnostics/` | `FDreamShaderDiagnosticsStore`: error-location parsing, `diagnostics.json`, the per-file `diagnostics/` tree and the SQLite `diagnostics` table. |
| `MaterialAssetGeneration/` | The generator. See the file-family table below. |
| `Preview/` | `FDreamShaderPreviewRenderer`: thumbnail scene, render targets, orbit and mesh handling, PNG encoding, blocking and async readback. See [Preview](../tools/preview.md). |
| `SourceFiles/` | `FDreamShaderSourceFileUtils`: project source discovery, the `DShader/Packages` exclusion, and the under-directory predicates. |
| `Tests/` | The automation suite and the two corpus runners. See [Testing](testing.md). |
| `UI/` | The [Material Content Browser](../tools/material-browser.md) tab, its Project and Gen pages, the material details panel, the instance factory and the generated-asset path helpers. |
| `VirtualFunction/` | `VirtualFunction` declaration authoring and the startup sync service. See [VirtualFunction tools](../tools/virtual-function-tools.md). |
| `Workspace/` | `FDreamShaderWorkspaceService`: the `.code-workspace` writer, the three bridge manifests, `bridge.db`, and the external-editor launch helpers. See [Workspace](../tools/workspace.md). |

### Inside `MaterialAssetGeneration/`

| File family | Responsibility |
| :-- | :-- |
| `DreamShaderMaterialGenerator.{h,cpp}`, `…GeneratorPrivate.h` | `FMaterialGenerator::GenerateAssetsFromFile` / `GenerateMaterialFromFile` — the two entry points and their control flow. |
| `…GeneratorSourceLoading.{h,cpp}` | Loading a source file and expanding `import` directives into one text, with the byte-offset map used for diagnostics. |
| `…GeneratorDiagnostics.{h,cpp}` | Mapping a prepared-source byte index back to (file, line, column) and formatting parse / generate / code-block errors with that location. |
| `…GeneratorCode.cpp`, `…GeneratorCodeShared.h` | `FCodeGraphBuilder` — the `Graph` statement walker and the state shared by the code translation units. |
| `…GeneratorCodeCalls.cpp`, `…CodeFunctionCalls.cpp`, `…CodeExpressions.cpp`, `…CodeLiterals.cpp`, `…CodeConstructors.cpp`, `…CodeSwizzle.cpp`, `…CodeCoercion.cpp`, `…CodeMathBuiltins.cpp`, `…CodeUE.cpp`, `…CodeProperties.cpp`, `…CodeParsing.cpp` | One translation unit per lowering concern: calls, function calls, expressions, literals, constructors, swizzles, coercion, math builtins, `UE.*` and `Substrate.*` builtins, property reads, and expression-text parsing. |
| `…CodeReuse.cpp` | Reusable-expression caching: builds a stable key per call/expression so identical sub-expressions share one node. See [Node reuse](../graph/node-reuse.md). |
| `…GeneratorFunctionLookups.cpp` | Read-only name → definition scans for `Function`, `GraphFunction`, material functions and `VirtualFunction`. |
| `DreamShaderAssetFactory.cpp`, `DreamShaderAssetReferenceResolution.cpp` | Package and asset creation, `Root=` resolution, plugin-mount checks, and `Path(...)` reference resolution. |
| `DreamShaderExpressionFactory.cpp`, `DreamShaderMaterialExpressionReflection.cpp` | `UMaterialExpression` creation, and the reflected class/property surface that also feeds `material-expressions.json`. |
| `DreamShaderMaterialSettings.cpp`, `DreamShaderMaterialValueParsing.cpp`, `DreamShaderTypeResolution.cpp`, `DreamShaderMaterialLiteralPropertyWriter.cpp` | Applying `Settings`, parsing setting and metadata values, resolving type tokens, and writing reflected literal properties. |
| `DreamShaderMaterialGeneratorTransformBasis.cpp` | Transform basis names for `UE.TransformVector` / `UE.TransformPosition`. |
| `DreamShaderMaterialGraphLayout.cpp`, `DreamShaderMaterialGraphTeardown.cpp` | Automatic node placement, and clearing a material graph before regeneration. |
| `DreamShaderMaterialGeneratorSupport.cpp` | Material reset and graph-editing support built on `MaterialEditingLibrary`. |
| `DreamShaderHlslFunctionCodegen.cpp` | Identifier tokenizing, call rewriting, Custom-node code assembly and generated `.ush` emission. |
| `DreamShaderGeneratedAssetMetadata.cpp` | The `DreamShader.SourceFile` / `DreamShader.SourceHash` package metadata, the CRC32 source hash, and the regeneration skip check. |

## Building

| Goal | How |
| :-- | :-- |
| Iterate on the plugin inside a project | Build the host project's editor target normally. The plugin is `EnabledByDefault`. |
| Validate the plugin standalone, exactly as the consumer sees it | `RunUAT BuildPlugin`, as in the [Synopsis](#synopsis). |
| Reproduce the release archive | Stage the seven shipped items by hand, or push a tag and let the [release workflow](release.md) do it. |

`BuildPlugin` compiles all three modules against the target engine and fails on the first UBT or UHT
error. It is the check that matters when adding an engine-version gate, because the project build
only ever exercises one engine version.

## Engine versions

| Engine | Status |
| :-- | :-- |
| `5.8` | Verified with `RunUAT BuildPlugin` |
| `5.7` | Verified with `RunUAT BuildPlugin` |
| `5.6` | Verified with `RunUAT BuildPlugin` |
| `5.5` | Verified with `RunUAT BuildPlugin` |
| `5.4` | Verified with `RunUAT BuildPlugin` |
| `5.3` | Verified with `RunUAT BuildPlugin` |

> [!WARNING]
> On Windows, Unreal Engine `5.3` and `5.4` may require the MSVC `14.38` toolchain. Newer compiler
> toolchains can fail while compiling **older engine headers**, before any plugin code is reached —
> the failure is not in `Source/DreamShader*`. Install `14.38` through the Visual Studio Installer
> and select it in `BuildConfiguration.xml` before concluding the plugin is broken on those engines.

Every engine-version test in the plugin goes through the six macros in `DreamShaderVersionCompat.h`;
there are no raw `ENGINE_MAJOR_VERSION` or `UE_VERSION_NEWER_THAN` uses anywhere in `Source/`. When
you add version-dependent code, add it there and use `DREAMSHADER_UE_VERSION_AT_LEAST(Major, Minor)`
or `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`. The complete list of currently gated behaviour is on
[Version compatibility](../api/version-compat.md).

## Notes

- **`DreamShaderEditor` exports nothing.** It has no `Public/` folder, and `DREAMSHADEREDITOR_API`
  never appears in source, so third-party C++ cannot link against the generator, the bridge or the
  decompiler. The supported C++ extension point is `IDreamShaderCompiler` in `DreamShaderCompiler`;
  everything else is reachable only through the editor UI, the
  [commandlet](../tools/commandlet.md) or the [bridge](../tools/bridge.md).
- **No delegates and no module singletons.** The plugin declares zero `DECLARE_DELEGATE*`,
  `DECLARE_EVENT*` or `DECLARE_DYNAMIC*` types, and neither module class has `Get()` / `IsAvailable()`
  accessors. Use `FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"))`.
- **The generator is game-thread, editor-only.** Both `FMaterialGenerator` entry points open an
  `FScopedSlowTask`, create and modify `UPackage`s and `UMaterial`s, and call `PostEditChange()`.
- **Three different "normalize" helpers exist** and are easy to confuse: `NormalizeSettingKey`
  (trim + lowercase), `UDreamShaderSettings::NormalizeMappingKey` (trim + lowercase + strip spaces,
  `_` and `-`), and `NormalizeSourceFilePath` (absolute path with `/` separators). `SanitizeIdentifier`
  is an identifier sanitizer, not a key normalizer.
- The generated documentation site is built from `Docs/`; `DocsURL` in the descriptor points at
  <https://lang.64hz.cn/>.

## Example

A complete standalone build of the plugin against a 5.7 engine:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="D:\Work\MyProject\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="D:\Work\Out\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

Staged output:

```text
D:\Work\Out\DreamShader\
  Binaries\Win64\UnrealEditor-DreamShader.dll
  Binaries\Win64\UnrealEditor-DreamShaderCompiler.dll
  Binaries\Win64\UnrealEditor-DreamShaderEditor.dll
  Binaries\Win64\UnrealEditor.modules
  Intermediate\              UBT and UHT working files
  Source\                    copied from the plugin tree
  Shaders\                   copied from the plugin tree
  Resources\                 copied from the plugin tree
  Docs\                      copied from the plugin tree
  DreamShader.uplugin
```

The same three module names appear in a normal project build under
`<Plugin>/Binaries/Win64/`, alongside their `.pdb` files.

## See also

- [Testing](testing.md) — the automation suite, the fixture corpus, and how to run both headlessly
- [Release](release.md) — tag conventions, the release workflow, and what the archive contains
- [C++ API](../api/index.md) — the exported headers, class by class
- [Compiler module](../api/compiler-module.md) — `IDreamShaderCompiler`, the supported extension point
- [Version compatibility](../api/version-compat.md) — the six macros and every gated behaviour
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the headless entry point
- [Editor bridge](../tools/bridge.md) — request files, `bridge.db`, the preview WebSocket
- [Generation pipeline](../generation/index.md) — what the generator does, end to end
- [Project settings](../settings/project.md) — `UDreamShaderSettings`, including the source directory
</content>
</invoke>
