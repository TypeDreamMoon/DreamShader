<p align="center">
  <img alt="DreamShader banner" src="./Images/banner.png" />
</p>

<table>
  <tr>
    <td width="64%" valign="top">
      <h1>DreamShader</h1>
      <p><strong>Text-first Unreal Engine material authoring with DreamShaderLang.</strong></p>
      <p>
        DreamShader turns <code>.dsm</code>, <code>.dsf</code>, and <code>.dsh</code> source files into Unreal
        <code>UMaterial</code>, <code>UMaterialFunction</code>, Material Layer, and Material Layer Blend assets.
      </p>
      <p>
        <img alt="Unreal Engine 5.3-5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.3--5.8-313131" />
        <img alt="Version 1.5.0 - Beta" src="https://img.shields.io/badge/version-1.5.0%20--%20Beta-orange" />
        <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-green" />
      </p>
      <p>
        <a href="README.zh-CN.md">中文文档</a> |
        <a href="Docs/index.md">Documentation</a> |
        <a href="Docs/language/index.md">Language Reference</a> |
        <a href="Docs/examples/index.md">Examples</a> |
        <a href="Docs/tools/packages.md">Packages</a> |
        <a href="Docs/tools/workspace.md">VSCode</a> |
        <a href="CHANGELOG.md">Changelog</a>
      </p>
      <p>
        <a href="https://github.com/TypeDreamMoon/DreamShader/issues">
          <img alt="Issues" src="https://img.shields.io/github/issues/TypeDreamMoon/DreamShader" />
        </a>
        <a href="https://github.com/TypeDreamMoon/dreamshader-language-support/releases">
          <img alt="VSCode Extension" src="https://img.shields.io/badge/VSCode-DreamShaderLang-007ACC" />
        </a>
        <a href="https://github.com/tsdaer/dreamshader-language-support">
          <img alt="Rider Plugin" src="https://img.shields.io/badge/Rider-DreamShaderLang-7F52FF" />
        </a>
      </p>
      <p><strong>QQ group:</strong> <a href="https://qm.qq.com/q/X9uCLjVcY">466585194</a></p>
    </td>
    <td width="36%" align="center" valign="middle">
      <img src="./Images/character.png" width="260" alt="DreamShader character" />
    </td>
  </tr>
</table>

> [!NOTE]
>
> **`1.5.0` is a Beta release.** The unified ThinCustom backend and the new Material Content Browser are render-verified, but generated-asset layout and some APIs may still change before the stable `1.5.0`. See the [changelog](CHANGELOG.md) for the full feature list.

> [!TIP]
>
> DreamShader is actively developed against Unreal Engine `5.8` and has been verified with single-plugin `BuildPlugin` builds on Unreal Engine `5.3`, `5.4`, `5.5`, `5.6`, `5.7`, and `5.8`.
>
> Keep all `.dsm`, `.dsf`, and `.dsh` source files in version control. The generated Unreal assets can always be rebuilt from source.
>
> The decompiler is a migration helper. It handles many common materials and some large Lyra cases, but it is not intended to be a perfect round-trip system yet.

## Overview

<p align="center">
  <img alt="DreamShader workflow overview" src="./Images/workflow-overview.png" />
</p>

| Workflow | Language | Tooling |
| :------- | :------- | :------ |
| Generate `UMaterial` from `Shader`. | Use `Graph = { ... }` as a node-oriented material DSL. | Auto-compile on save in the Unreal Editor. |
| Generate `UMaterialFunction` from `ShaderFunction`. | Write HLSL-style helpers with `Function`, `GraphFunction`, and `Namespace`. | Open generated VSCode workspaces from the editor toolbar. |
| Generate native Material Layer and Layer Blend functions. | Use typed `Properties` for scalars, vectors, textures, switches, MPC values, and reflected node settings. | Export existing materials and material functions to starter `.dsm` and `.dsf` files. |
| Reference existing Unreal material functions with `VirtualFunction`. | Pass `Texture2D`, `Texture2DArray`, `VolumeTexture`, and `MaterialAttributes` values through graph code. | Use the VSCode extension or Rider plugin for highlighting, completion, navigation, diagnostics, and package tools. |

## Quick Start

1. Copy this plugin into your Unreal project:

   ```text
   MyProject/Plugins/DreamShader/
   ```

