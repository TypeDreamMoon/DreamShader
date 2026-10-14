---
title: "ShaderFunction"
description: "ShaderFunction"
---

<a id="shaderfunction"></a>

# ShaderFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **ShaderFunction**

一个顶层块，声明一个可复用的 Unreal 材质函数：它带类型的输入和输出
引脚、它的函数局部参数节点，以及连接它们的节点图。

| | |
| :-- | :-- |
| 声明于 | `.dsf` 或 `.dsm`，也可位于 `Shader` 旁；在 `.dsh` 中它是 [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)（参见 [源文件](source-files.md#how-the-restriction-is-enforced)） |
| 种类 | 顶层块 |
| 生成 | 带 `EMaterialFunctionUsage::Default` 的 `UMaterialFunction` |
| 多重性 | 每个文件任意数量 |

旧版前端将 `ShaderFunction` 读取为
[2.0 语言](../language-v2/index.md)的 `export` 函数：其输入为参数，第一个名为 `Result` 的输出
是返回值，其他输出为 `out` 参数。[`dsc migrate`](../tools/migrate.md)
就是这样写入的。

<a id="synopsis"></a>

## 概要

```c
ShaderFunction(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { <parameter-declaration> ; … }]
    { Outputs | Results }  [=] { <parameter-declaration> ; … }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

函数至少需要一个输出（[`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315)）以及一个为其输出赋值的 `Graph`
。各节可以按任意顺序出现，也可以重复。节名称与其 `{ … }` 块之间的 `=`
是可选的语法糖*（自 1.5.0 起）*；节结束的
`}` 之后的 `;` 是可选的。

