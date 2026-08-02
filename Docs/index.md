# DreamShader reference

DreamShader compiles **DreamShaderLang** source files — `.dsm`, `.dsf`, `.dsh` — into standard Unreal
Engine material assets: `UMaterial`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, and
`UMaterialFunctionMaterialLayerBlend`. Source files are the authoring surface; the assets are build
output and can always be regenerated.

| | |
| :-- | :-- |
| Version | `1.5.1` |
| Engines | Unreal Engine `5.3` – `5.8` (Win64 verified) |
| Modules | `DreamShader` (Runtime), `DreamShaderCompiler` (Runtime), `DreamShaderEditor` (Editor) |
| Source extensions | `.dsm` material · `.dsf` function · `.dsh` header |
| Project settings | *Project Settings ▸ DreamPlugin ▸ Dream Shader* |
| License | MIT |

New here? Start with **[Getting started](getting-started.md)**, then **[Examples](examples/index.md)**.

---

## DreamShaderLang

### [Language reference](language/index.md)

The declaration grammar: source files, blocks, and sections.

| | |
| :-- | :-- |
| [Source files](language/source-files.md) | `.dsm` / `.dsf` / `.dsh` and what each may contain |
| [Lexical elements](language/lexical.md) | Comments, identifiers, case sensitivity, literals, suffixes |
| [Keyword index](language/keywords.md) | Every reserved word and alias, with links |
| [`import`](language/import.md) | Including headers, functions, and packages |
| [Types](language/types.md) | Type tokens, GLSL aliases, per-context validity |

**Top-level blocks**

| | |
| :-- | :-- |
| [`Shader`](language/shader.md) | Generates a `UMaterial` |
| [`ShaderFunction`](language/shader-function.md) | Generates a `UMaterialFunction` |
| [`ShaderLayer` / `ShaderLayerBlend`](language/shader-layer.md) | Generates native material layer functions |
| [`VirtualFunction`](language/virtual-function.md) | Declares an existing `UMaterialFunction` |
| [`Function`](language/function.md) | Reusable HLSL helper |
| [`GraphFunction`](language/graph-function.md) | HLSL helper that may pull `UE.*` nodes into its inputs |
| [`Namespace`](language/namespace.md) | Groups helpers under `Ns::Name` |

**Sections**

| | |
| :-- | :-- |
| [`Properties`](language/properties.md) | Parameter declarations and group scopes |
| [`Inputs` / `Outputs`](language/inputs-outputs.md) | Function signatures and material outputs |
| [Output bindings](language/output-bindings.md) | `Base.<Target> = <value>;` |
| [`Settings`](settings/index.md) | Material and function settings |
| [`Options`](language/options.md) | `VirtualFunction` asset binding |
| [`Layout`](language/layout.md) | Node placement, comment boxes, `#Region` |

### [Graph language](graph/index.md)

The statement and expression language inside `Graph = { ... }`, which materialises material nodes.

| | |
| :-- | :-- |
| [Statements](graph/statements.md) | Every statement form |
| [Declarations](graph/declarations.md) | Variables, initialisers, scope |
| [Expressions and operators](graph/expressions.md) | Precedence, operand rules, integer division |
| [Literals](graph/literals.md) | Numeric, boolean, suffixes |
| [Constructors](graph/constructors.md) | `float3(...)`, `vec4(...)`, integer constructors |
| [Swizzles](graph/swizzle.md) | Channel masks, reorder, repeat |
| [Conversions](graph/conversions.md) | Widening, narrowing, component-count rules |
| [`if` / `else`](graph/if.md) | Conditions, truthiness, branch selection |
| [`MaterialAttributes`](graph/material-attributes.md) | Struct-like member writes |
| [Calls](graph/calls.md) | Calling functions and parameter pins |
| [Name resolution](graph/name-resolution.md) | Lookup order and shadowing |
| [Node reuse](graph/node-reuse.md) | Common-subexpression deduplication |
| [Unsupported constructs](graph/unsupported.md) | Loops, `return`, `%`, indexing — and what happens instead |

---

## Node library

### [Builtins](builtins/index.md)

| | |
| :-- | :-- |
| [`UE.*` catalogue](builtins/ue.md) | Every named material-node builtin |
| [`UE.Expression`](builtins/ue-expression.md) | The generic reflected-node escape hatch |
| [`OutputType` values](builtins/output-type.md) | Every accepted output-type token |
| [Math builtins](builtins/math.md) | `lerp`, `saturate`, `frac`, … |
| [Transform builtins](builtins/transform.md) | `UE.TransformVector` / `UE.TransformPosition` bases |
| [`Substrate.*`](builtins/substrate.md) | Substrate nodes (UE 5.4+) |
| [`DreamShaderBuiltins.ush`](builtins/hlsl-library.md) | Shipped HLSL helper header |

