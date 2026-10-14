---
title: 当前限制
description: DreamShader 2.1.2 还做不到的事 —— 1.x Graph 与 .dss 的语言边界、1.x 照做但会说出来的事、反编译器的缺口、材质图层只到函数级、引擎版本与 Custom Pass 的边界。
---

DreamShaderLang 覆盖的是那些值得以可评审文本形式保存下来的材质编写模式。它不是 Unreal 材质编辑器的替代品，
也不是着色器编译器。本页是 **2.1.2** 上边界所在位置的清单，好让你在这里发现它们，而不是在写了三小时着色器之后。

每一节都链接到真正展开讲这个主题的页面。下面不重复叙述细节。

**提示：**

**（自 2.0.0 起） 起，越过边界的写法会报错，而不是被静默编译成别的东西。** 1.x 生成器遇到不认识的运算符会截断表达式、
遇到重复的设置会悄悄取后一个；今天每一种这样的情况都有一个 `DSHnnnn` 码，带着出问题那一处的行列。本页列出的边界，
撞上时都会告诉你 —— 码的对照见[错误速查](index.md)。

<a id="1x-的-graph-不是通用编程语言"></a>

## 1.x 的 `Graph` 不是通用编程语言

`.dsm` / `.dsf` 里的 `Graph` 块构建的是节点图。它只有固定的几种语句形式、四个算术运算符，除 `if` / `else` 之外没有任何
控制流。legacy 前端用 2.0 的解析器读它，然后拒绝 1.x 没有的写法：