关键字 `ShaderFunction` 按**区分大小写**匹配；节名称按
不区分大小写匹配。参见 [词法元素](lexical.md#case-sensitivity)。

<a id="header-attributes"></a>

## 头部属性

| 属性 | 必需 | 值 | 效果 |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | 字符串 | 资产的逻辑路径。最后一个以 `/` 分隔的片段是资产名称；之前的片段成为文件夹。 |
| `Root` | 否 | 字符串 | 文件夹所挂载的包根。缺省或为空时默认为 `/Game`。 |
| `NoGeneration` | 否 | `true` \| `false` | *（2.1.2 之后）* 只有指定了该文件的编译才会构建此块的资产；参见 [NoGeneration](../generation/no-generation.md)。其他值为 [`DSH2260`](../diagnostics/DSH2xxx.md#dsh2260)。 |

属性键按不区分大小写匹配（`name=` 也可）。值可以加引号或不加引号；不加引号的值
延伸到括号外的下一个 `,` 或 `)`。重复写入的键是
警告（[`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)），后一个值生效。

`)` 之前的尾随逗号是被接受的，与 1.x 一样 *（2.0.0 – 2.1.0 拒绝它并报
[`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243)）*。缺失或为空的 `Name` 是
[`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)。

完整的 `Name` / `Root` 语法、可接受的根拼写以及生成的磁盘路径
在 [资产路径](../generation/asset-paths.md) 中有说明。

<a id="sections"></a>

## 节

| 节 | 是否接受 | 重复行为 | 参考 |
| :-- | :-- | :-- | :-- |
| `Properties` | 是 — 函数局部参数、`const` 和 `UE.*` 节点 | 追加 | [属性](properties.md) |
| `Inputs` | 是 — `UMaterialExpressionFunctionInput` 引脚 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Outputs` | 是 — `UMaterialExpressionFunctionOutput` 引脚 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Results` | 是 — `Outputs` 的别名，无警告 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Settings` | 是 — 三个键被支持，参见[下文](#generated-asset) | 合并；重复写入的键是警告（[`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262)），后一个值生效 | [函数设置](../../settings/function.md) |
| `Graph` | 是 | 后一个生效，并给出警告（[`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258)） | [图](../graph/index.md) |
| `Layout` | 是 | 后一个替换前一个，并给出 `DSH2258` | [布局](layout.md) |
| `Code` | **否** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | — | — |
| 其他任何内容，包括 `Options` | **否** — [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |

同一个正文解析器服务于 `ShaderFunction`、[`ShaderLayer` 和
`ShaderLayerBlend`](shader-layer.md)；上面的节表对这三者完全相同。

<a id="properties-versus-inputs"></a>

## `Properties` 与 `Inputs`

两节都会向生成的函数中放入内容，但它们是不同的语法，生成
不同的节点。

| | `Properties` | `Inputs` |
| :-- | :-- | :-- |
| 语法 | `[const] <type> <name> [= <default>] [[ … ]] ;` | `[opt] <type> <name> [= <default>] [[ … ]] ;` |
| 生成 | 函数图*内部*的参数节点、常量节点或 `UE.*` 内置节点 | 函数接口上的 `UMaterialExpressionFunctionInput` 引脚 |
| 对调用者可见性 | 作为任何使用该函数的材质上的材质参数 | 作为可连线的输入引脚 |

*（自 2.0.0 起）* 两节都按词读取：类型和名称各为一个词，无论它们之间
有多少空白。

`Graph` 从未提及的属性在生成的资产中不产生任何节点 *（自 1.3.2 起）*。

`ShaderFunction` 中的 `Properties` 与
[`VirtualFunction`](virtual-function.md) 中的 `Properties` *不*是同一回事，后者中该关键字是 `Inputs` 的同义词。

<a id="parameter-types"></a>

## 参数类型

`Inputs`、`Outputs` 和 `Results` 接受以下类型标记，按不区分大小写匹配。一个类型是一个
词。

| 标记 | 函数引脚类型 | 分量 |
| :-- | :-- | :-- |
| `StaticBool`、`StaticBoolParameter` | 输入时为 `FunctionInput_StaticBool`（作为输出时读取为 `bool`） | 1 |
| `MaterialAttributes` | `FunctionInput_MaterialAttributes` | — |
| `Substrate` | `FunctionInput_Substrate` | — |
| `float`、`float1`、`half`、`half1`、`int`、`uint`、`bool` | `FunctionInput_Scalar` | 1 |
| `float2`、`half2`、`vec2`、`int2`、`uint2`、`bool2`、`ivec2`、`uvec2`、`bvec2` | `FunctionInput_Vector2` | 2 |
| `float3`、`half3`、`vec3`、`int3`、`uint3`、`bool3`、`ivec3`、`uvec3`、`bvec3` | `FunctionInput_Vector3` | 3 |
| `float4`、`half4`、`vec4`、`int4`、`uint4`、`bool4`、`ivec4`、`uvec4`、`bvec4` | `FunctionInput_Vector4` | 4 |
| `Texture2D` | `FunctionInput_Texture2D` | — |
| `TextureCube` | `FunctionInput_TextureCube` | — |
| `Texture2DArray` | `FunctionInput_Texture2DArray` | — |
| `Texture3D`、`VolumeTexture` | `FunctionInput_VolumeTexture` | — |

| 标记 | 发生什么 |
| :-- | :-- |
| `SamplerState` | 没有任何函数引脚带有采样器：[`DSH4364`](../diagnostics/DSH4xxx.md#dsh4364) *（自 2.0.0 起；1.x 将其设为 `Texture2D` 输入）* |
| `Material Attributes`、`float 4` — 写作两个词的类型 | 该语句不再被读取为 `<type> <name>`：[`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271) *（自 2.0.0 起）* |
| 不命名任何类型的词 | [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) |

引脚类型会在运行中引擎自身的列表中查找；引擎没有的类型（例如
在没有该类型的引擎上使用 `Substrate`）为 [`DSH8228`](../diagnostics/DSH8xxx.md#dsh8228)。

<a id="input-defaults-and-opt"></a>

## 输入默认值与 `opt`

| 形式 | 效果 |
| :-- | :-- |
| `<type> <name>;` | 必填输入；调用方必须提供 |
| `<type> <name> = <expr>;` | 可选输入，其默认为 `<expr>` *（自 2.0.0 起：默认值使输入变为可选——旧规则 L7；1.x 中该引脚仍为必填）* |
| `opt <type> <name>;` | 可选输入（`bUsePreviewValueAsDefault = true`），无预览值 |
| `opt <type> <name> = <expr>;` *（自 1.2.3 起）* | 可选输入；当引脚未连接时，使用 `<expr>` 的值 |

折叠为常量的默认值会被写入 `PreviewValue`。其他任何内容都会构建为图表达式，并连接到输入的 `Preview` 引脚——这正是 `opt Texture2D Tex = PreviewTex;` 引用生成的 `Properties` 项的方式 *（自 1.2.6 起）*。`StaticBool` 输入的默认值会成为 `Preview` 引脚上的 `StaticBool` 节点，因为引擎会忽略该处的 `PreviewValue`。类型与输入不匹配的默认值会像赋值一样被报告
（[`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)；更宽的向量会通过
[`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) 进行裁剪）。

*（自 2.0.0 起）* `opt`，类型和名称都是词：它们之间的制表符与空格效果相同。

<a id="per-parameter-metadata"></a>

## 逐参数元数据

尾部的 `[ … ]` 块可在 `Inputs`、`Outputs` 和 `Results` 声明上使用。只有下列
键会产生效果；与 `Properties` 不同，其他任何键都不会被反射到生成的节点上。

| 元数据键（不区分大小写） | 对输入/输出节点的效果 |
| :-- | :-- |
| `Description`、`Desc`、`Tooltip` | 写入 `Description` |
| `SortPriority`、`Sort` | 为引脚排序：某个 section 的条目会按它稳定排序，没有该键的条目按其声明索引计，资产会按此顺序获得密集的优先级 `0, 1, 2, …` *（自 2.0.0 起）*。不是整数时：[`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) |
| `Group`、`Category`、任何其他键 | 被忽略，且不产生诊断——引擎的函数输入/输出节点没有分组字段 |

格式错误块会引发 [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255)–[`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257)。
完整元数据语法（包括 `Slider(min, max)` 简写）见
[Metadata](../parameters/metadata.md)。

<a id="outputs"></a>

## `Outputs`

第一个输出在名称恰为 `Result` 时，就是函数的返回值：它从零开始，
由 `Graph` 对其进行赋值。其他每个输出都是 `out` 参数，`Graph` 必须对其赋值。

| 规则 | 代码 |
| :-- | :-- |
| 至少必须声明一个输出 | `DSH4315` |
| `Graph` 必须为第一个 `Result` 之外的每个输出赋值 | [`DSH6211`](../diagnostics/DSH6xxx.md#dsh6211) |
| 所赋的值必须符合声明的类型 | `DSH4226`；更宽的向量会通过 `DSH5289` 进行裁剪 |
| 输出上的 `opt` | [`DSH3272`](../diagnostics/DSH3xxx.md#dsh3272)（警告；被忽略） |
| 输出上的 `= <expression>` | [`DSH3273`](../diagnostics/DSH3xxx.md#dsh3273)（警告；被丢弃） |

`MaterialAttributes` 输出会初始为空材质，因此像
`Attrs.BaseColor = …;` 这样的成员写入会有目标。参见 [MaterialAttributes](../graph/material-attributes.md)。

> [!NOTE]
> 输出声明上的初始化器（`vec3 OutColor = InColor;`）会被丢弃并引发 `DSH3273`——
> 函数输出始终只由 `Graph` 驱动。这与 [`Shader`](shader.md) 的
> `Outputs` 不同，后者的初始化器是有意义的。

<a id="generated-asset"></a>

## 生成的资产

`Name` + `Root` 会完全按照
[Asset paths](../generation/asset-paths.md) 中所述解析为包路径。在那里创建的资产是 `UMaterialFunction`，其
`MaterialFunctionUsage` 会被设为 `Default`。

当该路径上已存在资产时，只有当它的类**恰好**是
`UMaterialFunction` 时才会被复用；`UMaterialFunctionMaterialLayer` 之类的子类会被
[`DSH8110`](../diagnostics/DSH8xxx.md#dsh8110)，无关对象则会被
[`DSH8111`](../diagnostics/DSH8xxx.md#dsh8111)。如果该包已存在于磁盘上，而该对象不带有
DreamShader 来源元数据，生成会拒绝改动它
（[`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112)）。参见 [Regeneration](../generation/regeneration.md)。

`Settings` 键会在每次构建时应用到资产，因此从源中移除某个键也会将其从资产中移除：

| 键 | `UMaterialFunction` 字段 | 缺失时 |
| :-- | :-- | :-- |
| `Description` | `Description` | 清空 |
| `ExposeToLibrary` | `bExposeToLibrary`——仅在它为 `true` **且** `LibraryCategories` 非空时设置 *（自 2.0.0 起）* | 设为 `false` |
| `LibraryCategories` | `LibraryCategoriesText`——按逗号拆分，每项去除首尾空白，空项被丢弃 | 清空 |
| `UserExposedCaption` | **不应用** *（自 2.0.0 起）*：警告（[`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264)）；该值会保留用于 `dsc migrate` | — |

`ExposeToLibrary` 的值不是 `true` / `false` 时会产生警告
（[`DSH3265`](../diagnostics/DSH3xxx.md#dsh3265)）并按 `false` 计。其他每个键都会产生警告
（[`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263)）并被忽略——此处没有通用的
反射属性路径，这一点与 [`Shader` 设置](../settings/material.md) 不同。参见
[Function settings](../../settings/function.md)。

<a id="input-and-output-identity-across-regeneration"></a>

### 重新生成时的输入与输出标识

在旧图被清除之前，每个 `FunctionInput` 和 `FunctionOutput` 的 `Id` GUID 会
按引脚名称缓存，并恢复到新建的节点上 *（自 1.3.2 起）*。因此，手工编写的材质中已有的
`MaterialFunctionCall` 节点在重新生成后会保留其连线，**只要引脚名称不变**。重命名输入或输出
等同于删除它并添加一个新的：调用点会失去该连接。

> [!WARNING]
> 重建会替换函数的图。不再包含 DreamShader 生成内容的函数不会被重建
> （[`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207)），在资产编辑器中打开的函数也不会
> （[`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206)）。参见
> [Regeneration](../generation/regeneration.md)。

<a id="calling-a-shaderfunction"></a>

## 调用 ShaderFunction

在同一文件的另一个函数块的 `Graph` 中，被调用者的名称是资产的叶名称，
作为标识符：`Name="Functions/F_Tint"` 对应 `F_Tint`。仅大小写匹配的调用在
匹配唯一时会解析，并伴随 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。当该文件可调用的某个
函数已经具有该名称时——例如来自导入头文件的 `Function`——该块的函数会以另一个名称
声明（`<Leaf>_Asset`，伴随
[`DSH5290`](../diagnostics/DSH5xxx.md#dsh5290)），资产保留其名称，而调用会到达
另一个函数。

`Shader` 会像文件中的任何函数一样，按名称调用其自身 `.dsm` 的 `ShaderFunction`；
该函数的资产会首先构建。*（2.0.0 – 2.1.0 拒绝在同一文件中同时使用二者，并引发
[`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201)。）* 资产块不能被导入（`DSH2249`），因此任何
其他文件都要通过 [`VirtualFunction`](virtual-function.md) 访问生成的资产。

```c
// ShaderFunction(Name="Functions/F_Tint") declared elsewhere in the same .dsf:
vec3 Tinted = F_Tint(BaseColor, Tint);          // single-output call as a value  (since 1.5.0)
F_Tint(BaseColor, Tint, OutColor, OutLuma);     // statement call: inputs, then one target per output
```

参数规则、`default` 参数以及命名参数形式在
[Calls](../graph/calls.md) 中有详细说明。

<a id="notes"></a>

## 说明

- **对单个文件的一次编译会构建其声明的所有资产。** 一个 `.dsm` 或 `.dsf` 可以声明任意数量的 `ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、`VirtualFunction`、`Function`、`GraphFunction` 和 `Namespace` 块，而一个 `.dsm` 则可以在它们旁边声明一个 `Shader` *(2.0.0 – 2.1.0 曾拒绝在函数或层旁边声明带有 `DSH6201` 的 `Shader`)*。一个 `.dss` 会生成材质资产或函数资产，而非两者兼顾，因此 [`dsc migrate`](../tools/migrate.md) 希望先将这些函数移动到 `.dsf` 中。参见 [源文件](source-files.md)。
- **文件类型限制是一种解析行为，按块进行判定** *(自 2.0.0 起)*。包含 `ShaderFunction` 块的 `.dsh` 就是 `DSH2249`；而注释或字符串中提到 `ShaderFunction(` 则没有问题。
- 仅声明函数的文件不需要 `Shader` 块；每个构建出的函数都会向编译输出添加一行 `Generated <Kind> <AssetPath> from <SourceFile>.`（此处 `<Kind>` 为 `MaterialFunction`）。
- 当源哈希未更改 **且** 资产的 `MaterialFunctionUsage` 已与块类型匹配时，会跳过重新生成；此时编译输出会为其显示 `Skipped <AssetPath> from <SourceFile>; …`，与材质一样。参见 [缓存](../generation/caching.md)。
- `Layout` 被接受，并应用于函数自身的图表。
- `Code` 节会被直接拒绝（`DSH2246`）。

<a id="diagnostics"></a>

## 诊断

每个代码都会携带该构造的行号和列号；代码对应页面上有消息内容。

<a id="parse-time"></a>

### 解析期

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH2249` | 该块位于 `.dsh` 头文件中 |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | `ShaderFunction` 之后没有 `(` |
| `DSH2243` | 格式错误的属性列表 |
| `DSH2244` | 某个属性被写了两次（警告） |
| `DSH2242` | 没有 `Name`，或其为空 |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | 头之后没有 `{`，或某个节没有其名称或其 `{` |
| `DSH2245` | 未知的节 |
| `DSH2246` | 一个 `Code` 节 |
| `DSH2258` | 第二个 `Graph` 或 `Layout`（警告） |
| `DSH3271` | 一个 `Inputs` / `Outputs` / `Results` 语句不是 `[opt] <type> <name> [= <default>] [[ … ]] ;` |
| `DSH3272`、`DSH3273` | `opt` / 输出上的默认值（警告） |
| `DSH3255`–`DSH3258` | 格式错误的元数据块 |
| [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261)、`DSH3262` | 格式错误的 `Settings` 语句 / 某个键被写了两次（警告） |
| `DSH3263`、`DSH3264`、`DSH3265` | 被忽略的设置、`UserExposedCaption`、非布尔的 `ExposeToLibrary`（警告） |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 正文从未闭合 |

`Properties`、`Graph` 和 `Layout` 内部的错误列在 [属性](properties.md)、[图表](../graph/index.md) 和 [布局](layout.md) 页面上。

<a id="binding-and-ir"></a>

### 绑定与 IR

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4201` | 某个类型标记未对应任何类型 |
| [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214) | 两个输入或输出共享同一名称 |
| `DSH6211` | `Graph` 从未给输出赋值 |
| `DSH4226`、`DSH5289` | 某个值不适合某个输出，或某个默认值不适合其输入 / 更宽的向量被截断（信息） |
| `DSH4315` | 该函数没有输出 |
| `DSH4364` | 某个输入类型没有函数引脚承载 |
| `DSH5290` | 该块的名称已被文件可调用的某个函数占用，因此其函数被重命名（信息） |

<a id="emit-time"></a>

### 发出期

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH8110`、`DSH8111`、`DSH8112` | 目标路径处存在另一个类的资产、一个无关对象，或一个并非由 DreamShader 生成的资产 |
| [`DSH8113`](../diagnostics/DSH8xxx.md#dsh8113)、[`DSH8114`](../diagnostics/DSH8xxx.md#dsh8114) | 无法创建包或函数 |
| [`DSH8098`](../diagnostics/DSH8xxx.md#dsh8098)–[`DSH8100`](../diagnostics/DSH8xxx.md#dsh8100) | `Name` 解析为空或无效的资产名称 |
| `DSH8206`、`DSH8207` | 该资产已在编辑器中打开，或自生成以来已被编辑 |
| `DSH8228` | 引擎没有该名称的函数输入类型 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 无法创建 `FunctionInput` / `FunctionOutput` 节点 |

完整的跨阶段列表见 [诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Properties = {
        Group("Tint") {
            float Boost = 1.0 [Slider(0, 4); Description="Extra gain applied after tinting"];
        }
        const float Epsilon = 0.001;
    }

    Inputs = {
        vec3  InColor;
        vec3  InTint                [Description="Multiplied with InColor"];
        opt float Strength = 1.0    [Description="Blend amount"; SortPriority=10];
    }

    Outputs = {
        vec3  OutColor              [Description="Tinted colour"];
        float OutLuma;
    }

    Settings = {
        Description        = "Tint helper";
        ExposeToLibrary    = true;
        LibraryCategories  = "DreamShader, Color";
    }

    Graph = {
        vec3 Tinted = InColor * InTint * Boost;
        OutColor    = lerp(InColor, Tinted, Strength);
        OutLuma     = dot(OutColor, vec3(0.2126, 0.7152, 0.0722)) + Epsilon;
    }
}
```

生成的资产：

```text
package     /Game/Functions/F_Tint
object path /Game/Functions/F_Tint.F_Tint
class       UMaterialFunction   (usage: Default)
on disk     <Project>/Content/Functions/F_Tint.uasset      (persist mode only)

interface   in  InColor   Vector3
            in  InTint    Vector3
            in  Strength  Scalar     (optional, preview 1.0; SortPriority=10 keeps it last)
            out OutColor  Vector3
            out OutLuma   Scalar
parameters  Boost         ScalarParameter, group "Tint", slider 0..4
```

<a id="see-also"></a>

## 另见

- [源文件](source-files.md) — `.dsm` / `.dsh` / `.dsf` 各自可以包含哪些块类型
- [Shader](shader.md) — 生成 `UMaterial` 的顶层块
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — 此块的材质层变体
- [VirtualFunction](virtual-function.md) — 声明一个已有的 `UMaterialFunction`，而非生成它
- [属性](properties.md) — `Properties` 节语法、`Group(…)` 作用域和 `const`
- [输入 / 输出 / 结果](inputs-outputs.md) — 完整的带类型参数语法
- [元数据](../parameters/metadata.md) — `[ … ]` 块、`Slider(…)` 和反射式透传
- [类型](types.md) — 完整的类型标记目录和逐上下文有效性矩阵
- [布局](layout.md) — `Node` / `Comment` 放置指令和 `#Region`
- [图表](../graph/index.md) — `Graph` 内部的语句/表达式语言
- [调用](../graph/calls.md) — 从 `Graph` 调用函数、参数形式、`default`
- [函数设置](../../settings/function.md) — 材质函数 `Settings` 所遵循的键
- [资产路径](../generation/asset-paths.md) — `Name=` + `Root=` → 包路径
- [重新生成](../generation/regeneration.md) — 重建后哪些内容会保留、哪些不会
- [缓存](../generation/caching.md) — 源哈希跳过
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每个代码
