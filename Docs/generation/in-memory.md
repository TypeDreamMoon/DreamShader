# In-memory materials

> [DreamShader](../index.md) » [Generation](index.md) » **In-memory materials**

Materials that exist only as live `UObject`s in the editor process, with no `.uasset` on disk — the
default result of every interactive compile.

| | |
| :-- | :-- |
| Applies to | `Shader` blocks under the `ThinCustom` backend (the project default) and under the `Graph` backend |
| Emitted class | `UDreamShaderMaterialInstance` (ThinCustom) or `UMaterial` (Graph) |
| Marker | the containing package still carries `PKG_NewlyCreated` |
| Since | `1.5.0` — the ThinCustom backend and the Content Browser visibility toggle |

## What a DreamShader material is

Under the default backend a `Shader` block does **not** produce a `UMaterial` asset. It produces:

```text
UDreamShaderMaterialInstance          "M_Emissive"            <- the asset you reference
  └─ UMaterial (subobject, hidden)    "MB_DreamThinBase_M_Emissive"
       └─ the generated node graph
```

The node graph — every `Properties` node, every `Graph` statement, every `Outputs` binding — is
built on the hidden base. The instance is a thin wrapper that carries the parameter values, the
provenance metadata, and the compiled shader map.

| Member | Visibility | Meaning |
| :-- | :-- | :-- |
| `SourceFilePath` | read-only in the details panel, category `DreamShader` | the `.dsm` this instance was generated from |
| `SourceHash` | read-only in the details panel, category `DreamShader` | the source hash — see [Caching](caching.md) |
| `Parent` | standard | the hidden base material |

Two overrides give the class its behaviour:

| Override | Result | Consequence |
| :-- | :-- | :-- |
| `HasOverridenBaseProperties()` | `true` exactly when the parent is a `UMaterial` | the **root** instance owns its own static permutation and shader map; a child `UMaterialInstanceConstant` parented to it falls through to stock behaviour and **shares** that shader map, so many colour/parameter variants cost one compile |
| `IsAsset()` | `false` while the package is `PKG_NewlyCreated` **and** *Show In-Memory Materials In Content Browser* is off | memory-only materials are hidden from the Content Browser, asset pickers, and save pickers |

### The hidden base

| Mode | Base object name | Outer | Object flags |
| :-- | :-- | :-- | :-- |
| memory-only | `MB_DreamThinBase_<sanitized Name>` | the transient package | `RF_Public`, `RF_Standalone`, `RF_Transient` |
| persisted | `MB_DreamThinBase_<instance leaf name>` | **the instance object itself** | `RF_Public`, `RF_Standalone` |

`<sanitized Name>` is the block's whole logical `Name` with every character outside `[A-Za-z0-9_]`
replaced by `_` and runs of underscores collapsed, so `Shader(Name="Mat/Test")` yields
`MB_DreamThinBase_Mat_Test`. Sanitization is not cosmetic: a `/` inside an `FName` reads as a
subobject separator, which would break base reuse and leak a fresh base on every regeneration.

In persist mode the base is a subobject of the instance, so it serializes **into the instance's own
package** as a plain export. One asset, one `.uasset`, no `MB_DreamThinBase_*` sibling in the Content
Browser, and no cross-package parent import to lose at cook. Because a non-package outer already
makes `IsAsset()` false, the base is invisible in both modes.

> [!NOTE]
> An instance saved by a pre-1.5.0 build whose parent lives in a separate `MB_*` package is **not**
> reused. Regeneration creates a fresh subobject base and leaves the old sibling package orphaned.
> The orphan is harmless and can be deleted.

## Why nothing appears in the Content Browser

