---
title: 关于 DreamShader
description: 这个插件是什么、包含哪五个模块、支持哪些引擎，以及相关仓库在哪里。
---

DreamShader 是一个 Unreal Engine 插件，把 DreamShaderLang 源文件编译成标准材质资产 —— 在 UE 5.8 上，还能把 `.dsp`
Custom Pass 管线编译成你自己的渲染 pass。DreamShaderLang 是语言；解析、生成、缓存、诊断、反编译和编辑器集成都由插件负责。

| | |
| --- | --- |
| 版本 | `2.1.2` —— 描述文件 `Version` `212`，`IsBetaVersion` 为 `false` |
| 引擎 | Unreal Engine `5.3` – `5.8`，Win64；Custom Pass 需要 `5.8` —— `2.1.2` 实际对哪些引擎验证过见[引擎支持](#引擎支持) |
| 模块 | `DreamShaderLang`、`DreamShader`、`DreamShaderPass`（Runtime），`DreamShaderCompiler`、`DreamShaderEditor`（Editor） |
| 源文件 | `.dss` 2.0 编译单元 · `.dsi` 材质实例 · `.dsp` Custom Pass 管线 · `.dsm` 1.x 材质 · `.dsf` 1.x 函数 · `.dsh` header |
| 插件依赖 | `WebSocketNetworking`、`SQLiteCore` |
| 描述文件标志 | `EnabledByDefault`、`CanContainContent`；`LocalizationTargets` 加载 `Content/Localization/DreamShader/`（`zh-Hans`） |
| 许可证 | MIT |
| 仓库 | [TypeDreamMoon/DreamShader](https://github.com/TypeDreamMoon/DreamShader) |

<a id="解决什么问题"></a>

## 解决什么问题

Unreal 材质图一旦变大，就很难 review、diff、复用和迁移。DreamShader 把其中可重复的部分搬进文本：

| 痛点 | 处理方式 |
| --- | --- |
| 大图难以 review 和 diff | 图的结构存在源文件里 —— `.dss`，或 1.x 的 `.dsm` / `.dsf` |
| 共享逻辑被反复复制 | `.dsh` header、函数资产、`/// @custom` 与 `Function` / `GraphFunction` helper、Package |
| 重新生成材质函数会让调用方断线 | `FunctionInput` / `FunctionOutput` 的 pin 身份按名字保留 |
| Substrate 图需要文本化编写 | `Substrate` 类型、`FrontMaterial`、`Substrate.*` 内置，以及 `+`、`*`、`lerp` 组合 （自 UE 5.4 起） |
| 已有的图需要迁移 | 反编译器把材质、材质函数、材质实例和 Custom Pass 管线导出回 `.dss` / `.dsi` / `.dsp`（或 1.x 的 `.dsm` / `.dsf`） |
| 1.x 源文件要升级 | `dsc migrate` 改写成 `.dss`，写盘之前先证明构建出同一张图 （自 2.0.0 起） |
| 生成的资产追溯不到来源 | `DreamShader.SourceFile` 和 `DreamShader.SourceHash` 写进 package 元数据 |
| 一次重建把手改的资产悄悄覆盖了 | 输出指纹 `DreamShader.OutputDigest` 拦下这次重建，并给出 Revert / Adopt / Detach （自 1.8.0 起） |
| 插件想自带自己的材质源码 | 每个带 `DShader` 目录的已启用插件贡献一个源码根 （自 1.6.0 起） |
| 图里想写真正的表达式和控制流 | `.dss`：带声明的 HLSL，一个 `export` 一个资产 （自 2.0.0 起） |
| 材质实例只能在面板里一项项勾 | `.dsi`：父材质和覆盖项写成文本 （自 2.0.0 起） |
| 想要引擎没有的渲染 pass —— 描边遮罩、模糊、屏幕空间效果、写数据纹理的 compute | `.dsp` Custom Pass 管线，不改引擎 （自 2.1.0 起），需要 （自 UE 5.8 起） |

它不是材质编辑器的替代品。1.x 的 `Graph` 是一个只有四个运算符和 `if` / `else` 的节点图构建器；`.dss` 有循环和完整的
运算符，但同样降级成一张材质图。真正命令式的逻辑写进 `/// @custom` 函数或 1.x 的 `Function`，它的函数体是真正的 HLSL，
会被编译进 `Custom` 节点。一次性的、非常视觉化的图，在 Unreal 里直接连更快 —— 等它稳定下来再导出成源文件也不迟。

<a id="五个模块"></a>

## 五个模块

| 模块 | 类型 | 公开头文件 | 职责 |
| --- | --- | --- | --- |
| `DreamShaderLang` （自 2.0.0 起） | Runtime | 31 | 语言本身，只依赖 `Core`：词法、两套解析器（2.0 与 1.x 前端）、预处理器、绑定器、与引擎无关的图 IR、从 IR 回到源码的路径，以及 1.x → 2.0 迁移器。 |
| `DreamShader` | Runtime | 8 | 日志类别、规范化路径 helper、1.x 源码数据模型、define 表的引擎一侧、`UDreamShaderSettings`、`UDreamShaderMaterialInstance`、引擎版本宏，以及编译接口 `IDreamShaderCompiler`。 |
| `DreamShaderPass` （自 2.1.0 起） | Runtime | 11 | Custom Pass 运行时：管线资产 `UDreamPassPipeline`、它的设置、子系统、`ADreamPassVolume` 与 `UDreamPassComponent`、场景视图扩展和各种 pass。每个支持的引擎上都能编译，pass 只在 5.8 上运行。 |
| `DreamShaderCompiler` | Editor | 20 | 编译器的后半段：编译管线、IR 发射器、资产层、从反射读出的内置目录、产物索引、`.dsi` 的父材质 schema。`1.9.x` 及以前它是只含接口的 Runtime 模块。 |
| `DreamShaderEditor` | Editor | **0** | 用户碰得到的一切：反编译器、`dsc migrate`、commandlet、桥接、预览、Material Content Browser、来源追溯操作、workspace 导出。 |

三个 Runtime 模块在 `PostConfigInit` 阶段加载，早到设置对象、shader 目录映射和 `DreamShaderPass` 的全局 shader 类型在引擎
确定 shader 类型列表之前就已就位；两个 Editor 模块在 `Default` 阶段加载。

**提示：**

`DreamShaderEditor` 不导出任何符号，也无法被链接 —— 它根本没有 `Public/` 目录。受支持的 C++ 入口是
  `IDreamShaderCompiler`：声明在 `DreamShader`（`DreamShaderCompilerInterface.h`），通过
  `GetDreamShaderCompiler()` 拿到，由 `DreamShaderCompiler` 实现；以及 `DreamShaderPass` 的 C++ 与蓝图 API。其余能力只能
  通过编辑器 UI、commandlet 或桥接文件访问。

语言本身与引擎无关：`DreamShaderLang` 只依赖 `Core`，不创建 `UObject`，不加载资产。生成则相反 —— 仅编辑器、
仅游戏线程，构建在材质编辑库之上。

<a id="引擎支持"></a>

## 引擎支持

`2.1.2` 已用单插件 `RunUAT BuildPlugin` 在 Win64 上对 `5.5`、`5.6` 和 `5.8` 验证。`5.7` 最后一次验证是在 `1.8.0`：
之后加的引擎开关都直接问引擎自己的头文件或类型，而不是看版本号，所以 5.7 不需要单独的答案。`5.3` 和 `5.4` 源码兼容，但新版
MSVC 工具链编不过这两个引擎 —— 失败发生在引擎头文件里，还没轮到插件代码；见[安装 ▸ 引擎兼容性](installation.md#引擎兼容性)。

功能可用性随引擎版本不同。插件二进制按引擎编译，所以把项目升级到新引擎意味着要重新编译插件。

| 需要 | 功能 |
| --- | --- |
| **5.4** | Substrate —— `Substrate.*`、`ShadingModel="Substrate"`、`Base.FrontMaterial`、`Strata` 别名 · 生成的 `Custom` 节点默认折叠代码 · `bHasPixelAnimation` 的重置与导出 |
| **5.5** | `PeriodicWorld` transform 基准 · `UE.TransformPosition(PeriodicWorldTileSize=…)` |
| **5.6** | `FirstPersonTranslatedWorld` transform 基准 · `UE.TransformPosition(FirstPersonInterpolationAlpha=…)` · `Root=` 和 `Path(…)` 的插件挂载校验 · `TextureSample.GatherMode` 往返 · `Substrate.Select` |
| **5.7** | Collection 参数上的 `Group` / `SortPriority` · Layer Blend 输入的 `BlendInputRelevance` · 按平台、按质量等级的材质资源诊断 |
| **5.8** | Custom Pass —— `.dsp` 管线运行、`UE.DreamPassOutput` / `UE.DreamPassBuffer` · `Substrate.Toon` · `.dsi` 对参数集合参数的覆盖 · ThinCustom 实例回写保留下来的 double vector 与 static component mask 覆盖（后两项按引擎自己的头文件判断，带了这些成员的 5.7 也能用） |

未列出的功能在 5.3 到 5.8 上行为一致。节点目录跟着运行中的引擎走：引擎没有的类或枚举取值会明确报错（`DSH5210`、
`DSH5215`、`DSH5294`……），而不是静默降级。见[当前限制](../diagnostics/limitations.md#引擎版本边界)。

<a id="各部分由谁实现"></a>

## 各部分由谁实现

DreamShader 实际上是两个独立产品，通过 `<Project>/Saved/DreamShader/Bridge/` 下的文件和一个本地回环
WebSocket 通信。

| 能力 | 由谁实现 |
| --- | --- |
| 解析、生成、缓存、诊断、反编译、迁移 | Unreal 插件 |
| 菜单、工具栏、右键菜单、Material Content Browser 标签页 | Unreal 插件 |
| 预览渲染和 PNG 帧 | Unreal 插件 |
| Custom Pass 运行时 | Unreal 插件（`DreamShaderPass`） |
| `DShader/Packages` 创建、`import` / `#include` 解析、编译排除规则 | Unreal 插件 |
| 高亮、补全、Hover、跳转、本地诊断 | 编辑器扩展 |
| 预览的相机控制与帧确认 | 编辑器扩展 |
| `dreamshader.package.json`、`dreamshader.lock.json`、安装 / 更新 / 商店 | 编辑器扩展 |

插件的 C++ 从不读取这两个 Package 文件。一个 Package 之所以能被解析，仅仅因为它的文件在
`DShader/Packages` 下。

<a id="仓库"></a>

## 仓库

- [DreamShader](https://github.com/TypeDreamMoon/DreamShader) — Unreal 插件本体：编译器、编辑器工具、Custom Pass 运行时、Agent skills，以及本文档所依据的参考手册。MIT 协议。
  - [VSCode 扩展](https://github.com/TypeDreamMoon/dreamshader-language-support) — 当前 2.1.0：`.dss` / `.dsi` / `.dsp` 与 1.x 源文件的高亮、代码片段、补全、跳转定义、查找引用、Hover、Signature Help、 本地与桥接诊断、材质预览、Package 命令、快速模板。
  - [Rider 插件](https://github.com/tsdaer/dreamshader-language-support) — `.dsm` / `.dsf` / `.dsh` 文件类型、语法与 PSI 解析、高亮、补全、导航、诊断、桥接集成、语义 token、 inlay hints、Package 工具。
  - [Package Store Index](https://github.com/TypeDreamMoon/dreamshader-package-index) — 扩展的 Package 商店读取的索引。带 GitHub topic `dreamshader-package` 的仓库同样可以被发现。

问题反馈到插件仓库的
[issue tracker](https://github.com/TypeDreamMoon/DreamShader/issues)。扩展自身的设置、命令和 Package
商店记录在扩展仓库里，不在这里。

<a id="下一步"></a>

## 下一步

- [安装](installation.md) —— 把它装进项目
- [第一个材质](first-material.md) —— 写一个 `.dss`，然后找到它
- [日常工作流](workflow.md) —— 编译循环
- [Custom Pass](../language-v2/passes.md) —— `.dsp` 管线
- [更新日志](../changelog/dreamshader.md) —— `2.1.2` 改了什么
