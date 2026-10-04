---
name: dream-shader-decompile
description: Export an existing Unreal material, material function, layer, layer blend, material instance or Custom Pass pipeline back into DreamShaderLang source headlessly, so a hand-built material graph or pipeline can be migrated to a text source file. Use when asked to decompile, export, reverse, convert, or migrate a material / material function / material layer / material instance / pass pipeline asset to .dss, .dsi or .dsp (or, with -Format Legacy, a material or function to .dsm / .dsf).
---

# dream-shader-decompile `<asset>`

Read an existing asset's node graph and write the DreamShaderLang source that builds it. Since 2.0
the default output is **2.0 text** — a `.dss`, a `.dsi` for a material instance, or *(since 2.1.0)*
a `.dsp` for a Custom Pass pipeline — written by the compiler run backwards (graph → IR → AST →
printer; a pipeline has no graph, so its fields are read back into the payload a `.dsp` binds to). The 1.x exporter that writes `.dsm` /
`.dsf` is still there behind `-Format Legacy`.

This is the front half of a migration; the back half is
[`dream-shader-optimize`](../dream-shader-optimize/SKILL.md).

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Run it

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 decompile /Game/Materials/M_Steel
```

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 decompile /Game/Materials/MI_Steel_Worn
```

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 decompile /MyPlugin/Materials/M_RectBlock -Out D:/Work/M_RectBlock.dss -KeepAssetPath
```

Exit `0` writes the file and logs where, and in which format:

```text
LogDreamShader: Display: DreamShader decompiled '/Game/Materials/M_Steel.M_Steel' to '…/M_Steel.dss' (dss).
dsc: OK (exit 0)
```

| Switch | |
| :-- | :-- |
| `-Out <file>` | where to write. Its extension picks the format when `-Format` is absent: `.dsm` / `.dsf` is 1.x text, anything else 2.0. |
| `-Format Dss\|Legacy\|Auto` | `Auto` is the default. A format that contradicts the extension — or a `.dss` for an instance — is `DSH9085`. |
| `-SourceFile <source>` | instead of an asset: decompile **every asset that source builds** into one file. Build the source first (`DSH9087`). |
| `-KeepAssetPath` | write `/// @name <the asset's own path>` wherever the output file's place would name another asset, so a rebuild **takes over the original**. A `.dsp` has no `/// @name`: a pipeline keeps its path only when the file is written where its source belongs, and anywhere else is `DSH9224` (warning). |
| `-Readable` | prefer HLSL sugar over class-exact node calls. Easier to read; a rebuilt graph may then differ in node classes. |
| `-DiagnosticsOut <file>` | the decompile's diagnostics as JSON, written whether or not it succeeded. |

Without `-Out` the destination is computed from the asset's package path:

| Asset class | Lands in | Extension |
| :-- | :-- | :-- |
| `UMaterial` | `DShader/Decompiled/Materials/<package path>` | `.dss` |
| `UMaterialFunction` | `DShader/Decompiled/Functions/<package path>` | `.dss` |
| `UMaterialFunctionMaterialLayer` | `DShader/Decompiled/Layers/<package path>` | `.dss` |
| `UMaterialFunctionMaterialLayerBlend` | `DShader/Decompiled/LayerBlends/<package path>` | `.dss` |
| `UMaterialInstanceConstant` | `DShader/Decompiled/Instances/<package path>` | `.dsi` |
| `UDreamPassPipeline` *(since 2.1.0)* | `DShader/Decompiled/Pipelines/<package path>` | `.dsp` |

So `/Game/Materials/Metal/M_Steel` → `DShader/Decompiled/Materials/Game/Materials/Metal/M_Steel.dss`.
With `-Format Legacy` the same places get `.dsm` / `.dsf`, and an instance or a pipeline is refused.

## What comes out

```hlsl
// Decompiled by DreamShader from /Game/Materials/M_Steel.M_Steel
uniform float A = 1.0;
uniform float B = 2.0;

export void M_Steel(inout material m)
{
    float Sum = A + B;
    float Twice = Sum * Sum;

    m.Opacity = saturate(Twice - Sum);
    m.Roughness = A * 0.5;
}
```

- Uniforms come out in the material's display order; `/// @sort` is written only where a priority is
  not the uniform's place in that order.
- A value gets a local when it is read twice, or when the statement that made it named it; the rest
  is written inline.
- A Custom node comes back as a `/// @custom` function with its helpers; a function asset the graph
  calls becomes an `extern` prototype with `/// @asset`.
- Comment boxes are `#pragma region`s, node positions are `#pragma layout` hints.
- A material instance comes out as a [`.dsi`](../../Docs/language-v2/instances.md): its parent, the
  `#pragma instance` keys it overrides, and only the parameters that **differ from the parent**.
- A DreamShader thin-custom pair (hidden `MB_DreamThinBase_*` base + instance) is decompiled as the
  one material it is — pass the instance. `Backend = ThinCustom` is written only where it is not the project
  default anyway, or with `-SourceFile` / `-KeepAssetPath`.
