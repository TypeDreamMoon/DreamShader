---
name: dream-shader-optimize
description: Clean up a decompiled DreamShaderLang source — deduplicate repeated subexpressions, rename machine-generated variables, retarget the asset path, and restore state the decompiler drops — then recompile to prove the result still builds. Use when asked to optimize, clean up, tidy, refactor, or make readable a .dsm / .dsf produced by the DreamShader decompiler.
---

# dream-shader-optimize `<file>`

The decompiler is a **migration starting point, not a round-trip guarantee**. Its output compiles,
but it is machine-shaped: `Multiply_7`, duplicated subexpressions, an asset path that points at
`Decompiled/…`, and silently missing node state. This skill turns that into a source file a human
would have written — without changing what the material renders.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## The rule

**Behaviour must not change.** Deduplicating a repeated expression, renaming a variable and
collapsing an alias are safe. Reordering `lerp` arguments, folding `pow(x, 2)` into `x * x`, or
dropping a `saturate` are not — they are rewrites, and they belong in a separate, explicitly
requested change.

## Do this

**1 — Read the file and the header warnings.** Every `// Warning:` line under
`// Decompiled from …` names something the exporter could not reproduce. They are the work list.
Also read [`reference/dreamshaderlang.md`](../reference/dreamshaderlang.md).

**2 — Establish the baseline.** Compile it *before* touching it, so a later failure is yours:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Decompiled/Materials/X.dsm -Force -CleanNew
```

**3 — Apply the passes below**, in order.

**4 — Recompile.** Same command. Exit `0`, and the same `Generated …` asset path as the baseline.

**5 — Report** what changed, and — separately — anything you found that the decompiler *lost* and
you could not recover from the source alone. That list is for the human; it needs the original
asset open.

## The passes

### 4.1 Retarget the asset

The emitted name always points into `Decompiled/`, so recompiling creates a **second** asset and
leaves the original untouched:

```c
Shader(Name="Decompiled/Materials/Game/DreamShaderSkillProbe/M_SkillProbe")   // before
Shader(Name="DreamShaderSkillProbe/M_SkillProbe")                            // after — takes over /Game/…
```

Only do this once the source is trusted; it is the step that makes the source authoritative.
`Root="Plugin.LGUI"` targets a content plugin — see
[`Docs/generation/asset-paths.md`](../../Docs/generation/asset-paths.md).

### 4.2 Hoist duplicated subexpressions

The exporter shares *some* nodes into `DS_Shared_N` temporaries and re-emits others inline. Both
appear in the same file. From a real round trip:

```c
// before — the same TextureCoordinate node emitted twice, and pow(saturate(…), 3.0) twice
float2 MaterialExpressionTextureCoordinate_0 = UE.Expression(Class="TextureCoordinate", OutputType="float2");
float2 DS_Shared_0 = MaterialExpressionTextureCoordinate_0;
…
float Multiply_4 = ((UE.Expression(Class="TextureCoordinate", OutputType="float2")).g * LineCount);
float Multiply_2 = (pow(saturate(Subtract_2), 3.0) * 1.6);
float DS_Shared_2 = pow(saturate(Subtract_2), 3.0);
```

```c
// after
float2 uv   = UE.TexCoord(Index=0);
float  glow = pow(saturate(edgeFalloff), 3.0);
```

### 4.3 Replace `UE.Expression` with its curated wrapper

`UE.Expression` is the generic fallback. Where a real builtin exists, use it — it is checked,
readable, and does not depend on a class-name string:

| Fallback the decompiler emitted | Write instead |
| :-- | :-- |
| `UE.Expression(Class="TextureCoordinate", OutputType="float2")` | `UE.TexCoord(Index=0)` |
| `UE.Expression(Class="VertexColor", OutputType="float4")` | `UE.VertexColor()` |
| `UE.Expression(Class="ScreenPosition", …)` | `UE.ScreenPosition()` |

Check [`Docs/builtins/ue.md`](../../Docs/builtins/ue.md) before assuming a wrapper exists — keep
`UE.Expression` when it does not. Mind the arguments: `UE.Expression(Class="TextureCoordinate")`
with no `CoordinateIndex` means index 0, so `UE.TexCoord(Index=0)` is the equivalent, not
`UE.TexCoord()` with some other default.

### 4.4 Rename machine identifiers

`Multiply_7`, `Subtract_2`, `Add_1`, `DS_Shared_3`, `MaterialExpressionTextureCoordinate_0` are
node class names with a counter. Rename to what the value *is*: `scanline`, `bandPhase`,
`edgeFalloff`, `panelTint`.

> **A renamed `Graph` variable must be renamed in `Layout` too.** `Node(Var="Multiply_7", …)`
> refers to the variable by name; leaving the old spelling behind orphans that position. Rename
> both, or drop the entry.

### 4.5 Collapse pointless aliases

```c
float4 DS_Shared_3      = Tint;              // then only ever used as DS_Shared_3.a
float3 DS_EmissiveColor_0 = Multiply_3;
EmissiveColor           = DS_EmissiveColor_0;
```
becomes
```c
EmissiveColor = panelColour;
Opacity       = mask * Tint.a;
```

### 4.6 Restore what the exporter never emitted

None of this is recoverable from the decompiled text — it needs the original asset. Check each,
and list what you could not confirm.

| Lost | Where it belongs |
| :-- | :-- |
| `Backend` | `Settings` — the exporter never writes it, so the file rebuilds on the default backend. Add `Backend = "Graph";` if the graph must stay editable |
| `UMaterial` properties outside the blessed set — `OpacityMaskClipValue`, `NumCustomizedUVs`, translucency lighting mode, displacement, Nanite override | `Settings`; they resolve by reflection. [`Docs/settings/material.md`](../../Docs/settings/material.md) |
| Material-function settings — `Description`, `ExposeToLibrary`, `LibraryCategories`, `UserExposedCaption` | a `Settings` block in the `.dsf`. [`Docs/settings/function.md`](../../Docs/settings/function.md) |
| **Struct-, array-, map- and set-valued node properties** | dropped **silently** from a fallback `UE.Expression`, with no per-property warning. Re-add the argument, or set it on the asset after generation |
| Node comment text (`Desc`) and pin `SortPriority` | re-apply by hand |

### 4.7 Check the numbers

Emitted literals are rounded. A real round trip turned `6.2831853` into `6.283185`. Where a
constant is recognisable — τ, π, a colour, a tiling count — restore the exact value.

### 4.8 Keep `Layout`

Do not delete it to "tidy up". Without a `Layout` block a large regenerated graph skips automatic
layout and comes back visually unordered. Comment boxes prefixed `DreamShader: ` are generated
markers and are correctly absent.

## What the passes are worth

Measured on a real round trip — a 45-line hand-written `.dsm`, generated, decompiled, then
optimized back:

| | Decompiled | After the passes |
| :-- | --: | --: |
| `Graph` statements | 21 | 8 |
| distinct variables | 19, all machine-named | 6, all semantic |
| `UE.Expression` fallbacks | 2 (the same node, twice) | 0 — one `UE.TexCoord(Index=0)` |
| duplicated subexpressions | 2 | 0 |
| asset it rebuilds | a second copy under `/Game/Decompiled/…` | the original `/Game/DreamShaderSkillProbe/M_SkillProbe` |

Both versions compile — `dsc: OK (exit 0)` — and generate the same asset path once retargeted. That
equivalence is the acceptance test: **if the optimized file generates a different asset path, or
fails to compile, the pass was wrong.**

## Known round-trip gaps you cannot fix in the source

Read these before promising a clean migration — full table in
[`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md#known-round-trip-gaps).

