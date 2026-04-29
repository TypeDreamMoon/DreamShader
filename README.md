# DreamShader

DreamShader 是一个 Unreal Engine 材质生成插件。它提供 `DreamShaderLang` 文本语言，让你用 `.dsm` / `.dsh` 源文件描述材质图、共享函数和材质函数，并自动生成标准 Unreal `UMaterial` / `UMaterialFunction` 资产。

> 当前版本：`1.2.1`。插件仍在持续开发中，核心工作流已经可用，建议在项目中逐步接入并保留源文件版本管理。

## 核心能力

- 使用文本源文件维护材质逻辑，减少手动连材质节点的重复工作。
- 从 `Shader(Name="...", Root="Game")` 生成 `UMaterial`，从 `ShaderFunction(Name="...", Root="Game")` 生成 `UMaterialFunction`。
- 使用 `VirtualFunction(Name="...")` 声明现有 Unreal `UMaterialFunction` 资产，并在 `Graph` 中直接调用，不会生成或覆盖资产。
- 在 `Graph = { ... }` 中声明变量、调用 UE 材质节点、调用共享函数，并绑定材质输出。
- 在 `Function` / `Namespace` 中编写可复用 HLSL 风格 helper。
- 支持 `Inline` / `SelfContained` 函数，把依赖代码嵌入材质 Custom 节点，便于在未安装 DreamShader 的项目中继续使用生成资产。
- 支持 `.dsh` import graph，头文件变更时只重编受影响的 `.dsm`。
- 支持 DreamShader Package，通过 VSCode 扩展安装和导入共享库。
- 支持 VSCode 语法高亮、补全、跳转、Hover、Signature Help、本地诊断和 Unreal 桥接诊断。

## 文件模型

| 文件或概念 | 用途 |
| --- | --- |
| `.dsm` | 材质实现文件，通常包含 `Shader(...)`、`ShaderFunction(...)` 或 `VirtualFunction(...)`。 |
| `.dsh` | 共享头文件，通常包含 `Function`、`Namespace`、`VirtualFunction` 和 `import`。 |
| `Graph` | `Shader` / `ShaderFunction` 内的材质图 DSL，用于生成材质节点。 |
| `Function` | 可复用 helper，函数体按 HLSL 风格编写。 |
| `Namespace` | 组织一组 helper，例如 `Texture::Sample2DRGB(...)`。 |
| `VirtualFunction` | 声明一个现有 `UMaterialFunction` 资产，供 `Graph` 作为值函数调用。 |
| `Path(...)` | 为纹理属性、对象属性或 `VirtualFunction` 资产声明 Unreal 路径。 |
| `DShader/Packages` | 项目安装的 DreamShader Package 目录。 |

推荐项目结构：

```text
Moon_Dev/
├─ DShader/
│  ├─ Materials/
│  │  └─ M_Sample.dsm
│  ├─ Shared/
│  │  └─ Common.dsh
│  └─ Packages/
└─ Plugins/
   └─ DreamShader/
```

## 快速开始

1. 把插件放入 Unreal 项目的 `Plugins/DreamShader`。
2. 在 Unreal 中启用插件并重启编辑器。
3. 在项目根目录创建 `DShader` 目录。
4. 新建 `.dsm` 文件，例如 `DShader/Materials/M_Sample.dsm`。
5. 保存文件后，DreamShader 会自动解析并生成或更新对应材质资产。

Project Settings > Plugins > DreamShader 中可以配置：

| 设置 | 默认值 | 说明 |
| --- | --- | --- |
| `SourceDirectory` | `DShader` | DreamShader 源文件根目录。 |
| `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | 生成 `.ush` 文件的目录。 |
| `AutoCompileOnSave` | `true` | 保存 `.dsm` / `.dsh` 时自动刷新资产。 |
| `SaveDebounceSeconds` | `0.25` | 保存防抖时间。 |
| `VerboseLogs` | `false` | 输出更详细的日志。 |

## 最小示例

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

`Root` 可省略，默认保存到 `/Game`。需要保存到已启用的项目内容插件根时可以写：

```c
Shader(Name="DreamMaterials/M_Minimal", Root="Plugin.MyPlugin")
{
    // ...
}
```

这会使用 `/MyPlugin/DreamMaterials/M_Minimal.M_Minimal` 作为 Unreal object path，并物理保存到 `[Project]/Plugins/MyPlugin/Content/DreamMaterials/M_Minimal.uasset`。
`Plugins.MyPlugin` / `Plugins/MyPlugin` 也作为兼容写法支持。

## 共享函数示例

`.dsh` 文件适合放共享函数：

```c
Namespace(Name="Color")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

`.dsm` 中通过 `import` 使用：

```c
import "Shared/Color.dsh";

Shader(Name="DreamMaterials/M_Tinted")
{
    Properties = {
        vec3 Tint = vec3(1.0, 1.0, 1.0);
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
        vec3 baseColor = vec3(0.7, 0.2, 1.0);
        Color::ApplyTint(baseColor, Tint, Color);
    }
}
```

