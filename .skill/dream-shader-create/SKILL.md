---
name: dream-shader-create
description: Write a new DreamShader source from a plain-language description — a material, material function, layer, material instance or Custom Pass pipeline — then check and compile it headlessly to prove it builds. Use when asked to create, author, add or write a DreamShader material, shader, material function, material instance, .dss / .dsi / .dsp file (or a .dsm in a 1.x project), or a render pass for an Unreal project.
---

# dream-shader-create `<description>`

Turn a description — "a UI panel with scrolling scanlines and a glow band" — into a source file that
**provably builds**. The proof is not optional: the compiler checks names, types, widths and engine pins, and
the only way to know a file passes is to run it.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Do this

**1 — Pick the kind, then read its reference.** Do not write DreamShaderLang from memory.

| The user wants | Write | Read first |
| :-- | :-- | :-- |
| a material, material function, layer or layer blend | a `.dss` | [`reference/dss.md`](../reference/dss.md) |
| a variant of an existing material — other colours, other textures | a `.dsi` (material instance) | [`reference/dss.md` § 7](../reference/dss.md#7-instances--dsi) |
| a render pass — an outline, a blur, a screen effect, a compute shader writing a texture | a `.dsp` and the `.dss` materials its passes use. **UE 5.8+ only** | [`reference/dsp.md`](../reference/dsp.md) |
| a change in a project whose sources are `.dsm` / `.dsf` | match the project: a `.dsm` / `.dsf` — or offer [`dream-shader-migrate`](../dream-shader-migrate/SKILL.md) first | [`reference/legacy.md`](../reference/legacy.md) |

New work is `.dss`. Look at what the project already has (`DShader/**`) before choosing: mirror its folders,
naming (`M_`, `MF_`, `MI_`, `CP_`) and the doc-tag style of its parameters.

**2 — Decide the details, then write the file** under the project's `DShader/` tree. A `.dss` builds its asset
where the file is: `DShader/UI/M_Panel.dss` → `/Game/UI/M_Panel`, named after its `export` function.

| Question | Answer it with |
| :-- | :-- |
| Which surface? | `#pragma material(Domain = …, ShadingModel = …, BlendMode = …)` — [`Docs/settings/material-enums.md`](../../Docs/settings/material-enums.md) |
| Must an artist open the graph? | `#pragma material(Backend = Graph)` for a plain `UMaterial`; leave it out for the project default (ThinCustom) |
| Dense math, a matrix, a real loop? | a `/// @custom` function — its body is verbatim HLSL |
| Reuse an existing material function? | `/// @asset <path>` + an `extern` prototype |
| Something a material cannot do — draw objects again, read another pass's output, run compute? | a `.dsp` |

Give every parameter `@group`, `@sort` and `@desc` — that is what makes the generated material instance usable
without reading the source — and `@slider min max` on scalars with a sensible range.

**3 — Check it.** Fast, writes nothing, reports every diagnostic of the file at once:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/UI/M_ScanlinePanel.dss
```

Fix everything it reports, then check again. An unfamiliar code →
[`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md); every code has a page under
[`Docs/diagnostics/`](../../Docs/diagnostics/README.md).

**4 — Build it.** `check` stops before the shader compiler; `compile` builds the asset:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_ScanlinePanel.dss -Force -CleanNew
```

Exit `0` and a `Generated …` line mean it built. `-CleanNew` deletes the `.uasset` the run created — keep it on:
the editor generates the same material in memory, and a leftover file shadows that. For a file with a
`/// @custom` body, or a `.dsp` with HLSL passes, run `check -Shaders` as well — it is the only thing that
compiles that HLSL ([`dream-shader-verify`](../dream-shader-verify/SKILL.md)).

Batch: every run boots the editor (tens of seconds), and a run over many files costs about the same as one.
Write all the files first, then check them with `check -All`.