2. Enable `DreamShader` in Unreal Editor and restart the editor.
3. Create a source directory in the project root:

   ```text
   MyProject/DShader/
   ```

4. Add a material source file, for example:

   ```text
   MyProject/DShader/Materials/M_Minimal.dsm
   ```

5. Save the file. DreamShader parses the source and generates or updates the target Unreal asset.

Project settings are available under `Project Settings > DreamPlugin > Dream Shader`.

Recommended layout:

```text
MyProject/
|- DShader/
|  |- Materials/
|  |  `- M_Sample.dsm
|  |- Shared/
|  |  `- Common.dsh
|  `- Packages/
`- Plugins/
   `- DreamShader/
```

## Compatibility

DreamShader targets Unreal Engine 5 and is currently verified on Windows with single-plugin `RunUAT BuildPlugin` builds:

| Unreal Engine | Status |
| :------------ | :----- |
| `5.8` | Verified |
| `5.7` | Verified |
| `5.6` | Verified |
| `5.5` | Verified |
| `5.4` | Verified |
| `5.3` | Verified |

Use Unreal's plugin packaging command when you want to validate the plugin without building a full project target:

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

On Windows, UE `5.3` and `5.4` may require the MSVC `14.38` toolchain. Newer compiler toolchains can fail while compiling older engine headers before plugin code is reached.

## Minimal Material

```c
Shader(Name="DreamMaterials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

`Root` is optional and defaults to `Game`. To generate into a project content plugin, use `Root="Plugin.MyPlugin"`:

```c
Shader(Name="DreamMaterials/M_Minimal", Root="Plugin.MyPlugin")
{
    // ...
}
```

This produces `/MyPlugin/DreamMaterials/M_Minimal.M_Minimal` and saves the asset under `[Project]/Plugins/MyPlugin/Content/DreamMaterials/M_Minimal.uasset`.

## Language Model

<p align="center">
  <img alt="DreamShader language model" src="./Images/language-model.png" />
</p>

| Item | Purpose |
| :--- | :------ |
| `.dsm` | Material implementation file. Usually contains `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, or `VirtualFunction` blocks. |
| `.dsf` | Dream Shader Function file. Generates reusable `ShaderFunction`, `ShaderLayer`, and `ShaderLayerBlend` assets that can be imported by `.dsm` files. |
| `.dsh` | Shared header file. Usually contains `import`, `Function`, `GraphFunction`, `Namespace`, and `VirtualFunction` declarations. |
| `Shader` | Generates an Unreal `UMaterial`. |
| `ShaderFunction` | Generates an Unreal `UMaterialFunction`. |
| `ShaderLayer` | Generates a native `UMaterialFunctionMaterialLayer`. |
| `ShaderLayerBlend` | Generates a native `UMaterialFunctionMaterialLayerBlend`. |
| `VirtualFunction` | Describes an existing Unreal `UMaterialFunction` so it can be called from `Graph`. |
| `Graph` | Node-oriented DSL used inside generated materials and functions. |
| `Function` | Reusable HLSL-style helper. |
| `GraphFunction` | Reusable Custom-node helper that can pull `UE.*` material nodes into generated Custom inputs. |
| `Namespace` | Groups helpers, for example `Texture::Sample2DRGB(...)`. |
| `Path(...)` | Declares Unreal asset paths for textures, object settings, or virtual functions. |

## Properties

`Properties` can use compact types such as `float`, `float3`, `Texture2D`, `Texture2DArray`, and `VolumeTexture`, or explicit Unreal parameter node types. Reflected Unreal expression properties can be written in a trailing `[...]` block.

```c
Properties = {
    ScalarParameter Roughness = 0.35 [
        Group="Surface";
        SortPriority=10;
        Description="Material roughness";
    ];

    VectorParameter Tint = float4(1.0, 0.9, 0.8, 1.0) [
        Group="Surface";
        SortPriority=20;
    ];

    StaticSwitchParameter UseDetail = true [
        Group="Switches";
        SortPriority=30;
    ];

    TextureSampleParameter2D Albedo = Path(Game, "Textures/T_Albedo") [
        Group="Textures";
        SamplerType="Color";
    ];
}
```

Material Parameter Collections can be read directly from graph code:

```c
Graph = {
    float wind = UE.CollectionParam(
        Collection=Path(Game, "MaterialParameterCollections/MPC_Global"),
        Parameter="WindStrength");
}
```

## Graph And Helpers

`Graph = { ... }` is the material-node DSL. Use it for variable declarations, assignments, constructors, `UE.*` material nodes, DreamShader helper calls, generated or virtual material function calls, and simple graph branches.

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float pulse = UE.Expression(Class="Sine", OutputType="float1", Input=UE.Time());
    Color = vec3(pulse, pulse, pulse);
}
```

`Function` is HLSL-style reusable code. It is better for calculations, loops, and logic that should live inside a Custom node.

```c
Namespace(Name="Color")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

Then import and call it:

```c
import "Shared/Color.dsh";

Graph = {
    Color::ApplyTint(BaseColor, Tint, Color);
}
```

`GraphFunction` is also generated as a Custom node, but it can reference `UE.*` calls in the body. DreamShader creates those Unreal material nodes and wires their outputs into the Custom node as generated inputs.

## Material Workflows

### MaterialAttributes

`MaterialAttributes` can be used as a graph value, a function output, a virtual function output, and a material output binding. When a `Shader` binds to `Base.MaterialAttributes`, DreamShader automatically enables Unreal's `Use Material Attributes` option on the generated material.

```c
Outputs = {
    MaterialAttributes Attrs;
    Base.MaterialAttributes = Attrs;
}

Graph = {
    Attrs.BaseColor = Color;
    Attrs.Roughness = Roughness;
}
```

### Substrate

`Substrate` can be used as a graph value, a `.dsf` / `ShaderFunction` / `VirtualFunction` input or output, and a material output binding through `Base.FrontMaterial`. Binding `Base.FrontMaterial` sets the generated material to the Substrate shading model unless `ShadingModel="Substrate"` / `"Strata"` is already set.

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}

Graph = {
    Surface = Substrate.Unlit(EmissiveColor=Color);
}
```

Use `Substrate.*` wrappers for UE 5.7 Substrate nodes such as `Unlit`, `Slab`, `ConvertMaterialAttributes`, `VerticalLayer`, `Add`, `Weight`, `Select`, and utility nodes like `ThinFilm`. Generic `UE.Expression(..., OutputType="Substrate")` is also supported for non-Custom material expressions.

### Material Layers

`ShaderLayer` and `ShaderLayerBlend` generate native Unreal Material Layer function assets.

```c
ShaderLayer(Name="Layers/L_SimpleSurface")
{
    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = vec3(0.8, 0.2, 0.1);
        Attrs.Roughness = 0.5;
    }
}
```

Current rules:

- `ShaderLayer` creates `UMaterialFunctionMaterialLayer`.
- `ShaderLayerBlend` creates `UMaterialFunctionMaterialLayerBlend`.
- Both reuse the `ShaderFunction` sections: `Properties`, `Inputs`, `Outputs`, `Settings`, and `Graph`.
- `ShaderLayer` may declare at most one input, and that input must be `MaterialAttributes`; use `Properties` for layer controls.
- Both must declare exactly one `MaterialAttributes` output.
- `ShaderLayerBlend` must declare exactly two inputs, both `MaterialAttributes`; use `Properties` for blend controls.
- Legacy `MaterialLayer(...)` and `MaterialLayerBlend(...)` syntax still parses, but emits deprecation warnings.

This is native layer-function generation. Full Material Layer Stack and Layer Instance workflow support is still on the roadmap.

### VirtualFunction

Use `VirtualFunction` to expose an existing Unreal `UMaterialFunction` to DreamShader without generating, saving, or overwriting that asset.

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset = Path(Plugins.MoonToon, "MaterialFunctions/Buffer/Writer");
    }

    Inputs = {
        float3 Color;
        float Alpha;
    }

    Outputs = {
        float3 Result;
    }
}

