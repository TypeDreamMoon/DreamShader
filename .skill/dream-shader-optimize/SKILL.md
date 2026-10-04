---
name: dream-shader-optimize
description: Clean up a machine-written DreamShader source — a decompiled or migrated .dss (or a 1.x .dsm / .dsf export) — into one a person would have written, without changing what it builds, and prove the graph is unchanged. Use when asked to optimize, clean up, tidy, refactor or make readable a source produced by the DreamShader decompiler or by dsc migrate.
---

# dream-shader-optimize `<file>`

A decompiled or migrated source builds the right graph, but it reads like a machine wrote it. This skill turns
it into the file a person would have written — and proves, node for node, that the graph did not change.

Paths below are relative to the plugin root, `Plugins/DreamShader/`.

## The rule

**Behaviour must not change.** Renaming a local, inlining a value read once, naming a value read twice,
moving repeated logic into a helper, swapping a class-exact node call for the HLSL that builds the same node —
safe. Reordering `lerp` arguments, folding `pow(x, 2)` into `x * x`, dropping a `saturate`, renaming a
parameter — not: those are changes, and they belong in a separate, explicitly requested edit.

## Do this

**1 — Read the file, its header warnings and the language.** Every `// Warning: DSHnnnn: …` line under
`// Decompiled by DreamShader from …` names something the text cannot state the way the asset has it; they
are the work list ([`Docs/diagnostics/DSH9xxx.md`](../../Docs/diagnostics/DSH9xxx.md)). Read
[`reference/dss.md`](../reference/dss.md).

**A `.dsm` / `.dsf` from `-Format Legacy`?** Do not clean 1.x text by hand. Decompile the asset again as 2.0
(the default) — or `dsc migrate` the file ([`dream-shader-migrate`](../dream-shader-migrate/SKILL.md)) — and
optimize the `.dss`. The 2.0 text starts far cleaner: a value read once is already inline, one read twice
already has one local, literals are exact.

**2 — Capture the graph before touching anything.** `dump-graph` generates the source in memory and writes a
canonical JSON of each asset's graph — node classes, properties, connections, pin order; no coordinates, no
GUIDs. It writes no asset:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 dump-graph DShader/Decompiled/Materials/Game/M_Steel.dss -Out Saved/Optimize/before
```

If an asset of that path already exists on disk, `dump-graph` reads it as it stands instead of generating it
(the run says so). A fresh decompile under `Decompiled/` has none.

**3 — Apply the passes below.** Then `check` it.

**4 — Prove nothing changed.**

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 dump-graph DShader/Decompiled/Materials/Game/M_Steel.dss -Out Saved/Optimize/after
git diff --no-index -- Saved/Optimize/before Saved/Optimize/after
```

Every hunk is a behavioural difference. The only acceptable ones are those the user asked for, and the
`Constant` nodes a `-Readable` decompile adds (the shader is the same). Anything else: undo that pass.

**5 — Retarget, last.** Move the file to the place that names the original asset, or give its `export` a
`/// @name /Game/Materials/Metal/M_Steel` — the step that makes this source authoritative. Then
`compile -Force -CleanNew` once; delete nothing until the rebuilt asset has been looked at in the editor.

**6 — Report** what changed, the dump diff verdict, and — separately — every header warning you could not
resolve from the source alone. That list is for the human; it needs the original asset open.

## The passes

### Resolve the header warnings

Each `// Warning:` is something the asset has and the text could not say. Some are fixed in the source (a
function asset that moved: correct the `/// @asset` path); some only in the original asset (re-assign it, decompile
again); some are accepted with the user (a node property the language cannot state). Delete a warning line only
once it is resolved or accepted.

### Rename what deserves a name

Locals named after their node (`Multiply_2`, `Sample`, `Combined`) become what the value *is*: `scanline`,
`bandPhase`, `edgeFalloff`.

- A renamed local must be renamed in its `#pragma layout(Node, Var = …)` line too, or that position is orphaned.
- **A `uniform`'s name is the parameter's name.** Renaming it renames the parameter, and every material instance
  that overrides it silently loses the override. Keep the name — or rename with `/// @name <the old name>` above
  it, which keeps the parameter.
- An `export` function's name is the asset's name: renaming it moves the asset unless `/// @name` keeps it.

### Prefer HLSL to class-exact node calls

`UE.Multiply(A = x, B = y)` is `x * y`; `UE.LinearInterpolate(A = a, B = b, Alpha = t)` is `lerp(a, b, t)`. Write
the HLSL where it builds the same node, and keep `UE.<Class>(…)` where the language has no spelling for it. Or
decompile again with `-Readable`, which does this throughout (the graph may gain `Constant` nodes; the shader does
not change).

### Factor repeated logic

Logic written out twice becomes a helper (`float3 Tint(float3 C, float K) { … }`, no linkage keyword). A helper is
inlined at each call, and two identical calls are one node, so the graph does not grow.

### Keep the layout

`#pragma layout(…)` lines are the node positions of the original graph and `#pragma region` its comment boxes.
They change no behaviour, and without them a large graph comes back auto-laid-out. Keep them.

### Leave the numbers alone

The 2.0 decompiler writes the shortest decimal that reads back as the same float, so `6.2831855` *is* the value
the asset holds. "Restoring" `6.2831853` changes nothing; rounding to `6.28` changes the material.

## A 1.x export, if the user insists on keeping it

The 1.x exporter (`-Format Legacy`) is a migration starting point, not a guarantee. What to know about its text:

- `Settings` always starts with `Domain`, `ShadingModel` and `BlendMode`, whether or not they were set. `Backend` is
  never written, so the file rebuilds on the project default — add `Backend = "Graph";` if the graph must stay a
  plain material.
- `ShadingModel = "Substrate"` is forced when a `Base.FrontMaterial` was decompiled. Do not "correct" it.
- Struct-, array-, map- and set-valued node properties are **dropped silently**. A `MaterialFunctionCall` with no
  function becomes `0.0`, and a graph cycle a default literal — both with a warning.
- Literals are rounded to six decimals (`6.2831853` → `6.283185`): restore recognisable constants.
- `DS_Shared_N` names are DreamShader's own reroute nodes, carried over from a graph DreamShader built.
- `ParameterName="…"` means the identifier had to differ from the parameter: renaming the identifier is safe,
  deleting `ParameterName` is not.
- It rebuilds into a second asset under `Decompiled/…` until `Shader(Name=…)` is retargeted.

The full table of what each decompiler reproduces:
[`Docs/tools/decompiler.md`](../../Docs/tools/decompiler.md#known-round-trip-gaps).

## Gotchas

- **Re-running the decompiler overwrites the cleaned file.** Move it out of `Decompiled/` once you own it.
- **`@sort` is the material instance's parameter order.** Reordering the `uniform` lines is free; changing a
  `@sort` value changes what an artist sees.
- **A material instance decompiles to a `.dsi` of overrides only** — there is little to optimize, and its
  `uniform` names must match the parent's exactly.
- **`dump-graph` boots the editor too.** Capture before, apply every pass, capture after — two runs, not one per
  pass.

## See also

- [`dream-shader-decompile`](../dream-shader-decompile/SKILL.md) — producing the input
- [`dream-shader-verify`](../dream-shader-verify/SKILL.md) — `check` and `compile`
- [`Docs/tools/commandlet.md`](../../Docs/tools/commandlet.md#dump-graph-since-190) — `dump-graph` and diffing two captures
