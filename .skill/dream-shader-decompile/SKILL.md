---
name: dream-shader-decompile
description: Export an existing Unreal UMaterial or UMaterialFunction back into DreamShaderLang source headlessly, so a hand-built material graph can be migrated to a text source file. Use when asked to decompile, export, reverse, convert, or migrate a material / material function / material layer asset to .dsm or .dsf.
---

# dream-shader-decompile `<asset>`

Walk an existing `UMaterial` / `UMaterialFunction` node graph and write the equivalent
DreamShaderLang source. This is the front half of a migration; the back half is
[`dream-shader-optimize`](../dream-shader-optimize/SKILL.md).

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Run it

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 decompile /Game/Materials/M_Steel
```

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 decompile /LGUI/Materials/LexUI_RectBlock -Out I:/Work/LexUI_RectBlock.dsm
```

Exit `0` writes the file and logs where:

```text
LogDreamShader: Display: DreamShader decompiled '/Game/DreamShaderSkillProbe/M_SkillProbe.M_SkillProbe' to '…/M_SkillProbe.roundtrip.dsm'.
dsc: OK (exit 0)
```

Without `-Out` the destination is computed from the asset's package path:

| Asset class | Lands in | Extension |
| :-- | :-- | :-- |
| `UMaterial` | `DShader/Decompiled/Materials/<package path>` | `.dsm` |
| `UMaterialFunction` | `DShader/Decompiled/Functions/<package path>` | `.dsf` |
| `UMaterialFunctionMaterialLayer` | `DShader/Decompiled/Layers/<package path>` | `.dsf` |
| `UMaterialFunctionMaterialLayerBlend` | `DShader/Decompiled/LayerBlends/<package path>` | `.dsf` |

So `/Game/Materials/Metal/M_Steel` → `DShader/Decompiled/Materials/Game/Materials/Metal/M_Steel.dsm`.

## After the export — do this, in order

1. **Read the `// Warning:` lines** under the `// Decompiled from …` header. Each names something
   the exporter could not reproduce. They are the migration work list.
2. **Compile it as-is**, to establish that the export is at least buildable:
   ```bash
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Decompiled/Materials/…/M_Steel.dsm -Force -CleanNew
   ```
   This is safe by construction: the emitted `Name=` points into `Decompiled/…`, so it builds a
   *new* asset and leaves the original alone.
3. **Clean it up** — [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md).
4. **Only then** retarget `Name=` / `Root=` at the original path, and delete the original asset.

## Gotchas

- **This is a migration starting point, not a round-trip guarantee.** The exporter reproduces the
  graph's structure and the node state it can express, and leaves a `// Warning:` comment for the
  rest. Read
  [`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md#known-round-trip-gaps) before deleting
  any original.
- **Struct-, array-, map- and set-valued node properties are dropped silently** — no warning names
  them. A fallback `UE.Expression` node comes back with that state at its class default.
- **Material instances are rejected outright.** `UMaterialInstanceConstant` is not supported —
  export the parent `UMaterial` and re-create the instance. For a thin-custom DreamShader material,
  the graph lives on the hidden `MB_DreamThinBase_*` base, so pass the instance's parent.
- **In Git Bash, a leading-slash asset path is mangled.** `/LGUI/Materials/X` becomes
  `C:/Program Files/Git/LGUI/Materials/X` and the asset "cannot be loaded". Run decompiles from
  PowerShell.
- The asset path is normalised: `\` → `/`, and a path starting with `/` that contains no `.` gets
  the short name appended, so `/Game/Path/Asset` is loaded as `/Game/Path/Asset.Asset`. Both forms
  work.
- **`Layout` export is a project setting** (*Export Decompiled Layout*, default on). With it off you
  lose node positions and the regenerated graph is auto-laid-out. `#Region` directives are emitted
  regardless.
- Comment boxes whose text begins with `DreamShader: ` are generated markers and are deliberately
  not re-emitted.

## Troubleshooting

| Message | Cause |
| :-- | :-- |
| `DreamShader could not load asset '…'.` | the object path is wrong, or the plugin/mount point is not loaded. Check the path in the Content Browser's *Copy Reference* |
| `DreamShader decompile supports Material and MaterialFunction assets only: …` | wrong asset class — most often a material **instance** |
| `MaterialFunction '…' does not expose any outputs.` | the function declares no outputs; nothing to export |
| `DreamShader failed to create output directory '…'.` | bad `-Out` path |
| exports, but the graph is full of `UE.Expression` | expected for nodes with no curated case. Pass 2 of the argument builder still exports reflected literal properties that differ from the class default |

## See also

- [`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md) — what is exported faithfully, and what is not
- [`Docs/builtins/ue-expression.md`](../../Docs/builtins/ue-expression.md) — the generic fallback call
- [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md) — the required next step
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — the compile gate
