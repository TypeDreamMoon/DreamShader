---
name: dream-shader-diagnose
description: Resolve a DreamShader compile error or warning — look its DSH code up, explain the cause, and fix the source — or find out why a DreamShader material silently comes out wrong. Use when a .dss / .dsi / .dsp / .dsm / .dsf fails to build, when a LogDreamShader message or a DSHnnnn code needs explaining, or when a material or pass looks wrong with no message at all.
---

# dream-shader-diagnose `<message>`

Turn a DreamShader message into a fix. Every message the compiler raises carries a stable code, `DSHnnnn`, and
every code has a page with its cause and fix. This skill is the route to that page, and what to do when there is
no message.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Do this

**1 — Get the exact message.** A diagnostic is `file(line,col): DSHnnnn: message`:

```text
C:/Project/DShader/UI/M_Panel.dss(7,16): DSH4200: 'Tin' is not declared in this scope.
```

If all you have is "it doesn't work", reproduce it — `check` writes nothing and reports every diagnostic of the
file at once:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/UI/M_Panel.dss
```

**2 — Open the code's page.** The leading digit is the stage; the page is
`Docs/diagnostics/DSH<digit>xxx.md`, anchored at the code, and
[`Docs/diagnostics/README.md`](../../Docs/diagnostics/README.md) lists every code with its message.

| Code | Stage | Page |
| :-- | :-- | :-- |
| `DSH1xxx` | the preprocessor (`#if`), source files, asset references | [`DSH1xxx.md`](../../Docs/diagnostics/DSH1xxx.md) |
| `DSH2xxx` | lexer and syntax; 1.x `Graph` statements and blocks (`22xx`); `.dsp` syntax (`23xx`) | [`DSH2xxx.md`](../../Docs/diagnostics/DSH2xxx.md) |
| `DSH3xxx` | declarations, `#` directives, `///` blocks; 1.x sections (`325x`+); `.dsp` declarations (`33xx`) | [`DSH3xxx.md`](../../Docs/diagnostics/DSH3xxx.md) |
| `DSH4xxx` | the binder — names, types, calls (`42xx`); the IR validator (`43xx`); what the graph cannot express — matrices, loops (`435x`); `.dsp` names (`44xx`) | [`DSH4xxx.md`](../../Docs/diagnostics/DSH4xxx.md) |
| `DSH5xxx` | `UE.*` nodes, math, Substrate; 1.x call spellings and the numbered legacy rules (`5250`–`5292`); Custom Pass material nodes (`53xx`) | [`DSH5xxx.md`](../../Docs/diagnostics/DSH5xxx.md) |
| `DSH6xxx` | functions: entries, `export` / `extern`, helper inlining, `/// @custom` HLSL, 1.x `Function` blocks | [`DSH6xxx.md`](../../Docs/diagnostics/DSH6xxx.md) |
| `DSH7xxx` | uniforms, `///` tags, `#pragma material`; `.dsi` overrides (`725x`); `.dsp` checks — kinds, injection points, buffers in frame order (`73xx`) | [`DSH7xxx.md`](../../Docs/diagnostics/DSH7xxx.md) |
| `DSH8xxx` | the emitter and asset creation; material instances (`824x`–`826x`); Custom Pass emission, HLSL slots and the pre-check (`83xx`) | [`DSH8xxx.md`](../../Docs/diagnostics/DSH8xxx.md) |
| `DSH9xxx` | the tools: `check`, the decompiler (`906x`–`908x`), `migrate` (`909x`), `.dsi` read-back (`910x`), `pass-registry` (`920x`), pipeline decompile (`921x`–`922x`) | [`DSH9xxx.md`](../../Docs/diagnostics/DSH9xxx.md) |

The finer ranges are tabled in [`Docs/language-v2/index.md` § Diagnostics](../../Docs/language-v2/index.md#diagnostics).
For the language rule behind a code: [`reference/dss.md`](../reference/dss.md), [`reference/dsp.md`](../reference/dsp.md)
or [`reference/legacy.md`](../reference/legacy.md).

A message **without** a code comes from outside the compiler — the commandlet's own arguments, the engine's
material compile, the VirtualFunction sync — and [`Docs/diagnostics/index.md`](../../Docs/diagnostics/index.md)
catalogues those. Most of that page is the 1.x generator's catalogue, kept for reference; a message you find only
there is from an old version.

**3 — Fix the source, check again, confirm exit `0`.** A stage reports all of its errors at once, but a syntax
error stops the file before the binder runs — so fixing the syntax can bring out binder errors that were there all
along.

## When there is no message

| Symptom | Cause |
| :-- | :-- |
| a 1.x `Function` body behaves as if a name meant something else | inside a 1.x `Function` / `GraphFunction` body, `mix` `fract` `mod` `vec2..4` `mat2..4` (and `ivec`/`uvec`/`bvec`) are rewritten as whole identifiers, case-insensitively — a local named `Mix` becomes `lerp`. Silently. A `.dss` has no such rewrite |
| a `/// @custom` body or a `.dsp`'s HLSL is wrong, and `check` passed | `check` does not compile HLSL. `check -Shaders` does — and even it skips code no entry reaches: an uncalled helper is never compiled |
| one HLSL error is reported twice, in two wordings | the pre-check compiles for each shader format the project targets (SM5 with FXC, SM6 with DXC); one mistake |
| a material reads a Custom Pass buffer one frame late | the buffer's last writer runs after the base pass — `DSH7354` (info) says so; see [`reference/dsp.md`](../reference/dsp.md) |
| the editor shows a stale material | a headless `compile` left a `.uasset` that now wins over the in-memory product. Delete it, or *Tools ▸ DreamShader ▸ Make Ephemeral* |
| `compile -All` was green but nothing changed | an empty source list is a Warning and exits `0`; or every file was skipped as unchanged — add `-Force` |
| a commandlet run did nothing and logged `DSH9110` | a flag's value is neither on nor off (`-Force=banana`, an empty `-Force=`). Write it bare, or `=true` / `=false` (also `1`/`0`, `yes`/`no`, `on`/`off`) |
| VS Code lost a header's squiggles after another material compiled | compiling a file clears the diagnostics filed under every header it includes — including the ones another material put there. Compile the other material again |

## Where diagnostics live

| Surface | Contents |
| :-- | :-- |
| Output Log / `dsc.ps1` output | every message, success included. The only surface a headless run has |
| Material Content Browser | per-file status and diagnostics, live from the editor |
| `Saved/DreamShader/Bridge/diagnostics.json`, `diagnostics/` shards, `bridge.db` | what the VS Code and Rider extensions show: each record with its `code`, `severity` (`error`, `warning` or `info`), file, line and column. A failed compile files all its records, a successful one its warnings and notes *(since 2.1.0)* |

**None of the bridge files are written by a commandlet**, so a headless run cannot be read from
`diagnostics.json` — read the log. A message that names a `.dsh` line came from a file that includes it: a header
is never compiled on its own.

## See also

- [`Docs/diagnostics/README.md`](../../Docs/diagnostics/README.md) — every code
- [`Docs/tools/bridge.md`](../../Docs/tools/bridge.md) — how the extensions receive diagnostics
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — reproducing it headlessly