**5 — Report** the source path, the asset path, every parameter with its default and range, and anything the
user should look at in the editor (a material's look cannot be checked headlessly).

## Worked example

*"A UI scanline panel with a scrolling glow band and an adjustable tint"*:

```hlsl
// DShader/UI/M_ScanlinePanel.dss -- scanlines and a glow band that scrolls down the panel
#pragma material(Domain = UI, BlendMode = Translucent, ShadingModel = Unlit)

/// @group Scanline|Style   @sort 10   @desc Base panel colour; alpha scales the opacity
uniform float4 Tint = float4(0.25, 0.85, 1.0, 1.0);

/// @group Scanline|Style   @sort 20   @slider 8 128   @desc Scanlines across the panel height
uniform float LineCount = 48.0;

/// @group Scanline|Motion   @sort 30   @slider 0 2   @desc Glow band travel speed, panels per second
uniform float ScrollSpeed = 0.35;

float GlowBand(float V, float Phase)
{
    float Band = frac(V - Phase);
    return pow(saturate(1.0 - abs(Band - 0.5) * 4.0), 3.0);
}

export void M_ScanlinePanel(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float Scan = 0.5 + 0.5 * sin(UV.y * LineCount * 6.2831853);
    float Glow = GlowBand(UV.y, UE.Time() * ScrollSpeed);
    m.EmissiveColor = Tint.rgb * (0.4 + Glow * 1.6);
    m.Opacity = saturate(Scan * 0.6 + Glow) * Tint.a;
}
```

```text
LogDreamShader: Display: Generated Material /Game/UI/M_ScanlinePanel.M_ScanlinePanel from …/DShader/UI/M_ScanlinePanel.dss.

Assets written to disk by this run:
  Content/UI/M_ScanlinePanel.uasset  [NEW]
    deleted (-CleanNew)

dsc: OK (exit 0)
```

A red variant is a `.dsi` beside it, overriding only what differs:

```hlsl
// DShader/UI/MI_ScanlinePanel_Red.dsi
#pragma instance(Parent = "M_ScanlinePanel")

uniform float4 Tint = float4(1.0, 0.25, 0.2, 1.0);
```

## Gotchas

- **Names are case-sensitive** (`DSH4200` / `DSH4208`, with a "did you mean"). HLSL spellings only — `mix`,
  `fract`, `mod`, `vec3` are `DSH4250`.
- **A vector never narrows silently**: assigning a `float4` to a `float3` is `DSH4226` — write `.rgb`.
- **No matrices, unbounded loops, `discard` in a branch or `switch` in graph code** (`DSH4361`, `DSH4360`,
  `DSH4362`, `DSH2160`). Those belong in a `/// @custom` body.
- **`UE.*` calls take named arguments** (`UE.LinearInterpolate(A = 0.0, B = Gain, Alpha = t)`); positional is
  `DSH5220` except for a few (`UE.TexCoord`, `UE.Time`, …). Look the node up in [`Docs/builtins/ue.md`](../../Docs/builtins/ue.md) rather than guessing pin names.
- **A `/// @custom` body is not checked by `check`** — only `check -Shaders` compiles it.
- **One `export void … (inout material m)` per file**, and a material's functions in another file (`DSH6201`).
- **A texture uniform has no `= …`**: `/// @default /Game/T_X` above `uniform Texture2D T;` (`DSH7213`).
- **`@slider` is for scalars** — on a vector it is ignored with `DSH7233`.
- **A `.dsp` is UE 5.8+** and needs its pass materials written as `.dss` too; its traps are in
  [`reference/dsp.md` § 6](../reference/dsp.md#6-traps).

## See also

- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — `check`, `compile`, `check -Shaders`, `-All`
- [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) — resolving a code
- [`Docs/language-v2/index.md`](../../Docs/language-v2/index.md) — the 2.0 language in full
- [`Docs/examples/custom-pass.md`](../../Docs/examples/custom-pass.md) — complete pipelines with their materials
