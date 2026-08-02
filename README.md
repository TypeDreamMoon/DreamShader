<p align="center">
  <img alt="DreamShader banner" src="./Images/banner.png" />
</p>

<table>
  <tr>
    <td width="64%" valign="top">
      <h1>DreamShader</h1>
      <p><strong>Text-first Unreal Engine material authoring with DreamShaderLang.</strong></p>
      <p>
        DreamShader compiles <code>.dsm</code>, <code>.dsf</code> and <code>.dsh</code> source files into standard
        Unreal <code>UMaterial</code>, <code>UMaterialFunction</code>, Material Layer and Material Layer Blend
        assets. The source file is the authoring surface; the asset is build output, and can always be thrown
        away and regenerated.
      </p>
      <p>
        <img alt="Unreal Engine 5.3-5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.3--5.8-313131" />
        <img alt="Version 1.5.1" src="https://img.shields.io/badge/version-1.5.1-blue" />
        <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-green" />
      </p>
      <p>
        <a href="README.zh-CN.md">中文文档</a> &nbsp;·&nbsp;
        <a href="Docs/index.md">Documentation</a> &nbsp;·&nbsp;
        <a href="Docs/getting-started.md">Getting started</a> &nbsp;·&nbsp;
        <a href="Docs/language/index.md">Language reference</a> &nbsp;·&nbsp;
        <a href="Docs/examples/index.md">Examples</a> &nbsp;·&nbsp;
        <a href=".skill/README.md">AI skills</a> &nbsp;·&nbsp;
        <a href="CHANGELOG.md">Changelog</a>
      </p>
      <p>
        <a href="https://github.com/TypeDreamMoon/DreamShader/issues">
          <img alt="Issues" src="https://img.shields.io/github/issues/TypeDreamMoon/DreamShader" />
        </a>
        <a href=".skill/README.md">
          <img alt="Agent skills" src="https://img.shields.io/badge/Agent%20skills-5-8A2BE2" />
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

> [!TIP]
> Keep every `.dsm`, `.dsf` and `.dsh` file in version control. The generated Unreal assets can
> always be rebuilt from source, so they do not need to be.

---

## What it looks like

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

Save the file and DreamShader builds `/Game/DreamMaterials/M_Minimal`. `Name` is the asset path
relative to `Root`, which defaults to `Game`; `Root="Plugin.MyPlugin"` generates into a content
plugin instead.

By default materials are generated **in memory** — no `.uasset` is written and nothing appears in
the Content Browser, because the source file is what you edit. Cooking materialises them
automatically, and you can materialise one by hand from the Material Content Browser.

<p align="center">
  <img alt="DreamShader workflow overview" src="./Images/workflow-overview.png" />
</p>

## Quick start

1. Copy the plugin into your project, enable **DreamShader** in *Edit ▸ Plugins*, and restart the
   editor. The engine plugins `WebSocketNetworking` and `SQLiteCore` are enabled automatically.
2. Create the source directory in the project root and add a `.dsm` file:

   ```text
   MyProject/
   ├─ DShader/
   │  ├─ Materials/   *.dsm   material implementations
   │  ├─ Functions/   *.dsf   reusable material function assets
   │  ├─ Shared/      *.dsh   headers: Function, GraphFunction, Namespace, VirtualFunction
   │  └─ Packages/           installed shared libraries
   └─ Plugins/
      └─ DreamShader/
   ```

3. Save it. With *Auto Compile On Save* on — the default — the source is parsed after a short
   debounce and the asset is built.

Settings live under *Project Settings ▸ DreamPlugin ▸ Dream Shader*; every key and its default is on
[Project settings](Docs/settings/project.md). The full walkthrough is
[Getting started](Docs/getting-started.md).

## What it generates

<p align="center">
  <img alt="DreamShader language model" src="./Images/language-model.png" />
</p>

| Block | Produces | Reference |
| :-- | :-- | :-- |
| `Shader` | a `UMaterial` | [Shader](Docs/language/shader.md) |
| `ShaderFunction` | a `UMaterialFunction` | [ShaderFunction](Docs/language/shader-function.md) |
| `ShaderLayer` | a native `UMaterialFunctionMaterialLayer` | [ShaderLayer](Docs/language/shader-layer.md) |
| `ShaderLayerBlend` | a native `UMaterialFunctionMaterialLayerBlend` | [ShaderLayer](Docs/language/shader-layer.md) |
| `VirtualFunction` | nothing — declares an **existing** asset so `Graph` can call it | [VirtualFunction](Docs/language/virtual-function.md) |
| `Function` | one HLSL `Custom` node, via a generated `.ush` helper | [Function](Docs/language/function.md) |
| `GraphFunction` | a `Custom` node that may pull `UE.*` nodes into its inputs | [GraphFunction](Docs/language/graph-function.md) |
| `Namespace` | nothing — groups helpers as `Ns::Name` | [Namespace](Docs/language/namespace.md) |

