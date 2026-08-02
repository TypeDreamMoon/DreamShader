# Tools

> [DreamShader](../index.md) » **Tools**

The editor-side surface of DreamShader: the menus, the docked browser tab, the preview renderer, the
decompiler, the VirtualFunction actions, the on-disk bridge, and the headless commandlet.

| | |
| :-- | :-- |
| Implemented in | the `DreamShaderEditor` module (type `Editor`, loading phase `Default`) |
| Namespace | `UE::DreamShader::Editor::Private` — there is no public editor header |
| Plugin dependencies | `WebSocketNetworking` (preview streaming), `SQLiteCore` (bridge database) |
| Disabled by | the `-NoDreamShaderEditorBridge` command-line switch, and by any commandlet run |

Everything on this page runs inside `UnrealEditor.exe`. None of it is present in a packaged game: the
module is editor-only, and the runtime `DreamShader` module carries no UI.

## Pages

| Page | Covers |
| :-- | :-- |
| [Editor integration](editor-integration.md) | every menu entry, toolbar button, context-menu action and tab spawner, with labels, tooltips, icons and effects |
| [Material Content Browser](material-browser.md) | the docked browser tab: the Project page, the Dream Shader Gen page, the instance factory and *Materialize* |
| [Preview](preview.md) | the thumbnail renderer, the streaming WebSocket preview, the mesh set and the limits |
| [Decompiler](decompiler.md) | exporting an existing `UMaterial` / `UMaterialFunction` back to `.dsm` / `.dsf` |
| [VirtualFunction tools](virtual-function-tools.md) | the conditional VirtualFunction context menu and the startup sync service |
| [Workspace](workspace.md) | the generated `DreamShader.code-workspace`, VSCode discovery and launch, the exported manifests |
| [Packages](packages.md) | `DShader/Packages`: what the plugin implements and what it does not |
| [Commandlet](commandlet.md) | `-run=DreamShader` — headless compile and decompile |
| [Bridge](bridge.md) | request files, the diagnostics sinks, `bridge.db`, the WebSocket protocol |

## Which side implements what

DreamShader ships as two independent products that talk through files in
`<Project>/Saved/DreamShader/Bridge/` and one loopback WebSocket. This manual documents the Unreal
plugin. Editor-extension behaviour is named only where the plugin's contract depends on it, and is
labelled as such.

| Surface | Implemented by | Notes |
| :-- | :-- | :-- |
| Menus, toolbar, context menus, tab | Unreal plugin | [Editor integration](editor-integration.md) |
| Auto-compile-on-save, diagnostics store | Unreal plugin | writes three diagnostic sinks; see [Bridge](bridge.md) |
| Preview rendering (PNG frames) | Unreal plugin | the renderer, the mesh set and the clamps are plugin-side — [Preview](preview.md) |
| Preview camera control, pitch clamping, frame acknowledgement | editor extension | the plugin applies no pitch clamp of its own |
| Decompiler | Unreal plugin | [Decompiler](decompiler.md) |
| `.dsm` / `.dsh` / `.dsf` syntax highlighting, completion, hovers | editor extension | fed by the manifests the plugin exports — [Workspace](workspace.md) |
| `DreamShader.code-workspace` file | Unreal plugin | rewritten on every *Open Dream Shader Workspace* |
| Package manifest (`dreamshader.package.json`), lock file, install/update commands | editor extension | **no plugin C++ reads either file** — [Packages](packages.md) |
| `DShader/Packages` directory creation, import resolution, auto-compile exclusion | Unreal plugin | [Packages](packages.md) |

The editor extension is published separately as `TypeDreamMoon/dreamshader-language-support`; its
settings, commands and package store are outside the scope of this manual.

## Startup order

The bridge and the browser tab are registered from `StartupModule`. Skipping any of it also skips
everything below it.

| # | Step | Skipped when |
| :-- | :-- | :-- |
| 1 | If running a commandlet: install the cook hook when `-run=` contains `Cook` and `-cookworker` is absent, then stop | — |
| 2 | Bail out entirely | `-NoDreamShaderEditorBridge` is on the command line |
| 3 | Create and start the editor bridge | as above |
| 4 | Register the Material Content Browser nomad tab and its menu entries | as above |

The bridge's own startup then resets `bridge.db`, exports the three manifests, runs the
[VirtualFunction sync service](virtual-function-tools.md#startup-sync-service), queues a full scan,
opens the [preview WebSocket server](preview.md#streaming-preview) on port `17864`, registers the
source-directory watcher, and installs the menus.

## Notes

- Menu registration is idempotent and bails during editor shutdown. Bridge menus are owned by the
  ToolMenu owner `DreamShaderEditor`; the browser tab's own entries use `DreamShaderMaterialBrowser`.
- Every editor log line goes to the `LogDreamShader` category.
- Toasts raised by the bridge expire after 4 seconds; toasts raised from the Dream Shader Gen page
  expire after 3.5 seconds.
- The editor never writes a per-material `.uasset` on its own. See
  [In-memory materials](../generation/in-memory.md).

## See also

- [Editor integration](editor-integration.md) — the complete menu and command surface
- [Project settings](../settings/project.md) — the thirteen settings these tools read
- [Generation](../generation/index.md) — what a compile actually does
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
- [Getting started](../getting-started.md) — the first-run walkthrough
