# Agent skills

> [DreamShader](../Docs/index.md) » **Agent skills**

Six skills that let an agent write, migrate, decompile and verify DreamShader sources without opening the
Unreal editor, and the driver they all call. The driver is the deliverable; each `SKILL.md` is its man page
for one task, and each reference page condenses a language to what an author needs.

| | |
| :-- | :-- |
| Source of truth | `Plugins/DreamShader/.skill/` — edit here |
| Published to | `<project>/.claude/skills/` by [`sync-skills.ps1`](sync-skills.ps1); that is where Claude Code finds them |
| Driver | [`dsc.ps1`](dsc.ps1) — wraps `UnrealEditor-Cmd.exe <project> -run=DreamShader …` |
| Engines | Unreal Engine `5.3` – `5.8`, Win64; Custom Pass (`.dsp`) needs `5.8` |
| Plugin | DreamShader `2.1.0`, compiled — the commandlet lives in the `DreamShaderEditor` module |
| Shell | PowerShell 7 (`pwsh`); every script says `#Requires -Version 7.0` |

## Skills

| Skill | Argument | Does |
| :-- | :-- | :-- |
| [`dream-shader-create`](dream-shader-create/SKILL.md) | `<description>` | writes a new material, function, instance or Custom Pass pipeline from plain language — `.dss` / `.dsi` / `.dsp` — then checks and compiles it |
| [`dream-shader-verify`](dream-shader-verify/SKILL.md) | `<file>` \| `-All` | `check` (writes nothing), `compile`, `check -Shaders`; exit `0` / `1` with every diagnostic |
| [`dream-shader-diagnose`](dream-shader-diagnose/SKILL.md) | `<message>` | routes a `DSHnnnn` code to its page, explains it, fixes it — and the failures that have no message |
| [`dream-shader-decompile`](dream-shader-decompile/SKILL.md) | `<asset>` | exports a material, function, layer, blend, instance or pipeline back to source — `.dss` / `.dsi` / `.dsp`, or `.dsm` / `.dsf` with `-Format Legacy` |
| [`dream-shader-optimize`](dream-shader-optimize/SKILL.md) | `<file>` | turns a decompiled or migrated source into a hand-written one, and proves the graph unchanged with `dump-graph` |
| [`dream-shader-migrate`](dream-shader-migrate/SKILL.md) | `<file>` \| `-All` | rewrites 1.x `.dsm` / `.dsf` / `.dsh` as 2.0 with `dsc migrate`, which proves each rewrite before writing it |

| Reference | For |
| :-- | :-- |
| [`reference/dss.md`](reference/dss.md) | the 2.0 language — `.dss` materials and functions, `.dsi` instances: the entry, `uniform`s and their `///` tags, `#pragma material`, `UE.*` calls, `/// @custom`, the traps and their codes |
| [`reference/dsp.md`](reference/dsp.md) | Custom Pass pipelines — buffers, passes, injection points, HLSL passes, the materials a pipeline uses, the traps |
| [`reference/legacy.md`](reference/legacy.md) | reading and maintaining 1.x `.dsm` / `.dsf` / `.dsh`, as the legacy front end builds them today |

### Routes

```text
new source          dream-shader-create ──► dream-shader-verify
existing asset      dream-shader-decompile ──► dream-shader-optimize ──► dream-shader-verify
1.x sources         dream-shader-migrate ──► dream-shader-optimize (when it should read hand-written)
stuck               dream-shader-diagnose
```

## Quick start

```powershell
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/UI/M_Panel.dss
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew
```

Run it from anywhere inside the project. The driver walks up for the `.uproject`, resolves the engine from its
`EngineAssociation`, prints the DreamShader messages and a verdict, and exits with the commandlet's own code.

