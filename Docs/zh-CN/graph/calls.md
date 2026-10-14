---
title: "调用"
description: "调用"
---

<a id="calls"></a>

# 调用

> [DreamShader](../index.md) » [Graph](index.md) » **调用**

从 `Graph` 块中调用 `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction` 或带输入的参数，既可以作为值表达式，也可以作为独立语句。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf` — 位于 `Graph { … }` 函数体或 `Outputs` 绑定表达式内部 |
| 种类 | 表达式形式和语句形式 |
| 生成 | `UMaterialExpressionCustom`（`Function`、`GraphFunction`）、`UMaterialExpressionMaterialFunctionCall`（`ShaderFunction`、`VirtualFunction`）、`UMaterialExpressionStaticSwitchParameter`（静态开关调用），或参数自身的节点并接好其输入引脚（参数调用） |

`.dsm` / `.dsf` 由 legacy 前端读取，由 2.0 绑定器绑定 *(自 2.0.0 起)*。本页的调用写法构建的节点和以前一样；1.x 调用与 `.dss` 调用读法不同的地方，由编号的 legacy 规则处理，并用自己的码说明。每种调用的 `.dss` 写法见 [`dsc migrate`](../../tools/migrate.md#what-the-rewrite-does)。

<a id="synopsis"></a>

## 语法概要

```c
// value form — the call produces a value
<target> = <callee> ( [ <argument> [ , <argument> ] … ] ) ;

// statement form — the call writes into named out variables
<callee> ( [ <input-argument> , ] … <out-target> [ , <out-target> ] … ) ;

