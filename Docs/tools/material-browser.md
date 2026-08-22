# Material Content Browser

> [DreamShader](../index.md) » [Tools](index.md) » **Material Content Browser**

A dockable editor tab over every DreamShader source and every material in the project: what each
source compiles to, whether that asset is current, whether somebody has edited it by hand, what it
imports and what inherits from it — and the actions that follow from each of those answers.

| | |
| :-- | :-- |
| Kind | nomad tab, registered by the `DreamShaderEditor` module |
| Tab id | `DreamShaderMaterialBrowser` |
| Display name | **Material Content Browser** |
| Modes | **Sources** (the `.dsm` / `.dsf` / `.dsh` tree) · **Assets** (the project's materials) |
| Since | `1.5.0` |

## Opening it

| Route | Path |
| :-- | :-- |
| Tools menu | *Tools ▸ DreamShader ▸ Material Content Browser* |
| Window menu | *Window ▸ Tools ▸ Material Content Browser* |
| Content Browser | right-click a material or instance ▸ **Show in Material Content Browser** — opens the tab on that asset |

The tab is registered only when the editor bridge starts. Launching the editor with
`-NoDreamShaderEditorBridge` removes every route — see
[Editor integration](editor-integration.md#disabling-the-integration).

## Layout

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ [⟳ Refresh] [▶ Compile ▾] [+ New ▾] │ search…          │ [View ▾] [VSCode] [⚙] │
├──────────────┬──────────────────────────────────┬──────────────────────────┤
│ NAVIGATION   │ LIST                              │ INSPECTOR                │
│ ▾ Sources    │ Sources mode: columns or tiles    │ live preview · mesh      │
│   ▾ Project  │ Assets mode: the asset picker     │ name · status · storage  │
│     Materials│                                   │ actions                  │
│   ▸ PluginA  │                                   │ Root / Source / Asset /  │
│ ▾ Content    │                                   │   Domain / Blend / …     │
│   ▸ /Game    │                                   │ Diagnostics (N)          │
│ ──────────── │                                   │ Provenance               │
│ Quick filters│                                   │ Imports / Used by        │
│ ☐ Errors …   │                                   │ Inheritance · Children   │
├──────────────┴──────────────────────────────────┴──────────────────────────┤
│ 14 sources · 9 ok · 3 stale · 1 errors · 1 edited by hand · 5 in memory │ bridge: idle │
└────────────────────────────────────────────────────────────────────────────┘
```

Three panes in a horizontal splitter, a toolbar above, a status bar below. The pane fractions,
the mode, the navigation scope, the filters, the sort, the list/tile choice, the preview mesh and
the last selection are all saved per user and per project
(`UDreamShaderBrowserUserSettings`, `EditorPerProjectUserSettings`) and restored when the tab is
next opened.

## Toolbar

| Control | Label | Effect |
| :-- | :-- | :-- |
| Button | **Refresh** (`F5`) | Rescans the source roots, rebuilds the dependency graph, recomputes every status |
| Menu | **Compile ▾** | **Compile** (`Ctrl+B`) the selection · **Compile stale** — every source that is stale, never compiled, or failed · **Compile all** (`Ctrl+Shift+B`) — every `.dsm` and `.dsf`, behind a progress dialog |
| Menu | **New ▾** | **Material (.dsm)** · **Material function (.dsf)** · **Header (.dsh)** — see [New source](#new-source) |
| Search box | (`Ctrl+F` focuses it) | Case-insensitive substring match against the file name, the root name, the source path, the asset path and the status detail — so an error message is searchable, and a plugin's name filters to everything it ships. In Assets mode it matches the asset name |
| Menu | **View ▾** | **Tiles** — the Sources list as thumbnail tiles · **Sort by** Name / Status / Root / Asset path, **Ascending** · **Show in-memory materials** — the global project setting, see [Editor integration](editor-integration.md#show-in-memory-materials) |
| Button | *(VSCode icon)* | Writes and opens the DreamShader workspace — identical to *Tools ▸ DreamShader ▸ Open Dream Shader Workspace* |
| Button | *(gear)* | Opens *Project Settings ▸ DreamPlugin ▸ DreamShader* |

Every compile started from this tab goes through the editor bridge, so its result reaches the
diagnostics store, `diagnostics.json` and the VSCode extension exactly as a compile-on-save does,
and a success clears the file's previous error.

## Navigation

A tree of two groups. Selecting a node **scopes the list and switches the mode**.

| Node | Scope |
| :-- | :-- |
| **Sources** | every source under every root (Sources mode) |
| **Sources ▸ *root*** | one root — *Project*, or a plugin that ships a `DShader` folder — with the number of sources under it |
| **Sources ▸ *root* ▸ *folder*** | that folder and everything below it |
| **Content** | `/Game` (Assets mode) |
| **Content ▸ /Game**, **Content ▸ /*Plugin*** | a mount point — `/Game`, and the content root of every plugin that ships DreamShader sources, which is where its generated assets land |
| **Content ▸ … ▸ *folder*** | that content path and everything below it; folders are read from the asset registry on expand |

### Quick filters

Check boxes under the tree. The four status filters are **OR-ed**: ticking *Errors* and *Stale*
shows entries that are either.

| Filter | Keeps |
| :-- | :-- |
| **Errors** | sources whose last compile failed, or that could not be read or parsed |
| **Stale** | sources that changed since their asset was generated |
| **Edited by hand** | generated assets whose contents no longer match the last generation ([Divergence](../generation/divergence.md)) |
| **In memory** | materials that exist only in memory |
| **Hide functions** | drops every `.dsf` and `.dsh` |

The status-bar counts are the same filters: clicking a count turns it on alone, clicking it again
turns it off, and clicking the *ok* count clears all four.

## Sources mode

Every scanned source that passes the scope, the filters and the search, as a **list** with sortable
columns or — *View ▸ Tiles* — a grid of thumbnails. Multi-selection; right-click for the
[context menu](#context-menu); double-click opens the material when there is one, else the source.

| Column | Content |
| :-- | :-- |
| *(glyph)* | the status glyph, tooltip = the status detail |
| **Name** | the file name, tooltip = the absolute path |
| **Root** | *Project*, or the owning plugin |
| **Status** | the status label; for `.dsf` / `.dsh`: `function · used by N material(s)` |
| **Asset** | the package path the source compiles into |

### Listed files

**`.dsm`, `.dsf` and `.dsh`** found recursively under every source root, excluding each root's
`Packages` folder. Selection survives a Refresh: entries are re-selected by path.

### Status values

| Status | Glyph | Label | Meaning |
| :-- | :-- | :-- | :-- |
| `UpToDate` | `●` green | **up to date** | The generated asset exists and its stamped source hash matches the current source |
| `InMemoryUntracked` | `◐` green | **compiled in memory** | The asset exists in memory and carries no source hash — a memory-only build stamps the path alone, deliberately, so currency cannot be judged. Compiled, freshness unknown: **not** stale |
| `Stale` | `●` amber | **stale** | The asset exists and its stamped hash differs from the current source |
| `NotCompiled` | `○` grey | **not compiled** | No object at the resolved object path. Detail: `No generated asset at {ObjectPath}` |
| `Error` | `▲` red | **compile error** | A compile failed, from this tab or per the bridge's diagnostics |
| `Library` | `◆` blue | **function / header** | A `.dsf` or `.dsh` |
| `Unresolved` | `▲` red | **unresolved** | The source could not be read, could not be parsed, or declares no top-level block |

Status computation per `.dsm`:

| # | Step | Failure |
| :-- | :-- | :-- |
| 1 | Resolve the generated asset's object path from `Name=` and `Root=` (imports stripped first) | ⇒ `unresolved` |
| 2 | Look the object up **without loading it** | ⇒ `not compiled` |
| 3 | Load the prepared source, with `import` directives inlined | ⇒ `unresolved` |
| 4 | Hash it and compare against the asset's stamped source file and hash | match ⇒ `up to date` |
| 5 | No stamped hash and the asset is not on disk | ⇒ `compiled in memory` |
| 6 | otherwise | ⇒ `stale` |

Then the bridge's diagnostics are overlaid: any `error` record for the file makes it `compile
error`, with the first record as the detail. All records stay on the entry for the inspector.

> [!NOTE]
> Step 2 does not load, so a material that exists on disk but has not been loaded this session
> reports **not compiled** until something loads it.

## Assets mode

The engine's asset picker over `UMaterial` and `UMaterialInstanceConstant` (recursively, which
includes `UDreamShaderMaterialInstance`), scoped to the navigation tree's content path. Kept as the
engine widget on purpose: thumbnails, drag-to-viewport, the column view and the picker's own
settings all come with it.

The shared filters apply here too, and are answered **without loading anything**: a scanned
source's asset is judged by its entry, and *In memory* is read off the registry's package flags. A
hand-authored material that DreamShader never generated passes no status filter except *In
memory*.

Double-click opens the asset. Right-click gives the same [context menu](#context-menu).

## Inspector

One panel for both modes. It shows whichever halves of the selection exist — the **source** half
(a scanned file) and the **asset** half (a material in the project) — joined through the asset's
`DreamShader.SourceFile` stamp. A material picked in Assets mode therefore shows its source's
compile status and diagnostics; a source picked in Sources mode shows its asset's provenance and
inheritance.

| Section | Contents |
| :-- | :-- |
| **Preview** | A real render of the material through the plugin's [preview renderer](preview.md), 224×224, re-rendered on every compile. **Left-drag orbits**; the **Mesh** picker chooses sphere, plane, cube, cylinder or shaderball and is remembered. Rendered asynchronously, so it never stalls the editor. Shown only when there is a compiled material |
| **Header** | Name · status glyph and label · `memory-only (not saved)` / `on disk` · a badge when the asset is open in an asset editor (a rebuild refuses while it is) |
| **Actions** | **Create instance** · **Compile** · **Open** · **Materialize** (memory-only assets) · **Open source** |
| **Info rows** | **Base** · **Domain** · **Blend mode** · **Root** · **Source** (a link: shows the file in Sources mode) · **Asset** (a link: shows the asset in Assets mode) · **Storage** · **Provenance** |
| **Diagnostics (N)** | Every record the bridge holds for the file: `[DSHnnnn] L{line}:{col} message`, each a link that opens the file in your editor at that line — into the imported header when the record points there. A failure this tab pinned itself shows its message alone |
| **Provenance** | The digest state with a one-line explanation, then the answers: **Revert to Source** · **Adopt Into Source** · **Detach**; or, for a material DreamShader never generated, **Export DSM** / **Export DSF**. See [Divergence](../generation/divergence.md). *Adopt* is disabled, with the reason, when the source ships with a plugin |
| **Imports (N)** | For a material: every header and function it imports, transitively — links into the Sources list |
| **Used by (N)** | For a header or function: every material that imports it — links |
| **Inheritance** | The parent chain, root first, each row a link that re-targets the panel |
| **Child instances (N)** | Loaded instances whose parent this is, plus — from the asset registry's referencers — saved instances nobody has loaded this session, marked *(not loaded)*; clicking one loads it |

## Status bar

`{N} sources` · the counts of **ok** (up to date or compiled in memory), **stale**, **errors**,
**edited by hand** and **in memory**, each a one-click filter · the bridge's state: `bridge: idle`,
`bridge: {action}` while it compiles, `bridge: idle (another editor owns writes)` when a second
editor on the same project holds the write lock, or `bridge: off`. The tooltip is the bridge's last
result.

## Context menu

Right-click in either list. Commands act on the selection; most take the first selected entry.

| Section | Entries |
| :-- | :-- |
| **Open** | **Open material** (`Enter`) · **Open source** (`Ctrl+Enter`, at the first error when there is one) · **Reveal in Content Browser** |
| **Build** | **Compile** · **Create instance** · **Materialize** (shown for memory-only assets; acts on every memory-only entry in the selection) |
| **Generated asset** | **Revert to Source** (acts on every generated entry in the selection, confirming each) · **Adopt Into Source** · **Detach From DreamShader** — only when the asset carries DreamShader's stamp |
| **Decompiler** | **Export DSM / DSF** — only for a hand-authored material or function |
| **Copy** | **Copy source path** · **Copy asset path** |

## Following the editor

Nothing here polls. The tab listens to:

| Signal | Reaction |
| :-- | :-- |
| a source generated, by any route (the watcher, this tab, a provenance action, the VSCode extension) | that entry's status is recomputed |
| the bridge's diagnostics commit | every entry's error overlay is refreshed |
| a source file added, removed or renamed on disk (even with compile-on-save off) | the tree is rescanned |
| a source file modified on disk | that entry is recomputed |
| an asset added, removed or renamed in the registry | the entry it resolves to is recomputed |

Reactions are coalesced into one refresh per tick, so a header that recompiles its twelve
dependents repaints the tab once. **Refresh** remains for anything outside these — a plugin mounted
mid-session, say.

## New source

*New ▾* writes a file from the plugin's templates (`Resources/Templates/NewMaterial.dsm`,
`NewFunction.dsf`, `NewHeader.dsh`) and the bridge's watcher lists and compiles it as it would any
save. The dialog:

| Field | Default |
| :-- | :-- |
| **Name** | `M_NewMaterial` / `F_NewFunction` / `Common` — must be an identifier |
| **Folder** | the source folder the navigation tree points at, or the project's `DShader` root when that folder is a plugin's (plugin sources are read-only) |

The block's `Name=` is the folder's path relative to its root plus the name, so the asset lands
beside its neighbours' — a file created in `DShader/Materials/` compiles to `/Game/Materials/…`.

| Refusal | Message |
| :-- | :-- |
| not an identifier | `The name must be an identifier: letters, digits and underscores, not starting with a digit.` |
| outside every writable root | `Choose a folder under the project's DShader root. A plugin's sources are read-only.` |
| file exists | `'{Path}' already exists.` |

The three templates are compiled by the automation test
`DreamShader.Browser.NewSource.TemplatesRenderAndCompile`, so they cannot rot.

## Create material instance

Reached from the inspector, the context menu, and the Content Browser entry **Create DreamShader
instance**.

| Aspect | Value |
| :-- | :-- |
| Window title | **Create material instance** |
| Size | 480×240, modal, not resizable through minimize or maximize |

| Field | Kind | Default |
| :-- | :-- | :-- |
| **Parent** | read-only text | the parent material's name |
| **Name** | text box | `MI_<ParentName>`, uniquified against existing assets |
| **Folder** | text box | the parent's folder plus the *Material Instance Subfolder* setting (`Instances` by default); an empty subfolder puts the instance alongside the parent |
| **Browse...** | button — "Pick the destination folder." | opens a folder picker titled "Choose a destination folder" |
| **Open the instance after creating** | checkbox | checked |
| **Cancel** / **Create** | buttons | — |

Creation steps and their errors:

| Guard | Error |
| :-- | :-- |
| No parent material | `No parent material was provided.` |
| Empty name or empty folder | `Provide a name and a destination folder.` |
| The parent is memory-only | forwards the [Materialize](#materialize) error |
| An asset already exists at the target path | `An asset already exists at {PackageName}.` |
| The package could not be created | `Failed to create package {PackageName}.` |
| The object could not be created | `Failed to create the material instance object.` |
| The package could not be saved | `Generated DreamShader asset '{Path}' could not be saved.` |
| The parent died between opening and confirming | `The parent material is no longer available.` — the window closes |

On success the toast reads `Created {Name}` — the asset name from the **Name** box, not the object
path — and the window closes. On failure the error is toasted and **the window stays open** so the
name or folder can be corrected.

| Detail | Behaviour |
| :-- | :-- |
| Created class | a plain `UMaterialInstanceConstant`, not a `UDreamShaderMaterialInstance` |
| Object flags | `RF_Public \| RF_Standalone` |
| Parent assignment | set editor-only, followed by a post-edit change and an asset-created broadcast |
| Save failure rollback | the half-created object is un-broadcast, stripped of its flags, renamed into the transient package without redirectors, marked as garbage, and the package's dirty flag is cleared — so it cannot survive GC, appear in the Content Browser, be persisted by *Save All*, or block a same-name retry |

Instancing a memory-only parent materializes the parent first, so a child instance never references a
transient object. A source that has never been compiled is compiled first; if it still yields no
material the toast reads `Compile {File} first.`

## Materialize

Writes a memory-only material, and the hidden base it wraps, to disk.

| Rule | Detail |
| :-- | :-- |
| "Memory-only" test | the material's package carries the newly-created package flag |
| Already on disk | returned unchanged — the action is a no-op |
| Requirement | the material must be a `UDreamShaderMaterialInstance` with a recorded source file path |
| Implementation | re-runs generation for that source file with force on and transient **off**, then reloads the object at the same object path |

| Message | Cause |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | the material is not a DreamShader instance, or records no source path |
| `Failed to materialize the material to disk: {Error}` | generation failed |
| `Materialized the material but could not reload it at {ObjectPath}.` | generation succeeded but the object did not reload |
| `Materialized {Name} to disk` | success — the inspector re-targets the persisted asset |

The material is re-resolved by object path when the button is clicked, rather than held from when
the panel was built, so a delete or garbage collection in between cannot crash it.

## Keyboard

| Key | Command |
| :-- | :-- |
| `F5` | Refresh |
| `Ctrl+B` | Compile the selection |
| `Ctrl+Shift+B` | Compile all |
| `Enter` | Open the material |
| `Ctrl+Enter` | Open the source |
| `Ctrl+F` | Focus the search box |

## Example

A source tree and what Sources mode shows for it:

```text
<Project>/DShader/
    Materials/M_Emissive.dsm        ● up to date          /Game/Materials
    Materials/M_Toon.dsm            ◐ compiled in memory  /Game/Materials
    Materials/M_Broken.dsm          ▲ compile error       [DSH2104] L12:9 Unknown identifier 'Tin'.
    Materials/M_New.dsm             ○ not compiled        No generated asset at /Game/Materials/M_New.M_New
    Lib/Noise.dsf                   ◆ function · used by 2 material(s)
    Lib/Common.dsh                  ◆ function · used by 3 material(s)
    Packages/Sample/Demo.dsm        (not listed — under DShader/Packages)
```

## See also

- [Editor integration](editor-integration.md) — the menu entries that open this tab
- [Preview](preview.md) — the renderer behind the inspector's preview
- [Divergence](../generation/divergence.md) — Revert, Adopt, Detach
- [Decompiler](decompiler.md) — Export DSM / DSF
- [In-memory materials](../generation/in-memory.md) — memory-only generation, the hidden base, materializing
- [Caching](../generation/caching.md) — the source hash behind **up to date** and **stale**
- [Asset paths](../generation/asset-paths.md) — how `Name=` and `Root=` resolve to the object path
- [Project settings](../settings/project.md) — *Material Instance Subfolder* and the in-memory visibility toggle
- [Bridge](bridge.md) — the diagnostics store this tab reads and the events it follows
- [Material instance API](../api/material-instance.md) — `UDreamShaderMaterialInstance` in C++
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
