---
title: Installation
description: Add the DreamShader plugin to an Unreal project, find its settings, and confirm the compile chain works.
---

DreamShaderLang is compiled by the DreamShader Unreal plugin. Everything on this page is done once per project.

| | |
| --- | --- |
| Repository | [TypeDreamMoon/DreamShader](https://github.com/TypeDreamMoon/DreamShader) |
| Plugin version | `2.2.0` |
| Engines | Unreal Engine `5.3` – `5.8`, Win64; Custom Pass needs `5.8` |
| Location | `<Project>/Plugins/DreamShader` |
| Plugin dependencies | `WebSocketNetworking`, `SQLiteCore` — both engine plugins, both enabled automatically |

## Install

### Copy the plugin into the project

Download `DreamShader-<version>.zip` from a GitHub release — its single top-level folder is `DreamShader` — or clone the
repository. Put it at:

```text
MyProject/
└─ Plugins/
   └─ DreamShader/
      └─ DreamShader.uplugin
```

Keep the folder name `DreamShader`; the module names, the log category and every path in these docs assume it.

What the release archive holds:

| Content | Notes |
| --- | --- |
| `Source/`, `Resources/`, `DreamShader.uplugin` | the plugin itself; `Resources/Templates` holds the source templates of *New ▾* |
| `Shaders/` | `DreamShaderBuiltins.ush` (so `/Plugin/DreamShader/…` resolves in an archive install), and the Custom Pass shaders (`Pass/`) |
| `Content/` (since 2.1.2) | `Localization/DreamShader/` — the `.locmeta` and the `zh-Hans` `.locres`, the editor UI's Chinese translation |
| `Docs/`, `.skill/` | the complete English manual; the agent skills and their driver `dsc.ps1` |
| `README.md`, `README.zh-CN.md`, `CHANGELOG.md`, `LICENSE` | |

**Note:**

The archives from 1.6.0 to 2.1.1 did not carry `Content/`, so an archive install showed the editor UI in English only.
To get the translation on an older version, copy `Content/Localization/` in from the matching tag. `Tests/`, `Tools/`
and `Config/` are not in the archive.

### Enable it and restart

Enable **DreamShader** in *Edit ▸ Plugins* and restart the editor. The plugin descriptor enables `WebSocketNetworking`
and `SQLiteCore` automatically — the first serves the live-preview WebSocket, the second backs the editor bridge's
`bridge.db`. Only `DreamShaderEditor` uses either.

There are five modules: `DreamShaderLang`, `DreamShader` and `DreamShaderPass` (Runtime, loaded at `PostConfigInit`),
and `DreamShaderCompiler` and `DreamShaderEditor` (Editor, at `Default`). Only the two Editor modules generate assets;
`DreamShaderPass` is the Custom Pass runtime, built on every engine, running passes on 5.8 only. See
[About DreamShader](about.md#the-five-modules).

### Confirm the settings page

Open *Project Settings* and find:

```text
Project Settings ▸ DreamPlugin ▸ Dream Shader
```

**Warning:**

The settings live under the **DreamPlugin** category, **not** *Plugins*. If the page is missing, the plugin did not
load — check the Output Log for `LogDreamShader`. The Custom Pass settings are a separate page in the same category,
*DreamShader Custom Pass*.

### Let the plugin create its directories

On the first editor start, the runtime modules create the working directories whether or not you have written a single
source file:

```text
<Project>/DShader/                                        source root
<Project>/DShader/Packages/                               installed shared libraries
<Project>/Intermediate/DreamShader/GeneratedShaders/      mapped to /DreamShaderGenerated
```

`DShader` comes from the *Source Directory* setting; `Packages` is always the literal subfolder `Packages` under it and
cannot be configured on its own. `GeneratedShaders` is still created and mapped, but (since 2.0.0) no
compile writes there — a `Function`'s HLSL goes straight into its Custom node. On UE 5.8, `DreamShaderPass` also keeps `.dreampass/` under the source root — the
snapshots of Custom Pass HLSL, which belong in version control next to the sources.

### Write a source file and save it

Write new sources as `.dss`:

```hlsl
// <Project>/DShader/Materials/M_InstallCheck.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

export void M_InstallCheck(inout material m)
{
    m.EmissiveColor = float3(0.2, 0.6, 1.0);
}
```

Or in 1.x syntax, which builds the same asset — keep one of the two files:

```c
// <Project>/DShader/Materials/M_InstallCheck.dsm
Shader(Name="Materials/M_InstallCheck")
{
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(0.2, 0.6, 1.0);
    }
}
```

With *Auto Compile On Save* on (the default), the file watcher picks up the save and generates the material after the
debounce. The Output Log shows:

```text
Generated Material /Game/Materials/M_InstallCheck from .../M_InstallCheck.dss.
```

Or skip the typing: *Tools ▸ DreamShader ▸ Material Content Browser ▸ New ▾ ▸ Material (.dss)* writes the same shape
from a template.

## Verify the installation

| Check | Expected |
| --- | --- |
| The plugin loaded | *Project Settings ▸ DreamPlugin ▸ Dream Shader* exists |
| The directories exist | `DShader/` and `DShader/Packages/` under the project root |
| A source compiles | the Output Log prints `Generated Material /Game/Materials/M_InstallCheck from …` |
| The material really exists | in Sources mode of *Tools ▸ DreamShader ▸ Material Content Browser*, the row reads **up to date** |
| The editor extension is wired up | `Saved/DreamShader/Bridge/` holds `status.json`, `diagnostics.json`, `material-expressions.json`, `dreamshader-builtin-catalog.json` and more |
| Chinese UI (optional) | with the editor language set to Simplified Chinese, DreamShader's menus and panels are translated — needs `Content/` from the archive |

**Warning:**

Do not expect the new material in the Content Browser. Under the default `ThinCustom` backend it is **Ephemeral** —
in memory only — and hides itself from the Content Browser, asset pickers and save dialogs. See
[Your first material](first-material.md).

## The settings you will touch first

The full panel has 17 properties; these are the ones you meet on day one. Note that the config identifiers keep their
`b` prefix while the display names drop it.

| Display name | Config property | Default | Effect |
| --- | --- | --- | --- |
| Source Directory | `SourceDirectory` | `DShader` | The root scanned for source files. Relative paths resolve against the project directory. |
| Generated Shader Directory | `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | Where the virtual directory `/DreamShaderGenerated` is mapped. Since 2.0 no compile writes a `.ush`; a file there was left by a 1.x build. |
| Default Compiler Backend | `DefaultBackend` | `ThinCustom` | The backend of a source that names none. |
| Show Ephemeral Materials | `bShowEphemeralMaterials` | `false` | Makes Ephemeral materials visible in the Content Browser and asset pickers, like unsaved assets. |
| Auto Compile On Save | `bAutoCompileOnSave` | `true` | When off, the source directory watcher ignores file changes entirely. |
| Save Debounce Seconds | `SaveDebounceSeconds` | `0.25` | How long to wait after a change before compiling. Clamped to `[0.05, 10.0]`. |

The values go to the **project's** `Config/DefaultEngine.ini`, section `[/Script/DreamShader.DreamShaderSettings]`, so
everyone who pulls the project shares them. All 17 settings are on [Project settings](../settings/project.md).

**Warning:**

`GeneratedShaderDirectory` is read only while the `/DreamShaderGenerated` virtual shader mount is not yet registered.
Changing it mid-session does not move the output — restart the editor.

## Where the asset lands

A `.dss` asset lands under `/Game` at the file's folder relative to the source root, named after its `export`
function: `DShader/Materials/M_Minimal.dss` builds `/Game/Materials/M_Minimal`. A `/// @name` above the entry names it
otherwise.

In 1.x syntax the block header decides: `Shader(Name="Materials/M_Minimal")` resolves to `/Game/Materials/M_Minimal`.
`Name` is the package path relative to the root, without a `/Game` prefix of its own. `Root` is optional and defaults
to `Game`:

```c
Shader(Name="Materials/M_Minimal", Root="Plugin.MyPlugin")
```

This generates `/MyPlugin/Materials/M_Minimal`, physically
`<Project>/Plugins/MyPlugin/Content/Materials/M_Minimal.uasset`. Only **project** plugins under `<Project>/Plugins` are
accepted; engine and marketplace plugins are refused (`DSH8090`). The full dispatch rules are on
[Asset Paths](../generation/asset-paths.md).

## Engine compatibility

DreamShader `2.2.0` is developed against Unreal Engine `5.8` and verified on Win64 with a single-plugin
`RunUAT BuildPlugin`:

| Unreal Engine | Status |
| --- | --- |
| `5.8` | verified (`2.2.0`); the only engine Custom Pass runs on |
| `5.7` | verified through `1.8.0`; the engine gates added since ask the engine's own types, so 5.7 needs no answer of its own |
| `5.6` | verified (`2.2.0`) |
| `5.5` | verified (`2.2.0`) |
| `5.4` | source-compatible — see the toolchain warning below |
| `5.3` | source-compatible — see the toolchain warning below |

To check that the plugin builds against an engine without building a full project target:

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

`-Package` must point outside the source tree — UAT stages a clean copy there.

**Warning:**

**UE `5.3` and `5.4` do not build under a recent MSVC toolchain, whatever plugin you point at them.** The engine's
`ConcurrentLinearAllocator.h` writes `#elif __has_feature(address_sanitizer)` without a `defined()` guard, which raises
`C4668` on a toolchain that ships the ASan headers, and UBT compiles engine code with `/we4668`. The failure is in
**engine** code, and `Source/DreamShader*` is named nowhere in it — Epic's own `ExampleDeviceProfileSelector` fails the
same way on the same machine, which confirms the cause. Installing MSVC `14.38` does not by itself help (it ships the
ASan headers too); it takes a toolchain without them, or a one-line patch to the engine header. UE 5.5 fixed the
header.

Feature availability varies by engine: Substrate needs (since UE 5.4), a few transform bases and parameters need
`5.5` or `5.6`, and Custom Pass needs (since UE 5.8). The plugin binary is built per engine, so moving a project
to a newer engine means rebuilding the plugin. The full list is on
[About DreamShader ▸ Engine support](about.md#engine-support).

## Editor extensions

The language service is not in the plugin. For highlighting, completion, diagnostics and previews, install one of:

| Editor | Repository |
| --- | --- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support) — currently 2.1.0, with `.dss`, `.dsi`, `.dsp` and the 1.x sources |
| JetBrains Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) |

*Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* writes `DShader/DreamShader.code-workspace`, refreshes the
bridge manifests and opens it in VSCode. See [VSCode and Rider](../tools/vscode.md).

## Next

- [Your first material](first-material.md) — write one, then find it
- [Project layout](project-layout.md) — how `DShader/` is organized
- [Daily workflow](workflow.md) — the save, debounce, generate loop
- [About DreamShader](about.md) — what the plugin is made of
