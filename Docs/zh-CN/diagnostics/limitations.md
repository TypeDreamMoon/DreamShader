---
title: "限制"
description: "DreamShader 2.1.2 不做的事——1.x Graph 与 .dss 的语言边界、1.x 曾经会执行但现在会明说的行为、反编译器的缺口、材质图层受限于函数级的上限，以及引擎版本与 Custom Pass 的边界。"
---

DreamShaderLang 覆盖的是那些值得以可评审文本形式保留下来的材质编写模式。它不是 Unreal 材质编辑器的替代品，也不是着色器编译器。本页如实列出 **2.1.2** 中边界所在的位置，好让你在这里发现它们，而不是在写了三小时着色器之后才发现。

每一节都链接到深入展开该主题的页面。下面不再重复细节。

**注意：**

**（自 2.0.0 起）越过边界会报错，而不是被静默编译成别的东西。** 1.x 生成器会在不认识的运算符处截断表达式，并悄悄取两个设置中的后一个；如今每一种这样的情况都会在该构造所在的行列给出一个 `DSHnnnn` 码。本页列出的每一处边界，撞上时都会告诉你——码的对照见[诊断](index.md)。

<a id="a-1x-graph-is-not-a-general-purpose-language"></a>

## 1.x 的 `Graph` 不是通用编程语言

`.dsm` / `.dsf` 中的 `Graph` 块构建的是节点图。它只有固定的几种语句形式、四个算术运算符，除 `if` / `else` 之外没有任何控制流。legacy 前端用 2.0 解析器读取它，然后拒绝 1.x 没有的写法：

