---
title: Limitations
description: What DreamShader 2.2.0 does not do — the language boundaries of a 1.x Graph and of .dss, what 1.x did that still happens but is now said out loud, the decompiler's gaps, the function-level ceiling on material layers, and the engine-version and Custom Pass boundaries.
---

DreamShaderLang covers the material-authoring patterns that are worth keeping as reviewable text. It is not a
replacement for the Unreal Material Editor and it is not a shader compiler. This page is an honest list of where the
edges are in **2.2.0**, so you find them here rather than three hours into a shader.

Each section links the page that covers the topic in depth. The detail is not repeated below.

**Note:**

**(since 2.0.0) Crossing a boundary is an error, not something compiled silently into something else.** The 1.x
generator truncated an expression at an operator it did not know and quietly took the later of two settings; today
each such case has a `DSHnnnn` code at the line and column of the construct. Every boundary on this page tells you
when you hit it — the codes are on [Diagnostics](index.md).

## A 1.x `Graph` is not a general-purpose language

A `Graph` block in a `.dsm` / `.dsf` builds a node graph. It has a fixed set of statement forms, four arithmetic
operators, and no control flow beyond `if` / `else`. The legacy front end reads it with the 2.0 parser, then refuses
what 1.x did not have:

| Does not exist | Code | Use instead |
| :-- | :-- | :-- |
| `for`, `while`, `do`, `return`, `break`, `continue`, `discard` | `DSH2208` | unroll by hand, or move the loop into a [`Function`](../language/functions.md) whose body is real HLSL |
| `switch` | `DSH2160` | an `if` chain, or a `StaticSwitchParameter` call |
| ternary `? :`, a comparison outside an `if` condition, `&&`, `\|\|`, `!` | `DSH2200` | `if` / `else`, `lerp`, or a `StaticSwitchParameter` call |
| `%`, bitwise and shift operators | `DSH2201` | `fmod(a, b)`, or a `Function` |
| a second comparison in an `if` condition | `DSH2210` | nested `if` statements |
| indexing `v[i]`, arrays | `DSH2202`, `DSH2213` | [swizzles](../graph/swizzle.md); pick a node output by number with `OutputIndex = k` |
| compound assignment and `++` / `--` | `DSH2205`, `DSH2207` | `a = a + b` |
| C-style casts | `DSH2203` | constructors: `float3(x)` |
| hex literals, nested brace initializers, assignment inside an expression | `DSH2222`, `DSH2214`, `DSH2204` | decimal literals, constructors, separate statements |
| matrix types | `DSH4361` | [`UE.TransformVector` / `UE.TransformPosition`](../builtins/ue.md#uetransformvector), or matrix locals inside a `Function` body |
| integer arithmetic | — | `int`, `uint`, `bool` and `half` are floats in the graph; a division of two integer constructors is `DSH4243` |

This is by design, not a to-do list. Anything genuinely imperative belongs in a `Function` or `GraphFunction` — whose
body is HLSL compiled into a `Custom` node — or in a `.dss`: most of the constructs above are legal in
[DreamShaderLang 2.0](../language-v2/index.md), and [`dsc migrate`](../tools/migrate.md) rewrites a whole 1.x file
into it. The full list is on [Unsupported constructs](../graph/unsupported.md).

### What 1.x did, still done, now said

What 1.x documented and a 1.x source still relies on keeps working, but each case now carries a warning or an info —
the [legacy rules](index.md#1x-messages-and-their-codes). In a `.dss` the same text is an error.

| Written | Result today |
| :-- | :-- |
| a name that matches only when case is ignored | resolves, with the warning `DSH5275` (`DSH5276` for an engine name) |
| the GLSL spellings `mix`, `fract`, `mod`, `vec3` | read as the HLSL name, with the warning `DSH5277`; in a `.dss` the error `DSH4250` |
| a wider value into a narrower place (`float4` into a `vec3`) | its leading components are taken, with the info `DSH5289` |
| assigning to an undeclared name, or passing one as an out target | a local is declared on the spot, with the info `DSH5292` / `DSH5283` |
| a repeated header attribute, a repeated `Settings` key, a second `Graph` / `Layout` section | the later one wins, with the warnings `DSH2244`, `DSH3262`, `DSH2258` |
| an unknown key in a material-function `Settings`, a default on a function's `Outputs` entry | ignored, with the warnings `DSH3263`, `DSH3273` |
| an argument one of the 27 1.x `UE.*` names does not read | dropped, with the warning `DSH5254` |

**Warning:**

`if` and `else` are **case-sensitive** keywords. `If (x) { … }` is a call to a function named `If`, and the `{` after
it is `DSH2154`. Block keywords (`Shader`, `ShaderFunction`, …) and `SampleTexture2D` are case-sensitive too; section
names, type tokens and constructor names are not. See [Lexical rules](../language/lexical.md#case-sensitivity).

## What `.dss` does not have either

The 2.0 language has loops, `?:`, `&&` and compound assignment, but it is still lowered to a material graph, and a
graph has its limits:

| Not there | Instead |
| :-- | :-- |
| loops whose count is known only at run time, `discard` in a branch | write them in a `/// @custom` function |
| matrices in the graph | compute them in a `/// @custom` body |
| `switch`, the comma operator, `let` / `auto` | an `if` chain; spell the type |
| a material layer **stack** | `@layer` and `@layerblend` produce the two function kinds; the stack is assembled in the editor |

See [DreamShaderLang 2.0 ▸ What it does not have](../language-v2/index.md#what-the-pipeline-does-not-do-yet).

## Diagnostics

Every diagnostic has a code, a severity and its own line and column (since 2.1.0), and a stage reports all of its
errors. The remaining edges are elsewhere:

- The bridge does not run inside a commandlet or a cook, so a headless run produces no `diagnostics.json`, no shards and
  no `bridge.db` rows — only log lines, and a `-DiagnosticsOut` file. See [Commandlet](../tools/commandlet.md).
- The VirtualFunction sync service's messages have no `DSHnnnn` code (except `DSH9001`, for a source that uses `#if`);
  they are stored with `code = virtual-function-sync`.
- The site has no page per code; each code's messages, cause and fix are in the plugin repository's
  [`Docs/diagnostics/`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md).

**Warning:**

**One case is still silent: a texture that fails to load.** When the asset written to a texture property named
`Texture` or `TextureObject` does not load, a null is written and reported as success — `[Texture = Path(Game, "Typo")]`
builds with no diagnostic and leaves an unbound sampler; a texture-sample token's and a `const` texture's default go the
same way. When a sampler comes out wrong, check the asset path first. See [Metadata](../parameters/metadata.md#object-properties).

## The decompiler is a migration aid, not a round-trip

Exporting an existing `UMaterial`, `UMaterialFunction` or material instance gives you a working starting point, not a
guaranteed reproduction.

- **The 2.0 decompiler** (*Export .dss* / *Export .dsi*, `dsc decompile`) (since 2.0.0) runs the compiler
  backwards, and parses the printed text again before it writes it (`DSH9089` otherwise). Everything the language
  cannot state is **named**, at the top of the file — `DSH9060`–`DSH9074` on import (a missing function asset, a class
  outside the catalog, pins in an array, …), `DSH9075`–`DSH9084` on writing. A material instance is written as a
  `.dsi`, a Custom Pass pipeline as a `.dsp`.
- **The 1.x exporter** (*Export Legacy .dsm / .dsf*) keeps its 1.5.0 gaps: material-function settings are not written,
  only a curated set of `UMaterial` properties is, struct / array node properties are not reflected, comment text and
  `SortPriority` are dropped, a cycle in the graph and a `MaterialFunctionCall` with no function become constants, and
  material instances are refused outright.

| Shared by both | Consequence |
| :-- | :-- |
| the written path points at `Decompiled/…` | recompiling creates a **second** asset rather than replacing the original — the 2.0 decompiler's `-KeepAssetPath` writes a `/// @name` line that takes the original path over |
| node layout depends on the *Export Decompiled Layout* setting | with it off, the regenerated graph is laid out automatically |

Treat an export as a draft: compile it, compare the two materials in the editor, and take over the original path once
you trust it. The full lists are on [Decompiler ▸ Known gaps](../tools/decompiler.md#known-round-trip-gaps) and
[The 2.0 decompiler](../tools/decompiler.md#the-20-decompiler).

## Material layers are function-level only

`ShaderLayer` and `ShaderLayerBlend` (`/// @layer` and `/// @layerblend` in 2.0) generate the two layer *function*
assets — `UMaterialFunctionMaterialLayer` and `UMaterialFunctionMaterialLayerBlend`. That is where support ends.

| Boundary | Detail |
| :-- | :-- |
| Layer **stacks** cannot be authored from source | assigning layers and blends to a material or instance is still an editor operation |
| Layer interfaces are fixed | a layer always has exactly one `MaterialAttributes` input (the material it is applied to); a blend at least one `MaterialAttributes` input, and the engine's layer stack blends two. Both produce exactly one `MaterialAttributes` output. Anything else is `DSH6204` / `DSH6205` / `DSH3278` |
| Scalars, vectors and textures are not layer inputs | expose them through `Properties`; they become parameter nodes inside the generated function and show up on the layer stack's parameter panel |
| Layers cannot be called from a `Graph` | `DSH6208`; their consumer is the layer stack of a material or instance |
| `BlendInputRelevance` | written from the input names on (since UE 5.7) only |
| `MaterialLayer` / `MaterialLayerBlend` are deprecated | since 1.3.0; both still generate identical assets, with the warning `DSH2251` |

(since 2.0.0) `Base.MaterialAttributes` and `Base.FrontMaterial` on one `Shader` are no longer exclusive: both are
wired, and *Use Material Attributes* makes the material read the attribute set. The signature rules are on
[Top-level blocks](../language/blocks.md#shaderlayer-and-shaderlayerblend).

## Node coverage is reflection-shaped

`UE.<Name>(…)` reaches any material expression class of the running engine — by its short name (`Sine`,
`CameraVectorWS`) or by a 1.x alias the node catalog carries; [`UE.Expression(Class = …)`](../builtins/ue-expression.md)
is the same path. Some consequences:

- **Struct and array properties are awkward.** They go through Unreal's own text import, so they need Unreal's literal
  syntax — `(R=1,G=0,B=0,A=1)`; a node that keeps its pins in an array of structs (the Landscape layer blend, …) is
  wired by the names each node shows.
- **The catalog follows the engine.** A class the running engine does not have is `DSH5210`; a value its enum does not
  have (the `PeriodicWorld` basis on an older engine, say) is `DSH5215`.
- **The 27 1.x names keep their 1.x argument lists.** An argument they do not read is dropped with the warning
  `DSH5254`; in a `.dss` the same argument is the error `DSH5213`.
- **Identical calls are one node.** The IR merges nodes with the same class, properties and inputs, whatever the
  builtin (since 2.0.0).

## Engine-version boundaries

Supported engines are **UE 5.3 – 5.8** (Win64). Features that depend on a newer engine are refused explicitly, not
silently:

| Feature | Requires |
| :-- | :-- |
| `Substrate.*` calls, `Substrate` values, `Base.FrontMaterial`, `ShadingModel = Substrate` | (since UE 5.4) |
| a run-time branch between two Substrate values (`Substrate.Select`) | (since UE 5.6) — `DSH4378` below it |
| the `PeriodicWorld` basis on `UE.TransformPosition` | (since UE 5.5) |
| the `FirstPersonTranslatedWorld` basis and `FirstPersonInterpolationAlpha` on `UE.TransformPosition` | (since UE 5.6) |
| the plugin-content mount check on `Path( … )` and `Root=`, `GatherMode` on texture samples | (since UE 5.6) |
| `BlendInputRelevance` on `ShaderLayerBlend`, `Group` / `SortPriority` on `UE.CollectionParam` | (since UE 5.7) |
| running Custom Pass pipelines, `Substrate.Toon` | (since UE 5.8) |

Substrate also requires the project to have Substrate enabled, independently of the engine version. The
`DreamShaderPass` module builds on every supported engine; passes run on 5.8 only.

## Backend boundaries

`ThinCustom` is the default backend: the graph is built on a hidden base material, and the asset path holds a
`UDreamShaderMaterialInstance`. The consequence is that interactive compiles are **Ephemeral** by default — in memory
only — so the generated material does not appear in the Content Browser until you *Materialize* it, and a cook
materializes it automatically. This is intentional, not a fault; see [In-memory materials](../generation/ephemeral.md)
and [Backend](../settings/backend.md). For an ordinary `UMaterial` saved on every build, set `Backend = "Graph"`.

`Backend = ""` is the error `DSH7201` (1.x read it as `Graph`); `Instance` is the old spelling of `ThinCustom`, with the
warning `DSH7204`.

## Imports and packages

| Boundary | Detail |
| :-- | :-- |
| Import cycles are rejected | diamonds are fine; a cycle is `DSH4211` |
| One `Shader` per file | `DSH2250`; per file now, not across the import closure |
| Only a `.dsh` can be imported | importing a `.dsf` / `.dsm` is `DSH2252`; a function another file generates is called through a `VirtualFunction` / `extern` prototype |
| A `.dsh` holds functions and `VirtualFunction`s | decided per declaration, `DSH2249` — `Shader(` in a comment is fine |
| Sources under `DShader/Packages` | `Library/*.dsh` is never compiled directly and can be imported; `Examples/*.dsm` is never compiled on any path; the rest is on [Packages](../tools/packages.md#source-file-enumeration) |
| Resolution is root-relative | absolute machine paths do not travel; keep imports relative to the file, the source root or a package |

## Custom Pass boundaries

[Custom Pass](../language-v2/passes.md) pipelines run on (since UE 5.8) only. A buffer's `Mips` must be `1`
(`DSH7308` (since 2.1.2)); mesh passes cannot draw Nanite primitives and have no per-instance culling; HLSL passes
cannot ship in a DLC; a material with `UE.DreamPassOutput` or `UE.DreamPassBuffer` cannot use the new material
translator. The full list is on [Runtime ▸ Limits](../language-v2/passes.md#what-the-compiler-checks).

## What is deliberately not a goal

DreamShaderLang exists so that behaviour which must be reviewed, regenerated and shared lives as text. It does not try
to absorb the Material Editor. Exploratory, highly visual one-off graphs are faster to build in Unreal — build them
there, and [decompile](../tools/decompiler.md) the parts that settle into source.

## Where to next

  - [Diagnostics](index.md) — Where diagnostics appear, how to read them, and the common codes.
  - [Unsupported constructs](../graph/unsupported.md) — The full version of the 1.x Graph language boundary, each construct with its code.
  - [DreamShaderLang 2.0](../language-v2/index.md) — .dss: where loops, ?: and && live.
  - [Decompiler](../tools/decompiler.md) — What the exporter writes, and the full list of gaps.
  - [Backend](../settings/backend.md) — ThinCustom versus Graph, and what each one generates.
  - [Top-level blocks](../language/blocks.md) — ShaderLayer, ShaderLayerBlend and the rest of the block catalogue.
