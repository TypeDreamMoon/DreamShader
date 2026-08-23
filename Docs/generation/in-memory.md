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

Interactive compiles ask for memory-only by design: the `.dsm` file is the authoring surface, and a
generated `.uasset` on disk is a second copy of something the source already says. Every trigger
except cook, the commandlet, and an explicit *Materialize* asks for memory — and gets it, unless the
asset already exists on disk, in which case it is maintained there instead. See
[Generation](index.md#what-triggers-a-compile) and
[When the asset already exists on disk](#when-the-asset-already-exists-on-disk).

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
> on it writes a real `.uasset` to disk. From then on that path is maintained on disk — see
> [When the asset already exists on disk](#when-the-asset-already-exists-on-disk). Use *Materialize*
> rather than *Save* when you actually want the file.

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

## When the asset already exists on disk

**Storage decides how a rebuild persists, not the compile that asked for it.** A memory-only compile
that lands on a package which already exists on disk rebuilds the asset *and saves it*, rather than
rebuilding it in memory over the top of its own file. Generation succeeds and logs:

```text
'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run
Tools > DreamShader > Clean Persisted Generated Assets to make it memory-only.
```

| | |
| :-- | :-- |
| Applies to | every trigger, since every interactive compile asks for memory-only |
| Since | `1.8.0` — before it, such a compile rebuilt the asset in memory and then cleared the dirty flag |

> [!NOTE]
> What that older behaviour produced was an object matching neither the file on disk nor anything
> that would ever be written, and reporting itself clean — so a Save All could persist a state nobody
> chose, and the version you saw in the editor vanished on restart. There is now exactly one answer
> to "what is this asset": whatever the last compile produced, wherever the asset lives.

A consequence worth knowing: **the startup sweep no longer forces**. Forcing was free while every
in-memory asset regenerated regardless of its source hash; it stopped being free once a disk-backed
asset started being saved, because every launch would then rewrite every persisted generated asset.
Startup now respects the [source-hash skip](caching.md), so an up-to-date saved asset is left alone.
Changing the **Default Compiler Backend** still forces, because the hash cannot see that setting.

Two tools address the persisted assets themselves:

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

(Those assets are not *shadowing* anything any more — they are simply still on disk, and are rebuilt
there. The notification points at the cleaner for anyone who wants the memory-only end state.)

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
| Registry | every package the generation actually wrote is handed to `IAssetRegistry::ScanModifiedAssetFiles` before the commandlet's `Main` |
| On failure | **the cook aborts** |

Cook log lines:

```text
DreamShader cook: generating {Count} source file(s) as persistent assets...
  [Cook] {Message}
  [Cook] Failed: {Message}
DreamShader cook: registered {Count} generated package(s) with the AssetRegistry.
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

Writing the `.uasset` is not by itself enough to get it into the package. A cook request is resolved
through `IAssetRegistry::DoesPackageExistOnDisk`, which reads only the registry's in-memory state and
has no filesystem fallback, and post-engine-init is *after* the registry enumerated the content
directories — so a freshly generated package is invisible to that lookup, and even an explicit
`DirectoriesToAlwaysCook` entry covering its folder drops it without an error. The generation
therefore records what its saves wrote (via `UPackage::PackageSavedWithContextEvent`) and rescans
those files into the registry, which happens before the commandlet's `Main` collects the initial cook
requests. Assets that reach the cook some other way — a hard `UPROPERTY` reference from a class whose
CDO is loaded at startup, for instance — were never affected by this.

## Notes

- `IsMemoryOnly` is decided by one thing: the package still carries `PKG_NewlyCreated`. Nothing else
  distinguishes an in-memory material from a persisted one.
- Memory-only materials keep whatever node positions the construction pass produced — automatic
  layout is skipped in memory-only mode. See [Graph layout](graph-layout.md#when-layout-runs).
- Provenance metadata — source path **and** hash — is stamped on every generated asset in **both**
  modes; the hidden ThinCustom base additionally in persist mode. See
  [Caching](caching.md#where-the-metadata-lives).
- A generated instance is deliberately **not** `RF_Transactional`: material instances do not support
  undo/redo without desynchronizing the shader map.
- Nothing here changes the source-hash short circuit; a memory-only compile skips work when the
  hash is unchanged, whichever backend produced the asset. *Recompile DSM* and *Clean Generated
  Shaders* force past it, so a cleaned shader directory is always refilled.

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
