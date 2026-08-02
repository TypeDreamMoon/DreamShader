# Generation

> [DreamShader](../index.md) » **Generation**

The stage that turns a parsed DreamShaderLang translation unit into Unreal assets and one generated
HLSL include.

| | |
| :-- | :-- |
| Input | one `.dsm` or `.dsf` file, plus the transitive closure of its `import` directives |
| Rejected input | `.dsh` — a header never generates assets directly |
| Produces | `UMaterial` / `UDreamShaderMaterialInstance`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend`, and one `.ush` helper include |
| Runs in | the editor process only (the generator uses editor-only material APIs) |

## Pipeline

One compile of one source file runs this sequence. Every step is a gate: the first failure aborts the
compile and its message is returned as the compile result.

| # | Stage | What it does | Fails with |
| :-- | :-- | :-- | :-- |
| 1 | Normalize path | full path, `NormalizeFilename`, `MakeStandardFilename` | — |
| 2 | File-kind gate | `.dsh` never generates | `DreamShader header '{File}' does not generate assets directly. Recompile dependent .dsm or .dsf files instead.` |
| 3 | Load prepared source | recursive `import` inlining into one text | `DreamShader import '{Import}' referenced from '{File}' could not be resolved.` · `DreamShader import cycle detected at '{File}'.` · `DreamShader could not read '{File}'.` |
| 4 | Content gate | per-file substring scan, applied to each file's own text with its `import` lines blanked | `DreamShader header '{File}' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports.` · `DreamShader function file '{File}' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks.` |
| 5 | Parse | the whole prepared text, one parse unit | any parse diagnostic — see the [diagnostics index](../diagnostics/index.md) |
| 6 | Hash | CRC32 of the **prepared** text → the source hash | — |
| 7 | `.dsf` gate | a `.dsf` may not declare a top-level `Shader` | `{File}: .dsf files cannot define top-level Shader blocks.` |
| 8 | Write helper include | only when the unit declares at least one `Function` | `Failed to write generated helper include '{Path}'.` · `DreamShader Function '{Name}' is declared more than once.` |
| 9 | Material-function assets | one asset per `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend`, in declaration order | `{Kind} '{Name}' must declare at least one output.` · `{Kind} '{Name}' must provide a Graph block.` · asset-creation errors |
| 10 | Material asset | only when the unit declares a top-level `Shader` | `{File}: Outputs block is required.` · `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` · asset-creation errors |
| 11 | Compose result | success text, plus `Warnings:` and every parser warning when any were emitted | — |

Steps 9 and 10 share the same graph builder. Requesting only the material (the *compile material*
entry point) runs steps 1–7 and 10 and skips 9.

### Inside step 10 — the material

| # | Sub-stage | Fails with |
| :-- | :-- | :-- |
| 1 | Reject `.dsh` and `.dsf` | `DreamShader source '{File}' cannot generate a material asset directly.` |
| 2 | Require a top-level `Shader` | `{File}: This file does not define a top-level Shader block.` |
| 3 | Require a non-empty `Outputs` | `{File}: Outputs block is required.` |
| 4 | Validate `Settings`, then `Outputs` | see [Shader settings](../settings/material.md) and [Output bindings](../language/output-bindings.md) |
| 5 | Detect `Base.FrontMaterial` / `Base.MaterialAttributes` | `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` |
| 6 | Write the helper include (both backends need it) | as step 8 above |
| 7 | Resolve the backend | `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` |
| 8 | Create or reuse the target asset | see [Asset paths](asset-paths.md#diagnostics) |
| 9 | Source-hash short circuit | *(skips the rest)* |
| 10 | Build the graph, apply settings, lay out, recompile | see [Regeneration](regeneration.md) |
| 11 | Persist, or clear the dirty flag in memory-only mode | `Generated DreamShader asset '{Path}' could not be saved.` |

### Inside the graph build

Shared by both backends. For the ThinCustom backend it runs against the hidden base material, not
the emitted instance.

1. Clear the existing expressions, then reset every material property to its engine default.
2. Apply `Settings`.
3. Force `Substrate` shading when `Base.FrontMaterial` is bound.
4. Reject duplicate `Properties` names (compared ignoring case).
5. Seed one `MakeMaterialAttributes` node per uninitialized `MaterialAttributes` output declaration.
6. Build the body — either the `Graph` block, or, when the unit has no `Graph` and no initialized
   output, one whole-surface `Custom` node.
7. Connect each `Outputs` binding.
8. Lay the graph out — **skipped in memory-only mode**. See [Graph layout](graph-layout.md).
9. Recompile the material.

## What triggers a compile

| Trigger | Forced | Target | Result |
| :-- | :-- | :-- | :-- |
| Auto-compile on save (file watcher + debounce) | no | memory | hash-skip active; the most common path |
| *Generate all in-memory materials* — editor startup, and whenever the **Default Compiler Backend** setting changes | yes | memory | every project source recompiled |
| Material Content Browser *Compile* / thumbnail-refresh buttons | yes | memory | one source |
| Live preview renderer | yes | memory | one source |
| *Materialize*, and creating a child instance of a memory-only material | yes | **disk** | one source, persisted |
| Commandlet `-run=DreamShader` | caller's choice | **disk** | persisted |
| Cook, on the cook director process only | yes | **disk** | every project source persisted |

Auto-compile is governed by two project settings: **Auto Compile On Save** (default on) and **Save
Debounce Seconds** (default `0.25`, clamped to `[0.05, 10.0]`). See
[Project settings](../settings/project.md).

> [!NOTE]
> The interactive editor never writes a per-material `.uasset`. The source file is the authoring
> surface; generated assets live in memory until a cook, the commandlet, or an explicit
> *Materialize* puts them on disk. See [In-memory materials](in-memory.md).

## Outcomes that are not assets

A source file can compile successfully and produce nothing to place in the Content Browser.

| Result message | Outcome |
| :-- | :-- |
| `Generated DreamShader helper include '{Path}' from {File}.` | the unit declared only `Function` blocks — success |
| `DreamShader file '{File}' contains VirtualFunction declarations only; no assets were generated.` | success |
| `DreamShader file '{File}' contains GraphFunction declarations only; no assets were generated.` | success |
| `DreamShader file '{File}' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate.` | **failure** |

## Success messages

| Message | Emitted for |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {File}.` | each `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` |
| `Generated {AssetPath} from {File}.{Suffix}` | a `Graph`-backend material; `{Suffix}` is ` (virtual)` in memory-only mode |
| `Generated DreamShader thin-custom material {AssetPath} from {File}.` | a ThinCustom-backend material |
| `Skipped {AssetPath} from {File}; source hash is unchanged.` | the hash short circuit — see [Caching](caching.md) |
| `Generated DreamShader helper include '{Path}' from {File}.` | a unit with `Function` blocks and no assets |

