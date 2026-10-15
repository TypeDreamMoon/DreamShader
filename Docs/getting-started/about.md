---
title: About DreamShader
description: What the plugin is, the five modules it ships, which engines it supports, and where the related repositories live.
---

DreamShader is an Unreal Engine plugin that compiles DreamShaderLang source files into standard material assets — and,
on UE 5.8, `.dsp` Custom Pass pipelines into render passes of your own. DreamShaderLang is the language; parsing,
generation, caching, diagnostics, decompilation and editor integration are the plugin's job.

| | |
| --- | --- |
| Version | `2.2.0` — descriptor `Version` `220`, `IsBetaVersion` `false` |
| Engines | Unreal Engine `5.3` – `5.8`, Win64; Custom Pass needs `5.8` — see [Engine support](#engine-support) for what `2.2.0` was verified against |
| Modules | `DreamShaderLang`, `DreamShader`, `DreamShaderPass` (Runtime), `DreamShaderCompiler`, `DreamShaderEditor` (Editor) |
| Source files | `.dss` 2.0 compilation unit · `.dsi` material instance · `.dsp` Custom Pass pipeline · `.dsm` 1.x material · `.dsf` 1.x function · `.dsh` header |
| Plugin dependencies | `WebSocketNetworking`, `SQLiteCore` |
| Descriptor flags | `EnabledByDefault`, `CanContainContent`; `LocalizationTargets` loads `Content/Localization/DreamShader/` (`zh-Hans`) |
| License | MIT |
| Repository | [TypeDreamMoon/DreamShader](https://github.com/TypeDreamMoon/DreamShader) |

## What it is for

Unreal material graphs get hard to review, diff, reuse and migrate once they grow. DreamShader moves the repeatable
part into text:

| Pain | Answer |
| --- | --- |
| Large graphs are hard to review and diff | the graph's structure lives in source files — `.dss`, or the 1.x `.dsm` / `.dsf` |
| Shared logic gets copied around | `.dsh` headers, function assets, `/// @custom` and `Function` / `GraphFunction` helpers, packages |
| Regenerating a material function disconnects its callers | `FunctionInput` / `FunctionOutput` pin identity is preserved by name |
| Substrate graphs need to be authored as text | the `Substrate` type, `FrontMaterial`, the `Substrate.*` builtins, and `+`, `*`, `lerp` composition (since UE 5.4) |
| Existing graphs need migrating | the decompiler exports materials, material functions, material instances and Custom Pass pipelines back to `.dss` / `.dsi` / `.dsp` (or 1.x `.dsm` / `.dsf`) |
| 1.x sources need upgrading | `dsc migrate` rewrites them as `.dss`, proving the same graph before it writes (since 2.0.0) |
| Generated assets cannot be traced to a source | `DreamShader.SourceFile` and `DreamShader.SourceHash` are written to package metadata |
| A rebuild silently overwrites a hand-edited asset | the output digest `DreamShader.OutputDigest` stops the rebuild and offers Revert / Adopt / Detach (since 1.8.0) |
| A plugin wants to ship its own material sources | every enabled plugin with a `DShader` folder contributes a source root (since 1.6.0) |
| You want real expressions and control flow in the graph | `.dss`: HLSL with declarations, one asset per `export` (since 2.0.0) |
| Material instances are clicked together in a panel | `.dsi`: the parent and the overrides as text (since 2.0.0) |
| You want render passes the engine does not have — outline masks, blurs, screen-space effects, compute writing data textures | `.dsp` Custom Pass pipelines, without changing the engine (since 2.1.0), on (since UE 5.8) |

It is not a replacement for the Material Editor. A 1.x `Graph` is a node-graph builder with four operators and
`if` / `else`; `.dss` has loops and the full operator set, but it too is lowered to a material graph. Genuinely
imperative logic belongs in a `/// @custom` function or a 1.x `Function`, whose body is real HLSL compiled into a
`Custom` node. One-off, highly visual graphs are faster to wire in Unreal — export them to source once they settle.

## The five modules

| Module | Type | Public headers | Responsibility |
| --- | --- | --- | --- |
| `DreamShaderLang` (since 2.0.0) | Runtime | 31 | The language itself, depending on `Core` only: lexing, both parsers (the 2.0 one and the 1.x front end), the preprocessor, the binder, the engine-independent graph IR, the path from IR back to source, and the 1.x → 2.0 migrator. |
| `DreamShader` | Runtime | 8 | The log category, normalized-path helpers, the 1.x source data model, the engine side of the define table, `UDreamShaderSettings`, `UDreamShaderMaterialInstance`, the engine-version macros, and the compiler interface `IDreamShaderCompiler`. |
| `DreamShaderPass` (since 2.1.0) | Runtime | 11 | The Custom Pass runtime: the pipeline asset `UDreamPassPipeline`, its settings, the subsystem, `ADreamPassVolume` and `UDreamPassComponent`, the scene view extension and the passes. It builds on every supported engine; passes run on 5.8 only. |
| `DreamShaderCompiler` | Editor | 20 | The back half of the compiler: the compile pipeline, the IR emitter, the asset layer, the builtin catalog read from reflection, the product index, the parent schema of a `.dsi`. Through `1.9.x` it was an interface-only Runtime module. |
| `DreamShaderEditor` | Editor | **0** | Everything a user touches: the decompiler, `dsc migrate`, the commandlet, the bridge, previews, the Material Content Browser, the provenance actions, the workspace export. |

The three Runtime modules load at `PostConfigInit`, early enough that the settings object, the shader directory
mappings and `DreamShaderPass`'s global shader types are in place before the engine commits its shader type list; the
two Editor modules load at `Default`.

**Note:**

`DreamShaderEditor` exports no symbols and cannot be linked against — it has no `Public/` folder at all. The supported
C++ entry points are `IDreamShaderCompiler` — declared in `DreamShader` (`DreamShaderCompilerInterface.h`), obtained
through `GetDreamShaderCompiler()`, implemented by `DreamShaderCompiler` — and `DreamShaderPass`'s C++ and Blueprint
API. Everything else is reached through the editor UI, the commandlet or the bridge files.

The language itself is engine-independent: `DreamShaderLang` depends on `Core` only, creates no `UObject` and loads
no asset. Generation is the opposite — editor-only, game-thread-only, built on the material editing library.

## Engine support

`2.2.0` was verified with a single-plugin `RunUAT BuildPlugin` on Win64 against `5.5`, `5.6` and `5.8`. `5.7` was last
verified at `1.8.0`: every engine gate added since asks the engine's own headers or types rather than its version
number, so 5.7 needs no answer of its own. `5.3` and `5.4` are source-compatible, but a recent MSVC toolchain does not
build those engines — the failure is in engine headers, before plugin code is reached; see
[Installation ▸ Engine compatibility](installation.md#engine-compatibility).

Feature availability varies by version. The plugin binary is built per engine, so moving a project to a newer engine
means rebuilding the plugin.

| Requires | Feature |
| --- | --- |
| **5.4** | Substrate — `Substrate.*`, `ShadingModel="Substrate"`, `Base.FrontMaterial`, the `Strata` alias · generated `Custom` nodes collapse their code by default · `bHasPixelAnimation` reset and export |
| **5.5** | the `PeriodicWorld` transform basis · `UE.TransformPosition(PeriodicWorldTileSize=…)` |
| **5.6** | the `FirstPersonTranslatedWorld` transform basis · `UE.TransformPosition(FirstPersonInterpolationAlpha=…)` · the plugin mount check on `Root=` and `Path(…)` · `TextureSample.GatherMode` round-trip · `Substrate.Select` |
| **5.7** | `Group` / `SortPriority` on collection parameters · `BlendInputRelevance` on layer-blend inputs · per-platform, per-quality material resource diagnostics |
| **5.8** | Custom Pass — running `.dsp` pipelines, `UE.DreamPassOutput` / `UE.DreamPassBuffer` · `Substrate.Toon` · `.dsi` overrides of parameter-collection parameters · double-vector and static-component-mask overrides kept by the ThinCustom instance write-back (the last two are decided by the engine's own headers, so a 5.7 that has those members gets them too) |

Features not listed behave the same from 5.3 to 5.8. The node catalog follows the running engine: a class or enum value
the engine does not have is refused explicitly (`DSH5210`, `DSH5215`, `DSH5294`, …) rather than silently downgraded.
See [Limitations](../diagnostics/limitations.md#engine-version-boundaries).

## Who implements what

DreamShader is really two separate products that talk through files under `<Project>/Saved/DreamShader/Bridge/` and a
local loopback WebSocket.

| Capability | Implemented by |
| --- | --- |
| Parsing, generation, caching, diagnostics, decompilation, migration | the Unreal plugin |
| Menus, toolbar, context menus, the Material Content Browser tab | the Unreal plugin |
| Preview rendering and PNG frames | the Unreal plugin |
| The Custom Pass runtime | the Unreal plugin (`DreamShaderPass`) |
| Creating `DShader/Packages`, `import` / `#include` resolution, compile exclusions | the Unreal plugin |
| Highlighting, completion, hover, navigation, local diagnostics | the editor extension |
| Preview camera controls and frame acknowledgement | the editor extension |
| `dreamshader.package.json`, `dreamshader.lock.json`, install / update / store | the editor extension |

The plugin's C++ never reads either package file. A package resolves only because its files sit under
`DShader/Packages`.

## Repositories

  - [DreamShader](https://github.com/TypeDreamMoon/DreamShader) — The Unreal plugin: the compiler, editor tools, the Custom Pass runtime, the agent skills, and the reference manual this documentation is based on. MIT licensed.
  - [VSCode extension](https://github.com/TypeDreamMoon/dreamshader-language-support) — Currently 2.1.0: highlighting, snippets, completion, go to definition, find references, hover, signature help, local and bridge diagnostics, material preview, package commands and templates for `.dss` / `.dsi` / `.dsp` and the 1.x sources.
  - [Rider plugin](https://github.com/tsdaer/dreamshader-language-support) — `.dsm` / `.dsf` / `.dsh` file types, grammar and PSI parsing, highlighting, completion, navigation, diagnostics, bridge integration, semantic tokens, inlay hints, package tools.
  - [Package Store Index](https://github.com/TypeDreamMoon/dreamshader-package-index) — The index the extension's package store reads. Repositories with the GitHub topic `dreamshader-package` are discovered as well.

Report problems on the plugin repository's
[issue tracker](https://github.com/TypeDreamMoon/DreamShader/issues). The extension's own settings, commands and package
store are documented in the extension repository, not here.

## Next

- [Installation](installation.md) — put it in a project
- [Your first material](first-material.md) — write a `.dss` and find it
- [Daily workflow](workflow.md) — the compile loop
- [Custom Pass](../language-v2/passes.md) — `.dsp` pipelines
- [Changelog](../changelog/dreamshader.md) — what changed in `2.2.0`