- A `MaterialFunctionCall` **with no assigned function becomes `0.0`** — the branch is silently
  constant-folded. Re-assign it in the original asset and re-export.
- A `MaterialFunctionCall` on a **layer or layer blend** falls back to `UE.Expression`; export the
  layer separately and call it.
- **Graph cycles emit a default literal.** The cyclic branch became a constant. Break the cycle in
  the original.
- An **append wider than four components** was masked down — components were dropped. Check the
  emitted swizzle.
- **Material instances are rejected outright.** Export the parent `UMaterial`, then re-create the
  instance.
- `GatherMode` round-trips only on UE 5.6+; `bHasPixelAnimation`, `Base.FrontMaterial` and the
  `Substrate` shading-model spelling only on UE 5.4+.

## Gotchas

- **The exporter's `Settings` block always starts with `Domain`, `ShadingModel` and `BlendMode`,
  emitted unconditionally** — their presence is not evidence they were non-default.
- **A decompiled Substrate material has `ShadingModel = "Substrate"` forced**, because a Substrate
  material's own shading-model enum does not describe its surface. Do not "correct" it.
- `Properties` come out in the exporter's order, not the authored order. Re-sorting them is safe;
  changing `SortPriority` is not — that is the material instance's UI order.
- A `ParameterName="…"` metadata entry means the DreamShaderLang identifier had to differ from the
  asset's real parameter name. **Renaming the identifier is safe; deleting `ParameterName` is not**
  — it would rebind the parameter and break every existing material instance.
- Re-running the decompiler overwrites your cleaned file. Move it out of `Decompiled/` once you
  own it.

## See also

- [`dream-shader-decompile`](../dream-shader-decompile/SKILL.md) — producing the input
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — the compile gate
- [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) — resolving a message
