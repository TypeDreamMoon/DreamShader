<p align="center">
  <img alt="DreamShader banner" src="./Images/banner.png" />
</p>

<table>
  <tr>
    <td width="64%" valign="top">
      <h1>DreamShader</h1>
      <p><strong>用 DreamShaderLang 以文本优先的方式编写 Unreal Engine 材质。</strong></p>
      <p>
        DreamShader 会把 <code>.dsm</code>、<code>.dsf</code> 和 <code>.dsh</code> 源文件转换为 Unreal
        <code>UMaterial</code>、<code>UMaterialFunction</code>、Material Layer 和 Material Layer Blend 资产。
      </p>
      <p>
        <img alt="Unreal Engine 5.3-5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.3--5.8-313131" />
        <img alt="Version 1.5.0" src="https://img.shields.io/badge/version-1.5.0-blue" />
        <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-green" />
      </p>
      <p>
        <a href="README.md">English</a> |
        <a href="Docs/index.md">文档</a> |
        <a href="Docs/language/index.md">语法参考</a> |
        <a href="Docs/examples/index.md">示例</a> |
        <a href="Docs/tools/packages.md">Package</a> |
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
      <p><strong>QQ 群：</strong><a href="https://qm.qq.com/q/X9uCLjVcY">466585194</a></p>
    </td>
    <td width="36%" align="center" valign="middle">
      <img src="./Images/character.png" width="260" alt="DreamShader character" />
    </td>
  </tr>
</table>

> [!TIP]
>
> DreamShader 目前基于 Unreal Engine `5.8` 持续开发，并已通过单插件 `BuildPlugin` 构建验证 Unreal Engine `5.3`、`5.4`、`5.5`、`5.6`、`5.7` 和 `5.8`。
>
> 建议把所有 `.dsm`、`.dsf` 和 `.dsh` 源文件纳入版本管理。生成的 Unreal 资产可以随时从源文件重新生成。
>
> 反编译器定位是迁移辅助工具。它能处理很多常见材质和部分 Lyra 大型材质案例，但目前还不是完整稳定的双向往返系统。

## 总览

<p align="center">
  <img alt="DreamShader workflow overview" src="./Images/workflow-overview.png" />
</p>

| 工作流 | 语言能力 | 工具链 |
| :----- | :------- | :----- |
| 通过 `Shader` 生成 `UMaterial`。 | 使用 `Graph = { ... }` 编写面向材质节点的 DSL。 | 在 Unreal Editor 中保存源文件后自动刷新资产。 |
| 通过 `ShaderFunction` 生成 `UMaterialFunction`。 | 使用 `Function`、`GraphFunction` 和 `Namespace` 编写 HLSL 风格 helper。 | 从编辑器工具栏打开生成的 VSCode workspace。 |
| 生成原生 Material Layer 和 Layer Blend 函数资产。 | 使用 typed `Properties` 声明 scalar、vector、texture、switch、MPC 和反射节点属性。 | 从 Content Browser 将已有材质和材质函数导出为 `.dsm` / `.dsf` 初稿。 |
| 通过 `VirtualFunction` 引用已有 Unreal 材质函数。 | 在 graph 代码中传递 `Texture2D`、`Texture2DArray`、`VolumeTexture` 和 `MaterialAttributes`。 | 配套 VSCode 扩展和 Rider 插件提供高亮、补全、导航、诊断和 Package 工具。 |

## 快速开始

1. 把插件复制到 Unreal 项目中：

   ```text
   MyProject/Plugins/DreamShader/
   ```

2. 在 Unreal Editor 中启用 `DreamShader` 并重启编辑器。
3. 在项目根目录创建源文件目录：

   ```text
   MyProject/DShader/
   ```

4. 新建一个材质源文件，例如：

   ```text
   MyProject/DShader/Materials/M_Minimal.dsm
   ```

5. 保存文件后，DreamShader 会解析源文件并生成或更新目标 Unreal 资产。

配置位于 `Project Settings > DreamPlugin > Dream Shader`。

推荐项目结构：

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