| 不存在 | 码 | 替代方案 |
| :-- | :-- | :-- |
| `for`、`while`、`do`、`return`、`break`、`continue`、`discard` | `DSH2208` | 手工展开，或者把循环搬进一个函数体是真正 HLSL 的 [`Function`](../language/functions.md) |
| `switch` | `DSH2160` | 一条 `if` 链，或者一次 `StaticSwitchParameter` 调用 |
| 三元 `? :`、`if` 条件之外的比较、`&&`、`\|\|`、`!` | `DSH2200` | `if` / `else`、`lerp`，或者一次 `StaticSwitchParameter` 调用 |
| `%`、位运算与移位运算符 | `DSH2201` | `fmod(a, b)`，或者一个 `Function` |
| `if` 条件中的第二个比较 | `DSH2210` | 嵌套的 `if` 语句 |
| 下标 `v[i]`、数组 | `DSH2202`、`DSH2213` | [swizzle](../graph/swizzle.md)；用 `OutputIndex = k` 按序号取节点输出 |
| 复合赋值与 `++` / `--` | `DSH2205`、`DSH2207` | `a = a + b` |
| C 风格强制转换 | `DSH2203` | 构造器：`float3(x)` |
| 十六进制字面量、嵌套花括号初始化式、表达式内赋值 | `DSH2222`、`DSH2214`、`DSH2204` | 十进制字面量、构造器、拆成多条语句 |
| 矩阵类型 | `DSH4361` | [`UE.TransformVector` / `UE.TransformPosition`](../builtins/ue.md#uetransformvector)，或者 `Function` 函数体内部的矩阵局部变量 |
| 整数运算 | —— | `int`、`uint`、`bool` 和 `half` 在图里都是浮点；两个整数构造器相除是 `DSH4243` |

这是设计使然，不是待办清单。任何真正命令式的东西都属于 `Function` 或 `GraphFunction`——它们的函数体是 HLSL，会被编译进一个 `Custom` 节点——或者属于 `.dss`：上表里的大多数写法在 [DreamShaderLang 2.0](../language-v2/index.md) 中都是合法的，[`dsc migrate`](../tools/migrate.md) 会把整个 1.x 文件改写过去。完整清单见[不支持的写法](../graph/unsupported.md)。

<a id="what-1x-did-still-done-now-said"></a>

### 1.x 曾经会执行、现在仍会执行、但会明说的行为

1.x 写入文档、1.x 源文件仍在依赖的行为照样保留，只是现在每一处都带有一条 warning 或 info——即 [legacy 规则](index.md#1x-messages-and-their-codes)。在 `.dss` 中，同样的文本是错误。

| 写法 | 今天的结果 |
| :-- | :-- |
| 只靠忽略大小写才对得上的名字 | 照样解析，带 warning `DSH5275`（引擎名是 `DSH5276`） |
| GLSL 拼写 `mix`、`fract`、`mod`、`vec3` | 读成 HLSL 名，带 warning `DSH5277`；在 `.dss` 中是错误 `DSH4250` |
| 宽值写进窄位置（`float4` 赋给 `vec3`） | 取其前几个分量，带 info `DSH5289` |
| 给未声明的名字赋值，或把它作为 out 目标传入 | 当场声明一个局部变量，带 info `DSH5292` / `DSH5283` |
| 重复的块头属性、重复的 `Settings` 键、第二个 `Graph` / `Layout` section | 后一个胜出，带 warning `DSH2244`、`DSH3262`、`DSH2258` |
| 材质函数 `Settings` 中的未知键、函数 `Outputs` 条目上的默认值 | 被忽略，带 warning `DSH3263`、`DSH3273` |
| 1.x 的 27 个 `UE.*` 名字之一不读取的参数 | 参数被丢掉，带 warning `DSH5254` |

**警告：**

`if` 和 `else` 是**区分大小写**的关键字。`If (x) { … }` 是对一个名为 `If` 的函数的调用，后面跟着的 `{` 是 `DSH2154`。块关键字（`Shader`、`ShaderFunction`……）和 `SampleTexture2D` 同样区分大小写；section 名、类型 token 和构造器名则不区分。见[词法规则](../language/lexical.md#case-sensitivity)。

<a id="what-dss-does-not-have-either"></a>

## `.dss` 也没有的东西

2.0 语言有循环、`?:`、`&&` 和复合赋值，但它同样被降级成一张材质图，而图有图的边界：

| 没有 | 替代 |
| :-- | :-- |
| 只有运行时才知道次数的循环、分支中的 `discard` | 写进 `/// @custom` 函数 |
| 图里的矩阵 | 在 `/// @custom` 函数体里计算 |
| `switch`、逗号运算符、`let` / `auto` | 一条 `if` 链；把类型写出来 |
| 材质图层**堆栈** | `@layer` 与 `@layerblend` 产出两种函数；堆栈在编辑器里组装 |

见 [DreamShaderLang 2.0 ▸ 它没有的](../language-v2/index.md#what-the-pipeline-does-not-do-yet)。

<a id="diagnostics"></a>

## 诊断

每条诊断都有码、级别和自己的行列（自 2.1.0 起），一个阶段会报出它的全部错误。剩下的边界在别处：

- bridge 不在 commandlet 和 cook 中运行，所以无头运行不产生 `diagnostics.json`、不产生分片，也不产生 `bridge.db` 行——只有日志行，以及一个 `-DiagnosticsOut` 文件。见[Commandlet](../tools/commandlet.md)。
- VirtualFunction 同步服务的消息没有 `DSHnnnn` 码（使用了 `#if` 的源文件报的 `DSH9001` 除外）；它们以 `code = virtual-function-sync` 入库。
- 站点不逐码建页；每个码的消息、原因和处理在插件仓库的 [`Docs/diagnostics/`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md)。

**警告：**

**仍有一处不报：加载失败的纹理。** 写到名为 `Texture` 或 `TextureObject` 的纹理属性上的资产加载失败时，写入的是空值，并被当作成功报告——`[Texture = Path(Game, "Typo")]` 不产生任何诊断地构建，留下一个未绑定的采样器；纹理采样 token 和 `const` 纹理的默认值也走同一条路。采样器出来不对时，先检查资产路径。见[元数据](../../parameters/metadata.md#object-properties)。

<a id="the-decompiler-is-a-migration-aid-not-a-round-trip"></a>

## 反编译器是迁移辅助，不是往返系统

导出一个已有的 `UMaterial`、`UMaterialFunction` 或材质实例，得到的是一个能用的起点，不是保证一致的复现。

- **2.0 反编译器**（*Export .dss* / *Export .dsi*，`dsc decompile`）（自 2.0.0 起）把编译器倒着跑，并且在写盘之前把打印出的文本再解析一遍（否则是 `DSH9089`）。语言表达不了的一切都会被**点名**，写在文件顶部 —— 导入阶段是 `DSH9060`–`DSH9074`（缺失的函数资产、目录之外的类、数组中的 pin……），书写阶段是 `DSH9075`–`DSH9084`。材质实例写成 `.dsi`，Custom Pass 管线写成 `.dsp`。
- **1.x 导出器**（*Export Legacy .dsm / .dsf*）保留着它的 1.5.0 缺口：材质函数设置不会被写出，只写出一组精选的 `UMaterial` 属性，结构体 / 数组类型的节点属性不参与反射，注释文本和 `SortPriority` 被丢弃，图中的环以及没有函数的 `MaterialFunctionCall` 会变成常量，材质实例则被直接拒绝。

| 两者共有 | 后果 |
| :-- | :-- |
| 写出的路径指向 `Decompiled/…` | 重新编译会创建**第二个**资产，而不是替换原来的那个 —— 2.0 反编译器的 `-KeepAssetPath` 会写一行 `/// @name`，接管原路径 |
| 节点布局取决于 *Export Decompiled Layout* 设置 | 关闭时，重新生成的图会自动布局 |

请把一次导出当作草稿：先编译它，在编辑器中对比两个材质，等确认可靠之后再接管原路径。完整清单见 [反编译器 ▸ 已知缺口](../../tools/decompiler.md#known-round-trip-gaps)和
[2.0 反编译器](../../tools/decompiler.md#the-20-decompiler)。

<a id="material-layers-are-function-level-only"></a>

## 材质图层只到函数级

`ShaderLayer` 和 `ShaderLayerBlend`（2.0 里是 `/// @layer` 和 `/// @layerblend`）生成两种图层 *函数* 资产 —— `UMaterialFunctionMaterialLayer` 和 `UMaterialFunctionMaterialLayerBlend`。支持到此为止。

| 边界 | 细节 |
| :-- | :-- |
| 图层**栈**不能从源码编写 | 把图层和混合指派到材质或材质实例上，仍然是编辑器操作 |
| 图层接口是固定的 | layer 总是恰好有一个 `MaterialAttributes` 输入（它所作用的材质）；blend 至少有一个 `MaterialAttributes` 输入，而引擎的图层栈混合两个。两者都恰好产出一个 `MaterialAttributes` 输出。其他任何写法都是 `DSH6204` / `DSH6205` / `DSH3278` |
| 标量、向量和纹理不是图层输入 | 请通过 `Properties` 暴露它们；它们会变成所生成函数内部的参数节点，并出现在图层栈的参数面板上 |
| 图层不能从 `Graph` 调用 | `DSH6208`；它们的使用者是材质或材质实例的图层栈 |
| `BlendInputRelevance` | 仅在（自 UE 5.7 起）按输入名写入 |
| `MaterialLayer` / `MaterialLayerBlend` 已弃用 | 自 1.3.0 起；两者仍然生成完全相同的资产，带警告 `DSH2251` |

（自 2.0.0 起）同一个 `Shader` 上的 `Base.MaterialAttributes` 和 `Base.FrontMaterial` 不再互斥：两者都会接上，*Use Material Attributes* 让材质读取属性集。签名规则见
[顶层块](../language/blocks.md#shaderlayer-and-shaderlayerblend)。

<a id="node-coverage-is-reflection-shaped"></a>

## 节点覆盖面是反射形状的

`UE.<Name>(…)` 可以触达运行中引擎的任何材质表达式类 —— 按短名（`Sine`、`CameraVectorWS`）或按节点目录携带的 1.x 别名；[`UE.Expression(Class = …)`](../builtins/ue-expression.md)
走的是同一条路径。几个后果：

- **结构体和数组属性很别扭。** 它们走 Unreal 自己的文本导入，所以需要 Unreal 的字面量语法 —— `(R=1,G=0,B=0,A=1)`；把 pin 放在结构体数组里的节点（Landscape layer blend 等）按每个节点显示的名字接线。
- **目录跟着引擎走。** 运行中的引擎没有的类就是 `DSH5210`；引擎枚举里没有的取值（比如旧版本引擎上的 `PeriodicWorld` 基）是 `DSH5215`。
- **1.x 的 27 个名字保留 1.x 的参数表。** 它们不读的参数会被丢弃，带警告 `DSH5254`；在 `.dss` 里，同样的参数是错误 `DSH5213`。
- **相同的调用就是一个节点。** IR 会合并在类、属性和输入上相同的节点，不管是哪个内置（自 2.0.0 起）。

<a id="engine-version-boundaries"></a>

## 引擎版本边界

支持的引擎是 **UE 5.3 – 5.8**（Win64）。依赖更新引擎的特性会被明确拒绝，而不是静默处理：

| 特性 | 需要 |
| :-- | :-- |
| `Substrate.*` 调用、`Substrate` 值、`Base.FrontMaterial`、`ShadingModel = Substrate` | （自 UE 5.4 起） |
| 两个 Substrate 值之间的运行时分支（`Substrate.Select`） | （自 UE 5.6 起）—— 低于它是 `DSH4378` |
| `UE.TransformPosition` 上的 `PeriodicWorld` 基 | （自 UE 5.5 起） |
| `UE.TransformPosition` 上的 `FirstPersonTranslatedWorld` 基和 `FirstPersonInterpolationAlpha` | （自 UE 5.6 起） |
| `Path( … )` 和 `Root=` 上的插件内容挂载检查、纹理采样上的 `GatherMode` | （自 UE 5.6 起） |
| `ShaderLayerBlend` 上的 `BlendInputRelevance`、`UE.CollectionParam` 上的 `Group` / `SortPriority` | （自 UE 5.7 起） |
| 运行 Custom Pass 管线、`Substrate.Toon` | （自 UE 5.8 起） |

Substrate 还要求项目启用 Substrate，这一点与引擎版本无关。`DreamShaderPass` 模块在每个支持的引擎上都能构建；pass 只在 5.8 上运行。

<a id="backend-boundaries"></a>

## Backend 边界

`ThinCustom` 是默认 backend：图建在一个隐藏的 base 材质上，资产路径上放着一个 `UDreamShaderMaterialInstance`。它的后果是交互式编译默认是 **Ephemeral** —— 只在内存中 —— 所以生成的材质在你执行 *Materialize* 之前不会出现在 Content Browser 里，而 cook 时会自动落盘。这是刻意的，不是故障；见[内存材质](../generation/ephemeral.md)
和 [Backend](../settings/backend.md)。要一个普通的、每次构建都存盘的 `UMaterial`，请设置 `Backend = "Graph"`。

`Backend = ""` 是错误 `DSH7201`（1.x 读成 `Graph`）；`Instance` 是 `ThinCustom` 的旧拼写，带警告 `DSH7204`。

<a id="imports-and-packages"></a>

## import 与 package

| 边界 | 细节 |
| :-- | :-- |
| import 环被拒绝 | 菱形 import 没问题；成环是 `DSH4211` |
| 每个文件一个 `Shader` | `DSH2250`；现在按文件算，不再跨 import 闭包 |
| 只能 import 一个 `.dsh` | import 一个 `.dsf` / `.dsm` 是 `DSH2252`；另一个文件生成的函数通过 `VirtualFunction` / `extern` 原型调用 |
| 一个 `.dsh` 只放函数和 `VirtualFunction` | 按声明判定，`DSH2249` —— 注释里出现 `Shader(` 没问题 |
| `DShader/Packages` 下的源文件 | `Library/*.dsh` 从不直接编译，可以被 import；`Examples/*.dsm` 在任何路径下都不编译；其余见 [Package](../../tools/packages.md#source-file-enumeration) |
| 解析是相对源码根的 | 机器绝对路径带不走；请让 import 相对文件、源码根或 package |

<a id="custom-pass-boundaries"></a>

## Custom Pass 边界

[Custom Pass](../language-v2/passes.md) 管线仅在（自 UE 5.8 起）上运行。缓冲区的 `Mips` 必须是 `1`
（`DSH7308`（自 2.1.2 起））；mesh pass 无法绘制 Nanite 图元，也没有逐实例剔除；HLSL pass
无法随 DLC 发布；带 `UE.DreamPassOutput` 或 `UE.DreamPassBuffer` 的材质无法使用新的材质
翻译器。完整清单见[运行时 ▸ 限制](../language-v2/passes.md#what-the-compiler-checks)。

<a id="what-is-deliberately-not-a-goal"></a>

## 刻意不做的事

DreamShaderLang 存在的意义，是让那些必须被评审、重新生成和共享的行为以文本形式存在。它不试图
吞并材质编辑器。探索性的、高度视觉化的一次性图在 Unreal 里搭更快 —— 就在那里搭，然后把稳定下来的部分
[反编译](../tools/decompiler.md)出来放进源码。

<a id="where-to-next"></a>

## 继续阅读

  - [诊断](index.md) — 诊断出现在哪里、怎么读，以及常见的码。
  - [不支持的写法](../graph/unsupported.md) — 1.x Graph 语言边界的完整版，每种写法带它的码。
  - [DreamShaderLang 2.0](../language-v2/index.md) — .dss：循环、?: 和 && 都在这里。
  - [反编译器](../tools/decompiler.md) — 导出器会写出什么，以及完整的缺口清单。
  - [Backend](../settings/backend.md) — ThinCustom 与 Graph，以及各自生成什么。
  - [顶层块](../language/blocks.md) — ShaderLayer、ShaderLayerBlend 以及其余的块清单。