<callee>         := <identifier> | <namespace> :: <identifier>
<argument>       := <expression> | <identifier> = <expression> | default
<out-target>     := <identifier>
```

`( ) , = ::` 和 `default` 是字面 DreamShaderLang 文本。`[ … ]`、`{ a | b }` 和 `…` 是元记号。在任何调用里，命名参数之后再出现位置参数都是 [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) *(自 2.0.0 起)*。

<a id="callable-kinds"></a>

## 可调用的种类

| 种类 | 声明方 | 值形式 | 语句形式 | 命名参数 | 产生的节点 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `Function` | [`Function`](../language/function.md) | 支持——返回值，或第一个 `out` *(自 2.0.0 起)* | 支持 | 支持 *(自 2.0.0 起)* | `Custom` |
| `GraphFunction` | [`GraphFunction`](../language/graph-function.md) | 支持——同 `Function` *(自 2.0.0 起)* | 支持 | 支持 *(自 2.0.0 起)* | `Custom` |
| `ShaderFunction` | [`ShaderFunction`](../language/shader-function.md)，位于同一文件 | 支持，任意输出数 | 支持 | 支持 | `MaterialFunctionCall` |
| `ShaderLayer`、`ShaderLayerBlend` | [`ShaderLayer`](../language/shader-layer.md) | **不可调用**：[`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *(自 2.0.0 起)* | — | — | — |
| `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) | 支持，任意输出数 | 支持 | 支持 | `MaterialFunctionCall` |
| `StaticSwitchParameter` 属性 | [`Properties`](../language/properties.md) | 支持 | 不支持 | 支持 | `StaticSwitchParameter` |
| 带输入的参数 | [`Properties`](../language/properties.md) | 支持 | 不支持 | **只能**用命名参数 | 该参数自身的节点，并完成配置 |

别的文件里的 `ShaderFunction` 要经由指向其资产的 `VirtualFunction` 调用：`import` 只引入 `.dsh` 头文件，而头文件里不放资产块。

`UE.*`、`Substrate.*`、数学内置、构造器和 `SampleTexture2D` 也使用调用语法，但它们会在上述任何种类之前被解析，并有单独的文档——见[内置](../builtins/index.md)、[数学内置](../builtins/math.md)和[构造器](constructors.md)。与数学内置同名的函数是 [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) *(自 2.0.0 起)*。完整的分发顺序见[名称解析](name-resolution.md#call-names)。

<a id="callee-spellings"></a>

## 被调方的写法

| 写法 | 解析到 |
| :-- | :-- |
| `Name` | 以该名字声明的函数，**区分大小写**比较 *(自 2.0.0 起)*。只在忽略大小写时恰好匹配一个函数的名字会解析到它，并警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) |
| `Namespace::Name` | 声明在 `Namespace(Name="Namespace")` 内部的 `Function` / `GraphFunction`。解析器把 `N::F` 读成一个标识符 `N_F`，即函数声明所用的名字 *(自 2.0.0 起)*，所以 `N_F(…)` 也能调用到它；裸的 `F` 不行。后面没有名字的 `::` 是 [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) |
| `DreamShaderFn_Name` | 不再是函数的别名 *(自 2.0.0 起)*：[`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208) |
| 路径末段 | 对声明为 `Name="Functions/F_Tint"` 的 `ShaderFunction`，取最后一个 `/` 之后的一段——`F_Tint`——其中不是字母、数字或 `_` 的字符换成 `_`。`VirtualFunction` 的 `Name` 本身就必须是标识符（[`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)） |

没有重载。一个名字只声明一样东西：再声明一个同名的函数、属性或结构体是 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。例外是叶子名被它之前声明的函数（通常来自导入的头文件）占用的 `ShaderFunction` 块：块的函数改以 `<Name>_Asset` 声明，资产保留原名（提示 [`DSH5290`](../diagnostics/DSH5xxx.md#dsh5290)）。只差大小写的两个名字是两个声明；拼法与两者都不同的调用就没有唯一的不区分大小写匹配，报 `DSH4208`。

<a id="value-form"></a>

## 值形式

```c
float L = Luma(BaseColor);
vec3  C = Common::ApplyTint(BaseColor, Tint);
vec3  N = F_Normal(uv, Output="Normal");
```

*(单输出 `Function` / `GraphFunction` 值调用自 1.3.1 起)*

| 种类 | 要求 |
| :-- | :-- |
| `Function`、`GraphFunction` | 值是返回值；带 `out` 参数的函数则是它的第一个 `out`（legacy 规则 L3b）*(自 2.0.0 起；1.x 拒绝对多输出函数做值调用)*。其他输出用[输出选择器](#output-selection)读取。参数按声明顺序和名字填充参数：没有得到参数的输入是 [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217)，超出最后一个参数的参数是 [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224)。顺序也计入 `out` 参数：落在其上的参数接收那个输出，和语句调用一样。 |
| `ShaderFunction`、`VirtualFunction` | 值是输出 0——第一个声明的输出——除非[输出选择器](#output-selection)选了别的 *(自 2.0.0 起；1.x 在声明了多个输出时要求写选择器)*。声明为 `opt` 的输入可以省略或传 `default`。 |

`GraphFunction` 调用和 `Function` 调用一样是一个 Custom 节点；从函数体里提出来的 `UE.` 调用成为该节点的更多输入，用这次调用的参数降级。

被调方相同、参数相同的调用是一个节点；见[节点复用](node-reuse.md)。

<a id="statement-form"></a>

## 语句形式

```c
F_PulseTint(BaseColor, Tint, TintedColor, PulseAmount);
```

*(多输出 `ShaderFunction` / `VirtualFunction` 语句调用自 1.3.5 起)*

调用语句可以调用任何东西。没人接收其值的调用——内置、构造器、参数——会被接受，它产生的节点因为没人读而被丢掉 *(自 2.0.0 起；1.x 拒绝这样的语句)*。既不是调用也不是赋值的表达式语句是 [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211)（`F(a).Out;`、`x;`）；`x++;` 是 [`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207)，`break;` 是 [`DSH2208`](../diagnostics/DSH2xxx.md#dsh2208)。

函数语句调用的参数这样读：

| 参数 | 读作 |
| :-- | :-- |
| 全部为位置参数；每个输出至少一个，每个输入和输出至多一个 | 最后几个接收输出，每个一个，按声明顺序——函数有返回值时返回值排第一；它们前面的是输入，按顺序（legacy 规则 L5） |
| 任何命名参数，或数量超出该范围 | 像 `.dss` 调用一样按声明顺序和名字匹配到参数：太多是 `DSH4224`，没有参数的输入或输出是 `DSH4217` |

在第一种读法中，未被覆盖的输入必须声明为 `opt` 或传入 `default`，否则
`DSH4217`。每个声明的输出都必须收到一个目标——无法丢弃其中一个。

<a id="out-target-rules"></a>

### out 目标规则

| 规则 | 失败 |
| :-- | :-- |
| out 目标是可以被赋值的东西：一个变量，或它的成员或 swizzle *（自 2.0.0 起；1.x 只接受普通名字）* | [`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239) |
| 任何地方都未声明的名字在此声明，作为该输出类型的局部变量（legacy 规则 L5） | 提示 [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283) |
| 已声明的变量类型为该输出的类型。比变量更宽的数值输出给出其前导分量，标量输出则填满它；其他一律拒绝 | [`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218) |
| 一个变量被命名为两个输出 | 不报告 |

> [!NOTE]
> out 目标的拼写与某个声明只差大小写时，它就是那个声明，并带有 `DSH5275` ——
> 而不是第二个变量 *（自 2.0.0 起）*。只有当任何大小写下都没有声明某个名字时，它才会
> 成为新的局部变量。

<a id="arguments"></a>

## 参数

<a id="positional-and-named"></a>

### 位置参数与命名参数

| 被调方 | 位置参数 | 命名参数 |
| :-- | :-- | :-- |
| `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction` | 按声明顺序填充参数 | 按名字匹配参数 *（`Function` / `GraphFunction` 自 2.0.0 起）*；另一种大小写以 `DSH5275` 解析；没有该参数是 [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) |
| 带输入引脚的参数 | 拒绝：[`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259) | 必须：该节点的一个引脚 |
| `StaticSwitchParameter` | 下标 0 = true 分支，下标 1 = false 分支 | `True`/`A`、`False`/`B` |

位置参数在前、命名参数在后；反过来是 `DSH2158`。同一个参数被给了两次——按位置和按名字，或按名字两次——是 [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215)。
在语句调用中，命名参数会关闭末尾接收输出的读法（见
[语句形式](#statement-form)）。

参数名精确比较，带上面的 `DSH5275` 回退；节点引脚在另一种大小写下匹配时警告 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)。1.x 前端
自己读取的名字——`Output`、`OutputName`、`OutputIndex`、`True`、`False`、`A`、`B`——匹配时
忽略大小写。

<a id="default"></a>

### `default`

*（自 1.2.3 起）*

`default` 是一个裸标识符，用于占住一个有意留空的输入（legacy 规则 L25）。
它**区分大小写**地匹配 *（自 2.0.0 起）*，且仅在没有声明同名变量时生效。

| 情况 | 行为 |
| :-- | :-- |
| 对 `ShaderFunction` / `VirtualFunction` 的输入传 `default` | 输入引脚保持未连接；函数自身的默认值生效。 |
| 对不是 `opt` 的输入传 `default` | 同上 *（自 2.0.0 起；1.x 拒绝）*。 |
| 在 `Function` / `GraphFunction` 调用里用 `default` | 接受 *（自 2.0.0 起）*；Custom 节点不为该参数获得输入。 |
| `Default`、`DEFAULT` | 普通名字：没有声明同名对象时是 [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)。 |

完全省略末尾的可选输入，效果与传 `default` 相同。

<a id="output-selection"></a>

## 输出选择

`Output`、`OutputName` 和 `OutputIndex` 是 1.x 前端自己读取的命名参数：它会将它们从调用中移除，改为选择输出，
如同在调用之后写 `.Name` 或 `[k]`。它们
适用于任何函数种类的调用 *（`Function` 和 `GraphFunction` 自 2.0.0 起）*，也适用于 `UE.*`
调用。

| 参数 | 接受 | 含义 |
| :-- | :-- | :-- |
| `Output` | 带引号的名字或标识符 | 选择该名字的输出——先精确匹配，再不区分大小写并带 `DSH5275` |
| `OutputName` | 同上 | `Output` 的完全同义词 |
| `OutputIndex` | 0 或更大的整数字面量 | 按从 0 起的下标选择：按声明顺序的输出，返回值作为输出 0 |

| 规则 | 失败 |
| :-- | :-- |
| `Output`/`OutputName` 与 `OutputIndex` 互斥 | [`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252) |
| `OutputIndex` 必须是 0 或更大的整数 | [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) |
| `Output` / `OutputName` 必须是带引号的名字或标识符，不能是表达式 | [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) |
| 构造器不接受选择器 | [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253) |
| 参数调用不接受选择器 | [`DSH5265`](../diagnostics/DSH5xxx.md#dsh5265) |
| 名字必须匹配函数的某个输出 | [`DSH5280`](../diagnostics/DSH5xxx.md#dsh5280)。在返回值的函数上，不是输出名的名字会改按值的 swizzle 读取（不是 swizzle 时 [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230)） |
| 下标必须在输出范围内 | [`DSH5282`](../diagnostics/DSH5xxx.md#dsh5282) |
| 多输出函数不带选择器调用 | 不是错误 *（自 2.0.0 起）*：调用读取输出 0 |
| 所选输出必须存在于已加载的资产上——先按名字匹配，再不区分大小写，然后（对 `VirtualFunction`）按其声明位置匹配 | 发射资产时 [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221) |

> [!WARNING]
> 选择器使调用成为一个值。它不能用在语句调用中：被选中的调用是一个无人接收的表达式，
> 即 `DSH2211`。

`UE.Expression(…)` 接受同样的三个选择器；节点没有的名字是
[`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201)，超出其输出的下标是 `DSH5282`。见
[UE.Expression](../builtins/ue-expression.md)。

<a id="breakoutfloatncomponents"></a>

### `BreakOutFloatNComponents`

名为 `BreakOutFloat2Components`、`BreakOutFloat3Components` 或 `BreakOutFloat4Components`
（任意大小写）、第一个参数是位置参数、且带有输出选择器的调用会被**读作
第一个参数的 swizzle**。不会产生 `MaterialFunctionCall` 节点，也不会查找该名字的
函数 *（自 2.0.0 起）*。所选输出指名一个通道，其字母不区分大小写：

| 输出名 | 通道 |
| :-- | :-- |
| `x`、`r`、`"0"` | 0 |
| `y`、`g`、`"1"` | 1 |
| `z`、`b`、`"2"` | 2 |
| `w`、`a`、`"3"` | 3 |

`OutputIndex=` 按下标选择同一通道。该通道必须位于名字所说的宽度之内（`BreakOutFloat2Components` 为 0–1）。否则
——没有选择器、第一个参数是命名参数、通道超出宽度——该调用就是对同名函数的一次普通调用。两个选择器同时给出
是 `DSH5252`。

<a id="calling-a-parameter"></a>

## 调用参数

<a id="input-pin-wiring"></a>

### 输入引脚接线

*(自 1.4.1 起)*

节点自带输入引脚的已声明参数可以被“调用”，以接好这些引脚。前端把这类属性的**每一次**使用——调用或裸读取——都写成一次独立的
`UE.Expression(Class = "<type>", ParameterName = "<name>", …)` 调用，声明的默认值和元数据作为参数，调用的参数接到引脚上 *(自 2.0.0 起)*。参数相同的使用经过去重 pass（[dedupe pass](node-reuse.md)）后是一个节点；裸读取和引脚调用的参数不同，所以是指向同一个材质参数的两个节点。纹理采样参数读的是它的 `RGBA` 输出。

接受这种形式的参数节点类型完整清单：

| | | |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `StaticComponentMaskParameter` | `TextureSampleParameter2D` |
| `TextureSampleParameter2DArray` | `TextureSampleParameterCube` | `TextureSampleParameterCubeArray` |
| `TextureSampleParameterVolume` | `TextureSampleParameterSubUV` | `RuntimeVirtualTextureSampleParameter` |
| `SparseVolumeTextureSampleParameter` | | |

引脚名是节点自己的、引擎目录里列出的名字；另一种大小写带 `DSH5276` 解析。每个参数都必须是命名的（`DSH5259`），每个值都像任何 `UE.` 参数一样按引脚检查（[`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)）。

> [!NOTE]
> 资产槽位——采样器参数指向的贴图、曲线或字体——是用 `[TextureSlot=Path(…)]` 这类声明元数据设置的，它会成为节点的一个属性参数。
> 调用参数再指名声明已经设置过的属性是 `DSH4215`。既不是引脚也不是节点属性的名字是 [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)；当
> 它的值是图值时改为提示 [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)，节点建出来后按这个名字接引脚
> （节点上没有这个引脚时 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)）。

<a id="staticswitchparameter"></a>

### `StaticSwitchParameter`

*(自 1.2.3 起)*

`StaticSwitchParameter` 属性**不**作为裸标识符解析（`DSH4200`）；它需要两个分支，所以只能
通过调用形式读取。

| 分支 | 参数名，按查找顺序 |
| :-- | :-- |
| true | 先 `True=`，再 `A=`，再位置下标 0 |
| false | 先 `False=`，再 `B=`，再位置下标 1 |

两个分支都必须存在（[`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258)）；其他参数被丢弃并警告 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)。该调用成为一个
`UE.Expression(Class = "StaticSwitchParameter", …)`，两个分支接在它的 `A` 和 `B` 引脚上，
像任何 `UE.` 节点的引脚一样检查 *(自 2.0.0 起)*。

```c
vec3 Albedo = UseDetail(True = DetailColor, False = BaseColor);
```

<a id="recursion-and-nesting"></a>

## 递归与嵌套

| 情况 | 行为 |
| :-- | :-- |
| `GraphFunction` 经由提出来的 `UE.` 调用直接或间接再调用自己 | [`DSH6330`](../diagnostics/DSH6xxx.md#dsh6330)：每次调用都会产生一个新的 Custom 节点 |
| `Function` 函数体互相调用成环 | [`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260)：它们的函数体要嵌进同一个 Custom 节点 |
| 同一文件的 `ShaderFunction` 块互相调用成环 | [`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299) |
| 一条表达式里嵌套调用 | 合法——参数就是普通表达式，所以 `F(G(x), 2.0)` 对任何可调用种类都成立。 |
| 把调用用作 `Outputs` 绑定表达式 | 合法——绑定就是完整表达式。 |

<a id="diagnostics"></a>

## 诊断

每个诊断都有稳定的代码；代码的页面里有消息和处理方法。消息以 `<file>(<line>,<col>): DSHnnnn: <message>` 报告，位置在它
所指的调用或参数上。

<a id="resolution-and-dispatch"></a>

### 解析与分发

| 代码 | 触发时机 |
| :-- | :-- |
| `DSH4208` | 被调方既不是函数、内置、结构体，也根本不是名字（在加括号的表达式上调用）。 |
| `DSH5275` | *（警告）* 名字只在忽略大小写时才匹配到某个函数或内置。 |
| [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) | *（警告）* `mix`、`fract`、`mod` 或 `inversesqrt`：内置的 GLSL 拼法。 |
| `DSH4210` | 两个声明共用一个名字。 |
| `DSH5290` | *（提示）* `ShaderFunction` 块因为名字被占用而以 `<Name>_Asset` 声明。 |
| `DSH6206` | 函数与内置同名。 |
| `DSH6208` | 调用 `Shader`、`ShaderLayer` 或 `ShaderLayerBlend`。 |
| `DSH5260` | `::` 后面没有名字。 |
| `DSH2211` | 语句是一个没人接收其值的表达式。 |
| `DSH2158` | 位置参数出现在命名参数之后。 |

<a id="arguments-and-out-targets"></a>

### 参数与 out 目标

| 代码 | 触发时机 |
| :-- | :-- |
| `DSH4224` | 参数比函数的参数多。 |
| `DSH4216` | 命名参数指名的参数不存在。 |
| `DSH4215` | 同一个参数给了两次。 |
| `DSH4217` | 不是 `opt` 的输入没有得到参数；或在按名字读取的调用里，某个输出没有目标。 |
| [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) | 输入参数无法转换到参数的类型。 |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) | *（提示）* 比参数更宽的输入参数取前导分量。 |
| `DSH4239` | out 目标不能被赋值。 |
| `DSH4218` | out 目标的类型接不住这个输出。 |
| `DSH5283` | *（提示）* 没在任何地方声明过的 out 目标成为局部变量。 |
| `DSH4200` | 在另一种大小写下写 `default`，或任何其他未声明的名字被读作值。 |

<a id="output-selection-1"></a>

### 输出选择

| 代码 | 触发时机 |
| :-- | :-- |
| `DSH5252` | 同时给了 `Output`/`OutputName` 和 `OutputIndex`。 |
| `DSH5250` | `OutputIndex` 不是 0 或更大的整数。 |
| `DSH5251` | `Output` 既不是带引号的名字也不是标识符。 |
| `DSH5253` | 在构造器上用了选择器。 |
| `DSH5265` | 在参数调用上用了选择器。 |
| `DSH5280` | 函数没有这个名字的输出。 |
| `DSH5282` | 下标超出了输出范围。 |

<a id="function"></a>

### `Function`

| 代码 | 触发时机 |
| :-- | :-- |
| [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) | 参数或结果类型不是类型。 |
| [`DSH6305`](../diagnostics/DSH6xxx.md#dsh6305) | 函数既不返回值，也没有 `out` 参数。 |
| [`DSH6304`](../diagnostics/DSH6xxx.md#dsh6304) | 函数同时有返回类型和 `out` 参数。 |
| [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | `Substrate` 输入或结果：Custom 节点没有 Substrate 引脚。 |
| [`DSH6254`](../diagnostics/DSH6xxx.md#dsh6254) | 纹理结果。 |
| [`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210) | `MaterialAttributes` 输入。 |
| [`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257) | *（警告）* 有返回类型，但函数体从不返回值。 |
| [`DSH6308`](../diagnostics/DSH6xxx.md#dsh6308) | 带返回类型的函数里出现裸 `return;`。 |
| `DSH6260` | 函数体互相调用成环。 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建该 Custom 节点。 |

<a id="graphfunction"></a>

### `GraphFunction`

`Function` 之下的全部内容，以及：

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH6314`](../diagnostics/DSH6xxx.md#dsh6314) | 函数体中的 `UE.` 调用没有闭合的 `)`。 |
| [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316) | *（警告）* 函数体中的 `Substrate.` 调用未被提升为节点。 |
| [`DSH6325`](../diagnostics/DSH6xxx.md#dsh6325) | 被提升的 `UE.` 调用读取了调用方的变量，这在 1.x 中是允许的。 |
| [`DSH6326`](../diagnostics/DSH6xxx.md#dsh6326) | 被提升的 `UE.` 调用读取了一个既不是参数、也未在文件作用域声明的名字。 |
| [`DSH6329`](../diagnostics/DSH6xxx.md#dsh6329) | 被提升的 `UE.` 调用产生了一个没有任何 Custom 节点输入承载的值（纹理可以；材质或 Substrate 值不行）。 |
| `DSH6330` | 函数通过其被提升的调用到达自身。 |

被提升的调用像任何其他表达式一样绑定，所以其中的错误会以其所在表达式自身的代码报告。

<a id="shaderfunction-family-and-virtualfunction"></a>

### `ShaderFunction` 家族与 `VirtualFunction`

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312) | `VirtualFunction` 没有 `Asset` 选项。 |
| [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313) | `VirtualFunction` 未声明任何输出。 |
| `DSH6311` | `VirtualFunction` 的 `Name` 缺失或不是标识符。 |
| [`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315) | `ShaderFunction` 不产生任何输出。 |
| [`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270) | `Asset` 引用未解析为资产路径。 |
| [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) | 函数资产无法加载，或无法赋给调用节点（一个调用自身的函数）。 |
| [`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) | 资产没有该名称的输入（对于 `VirtualFunction`，也没有声明位置处的输入）。 |
| `DSH8221` | 资产没有该名称的输出。 |
| `DSH8214` | 发射器无法创建 `MaterialFunctionCall` 节点。 |

<a id="parameter-call-forms"></a>

### 参数调用形式

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH5259` | 引脚调用中出现位置参数。 |
| `DSH5276` | *（警告）* 引脚名仅在忽略大小写时匹配。 |
| `DSH5214` | 值放不进它的引脚。 |
| `DSH4215` | 某个引脚或属性被给了两次，或由调用给出，或由声明和调用共同给出。 |
| `DSH5213` | 一个既不是引脚也不是属性的名字，其值又不是图值。 |
| `DSH5291` | *（信息）* 目录中未列出、但在节点存在后按名字连接的名字。 |
| `DSH8212` | 活动节点没有该名称的引脚。 |
| `DSH5258` | 一个缺少两个分支的 `StaticSwitchParameter` 调用。 |
| `DSH5254` | *（警告）* `StaticSwitchParameter` 调用中不是分支的参数，被丢弃。 |
| `DSH4200` | 没有调用却读取 `StaticSwitchParameter`。 |

<a id="example"></a>

## 示例

```c
import "Helpers.dsh";

Shader(Name="Docs/M_Calls")
{
    Properties {
        vec3                   Tint    = vec3(1.0, 0.4, 0.1);
        TextureSampleParameter2D Albedo = Path(Game, "Textures/T_Albedo");
        StaticSwitchParameter  UseTint = true;
    }

    Settings {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        vec2 UV  = UE.TexCoord(Index = 0);

        // Parameter pin call: wires the sampler's Coordinates pin, read through RGBA.
        vec4 Tex = Albedo(Coordinates = UV);

        // Value form, Function with a return value, declared in Helpers.dsh.
        float L  = Luma(Tex.rgb);

        // Value form, namespaced Function: the value is its first out parameter.
        vec3 Lit = Common::ApplyTint(Tex.rgb, Tint);

        // Statement form: inputs first, then one out target per output. Neither target is
        // declared, so each becomes a local of its output's type (info DSH5283).
        PulseTint(Lit, Tint, Pulsed, Amount);

        // StaticSwitchParameter call selects between the two.
        Color = UseTint(True = Pulsed, False = vec3(L, L, L));
    }
}
```

`Helpers.dsh`：

```c
Function float Luma(in vec3 color) { return dot(color, float3(0.299, 0.587, 0.114)); }

Function PulseTint(in vec3 color, in vec3 tint, out vec3 result, out float amount) {
    amount = 0.5 + 0.5 * sin(color.r * 6.28318);
    result = color * tint * amount;
}

Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

生成的节点：

```text
TextureCoordinate                      -> UV
TextureSampleParameter2D  Albedo       -> Tex          (Coordinates pin wired to UV)
Custom  "Luma"                         -> L
Custom  "Common::ApplyTint"            -> Lit
Custom  "PulseTint"                    -> Pulsed (output result), Amount (output amount)
AppendVector, AppendVector             -> vec3(L, L, L)
StaticSwitchParameter  UseTint         -> Color
```

`PulseTint` Custom 节点的输出按函数的 `out` 参数命名——`result` 是输出 0，`amount` 是附加输出——*（自 2.0.0 起；1.x 按调用方的变量命名附加输出）*。

<a id="see-also"></a>

## 另请参阅

- [名字解析](name-resolution.md) —— 完整的派发顺序与遮蔽规则
- [语句](statements.md) —— 语句形式，包括独立调用语句
- [表达式](expressions.md) —— 调用如何融入表达式文法
- [`Function`](../language/function.md) —— 声明 `Function`、`Inline` / `SelfContained`、返回类型
- [`GraphFunction`](../language/graph-function.md) —— `UE.*` 提升规则及其限制
- [`ShaderFunction`](../language/shader-function.md) —— 可复用的材质函数块
- [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) —— 层函数块与元数
- [`VirtualFunction`](../language/virtual-function.md) —— 声明现有的 `UMaterialFunction`
- [`Namespace`](../language/namespace.md) —— `::` 限定名与禁止嵌套规则
- [输入 / 输出 / 结果](../language/inputs-outputs.md) —— `in` / `out` / `opt` 与声明的默认值
- [图中的参数](../../parameters/graph-usage.md) —— 读取参数与引脚调用形式
- [UE.Expression](../builtins/ue-expression.md) —— 通用节点内置及其输出选择器
- [节点复用](node-reuse.md) —— 哪些调用节点会被去重
- [`dsc migrate`](../tools/migrate.md) —— 本页每条 legacy 规则在 `.dss` 中的写法
- [诊断索引](../diagnostics/index.md) —— 按阶段列出的所有代码
