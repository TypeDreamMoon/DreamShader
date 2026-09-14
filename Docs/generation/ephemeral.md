# Ephemeral materials

> [DreamShader](../index.md) » [Generation](index.md) » **Ephemeral materials**

A **ThinCustom** product has two states, and exactly two:

| State | What it means | How you can tell |
| :-- | :-- | :-- |
| **Ephemeral** | no package on disk. The hidden base material lives in the transient package; the instance reports `IsAsset() == false` and hides itself. The default. | the package carries `PKG_NewlyCreated` |
| **Materialized** | a package on disk holds the instance, with the base as a subobject export of it | the package exists as a file |

Only two things move a product between them:

| Transition | Triggered by |
| :-- | :-- |
| **Materialize** | an explicit *Materialize* action, a cook, or creating a child material instance of the product |
| **Make Ephemeral** | *Tools ▸ DreamShader ▸ Make Ephemeral*, which deletes the package on disk |

And one rule decides everything else: **a product that has a package on disk stays on disk.** Storage
decides, never the compile that asked.

> [!IMPORTANT]
> **Only ThinCustom has these states.** A `Graph`-backend `UMaterial` and a `.dsf` `UMaterialFunction`
> are ordinary engine assets with no way to opt out of asset enumeration — there is no `IsAsset()` to
> lie with — so they have no Ephemeral form and nothing on this page applies to them. The 2.0 rule for
> those two is simply that **every successful generation saves them to disk**.
>
> *Not yet true of the binary.* The 1.x generator still honours a memory-only request for Graph
> materials and material functions; M4 removes that path along with the generator. Until then the
> interactive editor still builds those two in memory.

| | |
| :-- | :-- |
| Applies to | `Shader` blocks under the `ThinCustom` backend (the project default) |
| Emitted class | `UDreamShaderMaterialInstance` |
| Marker | the containing package still carries `PKG_NewlyCreated` |
| Since | `1.5.0` — the ThinCustom backend and the Content Browser visibility toggle; renamed from *in-memory* in `2.0.0` |

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
| `IsAsset()` | `false` while the package is `PKG_NewlyCreated` **and** *Show Ephemeral Materials* is off | Ephemeral materials are hidden from the Content Browser, asset pickers, and save pickers |

### The hidden base

| State | Base object name | Outer | Object flags |
| :-- | :-- | :-- | :-- |
| Ephemeral | `MB_DreamThinBase_<sanitized Name>` | the transient package | `RF_Public`, `RF_Standalone`, `RF_Transient` |
| Materialized | `MB_DreamThinBase_<instance leaf name>` | **the instance object itself** | `RF_Public`, `RF_Standalone` |

`<sanitized Name>` is the block's whole logical `Name` with every character outside `[A-Za-z0-9_]`
replaced by `_` and runs of underscores collapsed, so `Shader(Name="Mat/Test")` yields
`MB_DreamThinBase_Mat_Test`. Sanitization is not cosmetic: a `/` inside an `FName` reads as a
subobject separator, which would break base reuse and leak a fresh base on every regeneration.

When Materialized the base is a subobject of the instance, so it serializes **into the instance's own
package** as a plain export. One asset, one `.uasset`, no `MB_DreamThinBase_*` sibling in the Content
Browser, and no cross-package parent import to lose at cook. Because a non-package outer already
makes `IsAsset()` false, the base is invisible in both states.

> [!NOTE]
> An instance saved by a pre-1.5.0 build whose parent lives in a separate `MB_*` package is **not**
> reused. Regeneration creates a fresh subobject base and leaves the old sibling package orphaned.
> The orphan is harmless and can be deleted.

## Why nothing appears in the Content Browser

