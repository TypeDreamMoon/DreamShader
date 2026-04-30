# DreamShader ChangeLog

## 1.2.3 - 2026-04-29

### Parameters

- Added declaration metadata `[Group="...", SortPriority=32, Description="..."]` for material `Properties` and function input/output declarations.
- Added explicit Parameter node declarations including `ScalarParameter`, `VectorParameter`, `TextureObjectParameter`, texture sample parameter nodes, `StaticBoolParameter`, and `StaticSwitchParameter`.
- Added `StaticSwitchParameter` graph calls, for example `UseDetail(True=detailColor, False=baseColor)`.
- Added `UE.CollectionParam(Collection=Path(...), Parameter="...")` for Material Parameter Collection reads.

### Function Defaults

- Added `opt` inputs for `ShaderFunction` and `VirtualFunction`.
- Added `default` call arguments for optional material function inputs, preserving Unreal FunctionInput preview defaults.
- Generated `ShaderFunction` assets now write input/output descriptions and sort priorities to FunctionInput / FunctionOutput nodes.
- VirtualFunction copy/create/sync now emits optional inputs, preview defaults, and pin metadata when available.

## 1.2.2 - 2026-04-29

### VirtualFunction Workflow

- `CreateVirtualFunction` now reuses the existing declaration for the selected Material Function instead of creating duplicate `.dsh` files.
- When a matching declaration already exists, the Material Function `DreamShader` menu shows `OpenVirtualFunction` and `Copy Virtual Function Reference` instead of the create/copy-definition actions.
- `OpenVirtualFunction` opens the existing declaration in VSCode and jumps to the declaration location when possible.
- Added startup validation and refresh for `VirtualFunction` declarations under `DShader`, reporting missing source `UMaterialFunction` assets and updating changed signatures.

### Import Compatibility

- `import "File.dsh"` now works with or without a trailing semicolon in the Unreal generator import pass.

## 1.2.1 - 2026-04-29

### Editor Workflow

- Replaced the single Material Function toolbar action with a `DreamShader` dropdown menu.
- Added `CopyVirtualFunction`, `CreateVirtualFunction`, and `CopyVirtualFunctionCall` actions to the Material Function editor toolbar and Material Function asset context menu.
- `CreateVirtualFunction` writes a `.dsh` declaration file under the configured `DShader/VirtualFunctions` directory and opens it in the default external editor.
- `CopyVirtualFunctionCall` copies a ready-to-paste Graph call using the generated input names and first output.
- Added `Open Dream Shader Workspace (VSCode)` to the editor Tools menu and DreamShader toolbar section. It writes `DShader/DreamShader.code-workspace`, opens it in VSCode when available, and falls back to the default editor or Notepad.

### Release

- Added a GitHub Actions release workflow that packages the plugin source and publishes a GitHub Release from version tags or manual workflow dispatch.

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
