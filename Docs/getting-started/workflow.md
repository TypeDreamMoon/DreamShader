---
title: Daily Workflow
description: The save, debounce and generate loop — what triggers a rebuild, what is skipped, what editing a .dsh invalidates, and where to look when something fails.
---

The daily loop is: edit a source, save, let the plugin compile, look at the result. The editor extension improves the
*editing* half; generation belongs to the Unreal plugin and happens only in the editor process (or in a commandlet or a
cook).

## The loop

1. Edit a source under `DShader/` — a `.dss`, `.dsi`, `.dsp`, `.dsm`, `.dsf` or `.dsh`.
2. Fix what the editor extension reports locally first — syntax, unknown identifiers, bad imports.
3. **Save.** The source directory watcher sees the change.
4. A debounce timer waits *Save Debounce Seconds* — `0.25` by default, clamped to `[0.05, 10.0]` — so a burst of saves
   triggers one compile.
5. DreamShader compiles the file and writes the result to the Output Log.
6. Look at the material through *Tools ▸ DreamShader ▸ Material Content Browser* or the extension's preview.

Auto-compile is governed by two project settings: **Auto Compile On Save** (on by default) and **Save Debounce
Seconds**. With auto-compile off, the watcher ignores file changes entirely and you trigger compiles by hand.

## What one compile does

(since 2.0.0) Every kind of source takes the same path: a `.dsm` / `.dsf` is read by the legacy front end into the
same syntax tree as a `.dss`, then preprocessed, bound, lowered to the graph IR, validated and emitted exactly like one.
Each imported `.dsh` is parsed **on its own** and its declarations become names of the current file — no text is pasted
in any more.

| Stage | Does | On failure |
| --- | --- | --- |
| File-kind gate | builds `.dss`, `.dsi`, `.dsp`, `.dsm` and `.dsf`; a `.dsh` is never built on its own | `DSH8296` |
| Preprocess | evaluates [`#if` / `#define`](../language/preprocessor.md) with the project's define table (since 1.9.0) | `DSH1xxx` |
| Parse | one of the two front ends | `DSH2xxx`, `DSH3xxx` |
| Bind | names, types, calls, settings, imported headers | `DSH4xxx`–`DSH7xxx`, `DSH8292`–`DSH8295` |
| Build key | the hash of the file and every imported header, plus the settings and versions that decide the output | — |
| Lower, validate | lowers the bound tree to the graph IR, folds, deduplicates and prunes, then validates | `DSH43xx`, `DSH6xxx` |
| Emit | one asset per product, in dependency order: a function called in the same file is built first | `DSH8xxx` |

