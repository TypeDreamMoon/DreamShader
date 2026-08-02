# Editor integration

> [DreamShader](../index.md) » [Tools](index.md) » **Editor integration**

Every place DreamShader attaches itself to the Unreal editor UI: the Tools menu, the Level Editor
toolbar, the Content Browser asset context menus, the Material Editor toolbar, and the Window menu.

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
| `DreamShader.RecompileAll` | **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics." | `Icons.Refresh` | Rebuilds the dependency graph and queues every project `.dsm` and `.dsf` for compilation | [below](#recompile-dsm) |
| `DreamShader.CleanGeneratedShaders` | **Clean Generated Shaders** | "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile." | `Icons.Delete` | Deletes every `*.ush` under the generated-shader directory, then queues a full scan | [below](#clean-generated-shaders) |
| `DreamShader.CleanPersistedGeneratedAssets` | **Clean Persisted Generated Assets** | "Delete DreamShader-generated material assets that are saved on disk (they shadow in-memory material mode). Shows a confirmation with the full list; source files are untouched and regenerate in memory." | `Icons.Delete` | Deletes on-disk assets carrying DreamShader provenance metadata, through the standard editor delete flow | [below](#clean-persisted-generated-assets) |
| `DreamShader.ToggleShowInMemoryMaterials` | **Show In-Memory Materials** | "Show memory-only DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. While shown, an explicit Save on one would persist it to disk (the shadow warning and Clean command cover recovery)." | *(none)* | Toggle button. Flips `bShowInMemoryMaterialsInContentBrowser` and writes it to `DefaultEngine.ini` | [below](#show-in-memory-materials) |
| `DreamShader.OpenWorkspace` | **Open Dream Shader Workspace (VSCode)** *(since 1.2.1)* | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | Re-exports the three bridge manifests, rewrites `DShader/DreamShader.code-workspace`, then launches it | [Workspace](workspace.md) |
| `OpenMaterialContentBrowser` | **Material Content Browser** *(since 1.5.0)* | "Open the DreamShader Material Content Browser." | `ClassIcon.Material` | Invokes the `DreamShaderMaterialBrowser` nomad tab | [Material Content Browser](material-browser.md) |

> [!NOTE]
> The first five entries and the sixth are registered by two different startup callbacks into the
> same `DreamShader` section. Their relative order within the section is registration-order
> dependent and is not guaranteed between editor runs.

## Level Editor toolbar

Extending `LevelEditor.LevelEditorToolBar.AssetsToolBar`, section `DreamShader`.

| Entry name | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.RecompileAllToolbar` | **DSM** | "Recompile all DreamShader .dsm and .dsf source files." | `Icons.Refresh` | Identical to *Recompile DSM* |
| `DreamShader.OpenWorkspaceToolbar` | **Open Dream Shader Workspace (VSCode)** | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | Identical to *Open Dream Shader Workspace (VSCode)* |

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
| `UMaterialInstanceConstant` | `DreamShader.InstanceCreateActions` | flat entry **Create DreamShader instance** |
| `UDreamShaderMaterialInstance` | `DreamShader.InstanceCreateActions` | flat entry **Create DreamShader instance** |

Menu names in Unreal are keyed on the exact class, so the instance entry is registered twice — once
for the stock class and once for the DreamShader subclass.

| Entry | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.CreateInstance` | **Create DreamShader instance** *(since 1.5.0)* | "Create a material instance that shares this material's compiled shader map." | `ClassIcon.MaterialInstanceConstant` | Opens the [Create material instance](material-browser.md#create-material-instance) dialog. Requires the selection to cast to `UMaterialInterface` |

### Material submenu

`DreamShader.MaterialActions` — label **DreamShader**, tooltip "DreamShader actions for this
Material.", icon `Icons.Settings`. Built only when the single selected asset is a `UMaterial`.

Section `DreamShader.DecompileActions`, titled **Decompiler**:

| Entry | Label | Tooltip | Icon | Effect | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportMaterialDSM` | **Export DSM** *(since 1.3.5)* | "Export this Material graph to a DreamShader .dsm source file." | `Icons.Save` | Decompiles to `DShader/Decompiled/Materials/…​.dsm` and opens the file | [Decompiler](decompiler.md) |

### Material Function submenu

`DreamShader.MaterialFunctionActions` — label **DreamShader**, tooltip "DreamShader actions for this
Material Function.", icon `Icons.Settings`.

Section 1, `DreamShader.DecompileActions`, titled **Decompiler**:

| Entry | Label | Tooltip | Icon | Effect | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportFunctionDSF` | **Export DSF** *(since 1.3.5)* | "Export this Material Function graph to a DreamShader .dsf source file." | `Icons.Save` | Decompiles to `DShader/Decompiled/{Functions,Layers,LayerBlends}/…​.dsf` and opens the file | [Decompiler](decompiler.md) |

Section 2, `DreamShader.VirtualFunctionActions`, titled **VirtualFunction** *(since 1.2.1)*. Its
contents depend on whether a `VirtualFunction` declaration already references this asset — the full
table is on [VirtualFunction tools](virtual-function-tools.md#context-menu).

| Declaration exists? | Entries |
| :-- | :-- |
| yes | **OpenVirtualFunction**, **Copy Virtual Function Reference** |
| no | **CopyVirtualFunction**, **CreateVirtualFunction**, **CopyVirtualFunctionCall** |

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

## Command semantics

Runtime substitutions in every quoted message on this page are shown as `{Placeholder}`.

### Recompile DSM

Rebuilds the material dependency graph, then stamps every project `.dsm` and `.dsf` into the pending
queue with the current time. Files under `DShader/Packages` that are `.dsm` are excluded. The files
are then compiled by the debounce ticker exactly as if they had been saved.

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

### Clean Persisted Generated Assets

Finds and deletes DreamShader-generated assets that exist on disk, because a saved asset shadows the
memory-only material generated from the same source.

| Aspect | Value |
| :-- | :-- |
| Search scope | asset registry, package paths `/Game`, recursive paths, recursive classes |
| Classes | `UMaterial`, `UMaterialFunction`, `UDreamShaderMaterialInstance` |
| Gate 1 | the package must exist on disk |
| Gate 2 | the package must carry non-empty `DreamShader.SourceFile` metadata |
| Deletion | the standard editor delete flow, with the confirmation dialog and reference check |
| After deletion | every source file is immediately regenerated in memory, so references resolve without an editor restart |

Because the filter is provenance-based, hand-authored materials are never touched, while orphans
whose source file was deleted or renamed still qualify.

| Toast | Success state | Cause |
| :-- | :-- | :-- |
| `No persisted DreamShader-generated assets found.` | success | nothing matched |
| `Deleted {Deleted} of {Total} persisted generated asset(s).` | success when `{Deleted}` > 0 | after the delete flow |

Log: `DreamShader deleted {Deleted} of {Total} persisted generated asset(s).`

### Show In-Memory Materials

Flips `bShowInMemoryMaterialsInContentBrowser` and writes it straight to the project's
`DefaultEngine.ini`. It then walks every live `UDreamShaderMaterialInstance` whose package is newly
created and broadcasts asset creation or asset removal, so tiles appear or disappear immediately
rather than at the next re-enumeration.

| Toast | Condition |
| :-- | :-- |
| `Showing {Count} in-memory material(s) in the Content Browser and asset pickers.` | turned on |
| `Hidden {Count} in-memory material(s) from the Content Browser and asset pickers.` | turned off |

> [!WARNING]
> While memory-only materials are shown, they also appear in save pickers, and an explicit *Save*
> writes one to disk. The saved copy then shadows the in-memory material. Recover with *Clean
> Persisted Generated Assets*.

The Project page of the [Material Content Browser](material-browser.md#project-page) carries a
checkbox for the same global setting. That checkbox early-outs when the value is unchanged and shows
no toast; the menu entry always toasts.

### Open Dream Shader Workspace (VSCode)

Runs in this order: re-export `material-expressions.json`, `settings.json` and
`substrate-builtins.json`; rewrite `DShader/DreamShader.code-workspace`; then launch it through a
three-step fallback chain — VSCode, the OS default editor, Notepad. See [Workspace](workspace.md)
for the discovery order and the exact file contents.

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
| The editor bridge — file watcher and auto-compile-on-save, the debounce queue, the diagnostics store and all three of its sinks, `bridge.db`, the request-file poller, the VirtualFunction startup sync, the in-memory generation of all sources at post-engine-init, the settings watcher | The runtime `DreamShader` module — parser, generator, settings object, `UDreamShaderMaterialInstance` |
| The preview WebSocket server on port `17864`, and the whole preview renderer | The `-run=DreamShader` [commandlet](commandlet.md), which never uses the bridge |
| Every menu, toolbar and context-menu entry on this page | Assets already generated and saved on disk |
| The Material Content Browser tab registration — the tab cannot be opened at all | |
| The three exported manifests, and `DreamShader.code-workspace` regeneration | |

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
- The *Show In-Memory Materials* toggle's checked state is read live from the settings object, so
  changing the value in Project Settings updates the menu check mark.
- Changing *Default Compiler Backend* in Project Settings regenerates every source file in memory and
  raises a failure-state toast when persisted assets would shadow the result:
  `{Count} previously generated asset(s) are still saved on disk and shadow the in-memory materials. Run Tools > DreamShader > Clean Persisted Generated Assets to remove them.`
  No other settings property triggers a reaction.

## See also

- [Tools](index.md) — the editor tooling hub
- [Material Content Browser](material-browser.md) — the tab the Tools menu opens
- [Decompiler](decompiler.md) — what *Export DSM* / *Export DSF* produce
- [VirtualFunction tools](virtual-function-tools.md) — the conditional VirtualFunction section
- [Workspace](workspace.md) — the workspace file, VSCode discovery and the launch fallback chain
- [Bridge](bridge.md) — the file and WebSocket surfaces the switch disables
- [Commandlet](commandlet.md) — the headless entry point
- [Project settings](../settings/project.md) — every setting these commands read or write
- [In-memory materials](../generation/in-memory.md) — why persisted assets shadow generated ones
- [Generated HLSL](../generation/generated-hlsl.md) — what *Clean Generated Shaders* deletes