### [Parameters](parameters/index.md)

| | |
| :-- | :-- |
| [Compact types](parameters/compact-types.md) | `float`, `float3`, `Texture2D`, … |
| [Parameter nodes](parameters/parameter-nodes.md) | `ScalarParameter`, `StaticSwitchParameter`, … |
| [Metadata block](parameters/metadata.md) | `Group`, `SortPriority`, `Slider`, reflected properties |
| [`SamplerType`](parameters/sampler-type.md) | Texture sampler configuration |
| [Using parameters in `Graph`](parameters/graph-usage.md) | Value reads and pin call forms |
| [`Path(...)`](parameters/path.md) | Asset references |

---

## Build and output

### [Settings](settings/index.md)

| | |
| :-- | :-- |
| [Material settings](settings/material.md) | Special keys plus reflected `UMaterial` properties |
| [Enum values](settings/material-enums.md) | Every `ShadingModel`, `BlendMode`, `Domain` |
| [`Backend`](settings/backend.md) | `ThinCustom` vs `Graph` |
| [Function settings](settings/function.md) | Keys honoured by material functions |
| [Project settings](settings/project.md) | *DreamPlugin ▸ Dream Shader* |

### [Generation](generation/index.md)

| | |
| :-- | :-- |
| [Asset paths](generation/asset-paths.md) | `Name=` and `Root=` to package and disk paths |
| [In-memory materials](generation/in-memory.md) | The thin instance, hidden base, cook behaviour |
| [Caching](generation/caching.md) | Source hashing and skipped rebuilds |
| [Graph layout](generation/graph-layout.md) | Automatic placement and its limits |
| [Regeneration](generation/regeneration.md) | What survives a rebuild and what does not |
| [Generated HLSL](generation/generated-hlsl.md) | The emitted `.ush` include |

---

## Tooling

### [Editor tools](tools/index.md)

| | |
| :-- | :-- |
| [Editor integration](tools/editor-integration.md) | Menus, toolbar, context menus |
| [Material Content Browser](tools/material-browser.md) | Browse, filter, instance, materialize |
| [Preview](tools/preview.md) | Thumbnail and streaming preview |
| [Decompiler](tools/decompiler.md) | Export existing materials to `.dsm` / `.dsf` |
| [VirtualFunction tools](tools/virtual-function-tools.md) | Declare and sync existing functions |
| [Workspace and editor extensions](tools/workspace.md) | VSCode workspace, VSCode and Rider plugins |
| [Packages](tools/packages.md) | `DShader/Packages` shared libraries |
| [Commandlet](tools/commandlet.md) | Headless compile and decompile |
| [Editor bridge](tools/bridge.md) | Request files, WebSocket protocol, artifacts |

### [C++ API](api/index.md)

| | |
| :-- | :-- |
| [`DreamShaderModule.h`](api/dreamshader-module.md) | Module, log category, exported functions |
| [`DreamShaderTypes.h`](api/types.md) | The parsed-source data model |
| [`DreamShaderParser.h`](api/parser.md) | Parser entry point |
| [`DreamShaderSettings.h`](api/settings.md) | `UDreamShaderSettings` |
| [`DreamShaderMaterialInstance.h`](api/material-instance.md) | The generated instance class |
| [`DreamShaderVersionCompat.h`](api/version-compat.md) | Engine-version macros |
| [`DreamShaderCompiler`](api/compiler-module.md) | Compiler interfaces and service |

---

## Reference

| | |
| :-- | :-- |
| [Diagnostics index](diagnostics/index.md) | Every error and warning, with cause and fix |
| [Examples](examples/index.md) | Complete, copy-pasteable sources |
| [Contributing](contributing/index.md) | Building the plugin |
| [Testing](contributing/testing.md) | Automation suite and the fixture corpus |
| [Release](contributing/release.md) | Tagging and packaging |

---

## External links

| | |
| :-- | :-- |
| Repository | <https://github.com/TypeDreamMoon/DreamShader> |
| Issues | <https://github.com/TypeDreamMoon/DreamShader/issues> |
| Documentation site | <https://lang.64hz.cn/> |
| VSCode extension | <https://github.com/TypeDreamMoon/dreamshader-language-support> |
| Rider plugin | <https://github.com/tsdaer/dreamshader-language-support> |
| Changelog | [../CHANGELOG.md](../CHANGELOG.md) |