Interactive compiles are memory-only by design: the `.dsm` file is the authoring surface, and a
generated `.uasset` on disk would shadow it. Every trigger except cook, the commandlet, and an
explicit *Materialize* generates in memory. See [Generation](index.md#what-triggers-a-compile).

While a material is memory-only:

- `IsAsset()` returns `false`, so it does not appear in the Content Browser or in any asset picker.
- Its package is marked non-dirty at the end of generation, so *Save All* and the exit prompt cannot
  silently persist it.
- The material is fully usable through the [Material Content Browser](../tools/material-browser.md),
  the [preview](../tools/preview.md), and by anything holding a live pointer.

### The visibility toggle

| | |
| :-- | :-- |
| Setting | **Show In-Memory Materials In Content Browser**, category `Compiler` |
| Default | off |
| Menu | *Tools ▸ DreamShader ▸ Show In-Memory Materials* |
| Also on | the DreamShader **Project** page, as a checkbox |

Toggling writes the project config and immediately broadcasts asset-created / asset-deleted for
every live `UDreamShaderMaterialInstance` whose package is still `PKG_NewlyCreated`, so tiles appear
and disappear without a rescan. The confirmation reads:

```text
Showing {Count} in-memory material(s) in the Content Browser and asset pickers.
Hidden {Count} in-memory material(s) from the Content Browser and asset pickers.
```

> [!WARNING]
> While the toggle is on, a memory-only material is a normal-looking tile, and an explicit **Save**
> on it writes a real `.uasset` to disk. That saved asset then shadows in-memory regeneration for
> that path — see [Shadowed by a saved asset](#shadowed-by-a-saved-asset). Use *Materialize* rather
> than *Save* when you actually want the file.

## Materializing to disk

*Materialize* re-runs generation for the material's own source file with persistence on and forcing
enabled, then reloads the object at its resolved path.

| Surface | Action |
| :-- | :-- |
| Material Content Browser, Gen page | the **Materialize** button — *"Write this memory-only material (and its base) to disk."* |
| Content Browser context menu | the DreamShader materialize action |
| Implicit | creating a child material instance of a memory-only parent materializes the parent first |

Creating a child instance must materialize first because a transient base cannot be a parent import.
The default destination for the child is `<parent directory>/<Instance Subfolder>` — the
**Instance Subfolder** project setting, default `Instances`; when it is empty the child is created
beside the parent. The child is named `MI_<parent leaf>`, uniquified.

A material that is already persisted is returned unchanged.

| Message | Cause |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | the object is not a `UDreamShaderMaterialInstance`, or its `SourceFilePath` is empty |
| `Failed to materialize the material to disk: {Message}` | the regeneration that materializes it failed |
| `Materialized the material but could not reload it at {ObjectPath}.` | generation succeeded but the object could not be loaded back |

### The `PKG_NewlyCreated` dance

When a memory-only ThinCustom material is persisted, the `PKG_NewlyCreated` flag is cleared as the
very last step before the save. Unreal's `IsEmptyPackage()` counts only objects for which
`IsAsset()` is `true`, and while the flag is set the instance reports `false` — so a package saved
with the flag still on would be skipped as empty. If the save fails the flag is restored. On a
first-time persist the asset registry is notified directly, so the Content Browser updates without a
rescan.

## Shadowed by a saved asset

If a memory-only compile targets a package path that already exists on disk, generation still
succeeds but logs a warning:

```text
In-memory material mode: '{PackageName}' already exists as a saved asset, which shadows in-memory
regeneration. Delete the saved asset to make it fully in-memory.
```

Two tools address this:

| Command | Effect |
| :-- | :-- |
| *Tools ▸ DreamShader ▸ Clean Persisted Generated Assets* | deletes saved assets that carry DreamShader provenance metadata, with a confirmation listing every one. Hand-authored assets are never touched. Empty case: `No persisted DreamShader-generated assets found.` |
| *Tools ▸ DreamShader ▸ Clean Generated Shaders* | deletes every `*.ush` under the generated-shader directory and queues a full recompile — see [Generated HLSL](generated-hlsl.md) |

Changing the **Default Compiler Backend** setting regenerates everything in memory and then warns if
any saved generated assets remain:

```text
{Count} previously generated asset(s) are still saved on disk and shadow the in-memory materials.
Run Tools > DreamShader > Clean Persisted Generated Assets to remove them.
```

The cleaner scans `/Game` recursively for `UMaterial`, `UMaterialFunction` and
`UDreamShaderMaterialInstance` assets whose package exists on disk and whose metadata carries a
non-empty `DreamShader.SourceFile`.

## Cook behaviour

| Aspect | Behaviour |
| :-- | :-- |
| Detection | the process is a cook when the `-run=` value contains `Cook` |
| Who generates | **the cook director only** — a process launched with `-cookworker` skips generation and loads what the director saved |
| When | on post-engine-init, after engine subsystems exist but before the commandlet's `Main` |
| What | every project DreamShader source file except `.dsh`, generated forced and persisted |
| On failure | **the cook aborts** |

Cook log lines:

```text
DreamShader cook: generating {Count} source file(s) as persistent assets...
  [Cook] {Message}
  [Cook] Failed: {Message}
DreamShader cook asset generation complete.
```

> [!WARNING]
> A single generation failure ends the cook with a fatal log entry:
> `DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook]
> Failed entries above.` Compile every source cleanly in the editor, or run the
> [commandlet](../tools/commandlet.md), before cooking.

Generation runs on post-engine-init rather than at module startup because the material editing
library it depends on needs editor subsystems that do not exist during module load. Restricting it
to the director avoids every worker racing to save the same packages.

## Notes

- `IsMemoryOnly` is decided by one thing: the package still carries `PKG_NewlyCreated`. Nothing else
  distinguishes an in-memory material from a persisted one.
- Memory-only materials keep whatever node positions the construction pass produced — automatic
  layout is skipped in memory-only mode. See [Graph layout](graph-layout.md#when-layout-runs).
- Provenance metadata is stamped on a ThinCustom instance in **both** modes, and additionally on the
  base in persist mode. `Graph`-backend materials and material functions are stamped only when
  persisted. See [Caching](caching.md#where-the-metadata-lives).
- A generated instance is deliberately **not** `RF_Transactional`: material instances do not support
  undo/redo without desynchronizing the shader map.
- Nothing here changes the source-hash short circuit; a memory-only compile still skips work when the
  hash is unchanged.

## Example

```c
Shader(Name="Materials/M_Emissive")
{
    Properties { ScalarParameter Intensity = 2.0 [Slider(0, 10)]; }
    Settings   { Backend = "ThinCustom"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = vec3(1.0, 0.4, 0.1) * Intensity; }
}
```

Saving that file in the editor produces, in memory only:

```text
package        /Game/Materials/M_Emissive          (PKG_NewlyCreated, not dirty, not on disk)
  object       M_Emissive                          UDreamShaderMaterialInstance
    subobject  MB_DreamThinBase_Materials_M_Emissive   UMaterial, holds the node graph
```

After *Materialize*:

```text
on disk        <Project>/Content/Materials/M_Emissive.uasset
  export       M_Emissive                          UDreamShaderMaterialInstance
  export       MB_DreamThinBase_M_Emissive         UMaterial (hidden, same package)
```

## See also

- [Backend](../settings/backend.md) — `Graph` vs `ThinCustom`, and the deprecated `Instance` alias
- [Project settings](../settings/project.md) — default backend, visibility toggle, instance subfolder
- [`UDreamShaderMaterialInstance`](../api/material-instance.md) — the C++ class reference
- [Asset paths](asset-paths.md) — where the `.uasset` lands when it is written
- [Caching](caching.md) — the provenance metadata and the regeneration short circuit
- [Regeneration](regeneration.md) — parameter overrides on a generated instance do not survive
- [Graph layout](graph-layout.md) — why in-memory graphs are not laid out
- [Material Content Browser](../tools/material-browser.md) — Compile, Materialize, thumbnails
- [Commandlet](../tools/commandlet.md) — persisting assets headlessly
- [Generation](index.md) — the full pipeline
