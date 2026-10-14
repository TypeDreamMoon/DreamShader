---
title: 调用
description: 在 Graph 块里调用 Function、GraphFunction、ShaderFunction、VirtualFunction 和参数引脚——分发顺序、值形式、语句形式、参数与输出选择。
---

`Graph` 块体里凡是写成 `Name(...)` 的都是调用。可调用的有几种声明种类和两类参数，此外还有会被优先解析的
内置和构造器。（自 2.0.0 起） 起 `.dsm` / `.dsf` 由 legacy 前端读入、由 2.0 绑定器绑定：本页的调用写法
构建的节点和以前一样；1.x 调用与 `.dss` 调用读法不同的地方，由编号的 legacy 规则处理，并用自己的码说明。
每条规则在 `.dss` 里的写法见 [`dsc migrate`](../tools/migrate.md)。

<a id="语法概要"></a>

## 语法概要

```c
// 值形式 —— 调用产生一个值
<target> = <callee> ( [ <argument> [ , <argument> ] … ] ) ;

// 语句形式 —— 调用写入 out 目标
<callee> ( [ <input-argument> , ] … <out-target> [ , <out-target> ] … ) ;

<callee>     := <identifier> | <namespace> :: <identifier>
<argument>   := <expression> | <identifier> = <expression> | default
<out-target> := <identifier>
```

在任何调用里，命名参数之后再出现位置参数都是 `DSH2158` （自 2.0.0 起）。

<a id="可调用的种类"></a>

## 可调用的种类

| 种类 | 值形式 | 语句形式 | 命名参数 | 产生的节点 |
| :-- | :-- | :-- | :-- | :-- |
| `Function` | 支持——返回值，或第一个 `out` （自 2.0.0 起） | 支持 | 支持 （自 2.0.0 起） | `Custom` |
| `GraphFunction` | 支持——同 `Function` （自 2.0.0 起） | 支持 | 支持 （自 2.0.0 起） | `Custom` |
| `ShaderFunction`（同一文件） | 支持，任意输出数 | 支持 | 支持 | `MaterialFunctionCall` |
| `ShaderLayer`、`ShaderLayerBlend` | **不可调用**：`DSH6208` （自 2.0.0 起） | —— | —— | —— |
| `VirtualFunction` | 支持，任意输出数 | 支持 | 支持 | `MaterialFunctionCall` |
| `StaticSwitchParameter` 属性 | 支持 | 不支持 | 支持 | `StaticSwitchParameter` |
| 带输入引脚的参数 | 支持 | 不支持 | **只能**用命名参数 | 该参数自身的节点，并完成配置 |

别的文件里的 `ShaderFunction` 要经由指向其资产的 `VirtualFunction` 调用：`import` 只引入 `.dsh` 头文件，而
头文件里不放资产块。

`UE.*`、`Substrate.*`、数学内置、构造器和 `SampleTexture2D` 也使用调用语法，但它们会在上述任何种类*之前*
被解析。见 [UE.* 节点](../builtins/ue.md)和[数学内置](../builtins/math.md)。

<a id="分发顺序"></a>

## 分发顺序

调用名严格按此顺序探测，第一个认领该名字的表面胜出，后面的表面不会再被查询。