Runtime substitutions are rendered as `{Placeholder}` on this page; the compiler emits the
substituted text.

## Progress reporting

Generation reports through Unreal's slow-task system. Dialogs are delayed so a fast compile never
flashes one, and are suppressed entirely under `IsRunningCommandlet()`.

| Scope | Title | Frames | Dialog delay |
| :-- | :-- | :-- | :-- |
| whole file | `Compiling DreamShader source '{File}'...` | 6 | 0.35 s |
| one material | `Generating DreamShader material from '{File}'...` | 11 | 0.25 s |
| one material function | `Generating DreamShader function '{Name}'...` | 10 | 0.25 s |
| ThinCustom emission | `Generating thin-custom material for '{Name}'...` | 8 | inherited |
| graph build (nested) | `Building material graph for '{Name}'...` | 11 | inherited |
| automatic layout | `Laying out DreamShader material graph...` | one per node | inherited |

## Pages

| Page | Covers |
| :-- | :-- |
| [Asset paths](asset-paths.md) | `Name=` + `Root=` → package path → on-disk `.uasset` |
| [In-memory materials](in-memory.md) | the ThinCustom result, the hidden base, visibility, materializing, cook |
| [Caching](caching.md) | the source hash, the metadata keys, when regeneration is skipped |
| [Graph layout](graph-layout.md) | how generated nodes are positioned, and when layout is skipped |
| [Regeneration](regeneration.md) | what a rebuild destroys and what survives |
| [Generated HLSL](generated-hlsl.md) | the `/DreamShaderGenerated/*.ush` helper include |

## Notes

- **The parse unit is the import closure, not the file.** `import` directives are inlined before
  parsing, so "one `Shader` per file" is really "one `Shader` per closure", and the source hash
  covers every imported byte. See [import](../language/import.md).
- Import lines are replaced by blank lines and each inlined file is bracketed by
  `// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` markers, so reported
  line and column numbers stay in the file the user actually edited.
- Material-function assets are generated **before** the material, so a `Shader` in the same file can
  call a `ShaderFunction` declared beside it.
- Parser warnings never fail a compile. They are appended to the result message under a `Warnings:`
  header.
- Generation is editor-only. There is no runtime code path that builds a material from
  DreamShaderLang.

## Example

```c
// DShader/Materials/M_Emissive.dsm
import "Common.dsh";

ShaderFunction(Name="Functions/F_Tint")
{
    Inputs  { vec3 InColor; vec3 InTint; }
    Outputs { vec3 OutColor; }
    Graph   { OutColor = InColor * InTint; }
}

Shader(Name="Materials/M_Emissive")
{
    Properties { vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Settings   { ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = F_Tint(vec3(1.0, 1.0, 1.0), Tint); }
}
```

One compile of that file produces:

```text
Intermediate/DreamShader/GeneratedShaders/M_Emissive_9f2c41ab.ush   (only if Common.dsh declares Function blocks)
/Game/Functions/F_Tint                                             UMaterialFunction
/Game/Materials/M_Emissive                                         UDreamShaderMaterialInstance + hidden UMaterial base
```

Result message:

```text
Generated ShaderFunction /Game/Functions/F_Tint from I:/.../M_Emissive.dsm.
Generated DreamShader thin-custom material /Game/Materials/M_Emissive from I:/.../M_Emissive.dsm.
```

## See also

- [Shader](../language/shader.md) — the block that declares a material
- [ShaderFunction](../language/shader-function.md) — the block that declares a material function
- [Source files](../language/source-files.md) — what `.dsm`, `.dsf` and `.dsh` may contain
- [import](../language/import.md) — how the parse unit is assembled
- [Backend](../settings/backend.md) — `Graph` vs `ThinCustom`, and the deprecated `Instance` alias
- [Project settings](../settings/project.md) — auto-compile, debounce, default backend, paths
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the headless generation entry point
- [Material Content Browser](../tools/material-browser.md) — the Compile / Materialize surface
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