## 版本兼容

DreamShader 面向 Unreal Engine 5，目前已在 Windows 上通过单插件 `RunUAT BuildPlugin` 构建验证：

| Unreal Engine | 状态 |
| :------------ | :--- |
| `5.8` | 已验证 |
| `5.7` | 已验证 |
| `5.6` | 已验证 |
| `5.5` | 已验证 |
| `5.4` | 已验证 |
| `5.3` | 已验证 |

如果只想验证插件，不想编译完整项目目标，可以使用 Unreal 的插件打包命令：

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

在 Windows 上，UE `5.3` 和 `5.4` 可能需要 MSVC `14.38` 工具链。较新的编译器工具链可能会在旧版引擎头文件处失败，还没有进入插件代码编译。

## 最小材质

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

`Root` 可以省略，默认保存到 `Game`。如果需要生成到项目内容插件，可以使用 `Root="Plugin.MyPlugin"`：

```c
Shader(Name="DreamMaterials/M_Minimal", Root="Plugin.MyPlugin")
{
    // ...
}
```

这会生成 `/MyPlugin/DreamMaterials/M_Minimal.M_Minimal`，并保存到 `[Project]/Plugins/MyPlugin/Content/DreamMaterials/M_Minimal.uasset`。

## 语言模型

<p align="center">
  <img alt="DreamShader language model" src="./Images/language-model.png" />
</p>

| 项目 | 用途 |
| :--- | :--- |
| `.dsm` | 材质实现文件，通常包含 `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 或 `VirtualFunction`。 |
| `.dsf` | Dream Shader Function 文件，用于生成可复用 `ShaderFunction`、`ShaderLayer` 和 `ShaderLayerBlend` 资产，并可被 `.dsm` 导入。 |
| `.dsh` | 共享头文件，通常包含 `import`、`Function`、`GraphFunction`、`Namespace` 和 `VirtualFunction` 声明。 |
| `Shader` | 生成 Unreal `UMaterial`。 |
| `ShaderFunction` | 生成 Unreal `UMaterialFunction`。 |
| `ShaderLayer` | 生成原生 `UMaterialFunctionMaterialLayer`。 |
| `ShaderLayerBlend` | 生成原生 `UMaterialFunctionMaterialLayerBlend`。 |
| `VirtualFunction` | 描述已有 Unreal `UMaterialFunction`，供 `Graph` 调用。 |
| `Graph` | 用于生成材质节点的图 DSL。 |
| `Function` | 可复用 HLSL 风格 helper。 |
| `GraphFunction` | 可复用 Custom 节点 helper，可把 body 中的 `UE.*` 材质节点自动转成 Custom 输入。 |
| `Namespace` | 对 helper 分组，例如 `Texture::Sample2DRGB(...)`。 |
| `Path(...)` | 声明纹理、对象设置或虚拟函数使用的 Unreal 资产路径。 |

## Properties

`Properties` 可以使用 `float`、`float3`、`Texture2D`、`Texture2DArray` 和 `VolumeTexture` 等简写，也可以显式声明 Unreal 参数节点。声明尾部的 `[...]` 块会通过反射写入 Unreal 表达式属性。

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

Material Parameter Collection 可以在图代码中直接读取：

```c
Graph = {
    float wind = UE.CollectionParam(
        Collection=Path(Game, "MaterialParameterCollections/MPC_Global"),
        Parameter="WindStrength");
}
```

## Graph 与 Helper

`Graph = { ... }` 是材质节点 DSL。适合变量声明、赋值、构造、`UE.*` 材质节点、DreamShader helper 调用、生成或虚拟材质函数调用，以及简单图分支。

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float pulse = UE.Expression(Class="Sine", OutputType="float1", Input=UE.Time());
    Color = vec3(pulse, pulse, pulse);
}
```

`Function` 是 HLSL 风格可复用代码，适合计算、循环，以及应该放进 Custom 节点的逻辑。

```c
Namespace(Name="Color")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

导入后即可调用：

```c
import "Shared/Color.dsh";

