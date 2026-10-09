# Editor integration

> [DreamShader](../index.md) » [Tools](index.md) » **Editor integration**

Every place DreamShader attaches itself to the Unreal editor UI: the Tools menu, the Level Editor
toolbar, the Content Browser asset context menus, the Material Editor toolbar, the Window menu, the
details panel of a Custom Pass pipeline, and the notifications it raises on its own.

| | |
| :-- | :-- |
| Registered by | `DreamShaderEditor` at module startup, through `UToolMenus::RegisterStartupCallback` |
| ToolMenu owners | `DreamShaderEditor` (bridge entries) · `DreamShaderMaterialBrowser` (browser entries) |
| Suppressed by | `-NoDreamShaderEditorBridge`, and by every commandlet run |

Menu registration is idempotent — a second registration pass adds nothing — and is skipped while the
editor is shutting down.

## Tools menu

*Tools ▸ DreamShader*, extending `LevelEditor.MainMenu.Tools`, section `DreamShader`.

| Entry name | Label | Tooltip | Icon | Effect | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.RecompileAll` | **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first." | `Icons.Refresh` | After a Yes/No confirmation, rebuilds the dependency graph and queues every source of every root — `.dsm`, `.dsf`, `.dss`, `.dsi` and `.dsp`, despite the label — **forced** past the build-key skip | [below](#recompile-dsm) |
| `DreamShader.CleanGeneratedShaders` | **Clean Generated Shaders** | "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile." | `Icons.Delete` | Deletes every `*.ush` under the generated-shader directory, then queues a full **forced** scan so every file is regenerated | [below](#clean-generated-shaders) |
| `DreamShader.MakeEphemeral` | **Make Ephemeral** | "Delete the packages of Materialized ThinCustom products so they go back to being Ephemeral. Shows a confirmation with the full list; source files are untouched and the products are rebuilt in memory. Graph materials and material functions are not listed -- they have no Ephemeral state." | `Icons.Delete` | Deletes the on-disk packages of ThinCustom products carrying DreamShader provenance metadata, through the standard editor delete flow | [below](#make-ephemeral) |
| `DreamShader.ToggleShowEphemeralMaterials` | **Show Ephemeral Materials** | "Show Ephemeral ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. Graph-backend materials are plain UMaterials with no Ephemeral state, so this toggle does not affect them. While shown, an explicit Save on one would materialize it to disk (the shadow warning and Make Ephemeral cover recovery)." | *(none)* | Toggle button. Flips `bShowEphemeralMaterials` and writes it to `DefaultEngine.ini` | [below](#show-ephemeral-materials) |
| `DreamShader.OpenWorkspace` | **Open Dream Shader Workspace (VSCode)** *(since 1.2.1)* | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | Re-exports the three bridge manifests, rewrites `DShader/DreamShader.code-workspace`, then launches it | [Workspace](workspace.md) |
| `OpenMaterialContentBrowser` | **Material Content Browser** *(since 1.5.0)* | "Open the DreamShader Material Content Browser." | `ClassIcon.Material` | Invokes the `DreamShaderMaterialBrowser` nomad tab | [Material Content Browser](material-browser.md) |

> [!NOTE]
> The first five entries and the sixth are registered by two different startup callbacks into the
> same `DreamShader` section. Their relative order within the section is registration-order
> dependent and is not guaranteed between editor runs.

## Level Editor toolbar

One **Dream** combo button, shared by the Dream-family plugins (DreamShader, DreamFX, DreamGUI): each
plugin adds it if it is not there yet, so it appears once whichever of them loads first, and belongs to
the owner `DreamToolsShared`, which no plugin unregisters.

| Aspect | Value |
| :-- | :-- |
| Extends | `LevelEditor.LevelEditorToolBar.AssetsToolBar`, section `DreamTools` |
| Entry | `DreamTools.OpenWorkspaceCombo`, label **Dream**, icon `Icons.OpenInExternalEditor`, tooltip "Dream-family language tools: open a source workspace in VSCode, or rebuild a whole source tree (DreamShader / DreamFX / DreamUI)." |
| Opens | the shared menu `DreamTools.Actions`, where each plugin adds a section of its own |

DreamShader's section there, titled **DreamShader**:

| Entry name | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.OpenWorkspaceShared` | **DreamShader Workspace** | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | Identical to *Open Dream Shader Workspace (VSCode)* |
| `DreamShader.RecompileAllShared` | **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first." | `Icons.Refresh` | Identical to *Recompile DSM*, confirmation included |