| Verb | Does |
| :-- | :-- |
| `check <file>` \| `-All` | compiles as far as IR validation and writes **no asset** — the fast gate. `-Shaders` builds the products and compiles their shaders, reporting HLSL errors against source lines — for a `.dsp`, by pre-checking every HLSL pass in its slot |
| `compile <file>` \| `-All` | builds the assets. `-Force` bypasses the source-hash skip; `-CleanNew` deletes what the run created |
| `decompile <asset>` | 2.0 text by default: `.dss`, a `.dsi` for a material instance, a `.dsp` for a pipeline — [`Docs/tools/decompiler.md`](../Docs/tools/decompiler.md) |
| `migrate <file>` \| `-All` \| `-Root <name>` | rewrites 1.x sources as `.dss`, proving each rewrite first; `-Check` writes nothing — [`Docs/tools/migrate.md`](../Docs/tools/migrate.md) |
| `dump-graph <file>` \| `-All` | a canonical JSON of each generated graph, writing no asset — two captures diff with `git diff --no-index` |
| `dump-ir`, `index`, `export-catalog` | language-service tools: the lowered IR, the symbol index, the builtin node catalog |
| `dump-layout <file>` \| `-All` | draws the IR graph layouts — `Blocks`, `SourceBands`, `Layered` — as SVG, building nothing |
| `fmt <file>` \| `-All` | rewrites 2.0 sources in the printer's layout, refusing any file it cannot vouch for; `-Check` writes nothing |
| `list-generated <file>` \| `-All` | names every asset the sources build, building none: `-ListAs Packages \| Files \| GitIgnore \| Json` — [`Docs/generation/source-control.md`](../Docs/generation/source-control.md) |
| `pass-registry` | lists the Custom Pass HLSL slots in `DShader/.dreampass`; `-Gc` frees the dead ones; `-Rebuild` compiles every `.dsp` again and rewrites the registry — [`Docs/tools/commandlet.md`](../Docs/tools/commandlet.md#pass-registry) |

Every verb that takes a source takes all five compilable kinds: `.dss`, `.dsi`, `.dsp`, `.dsm`, `.dsf`. The full
flag surface: [`Docs/tools/commandlet.md`](../Docs/tools/commandlet.md); the skills' view of it:
[`dream-shader-verify`](dream-shader-verify/SKILL.md).

## What the driver adds over the raw commandlet

| | |
| :-- | :-- |
| Engine resolution | from the `.uproject`'s `EngineAssociation`, via the registry — no hard-coded path |
| Project discovery | walks up from the target file (or `-SourceFile`), then the working directory |
| Whole messages | a compile's report is one multi-line log message — every `Generated` line, then `Warnings:` and the warnings, or every error — and the engine prefixes only its first line. The driver keeps each message whole, and drops the engine's end-of-run summary that repeats them |
| Asset accounting | records every `.uasset` under the Content folders of the project and its plugins before the run, and reports what the run created (`NEW`) and rewrote — the latter classified by git where the file is in a repository, with the restore command for a tracked one. It works in a project that is not a repository, and it counts what no log line names: a `.dsp`'s render targets, `check -Shaders`' assets |
| Cleanup | `-CleanNew` deletes only what the run created, and prunes the folders it leaves empty |
| Registry report | after a run that can touch them, the files under `DShader/.dreampass` that changed — sources to commit with the `.dsp` |
| Paths and lists | a relative `-Out` / `-DiagnosticsOut` is the working directory's, not the engine's Binaries folder; `-Define A=1,B` is split into one `-Define=` per item |

> [!IMPORTANT]
> **The commandlet writes real `.uasset` files; the interactive editor, for a ThinCustom material, does not.**
> DreamShader keeps a ThinCustom product (the project default) in memory in the editor — the source file is the
> authoring surface. A file a headless compile left on disk wins over that on the next load. Keep `-CleanNew` on
> while iterating; `check` writes nothing at all.

> [!WARNING]
> **`compile -All` overwrites assets you may not own.** It builds every source of every source root — the
> project's and **each enabled plugin's** `DShader/` — into those `Content/` folders, and `-CleanNew` never
> deletes what existed before: a rewritten tracked asset is reported in red, with its restore command. Compile
> single files while working; keep `-All` for `check`.

## Cost

Engine boot dominates: every run takes tens of seconds before DreamShader gets control, and one file costs about
what a whole tree does. Batch edits, then run once.

## Publishing

Claude Code auto-loads skills from `.claude/skills/`, searching the directory tree at and *above* where the agent
is working. `.skill/` is not on that path, so publish it:

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

A symlink or junction is not enough — the link *text* has to change. Each file is written for `.skill/`, so three
path families are rewritten against the destination:

| Written in `.skill/` | Published as |
| :-- | :-- |
| `](../../Docs/…)` | the real relative path to the plugin's `Docs/` |
| `](../reference/…)` | `](../dream-shader-reference/…)` — `reference/` is published under a name of its own, so another plugin's `reference/` never collides with it |
| `Plugins/DreamShader/` | the plugin's real path from the project root — `Plugins/Dream/DreamShader/` for a plugin in a group folder — the driver invocation included |

| Flag | Effect |
| :-- | :-- |
| *(none)* | publish to the host project — the nearest `.uproject` above the plugin |
| `-Target <dir>` | publish to a specific `.claude/skills`, absolute or relative |
| `-Check` | compare without writing; exit `1` on drift — a stale file, a missing one, or a published one whose source is gone |
| `-Prune` | remove what this run did not write: `dream-shader-*` directories and files whose source is gone, and the `reference/` files an older version published |

Published Markdown carries an HTML comment under the frontmatter marking it as generated; editing a published copy
shows up as drift. Whether `.claude/skills/` is committed is the host project's choice: committed, a teammate gets
the skills from the clone (under a `.gitignore` that excludes `.claude/*`, negate `!.claude/skills/`); not
committed, everyone runs `sync-skills.ps1` once.

## The other scripts

| Script | Does |
| :-- | :-- |
| [`build-plugin.ps1`](build-plugin.ps1) | `RunUAT BuildPlugin` across a list of engines — does the *plugin* still build on every engine it claims. Reports `BLOCKED` when another UnrealBuildTool holds the build mutex. Not a skill; not published |
| [`gen-diagnostics.ps1`](gen-diagnostics.ps1) | regenerates the machine-written half of `Docs/diagnostics/` from the raise sites in `Source/`; `-Check` is the CI gate. Hand-written prose is never dropped — a code that loses its raise site keeps it under *Retired codes* |

## Layout

```text
.skill/
├─ README.md                        this page
├─ dsc.ps1                          the driver — wraps -run=DreamShader
├─ sync-skills.ps1                  publishes into .claude/skills, rewriting paths
├─ build-plugin.ps1                 RunUAT BuildPlugin across a list of engines
├─ gen-diagnostics.ps1              Docs/diagnostics/ from the source
├─ reference/
│  ├─ dss.md                        the 2.0 language, for .dss and .dsi
│  ├─ dsp.md                        Custom Pass pipelines
│  └─ legacy.md                     the 1.x language, as built today
├─ dream-shader-create/SKILL.md
├─ dream-shader-verify/SKILL.md
├─ dream-shader-diagnose/SKILL.md
├─ dream-shader-decompile/SKILL.md
├─ dream-shader-optimize/SKILL.md
└─ dream-shader-migrate/SKILL.md
```

## Notes

- **Use PowerShell, not Git Bash, for anything taking an asset path.** Git Bash rewrites a leading-slash path
  such as `/Game/Materials/X` into `C:/Program Files/Git/Game/Materials/X`, and the asset "cannot be loaded".
- **A stage reports all of its errors at once.** The pipeline stops between stages, so a syntax error hides the
  binder's errors behind it; fixing it can bring out more.
- **The editor bridge never runs inside a commandlet** — no watcher, no WebSocket on 17864, no
  `diagnostics.json`. A headless run's diagnostics are in the log, which the driver prints.
- **Write commandlet flags bare** when calling it by hand: `-Force=true` is not read at all. The driver always
  writes them bare.

## See also

- [`Docs/contributing/index.md`](../Docs/contributing/index.md#the-engine-matrix) — `build-plugin.ps1`, and why 5.3 / 5.4 fail on a recent MSVC
- [`Docs/tools/commandlet.md`](../Docs/tools/commandlet.md) — the full flag surface behind the driver
- [`Docs/diagnostics/README.md`](../Docs/diagnostics/README.md) — every code
- [`Docs/language-v2/index.md`](../Docs/language-v2/index.md) — the 2.0 language
- <https://shader.toolchain.64hz.cn/docs> — the same reference, published, zh + en