Graph = {
    Color::ApplyTint(BaseColor, Tint, Color);
}
```

`GraphFunction` 同样会生成 Custom 节点，但它的 body 中可以直接写 `UE.*` 调用。DreamShader 会先创建对应 Unreal 材质节点，再把节点输出作为自动输入引脚接入 Custom 节点。

## 材质工作流

### MaterialAttributes

`MaterialAttributes` 可以作为 Graph 值、函数输出、虚拟函数输出和材质输出绑定使用。当 `Shader` 绑定到 `Base.MaterialAttributes` 时，DreamShader 会自动启用 Unreal 材质的 `Use Material Attributes`。

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

`Substrate` 可以作为 Graph 值、`.dsf` / `ShaderFunction` / `VirtualFunction` 输入输出，以及通过 `Base.FrontMaterial` 绑定到材质输出。绑定 `Base.FrontMaterial` 时，生成器会把材质设为 Substrate shading model，除非已经显式写了 `ShadingModel="Substrate"` / `"Strata"`。

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}

Graph = {
    Surface = Substrate.Unlit(EmissiveColor=Color);
}
```

推荐使用 `Substrate.*` 封装调用 UE 5.4+ Substrate 节点，例如 `Unlit`、`Slab`、`ConvertMaterialAttributes`、`VerticalLayer`、`Add`、`Weight`、`Select`，以及 `ThinFilm` 等工具节点。非 Custom 材质表达式也可以通过 `UE.Expression(..., OutputType="Substrate")` 作为逃生口。

### Material Layer

`ShaderLayer` 和 `ShaderLayerBlend` 会生成 Unreal 原生 Material Layer 函数资产。

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

当前规则：

- `ShaderLayer` 创建 `UMaterialFunctionMaterialLayer`。
- `ShaderLayerBlend` 创建 `UMaterialFunctionMaterialLayerBlend`。
- 两者复用 `ShaderFunction` 的 `Properties`、`Inputs`、`Outputs`、`Settings` 和 `Graph` sections。
- `ShaderLayer` 最多只能声明一个输入，且该输入必须是 `MaterialAttributes`；层控制量请使用 `Properties`。
- 两者必须声明且只声明一个 `MaterialAttributes` 输出。
- `ShaderLayerBlend` 必须刚好声明两个输入，且都必须是 `MaterialAttributes`；混合控制量请使用 `Properties`。
- 旧语法 `MaterialLayer(...)` 和 `MaterialLayerBlend(...)` 仍可解析，但会产生废弃警告。

当前支持的是原生 Layer Function 资产生成；完整 Material Layer Stack 和 Layer Instance 工作流仍在 Roadmap 中。

### VirtualFunction

`VirtualFunction` 用于把已有 Unreal `UMaterialFunction` 暴露给 DreamShader，不生成、不保存、不覆盖对应资产。

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

Unreal Material Function 资产右键菜单和编辑器工具栏中会提供 DreamShader 操作，用于复制虚拟函数定义、创建 `.dsh` 声明文件、打开已有声明，以及复制调用示例。

## 编辑器工具

<p align="center">
  <img alt="DreamShader editor tools" src="./Images/editor-tools.png" />
</p>

### Material Content Browser

`Tools > DreamShader > Material Content Browser` 打开一个专用面板，用于管理和创建 DreamShader 材质，分为两个分页：

- **Project** —— 浏览、筛选并查看 `/Game` 下的所有材质与材质实例，显示完整继承链（隐藏 base → 根实例 → 变体），并可一键 **Create instance** 到指定目录。
- **Dream Shader Gen** —— 列出 `.dsm` 源文件，带实时预览、搜索与筛选、一键全部编译，以及加载期错误提示。

它还能把仅存于内存（预览）的材质固化（materialize）到磁盘，并切换 DreamShader 的内存态材质是否显示在 Content Browser 中。

### 反编译导出

在 Content Browser 右键 `Material` 或 `Material Function`，选择 `DreamShader > Export DSM/DSF`。导出的源文件会写入 `DShader/Decompiled/Materials` 或 `DShader/Decompiled/Functions`，并自动打开。

