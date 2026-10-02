# Contributing

> [DreamShader](../index.md) » **Contributing**

Building the plugin itself, the layout of its five C++ modules, and where each subsystem's code
lives.

| | |
| :-- | :-- |
| Repository | <https://github.com/TypeDreamMoon/DreamShader> |
| Kind | contributor reference |
| Plugin version | `2.0.2` — descriptor `Version` `202`, `IsBetaVersion` `false` |
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

One engine proves nothing about the others, so run the whole matrix with
[`.skill/build-plugin.ps1`](#the-engine-matrix).

## Repository layout

| Path | Contents | In the [release archive](release.md#archive-contents) |
| :-- | :-- | :-- |
| `Source/` | The five C++ modules | yes |
| `Docs/` | This manual | yes |
| `Resources/` | `Icon128.png`, the plugin icon | yes |
| `DreamShader.uplugin` | Plugin descriptor | yes |
| `README.md` | English readme | yes |
| `CHANGELOG.md` | Version history; the release workflow reads its `## <VersionName>` section | yes |
| `LICENSE` | MIT | yes |
| `Content/Localization/DreamShader/` | `.locmeta` plus one `.locres` per culture; loaded through the `LocalizationTargets` entry in the descriptor | yes |
| `Shaders/` | `DreamShaderBuiltins.ush` and the Custom Pass shaders under `Pass/`, mounted at `/Plugin/DreamShader` | yes *(since 1.5.1)* |
| `Tests/Corpus/` | Data-driven fixtures and `.expected.json` goldens | **no** |
| `Tools/Localization/` | `localization_lint.ps1` and the gather baseline it emits | **no** |
| `Config/` | `FilterPlugin.ini` — the stock commented template; it declares no extra packaged files | **no** |
| `Images/` | Readme artwork | **no** |
| `README.zh-CN.md` | Chinese readme | **no** |
| `.github/workflows/release.yml` | The only workflow in the repository | **no** |
| `Binaries/`, `Intermediate/` | Build output; git-ignored | **no** |

## Modules

| Module | Type | Loading phase | Public headers | Export macro |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | Runtime | PostConfigInit | 30 | `DREAMSHADERLANG_API` |
| `DreamShader` | Runtime | PostConfigInit | 8 | `DREAMSHADER_API` |
| `DreamShaderPass` *(since 2.1.0)* | Runtime | PostConfigInit | 11 | `DREAMSHADERPASS_API` |
| `DreamShaderCompiler` | Editor | Default | 20 | `DREAMSHADERCOMPILER_API` |
| `DreamShaderEditor` | Editor | Default | **0** — there is no `Public/` folder | `DREAMSHADEREDITOR_API` is defined by UBT and never used in source |

The order is the descriptor's. `DreamShaderPass` loads at `PostConfigInit` because it maps a shader
source directory and its global shader types register when its DLL loads, both of which have to happen
before the engine commits the shader type list.

The descriptor declares two enabled plugin dependencies: `WebSocketNetworking` (the preview
WebSocket server) and `SQLiteCore` (the bridge database). Both are engine plugins.

### Build rules

All five modules set `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs`. Three of them add
definitions, each from a probe of the engine being built against:

| Module | Definitions |
| :-- | :-- |
| `DreamShader` | `PublicDefinitions`: `DREAMSHADER_WITH_MOON_ENGINE` and `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS`, `0` or `1` from header probes — see [Version compatibility](../api/version-compat.md#macros) |
| `DreamShaderPass` | `PublicDefinitions`: `DREAMSHADER_WITH_CUSTOM_PASS`, `1` from UE 5.8 on — see [`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass). At `1` it also adds the Renderer's `Internal` folder to its private include paths |
| `DreamShaderEditor` | `PrivateDefinitions`: `MOON_ENGINE=1`, only when the engine's `SceneTypes.h` declares `MP_MoonEncodedAttribute0`; no source file reads it |

| Module | Dependency list |
| :-- | :-- |
| `DreamShaderLang` | **Public (1)**: `Core`. No private dependencies. |
| `DreamShader` | **Public (7)**: `Core`, `CoreUObject`, `DeveloperSettings`, `DreamShaderLang`, `Engine`, `Projects`, `RenderCore`. No private dependencies. |
| `DreamShaderPass` | **Public (7)**: `Core`, `CoreUObject`, `DeveloperSettings`, `DreamShader`, `Engine`, `RenderCore`, `RHI`. **Private (2)**: `Projects`, `Renderer`. |
| `DreamShaderCompiler` | **Public (5)**: `Core`, `CoreUObject`, `DreamShader`, `DreamShaderLang`, `Engine`. **Private (10)**: `AssetRegistry`, `AssetTools`, `DreamShaderPass`, `Json`, `MaterialEditor`, `Projects`, `RenderCore`, `RHI`, `TargetPlatform`, `UnrealEd`. |
| `DreamShaderEditor` | **Private (31)**: `ApplicationCore`, `AssetRegistry`, `AssetTools`, `ContentBrowser`, `Core`, `CoreUObject`, `DesktopPlatform`, `DirectoryWatcher`, `DreamShader`, `DreamShaderCompiler`, `DreamShaderLang`, `DreamShaderPass`, `Engine`, `InputCore`, `Json`, `MaterialEditor`, `Projects`, `PropertyEditor`, `RHI`, `RenderCore`, `Renderer`, `Settings`, `Slate`, `SlateCore`, `SQLiteCore`, `TargetPlatform`, `ToolMenus`, `ToolWidgets`, `UnrealEd`, `WebSocketNetworking`, `WorkspaceMenuStructure`. No public dependencies. |

`DreamShaderPass` is a **private** dependency of `DreamShaderCompiler` and `DreamShaderEditor`. Both
read its public definition `DREAMSHADER_WITH_CUSTOM_PASS` instead of testing the engine version
again. `DreamShaderPass` itself depends on `DreamShader` — for the project source root its
`.dreampass/` folder lives in — and on no editor module.

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

### `Source/DreamShaderPass` — Runtime

*(since 2.1.0)* The Custom Pass runtime: the asset a `.dsp` compiles to, the subsystem that decides
which pipelines apply to a view, and the scene view extension that runs their passes. The module builds
on every engine; what needs the renderer is compiled only where
[`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass) is `1`. See
[`DreamShaderPass`](../api/pass-module.md) and [Custom Pass runtime](../runtime/index.md).

| Path | Responsibility |
| :-- | :-- |
| `DreamShaderPass.Build.cs` | The UE 5.8 gate: `DREAMSHADER_WITH_CUSTOM_PASS`, and the Renderer `Internal` include path that only the 5.8 build needs. |
| `Public/` | The eleven exported headers: the module (`LogDreamPass` and the `UE::DreamPass` paths of the user shader directory, the slot registry files and the slot folders), `DreamPassTypes.h`, `DreamPassPipeline.h` (`UDreamPassPipeline`, `FOnDreamPassPipelineChanged`), `DreamPassSettings.h`, `DreamPassSubsystem.h`, `DreamPassVolume.h`, `DreamPassComponent.h`, `DreamPassLayerComponent.h`, `DreamPassBlueprintLibrary.h`, and under `Materials/` the two material expressions. |
| `Private/DreamShaderPassModule.cpp` | Module startup at 5.8: the `/DreamPassUser` shader directory mapping to the project source root's `.dreampass/`, the empty registry files the slot shaders include, and the removal of registry sections whose snapshot is missing. Below 5.8, one log line. |
| `Private/DreamPassPipeline.cpp`, `DreamPassTypes.cpp`, `DreamPassSettings.cpp` | The asset and its `Validate`, the shared enums and their spellings, the project settings. |
| `Private/DreamPassSubsystem.cpp`, `DreamPassVolume.cpp`, `DreamPassComponent.cpp`, `DreamPassLayerComponent.cpp`, `DreamPassBlueprintLibrary.cpp` | Activation: what makes a pipeline apply to a view, pass layers and lists, and the Blueprint surface. |
| `Private/DreamPassConsole.{h,cpp}` | `r.DreamPass.Enable`, `r.DreamPass.DisablePipelines`, `r.DreamPass.Visualize` and the `DreamPass.Dump` command. |
| `Private/Materials/` | `UMaterialExpressionDreamPassOutput` and `UMaterialExpressionDreamPassBuffer` — `UE.DreamPassOutput` and `UE.DreamPassBuffer`. |
| `Private/Render/` | The renderer half, compiled at 5.8 only: the scene view extension and its scheduler, the per-frame snapshot, buffers, the fullscreen, compute, mesh, clear and copy passes, the slot global shaders `FDreamPassCS` / `FDreamPassPS`, exports, buffer visualization, and `DreamPassRendererInternal.cpp` — the one file that includes from the Renderer's `Internal` folder. |

The shaders these passes compile live in the plugin's `Shaders/Pass/`, mounted at
`/Plugin/DreamShader`.

### `Source/DreamShaderCompiler` — Editor

| Path | Responsibility |
| :-- | :-- |
| `Public/DreamShaderCompilerInterfaces.h` | `FDreamShaderCompileRequest`, `FDreamShaderCompileResult`, `IDreamShaderCompiler`. |
| `Public/DreamShaderCompileService.h` | `FDreamShaderCompileService` — argument packing over an `IDreamShaderCompiler&`. |
| `Public/DreamShaderCompilerModule.h` | `FDreamShaderCompilerModule`. |
| `Private/` | The service forwarders and the module implementation; both module methods are empty. |
| `Public/DreamShaderPassPipelines.h` *(since 2.1.0)* | The Custom Pass slot registry from outside a compile: the listing and its slot states, garbage collection, the reset of an unreadable `Registry.json`, the rewrite of the registry files, the slot pre-check of `dsc check -Shaders`, and the `.usf` ↔ `.dsp` lookups the bridge watches with. See [`DreamShaderPassPipelines.h`](../api/compiler-module.md#dreamshaderpasspipelinesh). |
| `Private/Pass/` *(since 2.1.0)* | The HLSL slots. `DreamShaderPassSlotRegistry` plans the slots, pre-checks the changed ones in process, commits the snapshots, `Registry.json` and the registry `.ush` files, and hot-reloads the slot shaders; `DreamShaderPassShaderText` reads a `.usf` as text — where a `Shader = "..."` reference lands, what the file defines, which files it includes by a relative path; `DreamShaderPassPipelines.cpp` implements the public header. |
| `Private/Pipeline/DreamShaderPipelineReferences.{h,cpp}` *(since 2.1.0)* | The two stages that cross between a `.dsp` and the `.dss` sources around it: a `.dsp`'s `Material` and `Shader` references resolved and read for the facts the binder checks, before the bind; a `.dss`'s `UE.DreamPassBuffer` resolved to its pipeline and buffer, after the lower. |
| `Private/Emitter/DreamShaderIREmitterPassPipeline.{h,cpp}` *(since 2.1.0)* | The `PassPipeline` product: the `UDreamPassPipeline`, the render targets of its exported buffers and its HLSL slots — everything staged first, so that a failure writes nothing. |

The compiler itself — the compile pipeline, the IR emitter and the asset layer — moved into this
module in 2.0; see [Compiler module](../api/compiler-module.md).

### `Source/DreamShaderEditor` — Editor

Everything is under `Private/`; nothing is exported.

| Path | Responsibility |
| :-- | :-- |
| `DreamShaderEditorModule.cpp` | Module entry. Gates the bridge on `IsRunningCommandlet()`, the cook-director check and `-NoDreamShaderEditorBridge`; owns the cook-time materialization pass. |
| `DreamShaderEditorPersistenceUtils.h` | `BindAndExecute` — the shared prepared-statement helper both SQLite writers use. |
| `Bridge/` | The [editor bridge](../tools/bridge.md): request-file polling, directory watcher, debounce and compile queue, material-compile diagnostics, and the preview WebSocket server. |
| `Commandlet/` | `UDreamShaderCommandlet` and the `compile` / `decompile` runners plus the shared argument helpers. See [Commandlet](../tools/commandlet.md). *(since 2.1.0)* `DreamShaderPassRegistryCommandlet.{h,cpp}`: the [`pass-registry`](../tools/commandlet.md#pass-registry) verb — the listing, `-Gc` and `-Rebuild`. |
| `Compile/` | `FEditorCompileAdapter` — the editor's implementation of `IDreamShaderCompiler`, and its process-wide accessor. |
| `Decompiler/` | `UMaterial` / `UMaterialFunction` → `.dsm` / `.dsf` export: the decompile service, the graph decompiler and its helpers, and layout emission. See [Decompiler](../tools/decompiler.md). *(since 2.1.0)* `DreamShaderPipelineDecompiler.{h,cpp}`: a `UDreamPassPipeline` read back into the payload a `.dsp` binds to — for `dsc decompile`, which prints it and checks the text by parsing and binding it again, and for Adopt, which splices the payload into the existing `.dsp`. |
| `DependencyGraph/` | `import` dependency tracking: `TryExtractImportPathFromLine`, `NormalizeImportSpecifier`, `ResolveImportPath` and the recursive header-dependency collection that decides which sources a `.dsh` or `.dsf` change requeues. |
| `Diagnostics/` | `FDreamShaderDiagnosticsStore`: error-location parsing, `diagnostics.json`, the per-file `diagnostics/` tree and the SQLite `diagnostics` table. |
| `MaterialAssetGeneration/` | The generator. See the file-family table below. |
| `Pass/` *(since 2.1.0)* | `FDreamPassPipelineCustomization` — the [details panel of a `UDreamPassPipeline`](../tools/editor-integration.md#pass-pipeline-details-panel), registered on every engine the asset types build on — and `DreamPassSpellings`, the `.dsp` spelling of every DreamShaderPass enumeration the decompiler, the graph dump, the details panel and `pass-keys.json` write. |
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
| Prove an engine-version gate on every supported engine | [`.skill/build-plugin.ps1`](#the-engine-matrix). |
| Reproduce the release archive | Stage the seven shipped items by hand, or push a tag and let the [release workflow](release.md) do it. |

`BuildPlugin` compiles all five modules against the target engine and fails on the first UBT or UHT
error. It is the check that matters when adding an engine-version gate, because the project build
only ever exercises one engine version. It is also the only check that sees a **link** error: an
engine class that compiles everywhere and only resolves from UE 5.6 on is invisible to every
compile-time gate. See the note under
[UE ≥ 5.6](../api/version-compat.md#ue--56) on version compatibility.

### The engine matrix

`.skill/build-plugin.ps1` runs `BuildPlugin` once per engine root, each into its own `-Package`
directory, and prints one `PASS` / `FAIL` line per engine plus the compiler diagnostics of the ones
that failed. The plugin's own `Binaries/` and `Intermediate/` are never touched. Its exit code is
the number of engines that failed, so `0` means the matrix is green.

```powershell
./.skill/build-plugin.ps1 -Engine C:\UE\UE_5.5, C:\UE\UE_5.6, C:\UE\UE_5.7
```

| Switch | Effect |
| :-- | :-- |
| `-Engine` | Engine roots. An array, or one comma-separated string |
| `-Plugin` | The `.uplugin`. Defaults to the one at or above the script |
| `-Package` | Staging root. Defaults to `%TEMP%\DreamShaderBuildPlugin`; must be outside the plugin tree |
| `-TargetPlatforms` | UAT syntax, `+`-separated. Defaults to `Win64` |
| `-StopOnFailure` | Stop at the first failing engine instead of finishing the matrix |
| `-KeepOutput` | Keep the staged plugin. Without it only the log survives — a green build is a few hundred MB per engine |
| `-Raw` | Echo the whole UAT log instead of diagnostics and step lines |
| `-NoRocket` | Drop `-Rocket` |

The full UAT log of every engine is kept under `<Package>\Logs\<Engine>.log`, pass or fail.

## Engine versions

| Engine | Status |
| :-- | :-- |
| `5.8` | Verified with `RunUAT BuildPlugin` (`2.0.2`) |
| `5.7` | Verified with `RunUAT BuildPlugin` through `1.8.0`; not re-run for `2.0.0`, `2.0.1` or `2.0.2`, whose gates are written so that 5.7 needs no answer of its own — see [Asked of the type](../api/version-compat.md#asked-of-the-type) |
| `5.6` | Verified with `RunUAT BuildPlugin` (`2.0.0`); not re-run for `2.0.1` or `2.0.2`. `2.0.1`'s two new engine headers are asked for with `__has_include`, and `2.0.2` adds no engine-version gate — see [Asked of the type](../api/version-compat.md#asked-of-the-type) |
| `5.5` | Verified with `RunUAT BuildPlugin` (`2.0.0`); not re-run for `2.0.1` or `2.0.2`. `2.0.1`'s two new engine headers are asked for with `__has_include`, and `2.0.2` adds no engine-version gate — see [Asked of the type](../api/version-compat.md#asked-of-the-type) |
| `5.4` | Source-compatible; see the toolchain warning below |
| `5.3` | Source-compatible; see the toolchain warning below |

> [!WARNING]
> **Unreal Engine `5.3` and `5.4` do not build under a recent MSVC toolchain, whatever plugin you
> point at them.** `ConcurrentLinearAllocator.h` writes `#elif __has_feature(address_sanitizer)`
> without a `defined()` guard, inside `#if PLATFORM_HAS_ASAN_INCLUDE` — which is
> `__has_include(<sanitizer/asan_interface.h>)`, true for every toolchain that ships the ASan
> headers. MSVC has no `__has_feature`, so the line raises `C4668`, and UBT compiles engine code
> with `/we4668`. UE 5.5 fixed the header by splitting the test.
>
> The failure is in **engine** code, before any plugin source is reached, and `Source/DreamShader*`
> is named nowhere in it. Epic's own `ExampleDeviceProfileSelector` fails identically on the same
> engine, which is the cheapest way to confirm the cause on a given machine. Note that installing
> MSVC `14.38` does **not** by itself help — it ships `sanitizer/asan_interface.h` too. It takes a
> toolchain without those headers, or a one-line patch to the engine header.

Every engine-version test in the plugin goes through the macros in `DreamShaderVersionCompat.h`;
there are no raw `ENGINE_MAJOR_VERSION` or `UE_VERSION_NEWER_THAN` uses anywhere in `Source/`. When
you add version-dependent code, add it there and use `DREAMSHADER_UE_VERSION_AT_LEAST(Major, Minor)`
or `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`. The one exception is Custom Pass: `DreamShaderPass.Build.cs`
tests `Target.Version` and defines `DREAMSHADER_WITH_CUSTOM_PASS`, and code that depends on
`DreamShaderPass` tests that definition with `#if` instead of the version. Both sides compile and
test on a 5.8 machine: see [Both sides of the Custom Pass gate](testing.md#both-sides-of-the-custom-pass-gate).
The complete list of currently gated behaviour is on [Version compatibility](../api/version-compat.md).

## Localization

Editor-facing text is `LOCTEXT`/`NSLOCTEXT`. The wire format is not: `diagnostics.json`,
`diagnostics/*.json`, `bridge.db`, `preview.json` and the preview WebSocket are parsed by the VSCode
and Rider extensions and must read the same in every editor culture.

The rule that keeps both true:

- **Text that reaches the wire stays an `FText` all the way there**, and the write goes through
  `ToInvariantWireString()` (`Diagnostics/DreamShaderTextWireUtils.h`). That function replays the
  `FText`'s historic format data — pattern, arguments, nested formats — in the invariant culture.
  `FTextInspector::GetSourceString` alone is not enough: for an `FText::Format` result it returns the
  *localized* substituted display.
- **Never collapse such an `FText` with `ToString()` on the way.** `ToString()` is the localized
  display, and re-wrapping the result in `FText::FromString` cannot recover the English. An API whose
  error channel is `FString`-typed — `FDreamShaderPreviewRenderContext`, the generator entry points —
  therefore keeps literal `FString::Printf` messages, marked `I18N-EXEMPT`.
- Text that only ever reaches Slate has no such constraint; localize it normally.

```bash
pwsh -File Tools/Localization/localization_lint.ps1
```

Checks `LOCTEXT_NAMESPACE` define/undef pairing, rejects the define in headers, and flags literals
gather cannot see (`FText::FromString(TEXT(...))`, `FString::Printf(TEXT(...))`). A deliberate
non-display literal is exempted with an `I18N-EXEMPT` comment **on the same line as the call** — the
check reads one line, not a span. `-IncludeDeferred` widens which files those literal rules run on.

`-EmitBaseline` regenerates `Tools/Localization/BASELINE.md`, whose gather count is the regression
check; it covers every source file, because `GatherText` does. A count that moves without a matching
`.locres` update means the translations are behind.

There is no `Config/Localization/DreamShader.ini` yet, so the Dashboard cannot re-gather the target
and the checked-in `zh-Hans` `.locres` is a hand-made snapshot. New `LOCTEXT` falls back to English
until it is regenerated.

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
  <https://shader.toolchain.64hz.cn/>.

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
- [Version compatibility](../api/version-compat.md) — the compat macros and every gated behaviour
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the headless entry point
- [Editor bridge](../tools/bridge.md) — request files, `bridge.db`, the preview WebSocket
- [Generation pipeline](../generation/index.md) — what the generator does, end to end
- [Project settings](../settings/project.md) — `UDreamShaderSettings`, including the source directory
</content>
</invoke>