- A Custom Pass pipeline comes out as a [`.dsp`](../../Docs/language-v2/passes.md): `#pragma pipeline`,
  the uniforms, the buffers and the passes in the asset's order, every key that would read back as its
  default left out. What the text cannot say the way the asset has it is `DSH9211`–`DSH9220`; the text is
  then parsed and bound again and compared with the asset (`DSH9221`–`DSH9223`, warnings — the file is
  written either way). [`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md#pipelines-dsp) has
  the details.

## After the export — do this, in order

1. **Read the `// Warning: DSHnnnn: …` lines** under the `// Decompiled by DreamShader from …`
   header. Each names something the text cannot state the way the asset has it (warnings of
   `DSH9060`–`DSH9084`; for a pipeline `DSH9211`–`DSH9220`) — the notes of those ranges stay in the log. They
   are the migration work list; [`Docs/diagnostics/DSH9xxx.md`](../../Docs/diagnostics/DSH9xxx.md) says what
   each one means and what to do about it.
2. **Compile it as-is**, to establish that the export is at least buildable:
   ```bash
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Decompiled/Materials/…/M_Steel.dss -Force -CleanNew
   ```
   Without `-KeepAssetPath` this is safe by construction: a `.dss` builds its asset where the file
   is, so a file under `Decompiled/…` builds a *new* asset (`/Game/Decompiled/Materials/…`) and
   leaves the original alone.
3. **Clean it up** — [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md).
4. **Only then** let it take over the original: move the file to the place that names the original
   asset, or add `/// @name /Game/Materials/Metal/M_Steel` to the export (what `-KeepAssetPath`
   would have written), and delete nothing until the rebuilt asset has been looked at.

## Gotchas

- **The 2.0 output is proved, the 1.x output is not.** A 2.0 decompile is parsed again before it is
  written (`DSH9089`, a warning, if that fails — the file is written anyway, and it is a decompiler defect,
  not an asset problem), and the round-trip
  suite compiles decompiled text and compares IRs. The 1.x exporter stays "a migration starting
  point"; read
  [`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md#known-round-trip-gaps) before trusting
  a `-Format Legacy` export.
- **What 2.0 text cannot say is named, never dropped silently.** Known today: a default expression
  wired into a function input's Preview pin (`DSH9071`), additional defines on a Custom node
  (`DSH9067`), a node class outside the builtin catalog (`DSH9065`), a function asset that no longer
  exists (`DSH9064`), a node property that is an array, a map or a delegate (`DSH9068`).
- **An instance decompile writes overrides only.** A parameter the instance sets to the parent's own
  value is not an override and is left out; layer-only parameters and overrides the language cannot
  state are named (`DSH9101`, `DSH9104`, `DSH9105`).
- **A decompiled `.dsp` with HLSL passes takes registry slots when compiled.** Its pipeline is a new
  asset under `Decompiled/…`, and each `compute` pass or `fullscreen` pass with `Shader =` gets a slot
  of its own in the project's committed `.dreampass/` registry. Delete the file afterwards and run
  `dsc.ps1 pass-registry -Gc` to free them.
- **In Git Bash, a leading-slash asset path is mangled.** `/LGUI/Materials/X` becomes
  `C:/Program Files/Git/LGUI/Materials/X` and the asset "cannot be loaded". Run decompiles from
  PowerShell.
- The asset path is normalised: `\` → `/`, and a path starting with `/` that contains no `.` gets
  the short name appended, so `/Game/Path/Asset` is loaded as `/Game/Path/Asset.Asset`. Both forms
  work.
- **Layout export is a project setting** (*Export Decompiled Layout*, default on). With it off you
  lose node positions and the regenerated graph is auto-laid-out. Regions are emitted regardless.
- Comment boxes whose text begins with `DreamShader: ` are DreamShader's own markers: the 2.0 decompiler
  strips the prefix and keeps a box that encloses nodes as a `#pragma region`; the 1.x exporter drops them.
- `-Format Legacy` only: struct-, array-, map- and set-valued node properties are dropped silently,
  and material instances are rejected outright.

## Troubleshooting

| Message | Cause |
| :-- | :-- |
| `DreamShader could not load asset '…'.` | the object path is wrong, or the plugin/mount point is not loaded. Check the path in the Content Browser's *Copy Reference* |
| `DSH9085` | the format and the output file disagree: `-Format Dss` with `-Out x.dsm`, or a material instance with anything but a `.dsi` |
| `DSH9086` | the asset is not a material, function, layer, blend, material instance or pass pipeline |
| `DSH9210` | a pass pipeline with an `-Out` that does not end in `.dsp` |
| `DSH9221` / `DSH9222` / `DSH9223` | warnings, a pipeline only: the `.dsp` text did not parse again, did not bind into a pipeline, or reads back as a different pipeline (the first difference is named). The file is written either way. `DSH9221` and `DSH9223` are decompiler defects — report them with the asset; `DSH9222` can also mean the asset breaks a rule a `.dsp` is checked against, which a hand edit can do |
| `DSH9087` | `-SourceFile`: the source does not resolve to its products, or a product has not been built yet |
| `DSH9088` | the graph did not read into a valid module — a decompiler defect; report it with the asset |
| `DSH9089` | warning: the printed text does not parse back. The file is written; report it with the asset |
| `MaterialFunction '…' does not expose any outputs.` | `-Format Legacy`: the function declares no outputs; nothing to export |
| exports, but the graph is full of `UE.<Class>(…)` | expected for nodes the language has no sugar for. `-Readable` trades class-exactness for readability |

## See also

- [`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md) — both decompilers, and what each reproduces
- [`Docs/language-v2/instances.md`](../../Docs/language-v2/instances.md) — the `.dsi` an instance decompiles to
- [`Docs/language-v2/passes.md`](../../Docs/language-v2/passes.md) — the `.dsp` a pass pipeline decompiles to
- [`Docs/tools/migrate.md`](../../Docs/tools/migrate.md) — `dsc migrate`, for 1.x *sources* rather than assets
- [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md) — the required next step
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — the compile gate
