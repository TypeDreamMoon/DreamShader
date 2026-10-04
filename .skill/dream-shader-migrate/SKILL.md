---
name: dream-shader-migrate
description: Rewrite DreamShaderLang 1.x sources (.dsm / .dsf / .dsh) as 2.0 .dss with dsc migrate, which proves each rewrite builds the same graph before it writes anything. Use when asked to migrate, upgrade, port or convert 1.x DreamShader sources, a .dsm or .dsf file, or a whole DShader tree to the 2.0 language.
---

# dream-shader-migrate `<file>` | `-All` | `-Root <name>`

Turn 1.x sources into `.dss` without changing what they build. `dsc migrate` does the rewriting and the
proving; this skill is the order to run it in and what to do with what it reports.

Nothing forces a migration — the legacy front end keeps building 1.x sources through the same compiler.
Migrate when the user wants what only a `.dss` has: real expressions and control flow, `#include`, material
instances beside the material, comments that survive `dsc fmt`. Full reference:
[`Docs/tools/migrate.md`](../../Docs/tools/migrate.md).

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## Do this

**1 — See what will change.** Every 1.x leniency the rewrite has to spell out is also a diagnostic on the 1.x
source, so a `check` of the `.dsm` lists them — the legacy-rule codes `DSH5275`–`DSH5292`, warnings and
infos — without writing anything:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/Materials/M_Foo.dsm
```

**2 — Prove the rewrite, write nothing.**

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 migrate DShader/Materials/M_Foo.dsm -Check
```

For a whole tree, `migrate -All -Check` (every 1.x source under the project's own roots, `Packages/` excluded —
a plugin's root is left out) or `migrate -Root <name> -Check` (one root, by its display name or its plugin's
name). Batch: one run over many files costs one editor boot, the same as one file. (`-DryRun` is the same as
`-Check`.)

A file is refused before anything is rewritten unless it builds as 1.x first. Then it is proved: no comment is
lost (`DSH9092`) and the text builds as 2.0 (`DSH9097`) — either failure writes nothing. Two more checks are
**warnings**, which do not stop the write: the graph of the new text is compared with the old one node by node
(`DSH9098` names the first node that differs), and the asset must keep its path — the export gets a
`/// @name` when the file's place would name another one, and `DSH9098` says so when it cannot.

**3 — Read every `DSH9098`.** It is not noise: each one is a real difference between the two graphs. Decide
with the user whether the 2.0 behaviour is acceptable — typically it is a 1.x artefact (a node declared
narrower than it is, an explicit `.x` into a float pin). An error stops that file; the table below says what
to do.

**4 — Migrate.**

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 migrate DShader/Materials/M_Foo.dsm
```

This writes `M_Foo.dss` beside the old file and moves `M_Foo.dsm` to
`Saved/DreamShader/Migrated/Sources/<root>/<path>` (numbered `.1`, `.2`, … rather than overwriting an older
backup) — outside the source tree, and outside version control. A `.dsh` is rewritten **in place**, and stays a
`.dsh`. `-Out <dir>` writes the `.dss` files under another directory,
mirroring the tree, and leaves the 1.x files where they are: the way to look at the result first.

**5 — Check the result.**

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/Materials/M_Foo.dss
```

`check` builds no asset, which is what you want here: the `.dss` names the same asset the `.dsm` did, so a
`compile` would rewrite it on disk. The editor rebuilds it from the new source on its next load.

**6 — Report** the files written, the `DSH9098` differences and the decision taken on each, and remind the
user to commit the `.dss` and the deletion of the `.dsm` together — a commit with only one of them leaves the
asset with no source, or with two.

## What can stop a file

| Code | Cause | Fix |
| :-- | :-- | :-- |
| `DSH9090` | the source has `#if` lines — only the branch taken today would survive | resolve the condition by hand first |
| `DSH9091` | an `import` names a source root in front of its path | write the import relative to the file's own root, or migrate that file by hand |
| `DSH9097` | the migrated text does not build — a construct the migrator does not handle, or a 1.x leniency with no 2.0 spelling. The rejected text is kept under `Saved/DreamShader/Migrated/Rejected/` and the diagnostics point into it | fix the 1.x source, migrate again |
| `DSH9099` | the `.dss` already exists, or a file could not be written or moved. The 1.x file is never deleted unless the `.dss` was written | move the existing `.dss` aside, or check permissions |
| `DSH9095` | the file is not a `.dsm`, `.dsf` or `.dsh` | — |
| *(a 1.x error)* | the 1.x file does not build as it is, so there is nothing to prove the rewrite against | fix it first — [`dream-shader-diagnose`](../dream-shader-diagnose/SKILL.md) |

## Gotchas

- **A shared header waits.** Migrating one file takes a header with it only when nothing else includes that
  header; a shared one stays 1.x — which is fine, because a `.dsh` may hold both dialects. `-All` takes every
  1.x file, headers first. A header with no 1.x declarations left is skipped (`DSH9093`).
- **Plugin sources are not in `-All`.** A plugin's source root is read-only to `-All`; name it with `-Root`.
- **The old file is moved, not deleted** — unless `-NoBackup`. It lands under `Saved/`, which nothing commits,
  so the backup is local to this machine.
- **What a `.dss` looks like afterwards** is in [`reference/dss.md`](../reference/dss.md): `Function` and
  `GraphFunction` become `/// @custom` functions with the body verbatim, a `Namespace` function `N::F` becomes
  `N_F` with `/// @name N::F`, a `VirtualFunction` becomes `/// @asset` + `extern`, `mix` / `fract` / `mod` /
  `vec3` become `lerp` / `frac` / `fmod` / `float3`.
- **A migrated file is still machine-written.** It builds the same graph, which is the guarantee; making it
  read like hand-written 2.0 is [`dream-shader-optimize`](../dream-shader-optimize/SKILL.md)'s job, under the
  same rule — behaviour must not change.

## See also

- [`Docs/tools/migrate.md`](../../Docs/tools/migrate.md) — the full rewrite table, rule by rule
- [`Docs/language-v2/index.md`](../../Docs/language-v2/index.md) — the 2.0 language
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — `check` and `compile`
- [`dream-shader-decompile`](../dream-shader-decompile/SKILL.md) — for an asset that has no source at all