| 不存在 | 码 | 替代方案 |
| :-- | :-- | :-- |
| `for`、`while`、`do`、`return`、`break`、`continue`、`discard` | `DSH2208` | 手工展开，或者把循环搬进函数体是真正 HLSL 的 [`Function`](../language/functions.md) |
| `switch` | `DSH2160` | `if` 链，或 `StaticSwitchParameter` 调用 |
| 三元 `? :`、`if` 条件之外的比较、`&&`、`\|\|`、`!` | `DSH2200` | `if` / `else`、`lerp`，或者 `StaticSwitchParameter` 调用 |
| `%`、位运算与移位运算符 | `DSH2201` | `fmod(a, b)`，或者一个 `Function` |
| `if` 条件里的第二个比较 | `DSH2210` | 嵌套 `if` 语句 |
| 下标 `v[i]`、数组 | `DSH2202`、`DSH2213` | [swizzle](../graph/expressions.md#swizzle)；按序号取节点输出写 `OutputIndex = k` |
| 复合赋值与 `++` / `--` | `DSH2205`、`DSH2207` | `a = a + b` |
| C 风格强制转换 | `DSH2203` | 构造器：`float3(x)` |
| 十六进制字面量、嵌套花括号初始化式、表达式内赋值 | `DSH2222`、`DSH2214`、`DSH2204` | 十进制字面量、构造器、拆成多条语句 |
| 矩阵类型 | `DSH4361` | [`UE.TransformVector` / `UE.TransformPosition`](../builtins/ue.md#变换内置)，或者 `Function` 体内部的矩阵局部变量 |
| 整数运算 | —— | `int`、`uint`、`bool` 和 `half` 在图里都是浮点；两个整数构造器相除是 `DSH4243` |

这是设计，不是待办清单。任何真正命令式的东西都属于 `Function` 或 `GraphFunction` —— 它们的函数体是 HLSL，会被编译进一个
`Custom` 节点 —— 或者属于 `.dss`：上表里的大多数写法在 [DreamShaderLang 2.0](../language-v2/index.md) 里都是合法的，
[`dsc migrate`](../tools/migrate.md) 会把整个 1.x 文件改写过去。完整清单见[不支持的写法](../graph/unsupported.md)。

<a id="1x-照做但会说出来的事"></a>

### 1.x 照做、但会说出来的事

1.x 写进文档、1.x 源文件仍在依赖的行为照样保留，只是现在每一处都有一条 warning 或 info —— 也就是
[legacy 规则](index.md#legacy-规则)。在 `.dss` 里，同样的写法是错误。

| 写法 | 今天的结果 |
| :-- | :-- |
| 只靠大小写才对得上的名字 | 照样解析，warning `DSH5275`（引擎名是 `DSH5276`） |
| GLSL 拼写 `mix`、`fract`、`mod`、`vec3` | 读成 HLSL 名，warning `DSH5277`；`.dss` 里是错误 `DSH4250` |
| 宽值写进窄位置（`float4` 赋给 `vec3`） | 取前几个分量，info `DSH5289` |
| 给未声明的名字赋值、把未声明的名字当 out 目标 | 当场声明一个局部变量，info `DSH5292` / `DSH5283` |
| 块头属性重复、`Settings` 键重复、第二个 `Graph` / `Layout` section | 后一个胜出，warning `DSH2244`、`DSH3262`、`DSH2258` |
| 材质函数 `Settings` 里的未知键、函数 `Outputs` 条目上的默认值 | 被忽略，warning `DSH3263`、`DSH3273` |
| 1.x 的 27 个 `UE.*` 名字收到它不读的参数 | 参数被丢掉，warning `DSH5254` |

**警告：**

`if` 和 `else` 是**区分大小写**的关键字。`If (x) { … }` 是对一个叫 `If` 的函数的调用，后面的 `{` 报 `DSH2154`。
块关键字（`Shader`、`ShaderFunction`……）和 `SampleTexture2D` 也区分大小写；section 名、类型 token 和构造器名不区分。
见[词法与大小写](../language/lexical.md#大小写敏感性)。

<a id="dss-也没有的"></a>

## `.dss` 也没有的

2.0 语言有循环、`?:`、`&&` 和复合赋值，但它同样降级成一张材质图，图有图的边界：

| 没有 | 替代 |
| :-- | :-- |
| 运行时才知道次数的循环、分支里的 `discard` | 写进 `/// @custom` 函数 |
| 图里的矩阵 | 在 `/// @custom` 函数体里算 |
| `switch`、逗号运算符、`let` / `auto` | `if` 链；把类型写出来 |
| 材质层**堆栈** | `@layer` 与 `@layerblend` 产出两种函数；堆栈在编辑器里组装 |

见 [DreamShaderLang 2.0 ▸ 它没有的](../language-v2/index.md#它没有的)。

<a id="诊断"></a>

## 诊断

每条诊断都有码、级别和自己的行列 （自 2.1.0 起），一个阶段会报出它的全部错误。剩下的边界在别处：

- bridge 不在 commandlet 和 cook 里运行，所以无头运行不产生 `diagnostics.json`、不产生分片、不产生 `bridge.db` 行 ——
  只有日志行，以及 `-DiagnosticsOut` 文件。见[命令行](../tools/commandlet.md)。
- VirtualFunction 同步服务的消息没有 `DSHnnnn` 码（用了 `#if` 的源文件报的 `DSH9001` 除外），以
  `code = virtual-function-sync` 入库。
- 站点不逐码建页；每个码的消息、原因和处理在插件仓库的
  [`Docs/diagnostics/`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md)。

**警告：**

**仍有一处不报：加载失败的纹理。** 写到名为 `Texture` 或 `TextureObject` 的纹理属性上的资产加载失败时，写入的是空、
并被当作成功 —— `[Texture = Path(Game, "Typo")]` 不产生任何诊断，留下一个未绑定的采样器；纹理采样 token 和 `const`
纹理的默认值也走这条路。采样器出来不对时，先检查资产路径。见[元数据与分组](../parameters/metadata.md#对象属性)。

<a id="反编译器是迁移辅助不是往返系统"></a>

## 反编译器是迁移辅助，不是往返系统

导出一个已有的 `UMaterial`、`UMaterialFunction` 或材质实例，得到的是一个能用的起点，不是保证一致的复现。

- **2.0 反编译器**（*Export .dss* / *Export .dsi*，`dsc decompile`）（自 2.0.0 起） 把编译器倒着跑，并且把打印出的文本
  再解析一遍才写盘（不行是 `DSH9089`）。它表达不了的东西一律**点名**，写在文件头部 —— 导入阶段 `DSH9060`–`DSH9074`
  （缺失的函数资产、目录外的类、数组里的 pin……），书写阶段 `DSH9075`–`DSH9084`。材质实例写成 `.dsi`，Custom Pass
  管线写成 `.dsp`。
- **1.x 导出器**（*Export Legacy .dsm / .dsf*）保留着 1.5.0 的缺口：材质函数的 settings 不写出、只写出一组钦定的
  `UMaterial` 属性、struct / array 类型的节点属性不参与反射、注释文本和 `SortPriority` 被丢弃、图里的环和没有指派函数的
  `MaterialFunctionCall` 变成常量、材质实例被直接拒绝。

| 两者共有的边界 | 后果 |
| :-- | :-- |
| 生成的路径指向 `Decompiled/…` | 重新编译会创建**第二个**资产，而不是替换原来那个 —— 2.0 反编译器用 `-KeepAssetPath` 写一行 `/// @name` 接管原路径 |
| 节点布局依赖 *Export Decompiled Layout* 设置 | 关掉时，重新生成的图改为自动布局 |

请把一次导出当成初稿：先编译它，在编辑器里对比两个材质，等确认可靠之后再接管原路径。完整清单见
[反编译导出 ▸ 已知缺口](../tools/decompiler.md#已知缺口)和 [2.0 反编译器](../tools/decompiler.md#20-反编译器)。

<a id="材质图层只到函数级"></a>

## 材质图层只到函数级

`ShaderLayer` 和 `ShaderLayerBlend`（2.0 里是 `/// @layer` 与 `/// @layerblend`）生成两种图层 *函数* 资产 ——
`UMaterialFunctionMaterialLayer` 和 `UMaterialFunctionMaterialLayerBlend`。支持到此为止。

| 边界 | 细节 |
| :-- | :-- |
| 图层 **栈** 不能从源码编写 | 把图层和混合指派到材质或材质实例上，仍然是编辑器操作 |
| 图层接口是固定的 | layer 总是恰好一个 `MaterialAttributes` 输入（它所作用的材质）；blend 至少一个 `MaterialAttributes` 输入，引擎的图层栈混合两个。两者都恰好产出一个 `MaterialAttributes` 输出。违反是 `DSH6204` / `DSH6205` / `DSH3278` |
| 标量、向量和纹理不能作为图层输入 | 请通过 `Properties` 暴露；它们会变成生成函数内部的参数节点，并出现在图层栈的参数面板上 |
| 图层不能从 `Graph` 调用 | `DSH6208`；它们的使用者是材质或材质实例的图层栈 |
| `BlendInputRelevance` | 只在 （自 UE 5.7 起） 上按输入名写入 |
| `MaterialLayer` / `MaterialLayerBlend` 已弃用 | 自 1.3.0 起；仍然生成完全相同的资产，带 warning `DSH2251` |

（自 2.0.0 起） 起，同一个 `Shader` 上的 `Base.MaterialAttributes` 与 `Base.FrontMaterial` 不再互斥：两者都会接上，
*Use Material Attributes* 让材质读取属性集。签名规则见[顶层块](../language/blocks.md#shaderlayer-与-shaderlayerblend)。

<a id="节点覆盖面是反射形状的"></a>

## 节点覆盖面是反射形状的

`UE.<Name>(…)` 触达运行中引擎的任何材质表达式类 —— 按短名（`Sine`、`CameraVectorWS`），或按节点目录携带的 1.x 别名；
[`UE.Expression(Class = …)`](../builtins/ue-expression.md) 是同一条路径。几个后果：

- **结构体和数组属性很别扭。** 它们走 Unreal 自己的文本导入，所以需要 Unreal 的字面量语法 —— `(R=1,G=0,B=0,A=1)`；
  pin 放在结构体数组里的节点（Landscape layer blend 等）按每个节点显示的名字接线。
- **目录跟着引擎走。** 一个类在当前引擎上不存在，调用就是 `DSH5210`；引擎枚举里没有的取值（比如低版本上的
  `PeriodicWorld` 基）是 `DSH5215`。
- **1.x 的 27 个名字保留 1.x 的参数表。** 它们不读的参数被丢掉，带 warning `DSH5254`；在 `.dss` 里同样的参数是错误
  `DSH5213`。
- **相同的调用就是一个节点。** IR 合并类、属性和输入都相同的节点，不论哪个内置 （自 2.0.0 起）。

<a id="引擎版本边界"></a>

## 引擎版本边界

支持的引擎是 **UE 5.3 – 5.8**（Win64）。依赖更新引擎的特性会明确报错，而不是静默处理：

| 特性 | 需要 |
| :-- | :-- |
| `Substrate.*` 调用、`Substrate` 值、`Base.FrontMaterial`、`ShadingModel = Substrate` | （自 UE 5.4 起） |
| 两个 Substrate 值之间的运行时分支（`Substrate.Select`） | （自 UE 5.6 起） —— 更低版本是 `DSH4378` |
| `UE.TransformPosition` 的 `PeriodicWorld` 基 | （自 UE 5.5 起） |
| `UE.TransformPosition` 的 `FirstPersonTranslatedWorld` 基与 `FirstPersonInterpolationAlpha` | （自 UE 5.6 起） |
| `Path( … )` 与 `Root=` 上的插件内容挂载检查、纹理采样的 `GatherMode` | （自 UE 5.6 起） |
| `ShaderLayerBlend` 的 `BlendInputRelevance`、`UE.CollectionParam` 的 `Group` / `SortPriority` | （自 UE 5.7 起） |
| Custom Pass 管线运行 pass、`Substrate.Toon` | （自 UE 5.8 起） |

Substrate 还要求项目里启用 Substrate，这一点与引擎版本无关。`DreamShaderPass` 模块在每个支持的引擎上都能编译，
只是 pass 只在 5.8 上运行。

<a id="backend-边界"></a>

## Backend 边界

`ThinCustom` 是默认 backend：图建在一个隐藏的 base 材质上，资产路径上放的是一个 `UDreamShaderMaterialInstance`。
它的后果是交互式编译默认是 **Ephemeral**（只在内存里）—— 生成的材质在你执行 *Materialize* 之前不会出现在
Content Browser 里，cook 时自动落盘。这是刻意的，不是故障；见[内存材质](../generation/ephemeral.md)和
[Backend](../settings/backend.md)。要一个普通的、每次构建都存盘的 `UMaterial`，设 `Backend = "Graph"`。

`Backend = ""` 是错误 `DSH7201`（1.x 读成 `Graph`）；`Instance` 是 `ThinCustom` 的旧拼写，带 warning `DSH7204`。

<a id="import-与-package"></a>

## import 与 package

| 边界 | 细节 |
| :-- | :-- |
| import 环被拒绝 | 菱形 import 没问题；成环是 `DSH4211` |
| 每个文件只能有一个 `Shader` | `DSH2250`；按文件算，不再跨 import 闭包 |
| 只能 import `.dsh` | import `.dsf` / `.dsm` 是 `DSH2252`；另一个文件生成的函数通过 `VirtualFunction` / `extern` 原型调用 |
| `.dsh` 只放函数和 `VirtualFunction` | 按声明判定，`DSH2249` —— 注释里出现 `Shader(` 没问题 |
| `DShader/Packages` 下的源文件 | `Library/*.dsh` 从不直接编译、可以被 import；`Examples/*.dsm` 在任何路径下都不编译；其余见 [Package](../tools/packages.md#源文件枚举) |
| 解析是相对源码根的 | 机器绝对路径带不走；请让 import 相对文件、源码根或 package |

<a id="custom-pass-边界"></a>

## Custom Pass 边界

[Custom Pass](../language-v2/passes.md) 管线只在 （自 UE 5.8 起） 上运行。缓冲区的 `Mips` 只能是 `1`（`DSH7308`
（自 2.1.2 起））；mesh pass 画不了 Nanite 图元、没有逐实例剔除；HLSL pass 不能放进 DLC；带 `UE.DreamPassOutput` 或
`UE.DreamPassBuffer` 的材质不能用新的材质翻译器。完整清单见[运行时 ▸ 限制](../runtime/index.md#限制)。

<a id="刻意不做的事"></a>

## 刻意不做的事

DreamShaderLang 存在的意义，是让那些必须被评审、重新生成和共享的行为以文本形式存在。它不打算吞并材质编辑器。
探索性的、高度视觉化的一次性图，在 Unreal 里搭更快 —— 就在那里搭，然后把稳定下来的部分
[反编译](../tools/decompiler.md)出来放进源码。

<a id="继续阅读"></a>

## 继续阅读

- [错误速查](index.md) — 诊断出现在哪里、怎么读，以及常见的码。
  - [不支持的写法](../graph/unsupported.md) — 1.x Graph 语言边界的完整版，每种写法带它的码。
  - [DreamShaderLang 2.0](../language-v2/index.md) — .dss：循环、?: 和 && 都在这里。
  - [反编译导出](../tools/decompiler.md) — 导出器会写出什么，以及完整的缺口清单。
  - [Backend](../settings/backend.md) — ThinCustom 与 Graph 的取舍，以及各自生成什么。
  - [顶层块](../language/blocks.md) — ShaderLayer、ShaderLayerBlend 以及其余的块清单。
