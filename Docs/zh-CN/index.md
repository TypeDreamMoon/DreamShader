---
title: "DreamShader 参考"
description: "DreamShader 参考"
---

<a id="dreamshader-reference"></a>

# DreamShader 参考

如需教程和面向任务的阅读，请从[文档概览](README.md)开始。

DreamShader 将 **DreamShaderLang** 源文件——`.dsm`、`.dsf`、`.dsh`，以及在 2.0 系列上的
`.dss`——编译为标准 Unreal Engine 材质资产：`UMaterial`、`UMaterialFunction`、
`UMaterialFunctionMaterialLayer` 和 `UMaterialFunctionMaterialLayerBlend`——并且在 Unreal Engine 5.8 上，将
`.dsp` [Custom Pass](#custom-pass) 管线编译为 `UDreamPassPipeline` 资产，从而添加你自己的渲染 pass。
源文件是创作表面；资产是构建产物，随时可以重新生成。

| | |
| :-- | :-- |
| 版本 | `2.1.2` |
| 引擎 | Unreal Engine `5.3` – `5.8`（Win64 已验证）；Custom Pass 需要 `5.8` |
| 模块 | `DreamShaderLang` (Runtime)、`DreamShader` (Runtime)、`DreamShaderPass` (Runtime)、`DreamShaderCompiler` (Editor)、`DreamShaderEditor` (Editor) |
| 源文件扩展名 | `.dsm` 材质 · `.dsf` 函数 · `.dsh` 头文件 · `.dss` 2.0 编译单元 · `.dsi` 材质实例 · `.dsp` Custom Pass 管线 |
| 项目设置 | *Project Settings ▸ DreamPlugin ▸ Dream Shader* |
| 许可证 | MIT |

初次接触？请从 **[快速开始](getting-started.md)** 开始，然后是 **[示例](examples/index.md)**。

---

<a id="dreamshaderlang"></a>

## DreamShaderLang

> [!NOTE]
> 两套语法，一个编译器。下面记录的 **1.x** 语法（`.dsm` / `.dsf` / `.dsh`）是
> 已发布的语法。**2.0** 语法——`.dss`，用声明代替 Section 块的 HLSL——
> 在 `2.0` 系列上通过新的 binder/IR/emitter 管线编译，并有自己的页面：
> **[DreamShaderLang 2.0](language-v2/index.md)**。

<a id="language-referencelanguageindexmd"></a>

### [语言参考](language/index.md)

声明语法：源文件、块和 Section。

| | |
| :-- | :-- |
| [源文件](language/source-files.md) | `.dsm` / `.dsf` / `.dsh` 以及各自可包含的内容 |
| [词法元素](language/lexical.md) | 注释、标识符、大小写敏感性、字面量、后缀 |
| [关键字索引](language/keywords.md) | 每个保留字和别名，附链接 |
| [`import`](language/import.md) | 引入头文件、函数和包 |
| [预处理器](language/preprocessor.md) | `#if` 和 define 表——条件编译 |
| [类型](language/types.md) | 类型标记、GLSL 别名、各上下文有效性 |

**顶层块**

| | |
| :-- | :-- |
| [`Shader`](language/shader.md) | 生成一个 `UMaterial` |
| [`ShaderFunction`](language/shader-function.md) | 生成一个 `UMaterialFunction` |
| [`ShaderLayer` / `ShaderLayerBlend`](language/shader-layer.md) | 生成原生材质层函数 |
| [`VirtualFunction`](language/virtual-function.md) | 声明一个已有的 `UMaterialFunction` |
| [`Function`](language/function.md) | 可复用的 HLSL 辅助函数 |
| [`GraphFunction`](language/graph-function.md) | 可将 `UE.*` 节点引入其输入的 HLSL 辅助函数 |
| [`Namespace`](language/namespace.md) | 将辅助函数归组到 `Ns::Name` 下 |

**Section**

| | |
| :-- | :-- |
| [`Properties`](language/properties.md) | 参数声明和组作用域 |
| [`Inputs` / `Outputs`](language/inputs-outputs.md) | 函数签名和材质输出 |
| [输出绑定](language/output-bindings.md) | `Base.<Target> = <value>;` |
| [`Settings`](../settings/index.md) | 材质和函数设置 |
| [`Options`](language/options.md) | `VirtualFunction` 资产绑定 |
| [`Layout`](language/layout.md) | 节点放置、注释框、`#Region` |

<a id="graph-languagegraphindexmd"></a>

### [Graph 语言](graph/index.md)

`Graph = { ... }` 内部的语句和表达式语言，用于具现化材质节点。

| | |
| :-- | :-- |
| [语句](graph/statements.md) | 每种语句形式 |
| [声明](graph/declarations.md) | 变量、初始化器、作用域 |
| [表达式和运算符](graph/expressions.md) | 优先级、操作数规则、整数除法 |
| [字面量](graph/literals.md) | 数值、布尔值、后缀 |
| [构造函数](graph/constructors.md) | `float3(...)`、`vec4(...)`、整数构造函数 |
| [Swizzle](graph/swizzle.md) | 通道掩码、重排、重复 |
| [类型转换](graph/conversions.md) | 拓宽、收窄、分量数量规则 |
| [`if` / `else`](graph/if.md) | 条件、真值性、分支选择 |
| [`MaterialAttributes`](graph/material-attributes.md) | 类结构体成员写入 |
| [调用](graph/calls.md) | 调用函数和参数引脚 |
| [名称解析](graph/name-resolution.md) | 查找顺序和遮蔽 |
| [节点复用](graph/node-reuse.md) | 公共子表达式去重 |
| [不支持的写法](graph/unsupported.md) | 循环、`return`、`%`、索引——以及取而代之的行为 |

---

<a id="node-library"></a>

## 节点库

<a id="builtinsbuiltinsindexmd"></a>

### [内置节点](builtins/index.md)

| | |
| :-- | :-- |
| [`UE.*` 目录](builtins/ue.md) | 每个具名材质节点内置 |
| [`UE.Expression`](builtins/ue-expression.md) | 通用反射节点逃生舱 |
| [`OutputType` 值](builtins/output-type.md) | 每个可接受的输出类型标记 |
| [数学内置](builtins/math.md) | `lerp`、`saturate`、`frac`…… |
| [变换内置](builtins/transform.md) | `UE.TransformVector` / `UE.TransformPosition` 坐标基 |
| [`Substrate.*`](builtins/substrate.md) | Substrate 节点（UE 5.4+） |
| [Custom Pass 节点](builtins/dream-pass.md) | `UE.DreamPassOutput`、`UE.DreamPassBuffer`（UE 5.8） |
| [`DreamShaderBuiltins.ush`](builtins/hlsl-library.md) | 随附的 HLSL 辅助头文件 |

<a id="parametersparametersindexmd"></a>

### [参数](../parameters/index.md)

| | |
| :-- | :-- |
| [紧凑类型](parameters/compact-types.md) | `float`、`float3`、`Texture2D`…… |
| [参数节点](../parameters/parameter-nodes.md) | `ScalarParameter`、`StaticSwitchParameter`…… |
| [元数据块](parameters/metadata.md) | `Group`、`SortPriority`、`Slider`、反射属性 |
| [`SamplerType`](../parameters/sampler-type.md) | 纹理采样器配置 |
| [在 `Graph` 中使用参数](../parameters/graph-usage.md) | 值读取和引脚调用形式 |
| [`Path(...)`](parameters/path.md) | 资产引用 |

---

<a id="custom-pass"></a>

## Custom Pass

在 Unreal Engine 5.8 上添加你自己的渲染 pass，无需修改引擎：遮罩和自定义缓冲区、
屏幕空间效果、写入数据纹理的 compute shader。

| | |
| :-- | :-- |
| [管线——`.dsp`](language-v2/passes.md) | 缓冲区、pass、注入点、绑定、网格选择、编译器检查的内容 |
| [运行时](runtime/index.md) | 什么使管线生效、pass 何时运行、内置和导出的缓冲区、视图、调试、限制 |
| [HLSL pass](../runtime/hlsl.md) | 固定参数、槽位、快照和预检查 |
| [材质节点](builtins/dream-pass.md) | 用于网格 pass 的 `UE.DreamPassOutput`，用于读取导出缓冲区的 `UE.DreamPassBuffer` |
| [示例](examples/custom-pass.md) | 高亮描边、UI 毛玻璃、透视、风场 |
| [`DreamShaderPass`](api/pass-module.md) | 该模块的 C++ 和蓝图 API |

---

<a id="build-and-output"></a>

## 构建与输出

<a id="settingssettingsindexmd"></a>

### [设置](../settings/index.md)

| | |
| :-- | :-- |
| [材质设置](settings/material.md) | 特殊键加上反射的 `UMaterial` 属性 |
| [枚举取值](settings/material-enums.md) | 每一个 `ShadingModel`、`BlendMode`、`Domain` |
| [`Backend`](settings/backend.md) | `ThinCustom` 对比 `Graph` |
| [函数设置](../settings/function.md) | 材质函数支持的键 |
| [项目设置](settings/project.md) | *DreamPlugin ▸ Dream Shader* |

<a id="generationgenerationindexmd"></a>

### [生成](generation/index.md)

| | |
| :-- | :-- |
| [资产路径](generation/asset-paths.md) | `Name=` 和 `Root=` 到包路径与磁盘路径 |
| [瞬时材质](generation/ephemeral.md) | 薄实例、隐藏基础、烹饪行为 |
| [缓存](generation/caching.md) | 源哈希与跳过的重建 |
| [图表布局](generation/graph-layout.md) | 自动放置及其限制 |
| [重新生成](generation/regeneration.md) | 重建后什么保留、什么不保留 |
| [分歧](generation/divergence.md) | 手改过的资产：检测，以及三种出路 |
| [NoGeneration](generation/no-generation.md) | 只有指名它们的那次编译才会构建的源文件 |
| [生成的 HLSL](generation/generated-hlsl.md) | 生成的 `.ush` include |

---

<a id="tooling"></a>

## 工具链

<a id="editor-toolstoolsindexmd"></a>

### [编辑器工具](../tools/index.md)

| | |
| :-- | :-- |
| [编辑器集成](tools/editor-integration.md) | 菜单、工具栏、上下文菜单 |
| [材质 Content Browser](../tools/material-browser.md) | 浏览、筛选、实例化、实体化 |
| [预览](../tools/preview.md) | 缩略图和流式预览 |
| [反编译器](tools/decompiler.md) | 把已有的材质、实例和 Custom Pass 管线导出为源文件 |
| [VirtualFunction 工具](../tools/virtual-function-tools.md) | 声明并同步已有的函数 |
| [工作区与编辑器扩展](../tools/workspace.md) | VSCode 工作区、VSCode 与 Rider 插件 |
| [Package](tools/packages.md) | `DShader/Packages` 共享库 |
| [Commandlet](tools/commandlet.md) | 无头编译与反编译 |
| [编辑器 bridge](../tools/bridge.md) | 请求文件、WebSocket 协议、产物 |

<a id="c-apiapiindexmd"></a>

### [C++ API](api/index.md)

| | |
| :-- | :-- |
| [`DreamShaderModule.h`](api/dreamshader-module.md) | 模块、日志类别、导出的函数 |
| [`DreamShaderTypes.h`](api/types.md) | 解析后的源数据模型 |
| [`DreamShaderParser.h`](api/parser.md) | 2.0 中已退役 —— 取代 1.x 解析器入口点的是什么 |
| [`DreamShaderSettings.h`](api/settings.md) | `UDreamShaderSettings` |
| [`DreamShaderMaterialInstance.h`](api/material-instance.md) | 生成的实例类 |
| [`DreamShaderVersionCompat.h`](api/version-compat.md) | 引擎版本宏 |
| [`DreamShaderCompiler`](api/compiler-module.md) | 编译器接口与服务 |
| [`DreamShaderPass`](api/pass-module.md) | Custom Pass：管线资产、设置、子系统、体积与组件 |

---

<a id="reference"></a>

## 参考

| | |
| :-- | :-- |
| [诊断索引](diagnostics/index.md) | 每一个错误和警告，附原因与修复方法 |
| [禁用诊断](language-v2/disable-diagnostics.md) | `disable_warning` / `disable_error`，针对一个文件或一行 |
| [示例](examples/index.md) | 完整、可直接复制粘贴的源文件 |
| [贡献](contributing/index.md) | 构建插件 |
| [测试](contributing/testing.md) | 自动化测试套件与固定样本集 |
| [发布](contributing/release.md) | 打标签与打包 |

---

<a id="external-links"></a>

## 外部链接

| | |
| :-- | :-- |
| 仓库 | <https://github.com/TypeDreamMoon/DreamShader> |
| Issues | <https://github.com/TypeDreamMoon/DreamShader/issues> |
| 文档站点 | <https://shader.toolchain.64hz.cn/> |
| VSCode 扩展 | <https://github.com/TypeDreamMoon/dreamshader-language-support> |
| Rider 插件 | <https://github.com/tsdaer/dreamshader-language-support> |
| 更新日志 | [../CHANGELOG.md](../../CHANGELOG.md) |
