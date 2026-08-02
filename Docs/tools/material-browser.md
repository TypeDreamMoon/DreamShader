# Material Content Browser

> [DreamShader](../index.md) » [Tools](index.md) » **Material Content Browser**

A dockable editor tab with two pages: one for browsing the project's materials and instancing them,
one for the DreamShader source files and their generated assets.

| | |
| :-- | :-- |
| Kind | nomad tab, registered by the `DreamShaderEditor` module |
| Tab id | `DreamShaderMaterialBrowser` |
| Display name | **Material Content Browser** |
| Pages | **Project** (index 0, active by default) · **Dream Shader Gen** (index 1) |
| Since | `1.5.0` |

## Opening it

| Route | Path |
| :-- | :-- |
| Tools menu | *Tools ▸ DreamShader ▸ Material Content Browser* |
| Window menu | *Window ▸ Tools ▸ Material Content Browser* |

The tab is registered only when the editor bridge starts. Launching the editor with
`-NoDreamShaderEditorBridge` removes both routes — see
[Editor integration](editor-integration.md#disabling-the-integration).

## Shell

The root widget is a header bar of two radio-style check boxes over a widget switcher.

| Control | Label | Behaviour |
| :-- | :-- | :-- |
| Page button 0 | **Project** | Switches to the Project page. Active by default |
| Page button 1 | **Dream Shader Gen** | Switches to the Gen page |

Only the *checked* transition is honoured, so re-clicking the page you are already on does nothing.
The two pages keep independent state; switching does not refresh either one.

## Project page

A horizontal splitter: the asset picker takes `0.62` of the width, the details panel `0.38`.

### Asset picker

| Aspect | Value |
| :-- | :-- |
| Classes | `UMaterial`, `UMaterialInstanceConstant` |
| Recursive classes | yes — this is what includes `UDreamShaderMaterialInstance` |
| Package paths | `/Game`, recursive |
| Initial view | Tile |
| Selection mode | single |
| Dragging | allowed |
| Class column | hidden |
| Engine content | never force-shown |
| Path in column view | shown |
| Double-click | opens the asset in its editor |
| Empty-state text | "No materials found under /Game." |

### Header controls

| Control | Label | Tooltip | Effect |
| :-- | :-- | :-- | :-- |
| Button | **Create instance** | "Create a material instance of the selected material (shares its compiled shader map)." | Opens the [Create material instance](#create-material-instance) dialog for the selected asset. With nothing selected it raises the toast `Select a material to create an instance of.` and opens no window |
| Status text | *(dynamic)* | — | Reads `Selected: {Asset}` when something is selected, otherwise `Select a material, then create an instance.` |
| Checkbox | **Show in-memory materials** | "Show DreamShader's memory-only materials here and in the Content Browser (global setting)." | Writes `bShowInMemoryMaterialsInContentBrowser` to `DefaultEngine.ini` and re-broadcasts asset creation or removal for every newly created in-memory instance |

> [!NOTE]
> The checkbox and *Tools ▸ DreamShader ▸ Show In-Memory Materials* drive the same **global project
> setting**, not a per-tab filter. The checkbox early-outs when the value is unchanged and raises no
> toast; the menu entry always toasts.

### Details panel

Empty state: "Select a material to see its inheritance and settings."

The thumbnail is 96×96, drawn from a 16-entry pool refreshed by a 0.05 s timer.

| Button | Tooltip | Visible when |
| :-- | :-- | :-- |
| **Create instance** | — | always |
| **Open** | — | always |
| **Materialize** | "Write this memory-only material (and its base) to disk." | only when the selected material is memory-only; collapsed otherwise |

| Info row | Value |
| :-- | :-- |
| **Base** | the name of the material's base material, or `-` |
| **Domain** | the base material's material-domain display name |
| **Blend mode** | the base material's blend-mode display name |
| **Storage** | `memory-only (not saved)` or `on disk` |
| **Source** | the `.dsm` path recorded on a `UDreamShaderMaterialInstance`, or `-` |

| Section | Contents |
| :-- | :-- |
| **Inheritance** | The parent chain, root first, built by walking each instance's parent upward. Every row is a clickable link that re-targets the panel. Rows are indented four spaces per level behind a `└ ` prefix; ancestors are drawn in blue, the selected material in the default foreground |
| **Child instances (N)** | Material instance constants whose parent is the selected material. Empty state: "No loaded child instances." |

> [!WARNING]
> **Child instances** scans only instances that are currently **loaded** in the editor. An instance
> that exists on disk but has not been loaded does not appear, and the count is not a reference
> count. Load the asset, or use the Content Browser's *Reference Viewer*, for a complete picture.

## Dream Shader Gen page

A horizontal splitter: the source list takes `0.6` of the width, the preview pane `0.4`. Thumbnails
come from an 8-entry pool refreshed by a 0.05 s timer.

### Header controls

| Control | Label / hint | Tooltip | Effect |
| :-- | :-- | :-- | :-- |
| Button | **Refresh** | "Rescan the source directory and recompute status." | Re-runs the whole [refresh pipeline](#refresh-pipeline) |
| Button | **Compile all** | "Force-recompile every .dsm/.dsf source (in memory)." | See [Compile actions](#compile-actions) |
| Search box | hint **Search sources** | — | Case-insensitive substring filter, matched against the **display name only** — not the path, not the status |
| Checkbox | **Errors only** | — | Keeps only items with status `compile error` or `unresolved` |
| Checkbox | **Hide functions** | — | Drops every `.dsf` and `.dsh` item |
| Counter | `{Visible} / {Total}` | — | Subdued text at the right of the bar |

### Listed files

The page lists **`.dsm`, `.dsh` and `.dsf`** sources found recursively under the configured source
directory. Everything under `DShader/Packages` is excluded — including package headers and function
files, which therefore never appear here.

### Status values

| Status | Glyph | Label | Meaning |
| :-- | :-- | :-- | :-- |
| `UpToDate` | `●` green | **up to date** | The generated asset exists and its stored source hash matches the current source |
| `Stale` | `●` amber | **stale** | The generated asset exists but its stored hash differs from the current source |
| `NeverCompiled` | `○` grey | **not compiled** | No object exists at the resolved object path. Detail: `No generated asset at {ObjectPath}` |
| `Error` | `▲` red | **compile error** | A compile through this page failed, or `diagnostics.json` reports an error for this file |
| `Function` | `◆` blue | **function / header** | The item is a `.dsf` or `.dsh`. Detail: "Function library / header. Recompiles the materials that import it." |
| `Unresolved` | `▲` red | **unresolved** | The source could not be read, could not be parsed, or declares no top-level `Shader` block |

Each row draws the status glyph — whose tooltip is the status detail — then the file name, then a
subdued sub-label. For `.dsf` and `.dsh` items the sub-label is `function · used by {N} material(s)`.

### Refresh pipeline

| # | Step |
| :-- | :-- |
| 1 | Enumerate `.dsm`, `.dsh` and `.dsf` under the source directory, excluding `DShader/Packages`, and sort |
| 2 | Rebuild the material dependency graph to fill each header's and function's dependent count |
| 3 | Recompute every item's status |
| 4 | Clear the selection — the list is rebuilt, so the previously selected item no longer exists |
| 5 | Overlay errors read from `diagnostics.json` |
| 6 | Re-apply the search and filter check boxes |
| 7 | Rebuild the preview pane |

Status computation per item:

| # | Step | Failure |
| :-- | :-- | :-- |
| 1 | `.dsf` / `.dsh` short-circuit | ⇒ `function / header` |
| 2 | Resolve the generated asset's object path from `Name=` and `Root=` | ⇒ `unresolved` |
| 3 | Look the object up **without loading it** | ⇒ `not compiled` |
| 4 | Load the prepared source, with `import` directives inlined | ⇒ `unresolved` |
| 5 | Hash the prepared source and compare against the asset's recorded source file and hash | mismatch ⇒ `stale` |

Step 2 reads the file, strips every `import` line before parsing — import lines carry no top-level
block and would confuse block detection — and then resolves the asset destination.

| Message | Cause |
| :-- | :-- |
| `Failed to read DreamShader source '{File}'.` | the file could not be read |
| `{File}: {ParserError}` | the file did not parse |
| `{File}: this file does not define a top-level Shader block.` | the parse produced no `Shader` |

### Error overlay

Errors shown on this page are read straight out of
`<Project>/Saved/DreamShader/Bridge/diagnostics.json`, not from the bridge's in-memory store. The
page is deliberately decoupled from the bridge internals.

| Rule | Detail |
| :-- | :-- |
| Accepted severities | an empty `severity`, or `error` compared case-insensitively |
| Formatting | `L{Line}:{Column} {Message}` when the line is greater than zero, otherwise just the message |
| Per file | **only the first accepted diagnostic is kept** |
| Effect | any matching item's status is overwritten to `compile error` |

> [!NOTE]
> A file with five errors shows one. Open the source in the editor extension, or read the Output Log,
> for the full list. The plugin emits exactly one severity — `error` — so no warning or hint ever
> reaches this list.

### Compile actions

Both actions generate **in memory** and force a rebuild, ignoring the source-hash cache.

| Action | Scope | Progress UI | Toast |
| :-- | :-- | :-- | :-- |
| **Compile all** | every listed item except `.dsh` headers — `.dsf` files *are* included | A modal slow task titled "Compiling all DreamShader sources...", one progress frame per file, labelled with the file name | `Compiled {Count} source(s), {Failed} failed` — success only when `{Failed}` is 0 |
| **Compile** (preview pane) | the selected item | none | `Compiled {File}` / `Failed to compile {File}` |

Headers are skipped by *Compile all* because their dependents are in the target set already. A failed
single compile also logs `Material Content Browser compile failed: {Message}` at Error level and pins
the message onto the item, so the preview pane shows why.

### Preview pane

Empty state: "Select a source file to preview its material."

> [!WARNING]
> This pane's image is a **static asset thumbnail**, 160×160, of the already-generated material. It
> is not the streaming renderer, it does not update while you type, and it has no mesh or camera
> control. The live preview is the WebSocket surface described on [Preview](preview.md); it is
> driven by the editor extension, not by this tab.

The placeholder tile reads "function library" for `.dsf` / `.dsh` items and "not compiled yet"
otherwise. The material is resolved by loading the object path, and only for non-function items.

| Button | Tooltip | Shown when |
| :-- | :-- | :-- |
| **Compile** | "Force-recompile this source (in memory)." | always |
| **Create instance** | "Create a material instance of this material." | the item is not a function or header |
| **Open material** | "Open the generated material asset." | the material resolved |
| **Materialize** | "Write this memory-only material (and its base) to disk." | the material resolved **and** it is memory-only |
| **Open source** | "Open the .dsm/.dsf in your preferred editor." | always |

Below the buttons: the file name in large text, the status glyph and label, the absolute source path
in small text, `used by {N} material(s)` for functions and headers, and — only for `compile error`
and `unresolved` — the error detail in red.

*Create instance* on an item that was never compiled first force-compiles it and then re-resolves the
object. If it is still missing, the toast reads `Compile {File} first.`

## Create material instance

Reached from the Project page button, the details panel, the Gen page preview pane, and the Content
Browser entry **Create DreamShader instance**.

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
path — and the window closes. On failure the error is
toasted and **the window stays open** so the name or folder can be corrected.

| Detail | Behaviour |
| :-- | :-- |
| Created class | a plain `UMaterialInstanceConstant`, not a `UDreamShaderMaterialInstance` |
| Object flags | `RF_Public \| RF_Standalone` |
| Parent assignment | set editor-only, followed by a post-edit change and an asset-created broadcast |
| Save failure rollback | the half-created object is un-broadcast, stripped of its flags, renamed into the transient package without redirectors, marked as garbage, and the package's dirty flag is cleared — so it cannot survive GC, appear in the Content Browser, be persisted by *Save All*, or block a same-name retry |

Instancing a memory-only parent materializes the parent first, so a child instance never references a
transient object.

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
| `Materialized {Name} to disk` | success — the details panel re-targets the persisted asset |

The Gen page's *Materialize* button deliberately re-resolves the material by object path when
clicked, rather than holding the pointer captured when the pane was built, so a delete or garbage
collection between build and click cannot crash it.

## Notes

- The two pages are independent: refreshing the Gen page does not touch the Project page's picker,
  and neither page auto-refreshes when a source file changes on disk. Use **Refresh**.
- Status is computed with a non-loading object lookup, so a material that exists on disk but has not
  been loaded in this session reports **not compiled** until something loads it.
- The Gen page's counter counts items, not materials: a `.dsh` header contributes one row.
- Toasts raised from the Gen page expire after 3.5 s; those from the details panel and the instance
  factory after 4 s.

## Example

A source tree and what the Gen page shows for it:

```text
<Project>/DShader/
    Materials/M_Emissive.dsm        ● up to date
    Materials/M_Broken.dsm          ▲ compile error   L12:9 Unknown identifier 'Tin'.
    Materials/M_New.dsm             ○ not compiled    No generated asset at /Game/Materials/M_New.M_New
    Lib/Noise.dsf                   ◆ function / header   function · used by 2 material(s)
    Lib/Common.dsh                  ◆ function / header   function · used by 3 material(s)
    Packages/Sample/Demo.dsm        (not listed — under DShader/Packages)
```

Creating an instance of `/Game/Materials/M_Emissive` with the default settings:

```text
parent      /Game/Materials/M_Emissive
folder      /Game/Materials/Instances          (Material Instance Subfolder = "Instances")
name        MI_M_Emissive                      (uniquified if taken)
object path /Game/Materials/Instances/MI_M_Emissive.MI_M_Emissive
class       UMaterialInstanceConstant
```

## See also

- [Editor integration](editor-integration.md) — the menu entries that open this tab and mirror its toggle
- [Preview](preview.md) — the streaming renderer this tab's thumbnail is not
- [Decompiler](decompiler.md) — going the other way, from an existing asset to a source file
- [In-memory materials](../generation/in-memory.md) — memory-only generation, the hidden base, materializing
- [Caching](../generation/caching.md) — the source hash behind **up to date** and **stale**
- [Asset paths](../generation/asset-paths.md) — how `Name=` and `Root=` resolve to the object path
- [Project settings](../settings/project.md) — *Material Instance Subfolder* and the in-memory visibility toggle
- [Bridge](bridge.md) — `diagnostics.json`, the file this page reads errors from
- [Material instance API](../api/material-instance.md) — `UDreamShaderMaterialInstance` in C++
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
