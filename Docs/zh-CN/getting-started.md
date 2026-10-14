---
title: "入门"
description: "入门"
---

<a id="getting-started"></a>

# 入门

> [DreamShader](index.md) » **入门**

本页将带领项目从“插件已复制进来”到“从文本文件生成材质”。

<a id="install"></a>

## 安装

1. 将插件复制到项目中：

   ```text
   MyProject/Plugins/DreamShader/
   ```

2. 在 *编辑 ▸ 插件* 中启用 **DreamShader** 并重启编辑器。DreamShader 依赖于
   引擎插件 `WebSocketNetworking` 和 `SQLiteCore`；两者会自动启用。
3. 确认设置页面存在：*项目设置 ▸ DreamPlugin ▸ DreamShader*。

| | |
| :-- | :-- |
| 引擎 | Unreal Engine `5.3` – `5.8`，已验证 Win64 |
| 加载的模块 | `DreamShaderLang`, `DreamShader`, `DreamShaderPass`（运行时）和 `DreamShaderCompiler`, `DreamShaderEditor`（编辑器） |

<a id="create-the-source-directory"></a>

## 创建源目录

DreamShader 从项目根目录下的一个目录读取源文件，默认是 `DShader`
（*项目设置 ▸ DreamPlugin ▸ DreamShader ▸ 路径 ▸ 源目录*）。

```text
MyProject/
├─ DShader/
│  ├─ Materials/        # .dss  — materials and material functions (DreamShaderLang 2.0)
│  │                    # .dsm  — a 1.x material, .dsf — 1.x material function assets
│  ├─ Instances/        # .dsi  — material instances
│  ├─ Shared/           # .dsh  — headers, included by other sources
│  └─ Packages/         # installed shared libraries
└─ Plugins/
   └─ DreamShader/
```

文件夹由你选择；重要的是扩展名。将新源文件写为 `.dss`；`.dsm` /
`.dsf` 是 1.x 语法，它仍然可以构建——通过同一个编译器——因此项目可以借助 [`dsc migrate`](tools/migrate.md) 一次迁移一个
文件。请参阅[源文件](language/source-files.md)
和 [DreamShaderLang 2.0](language-v2/index.md)。

<a id="write-a-material"></a>

## 编写材质

`MyProject/DShader/Materials/M_Minimal.dss`：

```hlsl
#pragma material(Domain = UI, ShadingModel = Unlit)

/// @group Look   @desc Base colour of the panel
uniform float3 Tint = float3(1.0, 0.2, 0.2);

export void M_Minimal(inout material m)
{
    m.EmissiveColor = Tint;
}
```

或者 *材质内容浏览器 ▸ 新建 ▾ ▸ 材质 (.dss)*，它会根据模板写入相同的结构。

保存该文件。启用 *保存时自动编译*（默认启用）后，DreamShader 会在短暂延迟后编译源文件
并构建材质。

| 部分 | 参考 |
| :-- | :-- |
| `#pragma material(…)` | [材质设置](settings/material.md)、[枚举值](settings/material-enums.md) |
| `///` 行和 `uniform` | 材质参数及其元数据 — [DreamShaderLang 2.0](language-v2/index.md) |
| `export void M_Minimal(inout material m)` | 入口：使文件成为材质的那个函数 |
| `m.EmissiveColor = …` | 材质输出，会根据引擎的属性列表进行检查 |

The asset lands under `/Game` at the file's folder relative to the source root, named after the
`export` function: this file produces `/Game/Materials/M_Minimal`. `/// @name` above the entry names
it otherwise — see [Asset paths](generation/asset-paths.md).

<a id="the-same-material-in-1x-syntax"></a>

### 1.x 语法中的同一材质

已有 `.dsm` 文件的项目会继续编写它们；下面这个会构建相同的材质：

```c
// MyProject/DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2) [Group="Look"; Description="Base colour of the panel"];
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

| 部分 | 参考 |
| :-- | :-- |
| `Shader(Name=…)` | [`Shader`](language/shader.md) |
| `Properties = { … }` | [`Properties`](language/properties.md)、[参数目录](../parameters/index.md) |
| `Settings = { … }` | [材质设置](settings/material.md) |
| `Base.EmissiveColor = …` | [输出绑定](language/output-bindings.md) |
| `Graph = { … }` | [图形语言](graph/index.md) |

`Name` 是相对于根目录的资产路径，`Root` 默认为 `Game`。保留这两个
文件中的一个：两者都会构建 `/Game/Materials/M_Minimal`。

<a id="find-the-generated-material"></a>

## 找到生成的材质

在默认后端 **ThinCustom** 下，DreamShader 会生成 **临时** 材质：在内存中，
磁盘上没有 `.uasset`，并且从内容浏览器中隐藏。这是有意为之——源文件
就是创作表面，隐藏该资产可以防止意外的 *保存* 将其具体化。一个
`Graph` 后端材质（`#pragma material(Backend = Graph)`）是普通资产，在每次
构建时保存。

要处理结果：

- 打开 *工具 ▸ DreamShader ▸ 材质内容浏览器*。它列出源文件、它们的编译
  状态和诊断信息，以及每个源文件构建的资产。
- 或者打开 *工具 ▸ DreamShader ▸ 显示临时材质*——或在
  *项目设置 ▸ DreamPlugin ▸ DreamShader ▸ 编译器* 下的同名设置——以在内容浏览器和
  资产选择器中显示它们。
- 或者在你需要磁盘上的真实资产时，从浏览器中 *具体化* 一个。

烘焙会自动将资产具体化。请参阅[临时材质](generation/ephemeral.md)。

<a id="when-something-fails"></a>

## 当出现失败时

每个诊断都带有一个 `DSHnnnn` 代码，并报告在其所在的行和列：

```text
I:/MyProject/DShader/Materials/M_Minimal.dss(8,24): DSH4200: 'Tnt' is not declared; did you mean 'Tint'? Names are case-sensitive.
```

它会进入输出日志、材质内容浏览器，以及 VSCode
扩展读取的桥接文件。在[诊断目录](diagnostics/README.md)中查找该代码；诊断出现的位置
以及如何阅读诊断，请参阅[诊断](diagnostics/index.md)。

<a id="next-steps"></a>

## 后续步骤

| | |
| :-- | :-- |
| [示例](examples/index.md) | 参数、函数、图层、Substrate 等的完整源文件 |
| [DreamShaderLang 2.0](language-v2/index.md) | `.dss` 语言：声明、`///` 元数据、管线 |
| [材质实例](language-v2/instances.md) | `.dsi` — 作为源文件的实例 |
| [语言参考 (1.x)](language/index.md) | 1.x 声明语法，用于 `.dsm` / `.dsf` 源文件 |
| [图形语言 (1.x)](graph/index.md) | 1.x `Graph = { … }` 接受什么——以及不接受什么 |
| [编辑器工具](../tools/index.md) | 浏览器、反编译器、VSCode 工作区、无头命令行工具 |
| [`dsc migrate`](tools/migrate.md) | 将 1.x 源文件重写为 `.dss`，在写入之前先证明 |
| [自定义 Pass 管线](language-v2/passes.md) | *（UE 5.8，自 2.1.0 起）* 从 `.dsp` 源文件创建你自己的渲染通道——全屏、计算和网格通道，由 [自定义 Pass 运行时](runtime/index.md) 运行 |
| [项目设置](settings/project.md) | 每个设置及其默认值 |

<a id="see-also"></a>

## 另请参阅

- [主页](index.md)
- [重新生成](generation/regeneration.md) — 重新构建会覆盖什么
- [贡献](contributing/index.md) — 从源代码构建插件
