---
title: "UE.Expression"
description: "UE.Expression"
---

<a id="ueexpression"></a>

# UE.Expression

> [DreamShader](../index.md) » [内置节点](index.md) » **UE.Expression**

反射节点调用：按名字创建内置目录中的任意 `UMaterialExpression` 类，并用参数
填充它的输入引脚和属性。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 在 `Graph { … }` 体内，或 `Properties { … }` 区段中（见[声明形式](#declaration-form)） |
| 类别 | 内置 |
| 生成 | 解析出的 `UMaterialExpression` 类的一个实例 |

<a id="synopsis"></a>

## 概要

```c
UE.Expression( Class = <class-specifier>
               [, { OutputType | ResultType } = <type-token> ]
               [, { { Output | OutputName } = <text> | OutputIndex = <int> } ]
               [, <arg-name> = <expression> ] … )

UE.<ClassName>( [ <arg-name> = <expression> ] … )
```

两种形式在绑定器中走的是同一条路径。调用用 `Class` 指明它的类，或用 `UE.` 后面的
名字，因此 `UE.Sine(…)` 和 `UE.Expression(Class = "Sine", …)` 完全相同；在另一个名字上写 `Class`
参数会用该参数覆盖名字。`Expression` 不指定任何类，所以第一种形式
需要 `Class` —— 没有它，该调用就是 [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218)。

`OutputType` 不再必需 *（自 2.0.0 起）*：目录知道每个类产出什么。见
[OutputType](#outputtype)。

参数是命名的。位置参数只在目录为其给定参数顺序的类上绑定 —— `TextureCoordinate`、`Constant`、`Constant2Vector`、`Constant3Vector`、`Constant4Vector`、
`Transform`、`TransformPosition`、`Panner`、`ComponentMask`、`Time`，以及五个 Substrate
组合节点 —— 在其他地方是 [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220)，
超出顺序末尾则是 [`DSH5221`](../diagnostics/DSH5xxx.md#dsh5221) *（自 2.0.0 起）*。在 [`UE.*` 目录](ue.md#catalogue)的 27 个
1.x 名字上，legacy 前端会先丢弃位置参数
（[`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)），两个变换的 `Input` 除外。

文本参数可以带引号也可以不带引号：`Class = Sine` 和 `Class = "Sine"` 相同，
`OutputType = float3` 和 `OutputType = "float3"` 也相同。

<a id="order-of-resolution"></a>

## 解析顺序

调用会依次经过编译器的各个阶段。每个阶段报告其所有错误，而有错误的阶段会在进入下一阶段之前
停止流水线 *（自 2.0.0 起；1.x 在第一个失败步骤处停止）*。

| # | 阶段 | 对调用做什么 | 错误码 |
| :-- | :-- | :-- | :-- |
| 1 | legacy 前端 | `Output` / `OutputIndex` 变成调用上的选择；`OutputType` / `ResultType` 从调用上取下（并在 Custom 调用上保留）；改写 1.x 简写 | `DSH5250`–`DSH5259`、`DSH5261`、`DSH5263`–`DSH5265` |
| 2 | 绑定器 —— 类 | 在目录中查找 `Class` 或名字 | `DSH5216`、`DSH5217`、`DSH5218`、`DSH5212`、`DSH5210`、`DSH5294`、`DSH5300`、`DSH5276` |
| 3 | 绑定器 —— 参数 | 每个参数绑定到引脚、属性、Custom 输入、Substrate 参数，或保留给构建出的节点 | `DSH5220`、`DSH5221`、`DSH5285`、`DSH4215`、`DSH5214`、`DSH5224`、`DSH5215`、`DSH5278`、`DSH5284`、`DSH5288`、`DSH5291`、`DSH5213` |
| 4 | 绑定器 —— 调用 | 必需的引脚；值的类型 | `DSH5279`（在 `.dss` 中为 `DSH5219`）、`DSH4231` |
| 5 | IR 构建器 | 属性值变成字面量 | `DSH4373` |
| 6 | 发射器 | 创建节点、写入其属性、连接其引脚 | `DSH8211`、`DSH8214`、`DSH8213`、`DSH8212`、`DSH8254` |

<a id="class-resolution"></a>

## 类解析

`Class` 接受带引号的字符串、裸词或带点的名字
（否则 [`DSH5217`](../diagnostics/DSH5xxx.md#dsh5217)）。说明符会去除首尾空白，并在
目录中按以下顺序查找：

| # | 匹配 |
| :-- | :-- |
| 1 | 目录的短名 —— `Sine` |
| 2 | 类名 —— `MaterialExpressionSine` |
| 3 | 类路径 —— `/Script/Engine.MaterialExpressionSine` |
| 4 | `MaterialExpression` + 说明符，以及带 `U` 的 C++ 名 —— `UMaterialExpressionSine` |
| 5 | 目录携带的别名 —— `TexCoord`、`Lerp`、`Mask`、`FunctionCall`…… |
| 6 | 仅在 1.x 源码中：第 1–3 行忽略大小写的唯一匹配，并给出 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) |

都没匹配上则是 [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212)。第 1–5 行区分大小写。

> [!IMPORTANT]
> `UMaterialExpressionSine` 可以解析 *（自 2.0.0 起；1.x 与反射名比较，带 `U` 前缀的拼写
> 永远匹配不上）*。`USine` 不能。

> [!NOTE]
> 目录列出构建它时已加载的类，抽象类和已弃用的类不在其中。位于编辑器尚未加载的插件模块中的
> 表达式类不在其中。

在 `Substrate.*` 路径上，名字就是类，而 `Class=` 是
[`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216)。

<a id="outputtype"></a>

## OutputType

`OutputType` —— 或其别名 `ResultType` —— **不是**必需的 *（自 2.0.0 起）*。legacy 前端
会把它从调用上取下，值的类型来自目录。它仍在两个
地方有意义：

- 在 `Custom` 节点上，它是节点的输出类型 —— 见 [Custom 节点](#custom-nodes)；
- 在只有一个数值输出的节点上，数值类型 token 是值的宽度，和
  1.x 一样，并且节点整体连接。

它曾经的其他一切 —— 纹理、`MaterialAttributes` 或 `Substrate` token、拼写错误的 token —— 都会
无诊断地丢弃。token 表在 [`OutputType`](output-type.md) 页面。

<a id="argument-dispatch"></a>

## 参数分派

这些名字不会被分派：

| 名字 | 用途 |
| :-- | :-- |
| `Class` | 类说明符；按完全相同的拼写匹配 |
| `OutputType`、`ResultType` | 声明的输出类型 —— 由前端取下，大小写不限 |
| `Output`、`OutputName` | 按名字选择输出 —— 大小写不限 |
| `OutputIndex` | 按索引选择输出 —— 大小写不限 |

其他每个参数都会按以下顺序对类的目录条目解析。第一个匹配的
胜出。

| # | 判断 | 结果 |
| :-- | :-- | :-- |
| a | 名字是类的输入引脚，或其别名之一 | 值连接到该引脚 |
| b | 名字是类的属性，或其别名之一 | 值写入节点 |
| c | 在 1.x 源码中：名字或其别名是忽略大小写时唯一匹配的引脚，其次是属性 | 同 `a` 或 `b`，并给出 `DSH5276` |
| d | 类是 `Custom` | 节点的一个新输入，按书写原样命名 —— 见 [Custom 节点](#custom-nodes) |
| e | 节点是 Substrate BSDF，且名字是一个[虚拟参数](../language-v2/substrate.md#legacy-parameters-on-a-slab) | 引脚前面某个转换节点的输入 |
| f | 在 1.x 源码中：`DefaultValue` 用在没有该参数的类上 | 以 [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288) 丢弃 |
| g | 在 1.x 源码中，或在引脚取决于其属性的类上：值是数字、纹理、材质、Substrate 值或节点 | 以 [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) 保留，并在构建节点时按该名字连接（若节点没有此引脚则为 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)） |
| h | —— | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)，当拼写接近时附带“您是否想输入” |

输入 pin 名优先于同名属性；真正的 pin 名或属性名优先于别名。
同一个 pin 或属性给两次报 [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215)。

别名有：

| 别名 | 示例 |
| :-- | :-- |
| bool 属性去掉开头 `b` 的名字，且 `b` 后面紧跟大写字母时 *(自 2.0.0 起)* | `FractionalPart` → `bFractionalPart` |
| 属性的显示名，去掉空格或把空格换成 `_` | `ShaderOffsets`、`Shader_Offsets` → `WorldPositionShaderOffset` |
| pin 的显示名写成标识符 | `True`、`False` → `StaticSwitch.A`、`StaticSwitch.B` |
| 没有规则可循的 1.x 拼写 | `Index`（`TextureCoordinate.CoordinateIndex`）、`Origin`（`ObjectPositionWS.OriginType`）、`Source` / `Destination`（`Transform`、`TransformPosition`）、`Asset` / `Parameter`（`CollectionParameter`）、`UV`（`TextureSample.Coordinates`） |

目录列出输入 pin，以及可编辑、既未弃用也非 transient 的属性；1.x 能触达任意 `UPROPERTY`，可编辑与否都行 *(自 2.0.0 起)*。除此之外参数名必须精确：不会去掉任何分隔符。

<a id="input-pins"></a>

### 输入 pin

解析到 pin 的参数是表达式，会被连接。它的类型按目录给 pin 的类型检查：

| pin | 值 | 结果 |
| :-- | :-- | :-- |
| `float1`–`float4` | 同宽，或标量（铺满 pin） | 连接 |
| `float1`–`float4` | 更窄的向量 | 原样连接 —— 分量少了节点怎么处理是节点的事 |
| `float1`–`float4` | 更宽的向量 | 1.x 源码里整体连接，给出提示 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)；否则 [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) |
| 引擎没给类型的 pin | 任意数值、material、Substrate 值 | 原样连接 |
| `MaterialAttributes`、Substrate、纹理 | 别的种类的值 | `DSH5214` |

<a id="literal-properties"></a>

### 字面量属性

其余匹配上的名字是属性，建节点时写进去。绑定器检查值；发射器用 1.x 的字面量写入器写入，写不进去的值报
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)，其消息带着写入器自己的码。

| 属性类型 | 绑定器接受 | 发射器读作 | `DSH8213` 中的码 |
| :-- | :-- | :-- | :-- |
| `FBoolProperty` | 常量 | `true` / `false`，不区分大小写 | `DSH7132` |
| `FIntProperty` | 常量 | 十进制整数 | `DSH7133` |
| `FUInt32Property` | 常量 | `0 … 4294967295` 内的整数 | `DSH7134` |
| `FFloatProperty` | 常量 | 数字 | `DSH7135` |
| `FDoubleProperty` | 常量 | 数字 | `DSH7136` |
| `FStrProperty` | 带引号的字符串、词或带点的名字 | 去空白后的文本，原样写入 —— **永远成功** | — |
| `FNameProperty` | 同上 | 去空白后的文本作为 `FName` —— **永远成功** | — |
| `FObjectPropertyBase` | 同上 | 见[对象属性](#object-properties) | `DSH7137`–`DSH7139` |
| `FEnumProperty`、带枚举的 `FByteProperty` | 见[枚举值](#enum-values) | 枚举值 | `DSH7140`、`DSH7141` |
| 不带枚举的 `FByteProperty` | 常量 | `0 … 255` 内的整数 | `DSH7142` |
| 其他 —— 结构体、数组 | 带引号的字符串或词 | Unreal 自己的导入文本，例如 `"(R=1,G=0,B=0,A=1)"` | `DSH7143` |

绑定器不接受的值报 [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224)：数值属性收到运行时计算的值，或文本属性收到数字。

> [!WARNING]
> `FStrProperty` 和 `FNameProperty` 永远不会失败。参数携带的任何文本 —— 包括解析不了的路径 —— 都被原样存下来。
> 既不报诊断，也不产生回退值。

<a id="enum-values"></a>

### 枚举值

绑定器把枚举值与目录给该属性列出的值匹配 —— 隐藏值不算，每个值都写成不带前缀的名字（`PostProcessInput0` 写成 `PPI_PostProcessInput0`）。

| 写法 | `.dss` 中 | 1.x 源码中 |
| :-- | :-- | :-- |
| `PostProcessInput0` | 接受 | 接受 |
| `postprocessinput0` | [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215)，附带“did you mean” | 接受，给出 [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) |
| `PPI_PostProcessInput0`、`ESceneTextureId::PPI_PostProcessInput0` | `DSH5215` | 接受，给出 `DSH5278` |
| 与值名不同的显示名 | `DSH5215` | `DSH5215` *(自 2.0.0 起)* |

1.x 的匹配（规则 L12）忽略大小写、空格、制表符以及字符 `_`、`-`、`:`、`.` 和 `/`，
并依次尝试整个值、``Enum::`` 作用域之后，以及去掉自身前缀之后。节点按目录的拼写写入。[`UE.SceneTexture`](ue.md#uescenetexture) 和变换的坐标系都依赖这一点。

<a id="object-properties"></a>

### 对象属性

对象属性接受 `Path(<root>, "<asset>")`、带引号的对象路径 `"/Game/…"`，或 `Class'/Game/…'`。``Path(…)`` 参数作为文本携带，建节点时解析。参见 [`Path(…)`](../parameters/path.md)。

| 情况 | 结果 |
| :-- | :-- |
| 值既不是带引号的字符串、词，也不是 `Path(…)` | `DSH5224` |
| 文本以 `Path(` 或 `/` 开头、或以 `'` 结尾，且解析不了 | `DSH8213`，带解析器自己的消息 |
| 文本不是资产引用 | `DSH8213`（`DSH7137`） |
| 资产加载失败，且属性是名为 `Texture` 或 `TextureObject` 的 `UTexture` | 属性被设为 **null**，并算作**成功** |
| 资产加载失败，其他属性 | `DSH8213`（`DSH7138`） |
| 资产加载成功但类不对 | `DSH8213`（`DSH7139`） |

> [!WARNING]
> 失败置空这条规则是静默的。`UE.Expression(Class = "TextureSample",
> Texture = Path(Game, "Missing/T_Nope"))` 会带着一个未赋值的纹理编译通过，而不是报告资产缺失。
> 只有 `UTexture` 类型属性上的 `Texture` 和 `TextureObject` 这两个属性名会这样。

<a id="selecting-an-output"></a>

## 选择输出

多输出的节点通过两个互斥的选择器之一读取，前端把它们变成调用上的选择。

| 参数 | 别名 | 类型 | 变成 |
| :-- | :-- | :-- | :-- |
| `Output` | `OutputName` | 带引号的名字或一个词 | 调用上的 `.Name` |
| `OutputIndex` | — | ≥ 0 的整数 | 调用上的 `[k]` |

| 错误 | 码 |
| :-- | :-- |
| 两个选择器都写 | [`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252) |
| `OutputIndex` 不是 ≥ 0 的整数 | [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) |
| `Output` 既不是带引号的名字也不是一个词 | [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) |
| 构造器上的选择器 | [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253) |
| 类没有这个名字的输出 | [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) |
| 索引超过最后一个输出 | [`DSH5282`](../diagnostics/DSH5xxx.md#dsh5282) |

名字与目录列出的输出精确比较；在 1.x 源码里，只在忽略大小写时才匹配的名字也接受，给出 `DSH5276`。名字可以带空格（`Output = "Second Roughness"`）。
多输出节点上引擎没命名、带掩码的输出，按掩码保留的通道列出
（`RGB`、`R`、`A`）；见[输出名](output-type.md#output-mask-pseudo-names)。

两个选择器都不用，调用就是节点本身：见
[结果类型与分量数](#result-type-and-component-count)。

<a id="custom-nodes"></a>

## Custom 节点

`UE.Expression(Class = "Custom", …)` —— `MaterialExpressionCustom` 也行 —— 建出一个 pin 就是调用参数的节点
（规则 L4）：

| 方面 | 行为 |
| :-- | :-- |
| `OutputType` | 节点的输出类型：`float`、`float1`、`half`、`half1` → Float1；`float2`、`vec2`、`half2` → Float2；`float3`、`vec3`、`half3` → Float3；`float4`、`vec4`、`half4` → Float4；`MaterialAttributes` → MaterialAttributes。比较时忽略大小写和空格；其他任何值都是 [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261)。不写时用类自己的输出类型 |
| `AdditionalOutputs` | 节点的其他输出，用引擎的导入文本写；之后每个都按名字选择，输出 0 叫 `return` |
| 既不是 pin 也不是属性的参数 | 一个新输入，按书写原样命名；值不是数字、bool 或纹理时报 [`DSH5284`](../diagnostics/DSH5xxx.md#dsh5284)，同名两次报 `DSH4215` |
| 新建的节点 | 写入调用的输入之前，先清空它的 `Inputs` 和 `AdditionalOutputs` 数组 |
| 输出选择器 | 指向声明过的某个输出：其他名字报 `DSH5201`，超出的索引报 `DSH5282` *（自 2.0.0 起；1.x 会合成 `Output1`、`Output2`、… 占位输出直到所要的那个）* |
| 节点复用 | 两个相同的 Custom 调用是一个节点，和任何两个相同的调用一样 *（自 2.0.0 起）* |

<a id="result-type-and-component-count"></a>

## 结果类型与分量数

调用的值，取决于目录对类的描述：

| # | 类 | 值 |
| :-- | :-- | :-- |
| 1 | 没有输出（自定义输出类） | 没有 —— 调用是一条语句；当值用报 [`DSH4231`](../diagnostics/DSH4xxx.md#dsh4231) |
| 2 | 是 `Custom` | `OutputType` 给出的类型；有 `AdditionalOutputs` 时是输出带名字的节点 |
| 3 | 有多个输出 | 一个节点：指名一个，或在它的第一个输出 —— 或各通道输出拼成的整个值 —— 恰好合适的地方使用。1.x 源码里第一个输出没有固定宽度的读作那个输出（[`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287)），直接传给 pin 的任何节点的第一个输出也如此 *（2.0.0 – 2.1.0：`DSH5201`）*；否则 `DSH5201` |
| 4 | 只有一个已知类型的输出 | 那个类型 |
| 5 | 只有一个引擎没给类型的输出 | 和同样没给类型的 pin 上最宽的数值一样宽 —— 或这样的 pin 携带的 Substrate 或 material 值 —— 否则和它被读入的位置一样宽 |

在 1.x 源码里，第 4、5 行的类上的数值 `OutputType` 会改为决定值的类型，见上。

输出的类型：有掩码时是掩码的宽度，否则是引擎的值类型；引擎只说「一个 float」时，由已知宽度表决定 ——
1.x 自己那张表，在类只有一个输出时被查询，外加目录补充的几行：

| 宽度 | 类 |
| :-- | :-- |
| 2 | `TextureCoordinate`、`Panner`、`Rotator`、`SceneTexelSize`；`SceneTexture` 的 `Size` 和 `InvSize` 输出 |
| 3 | `ObjectPositionWS`、`CameraVectorWS`、`VertexNormalWS`、`VertexTangentWS`、`Transform`、`TransformPosition`、`SkyAtmosphereLightDirection`、`PixelNormalWS`、`CrossProduct`、`ObjectBounds`、`CameraPositionWS`、`ReflectionVectorWS` |
| 1 | `PixelDepth`、`TwoSidedSign`、`Arctangent2Fast`、`Length`、`MaterialXLuminance`、`Time`、`SceneDepth`、`ObjectRadius`、`PerInstanceRandom`、`PerInstanceFadeAmount` |

`MaterialAttributes` 结果是 [`material`](../graph/material-attributes.md) 值。

<a id="side-effects-on-the-material"></a>

## 对材质的副作用

有两个类在建节点时会注册成材质参数，因而出现在材质实例编辑器里：

| 类 | 注册内容 |
| :-- | :-- |
| 带参数名的 `UMaterialExpressionStaticSwitchParameter` | 材质或材质函数上的「仅编辑器」静态开关值 |
| 带参数名的 `UMaterialExpressionStaticComponentMaskParameter` | 「仅编辑器」静态分量掩码值，取自节点的四个默认通道 |

两者的 expression GUID 无效时都会被重新分配一个。

<a id="node-reuse"></a>

## 节点复用

IR 把类相同、属性相同、输入相同的两个节点合并成一个，读第二个的地方都改读第一个 *（自 2.0.0 起）*。比较的是
节点，不是调用的拼写：参数顺序、别名、只差大小写都不影响；`Custom` 节点和其他节点一样合并。
语句 —— 没有输出的节点 —— 永不合并。参见
[节点复用](../graph/node-reuse.md)。

<a id="sampletexture2d"></a>

## SampleTexture2D

```c
SampleTexture2D(<texture-object>, <uv>)
```

一个保留的双参数形式，由 legacy 前端在用户 property 和函数之前解析，且**区分大小写**。它会被改写成

```c
UE.Expression(Class = "TextureSample", TextureObject = <arg0>, Coordinates = <arg1>)
```

调用代表整个采样：`float4 t = SampleTexture2D(T, uv)` 是 `RGBA` 输出，
`float3 c = …` 是 `RGB` 输出，`.a` 是 `A` 输出。两个参数都是
位置参数，且都必需；其他任何形状 —— 命名参数、第三个参数、输出选择器 —— 都报
[`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256)。

用纹理采样参数 token（`TextureSampleParameter2D` 及其同类）声明的 property，则改用它的 pin 调用 ——
`BaseTex(Coordinates = uv)`。参见
[Graph 中的参数](../../parameters/graph-usage.md)。

<a id="declaration-form"></a>

## 声明形式

`UE.<Name>(…)` —— 包括 `UE.Expression(Class = …)` —— 可以作为 property 类型写在
[`Properties`](../language/properties.md) 里。它是任何 `UE.` 调用的[声明形式](ue.md#properties-declaration-form)：
在每个读取该 property 的 `Graph` 体开头声明一个局部变量，绑定方式与
`Graph` 形式相同。与 1.x 的不同 *（自 2.0.0 起）*：

| 方面 | 现在 | 1.x |
| :-- | :-- | :-- |
| `OutputType` / `ResultType` | 可选；是局部变量的类型 —— 见[声明的输出宽度](ue.md#declared-output-width) | 必需，取自缩减 token 集 |
| 输入 pin 名 | 和 `Graph` 中一样匹配 | 不匹配 —— 只匹配属性 |
| 输入值 | 带引号的字符串、数字、`true` / `false`，或一个词（另一个变量）；其他任何值，包括向量字面量 `float3(…)`，都保留为文本，pin 会拒绝（[`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202)） | 已声明的 property、标量，或 2–4 分量向量字面量 |
| `ParameterName` | 只在写了时设置 | 类有它时自动取 property 名 |
| 元数据 `[ … ]` | 读取但不应用 | 应用到节点上 |
| `Output` / `OutputIndex` | 普通参数：`DSH5213`，或 `DSH5291` 加 `DSH8212` | 选择输出 |

<a id="notes"></a>

## 说明

- 属性名是**扁平**的。这里不接受带点的路径和 `[index]` 选择器；那些只存在于
  [`Settings`](../settings/material.md) 一节，那是一个不同的解析器。
- 必需的 pin 没连是警告 —— 1.x 源码里的 [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279) —— 节点真需要这个 pin 时，引擎在编译材质时报错。
- 名字写错和 `Class` 写错是两个码：`UE.Sinee()` 报
  [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)，`UE.Expression(Class = "Sinee")` 报 `DSH5212`。
- 调用可以像任何表达式一样 swizzle：
  `UE.Expression(Class = "VertexColor").rgb`。参见
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result)。
- `dsc export-catalog` 写出每个类的 pin、属性、别名和输出 —— 这是查参数名最实用的办法。编辑器也会把清单导出到
  `Saved/DreamShader/Bridge/material-expressions.json`；参见[Bridge](../../tools/bridge.md)。

<a id="diagnostics"></a>

## 诊断

每个码都在[诊断](../diagnostics/index.md)里它自己的页面上列出了消息和完整说明。

| 码 | 触发条件 |
| :-- | :-- |
| [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218) | `UE.Expression` 没有 `Class` |
| [`DSH5217`](../diagnostics/DSH5xxx.md#dsh5217) | `Class` 不是带引号的字符串、词或带点的名字 |
| [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212) | `Class` 没匹配上目录里任何类 |
| [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) | `UE.` 后面的名字没匹配上目录里任何类 |
| [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216) | `Class` 写在 `Substrate.*` 调用上 |
| [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)（警告） | 在 1.x 源码中，类、pin、属性或输出只在忽略大小写时匹配 |
| [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) / [`DSH5221`](../diagnostics/DSH5xxx.md#dsh5221) | 在没有参数顺序的类上写了位置参数，或位置参数超出了末尾 |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | 参数没有指定任何 pin 或属性 |
| [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)（信息） | 没有指定 pin 的参数被保留，留给建好的节点来命名 |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | pin、属性或 Custom 输入给了两次 |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | 值放不进它的 pin |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)（信息） | 在 1.x 源码中，更宽的向量整体连接 |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | 属性值不是该属性接受的那种字面量或常量 |
| [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) | 枚举值不是该枚举的值 |
| [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278)（警告） | 在 1.x 源码中，枚举值按 1.x 的方式书写 |
| [`DSH5284`](../diagnostics/DSH5xxx.md#dsh5284) | Custom 输入收到了 Custom pin 承载不了的值 |
| [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261) | Custom `OutputType` 不在可接受的 token 中 |
| [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288)（警告） | 在 1.x 源码中，在没有该属性的类上写了 `DefaultValue` 会被丢弃 |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279)（警告） | 在 1.x 源码中，必需的 pin 没有连接 |
| [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250)–[`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253)、`DSH5201`、`DSH5282` | 输出选择器 —— 参见[选择输出](#selecting-an-output) |
| [`DSH4231`](../diagnostics/DSH4xxx.md#dsh4231) | 没有输出的节点被当作值使用 |
| [`DSH4373`](../diagnostics/DSH4xxx.md#dsh4373) | 属性值在运行时计算 |
| [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) | `SampleTexture2D` 不是恰好两个位置参数 |
| [`DSH8211`](../diagnostics/DSH8xxx.md#dsh8211) | 构建资产的引擎没有这个类 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 节点创建失败 |
| [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) | 属性写不进去；消息带着 `DSH7131`–`DSH7144` 或资产解析器的码 |
| [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) | 建好的节点没有以 `DSH5291` 保留的那个名字的 pin |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Generic")
{
    Properties {
        Texture2D BaseTex = Path(Game, "Textures/T_Noise");
        vec3      Dimmed  = vec3(0.2, 0.2, 0.2);
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // Class defaults to the function name.
        float pulse = UE.Sine(Input = UE.Time());

        // Explicit Class, an enum property by its value's name, then output 0 swizzled.
        float3 scene = UE.Expression(Class = "SceneTexture",
                                     SceneTextureId = "PostProcessInput0").Color.rgb;

        // An object property through Path(...), and a pin by its pin name.
        float4 tex = UE.Expression(Class = "TextureSample",
                                   Texture      = Path(Game, "Textures/T_Noise"),
                                   Coordinates  = UE.TexCoord(Index = 0));

        // Pins by their display names (True, False for A, B), and a bool property.
        float3 sel = UE.Expression(Class = "StaticSwitch",
                                   True = Dimmed, False = tex.rgb, DefaultValue = true);

        Color = (scene + sel) * pulse;
    }
}
```

生成的节点：

```text
Time                                  -> Sine                       (pulse)
SceneTexture (PPI_PostProcessInput0)  -> Color, mask .rgb           (scene)
TextureCoordinate (Index 0)           -> TextureSample.Coordinates
TextureSample (Texture = T_Noise)                                   (tex)
Constant3Vector (0.2, 0.2, 0.2)       -> StaticSwitch.True
StaticSwitch  (DefaultValue = true)                                 (sel)
Add, Multiply                                                       (Color)
```

<a id="see-also"></a>

## 另见

- [内置](index.md) —— 调用面和解析顺序
- [`UE.*` 目录](ue.md) —— 1.x 名字及其参数列表
- [`OutputType`](output-type.md) —— 该参数仍然做什么，以及输出名
- [Substrate](substrate.md) —— 共用这条路径的姊妹命名空间
- [Substrate 语法糖](../language-v2/substrate.md) —— 虚拟参数和运算符
- [`Path(…)`](../parameters/path.md) —— 对象属性的资产引用语法
- [图中的参数](../../parameters/graph-usage.md) —— 参数的 pin 调用形式
- [材质属性](../graph/material-attributes.md) —— `material` 值种类
- [转换](../graph/conversions.md) —— 宽度以及值如何适配
- [节点复用](../graph/node-reuse.md) —— 相同的节点
- [图函数](../language/graph-function.md) —— `UE.*` 调用提升为 Custom 节点 pin
- [Bridge](../../tools/bridge.md) —— 导出的反射表达式清单
- [诊断索引](../diagnostics/index.md) —— 每个码