Graph = {
    Result = BufferWriter(Tint, 1.0, Output="Result");
}
```

The Unreal Material Function asset context menu and editor toolbar include DreamShader actions for copying virtual function definitions, creating `.dsh` declarations, opening existing declarations, and copying call examples.

## Editor Tools

<p align="center">
  <img alt="DreamShader editor tools" src="./Images/editor-tools.png" />
</p>

### Material Content Browser

`Tools > DreamShader > Material Content Browser` opens a dedicated tab for managing and creating DreamShader materials, split into two pages:

- **Project** — browse, filter, and inspect every material and material instance under `/Game`, with the full inheritance chain (hidden base → root instance → variants) and one-click **Create instance** into a folder you pick.
- **Dream Shader Gen** — list your `.dsm` sources with a live preview, search and filters, compile-all, and load-time error surfacing.

It can also materialize memory-only (preview) materials to disk, and toggle whether DreamShader's memory-only materials appear in the Content Browser.

### Decompiler Export

Right-click a `Material` or `Material Function` in the Content Browser and choose `DreamShader > Export DSM/DSF`. Exported files are written to `DShader/Decompiled/Materials` or `DShader/Decompiled/Functions` and opened in your preferred text editor.

The exporter is intended as a migration starting point: common parameters, constants, arithmetic, swizzles, texture samples, Custom nodes, and MaterialFunctionCall nodes are emitted as DreamShader graph text. Less common reflected nodes fall back to `UE.Expression(...)` so the graph structure remains regeneratable.

### Packages

DreamShader packages are reusable `.dsh` libraries installed under:

```text
DShader/Packages/@scope/package-name/
```

Import example:

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

See [Docs/tools/packages.md](Docs/tools/packages.md) for package structure, lock files, install commands, and package authoring.

### Editor Language Plugins

DreamShaderLang editor support is available for both VSCode and JetBrains Rider.

| Editor | Repository | Main features |
| :----- | :--------- | :------------ |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases) | Syntax highlighting, snippets, completion, Go to Definition, Find References, Hover, Signature Help, local diagnostics, Unreal bridge diagnostics, package commands, and quick templates. |
| Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` file types, grammar and PSI parsing, highlighting, completion, navigation, diagnostics, Unreal Bridge integration, semantic tokens, inlay hints, and package tools. |