The three source kinds are not interchangeable: `.dsm` holds at most one `Shader`, `.dsf` holds
function assets and may not declare a `Shader`, and `.dsh` is a header consumed through `import`.
See [Source files](Docs/language/source-files.md).

`Graph = { … }` is where the node graph is written — declarations, arithmetic, swizzles, `UE.*`
material nodes, math builtins, function calls and `if` / `else`. Typed `Properties` cover scalars,
vectors, textures, switches, MPC values and reflected node settings. `MaterialAttributes` and
`Substrate` (UE 5.4+) are first-class values that can be passed through graph code, function
signatures and output bindings.

## Documentation

The full reference lives in [`Docs/`](Docs/index.md), and is published at
**<https://lang.64hz.cn/docs>** in Chinese and English.

| | |
| :-- | :-- |
| **[Language reference](Docs/language/index.md)** | source files, lexical rules, top-level blocks, sections, types, `import` |
| **[Graph language](Docs/graph/index.md)** | statements, expressions, conversions, swizzles, `if` / `else`, calls — and [what `Graph` is not](Docs/graph/unsupported.md) |
| **[Builtins](Docs/builtins/index.md)** | the `UE.*` catalogue, [math builtins](Docs/builtins/math.md), [`Substrate.*`](Docs/builtins/substrate.md), the [`UE.Expression`](Docs/builtins/ue-expression.md) escape hatch |
| **[Parameters](Docs/parameters/index.md)** | the 21 parameter-node tokens, compact types, [metadata keys](Docs/parameters/metadata.md), [`SamplerType`](Docs/parameters/sampler-type.md) |
| **[Settings](Docs/settings/index.md)** | [material settings](Docs/settings/material.md) and their [enum values](Docs/settings/material-enums.md), function settings, [project settings](Docs/settings/project.md) |
| **[Generation](Docs/generation/index.md)** | [asset paths](Docs/generation/asset-paths.md), [in-memory materials](Docs/generation/in-memory.md), [caching](Docs/generation/caching.md), [graph layout](Docs/generation/graph-layout.md) |
| **[Editor tools](Docs/tools/index.md)** | browser, preview, decompiler, workspace, packages, bridge, [commandlet](Docs/tools/commandlet.md) |
| **[Diagnostics](Docs/diagnostics/index.md)** | every message the compiler can emit, by pipeline stage |
| **[Examples](Docs/examples/index.md)** | complete sources you can copy as they are |
| **[C++ API](Docs/api/index.md)** | the public headers, for extending the plugin |

## Editor and tooling

<p align="center">
  <img alt="DreamShader editor tools" src="./Images/editor-tools.png" />
</p>

| | |
| :-- | :-- |
| **[Material Content Browser](Docs/tools/material-browser.md)** | *Tools ▸ DreamShader*. The **Project** page browses every material under `/Game` with its full inheritance chain and one-click instance creation; the **Dream Shader Gen** page lists your sources with a live preview, compile-all and error surfacing |
| **[Decompiler](Docs/tools/decompiler.md)** | right-click a `Material` or `Material Function` ▸ *DreamShader ▸ Export DSM/DSF*. A migration starting point — common nodes become graph text, the rest falls back to `UE.Expression(…)` so the structure stays regeneratable |
| **[Packages](Docs/tools/packages.md)** | reusable `.dsh` libraries under `DShader/Packages/@scope/name/`, imported as `import "@typedreammoon/dream-noise/Library/Noise.dsh";` |
| **[Workspace](Docs/tools/workspace.md)** | the generated `DShader/DreamShader.code-workspace`, opened in VSCode from the editor toolbar |
| **[Commandlet](Docs/tools/commandlet.md)** | `-run=DreamShader compile \| decompile` — headless generation for CI |

### Editor language extensions

