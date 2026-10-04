---
name: dream-shader-verify
description: Check and compile DreamShader sources headlessly — one file or the whole DShader tree — without opening the Unreal editor, and read back every diagnostic with its file, line and code. Use when asked to verify, validate, check, build, compile, test or CI-gate .dss / .dsi / .dsp / .dsm / .dsf sources, or to find out whether a material still builds.
---

# dream-shader-verify `<file>` | `-All`

The gate every other DreamShader skill ends with. The driver, `.skill/dsc.ps1`, wraps
`UnrealEditor-Cmd.exe <project> -run=DreamShader <verb>` so a check is one command and one exit code.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Three levels

| Verb | Goes as far as | Writes | Use it |
| :-- | :-- | :-- | :-- |
| `check` | parse, bind, lower, validate — everything before an asset | **nothing** | first, and after every edit: the fast gate |
| `compile` | the asset | the `.uasset` files (and a `.dsp`'s render targets and registry files) | to prove the file builds |
| `check -Shaders` | the asset **and its shaders**, for the project's shader formats | the assets, as `compile` does | for a `/// @custom` body or a `.dsp` with HLSL passes — the only thing that compiles that HLSL |

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/UI/M_Panel.dss
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/Passes/CP_Outline.dsp -Shaders -CleanNew
```

Exit `0` success, `1` failure — the commandlet's own codes. Run from anywhere inside the project: the driver
walks up for the `.uproject` and resolves the engine from its `EngineAssociation`.

**Every run boots the editor** — tens of seconds before DreamShader gets control. One run over many files costs
about what one file does, so batch: edit everything, then `check -All` once.

| Argument | Effect |
| :-- | :-- |
| *(positional)* | the source — absolute, or relative to the working directory. `.dss .dsi .dsp .dsm .dsf`; a `.dsh` is checked through the files that include it |
| `-All` | every source of every source root — the project's `DShader/` **and each enabled plugin's** — except `Packages/`. `compile` builds `.dsf` first, the rest in path order |
| `-Force` | `compile` / `check -Shaders`: rebuild even when the source hash is unchanged. Without it an unchanged file logs `Skipped … source hash is unchanged …` and proves nothing |
| `-CleanNew` | delete the `.uasset` files this run created (they were not on disk before it), then prune the emptied folders |
| `-DiagnosticsOut <file>` | `check`: the diagnostics as JSON as well |
| `-Platform SM6,SM5` / `-Quality High` / `-Timeout 120` | `check -Shaders`: which formats and quality levels, and how long one material may take |
| `-Project` / `-Engine` | override the discovery; `UE_ENGINE_ROOT` works too |
| `-Raw` | the whole engine log instead of the DreamShader messages |

## What you get back

```text
dsc: compile  project=MyProject.uproject  engine=C:\Program Files\Epic Games\UE_5.8
LogDreamShader: Display: Generated Material /Game/UI/M_Panel.M_Panel from …/DShader/UI/M_Panel.dss.
Warnings:
…/DShader/UI/M_Panel.dss(13,20): DSH7233: A slider range ('@slider', …) is for a scalar parameter; this one is 'float3', which has no slider, so the range is ignored.

Assets written to disk by this run:
  Content/UI/M_Panel.uasset  [NEW]
    deleted (-CleanNew)

dsc: OK (exit 0)
```

A failure lists **every** error of the stage that failed, each with its position and code:

```text
LogDreamShader: Error: …/M_Broken.dss(7,16): DSH4200: 'Tin' is not declared in this scope.
LogDreamShader: Error: …/M_Broken.dss(8,15): DSH4208: 'saturte' is not a function, a builtin or a struct.
LogDreamShader: Error: …/M_Broken.dss(9,16): DSH4250: 'mix' is the GLSL spelling; this language is HLSL, so write 'lerp'.
LogDreamShader: Error: DreamShader check: 1 source(s), 3 error(s), 0 warning(s). RESULT=FAILED
dsc: FAILED (exit 1)
```

The pipeline stops between stages — parse, bind, lower, validate, emit — not at the first error: fix them all,
then run again. A syntax error can hide the binder's errors behind it, so a second run may find more.

## The thing to understand about `compile`

**The commandlet writes real `.uasset` files. The interactive editor, for a ThinCustom material (the project
default), does not:** it keeps the product in memory (Ephemeral), and the source is the authoring surface. A
file a headless run left on disk then wins: the editor rebuilds that material on disk instead of in memory
(it logs so, and *Tools ▸ DreamShader ▸ Make Ephemeral* undoes it).

So `-CleanNew`: the driver records every `.uasset` under the Content folders of the project and its plugins
before the run, and deletes only the ones that were not there before. An asset that existed and was rewritten
is never deleted; when git tracks it, it is reported in red with the command that restores it:

```text
  Content/UI/Cards/M_Foil.uasset  [TRACKED AND MODIFIED]
    restore with: git -C "D:\Work\MyProject" checkout -- "Content/UI/Cards/M_Foil.uasset"
```

That is not a driver bug: the source genuinely regenerated the asset, and only the user knows whether the new
bytes should be kept. (A Graph-backend material and a material function are saved on every build in the editor
too; for those, a rewrite is expected.)

## Gotchas

- **`-All` covers the plugins' sources too.** Every enabled plugin with a `DShader/` folder is a source root, so
  `compile -All` rebuilds its materials into its own `Content/` — files another repository owns. Compile single
  files while iterating; keep `-All` for `check`, or for a deliberate gate.
- **A `.dsp` builds on UE 5.8+ only** (`DSH8300` below that). Besides the pipeline it writes a render target per
  exported buffer, and an HLSL pass rewrites the project's `DShader/.dreampass/` registry and snapshots — source
  files to commit, which the driver lists after the run.
- **`check -Shaders` saves assets**, because 2.0 has no transient asset; the driver reports them like
  `compile`'s. It drives the cook-target shader compilers (a commandlet does not render), so its HLSL errors come
  from the engine log.
- **A green `compile -All` can have built nothing**: an empty source list logs
  `DreamShader commandlet found no source files to compile.` at Warning and exits `0`. Look for `Generated …`.
- **Write flags bare.** `-Force=true` or `-Check=true` straight to the commandlet is not read at all — the flag
  stays off, with no message. The driver always passes them bare; this bites hand-written command lines.
- **The first bare token after `-run=DreamShader` is the verb**, unconditionally — `-run=DreamShader Source=X
  compile` runs a verb called `Source=X`.
- **The editor bridge never runs inside a commandlet**: no `diagnostics.json`, no squiggles in VS Code. The log is
  where a headless run's diagnostics are — and the driver prints them, each report whole.
- **Use PowerShell, not Git Bash**, for an asset path: Git Bash rewrites `/Game/X` into
  `C:/Program Files/Git/Game/X`.

## Troubleshooting

| Symptom | Fix |
| :-- | :-- |
| `EngineAssociation '…' is not registered.` | pass `-Engine <root>` or set `UE_ENGINE_ROOT` |
| `Could not find a .uproject at or above …` | pass `-Project` |
| `UnrealEditor-Cmd.exe not found at …` | the engine root is wrong — it is the directory *containing* `Engine/` |
| `dsc: FAILED (exit 1)` with no `LogDreamShader` line | re-run with `-Raw`; the failure was before DreamShader got control (a missing module, a crash at boot) |
| `Skipped … source hash is unchanged …` | add `-Force` |
| `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: …` | you pointed at a `.dsh` or another file. A header builds nothing — compile a file that includes it |
| `… (mount '/X/' is not the project or one of its plugins …)` | the asset went to an engine or external mount; find and clean it by hand |

## See also

- [`Docs/tools/commandlet.md`](../../Docs/tools/commandlet.md) — every verb and flag behind the driver
- [`Docs/generation/ephemeral.md`](../../Docs/generation/ephemeral.md) — Ephemeral and Materialized
- [`Docs/generation/caching.md`](../../Docs/generation/caching.md) — the hash skip `-Force` bypasses
- [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) — resolving a code