| 顺序 | 阶段 | 表面 | 大小写 | 说明 |
| --: | :-- | :-- | :-- | :-- |
| 1 | 解析 | `Path(…)` | 不敏感 | 资产引用，不是调用 |
| 2 | 解析 | 类型名 | 不敏感 | 构造器：见[构造器](expressions.md#构造器) |
| 3 | 解析 | `N::F` | —— | 读作标识符 `N_F`，然后从第 9 步起查找 |
| 4 | 解析 | 类型为参数节点的属性 | 不敏感 | `StaticSwitchParameter`：[静态开关调用](#staticswitchparameter)；其余：[引脚调用](#输入引脚接线) |
| 5 | 解析 | `SampleTexture2D` | **敏感** | `UE.Expression(Class="TextureSample", …)`；不是两个位置参数时报 `DSH5256` |
| 6 | 解析 | 带输出选择器的 `BreakOutFloat2Components` … `BreakOutFloat4Components` | 不敏感 | 第一个参数的 swizzle；见[下文](#breakoutfloatncomponents) |
| 7 | 解析 | 1.x `UE.*` 简写——`UE.TexCoord`、`UE.Time`、`UE.SceneTexture`、`UE.StaticSwitchParameter` …… | 不敏感 | 参数按 1.x 的方式读取，简写从不读的参数被丢弃并警告 `DSH5254` |
| 8 | 绑定 | `UE.<Node>` / `Substrate.<Node>` | 节点名先精确，再不区分大小写并警告 `DSH5276` | [UE.* 节点](../builtins/ue.md)与 [Substrate](../builtins/substrate.md)（UE 5.4+） |
| 9 | 绑定 | 数学内置 | 敏感 | [数学内置](../builtins/math.md)里的 HLSL 拼法 |
| 10 | 绑定 | `mix`、`fract`、`mod`、`inversesqrt` | 敏感 | 内置的 GLSL 拼法：就是那个内置，并警告 `DSH5277` |
| 11 | 绑定 | 本文件或导入头文件里的 `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction` | 敏感 | 见下文 |
| 12 | 绑定 | 忽略大小写时恰好一个内置、否则恰好一个函数匹配 | 不敏感 | 采用它，并警告 `DSH5275`（legacy 规则 L19） |
| 13 | —— | 无匹配 | —— | `DSH4208` |

`UE` 前缀本身精确比较 （自 2.0.0 起）：`ue.TexCoord()` 把 `ue` 读成未声明的名字，`DSH4200`。

内置直接认领它的名字。写错的 `clamp(x)` 是该内置的元数错误 `DSH4224`；不存在同名的用户函数可以落过去，因为
声明一个与内置同名的函数本身就是 `DSH6206` （自 2.0.0 起）。

**警告：**

遮蔽永远单向——靠前的表面遮蔽靠后的。一个叫 `float3`、`vec2` 或任何其他类型拼法的 `Function` 能正常声明，
却**永远不会被调用**：解析调用时名字就被读成了类型。一个叫 `SampleTexture2D` 的 `Function` 同样永远不会被
调用；`SampleTexture2D` 的改写是精确比较，所以叫 `sampletexture2d` 的 `Function` *可以*用这个拼法调用到。

<a id="被调方的写法"></a>

## 被调方的写法

| 写法 | 解析到 |
| :-- | :-- |
| `Name` | 以该名字声明的函数，**区分大小写**比较 （自 2.0.0 起）。只在忽略大小写时恰好匹配一个函数的名字会解析到它，并警告 `DSH5275` |
| `Namespace::Name` | 声明在 `Namespace(Name="Namespace")` 内部的 `Function` / `GraphFunction`。解析器把 `N::F` 读成一个标识符 `N_F`——函数就是以这个名字声明的 （自 2.0.0 起），所以 `N_F(…)` 也能调用到；裸的 `F` 不行。没有 `using`，也没有非限定回退 |
| `DreamShaderFn_Name` | 不再是函数的别名 （自 2.0.0 起）：`DSH4208` |
| 路径末段 | 对声明为 `Name="Functions/F_Tint"` 的 `ShaderFunction`，取最后一个 `/` 之后的一段——`F_Tint`——其中不是字母、数字或 `_` 的字符换成 `_`。`VirtualFunction` 的 `Name` 本身就必须是标识符（`DSH6311`） |

没有重载。一个名字只声明一样东西：再声明一个同名的函数、属性或结构体是 `DSH4210`。例外是叶子名被它之前声明
的函数（通常来自导入的头文件）占用的 `ShaderFunction` 块：块的函数改以 `<Name>_Asset` 声明，资产保留原名
（提示 `DSH5290`）。只差大小写的两个名字是两个声明 （自 2.0.0 起）；拼法与两者都不同的调用就没有唯一的
不区分大小写匹配，报 `DSH4208`。

<a id="值形式"></a>

## 值形式

```c
float L = Luma(BaseColor);
vec3  C = Common::ApplyTint(BaseColor, Tint);
vec3  N = F_Normal(uv, Output="Normal");
```

| 种类 | 要求 |
| :-- | :-- |
| `Function`、`GraphFunction` | 值是返回值；带 `out` 参数的函数则是它的第一个 `out`（legacy 规则 L3b）（自 2.0.0 起）（1.x 拒绝对多输出函数做值调用）。其他输出用[输出选择器](#输出选择)读取。参数按声明顺序和名字填充参数：没有得到参数的输入是 `DSH4217`，超出最后一个参数的参数是 `DSH4224`。顺序也计入 `out` 参数：落在 `out` 上的参数接收那个输出，和语句调用一样 |
| `ShaderFunction`、`VirtualFunction` | 值是输出 0——第一个声明的输出——除非[输出选择器](#输出选择)选了别的 （自 2.0.0 起）（1.x 在声明了多个输出时要求写选择器）。声明为 `opt` 的输入可以省略或传 `default` |

`GraphFunction` 调用和 `Function` 调用一样是一个 Custom 节点；从函数体里提出来的 `UE.` 调用成为该节点的
更多输入，用这次调用的参数降级。

被调方相同、参数相同的调用是一个节点；见[节点复用](expressions.md#节点复用)。调用结果是普通的后缀
表达式，所以可以直接 swizzle：`SampleTexture2D(Albedo, uv).rgb`。

<a id="语句形式"></a>

## 语句形式

```c
F_PulseTint(BaseColor, Tint, TintedColor, PulseAmount);
```

多输出 `ShaderFunction` / `VirtualFunction` 的语句调用自 （自 1.3.5 起） 起可用。

调用语句可以调用任何东西。没人接收其值的调用——内置、构造器、参数——会被接受，它产生的节点因为没人读而被
丢掉 （自 2.0.0 起）（1.x 拒绝这样的语句）。既不是调用也不是赋值的表达式语句是 `DSH2211`（`F(a).Out;`、
`x;`）。

函数语句调用的参数这样读：

| 参数 | 读作 |
| :-- | :-- |
| 全是位置参数；每个输出至少一个、每个输入和输出至多一个 | 最后几个依次接收输出，每个输出一个，按声明顺序——函数有返回值时返回值排第一；它们前面的是输入，按顺序（legacy 规则 L5） |
| 有任何命名参数，或个数不在上述范围内 | 像 `.dss` 调用一样按声明顺序和名字匹配参数：多了是 `DSH4224`，没有得到参数的输入或输出是 `DSH4217` |

第一种读法里，没有覆盖到的输入必须声明为 `opt` 或传 `default`，否则 `DSH4217`。每个声明的输出都必须有目标
——没有办法丢弃其中一个。

<a id="out-目标规则"></a>

### out 目标规则

| 规则 | 失败时 |
| :-- | :-- |
| out 目标必须可以被赋值：变量，或变量的成员、swizzle （自 2.0.0 起）（1.x 只接受普通名字） | `DSH4239` |
| 任何地方都没声明过的名字在这里声明，成为该输出类型的局部变量（legacy 规则 L5） | 提示 `DSH5283` |
| 已声明的变量类型就是它的类型。比变量更宽的数值输出取前导分量，标量输出填满它；其他都拒绝 | `DSH4218` |
| 同一个变量用作两个输出的目标 | 不报告 |

**提示：**

拼法与某个声明只差大小写的 out 目标**就是**那个声明，并警告 `DSH5275`——而不是第二个变量
（自 2.0.0 起）。只有任何大小写下都没有声明的名字才会成为新的局部变量。

<a id="参数"></a>

## 参数

<a id="位置参数与命名参数"></a>

### 位置参数与命名参数

| 被调方 | 位置参数 | 命名参数 |
| :-- | :-- | :-- |
| `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction` | 按声明顺序填充参数 | 按名字匹配参数（`Function` / `GraphFunction` 自 （自 2.0.0 起） 起）；另一种大小写带 `DSH5275` 解析；没有这个参数是 `DSH4216` |
| 带输入引脚的参数 | 拒绝：`DSH5259` | 必须：节点的引脚 |
| `StaticSwitchParameter` | 下标 0 是 true 分支，下标 1 是 false 分支 | `True` / `A`、`False` / `B` |

位置参数在前、命名参数在后；反过来是 `DSH2158`。同一个参数给了两次——位置一次、名字一次，或名字两次——是
`DSH4215`。在语句调用里，出现命名参数就关闭"末尾接收输出"的读法（见[语句形式](#语句形式)）。

参数名精确比较，带上面的 `DSH5275` 回退；节点引脚在另一种大小写下匹配时警告 `DSH5276`。1.x 前端自己读取的
名字——`Output`、`OutputName`、`OutputIndex`、`True`、`False`、`A`、`B`——不区分大小写。

每个输入参数都会被转换到该输入的声明类型：更宽的值取前导分量并给出提示 `DSH5289`，放不进去的是 `DSH4226`。
见[转换](expressions.md#转换)。

<a id="default"></a>

### `default`

（自 1.2.3 起） `default` 是一个裸标识符，占住一个有意留空的输入（legacy 规则 L25）。它**区分大小写**
（自 2.0.0 起），并且只在没有声明同名变量时生效。

| 情况 | 行为 |
| :-- | :-- |
| 对 `ShaderFunction` / `VirtualFunction` 的输入传 `default` | 输入引脚保持未连接；函数自身的默认值生效 |
| 对不是 `opt` 的输入传 `default` | 同上 （自 2.0.0 起）（1.x 拒绝） |
| 在 `Function` / `GraphFunction` 调用里用 `default` | 接受 （自 2.0.0 起）；Custom 节点不为该参数建输入 |
| `Default`、`DEFAULT` | 普通名字：没有同名声明时是 `DSH4200` |

完全省略末尾的可选输入，效果与传 `default` 相同。

```c
float3 tinted = F_Tint(BaseColor, default, Output="OutColor");
```

<a id="输出选择"></a>

## 输出选择

`Output`、`OutputName` 和 `OutputIndex` 是 1.x 前端自己读取的命名参数：它把它们从调用里移除，改为选择输出，
如同在调用后面写 `.Name`。它们可用在任何函数种类的调用上（`Function` 和 `GraphFunction` 自
（自 2.0.0 起） 起），也可用在 `UE.*` 调用上。

| 参数 | 接受 | 含义 |
| :-- | :-- | :-- |
| `Output` | 带引号的名字或标识符 | 选择该名字的输出——先精确，再不区分大小写并警告 `DSH5275` |
| `OutputName` | 同上 | `Output` 的完全同义词 |
| `OutputIndex` | 0 或更大的整数字面量 | 按从 0 起的下标选择：按声明顺序的输出，返回值是输出 0 |

```c
float value = F_MultiOutput(Input=Mask, Output="Height");
float other = F_MultiOutput(Input=Mask, OutputIndex=1);
```

| 规则 | 失败时 |
| :-- | :-- |
| `Output` / `OutputName` 与 `OutputIndex` 互斥 | `DSH5252` |
| `OutputIndex` 必须是 0 或更大的整数 | `DSH5250` |
| `Output` / `OutputName` 必须是带引号的名字或标识符，不能是表达式 | `DSH5251` |
| 构造器不接受选择器 | `DSH5253` |
| 参数调用不接受选择器 | `DSH5265` |
| 名字必须匹配函数的某个输出 | `DSH5280`。对有返回值的函数，不是输出名的名字会改按值的 swizzle 读（不是 swizzle 时 `DSH4230`） |
| 下标必须在输出范围内 | `DSH5282` |
| 多输出函数不带选择器调用 | 不是错误 （自 2.0.0 起）：读输出 0 |
| 选中的输出必须在已加载资产上存在——先按名字、再不区分大小写、（`VirtualFunction`）再按声明位置匹配 | 发射资产时 `DSH8221` |

**警告：**

选择器把调用变成一个值。它不能用在语句调用里：被选中的调用是一个没人接收的表达式，即 `DSH2211`。

`UE.Expression(…)` 接受同样的三个选择器；节点没有的名字是 `DSH5201`，超出输出的下标是 `DSH5282`。见
[UE.Expression](../builtins/ue-expression.md)。

<a id="breakoutfloatncomponents"></a>

### `BreakOutFloatNComponents`

名为 `BreakOutFloat2Components`、`BreakOutFloat3Components` 或 `BreakOutFloat4Components`（不区分大小写）、
第一个参数是位置参数、并带有输出选择器的调用，会被**读成第一个参数的 swizzle**。不产生
`MaterialFunctionCall` 节点，也不再查找同名函数 （自 2.0.0 起）。所选输出指名一个通道，字母不区分大小写：

| 输出名 | 通道 |
| :-- | --: |
| `x`、`r`、`"0"` | 0 |
| `y`、`g`、`"1"` | 1 |
| `z`、`b`、`"2"` | 2 |
| `w`、`a`、`"3"` | 3 |

`OutputIndex=` 按下标选中同样的通道。通道必须在名字所说的宽度之内（`BreakOutFloat2Components` 为 0–1）。否则
——没有选择器、第一个参数是命名参数、通道越界——这就是一次对同名函数的普通调用。两个选择器同时给是
`DSH5252`。

<a id="调用参数"></a>

## 调用参数

<a id="输入引脚接线"></a>

### 输入引脚接线

（自 1.4.1 起） 节点自带输入引脚的已声明参数可以被"调用"，以接好这些引脚。前端把这类属性的**每一次**
使用——调用或裸读取——都写成一次独立的 `UE.Expression(Class = "<type>", ParameterName = "<name>", …)` 调用，
声明的默认值和元数据作为参数，调用的参数接到引脚上 （自 2.0.0 起）。参数相同的使用经过去重后是一个节点；
裸读取和引脚调用的参数不同，所以是指向同一个材质参数的两个节点。纹理采样参数读的是它的 `RGBA` 输出。

接受这种形式的参数节点类型完整清单：

| | | |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `StaticComponentMaskParameter` | `TextureSampleParameter2D` |
| `TextureSampleParameter2DArray` | `TextureSampleParameterCube` | `TextureSampleParameterCubeArray` |
| `TextureSampleParameterVolume` | `TextureSampleParameterSubUV` | `RuntimeVirtualTextureSampleParameter` |
| `SparseVolumeTextureSampleParameter` | | |

引脚名是节点自己的、引擎目录里列出的名字；另一种大小写带 `DSH5276` 解析。每个参数都必须是命名的（`DSH5259`），
每个值都像任何 `UE.` 参数一样按引脚检查（`DSH5214`）。

**警告：**

资产槽位不是调用参数。采样器参数指向的贴图、曲线或字体，是用 `[TextureSlot=Path(…)]` 这类声明元数据设置的，
它会成为节点的一个属性参数；调用参数再指名声明已经设置过的属性是 `DSH4215`。既不是引脚也不是属性的名字是
`DSH5213`；它的值是图值时改为提示 `DSH5291`，节点建出来后按这个名字接引脚（节点上没有这个引脚时 `DSH8212`）。
见 [Properties 类型](../language/properties.md)和[元数据与分组](../parameters/metadata.md)。

```c
Properties = {
    TextureSampleParameter2D Albedo = Path(Game, "Textures/T_Albedo");
}

Graph = {
    vec2 UV  = UE.TexCoord(Index = 0);
    vec4 Tex = Albedo(Coordinates = UV);
}
```

<a id="staticswitchparameter"></a>

### `StaticSwitchParameter`

（自 1.2.3 起） `StaticSwitchParameter` 属性**不**作为裸标识符解析（`DSH4200`）——它需要两个分支，所以只能
通过调用形式读取。

| 分支 | 参数名，按查找顺序 |
| :-- | :-- |
| true | 先 `True=`，再 `A=`，再位置下标 0 |
| false | 先 `False=`，再 `B=`，再位置下标 1 |

两个分支都必须存在（`DSH5258`）；其他参数被丢弃并警告 `DSH5254`。该调用成为一个
`UE.Expression(Class = "StaticSwitchParameter", …)`，两个分支接在它的 `A` 和 `B` 引脚上，像任何 `UE.` 节点的
引脚一样检查 （自 2.0.0 起）。

```c
Properties = {
    StaticSwitchParameter UseDetail = true [
        Group="Switches";
        SortPriority=30;
    ];
}

Graph = {
    vec3 Albedo = UseDetail(True = DetailColor, False = BaseColor);
}
```

因为这个开关是在着色器变体阶段而不是每像素解析的，所以当"选哪个"是材质设置而不是计算结果时，它是 `if`
的廉价替代品。

<a id="递归与嵌套"></a>

## 递归与嵌套

| 情况 | 行为 |
| :-- | :-- |
| `GraphFunction` 经由提出来的 `UE.` 调用直接或间接再调用自己 | `DSH6330`：每次调用都会产生一个新的 Custom 节点 |
| `Function` 函数体互相调用成环 | `DSH6260`：它们的函数体要嵌进同一个 Custom 节点 |
| 同一文件的 `ShaderFunction` 块互相调用成环 | `DSH8299` |
| 一条表达式里嵌套调用 | 合法——参数就是普通表达式，所以 `F(G(x), 2.0)` 对任何可调用种类都成立 |
| 把调用用作 `Outputs` 绑定表达式 | 合法——绑定就是完整表达式 |

<a id="哪些调用会被复用"></a>

## 哪些调用会被复用

（自 2.0.0 起） 起复用是对整张图跑的一道去重 pass：类相同、参数相同的两个节点就是一个节点，适用于**所有**
调用种类——`Function` / `GraphFunction` 的 Custom 节点、材质函数调用、`UE.TexCoord` 这类 1.x 语法糖内置、
`UE.CollectionParam`、`StaticSwitchParameter` 调用、Custom 类的 `UE.Expression`。1.x 对其中大多数每个调用点
都新建节点。

| 写法 | 结果 |
| :-- | :-- |
| `F_Tint(a, b)` 写两次 | 一个节点 |
| `F_Tint(a, b)` 与 `F_Tint(b, a)` | 两个节点——操作数不同 |
| `F_Tint(a, b)` 与 `F_Tint(Color = a, Tint = b)` | 一个节点——构建出的节点相同 |
| `F_Tint(Color = a)` 与 `F_Tint(color = a)` | 一个节点——另一种大小写就是同一个声明（`DSH5275`） |
| `UE.TexCoord(Index = 0)` 与 `UE.TexCoord(Index = 1)` | 两个节点——属性不同 |
| 参数的引脚调用与它的裸读取 | 两个节点——参数不同 |

`Outputs` 表达式目标（`Expression( … ).Pin[i]` 和块形式）不是由这道 pass 分组的，而是在读取 `Outputs` section 时
按类和参数表分组：参数表多一个或少一个参数，就静默地成为第二个节点。见[输出绑定](../generation/outputs.md)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH4208: '{0}' is not a function, a builtin or a struct.` | 调用名在所有表面上都没匹配到——常见于拼错的构造器（vec1、float5）或 DreamShaderFn\_ 前缀；被调方不是名字时也报这个码。 | 修正拼写（名字区分大小写），或在调用之前声明函数。 [详解](expressions.md#构造器) |
| `DSH5275: '{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告（legacy 规则 L19）：名字只在忽略大小写时匹配到某个函数、内置或参数。 | 按声明的拼写写。 |
| `DSH5277: '{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'.` | 警告（legacy 规则 L2）：mix、fract、mod 或 inversesqrt。 | 写 HLSL 名字。 [详解](../builtins/math.md) |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | 两个声明共用一个名字——包括头文件与本文件、N::F 与名为 N\_F 的函数。 | 给其中之一改名；没有重载。 |
| `DSH5290: The asset of this block and a function this file can call are both named '{0}', which 1.x kept apart; the block is declared as '{1}', and a '.dss' writes that name.` | 提示（legacy 规则 L23）：ShaderFunction 块的名字被占用，改以 Name\_Asset 声明。 |  |
| `DSH6206: '{0}' is a builtin operation and cannot be redeclared; rename the function.` | 函数与数学内置同名。 | 给函数改名。 |
| `DSH6208: '{0}' is a {1} asset, not a function this file may call.` | 调用 Shader、ShaderLayer 或 ShaderLayerBlend。 | 把共用代码提成辅助函数，在两处调用它。 |
| `DSH2158: A positional argument cannot follow a named argument; give this argument a name too.` | 命名参数之后出现了位置参数。 | 给后面的参数也写上名字，或把命名参数挪到末尾。 |
| `DSH2211: Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used.` | 语句是一个没人接收其值的表达式——包括带输出选择器的调用语句。 |  |
| `DSH4224: '{0}' takes at most {1} arguments.` | 参数比函数的参数多；或内置的元数不对，如 clamp(x)。 |  |
| `DSH4216: '{0}' takes its arguments in order and has no argument called '{1}'.` | 命名参数指名的参数不存在。 | 修正名字，或改用位置参数。 |
| `DSH4215: The '{0}' argument of '{1}' is given twice.` | 同一个参数（或引脚、属性）给了两次——包括声明元数据已设置、调用又给的属性。 | 删掉重复的那个。 |
| `DSH4217: '{0}' is missing its '{1}' argument.` | 不是 opt 的输入没有得到参数；或按名字读的调用里某个输出没有目标。 | 传一个值，或把该输入声明为 opt。 |
| `DSH4226: {0} expects {1}, and this is {2}.` | 输入参数无法转换到参数的类型。 |  [详解](expressions.md#转换) |
| `DSH4239: '{0}' is an out parameter of '{1}', so its argument has to be a variable.` | out 目标不能被赋值。 | 声明一个该输出类型的变量并传入。 |
| `DSH4218: '{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly.` | out 目标变量的类型接不住这个输出。 | 按输出类型声明变量，调用后再转换。 |
| `DSH5283: '{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'.` | 提示（legacy 规则 L5）：没声明过的 out 目标成为局部变量。 |  |
| `DSH5252: Expected either 'Output' or 'OutputIndex' on this call, found both.` | 同时给了 Output / OutputName 和 OutputIndex。 | 只留一个。 |
| `DSH5250: Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'.` | OutputIndex 不是 0 或更大的整数。 |  |
| `DSH5251: Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'.` | Output 的值既不是带引号的名字也不是标识符。 | 写成 Output = "RGB"。 |
| `DSH5265: Expected the parameter call '{0}' to take only its inputs, found an output selector.` | 参数调用带了输出选择器。 | 在调用之后用成员选择：MyTex(Coordinates = uv).rgb。 |
| `DSH5280: '{0}' has no output to select, and this call selects '{1}'.` | 函数没有这个名字的输出。 |  |
| `DSH5282: This Custom node declares {0} output(s), counted from 0, and this selects output {1}.` | 下标超出了输出范围。 |  |
| `DSH8221: '{0}' has no output named '{1}'.` | 声明与已加载资产在输出上不一致。 | 重新生成资产，或修正声明。 |
| `DSH8219: This function call names no material function asset.` | 函数资产无法加载，或无法赋给调用节点（函数调用了自己）。 |  [详解](../generation/asset-paths.md) |
| `DSH8270: The function reference '{0}' does not resolve to an asset path. {1}` | VirtualFunction 的 Asset 引用解析不到资产路径。 |  [详解](../parameters/path.md) |
| `DSH6330: '{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end.` | GraphFunction 经由提出来的 UE. 调用直接或间接递归。 | 在图里算出内层的值再传进去。 |
| `DSH6260: The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.` | Function 函数体互相调用成环。 | 把共用部分提成第三个不调用别人的函数。 |
| `DSH6314: Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body.` | GraphFunction 函数体里的 UE. 调用没有闭合的 )。 |  |
| `DSH5259: Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument.` | 参数引脚调用里出现了位置参数。 | 给每个参数写名字：MyTex(Coordinates = uv)。 |
| `DSH5214: {0} expects {1}, and this is {2}.` | 值放不进它要接的引脚。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 名字既不是引脚也不是属性，且值不是图值——常见于把资产槽位当调用参数。 |  [详解](../parameters/metadata.md) |
| `DSH5258: Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.` | StaticSwitchParameter 调用缺少一个或两个分支。 | 传 True = ... 和 False = ...。 |
| `DSH5254: '{0}' takes no positional argument here; 1.x ignored it, so it is dropped.` | 警告：1.x 调用写法不读的参数被丢弃。 | 按该写法读取的方式给参数命名，或删掉它。 |

跨阶段的完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
import "Helpers.dsh";

Shader(Name="Docs/M_Calls")
{
    Properties {
        vec3                     Tint    = vec3(1.0, 0.4, 0.1);
        TextureSampleParameter2D Albedo  = Path(Game, "Textures/T_Albedo");
        StaticSwitchParameter    UseTint = true;
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

        // 参数引脚调用：接好采样器的 Coordinates 引脚，读 RGBA。
        vec4 Tex = Albedo(Coordinates = UV);

        // 值形式，Helpers.dsh 里声明的带返回值的 Function。
        float L  = Luma(Tex.rgb);

        // 值形式，带命名空间的 Function：值是它的第一个 out 参数。
        vec3 Lit = Common::ApplyTint(Tex.rgb, Tint);

        // 语句形式：先输入，再每个输出一个 out 目标。两个目标都没声明，
        // 各自成为该输出类型的局部变量（提示 DSH5283）。
        PulseTint(Lit, Tint, Pulsed, Amount);

        // StaticSwitchParameter 调用在两者之间选择。
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

```text
TextureCoordinate                      -> UV
TextureSampleParameter2D  Albedo       -> Tex          (Coordinates pin wired to UV)
Custom  "Luma"                         -> L
Custom  "Common::ApplyTint"            -> Lit
Custom  "PulseTint"                    -> Pulsed (output result), Amount (output amount)
AppendVector, AppendVector             -> vec3(L, L, L)
StaticSwitchParameter  UseTint         -> Color
```

`PulseTint` 这个 Custom 节点的输出按函数的 `out` 参数命名——`result` 是输出 0，`amount` 是附加输出
（自 2.0.0 起）（1.x 按**调用方**的 out 变量给附加输出命名）。

<a id="参见"></a>

## 参见

- [函数](../language/functions.md) —— 声明 `Function`、`GraphFunction` 与 `Namespace`
- [import 与命名空间](../language/import.md) —— `::` 名字从哪里来
- [Properties 类型](../language/properties.md) —— 哪些参数自带输入引脚
- [语句与声明](statements.md) —— 作为语句的调用形式
- [运算符与转换](expressions.md) —— 调用在表达式文法里的位置、参数转换与节点复用
- [UE.Expression](../builtins/ue-expression.md) —— 通用节点内置及其输出选择器
- [迁移到 .dss](../tools/migrate.md) —— 本页每条 legacy 规则在 `.dss` 里的写法
