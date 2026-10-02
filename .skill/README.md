# Agent skills

> [DreamShader](../Docs/index.md) » **Agent skills**

Five skills that let an agent author, migrate and verify DreamShaderLang sources without opening the
Unreal editor, and the driver they all call. The driver is the deliverable; each `SKILL.md` is its
man page for one task.

[`build-plugin.ps1`](build-plugin.ps1) sits alongside them and answers a different question — not
"does this source compile" but "does the *plugin* still build on every engine it claims to support".
It is not a skill and `sync-skills.ps1` does not publish it.

| | |
| :-- | :-- |
| Source of truth | `Plugins/DreamShader/.skill/` — edit here |
| Published to | `<project>/.claude/skills/` by [`sync-skills.ps1`](sync-skills.ps1); that is where Claude Code finds them |
| Driver | [`dsc.ps1`](dsc.ps1) — wraps `UnrealEditor-Cmd.exe … -run=DreamShader` |
| Engines | Unreal Engine `5.3` – `5.8`, Win64. Verified against a **5.8 source build** |
| Plugin | DreamShader `1.8.0`, compiled — `Binaries/Win64/UnrealEditor-DreamShaderEditor.dll` must exist |
| Shell | PowerShell 7 (`pwsh`) |

## Skills

| Skill | Argument | Does |
| :-- | :-- | :-- |
| [`dream-shader-create`](dream-shader-create/SKILL.md) | `<description>` | writes a new material or function from plain language, then compiles it to prove it builds |
| [`dream-shader-optimize`](dream-shader-optimize/SKILL.md) | `<file>` | dedupes, renames, retargets and restores lost state in a decompiled source |
| [`dream-shader-decompile`](dream-shader-decompile/SKILL.md) | `<asset>` | exports an existing material, function, layer, blend or material instance back to source — `.dss` / `.dsi` by default, `.dsm` / `.dsf` with `-Format Legacy` |
| [`dream-shader-verify`](dream-shader-verify/SKILL.md) | `<file>` \| `-All` | compiles headlessly; exit `0` / `1` |
| [`dream-shader-diagnose`](dream-shader-diagnose/SKILL.md) | `<message>` | routes a diagnostic to its pipeline stage, explains it, fixes it |

[`reference/dreamshaderlang.md`](reference/dreamshaderlang.md) holds the grammar subset an author
actually needs — file kinds, the `Shader` block, types, the 29 math builtins, the reserved-name and
identifier-rewrite traps. `create` and `optimize` both read it before writing a line.

### Routes

```text
new material        dream-shader-create ──► dream-shader-verify
existing asset      dream-shader-decompile ──► dream-shader-optimize ──► dream-shader-verify
stuck               dream-shader-diagnose
```

## Quick start

```powershell
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Panel.dsm -Force -CleanNew
```

Run it from anywhere inside the project. The driver walks up for the `.uproject`, resolves the
engine from its `EngineAssociation`, prints only the `LogDreamShader` lines plus a verdict, and
exits with the commandlet's own code.

| Flag | Effect |
| :-- | :-- |
| `-All` | every project source; `.dsf` function files build before `.dsm` materials |
| `-Force` | bypass the source-hash skip — without it an unchanged file logs `Skipped …` and proves nothing |
| `-CleanNew` | delete the `.uasset` files this run wrote, **only** those git reports untracked, then prune the emptied folders |
| `-Project` / `-Engine` | override the discovery; `UE_ENGINE_ROOT` works too |
| `-Raw` | the whole engine log instead of just the DreamShader lines |

Full flag surface and troubleshooting: [`dream-shader-verify`](dream-shader-verify/SKILL.md).

### The 2.0 verbs

The skills above were written for 1.x sources and still work: a `.dsm` / `.dsf` is built by the same
compiler as a `.dss`, through the legacy front end. The driver has more verbs than the skills use:

| Verb | Does |
| :-- | :-- |
| `check <file>` \| `-All` | compiles as far as IR validation and writes **no asset** — the fast gate. `-Shaders` builds the products and reports HLSL errors against source lines — for a `.dsp`, by pre-checking every HLSL pass in its slot |
| `migrate <file>` \| `-All` \| `-Root <name>` | rewrites 1.x sources as `.dss`, proving each rewrite first; `-Check` writes nothing — [`Docs/tools/migrate.md`](../Docs/tools/migrate.md) |
| `decompile <asset>` | 2.0 text by default; a material instance comes out as a [`.dsi`](../Docs/language-v2/instances.md), a Custom Pass pipeline as a [`.dsp`](../Docs/language-v2/passes.md) |
| `dump-ir`, `index`, `export-catalog` | language-service tools: the lowered IR, the symbol index, the builtin node catalog |
| `dump-layout <file>` \| `-All` | draws the IR graph layouts — `Blocks`, `SourceBands`, `Layered` — of every product as SVG, building nothing; `-Style` names one, `-Json` adds the coordinates |
| `fmt <file>` \| `-All` | rewrites 2.0 sources (`.dss`, `.dsi`, `.dsp`, 2.0 headers) in the printer's layout, refusing any file it cannot vouch for; `-Check` writes nothing and fails when a file would change |
| `list-generated <file>` \| `-All` | names every asset the sources build, building none: `-ListAs Packages \| Files \| GitIgnore \| Json`, `-Out <file>` — [`Docs/generation/source-control.md`](../Docs/generation/source-control.md) |
| `pass-registry` | lists the Custom Pass HLSL slots in `<source root>/.dreampass` and the state of each, writing nothing; `-Gc` frees the slots of pipelines and passes that are gone; `-Rebuild` compiles every `.dsp` again, frees what `-Gc` would, and rewrites the registry files — [`Docs/tools/commandlet.md`](../Docs/tools/commandlet.md#pass-registry) |

`compile`, `check`, `dump-ir`, `dump-layout`, `index`, `list-generated` and `dump-graph` take every
compilable source: `.dss`, `.dsi`, `.dsp`, `.dsm`, `.dsf`. A `.dsp` — a Custom Pass pipeline,
[`Docs/language-v2/passes.md`](../Docs/language-v2/passes.md) — builds on UE 5.8 only. The 2.0 language itself:
[`Docs/language-v2/index.md`](../Docs/language-v2/index.md); its Substrate sugar — operators, legacy
parameters on a slab, values built member by member — works in 1.x sources too:
[`Docs/language-v2/substrate.md`](../Docs/language-v2/substrate.md).

## What the driver adds over the raw commandlet

| | |
| :-- | :-- |
| Engine resolution | from the `.uproject`'s `EngineAssociation`, via the registry — no hard-coded path |
| Project discovery | walks up from the target file, then the working directory |
| Log de-duplication | every `LogDreamShader` line is emitted twice, once raw and once re-wrapped through `LogInit` |
| Asset accounting | classifies everything the run wrote as `NEW (untracked)` or `TRACKED AND MODIFIED`, the latter with its `git checkout --` command. It counts what the log reports as `Generated …`: a `.dsp`'s pipeline asset is counted, the render targets of its exported buffers are not — they log no such line, so `-CleanNew` leaves them — and the files a `.dsp` writes under `.dreampass/` are sources to commit, not assets |
| Cleanup | `-CleanNew` removes only the untracked ones and prunes the folders they leave behind |

> [!IMPORTANT]
> **The commandlet writes real `.uasset` files; the interactive editor does not.** DreamShader
> generates materials in memory by design — the source file is the authoring surface, and no asset
> appears in the Content Browser. A headless compile persists them, and those files then *shadow*
> Ephemeral generation on the next editor load.
>
> `-CleanNew` is the answer, and it is why the driver consults git rather than guessing. Keep it on
> while iterating.

> [!WARNING]
> **`compile -All` overwrites tracked assets.** Any `.dsm` whose `Name=` targets a path that already
> holds a committed `.uasset` rewrites it, and `-CleanNew` will not delete those — it reports them
> in red. Verified on this project: `-All` rewrites
> `Content/UI/Cards/Materials/M_TZM_CardFoil_Holographic.uasset`. Prefer single-file compiles while
> working; reserve `-All` for a deliberate gate.

## Cost

Editor boot dominates. Measured on this project, a single-file compile and a whole-tree `-All` over
six sources **both took ≈24 s**. Compiling one file is not meaningfully cheaper than compiling
everything, so batch edits rather than looping per file.

## Publishing

Claude Code auto-loads skills from `.claude/skills/`, searching the directory tree at and *above*
where the agent is working. `.skill/` is not on that path, so publish it:

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

A symlink or junction is not enough — the link *text* has to change. Each `SKILL.md` is written for
`.skill/<skill>/`, so two path families are rewritten against the destination:

| Written in `.skill/` | Published as |
| :-- | :-- |
| `](../../Docs/…)` | the real relative path to `Plugins/DreamShader/Docs` |
| `pwsh -File Plugins/DreamShader/.skill/dsc.ps1` | the driver, relative to wherever you now stand |

Both are computed, so a standalone plugin checkout publishes correctly too:
`sync-skills.ps1 -Target .claude/skills` there rewrites `Docs` to `../../../Docs` and the driver to
`.skill/dsc.ps1`.

| Flag | Effect |
| :-- | :-- |
| *(none)* | publish to the host project — the nearest `.uproject` above the plugin |
| `-Target <dir>` | publish to a specific `.claude/skills` |
| `-Check` | compare without writing; exit `1` on drift, so it works as a pre-commit gate |
| `-Prune` | remove published `dream-shader-*` directories this run did not write — after a rename or deletion |

Published files carry an HTML comment under the frontmatter marking them as generated. The
comparison accounts for it, so editing a published copy shows up as drift.

> [!NOTE]
> `.claude/skills/` is committed in this repo — `.gitignore` excludes `.claude/*` and negates
> `!.claude/skills/`, because git never descends into an excluded *directory*. A teammate gets the
> skills from the clone; re-run `sync-skills.ps1` after editing `.skill/`.

## Layout

```text
.skill/
├─ README.md                        this page
├─ dsc.ps1                          the driver — wraps -run=DreamShader
├─ build-plugin.ps1                 RunUAT BuildPlugin across a list of engines
├─ sync-skills.ps1                  publishes into .claude/skills, rewriting paths
├─ reference/
│  └─ dreamshaderlang.md            the grammar subset an author needs
├─ dream-shader-create/SKILL.md
├─ dream-shader-optimize/SKILL.md
├─ dream-shader-decompile/SKILL.md
├─ dream-shader-verify/SKILL.md
└─ dream-shader-diagnose/SKILL.md
```

## Notes

- **Use PowerShell, not Git Bash, for anything taking an asset path.** Git Bash rewrites a
  leading-slash path such as `/LGUI/Materials/X` into `C:/Program Files/Git/LGUI/Materials/X`, and
  the asset "cannot be loaded".
- **A compile stops at the first failing `Graph` statement.** A file with three mistakes reports
  one. Fix, recompile, repeat.
- **The editor bridge never runs inside a commandlet** — no source watcher, no auto-compile-on-save,
  no WebSocket on 17864, no `diagnostics.json`, no `bridge.db`. A headless run cannot be diagnosed
  from the bridge artifacts; the messages exist only in the log.

## See also

- [`Docs/contributing/index.md`](../Docs/contributing/index.md#the-engine-matrix) — `build-plugin.ps1`, and why 5.3 / 5.4 fail on a recent MSVC
- [`Docs/index.md`](../Docs/index.md) — the plugin's own reference manual
- [`Docs/tools/commandlet.md`](../Docs/tools/commandlet.md) — the full flag surface behind the driver
- [`Docs/diagnostics/index.md`](../Docs/diagnostics/index.md) — every message, by stage
- <https://shader.toolchain.64hz.cn/docs> — the same reference, published, zh + en