A stage reports **all** of its errors, and a stage with errors stops before the next one. The full table is on
[Pipeline](../generation/index.md#pipeline).

On success there is one line per product, and a `Warnings:` block when there were warnings:

```text
Generated Material /Game/Materials/M_Emissive from .../M_Emissive.dss.
Generated MaterialFunction /Game/Functions/F_Tint from .../F_Tint.dss.
Skipped /Game/Materials/M_Other from .../M_Other.dsm; source hash is unchanged (build key 9f2c41ab).
Compiled .../Helpers.dsm; it declares no material and no exported function, so no asset was written.
```

`{Kind}` is `Material`, `MaterialFunction`, `MaterialLayer`, `MaterialLayerBlend`, `MaterialInstance` or
`PassPipeline`. A file that declares only helpers or `VirtualFunction`s compiles and writes nothing; a 1.x file with no
block at all is a **failure** (`DSH2254`).

## The file kinds in practice

| Kind | You edit it to | Saving it |
| --- | --- | --- |
| `.dss` | change a material, or a set of material functions / layers / blends | compiles each of its `export`s |
| `.dsi` | change a material instance's parent or overrides | compiles the instance; compiles the parent first when it is missing or older than its source |
| `.dsp` | change a Custom Pass pipeline (UE 5.8) | compiles the pipeline asset, and the render targets of its exported buffers |
| `.dsm` | change a 1.x material | compiles the material and the function assets declared in the same file |
| `.dsf` | change a 1.x function asset | compiles the function assets it declares |
| `.dsh` | change shared helpers | **generates nothing itself** — queues every source that imports or includes it |

## What triggers a compile

| Trigger | Forced | ThinCustom result | Scope |
| --- | --- | --- | --- |
| Auto-compile on save — the file watcher plus debounce | no | Ephemeral | the saved file; for a `.dsh`, every source that imports it |
| The editor startup scan, and the scan after a setting the build key covers changes | no | Ephemeral | all project sources |
| *Recompile DSM*, *Clean Generated Shaders* | yes | Ephemeral | all project sources |
| Material Content Browser *Compile* / thumbnail refresh | yes | Ephemeral | one source |
| Live preview rendering | depends on the request | Ephemeral | one source |
| *Materialize*, and creating a child instance of an Ephemeral material | yes | **Materialized** | one source |
| Commandlet `-run=DreamShader` | with `-Force` | **Materialized** | as specified |
| Cook, on the cook director process only | yes | **Materialized** | all project sources |

**Note:**

Only a `ThinCustom` product can live in memory: the interactive editor keeps it **Ephemeral** until a cook, the
commandlet or an explicit *Materialize* writes it to disk. A `Graph`-backend material, and every material function,
layer and blend, is saved on every successful build (since 2.0.0). See
[In-memory Materials](../generation/ephemeral.md).

A batch compiles in dependency order: imported headers, a `.dsi`'s parent and the materials a `.dsp` references come
first. `compile -All` and the cook compile every `.dsf` first, then retry what failed in passes until one builds nothing
new (since 2.1.2) — so a material that calls a function another source generates no longer fails just because its
path sorts first.

## When a rebuild is skipped

Every generated asset stores two package-metadata keys:

| Key | Value |
| --- | --- |
| `DreamShader.SourceFile` | the source path relative to the project directory, with forward slashes |
| `DreamShader.SourceHash` | the CRC32 of the **build key**, eight hex digits |

The build key covers the **preprocessed** text of the file and of every header it imports, the defines it read, the
settings that decide the output, and the plugin and engine versions. On a non-forced compile the work is skipped when the
asset exists, the stored source path matches (case-insensitively) and the stored build key matches, with
`Skipped {AssetPath} from {File}; source hash is unchanged (build key {BuildKey}).`

| Change | Effect |
| --- | --- |
| edit `M_Foo.dss` | invalidates `M_Foo.dss` |
| edit `Common.dsh`, imported by two materials | invalidates **both** materials |
| change a value in *Preprocessor Defines* | invalidates only the sources that read that name |
| adjust whitespace, edit a comment | invalidates too — the comparison is of text, not meaning |
| move the project, or check the same repository out elsewhere | invalidates nothing — paths are stored project-relative (since 2.1.1) |
| upgrade the plugin | every asset rebuilds once — the version is in the build key; 2.2.0's key format is `DSK7`, as in 2.1.2 |
| rename the source | the stored path no longer matches, so nothing is skipped |

To force a rebuild through a non-forced trigger, change the source text or delete the generated asset. *Recompile DSM*
and the commandlet's `-Force` go past the build key. See [Regeneration ▸ The build key](../generation/caching.md).

## Editing a `.dsh`

Saving a header queues every source that imports or includes it, and each one decides by its build key whether to
rebuild. Compiling a `.dsh` directly (a commandlet `-Source` pointing at one, for example) is `DSH8296` — a header is
never built on its own.

The Material Content Browser shows a header as `function · used by N material(s)`, and its inspector lists the
dependents, so you can see the blast radius before you edit.

## What a rebuild destroys

Generated assets are products of the source, not documents. Regeneration clears generated comment boxes, deletes every
expression, resets the material's settings to engine defaults, then reapplies the settings and rebuilds. Manual edits
inside the asset are lost, with two exceptions: comment boxes whose text does **not** start with `DreamShader: `, and
parameter overrides on a generated ThinCustom instance — those are put back by name (since 1.9.0), and a parameter
renamed or deleted in the source loses its override.

**Warning:**

**A hand-edited asset is not overwritten silently.** An asset edited in the editor since it was generated is refused
by the next compile (`DSH8207`), with three ways out: **Revert to Source** (the source was right), **Adopt Into Source**
(the asset was right; decompile it back), **Detach From DreamShader** (neither; stop managing it). `-Force` does not
get past this. An asset open in an asset editor is refused too (`DSH8206`) — close the editor tab and compile again.
See [Regeneration](../generation/regeneration.md).

DreamShader also refuses to overwrite assets it did not generate: a material at the target path with no DreamShader
provenance metadata fails the compile with `DSH8201`.

## When something fails

Every diagnostic carries a `DSHnnnn` code and is reported at the line and column of the construct it is about:

```text
I:/MyProject/DShader/Materials/M_Minimal.dss(8,24): DSH4200: 'Tnt' is not declared; did you mean 'Tint'? Names are case-sensitive.
```

| Where | What it shows |
| --- | --- |
| Output Log | every message under `LogDreamShader`: errors, then warnings, then notes |
| Material Content Browser | the first error on the source's row, every record in the inspector |
| Editor extension | the per-file list, located to line and column; a successful compile's warnings too (since 2.1.0) |

A record raised inside a header points at the header, not at the file that imports it. Look the code up on
[Diagnostics](../diagnostics/index.md).

## Migrating an existing material

Right-click a `UMaterial`, `UMaterialFunction` or material instance in the Content Browser and use *DreamShader ▸ Export
.dss* / *Export .dsi* (*Export Legacy .dsm / .dsf* for 1.x text). The output lands under `DShader/Decompiled/` and opens
automatically; what the decompiler could not state is listed at the top of the file, each with its code. Treat it as a
first draft — sort out naming, helper layering and includes before adopting it as the source. See
[Decompiler](../tools/decompiler.md).

Existing `.dsm` / `.dsf` files are rewritten as `.dss` by [`dsc migrate`](../tools/migrate.md), which proves
both build the same graph before it writes.

## Next

- [Pipeline](../generation/index.md) — every stage and the codes it can raise
- [Regeneration](../generation/regeneration.md) — the full list of what survives
- [Editor tools](../tools/editor-integration.md) — the Tools menu, the Material Content Browser, Make Ephemeral
- [VSCode and Rider](../tools/vscode.md) — diagnostics, previews and package commands
- [Commandlet](../tools/commandlet.md) — headless compilation