反编译面向迁移和二次编辑：常见参数、常量、算术、swizzle、纹理采样、Custom 节点和 MaterialFunctionCall 会尽量导出为 DreamShader 图代码；少见节点会用 `UE.Expression(...)` 保留可生成结构。

### Package

DreamShader Package 是可复用 `.dsh` 函数库，安装位置为：

```text
DShader/Packages/@scope/package-name/
```

导入示例：

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

Package 结构、锁文件、安装命令和包开发方式见 [Docs/tools/packages.md](Docs/tools/packages.md)。

### 编辑器语言插件

DreamShaderLang 目前提供 VSCode 和 JetBrains Rider 两套编辑器支持。

| 编辑器 | 仓库 | 主要能力 |
| :----- | :--- | :------- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases) | 语法高亮、snippets、补全、Go to Definition、Find References、Hover、Signature Help、本地诊断、Unreal 桥接诊断、Package 命令和快速模板。 |
| Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` 文件类型、语法和 PSI 解析、高亮、补全、导航、诊断、Unreal Bridge 集成、语义 token、inlay hints 和 Package 工具。 |

Unreal 编辑器的 `Tools > DreamShader` 菜单和 DreamShader 工具栏可以打开生成的 `DShader/DreamShader.code-workspace`。

扩展发布地址：[dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases)。

## 配置

| 设置 | 默认值 | 说明 |
| :--- | :----- | :--- |
| `SourceDirectory` | `DShader` | DreamShader 源文件根目录。 |
| `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | 生成 `.ush` helper 文件的目录。 |
| `AutoCompileOnSave` | `true` | 保存 `.dsm`、`.dsf` 或 `.dsh` 时刷新受影响资产。 |
| `SaveDebounceSeconds` | `0.25` | 文件保存防抖时间。 |
| `VerboseLogs` | `false` | 输出更详细日志。 |
| `OpenInNewWindow` | `true` | 打开 DreamShader VSCode workspace 时默认使用新窗口。 |

## 发布

仓库包含 GitHub Actions 自动发布流程。推送与 `DreamShader.uplugin` 版本一致的 tag 即可。正式版用 `vX.Y.Z`；Beta 版（`IsBetaVersion: true`，或 `VersionName` 以 `Beta` 结尾）用 `b` 后缀，并会自动发布为 GitHub 预发布（pre-release）：

```powershell
# 正式版，VersionName "1.5.0"
git tag v1.5.0
git push origin v1.5.0

# Beta 版，VersionName "1.5.1 - Beta"
git tag v1.5.1b
git push origin v1.5.1b
```

发布包名为 `DreamShader-<slug>.zip`（例如 `DreamShader-1.5.0.zip`），包含插件源码、资源、文档、README、CHANGELOG 和 LICENSE，不包含 `Binaries` / `Intermediate`。Release workflow 还会附带 `TypeDreamMoon/dreamshader-language-support` 的最新 VSCode 扩展资产，并根据 `CHANGELOG.md` 中对应版本的段落生成发布说明。

## 项目信息

| 项目 | 内容 |
| :--- | :--- |
| Version | `1.5.0` |
| Language | `DreamShaderLang` |
| Unreal Engine | `5.3` - `5.8` |
| Author | TypeDreamMoon |
| GitHub | <https://github.com/TypeDreamMoon> |
| Docs | <https://lang.64hz.cn/> |
| Web | <https://dev.64hz.cn> |
| License | [MIT](LICENSE) |
| Copyright | Copyright (c) 2026 TypeDreamMoon. All rights reserved. |

## Roadmap

- Custom full-screen render pass 支持。
- 更完整的 VSCode 语义诊断。
- 更深入的 Material Layer 工作流支持。
- 更深入的 Moon Engine 集成。参考：<https://zhuanlan.zhihu.com/p/21979494450>

## License

DreamShader 使用 [MIT license](LICENSE) 发布。

问题和功能请求可以提交到 [Issues](https://github.com/TypeDreamMoon/DreamShader/issues/new)。