A ThinCustom product is Ephemeral by design: the `.dsm` file is the authoring surface, and a
generated `.uasset` on disk is a second copy of something the source already says. Every trigger
except cook, the commandlet, and an explicit *Materialize* leaves the product Ephemeral — unless the
asset already exists on disk, in which case it is maintained there instead. See
[Generation](index.md#what-triggers-a-compile) and
[When the asset already exists on disk](#when-the-asset-already-exists-on-disk).

While a product is Ephemeral:

- `IsAsset()` returns `false`, so it does not appear in the Content Browser or in any asset picker.
- Its package is marked non-dirty at the end of generation, so *Save All* and the exit prompt cannot
  silently persist it.
- The product is fully usable through the [Material Content Browser](../tools/material-browser.md),
  the [preview](../tools/preview.md), and by anything holding a live pointer.

### The visibility toggle

| | |
| :-- | :-- |
| Setting | **Show Ephemeral Materials** (`bShowEphemeralMaterials`), category `Compiler` |
| Default | off |
| Menu | *Tools ▸ DreamShader ▸ Show Ephemeral Materials* |
| Also on | the DreamShader **Project** page, as a checkbox |

Toggling writes the project config and immediately broadcasts asset-created / asset-deleted for
every live `UDreamShaderMaterialInstance` whose package is still `PKG_NewlyCreated`, so tiles appear
and disappear without a rescan. The confirmation reads:

```text
Showing {Count} Ephemeral material(s) in the Content Browser and asset pickers.
Hidden {Count} Ephemeral material(s) from the Content Browser and asset pickers.
```

> [!NOTE]
> The pre-2.0 config key was `bShowEphemeralMaterials`. A project that set it is
> migrated on load — the old key is read once and folded into the new one — so an existing
> `DefaultEngine.ini` keeps working and does not silently revert to the default.

> [!WARNING]
> While the toggle is on, an Ephemeral material is a normal-looking tile, and an explicit **Save**
> on it Materializes it. From then on that path is maintained on disk — see
> [When the asset already exists on disk](#when-the-asset-already-exists-on-disk). Use *Materialize*
> rather than *Save* when you actually want the file.

## Materializing to disk

*Materialize* re-runs generation for the material's own source file asking for the Materialized state
and forcing enabled, then reloads the object at its resolved path. It is one of the two transitions
named at the top of this page.

| Surface | Action |
| :-- | :-- |
| Material Content Browser, Gen page | the **Materialize** button — *"Write this memory-only material (and its base) to disk."* (the button's own wording) |
| Content Browser context menu | the DreamShader materialize action |
| Implicit | creating a child material instance of an Ephemeral parent Materializes the parent first |

Creating a child instance must materialize first because a transient base cannot be a parent import.
The default destination for the child is `<parent directory>/<Instance Subfolder>` — the
**Instance Subfolder** project setting, default `Instances`; when it is empty the child is created
beside the parent. The child is named `MI_<parent leaf>`, uniquified.

A product that is already Materialized is returned unchanged.

| Message | Cause |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | the object is not a `UDreamShaderMaterialInstance`, or its `SourceFilePath` is empty |
| `Failed to materialize the material to disk: {Message}` | the regeneration that materializes it failed |
| `Materialized the material but could not reload it at {ObjectPath}.` | generation succeeded but the object could not be loaded back |

### The `PKG_NewlyCreated` dance

When an Ephemeral ThinCustom product is Materialized, the `PKG_NewlyCreated` flag is cleared as the
very last step before the save. Unreal's `IsEmptyPackage()` counts only objects for which
`IsAsset()` is `true`, and while the flag is set the instance reports `false` — so a package saved
with the flag still on would be skipped as empty. If the save fails the flag is restored. On a
first Materialize the asset registry is notified directly, so the Content Browser updates without a
rescan.

## When the asset already exists on disk

**Storage decides how a rebuild persists, not the compile that asked for it.** A compile that lands on
a package which already exists on disk rebuilds the asset *and saves it*, rather than rebuilding it in
memory over the top of its own file. Generation succeeds and logs:

```text
'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run
Tools > DreamShader > Make Ephemeral to make it Ephemeral again.
```

| | |
| :-- | :-- |
| Applies to | every trigger, since every interactive compile leaves a ThinCustom product Ephemeral |
| Since | `1.8.0` — before it, such a compile rebuilt the asset in memory and then cleared the dirty flag |

> [!NOTE]
> What that older behaviour produced was an object matching neither the file on disk nor anything
> that would ever be written, and reporting itself clean — so a Save All could persist a state nobody
> chose, and the version you saw in the editor vanished on restart. There is now exactly one answer
> to "what is this asset": whatever the last compile produced, wherever the asset lives.

A consequence worth knowing: **the startup sweep no longer forces**. Forcing was free while every
Ephemeral product regenerated regardless of its source hash; it stopped being free once a disk-backed
asset started being saved, because every launch would then rewrite every persisted generated asset.
Startup now respects the [source-hash skip](caching.md), so an up-to-date saved asset is left alone.
Changing the **Default Compiler Backend** still forces, because the hash cannot see that setting.

Two tools address the persisted assets themselves:

| Command | Effect |
| :-- | :-- |
| *Tools ▸ DreamShader ▸ Make Ephemeral* | deletes the packages of **Materialized ThinCustom products** that carry DreamShader provenance metadata, with a confirmation listing every one, then rebuilds them Ephemeral. Hand-authored assets are never touched, and Graph materials and material functions are not listed — they have no Ephemeral state to return to. Empty case: `No Materialized DreamShader ThinCustom products found.` |
| *Tools ▸ DreamShader ▸ Clean Generated Shaders* | deletes every `*.ush` under the generated-shader directory and queues a full recompile — see [Generated HLSL](generated-hlsl.md) |

Changing the **Default Compiler Backend** setting regenerates every source and then warns if any
saved generated assets remain:

```text
{Count} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials.
Run Tools > DreamShader > Make Ephemeral to remove them.
```

(Those assets are not *shadowing* anything any more — they are simply still on disk, and are rebuilt
there. The notification points at *Make Ephemeral* for anyone who wants the Ephemeral end state.)

*Make Ephemeral* scans `/Game` recursively for `UDreamShaderMaterialInstance` assets whose package
exists on disk and whose metadata carries a non-empty `DreamShader.SourceFile`.

## Cook behaviour

| Aspect | Behaviour |
| :-- | :-- |
| Detection | the process is a cook when the `-run=` value contains `Cook` |
| Who generates | **the cook director only** — a process launched with `-cookworker` skips generation and loads what the director saved |
| When | on post-engine-init, after engine subsystems exist but before the commandlet's `Main` |
| What | every project DreamShader source file except `.dsh`, generated forced and Materialized |
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
through `IAssetRegistry::DoesPackageExistOnDisk`, which reads only the registry's cached state and
has no filesystem fallback, and post-engine-init is *after* the registry enumerated the content
directories — so a freshly generated package is invisible to that lookup, and even an explicit
`DirectoriesToAlwaysCook` entry covering its folder drops it without an error. The generation
therefore records what its saves wrote (via `UPackage::PackageSavedWithContextEvent`) and rescans
those files into the registry, which happens before the commandlet's `Main` collects the initial cook
requests. Assets that reach the cook some other way — a hard `UPROPERTY` reference from a class whose
CDO is loaded at startup, for instance — were never affected by this.

## Notes

- Ephemeral versus Materialized is decided by one thing: whether the package still carries
  `PKG_NewlyCreated`. Nothing else distinguishes the two states.
- **Layout always runs.** Graph placement used to be skipped for memory-only materials unless *Lay
  Out In-Memory Graphs* was on; that setting is gone in `2.0.0` and layout runs for every generated
  graph. The large-graph performance guard still applies. See
  [Graph layout](graph-layout.md#when-layout-runs).
- Provenance metadata — source path **and** hash — is stamped on every generated asset in **both**
  states; the hidden ThinCustom base additionally when Materialized. See
  [Caching](caching.md#where-the-metadata-lives).
- A generated instance is deliberately **not** `RF_Transactional`: material instances do not support
  undo/redo without desynchronizing the shader map.
- Nothing here changes the source-hash short circuit; a compile skips work when the hash is
  unchanged, whichever backend produced the asset. *Recompile DSM* and *Clean Generated
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

Saving that file in the editor produces an Ephemeral product:

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
- [Regeneration](regeneration.md) — what a rebuild destroys, and what it puts back
- [Graph layout](graph-layout.md) — node placement, and the large-graph guard
- [Material Content Browser](../tools/material-browser.md) — Compile, Materialize, thumbnails
- [Commandlet](../tools/commandlet.md) — Materializing assets headlessly
- [Generation](index.md) — the full pipeline