## Graph 与 Function 的区别

`Graph = { ... }` 是材质图 DSL，目标是生成 Unreal 材质节点。它支持声明、赋值、构造、`UE.*` builtin、`Function` 调用、`ShaderFunction` / `VirtualFunction` 调用以及基础 `if` / `else` 图分支。

`Function Name(...) { ... }` 是共享 helper 代码，适合放更自由的 HLSL 风格逻辑，例如 `for` / `while` / 复杂条件。`Function` 调用必须显式传入 `out` 目标变量。

> 迁移提示：`Shader` / `ShaderFunction` 的图逻辑使用 `Graph = { ... }`。`Code` 仍保留给 `Function` helper 语义，不再作为 `Shader` / `ShaderFunction` 的图 section 使用。

## VirtualFunction

`VirtualFunction` 用于把项目里已有的 `UMaterialFunction` 暴露给 DreamShader Graph。它只声明调用签名，不生成、不保存、不覆盖对应资产。

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

Shader(Name="DreamMaterials/M_UseBuffer")
{
    // ...
    Graph = {
        Color = BufferWriter(Tint, 1.0, Output="Result");
    }
}
```

`Asset` 支持 `Path(Game, "...")`、`Path(Engine, "...")`、`Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`，也支持直接写完整 `/Game/...`、`/Engine/...` 或插件挂载路径。插件会在 Material Function 资产右键菜单和 Material Function 编辑器工具栏提供 `DreamShader` 下拉菜单，其中 `CopyVirtualFunction` 会把 Input / Output / Options 生成到剪贴板，`CreateVirtualFunction` 会在 `DShader/VirtualFunctions` 下创建 `.dsh` 定义文件，`CopyVirtualFunctionCall` 会复制可粘贴到 `Graph` 的调用示例。

## UE 材质节点

`Graph` 中可以通过 `UE.*` 创建常用材质表达式：

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float time = UE.Time();
    float pulse = UE.Expression(Class="Sine", OutputType="float1", Input=time);

    Color = vec3(pulse, pulse, pulse);
}
```

也可以绑定辅助输出节点：

```c
Outputs = {
    float Tangent;
    Expression(Class="TangentOutput").Pin[0] = Tangent;
}
```

## Package

DreamShader Package 是可复用 `.dsh` 函数库的分发格式。安装后的包位于：

```text
DShader/Packages/@scope/package-name/
```

导入示例：

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

更多说明见 [Docs/Packages.md](Docs/Packages.md)。

## VSCode 扩展

VSCode 扩展位于：

```text
I:/UnrealProject_Moon/VSCodeExt/dreamshader-language-support
```

扩展提供：

- `.dsm` / `.dsh` 语法高亮和 snippets。
- `Function`、`Namespace::Function`、`UE.*`、`Path(...)`、Package import 补全。
- Go to Definition、Find References、Hover、Signature Help。
- 本地语法诊断和 Unreal 桥接诊断。
- Package 安装、更新、移除和商店浏览命令。
- 快速创建 Material / Header / Texture Sample / Noise Material 模板。

Unreal 编辑器的 `Tools > DreamShader` 菜单和顶部 DreamShader 工具栏提供 `Open Dream Shader Workspace (VSCode)`。它会生成 `DShader/DreamShader.code-workspace`，优先用 VSCode 打开，找不到 VSCode 时会尝试默认编辑器，最后用记事本打开。

更多说明见 [Docs/VSCode.md](Docs/VSCode.md)。

## 文档

- [文档总览](Docs/README.md)
- [语法参考](Docs/LanguageReference.md)
- [示例与模式](Docs/Examples.md)
- [Package 系统](Docs/Packages.md)
- [VSCode 支持](Docs/VSCode.md)

## 发布 Release

仓库包含 GitHub Actions 自动发布流程。推送与 `DreamShader.uplugin` 中 `VersionName` 一致的 tag 即可打包并发布 Release：

```powershell
git tag v1.2.1
git push origin v1.2.1
```

也可以在 GitHub Actions 页面手动运行 `Release` workflow。发布包会生成 `DreamShader-<Version>.zip`，解压后是 `DreamShader` 插件目录，内容包含插件源码、资源、内置库、文档、README、CHANGELOG 和 LICENSE，不包含 `Binaries` / `Intermediate`。

## 版本信息

| 项目 | 内容 |
| --- | --- |
| Version | `1.2.1` |
| Language | `DreamShaderLang` |
| Author | TypeDreamMoon |
| GitHub | <https://github.com/TypeDreamMoon> |
| Web | <https://dev.64hz.cn> |
| Copyright | Copyright (c) 2026 TypeDreamMoon. All rights reserved. |

## Roadmap

- Custom Render Pass。
- 更完整的 `Graph` 语义诊断。
- 更多内置材质函数库和 Package 示例。