## Window menu

The Material Content Browser registers a nomad tab, which also lists it under *Window ▸ Tools*.

| Aspect | Value |
| :-- | :-- |
| Tab id | `DreamShaderMaterialBrowser` |
| Display name | **Material Content Browser** |
| Tooltip | "Browse, manage, and create instances of project and DreamShader-generated materials." |
| Icon | `ClassIcon.Material` |
| Workspace group | Tools category |
| Tab role | `ETabRole::NomadTab` |

## Content Browser context menus

All entries are added into the stock `GetAssetActions` section of the per-class asset context menu.

> [!WARNING]
> **Every DreamShader context-menu entry requires exactly one selected asset.** With two or more
> assets selected the section is empty and nothing indicates why. Right-click a single asset.

| Asset class extended | Dynamic entry name | What appears |
| :-- | :-- | :-- |
| `UMaterial` | `DreamShader.MaterialAssetActions` | submenu **DreamShader** — [Material submenu](#material-submenu) |
| `UMaterialFunction` | `DreamShader.VirtualFunctionAssetActions` | submenu **DreamShader** — [Material Function submenu](#material-function-submenu) |
| `UMaterialFunctionMaterialLayer` | `DreamShader.MaterialLayerAssetActions` | the same Material Function submenu |
| `UMaterialFunctionMaterialLayerBlend` | `DreamShader.MaterialLayerBlendAssetActions` | the same Material Function submenu |
| `UDreamShaderMaterialInstance` | `DreamShader.MaterialInstanceAssetActions` | submenu **DreamShader** — [Material instance submenus](#material-instance-submenus) |
| `UMaterialInstanceConstant` | `DreamShader.MaterialInstanceConstantAssetActions` | submenu **DreamShader** — [Material instance submenus](#material-instance-submenus); not for a `UDreamShaderMaterialInstance` |
| `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | `DreamShader.InstanceCreateActions` | flat entry **Create DreamShader instance** |
| `UMaterial` · `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | `DreamShader.ShowInBrowserActions` | flat entry **Show in Material Content Browser** |

Menu names in Unreal are keyed on the exact class, so an entry meant for both instance classes is
registered twice — once for the stock class and once for the DreamShader subclass.

A `UDreamPassPipeline` *(since 2.1.0)* gets no DreamShader entry here. Its *Open Source*, *Revert to
Source* and *Adopt Into Source* are on its [details panel](#pass-pipeline-details-panel), its source is in
the [Material Content Browser](material-browser.md#pipelines), and it decompiles with
[`dsc decompile`](decompiler.md#pipelines-dsp).

| Entry | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.CreateInstance` | **Create DreamShader instance** *(since 1.5.0)* | "Create a material instance of this material: a .dsi source file compiled into the instance when DreamShader generated the material, an ordinary instance asset otherwise." | `ClassIcon.MaterialInstanceConstant` | Opens the [Create material instance](material-browser.md#create-material-instance) dialog. Requires the selection to cast to `UMaterialInterface` |
| `DreamShader.ShowInMaterialBrowser` | **Show in Material Content Browser** | "Open the DreamShader Material Content Browser on this asset: its source, compile status, provenance and inheritance." | `ClassIcon.Material` | Opens (or fronts) the [Material Content Browser](material-browser.md) in Assets mode, scoped to the asset's mount point, with the asset selected |

### Material submenu

`DreamShader.MaterialActions` — label **DreamShader**, tooltip "DreamShader actions for this
Material.", icon `Icons.Settings`. Built only when the single selected asset is a `UMaterial`.

Section 1, `DreamShader.ProvenanceActions`, titled **Generated Asset** — the
[Generated Asset section](#generated-asset-section) below; empty for an asset DreamShader did not
generate.

Section 2, `DreamShader.DecompileActions`, titled **Decompiler**:

| Entry | Label | Tooltip | Icon | Effect | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportMaterialDSS` | **Export .dss** *(since 2.0.0)* | "Decompile this Material graph into a 2.0 DreamShader .dss source file." | `Icons.Save` | Decompiles to `<SourceDirectory>/Decompiled/Materials/…​.dss` and opens the file | [Decompiler](decompiler.md) |
| `DreamShader.ExportMaterialLegacyDSM` | **Export Legacy .dsm** *(since 2.0.0; **Export DSM** since 1.3.5)* | "Decompile this Material graph into a 1.x .dsm source file, with the 1.x decompiler that is kept through 2.0.x." | `Icons.Save` | Decompiles to `<SourceDirectory>/Decompiled/Materials/…​.dsm` and opens the file | [Decompiler](decompiler.md) |

### Material Function submenu

`DreamShader.MaterialFunctionActions` — label **DreamShader**, tooltip "DreamShader actions for this
Material Function.", icon `Icons.Settings`. The same submenu serves a material layer and a layer blend.

Section 1, `DreamShader.ProvenanceActions`, titled **Generated Asset** — the
[Generated Asset section](#generated-asset-section) below.

Section 2, `DreamShader.DecompileActions`, titled **Decompiler**:

| Entry | Label | Tooltip | Icon | Effect | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportFunctionDSS` | **Export .dss** *(since 2.0.0)* | "Decompile this Material Function graph into a 2.0 DreamShader .dss source file." | `Icons.Save` | Decompiles to `<SourceDirectory>/Decompiled/{Functions,Layers,LayerBlends}/…​.dss` and opens the file | [Decompiler](decompiler.md) |
| `DreamShader.ExportFunctionLegacyDSF` | **Export Legacy .dsf** *(since 2.0.0; **Export DSF** since 1.3.5)* | "Decompile this Material Function graph into a 1.x .dsf source file, with the 1.x decompiler that is kept through 2.0.x." | `Icons.Save` | Decompiles to `<SourceDirectory>/Decompiled/{Functions,Layers,LayerBlends}/…​.dsf` and opens the file | [Decompiler](decompiler.md) |

Section 3, `DreamShader.VirtualFunctionActions`, titled **VirtualFunction** *(since 1.2.1)*. Its
contents depend on whether a `VirtualFunction` declaration already references this asset — the full
table is on [VirtualFunction tools](virtual-function-tools.md#context-menu).

| Declaration exists? | Entries |
| :-- | :-- |
| yes | **OpenVirtualFunction**, **Copy Virtual Function Reference** |
| no | **CopyVirtualFunction**, **CreateVirtualFunction**, **Copy extern Prototype (2.0)**, **Create extern Prototype (2.0)**, **CopyVirtualFunctionCall** |

The two *extern Prototype* entries *(since 2.0.0)* are the `.dss` counterpart of a `VirtualFunction`
block: the function's `extern` prototype with its `/// @asset` line, copied to the clipboard or written
to a new `.dsh` for `.dss` sources to `#include`.

### Material instance submenus

A `UDreamShaderMaterialInstance` — the product of a ThinCustom `Shader` — gets submenu
`DreamShader.MaterialInstanceActions`, label **DreamShader**, tooltip "DreamShader actions for this
generated material.", with the [Generated Asset section](#generated-asset-section) alone.

Any other `UMaterialInstanceConstant` — what a [`.dsi`](../language-v2/instances.md) builds, or a
hand-made instance — gets submenu `DreamShader.MaterialInstanceConstantActions`, label
**DreamShader**, tooltip "DreamShader actions for this material instance.", icon `Icons.Settings`:

| Section | Entry | Label | Effect |
| :-- | :-- | :-- | :-- |
| **Generated Asset** | | | the [Generated Asset section](#generated-asset-section); empty for a hand-made instance |
| **Decompiler** | `DreamShader.ExportMaterialInstanceDSI` | **Export .dsi** *(since 2.0.0)* | Decompiles the instance — its parent, its instance settings and every parameter that differs from the parent — to `<SourceDirectory>/Decompiled/Instances/…​.dsi` and opens the file |

### Generated Asset section

Section `DreamShader.ProvenanceActions` of every submenu above. It is empty unless the asset carries
DreamShader's source metadata, so a hand-authored asset shows none of it.

| Entry | Label | Effect | Reference |
| :-- | :-- | :-- | :-- |
| `DreamShader.RevertToSource` | **Revert to Source**, or **Revert to Source (discards your edits)** when the asset is `Diverged` | Rebuilds the asset from its source, discarding every hand edit; the source is not modified | [Divergence](../generation/divergence.md#revert-to-source) |
| `DreamShader.AdoptIntoSource` | **Adopt Into Source** | Rewrites the source from the asset's current contents; the previous source is backed up beside it | [Divergence](../generation/divergence.md#adopt-into-source) |
| `DreamShader.DetachFromDreamShader` | **Detach From DreamShader** | Keeps the asset as it is and stops DreamShader from rebuilding it | [Divergence](../generation/divergence.md#detach-from-dreamshader) |
| `DreamShader.AdoptTweaksIntoSourceDefaults` | **Adopt Tweaks as Source Defaults** *(since 2.0.0)* | Only on a `Tweaked` ThinCustom instance. Writes its parameter overrides into the `.dss` as the defaults of its `uniform`s, then clears them from the instance; the source is backed up first. Refuses scalar values that the declared `bool`/`int`/`uint` type cannot represent (`DSH9109`); change the source type explicitly or extract a `.dsi`. Disabled, with the reason in its tooltip, unless the source is a `.dss` under a writable root | [Divergence](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance) |
| `DreamShader.ExtractTweaksToInstanceSource` | **Extract Tweaks to .dsi** *(since 2.0.0)* | Only on a `Tweaked` ThinCustom instance. Writes its parameter overrides into a new `.dsi` whose parent is this material, compiles it, and clears the overrides from the instance | [Material instances](../language-v2/instances.md) |

## Material Editor toolbar

`DreamShader.MaterialEditorToolbarActions`, a dynamic entry in section `DreamShader` of
`AssetEditor.MaterialEditor.ToolBar` *(since 1.2.1)*.

| Step | Behaviour |
| :-- | :-- |
| 1 | Requires a valid `UMaterialEditorMenuContext` with a live `IMaterialEditor` |
| 2 | Scans the objects currently being edited and stops at the **first** `UMaterial`, or failing that the first `UMaterialFunction` |
| 3 | For a `UMaterial`: combo button `DreamShader.MaterialToolbarMenu`, label **DreamShader**, tooltip "DreamShader actions for this Material.", icon `Icons.Settings`, content = the [Material submenu](#material-submenu) |
| 4 | For a `UMaterialFunction`: combo button `DreamShader.MaterialFunctionToolbarMenu`, label **DreamShader**, tooltip "DreamShader actions for this Material Function.", icon `Icons.Settings`, content = the [Material Function submenu](#material-function-submenu) |
| 5 | Neither found ⇒ nothing is added to the toolbar |

## Pass pipeline details panel

*(since 2.1.0)* The details panel of a `UDreamPassPipeline` — the asset a [`.dsp`](../language-v2/passes.md)
compiles to — with three categories above the pipeline's own properties that read the asset the way a
frame runs it. The properties stay, and stay editable: the panel is where a pipeline is tuned, and
*Adopt Into Source* writes the tuning back into its `.dsp`.

| | |
| :-- | :-- |
| Registered by | `DreamShaderEditor` at module startup: a detail customization of the class `DreamPassPipeline`, through the PropertyEditor module |
| Present | in every editor process, **with or without** `-NoDreamShaderEditorBridge`, because it reads the asset alone; never in a commandlet |
| Shown | for one selected pipeline: opening the asset — a double-click, or *Open* in the [Material Content Browser](material-browser.md#pipelines) — or any details view of it. Several selected keep the default layout, whose rows edit them all |
| Engines | every engine the asset types build on. Below UE 5.8 the overview has one more row: *Custom Pass runs on Unreal Engine 5.8 and later. This engine loads and saves the pipeline, and runs none of it.* |

### Pipeline Overview

| Row | Shows |
| :-- | :-- |
| **Source** | the `.dsp` the pipeline was built from, absolute, resolved from its `DreamShader.SourceFile` stamp (*not built from a .dsp* when it has none), and under it the pipeline's [provenance](../generation/divergence.md#states), worked out again after every change. Three buttons: **Open Source** opens the `.dsp` in your preferred editor; **Revert to Source** rebuilds the pipeline from it, discarding every edit made here, and leaves the file alone; **Adopt Into Source** writes the edits back into the `.dsp`, value by value — see [Divergence](../generation/divergence.md#a-custom-pass-pipeline). Adopt is disabled, with the reason in its tooltip, when the source ships with a plugin or is not found |
| **Runs** | `Order {N}, in views {Views}, requiring {Requires}: {N} pass(es), {N} buffer(s), {N} parameter(s).` |
| **Problems** | only while `UDreamPassPipeline::Validate` fails — after a hand edit that breaks a rule the runtime relies on — one line per problem. The runtime skips every pass that fails, with a warning |

### Passes in Frame Order

One group per injection point the pipeline uses, in the order a frame reaches them, titled
`<injection point> (<number of passes>)`, with its passes in declaration order — the order they run in.
Another pipeline's passes at the same point run before or after these by `Order`. Each pass:

| Line | |
| :-- | :-- |
| the name | in bold; its tooltip is the pass's `/// @desc` |
| settings | the kind and its keys as the `.dsp` spells them, e.g. `compute    Shader = "BoxBlur.usf"    Entry = BlurCS    Threads = uint3(8, 8, 1)    Dispatch = Blurred` |
| bindings | `read …    write …    param …`, in the pass's own order |
| slot | an HLSL pass only: `compute shader slot C03` or `pixel shader slot P01`. In orange: *no compute shader slot yet: compile the source to give the pass one*, or *…, whose snapshot is missing: compile the source*. In red: a slot number this build does not have, with the count it has |
| **Open `<file>.usf`** | an HLSL pass whose shader path resolves to a file: opens it in your preferred editor |
| **Open `<file>.dsp` at line `<n>`** | an HLSL pass whose code is in its `.dsp`: opens the `.dsp` at the pass's `hlsl` block, or at its entry in the file's block |
| *The runtime skips this pass: see Problems above.* | a pass that fails validation |

### Buffers

One row per declared buffer, its tooltip the buffer's `/// @desc`: the format and the keys as the `.dsp`
spells them (`R8    Resolution = Render    Scale = 0.5    Clear = 0    Export = true`). An exported buffer
adds a link, **exported to `<render target>`**, that shows the render target in the Content Browser — or,
before the first compile, *exported, but its render target does not exist yet: compile the source*.

### Edits

The rows read the asset live, so a value edited in the properties below shows above at once. Adding,
removing or renaming a pass or a buffer, moving a pass to another injection point, and changing a slot, a
shader path or an export rebuild the panel. An edit takes effect on the next frame and makes the pipeline
[diverge](../generation/divergence.md#a-custom-pass-pipeline) from what its `.dsp` built: until it is
reverted or adopted, a rebuild of the changed `.dsp` refuses to overwrite it.

## Notifications

The one surface on this page that DreamShader raises by itself rather than registering and waiting to
be clicked. Notifications are suppressed wherever there is no Slate application or nobody to read one
— a commandlet, a cook, a dedicated server, and any run with `-unattended`.

| Notification | Raised when | Buttons | Reference |
| :-- | :-- | :-- | :-- |
| **Divergence refusal** *(since 1.9.0)* | a rebuild is refused because the asset was edited by hand (`DSH8115`) | **Revert to Source** · **Adopt Into Source** · **Detach** · **Show Ephemeral Materials** (hidden Ephemeral instances only) · **Dismiss** | [Divergence](../generation/divergence.md#what-you-see-since-190) |
| **Divergence summary** *(since 1.9.0)* | more than five assets diverge in one rebuild round | **Open Material Browser** · **Dismiss** | [Divergence](../generation/divergence.md#how-often-it-appears) |
| **Shadowed Ephemeral materials** | the compiler backend changes while persisted assets shadow the Ephemeral result | *(none)* | [below](#notes) |
| **Ephemeral visibility toggled** | *Show Ephemeral Materials* is flipped | *(none)* | [below](#show-ephemeral-materials) |
| **Export result** | an *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* from a context menu finishes or fails: `Exported '{File}'.`, `Exported '{File}' but could not open it.` or `DreamShader failed to export '{Asset}': {Reason}` | *(none)* | [Decompiler](decompiler.md) |
| **Provenance action result** | Revert, Adopt or Detach finishes or fails | *(none)* | [Divergence](../generation/divergence.md#the-three-ways-out) |

The two divergence notifications do **not** time out, because they ask a question; every other
notification on this page is a four-second toast reporting something already done. Closing the editor
retires any divergence notification still waiting for an answer.

## Command semantics

Runtime substitutions in every quoted message on this page are shown as `{Placeholder}`.

### Recompile DSM

Asks first — a Yes/No dialog, "Recompile every DreamShader .dsm and .dsf source file? This rebuilds
all generated materials and can take a while on a large tree." On Yes, rebuilds the material
dependency graph, then stamps every `.dsm`, `.dsf`, `.dss`, `.dsi` and `.dsp` of every source root into
the pending queue with the current time, forced. A `.dsm` under a root's `Packages` folder is excluded;
`.dsh` headers are never queued, because they build nothing. The files are then compiled by the
debounce ticker exactly as if they had been saved.

Log: `DreamShader queued a full .dsm/.dsf recompile scan.`

### Clean Generated Shaders

Deletes generated `.ush` includes and queues a full scan.

| Behaviour | Detail |
| :-- | :-- |
| Safety guard | Refuses to run when `GeneratedShaderDirectory` is not inside the project's `Intermediate/` directory |
| Deletion scope | Only `*.ush` files, recursively, deleted one at a time. The directory itself is never removed |
| Flags | Missing files are tolerated; read-only files are deleted anyway |

| Message | Severity | Cause |
| :-- | :-- | :-- |
| `DreamShader refused to clean generated shaders: '{Directory}' is not inside the project Intermediate directory. Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning.` | Warning | the guard tripped |
| `DreamShader deleted {Count} generated shader file(s) from '{Directory}'.` | Display | success |
| `DreamShader cleaned generated shader includes and queued a full .dsm/.dsf recompile scan.` | Display | after the queue is stamped |

### Make Ephemeral

Finds and deletes the Materialized ThinCustom products — the ones with a package on disk — because a
saved product shadows the Ephemeral one generated from the same source.

| Aspect | Value |
| :-- | :-- |
| Search scope | asset registry, package paths `/Game`, recursive paths, recursive classes |
| Classes | `UDreamShaderMaterialInstance` only *(since 2.0.0)*. A Graph-backend `UMaterial` or a `UMaterialFunction` has no Ephemeral state to go back to, so deleting it would only delete the product |
| Gate 1 | the package must exist on disk |
| Gate 2 | the package must carry non-empty `DreamShader.SourceFile` metadata |
| Deletion | the standard editor delete flow, with the confirmation dialog and reference check |
| After deletion | every source file is immediately regenerated in memory, so references resolve without an editor restart |

Because the filter is provenance-based, hand-authored materials are never touched, while orphans
whose source file was deleted or renamed still qualify.

| Toast | Success state | Cause |
| :-- | :-- | :-- |
| `No Materialized DreamShader ThinCustom products found.` | success | nothing matched |
| `Made {Deleted} of {Total} Materialized product(s) Ephemeral.` | success when `{Deleted}` > 0 | after the delete flow |

Log: `DreamShader made {Deleted} of {Total} Materialized product(s) Ephemeral.`

### Show Ephemeral Materials

Flips `bShowEphemeralMaterials` and writes it straight to the project's
`DefaultEngine.ini`. It then walks every live `UDreamShaderMaterialInstance` whose package is newly
created and broadcasts asset creation or asset removal, so tiles appear or disappear immediately
rather than at the next re-enumeration.

| Toast | Condition |
| :-- | :-- |
| `Showing {Count} Ephemeral material(s) in the Content Browser and asset pickers.` | turned on |
| `Hidden {Count} Ephemeral material(s) from the Content Browser and asset pickers.` | turned off |

> [!WARNING]
> While Ephemeral materials are shown, they also appear in save pickers, and an explicit *Save*
> Materializes one. The saved copy then shadows the Ephemeral product. Recover with
> *Make Ephemeral*.

**View ▾ ▸ Show Ephemeral materials** in the [Material Content Browser](material-browser.md#toolbar)
runs the same command on the same global setting, toast included.

### Open Dream Shader Workspace (VSCode)

Runs in this order: re-export `material-expressions.json`, `settings.json`,
`substrate-builtins.json`, `preprocessor-defines.json` and the builtin node catalog
(`dreamshader-builtin-catalog.json`); rewrite `DShader/DreamShader.code-workspace`; then launch it
through a three-step fallback chain — VSCode, the OS default editor, Notepad. See
[Workspace](workspace.md) for the discovery order and the exact file contents.

| Toast | Condition |
| :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | the workspace file could not be written |
| `Opened DreamShader workspace in VSCode: {Path}` | VSCode launched |
| `Opened DreamShader workspace: {Path}` | the OS default editor launched |
| `Opened DreamShader workspace in Notepad: {Path}` | Notepad launched |
| `DreamShader could not open workspace: {Path}` | every launcher failed |

## Disabling the integration

```powershell
UnrealEditor.exe "<Project>.uproject" -NoDreamShaderEditorBridge
```

The switch is parsed as a bare command-line parameter, so `-NoDreamShaderEditorBridge` is the only
accepted spelling. When present, `StartupModule` returns before creating anything.

| Disabled | Still active |
| :-- | :-- |
| The editor bridge — file watcher and auto-compile-on-save, the debounce queue, the diagnostics store and all three of its sinks, `bridge.db`, the request-file poller, the VirtualFunction startup sync, the Ephemeral generation of all sources at post-engine-init, the settings watcher | The runtime modules — `DreamShaderLang` (both front ends, the binder, the IR), `DreamShader` (settings object, `UDreamShaderMaterialInstance`, the compile interface) and `DreamShaderPass` — and the `DreamShaderCompiler` editor module that the compile interface reaches |
| The preview WebSocket server on port `17864`, and the whole preview renderer | The `-run=DreamShader` [commandlet](commandlet.md), which never uses the bridge |
| Every menu, toolbar and context-menu entry on this page, and the [node context menu](#material-editor-node-context-menu) | Assets already generated and saved on disk |
| The `.usf` / `.ush` watches of the Custom Pass shader files | The [pass pipeline details panel](#pass-pipeline-details-panel) *(since 2.1.0)* |
| The Material Content Browser tab registration — the tab cannot be opened at all | |
| The exported manifests and the builtin catalog, and `DreamShader.code-workspace` regeneration | |
| The asset-rename sync service | |

A commandlet run reaches the same state by a different route: the module returns early whenever
`IsRunningCommandlet()` is true, installing only the cook-time asset materialization hook, and only
when `-run=` contains `Cook` and `-cookworker` is absent.

## Example

Launch the editor with the integration off, then do the same work headlessly:

```powershell
# No menus, no watcher, no bridge.
& "$Engine\Binaries\Win64\UnrealEditor.exe" "I:\Project\Project.uproject" -NoDreamShaderEditorBridge

# The commandlet is unaffected by the switch.
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "I:\Project\Project.uproject" `
    -run=DreamShader compile -All -Force -unattended -nopause -nosplash -stdout -log
```

## Notes

- Four of the VirtualFunction entry labels are CamelCase with no spaces — `OpenVirtualFunction`,
  `CopyVirtualFunction`, `CreateVirtualFunction`, `CopyVirtualFunctionCall` — while the fifth reads
  **Copy Virtual Function Reference**. The inconsistency is in the shipped labels.
- Deciding between the two VirtualFunction entry sets re-reads and re-lexes every project source file
  from disk, on every right-click of a Material Function asset. On projects with many sources the
  menu takes measurably longer to open. See
  [VirtualFunction tools](virtual-function-tools.md#context-menu).
- The *Show Ephemeral Materials* toggle's checked state is read live from the settings object, so
  changing the value in Project Settings updates the menu check mark.
- Changing *Default Compiler Backend*, one of the three enum mapping tables or *Preprocessor Defines*
  in Project Settings regenerates every source file — not forced: each of them is part of the
  [build key](../generation/caching.md), so exactly the sources it affects fail the skip check — and
  raises a failure-state toast when Materialized ThinCustom products would shadow the result:
  `{Count} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials. Run Tools > DreamShader > Make Ephemeral to remove them.`
  No other settings property triggers a reaction.

## Material Editor node context menu

*(since 2.0.0)* Right-clicking a node in the Material Editor graph that DreamShader generated adds a
**DreamShader** submenu, in a `DreamShader` section of the node's own context menu.

| | |
| :-- | :-- |
| Registered by | `DreamShaderEditor` at module startup, through `UToolMenus::RegisterStartupCallback` |
| ToolMenu owner | `DreamShaderSourceNavigation` |
| Menus extended | `GraphEditor.GraphNodeContextMenu.MaterialGraphNode` and the `_Custom`, `_Composite`, `_Operator`, `_PinBase`, `_Knot` variants |
| Entry condition | the node's expression GUID appears in some loaded asset's `DreamShader.SourceSpans` metadata |

> [!NOTE]
> A node with no recorded source position gets **no menu at all** — not a disabled one. Every node of
> every hand-authored material in the project is in that state, and a greyed-out entry on all of them
> would be noise. The same is true of an asset last built before 2.0.0, by the 1.x generator, which
> wrote no span table; every source, `.dsm` and `.dsf` included, is built by the 2.0 pipeline now, so
> a forced rebuild (`compile -Force`) gives its nodes the entries.

| Entry | Label | Effect |
| :-- | :-- | :-- |
| `DreamShader.OpenSourceLine` | **Open Source Line** | Opens the source file at the node's line and column, through the same three-step launcher chain as *Open Dream Shader Workspace* — VSCode, the OS default editor, Notepad |
| `DreamShader.OpenCallSite` | **Open Call Site** | Present only when the node came from an **inlined helper**: opens the line that CALLED the helper, which is usually in a different function and often a different file |

A failure is a four-second failure toast plus a `LogDreamShader` error carrying `DSH9058` — either the
source file is gone, or no launcher would start.

## `reveal-node` — the reverse direction

*(since 2.0.0)* The [bridge](bridge.md) request that goes the other way: from a line of source to the
node in the Material Editor. It is what the VSCode extension's *Reveal in Material Editor* command
sends.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `action` | string | `"reveal-node"` |
| `file` | string | required, non-empty; absolute or project-relative, normalized either way |
| `line` | number | required, 1-based |

Served synchronously: it finds the assets stamped with that source file, picks the expressions whose
span falls on the line, opens the Material Editor for the asset that owns them and selects the first.
A node that **starts** on the line wins over one that was merely inlined from a call there.

The response is the ordinary `Responses/<requestId>.json` envelope with four fields added:

| Field | Type | Notes |
| :-- | :-- | :-- |
| `version` | number | always `1` — the payload version of the added fields, separate from `protocol` |
| `assetPath` | string | the asset whose graph was opened; for a ThinCustom product this is the hidden **base material**, which is where the graph lives |
| `instanceAssetPath` | string | present only for a ThinCustom product: the instance the base is addressed by |
| `expressions` | array | every matching expression GUID, hyphenated (`FGuid` `DigitsWithHyphens`, the spelling the asset metadata uses), the selected one first |

Failures come back as `ok: false` with the reason in `diagnostics` (`stage: "navigate"`), coded
`DSH9050`–`DSH9057`. The two worth knowing: `DSH9051` means no asset from that source is loaded in
this editor — compile the file first — and `DSH9052` means the assets exist but were built before
node navigation, so they carry no span table and need a `-Force` rebuild.

## See also

- [Tools](index.md) — the editor tooling hub
- [Material Content Browser](material-browser.md) — the tab the Tools menu opens
- [Decompiler](decompiler.md) — what *Export .dss*, *Export .dsi* and the two *Export Legacy* entries produce
- [VirtualFunction tools](virtual-function-tools.md) — the conditional VirtualFunction section
- [Workspace](workspace.md) — the workspace file, VSCode discovery and the launch fallback chain
- [Bridge](bridge.md) — the file and WebSocket surfaces the switch disables
- [Commandlet](commandlet.md) — the headless entry point
- [Project settings](../settings/project.md) — every setting these commands read or write
- [Ephemeral materials](../generation/ephemeral.md) — why persisted assets shadow generated ones
- [Generated HLSL](../generation/generated-hlsl.md) — what *Clean Generated Shaders* deletes
- [Divergence](../generation/divergence.md#a-custom-pass-pipeline) — Revert and Adopt for a pipeline edited in its details panel