| Editor | Repository | Features |
| :-- | :-- | :-- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases) | highlighting, snippets, completion, go to definition, find references, hover, signature help, local and bridge diagnostics, material preview, package commands, templates |
| Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` file types, grammar and PSI parsing, highlighting, completion, navigation, diagnostics, bridge integration, semantic tokens, inlay hints, package tools |

## AI support

DreamShaderLang is a text format, so a coding agent can author it — but only if it can *check its
own work*. [`.skill/`](.skill/README.md) ships the harness that closes that loop: a headless driver
plus five skills, in the [Claude Code](https://claude.com/claude-code) skill format.

| Skill | Argument | Does |
| :-- | :-- | :-- |
| [`dream-shader-create`](.skill/dream-shader-create/SKILL.md) | `<description>` | writes a new material or function from plain language, then compiles it to prove it builds |
| [`dream-shader-optimize`](.skill/dream-shader-optimize/SKILL.md) | `<file>` | dedupes, renames, retargets and restores lost state in a decompiled source |
| [`dream-shader-decompile`](.skill/dream-shader-decompile/SKILL.md) | `<asset>` | exports an existing `UMaterial` / `UMaterialFunction` back to source |
| [`dream-shader-verify`](.skill/dream-shader-verify/SKILL.md) | `<file>` \| `-All` | compiles headlessly; exit `0` / `1` |
| [`dream-shader-diagnose`](.skill/dream-shader-diagnose/SKILL.md) | `<message>` | routes a diagnostic to its pipeline stage, explains it, fixes it |

`dsc.ps1` wraps the [commandlet](Docs/tools/commandlet.md): it resolves the engine from the
`.uproject`'s `EngineAssociation`, finds the project by walking up, prints only the `LogDreamShader`
lines, and — because a headless compile writes real `.uasset` files where the editor generates in
memory — classifies everything the run wrote against git so a probe asset never survives as
untracked clutter.

```powershell
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Panel.dsm -Force -CleanNew
```

Publish the skills into `.claude/skills/` once, and an agent working anywhere in the project loads
them by name:

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

> [!NOTE]
> Only the auto-loading is Claude Code specific. The driver is a plain PowerShell script and each
> `SKILL.md` is plain Markdown, so any agent — or any human — can read the instructions and run the
> same commands. [`.skill/reference/dreamshaderlang.md`](.skill/reference/dreamshaderlang.md)
> condenses the grammar an author actually needs, including the traps that only surface at compile
> time: the 19 reserved math builtins that shadow user code silently, the whole-identifier GLSL
> rewrite inside `Function` bodies, and the absent matrix types.

## Compatibility

Unreal Engine `5.3` – `5.8` on Win64, each verified with a single-plugin `RunUAT BuildPlugin` build.
Active development targets `5.8`.

<details>
<summary>Validating the plugin without building a project target</summary>

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

On Windows, UE `5.3` and `5.4` may require the MSVC `14.38` toolchain — newer compilers can fail
while compiling older engine headers, before plugin code is reached.

</details>

> [!NOTE]
> The decompiler is a migration helper, not a round-trip guarantee. It handles many common materials
> and some large Lyra cases, and leaves a `// Warning:` comment for everything it could not
> reproduce. The [known round-trip gaps](Docs/tools/decompiler.md#known-round-trip-gaps) are worth
> reading before deleting an original asset.

## Project info

| | |
| :-- | :-- |
| Version | `1.5.1` |
| Language | `DreamShaderLang` |
| Unreal Engine | `5.3` – `5.8` |
| Modules | `DreamShader`, `DreamShaderCompiler` (Runtime), `DreamShaderEditor` (Editor) |
| Author | TypeDreamMoon |
| GitHub | <https://github.com/TypeDreamMoon> |
| Docs | <https://lang.64hz.cn/> |
| Web | <https://dev.64hz.cn> |
| License | [MIT](LICENSE) |
| Copyright | Copyright (c) 2026 TypeDreamMoon. All rights reserved. |

Releasing is documented on [Release](Docs/contributing/release.md); building the plugin from source
on [Contributing](Docs/contributing/index.md).

## Roadmap

- Custom full-screen render pass support.
- More complete VSCode semantic diagnostics.
- Deeper Material Layer Stack and Layer Instance workflow support.
- Deeper Moon Engine integration — reference: <https://zhuanlan.zhihu.com/p/21979494450>

## License

DreamShader is released under the [MIT license](LICENSE). For bug reports and feature requests, open
an [issue](https://github.com/TypeDreamMoon/DreamShader/issues/new).
