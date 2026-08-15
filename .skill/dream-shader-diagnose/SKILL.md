---
name: dream-shader-diagnose
description: Resolve a DreamShader compile error or warning — look the message up by pipeline stage, explain the cause, and fix the source. Use when a .dsm / .dsf fails to build, when a LogDreamShader error needs explaining, or when a DreamShader material silently comes out wrong.
---

# dream-shader-diagnose `<message>`

Turn a `LogDreamShader` message into a fix. Every message the parser, generator, commandlet and
VirtualFunction sync can emit is catalogued in
[`Docs/diagnostics/index.md`](../../Docs/diagnostics/index.md) — 1000 lines, grouped by the stage
that produced it. This skill is the routing table into it.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Do this

**1 — Get the exact message.** Positioned diagnostics are MSVC-style, `file(line,col): message`:

```text
I:/Project/DShader/Materials/M_Sample.dsm(37,9): Unknown Graph identifier 'Tin'.
```

If you only have "it doesn't work", reproduce it:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Sample.dsm -Force -CleanNew
```

**2 — Route by stage.** Jump to the section of
[`Docs/diagnostics/index.md`](../../Docs/diagnostics/index.md) that owns it:

| Message shape | Section | Deep reference |
| :-- | :-- | :-- |
| tokens, block structure, unterminated anything | **Parse** | [`Docs/language/lexical.md`](../../Docs/language/lexical.md) |
| a `Properties` / `Inputs` / `Outputs` / `Settings` entry | **Sections and declarations** | [`Docs/language/index.md`](../../Docs/language/index.md) |
| `Failed to evaluate Graph assignment for 'x'`, `Unknown Graph identifier`, type mismatches | **Graph statements and expressions** | [`Docs/graph/index.md`](../../Docs/graph/index.md) |
| `Math function '…' expects…`, `Unknown Graph function`, `UE.*`, `Substrate.*` | **Builtins** | [`Docs/builtins/math.md`](../../Docs/builtins/math.md), [`ue.md`](../../Docs/builtins/ue.md) |
| `DreamShader Function '…' input '…' uses unsupported type`, generated `.ush` failures | **Functions and HLSL codegen** | [`Docs/language/function.md`](../../Docs/language/function.md) |
| `Unsupported property type`, parameter nodes, sampler types | **Properties and parameters** | [`Docs/parameters/index.md`](../../Docs/parameters/index.md) |
| a `Settings` key or enum string | **Settings** | [`Docs/settings/material-enums.md`](../../Docs/settings/material-enums.md) |
| `Generated …`, `Skipped …`, save/package failures | **Asset generation and saving** | [`Docs/generation/index.md`](../../Docs/generation/index.md) |
| the usage banner, `Unknown DreamShader command` | **Commandlet** | [`Docs/tools/commandlet.md`](../../Docs/tools/commandlet.md) |
| `VirtualFunction` drift against a real asset | **VirtualFunction sync** | [`Docs/language/virtual-function.md`](../../Docs/language/virtual-function.md) |

**3 — Fix the source, recompile, confirm exit `0`.** Then check the message is gone rather than
replaced: a compile stops at the **first** failing `Graph` statement, so fixing one error routinely
reveals the next.

## When there is no message

The hardest DreamShader failures are the silent ones. Read
[`Docs/diagnostics/index.md` § Silent behaviour](../../Docs/diagnostics/index.md) first — that
section exists precisely for this. The recurring causes:

| Symptom | Cause |
| :-- | :-- |
| a `Function` / property is never called, and nothing is reported | its name collides with one of the **29 reserved math builtins** (`lerp` `dot` `pow` `min` `max` `clamp` `abs` `saturate` `sin` `cos` `floor` `ceil` `frac` `fract` `sqrt` `normalize` `fmod` `mod` `mix` `step` `smoothstep` `length` `cross` `asin` `acos` `atan` `atan2` `reflect` `refract`) or a constructor name. The builtin wins at the call site, silently |
| a helper inside a `Function` body behaves as a different function | the body identifier rewrite renamed it — `Mix`→`lerp`, `Mod`→`fmod`, `Fract`→`frac`, `Vec3`→`float3`, `Mat4`→`float4x4`, whole-identifier and case-insensitive |
| a shader compile error from Unreal that names no DreamShader line | math builtins do **not** check component counts. `dot(float3Value, floatValue)` passes DreamShader and fails inside Unreal's translator |
| a `UE.Expression` node comes back at its class default | struct-, array-, map- and set-valued properties are dropped with no per-property warning |
| a commandlet flag did the opposite of what you meant | an unrecognised boolean value evaluates to **on**. `-Force=disable` enables it |
| `compile -All` was green but built nothing | an empty source list is a Warning, and still exits `0` |
| the editor shows a stale material | a previous commandlet run left a real `.uasset` on disk that shadows in-memory generation. Delete it, or use *Tools ▸ DreamShader ▸ Clean Persisted Generated Assets* |

## Where diagnostics live

| Surface | Contents |
| :-- | :-- |
| Output Log / commandlet stdout | the raw message; the **only** surface that shows success too |
| Material Content Browser ▸ Dream Shader Gen | the per-file list, read from `diagnostics.json` |
| `Saved/DreamShader/Bridge/diagnostics.json` + `diagnostics/` shards + `bridge.db` | what the VSCode and Rider extensions render as squiggles |

> **None of the bridge artifacts are written by a commandlet.** `-run=DreamShader` produces log
> messages and nothing else, so a headless run cannot be diagnosed from `diagnostics.json`.

Every stored diagnostic has severity `error` — the store has no warning level. Parse *warnings*
(deprecated spellings, the missing-`Outputs` warning) never enter the store at all; they are
appended to the compile result message and appear only in the log. An extension that colours by
severity paints every DreamShader entry red.

## Gotchas

- Diagnostics are owned by the file that produced them. Recompiling `A.dsm` clears exactly what
  `A.dsm` produced — including records attributed to an imported `.dsh` — without disturbing another
  material's diagnostics for the same header.
- A message that names a `.dsh` line came from whichever `.dsm`/`.dsf` imported it; the header is
  never an entry point.
- `Skipped … source hash is unchanged.` is not a success message about your edit — it means nothing
  was compiled. Add `-Force`.

## See also

- [`Docs/diagnostics/index.md`](../../Docs/diagnostics/index.md) — the full catalogue
- [`Docs/tools/bridge.md`](../../Docs/tools/bridge.md) — how the extensions receive diagnostics
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — reproducing the failure headlessly
