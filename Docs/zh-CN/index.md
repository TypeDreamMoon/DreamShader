---
title: 概览
description: DreamShaderLang 是虚幻引擎材质的文本化语言，由 DreamShader 插件编译成标准材质资产；在 UE 5.8 上还能用 .dsp 写自己的渲染 pass。
---

DreamShaderLang 是虚幻引擎材质的文本化语言。你写源文件，**DreamShader** 插件在编辑器里编译它们，生成标准 Unreal 资产 ——
`UMaterial`、`UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend` 和
`UMaterialInstanceConstant`；在 Unreal Engine 5.8 上，`.dsp` [Custom Pass](#custom-pass) 管线还会编译成
`UDreamPassPipeline`，加上你自己的渲染 pass。

源文件才是你维护的东西。资产是生成产物，随时可以删掉重新生成。

两种语法，一个编译器 （自 2.0.0 起）：新源文件写 `.dss` —— 带声明的 HLSL —— 和 `.dsi` 材质实例，见
[DreamShaderLang 2.0](language-v2/index.md)；1.x 的 `.dsm` / `.dsf` / `.dsh` 照样编译，「语言核心」和「Graph 语言」两部分讲的就是
1.x 语法，[`dsc migrate`](tools/migrate.md) 可以一个文件一个文件地把它们改写成 `.dss`。

| | |
| --- | --- |
| 插件版本 | `2.1.2` |
| 引擎 | Unreal Engine `5.3` – `5.8`，Win64；Custom Pass 需要 `5.8` |
| 模块 | `DreamShaderLang`、`DreamShader`、`DreamShaderPass`（Runtime），`DreamShaderCompiler`、`DreamShaderEditor`（Editor） |
| 源文件扩展名 | `.dss` · `.dsi` （自 2.0.0 起） · `.dsp` （自 2.1.0 起） · `.dsm` · `.dsf` · `.dsh` |
| 项目设置 | *Project Settings ▸ DreamPlugin ▸ Dream Shader* |
| VS Code 扩展 | `2.1.0` |
| 许可证 | MIT |

<a id="六种源文件"></a>

## 六种源文件

| 扩展名 | 是什么 | 生成什么 |
| --- | --- | --- |
| `.dss` | DreamShaderLang 2.0 编译单元：`uniform`、`#pragma material`、`export` 函数 | 每个 `export` 一个资产 —— 一个材质，或者一组材质函数 / layer / blend |
| `.dsi` | 材质实例：父材质和覆盖项 | 一个 `UMaterialInstanceConstant` |
| `.dsp` | Custom Pass 管线：缓冲区和按顺序排列的 pass（UE 5.8） | 一个 `UDreamPassPipeline` |
| `.dsm` | 1.x 材质：最多一个 `Shader`，以及函数、Layer、Namespace、VirtualFunction 块 | 材质本体，加上同文件中声明的所有函数资产 |
| `.dsf` | 1.x 函数文件：`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 和 helper 块，不能写 `Shader` | 它声明的函数资产 |
| `.dsh` | header：helper、`VirtualFunction` / `extern` 声明；两种方言都能放 | 不直接生成资产，通过 `import` / `#include` 被使用 |

每个文件单独解析；被 import 的 header 也单独解析，它的声明成为导入它的文件里的名字 （自 2.0.0 起）。参见
[文件模型](language/source-files.md)和[项目结构](getting-started/project-layout.md)。

<a id="各个块生成什么"></a>

## 各个块生成什么

| `.dss` | 1.x 块 | Unreal 资产 |
| --- | --- | --- |
| `export void M(inout material m)` | `Shader` | `UDreamShaderMaterialInstance` 加一个隐藏的 `UMaterial` base（`ThinCustom`，默认），或一个普通 `UMaterial`（`Graph` backend） |
| 其他 `export` 函数 | `ShaderFunction` | `UMaterialFunction` |
| `/// @layer` / `/// @layerblend` 的 `export` | `ShaderLayer` / `ShaderLayerBlend` | `UMaterialFunctionMaterialLayer` / `UMaterialFunctionMaterialLayerBlend` |
| `/// @asset` 的 `extern` 原型 | `VirtualFunction` | 不生成资产 —— 只是声明一个已有的 `UMaterialFunction`，让材质可以调用 |
| `/// @custom` 函数 | `Function` / `GraphFunction` | `Custom` 节点代码 |
| `.dsi` | —— | `UMaterialInstanceConstant` |

<a id="最小材质"></a>

## 最小材质

```hlsl
// DShader/Materials/M_Minimal.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.2, 0.2);

export void M_Minimal(inout material m)
{
    m.EmissiveColor = Tint;
}
```

同一个材质的 1.x 写法：

```c
// DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal")
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

两个文件都生成 `/Game/Materials/M_Minimal` —— 留一个就够了。

**提示：**

在默认的 `ThinCustom` backend 下，生成的材质是 **Ephemeral** 的 —— 只存在于内存中，不会出现在 Content Browser 里。
  这是刻意设计的，有三种方式可以找到它 —— 见[第一个材质](getting-started/first-material.md)。

<a id="custom-pass"></a>

## Custom Pass

（自 2.1.0 起）

在 Unreal Engine 5.8 上，一个 [`.dsp` 管线](language-v2/passes.md)不改引擎就能加上你自己的渲染 pass：选中物体的遮罩、模糊、
屏幕空间效果、写数据纹理的 compute shader。它的 pass 运行在各自选择的注入点 —— base pass 之前、不透明光照之后、后处理链之前或
之中 —— 可以是 Post Process 材质、项目的 HLSL，或者把选中的网格再画一遍；它的缓冲区任何材质都能读。

- [管线（.dsp）](language-v2/passes.md) — 缓冲区、pass、注入点、绑定、网格选择，以及编译器检查什么。
  - [运行时](runtime/index.md) — 什么让管线生效、pass 何时运行、内置与导出的缓冲区、调试、限制。
  - [Custom Pass 示例](examples/custom-pass.md) — 高亮描边、UI 毛玻璃、透视、风场。

<a id="dreamshader-做什么不做什么"></a>

## DreamShader 做什么，不做什么

它在 Unreal 编辑器里生成材质资产，范围仅此而已。具体来说：

- **1.x 的 `Graph` 是节点图构建器，不是着色器编译器。** 它只有四个算术运算符和 `if` / `else`：没有循环、没有三元运算符、没有
  `%`、`if` 条件之外没有比较和位运算、没有矩阵、没有下标、没有复合赋值。（自 2.0.0 起） 起这些写法都会带码报错，不再被
  静默截断 —— 见[不支持的写法](graph/unsupported.md)。`.dss` 有循环和完整的运算符；真正命令式的代码写进 `/// @custom`
  函数或 1.x 的 `Function`，它的函数体是真正的 HLSL。
- **生成只发生在编辑器里。** 没有任何运行时路径能从 DreamShaderLang 构建材质，打包后的游戏里也不存在这些工具；唯一的运行时
  部分是执行已编译管线的 Custom Pass 模块。
- **生成的资产是产物，不是文档。** 重新生成会拆掉整张图再重建，手动改动会丢失；生成实例上的参数覆盖按名字放回。自
  （自 1.8.0 起） 起，会毁掉手改的那次重建**不会发生** —— 资产带指纹，对不上就拒绝编译，并让你在 Revert / Adopt /
  Detach 里选一个。见[重新生成](generation/regeneration.md)。
- **`Substrate.*` 需要 （自 UE 5.4 起），Custom Pass 需要 （自 UE 5.8 起）。** 节点目录跟着运行中的引擎走，引擎没有的东西
  会明确报错。
- **有些写法已经废弃。** 包括 `Backend = Instance`，以及 `MaterialLayer` / `MaterialLayerBlend` 这两种块写法 —— 都还能用，
  带警告。

<a id="从哪里开始"></a>

## 从哪里开始

- [快速开始](getting-started/installation.md) — 安装插件、生成第一个材质，并理解日常的保存 - 编译循环。
  - [DreamShaderLang 2.0](language-v2/index.md) — `.dss`、`.dsi` 与 `.dsp`：带声明的 HLSL、Substrate 语法糖、材质实例、Custom Pass、从 1.x 迁移。
  - [Custom Pass 运行时](runtime/index.md) — UE 5.8 上的渲染 pass：管线怎么生效、何时运行、缓冲区与调试。
  - [语言核心（1.x）](language/source-files.md) — 源文件、词法规则、预处理器、顶层块、Section、类型、函数、import、布局。
  - [Graph 语言（1.x）](graph/index.md) — 语句、表达式、类型转换、`if` / `else`、调用，以及 Graph 不能做什么。
  - [内置节点](builtins/ue.md) — `UE.*` 目录、通用逃生舱 `UE.Expression`、数学内置、`Substrate.*`、Custom Pass 节点。
  - [参数](language/properties.md) — Properties 类型、参数节点、元数据与分组、资产引用。
  - [设置](settings/material.md) — 材质 Settings 与枚举取值、Backend，以及全部 17 项项目设置。
  - [生成与产物](generation/index.md) — 生成流程、资产路径、内存材质、输出绑定、重新生成、版本控制。
  - [工具链](tools/editor-integration.md) — 编辑器工具、反编译导出、VSCode 与 Rider、命令行 commandlet、Package。
  - [诊断](diagnostics/index.md) — 诊断出现在哪里、`DSHnnnn` 码怎么读，以及当前限制。
  - [示例](examples/patterns.md) — 常用写法、完整示例和 Custom Pass 示例，每个都有 `.dss` 与 1.x 两种写法。
  - [HLSL / GLSL 背景](https://shader.toolchain.64hz.cn/docs/shader-languages/hlsl) — DreamShaderLang 借鉴的两种着色语言的背景知识。
  - [更新日志](https://shader.toolchain.64hz.cn/docs/changelog) — 插件、VSCode 扩展和网站的版本记录。
