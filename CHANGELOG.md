# DreamShader ChangeLog

## 1.2.1 - 2026-04-29

### Editor Workflow

- Replaced the single Material Function toolbar action with a `DreamShader` dropdown menu.
- Added `CopyVirtualFunction`, `CreateVirtualFunction`, and `CopyVirtualFunctionCall` actions to the Material Function editor toolbar and Material Function asset context menu.
- `CreateVirtualFunction` writes a `.dsh` declaration file under the configured `DShader/VirtualFunctions` directory and opens it in the default external editor.
- `CopyVirtualFunctionCall` copies a ready-to-paste Graph call using the generated input names and first output.

## 1.2.0 - 2026-04-28

### VirtualFunction

- Added `VirtualFunction(Name="...")` declarations for existing Unreal `UMaterialFunction` assets.
- `VirtualFunction` calls can be used from `Graph` like `ShaderFunction` calls, without generating or overwriting the referenced asset.
- `Options.Asset` supports `Path(Game, "...")`, `Path(Engine, "...")`, `Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`, and full Unreal object paths.
- Added Material Function context-menu and Material Editor toolbar actions that copy a complete `VirtualFunction` declaration with inputs, outputs, and options.

### Asset Roots

- Kept `Root="Plugin.PluginName"` mapped to the project plugin content root, physically saving generated assets under `[Project]/Plugins/PluginName/Content`.
- `Plugins.PluginName` and `Plugins/PluginName` remain compatibility spellings.

### Tooling

- Updated the VSCode extension language service for `VirtualFunction`, plugin path completion inside `Path(Plugins.)`, snippets, hover text, signatures, and diagnostics.
- Updated plugin documentation for DreamShader `1.2.0`.
