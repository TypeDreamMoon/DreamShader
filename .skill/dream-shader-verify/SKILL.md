---
name: dream-shader-verify
description: Compile DreamShaderLang sources headlessly to check they build — one file or the whole DShader tree — without opening the Unreal editor. Use when asked to verify, validate, build, compile, test, or CI-gate .dsm / .dsf / DreamShader sources, or to check whether a material still generates.
---

# dream-shader-verify `<file>` | `-All`

The compile gate. Wraps `UnrealEditor-Cmd.exe -run=DreamShader compile` so a check is one command
and one exit code. This is the harness the other DreamShader skills call.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Prefer the `dream` MCP server when it is connected

If the `dream` MCP server is available, call `dream_build` instead of the script below,
`dream_diagnostics` to read the editor's standing findings, and `dream_preview` to actually **look
at** the material.

Through a running editor it uses the bridge, which this script never does: a compile comes back in
a few hundred milliseconds instead of roughly 24 s of engine boot, with a real per-request result
and the file/line diagnostics attached. It also sidesteps the whole problem this page describes
below — the editor generates **in memory**, so no `.uasset` is written and nothing shadows anything.
When no editor is running it runs this same script, `-CleanNew` and all.

`dream_preview` is the one thing neither path can otherwise give you: it renders the material and
hands the image back, so a wrong-looking result is visible rather than inferred. There is no
headless equivalent — the commandlet runs `-nullrhi`.

Fall back to the script when the server is not connected, and for `decompile` and `optimize`, which
it deliberately does not wrap.

## Run it

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Panel.dsm -Force -CleanNew
```

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile -All -Force -CleanNew
```

Exit `0` success, exit `1` failure — the commandlet's own codes. Run it from anywhere inside the
project.

Editor boot dominates the cost: measured on this project, the single-file compile above and the
`-All` run over six sources both took **≈24 s**. Compiling one file is not meaningfully cheaper
than compiling everything, so batch edits rather than looping per file.

| Argument | Effect |
| :-- | :-- |
| *(positional)* | the source file — absolute, or relative to the working directory |
| `-All` | every project source. `.dsf` function files build before `.dsm` materials, so a material's dependencies exist first |
| `-Force` | bypass the source-hash skip. Without it an unchanged file logs `Skipped … source hash is unchanged.` and proves nothing |
| `-CleanNew` | delete the `.uasset` files this run wrote, **but only those git reports untracked**, then prune the emptied folders |
| `-Project` | the `.uproject`. Defaults to the nearest one at or above the target, then the working directory |
| `-Engine` | engine root. Defaults to the `EngineAssociation` lookup; `UE_ENGINE_ROOT` also works |
| `-Raw` | print the whole engine log instead of just the `LogDreamShader` lines |

## What you get back

```text
dsc: compile  project=TallyZeroMoment.uproject  engine=F:/UnrealEngine/UE_Moon
LogDreamShader: Display: Generated /Game/DreamShaderSkillProbe/M_SkillProbe.M_SkillProbe from …/M_SkillProbe.dsm.

Assets written to disk by this run:
  Content/DreamShaderSkillProbe/M_SkillProbe.uasset  [NEW (untracked)]
    deleted (-CleanNew)

dsc: OK (exit 0)
```

A failure names the file, line and column:

```text
LogDreamShader: Error: …/M_SkillProbeBroken.dsm(22,9): Failed to evaluate Graph assignment for 'bad'. Unknown Graph function 'saturte'.
dsc: FAILED (exit 1)
```

## The thing to understand about this command

**The commandlet writes real `.uasset` files. The interactive editor does not.** Inside the editor
DreamShader generates materials in memory, deliberately — the source file is the authoring surface,
and no `.uasset` appears in the Content Browser. A commandlet run persists them, and those files
then **shadow** the editor's in-memory generation on the next load (logged as a warning).

So a verification run leaves the project subtly different from how it started. `-CleanNew` is the
answer: it deletes exactly the assets that git says are untracked, prunes the folders they leave
behind, and refuses to touch anything tracked. Assets that were already tracked and got overwritten
are reported in red, with the restore command:

```text
  Content/UI/Cards/Materials/M_TZM_CardFoil_Holographic.uasset  [TRACKED AND MODIFIED]
    restore with: git -C "I:\UnrealProject_58\TallyZeroMoment" checkout -- "Content/…"
```

That is not a bug in the driver — the source genuinely regenerated the asset, and only you know
whether the new bytes should be kept.

## Gotchas

- **`-All` overwrites tracked assets.** Any `.dsm` whose `Name=` targets a path that already holds a
  committed `.uasset` rewrites it. `-CleanNew` will *not* delete those; it reports them. Verified on
  this project: `-All` rewrote `Content/UI/Cards/Materials/M_TZM_CardFoil_Holographic.uasset`. Use
  single-file compiles while iterating, and reserve `-All` for a deliberate CI gate.
- **A compile stops at the first failing `Graph` statement.** Three seeded errors reported one. Fix,
  recompile, repeat — do not expect a full error list.
- **`compile -All` on an empty source list exits `0`**, logging
  `DreamShader commandlet found no source files to compile.` at Warning. A green run does not prove
  anything was compiled — check the `Generated …` lines.
- **The first bare token is taken as the command name, unconditionally.** Writing an option without
  its dash first (`-run=DreamShader Source=X compile`) consumes `Source=X` as the command. The
  driver always emits the command first, but this bites hand-rolled invocations.
- **An unrecognised boolean value means *on*.** `-Force=banana`, `-Force=disable` and `-All=never`
  all enable the flag, with no diagnostic. Use bare flags.
- **The editor bridge never runs inside a commandlet** — no source watcher, no auto-compile-on-save,
  no WebSocket on 17864, no `diagnostics.json`, no `bridge.db`. What you get is the compile result
  and nothing else.
- **Every `LogDreamShader` line is emitted twice**, once raw and once re-wrapped through `LogInit`.
  The driver de-duplicates; raw `-run=` output does not.
- `-nullrhi` keeps the run off the GPU. Drop it (edit the driver) only when something needs real
  shader compilation, such as reading back material compile errors.

## Troubleshooting

| Symptom | Fix |
| :-- | :-- |
| `EngineAssociation '{…}' is not registered.` | pass `-Engine <root>` or set `UE_ENGINE_ROOT`. Source builds register a GUID under `HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds` |
| `Could not find a .uproject at or above …` | pass `-Project` |
| `UnrealEditor-Cmd.exe not found at …` | the engine root is wrong — it must be the directory *containing* `Engine/` |
| `dsc: FAILED (exit 1)` with no `LogDreamShader` line | re-run with `-Raw`; the failure was before DreamShader got control |
| `Skipped … source hash is unchanged.` | add `-Force` |
| `DreamShader compile requires a .dsm or .dsf file: …` | you pointed at a `.dsh`. Headers generate nothing — compile the dependent file |
| asset paths appear outside `/Game` | the driver cannot map them to `Content/`; clean by hand |

## See also

- [`Docs/tools/commandlet.md`](../../Docs/tools/commandlet.md) — the full flag surface behind the driver
- [`Docs/generation/in-memory.md`](../../Docs/generation/in-memory.md) — why the editor does not write assets
- [`Docs/generation/caching.md`](../../Docs/generation/caching.md) — the hash skip `-Force` bypasses
- [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) — resolving a message
