---
name: dream-shader-create
description: Write a new DreamShaderLang material or material function from a plain-language description, then compile it headlessly to prove it builds. Use when asked to create, author, add, or write a .dsm / .dsf / DreamShader material, shader, or material function for an Unreal project.
---

# dream-shader-create `<description>`

Turn a description — "a UI panel with scrolling scanlines and a glow band" — into a `.dsm` that
**provably compiles**. The verification step is not optional: DreamShaderLang has several traps
(reserved builtin names, no matrix types, silent identifier rewrites) that only surface at compile.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Do this

**1 — Read the grammar.** [`reference/dreamshaderlang.md`](../reference/dreamshaderlang.md).
Do not write DreamShaderLang from memory; the builtin set is small and closed, and inventing a
function name fails at step 3.

**2 — Decide, then write the file.**

| Question | Answer it with |
| :-- | :-- |
| Material, or reusable function? | `.dsm` holds one `Shader`; `.dsf` holds `ShaderFunction`/`ShaderLayer`/`ShaderLayerBlend`; `.dsh` holds helpers only |
| Where does the asset land? | `Shader(Name="UI/M_Panel")` → `/Game/UI/M_Panel`. `Root="Plugin.X"` for a content plugin |
| Node graph, or one HLSL node? | `Settings { Backend = "Graph"; }` for a graph an artist can open; omit for the default thin-custom material |
| Which surface? | `Domain` `ShadingModel` `BlendMode` — see [`Docs/settings/material-enums.md`](../../Docs/settings/material-enums.md) |

Write it under the project's `DShader/` tree — `DShader/Materials/` for `.dsm`,
`DShader/Functions/` for `.dsf`, `DShader/Shared/` for `.dsh`.

Every parameter gets `Group`, `SortPriority` and `Description` metadata. That is the house style in
`DShader/Materials/M_TZM_CardFoil_Holographic.dsm`, and it is what makes the generated material
instance usable without reading the source.

**3 — Compile it.** This is the whole point of the skill:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Panel.dsm -Force -CleanNew
```

Run it from anywhere inside the project — the driver walks up for the `.uproject` and resolves the
engine from its `EngineAssociation`. Exit `0` means the material generated; exit `1` means it did
not, and the `LogDreamShader: Error:` line carries `file(line,col): message`.

`-CleanNew` deletes the `.uasset` the commandlet wrote, provided git reports it untracked. Keep it
on while iterating: the interactive editor generates the same material **in memory**, and a
leftover file on disk shadows it. See [`dream-shader-verify`](../dream-shader-verify/SKILL.md) for
the details.

**4 — Fix one error, recompile.** The compiler stops at the **first** failing `Graph` statement, so
a file with three mistakes reports one. Iterate. Unfamiliar message →
[`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md).

**5 — Report** the source path, the generated asset path, and every parameter with its default.

## Worked example

A description of *"a UI scanline panel with a scrolling glow band and adjustable tint"* became this,
which compiled on the first run:

```c
Shader(Name="DreamShaderSkillProbe/M_SkillProbe")
{
    Properties = {
        VectorParameter Tint = float4(0.25, 0.85, 1.0, 1.0) [
            Group="Scanline | Style"; SortPriority=10; Description="Base panel colour";
        ];
        ScalarParameter LineCount = 48.0 [
            Group="Scanline | Style"; SortPriority=20; Description="Scanlines across the panel height";
        ];
        ScalarParameter ScrollSpeed = 0.35 [
            Group="Scanline | Motion"; SortPriority=30; Description="Glow band travel speed";
        ];
    }

    Settings = {
        Backend = "Graph";
        Domain = "UI";
        ShadingModel = "Unlit";
        BlendMode = "Translucent";
    }

    Outputs = {
        float3 EmissiveColor;
        float Opacity;

        Base.EmissiveColor = EmissiveColor;
        Base.Opacity = Opacity;
    }

    Graph = {
        float2 uv = UE.TexCoord(Index=0);
        float t = UE.Time();

        float scanline = 0.5 + 0.5 * sin(uv.y * LineCount * 6.2831853);
        float band = frac(uv.y - t * ScrollSpeed);
        float glow = pow(saturate(1.0 - abs(band - 0.5) * 4.0), 3.0);

        float mask = saturate(scanline * 0.6 + glow);

        EmissiveColor = Tint.rgb * (0.4 + glow * 1.6);
        Opacity = mask * Tint.a;
    }
}
```

```text
LogDreamShader: Display: Generated /Game/DreamShaderSkillProbe/M_SkillProbe.M_SkillProbe from …/M_SkillProbe.dsm.
dsc: OK (exit 0)
```

## Gotchas

- **The 29 math builtins are reserved and shadow user code silently.** A `Function`, property or
  `ShaderFunction` named `lerp`, `dot`, `pow`, `min`, `max`, `clamp`, `abs`, `step`, `length`,
  `cross`… is unreachable from a `Graph` block and there is **no diagnostic**. The declaration still
  compiles and still generates its asset; only the call site is redirected to the builtin.
- **Inside a `Function` / `GraphFunction` body, `mix` `mod` `fract` `vec3` `mat4` and friends are
  rewritten as whole identifiers, case-insensitively.** A local named `Mix` silently becomes `lerp`.
  `Graph` blocks are not rewritten.
- **No matrix types.** `mat3` parses in a `Function` signature — the normalizer rewrites it to
  `float3x3` before validation — and then fails at the first call site. A `Graph` block has no matrix
  value at all, because the Unreal material graph has none; matrix math belongs in a `Function` HLSL
  body.
- **`reflect` and `refract` are builtins but not nodes.** They expand to a 4-node and a 14-node
  subgraph. Correct, but if the surrounding code is already HLSL, write them in a `Function` body
  and let the intrinsic do it in one node.
- **Component counts are not checked for math builtins.** `dot(float3Value, floatValue)` passes
  DreamShader and fails later inside Unreal's shader compiler, with a message that does not name
  your source line.
- **A `.dsh` is rejected for containing the *text* `Shader(`** — in a comment, in a string, anywhere.
  The kind check is a substring scan that runs before parsing. `Shader (` with a space passes.
- **Only one `Shader` block per import closure**, not per file. Imports are inlined before parsing.
- `Path(Game, "…")`, not `"/Game/…"`, for asset references in `Properties`.

## Troubleshooting

| Message | Cause and fix |
| :-- | :-- |
| `Unknown Graph function 'saturte'.` | misspelled builtin, or a call to something the `Graph` dispatcher cannot see. Check the 19-name list |
| `Math function 'lerp' expects exactly 3 argument(s).` | wrong arity — **or a named argument**. Both report as arity. Builtin arguments are positional only |
| `Graph variable type 'Texture2D' requires an explicit initializer.` | texture / `SamplerState` / `Substrate` declarations in `Graph` need `= something` |
| `Unsupported property type 'Scalar'.` | `Scalar`, `Color` and `Vector` were removed. Use `float`, `float4`, `float2..4` |
| `Only one top-level Shader block is currently supported.` | a second `Shader` somewhere in the import closure |
| `DreamShader header '…' may only declare …` | a `.dsh` whose text contains one of the six forbidden substrings |
| `dsc: FAILED (exit 1)` with no `LogDreamShader` line | re-run the same command with `-Raw` to see the engine log |

## See also

- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — the compile gate, and `-All`
- [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md) — cleaning up decompiler output
- [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) — resolving a message
- [`Docs/examples/index.md`](../../Docs/examples/index.md) — complete sources to copy