The Unreal editor menu `Tools > DreamShader` and the DreamShader toolbar can open the generated `DShader/DreamShader.code-workspace` in VSCode.

Extension releases are available from [dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases).

## Configuration

| Setting | Default | Description |
| :------ | :------ | :---------- |
| `SourceDirectory` | `DShader` | Root directory for DreamShader source files. |
| `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | Output directory for generated `.ush` helper files. |
| `AutoCompileOnSave` | `true` | Rebuild affected assets when `.dsm`, `.dsf`, or `.dsh` files are saved. |
| `SaveDebounceSeconds` | `0.25` | File-save debounce time. |
| `VerboseLogs` | `false` | Print more detailed logs. |
| `OpenInNewWindow` | `true` | Open the generated VSCode workspace in a new window by default. |

## Release

The repository includes a GitHub Actions release workflow. Push a tag whose version matches `DreamShader.uplugin`. Stable releases use `vX.Y.Z`; Beta releases (`IsBetaVersion: true`, or a `VersionName` ending in `Beta`) use a `b` suffix and are published as GitHub pre-releases automatically:

```powershell
# Stable, VersionName "1.5.0"
git tag v1.5.0
git push origin v1.5.0

# Beta, VersionName "1.5.0 - Beta"
git tag v1.5.0b
git push origin v1.5.0b
```

The release archive is named `DreamShader-<slug>.zip` (e.g. `DreamShader-1.5.0b.zip`) and contains the plugin source, resources, documentation, README, CHANGELOG, and LICENSE. It excludes `Binaries` and `Intermediate`. The release workflow also attaches the latest VSCode extension assets from `TypeDreamMoon/dreamshader-language-support`, and builds the release notes from the matching `CHANGELOG.md` section.

## Project Info

| Item | Value |
| :--- | :---- |
| Version | `1.5.0 - Beta` |
| Language | `DreamShaderLang` |
| Unreal Engine | `5.3` - `5.8` |
| Author | TypeDreamMoon |
| GitHub | <https://github.com/TypeDreamMoon> |
| Docs | <https://lang.64hz.cn/> |
| Web | <https://dev.64hz.cn> |
| License | [MIT](LICENSE) |
| Copyright | Copyright (c) 2026 TypeDreamMoon. All rights reserved. |

## Roadmap

- Custom full-screen render pass support.
- More complete VSCode semantic diagnostics.
- Deeper Material Layer workflow support.
- Deeper Moon Engine integration. Reference: <https://zhuanlan.zhihu.com/p/21979494450>

## License

DreamShader is released under the [MIT license](LICENSE).

For bug reports and feature requests, open an [issue](https://github.com/TypeDreamMoon/DreamShader/issues/new).
