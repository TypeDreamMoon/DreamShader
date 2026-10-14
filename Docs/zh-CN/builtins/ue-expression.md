---
title: 通用 UE.Expression
description: 按类名创建内置目录里任意 UMaterialExpression 的反射调用 —— 类解析、参数分派、OutputType、输出选择、Custom 节点与结果类型。
---

`UE.Expression` 是反射节点调用：按名字创建内置目录里任意一个 `UMaterialExpression` 类，并用参数填充它的输入
pin 和属性。目录就是运行中引擎的材质表达式类（含已加载插件的类），靠反射读出 —— [`UE.*` 节点](ue.md)
里的 27 个 1.x 名字也只是目录里的条目。

```c
float pulse = UE.Expression(Class = "Sine", Input = UE.Time());
```

还有一种更短的写法。调用用 `Class` 指明类，没写时用 `UE.` 后面的名字，所以 `UE.Sine(…)` 和
`UE.Expression(Class = "Sine", …)` 完全相同；在别的名字上写 `Class`，以 `Class` 为准。`Expression` 不是任何类的
名字，所以第一种写法必须写 `Class` —— 不写报 `DSH5218`。

```c
float pulse = UE.Sine(Input = UE.Time());
```

`OutputType` 不再必需 （自 2.0.0 起）：目录知道每个类产出什么。见 [OutputType](#outputtype)。

```c
UE.Expression( Class = <class-specifier>
               [, { OutputType | ResultType } = <type-token> ]
               [, { { Output | OutputName } = <text> | OutputIndex = <int> } ]
               [, <arg-name> = <expression> ] … )

UE.<ClassName>( [ <arg-name> = <expression> ] … )
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

参数是命名参数。位置参数只在目录给了参数顺序的类上能绑定 —— `TextureCoordinate`、`Constant`、`Constant2Vector`、
`Constant3Vector`、`Constant4Vector`、`Transform`、`TransformPosition`、`Panner`、`ComponentMask`、`Time`，以及五个
Substrate 组合节点 —— 其他地方报 `DSH5220`，超出顺序报 `DSH5221` （自 2.0.0 起）。在
[27 个 1.x 名字](ue.md#完整名单)上，legacy 前端会先把位置参数丢弃（`DSH5254`），两个变换的
`Input` 除外。文本参数可以带引号也可以不带：`Class = Sine` 与 `Class = "Sine"` 相同。

<a id="解析顺序"></a>

## 解析顺序

调用依次经过编译器的各个阶段。每个阶段报出**全部**错误，有错误的阶段会让流水线在下一阶段之前停下
（自 2.0.0 起）（1.x 停在第一个失败的步骤）。

| # | 阶段 | 对调用做什么 | 码 |
| :-- | :-- | :-- | :-- |
| 1 | legacy 前端 | `Output` / `OutputIndex` 变成调用上的选择；`OutputType` / `ResultType` 从调用上取下（Custom 调用上保留）；改写 1.x 简写 | `DSH5250`–`DSH5259`、`DSH5261`、`DSH5263`–`DSH5265` |
| 2 | 绑定器 —— 类 | 在目录里查 `Class` 或名字 | `DSH5216`、`DSH5217`、`DSH5218`、`DSH5212`、`DSH5210`、`DSH5294`、`DSH5300`、`DSH5276` |
| 3 | 绑定器 —— 参数 | 每个参数绑定到 pin、属性、Custom 输入、Substrate 参数，或留给建好的节点 | `DSH5220`、`DSH5221`、`DSH5285`、`DSH4215`、`DSH5214`、`DSH5224`、`DSH5215`、`DSH5278`、`DSH5284`、`DSH5288`、`DSH5291`、`DSH5213` |
| 4 | 绑定器 —— 调用 | 必需的 pin；值的类型 | `DSH5279`（`.dss` 中 `DSH5219`）、`DSH4231` |
| 5 | IR 构建 | 属性值变成字面量 | `DSH4373` |
| 6 | 发射器 | 创建节点、写入属性、连接 pin | `DSH8211`、`DSH8214`、`DSH8213`、`DSH8212`、`DSH8254` |

<a id="类解析"></a>

## 类解析

`Class` 接受带引号的字符串、裸词或带点的名字（否则 `DSH5217`）。说明符去空白后在目录里按下列顺序查找：

| # | 匹配 |
| :-- | :-- |
| 1 | 目录的短名 —— `Sine` |
| 2 | 类名 —— `MaterialExpressionSine` |
| 3 | 类路径 —— `/Script/Engine.MaterialExpressionSine` |
| 4 | `MaterialExpression` + 说明符，以及带 `U` 的 C++ 名 —— `UMaterialExpressionSine` |
| 5 | 目录带的别名 —— `TexCoord`、`Lerp`、`Mask`、`FunctionCall`…… |
| 6 | 仅 1.x 源码：第 1–3 行忽略大小写的唯一匹配，并给出 `DSH5276` |

都没匹配上报 `DSH5212`。第 1–5 行区分大小写。

**提示：**

`UMaterialExpressionSine` 现在能解析 （自 2.0.0 起）（1.x 比较的是反射名，带 `U` 的写法永远匹配不上）。
`USine` 不能。

**提示：**

目录列出构建它时已加载的类，抽象类和已弃用的类不在其中。位于编辑器尚未加载的插件模块中的表达式类不在目录里。

在 [`Substrate.*`](substrate.md) 路径上，名字就是类，写 `Class=` 报 `DSH5216`。

<a id="outputtype"></a>

## OutputType

`OutputType` —— 或它的别名 `ResultType` —— **不再必需** （自 2.0.0 起）（1.x 在每个通用调用和名单之外的
property 声明上都要求它）。legacy 前端不区分大小写地找到它们并从调用上取下，它们永远不会被当作 pin 或属性绑定；
值的类型来自目录。token 可以带引号也可以不带。`.dss` 没有这个参数。它还在三个地方起作用：

| 使用面 | token | 效果 |
| :-- | :-- | :-- |
| `Graph` 中的 `UE.Expression(Class = "Custom", …)` | 下面的 Custom token 集 | **节点的输出类型** —— 写入节点，决定 HLSL 返回类型 |
| `Graph` 中其他 `UE.*` 调用 | 标量或向量类型拼写 | 在**只有一个数值输出**的节点上：值的类型，和 1.x 一样，节点整体连接；否则丢弃 |
| `UE.<Name>(…)` property 声明 | 任意类型拼写 | 声明变成的局部变量的类型 —— 见[声明的输出宽度](ue.md#声明的输出宽度) |
| [27 个 1.x 名字](ue.md#完整名单)之一 | —— | 和任何不读的参数一样被 `DSH5254` 丢弃 |
| `Substrate.*` | —— | 不报诊断直接丢弃 |

哪一条都不合适的 token —— 非 Custom 调用上的纹理、`MaterialAttributes`、`Substrate` 或 `SamplerState` token，矩阵，
拼写错误 —— 不报诊断直接丢弃 （自 2.0.0 起）（1.x 报不支持的 token），值由目录定类型。

**提示：**

宽度提示在目录会建出另一张图的地方跟随 1.x：`float Cam = UE.Expression(Class = "CameraPositionWS", OutputType = "float")`
把 `Cam` 定为 `float`，读 `Cam` 的地方都连接节点的整个输出 —— 1.x 的图就是这样。在引擎给的类型和提示一致的节点上，
它什么都不改变。

<a id="custom-节点上的-token"></a>

### Custom 节点上的 token

忽略大小写并去掉所有空格后比较。其他任何 token 报 `DSH5261`。

| Token | Custom 输出类型 |
| :-- | :-- |
| `float`、`float1`、`half`、`half1` | `Float1` |
| `float2`、`vec2`、`half2` | `Float2` |
| `float3`、`vec3`、`half3` | `Float3` |
| `float4`、`vec4`、`half4` | `Float4` |
| `MaterialAttributes` —— 带引号 | `MaterialAttributes` |

（自 2.0.0 起） `int`、`uint`、`bool` 及其向量、`ivec*`、`uvec*`、`bvec*`、纹理 token、`StaticBool` 和
`Substrate` 在这里都报 `DSH5261`。不带引号的 `MaterialAttributes` 被读成类型 `material`，同样报 `DSH5261`；要加引号。

<a id="其他调用和-property-声明上的-token"></a>

### 其他调用和 property 声明上的 token

token 是 1.x 的类型拼写，忽略大小写比较：

| Token | 类别 | `Graph` 调用 | property 声明 |
| :-- | :-- | :-: | :-: |
| `float`、`half`、`double`、`int`、`uint`、`bool`，以及各自带 `1` 的写法 | 标量 | ✔ | ✔ |
| 上述带 `2`、`3`、`4` 的写法；`vec2`–`vec4`、`ivec2`–`ivec4`、`uvec2`–`uvec4`、`bvec2`–`bvec4` | 向量 | ✔ | ✔ |
| `StaticBool`、`StaticBoolParameter` | 读作 `bool` | ✔ | ✔ |
| `Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` | 纹理 | 丢弃 | ✔ —— 保留维度 |
| `MaterialAttributes` | `material` | 丢弃 | ✔ |
| `Substrate`、`SamplerState` | | 丢弃 | ✔ |
| `mat2`–`mat4`、`float3x3` 和其他矩阵 | 矩阵 | 丢弃 | 能读，但没有节点的值放得进矩阵局部变量 |

在 `Graph` 调用上，token 去掉首尾空白但保留内部空格（`" float 4 "` 不是 token）；在 property 声明上先去掉所有空格。
下划线和连字符永远不会被去掉：`float_4` 和 `Material-Attributes` 都不是合法拼写。两个只在 `OutputType` 拼写上
不同的调用 —— `float3` 对 `vec3` —— 是同一个节点：IR 合并的是节点，不是拼写。

<a id="参数分派"></a>

## 参数分派

这些名字不会被分派：

| 名字 | 用途 |
| :-- | :-- |
| `Class` | 类说明符；只认这一种拼写 |
| `OutputType`、`ResultType` | 声明的输出类型 —— 前端取下，大小写不限 |
| `Output`、`OutputName` | 按名字选择输出 —— 大小写不限 |
| `OutputIndex` | 按索引选择输出 —— 大小写不限 |

其他参数按下列顺序对类的目录条目解析，第一个匹配上的胜出。

| # | 判断 | 结果 |
| :-- | :-- | :-- |
| a | 名字是类的输入 pin 或其别名 | 把值连到那个 pin |
| b | 名字是类的属性或其别名 | 把值写进节点 |
| c | 1.x 源码：唯一一个忽略大小写才匹配的 pin，其次是属性 | 同 a 或 b，并给出 `DSH5276` |
| d | 类是 `Custom` | 节点的一个新输入，按书写原样命名 —— 见 [Custom 节点](#custom-节点) |
| e | 节点是 Substrate BSDF，名字是一个[虚拟参数](../language-v2/substrate.md#slab-上的旧参数) | pin 前面一个转换节点的输入 |
| f | 1.x 源码：在没有 `DefaultValue` 的类上写了 `DefaultValue` | 以 `DSH5288` 丢弃 |
| g | 1.x 源码，或 pin 取决于属性的类：值是数字、纹理、material、Substrate 值或节点 | 以 `DSH5291` 保留，建节点时按这个名字连接（节点没有这个 pin 时 `DSH8212`） |
| h | —— | `DSH5213`，拼写接近时附带 *did you mean* |

输入 pin 名优先于同名属性；真正的 pin 名或属性名优先于别名。同一个 pin 或属性给两次报 `DSH4215`。

别名有：

| 别名 | 示例 |
| :-- | :-- |
| bool 属性去掉开头 `b` 的名字，`b` 后面紧跟大写字母时 （自 2.0.0 起） | `FractionalPart` → `bFractionalPart` |
| 属性的显示名，去掉空格或把空格换成 `_` | `ShaderOffsets`、`Shader_Offsets` → `WorldPositionShaderOffset` |
| pin 的显示名写成标识符 | `True`、`False` → `StaticSwitch.A`、`StaticSwitch.B` |
| 没有规则可循的 1.x 拼写 | `Index`（`TextureCoordinate.CoordinateIndex`）、`Origin`（`ObjectPositionWS.OriginType`）、`Source` / `Destination`（`Transform`、`TransformPosition`）、`Asset` / `Parameter`（`CollectionParameter`）、`UV`（`TextureSample.Coordinates`） |

目录列出输入 pin，以及可编辑、既未弃用也非 transient 的属性；1.x 能触达任意 `UPROPERTY`，可编辑与否都行
（自 2.0.0 起）。除此之外参数名必须精确：不会去掉任何分隔符。

<a id="输入-pin"></a>

### 输入 pin

解析到 pin 的参数是表达式，会被连接。它的类型按目录给 pin 的类型检查：

| pin | 值 | 结果 |
| :-- | :-- | :-- |
| `float1`–`float4` | 同宽，或标量（铺满 pin） | 连接 |
| `float1`–`float4` | 更窄的向量 | 原样连接 —— 分量少了节点怎么处理是节点的事 |
| `float1`–`float4` | 更宽的向量 | 1.x 源码里整体连接，给出提示 `DSH5289`；否则 `DSH5214` |
| 引擎没给类型的 pin | 任意数值、material、Substrate 值 | 原样连接 |
| `MaterialAttributes`、Substrate、纹理 | 别的种类的值 | `DSH5214` |

<a id="字面量属性"></a>

### 字面量属性

其余匹配上的名字是属性，建节点时写进去。绑定器检查值；发射器用 1.x 的字面量写入器写入，写不进去的值报
`DSH8213`，其消息带着写入器自己的码。

| 属性类型 | 绑定器接受 | 发射器读作 | `DSH8213` 中的码 |
| :-- | :-- | :-- | :-- |
| `FBoolProperty` | 常量 | `true` / `false`，不区分大小写 | `DSH7132` |
| `FIntProperty` | 常量 | 十进制整数 | `DSH7133` |
| `FUInt32Property` | 常量 | `0 … 4294967295` 内的整数 | `DSH7134` |
| `FFloatProperty` | 常量 | 数字 | `DSH7135` |
| `FDoubleProperty` | 常量 | 数字 | `DSH7136` |
| `FStrProperty` | 带引号的字符串、词或带点的名字 | 去空白后的文本，原样写入 —— **永远成功** | —— |
| `FNameProperty` | 同上 | 去空白后的文本作为 `FName` —— **永远成功** | —— |
| `FObjectPropertyBase` | 同上 | 见[对象属性](#对象属性) | `DSH7137`–`DSH7139` |
| `FEnumProperty`、带枚举的 `FByteProperty` | 见[枚举值](#枚举值) | 枚举值 | `DSH7140`、`DSH7141` |
| 不带枚举的 `FByteProperty` | 常量 | `0 … 255` 内的整数 | `DSH7142` |
| 其他 —— 结构体、数组 | 带引号的字符串或词 | Unreal 自己的导入文本，例如 `"(R=1,G=0,B=0,A=1)"` | `DSH7143` |

绑定器不接受的值报 `DSH5224`：数值属性收到运行时计算的值，或文本属性收到数字。

**警告：**

`FStrProperty` 和 `FNameProperty` 永远不会失败。参数携带的任何文本 —— 包括解析不了的路径 —— 都被原样存下来，
既不报诊断，也不回退到默认值。

<a id="枚举值"></a>

### 枚举值

绑定器把枚举值与目录给该属性列出的值匹配 —— 隐藏值不算，每个值都写成不带前缀的名字（`PPI_PostProcessInput0`
写成 `PostProcessInput0`）。

| 写法 | `.dss` 中 | 1.x 源码中 |
| :-- | :-- | :-- |
| `PostProcessInput0` | 接受 | 接受 |
| `postprocessinput0` | `DSH5215`，附带 *did you mean* | 接受，给出 `DSH5278` |
| `PPI_PostProcessInput0`、`ESceneTextureId::PPI_PostProcessInput0` | `DSH5215` | 接受，给出 `DSH5278` |
| 与值名不同的显示名 | `DSH5215` | `DSH5215` （自 2.0.0 起） |

1.x 的匹配忽略大小写、空格、制表符以及 `_`、`-`、`:`、`.`、`/`，并依次尝试整个值、`Enum::` 作用域之后、去掉自身
前缀之后。节点按目录的拼写写入。[`UE.SceneTexture`](ue.md#uescenetexture) 和变换的坐标系都依赖这一点。

<a id="对象属性"></a>

### 对象属性

对象属性接受 `Path(<root>, "<asset>")`、带引号的对象路径 `"/Game/…"`，或 `Class'/Game/…'`。`Path(…)` 参数作为文本
携带，建节点时解析。参见 [Path 资产引用](../parameters/path.md)。

| 情况 | 结果 |
| :-- | :-- |
| 值既不是带引号的字符串、词，也不是 `Path(…)` | `DSH5224` |
| 文本以 `Path(` 或 `/` 开头、或以 `'` 结尾，但解析不了 | `DSH8213`，带解析器自己的消息 |
| 文本不是资产引用 | `DSH8213`（`DSH7137`） |
| 资产加载失败，且属性是名为 `Texture` 或 `TextureObject` 的 `UTexture` 属性 | 属性被设为 **null**，并算作**成功** |
| 资产加载失败，其他属性 | `DSH8213`（`DSH7138`） |
| 资产加载成功但类不对 | `DSH8213`（`DSH7139`） |

**警告：**

**失败置空这条规则是静默的。** `UE.Expression(Class = "TextureSample", Texture = Path(Game, "Missing/T_Nope"))`
会带着一个未赋值的纹理编译通过，而不是报告资产缺失。只有 `UTexture` 类型属性上的 `Texture` 和 `TextureObject`
这两个名字会这样。

<a id="选择输出"></a>

## 选择输出

多输出的节点通过两个互斥的选择器之一读取，前端把它们变成调用上的选择。

| 参数 | 别名 | 类型 | 变成 |
| :-- | :-- | :-- | :-- |
| `Output` | `OutputName` | 带引号的名字或一个词 | 调用上的 `.Name` |
| `OutputIndex` | —— | ≥ 0 的整数 | 调用上的 `[k]` |

| 错误 | 码 |
| :-- | :-- |
| 两个选择器都写 | `DSH5252` |
| `OutputIndex` 不是 ≥ 0 的整数 | `DSH5250` |
| `Output` 既不是带引号的名字也不是一个词 | `DSH5251` |
| 构造器上的选择器 | `DSH5253` |
| 类没有这个名字的输出 | `DSH5201` |
| 索引超过最后一个输出 | `DSH5282` |

名字与目录列出的输出精确比较；1.x 源码里只在忽略大小写时匹配的名字也接受，给出 `DSH5276`。名字可以带空格
（`Output = "Second Roughness"`）。多输出节点上引擎没命名、带掩码的输出，按掩码保留的通道列出 —— `R`、`G`、`B`、
`A`、`RG`、`RGB`、`RGBA` —— 除非类已经用了这个名字；单个输出保留空名字，就是调用的值。

| 节点 | 目录列出的输出（UE 5.8） |
| :-- | :-- |
| `VertexColor` | `RGB`、`R`、`G`、`B`、`A` |
| `TextureSample` | `RGB`、`R`、`G`、`B`、`A`、`RGBA` |
| `SceneColor` | `RGB`、`A` |
| `SceneTexture` | `Color`、`Size`、`InvSize` |

在输出都是同一个值的通道视图的节点上，某个输出范围内的 swizzle 就是那个输出（`UE.VertexColor().a`）。
（自 2.0.0 起） 之前 1.x 按固定顺序拿七个掩码名去试每个无名输出。

**提示：**

这些不是 `OutputType` 的取值，`OutputType` 的 token 也永远不是合法的 `Output` 选择器。
`UE.Expression(Class = "BreakMaterialAttributes", Output = "BaseColor")` 按输出*名字*选择；
`UE.VertexColor(Output = "RGB")` 选择掩码给出名字的那个输出。

两个选择器都不用时，调用就是节点本身：见[结果类型与分量数](#结果类型与分量数)。

<a id="custom-节点"></a>

## Custom 节点

`UE.Expression(Class = "Custom", …)` —— 也可以写 `MaterialExpressionCustom` —— 建出一个 pin 就是调用参数的节点：

| 方面 | 行为 |
| :-- | :-- |
| `OutputType` | 节点的输出类型，见 [Custom 节点上的 token](#custom-节点上的-token)；不写时用类自己的输出类型 |
| `AdditionalOutputs` | 节点的其他输出，用引擎的导入文本写；之后每个都按名字选择，输出 0 叫 `return` |
| 不是 pin 也不是属性的参数 | 一个新输入，按书写原样命名；值不是数字、bool 或纹理时报 `DSH5284`，同名两次报 `DSH4215` |
| 新建的节点 | 写入调用的输入之前，先清空它的 `Inputs` 和 `AdditionalOutputs` 数组 |
| 输出选择器 | 指向声明过的某个输出：其他名字报 `DSH5201`，超出的索引报 `DSH5282` （自 2.0.0 起）（1.x 会合成 `Output1`、`Output2`… 占位输出直到所要的那个） |
| 节点复用 | 两个相同的 Custom 调用是一个节点，和任何两个相同的调用一样 （自 2.0.0 起） |

```c
float3 tinted = UE.Expression(Class = "Custom", OutputType = "float3",
                              Code = "return In * 0.5f;",
                              In   = baseColor);
```

<a id="结果类型与分量数"></a>

## 结果类型与分量数

调用的值，取决于目录对类的描述：

| # | 类 | 值 |
| :-- | :-- | :-- |
| 1 | 没有输出（自定义输出类） | 没有 —— 调用是一条语句；当值用报 `DSH4231` |
| 2 | 是 `Custom` | `OutputType` 给的类型；有 `AdditionalOutputs` 时是输出带名字的节点 |
| 3 | 有多个输出 | 一个节点：指名一个输出，或在第一个输出 —— 或各通道输出拼成的整个值 —— 恰好合适的地方使用。1.x 源码里第一个输出没有固定宽度的读作那个输出（`DSH5287`），直接传给 pin 的任何节点也读作第一个输出 （自 2.1.1 起）；否则 `DSH5201` |
| 4 | 只有一个已知类型的输出 | 那个类型 |
| 5 | 只有一个引擎没给类型的输出 | 和同样没给类型的 pin 上最宽的数值一样宽 —— 或这样的 pin 携带的 Substrate / material 值 —— 否则和它被读入的位置一样宽 |

1.x 源码里，第 4、5 行的类上的数值 `OutputType` 会改为决定值的类型，见上。

输出的类型：有掩码时是掩码的宽度，否则是引擎的值类型；引擎只说「一个 float」时由已知宽度表决定：

| 宽度 | 类 |
| :-- | :-- |
| 2 | `TextureCoordinate`、`Panner`、`Rotator`、`SceneTexelSize`；`SceneTexture` 的 `Size` 和 `InvSize` 输出 |
| 3 | `ObjectPositionWS`、`CameraVectorWS`、`VertexNormalWS`、`VertexTangentWS`、`Transform`、`TransformPosition`、`SkyAtmosphereLightDirection`、`PixelNormalWS`、`CrossProduct`、`ObjectBounds`、`CameraPositionWS`、`ReflectionVectorWS` |
| 1 | `PixelDepth`、`TwoSidedSign`、`Arctangent2Fast`、`Length`、`MaterialXLuminance`、`Time`、`SceneDepth`、`ObjectRadius`、`PerInstanceRandom`、`PerInstanceFadeAmount` |

`MaterialAttributes` 结果是一个 `material` 值。参见[类型与值](../language/types.md)。

<a id="对材质的副作用"></a>

## 对材质的副作用

有两个类在建节点时会注册成材质参数，因而出现在材质实例编辑器里：

| 类 | 注册内容 |
| :-- | :-- |
| 带参数名的 `UMaterialExpressionStaticSwitchParameter` | 材质或材质函数上的「仅编辑器」静态开关值 |
| 带参数名的 `UMaterialExpressionStaticComponentMaskParameter` | 「仅编辑器」静态通道掩码值，取自节点的四个默认通道 |

两者的 expression GUID 无效时都会被重新分配一个。

<a id="节点复用"></a>

## 节点复用

IR 把类相同、属性相同、输入相同的两个节点合并成一个，读第二个的地方都改读第一个 （自 2.0.0 起）。比较的是
节点，不是调用的拼写：参数顺序、别名、只差大小写都不影响；`Custom` 节点和其他节点一样合并。语句 —— 没有输出的
节点 —— 永不合并。参见[节点复用](../graph/expressions.md#节点复用)。

<a id="sampletexture2d"></a>

## SampleTexture2D

```c
float4 texel = SampleTexture2D(BaseTex, uv);
```

一个保留的双参数形式，由 legacy 前端在用户 property 和函数之前解析，且**区分大小写** —— `sampletexture2d(t, uv)`
不是它。它会被改写成

```c
UE.Expression(Class = "TextureSample", TextureObject = <arg0>, Coordinates = <arg1>)
```

调用代表整个采样：`float4 t = SampleTexture2D(T, uv)` 是 `RGBA` 输出，`float3 c = …` 是 `RGB` 输出，`.a` 是 `A`
输出。两个参数都是位置参数，且都必需；其他任何形状 —— 命名参数、第三个参数、输出选择器 —— 都报 `DSH5256`。

**提示：**

如果要采样已声明的纹理*参数*，用参数自己的 pin 调用形式 —— `BaseTex(Coordinates = uv)`。参见
[Properties 类型](../language/properties.md#在-graph-中使用参数)。

<a id="声明形式"></a>

## 声明形式

`UE.<Name>(…)` —— 包括 `UE.Expression(Class = …)` —— 可以作为 property 类型写在 `Properties` 里。它是任何 `UE.`
调用的[声明形式](ue.md#properties-声明形式)：在每个读取该 property 的 `Graph` 体开头声明一个
局部变量，绑定方式与 `Graph` 形式相同。与 1.x 的不同 （自 2.0.0 起）：

| 方面 | 现在 | 1.x |
| :-- | :-- | :-- |
| `OutputType` / `ResultType` | 可选；是局部变量的类型 —— 见[声明的输出宽度](ue.md#声明的输出宽度) | 必需，取自缩减 token 集 |
| 输入 pin 名 | 和 `Graph` 中一样匹配 | 不匹配 —— 只匹配属性 |
| 输入值 | 带引号的字符串、数字、`true` / `false`，或一个词（另一个变量）；其他（包括向量字面量 `float3(…)`）保留为文本，pin 会拒绝（`DSH4202`） | 已声明的 property、标量，或 2–4 分量向量字面量 |
| `ParameterName` | 只在写了时设置 | 类有它时自动取 property 名 |
| 元数据 `[ … ]` | 读取但不应用 | 应用到节点上 |
| `Output` / `OutputIndex` | 普通参数：`DSH5213`，或 `DSH5291` 加 `DSH8212` | 选择输出 |

```c
Properties {
    UE.Expression(Class = "ObjectRadius", OutputType = "float1") Radius;
}
```

<a id="一些值得知道的细节"></a>

## 一些值得知道的细节

- 属性名是**扁平**的。这里不接受带点的路径和 `[index]` 选择器。
- 必需的 pin 没连是警告 —— 1.x 源码里的 `DSH5279` —— 节点真需要这个 pin 时，引擎在编译材质时报错。
- 名字写错和 `Class` 写错是两个码：`UE.Sinee()` 报 `DSH5210`，`UE.Expression(Class = "Sinee")` 报 `DSH5212`。
- 调用可以像任何表达式一样 swizzle：`UE.Expression(Class = "VertexColor").rgb`。
- `dsc export-catalog` 写出每个类的 pin、属性、别名和输出 —— 这是查参数名最实用的办法。编辑器也会把清单导出到
  `Saved/DreamShader/Bridge/material-expressions.json`，参见[编辑器工具](../tools/editor-integration.md)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5218: 'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'.` | UE.Expression 没写 Class。 |  |
| `DSH5217: 'Class' takes the expression class as a quoted string.` | Class 不是带引号的字符串、词或带点的名字。 |  |
| `DSH5212: '{0}' is not a material expression class this engine has.` | Class 没匹配上目录里任何类。 | 检查拼写；确认类所在模块已加载。 |
| `DSH5210: '{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive.` | UE. 后面的名字没匹配上目录里任何类。 |  |
| `DSH5216: 'Substrate.' already names the node, so it takes no 'Class' argument.` | Substrate.\* 调用上写了 Class。 |  [详解](substrate.md) |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告，仅 1.x：类、pin、属性或输出只在忽略大小写时匹配。 |  |
| `DSH5220: '{0}.{1}' takes named arguments: write 'Pin = value'.` | 在没有参数顺序的类上写了位置参数。 | 所有参数都写名字。 |
| `DSH5221: '{0}.{1}' takes {2} arguments in order; name the rest.` | 位置参数超出了类的参数顺序。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 参数既不是 pin 也不是属性。 | 用 dsc export-catalog 查 pin 名和属性名。 |
| `DSH5291: '{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties.` | 提示：不是任何已列出 pin 的参数被保留，留给建好的节点按名字连接。 |  |
| `DSH4215: The '{0}' argument of '{1}' is given twice.` | pin、属性或 Custom 输入给了两次。 |  |
| `DSH5214: {0} expects {1}, and this is {2}.` | 值放不进它的 pin。 |  |
| `DSH5289: {0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.` | 提示，仅 1.x：更宽的向量整体连接。 |  |
| `DSH5224: '{0}' is written into the node itself, not connected to it, so its value has to be known at compile time.` | 属性值不是该属性接受的那种字面量或常量。 |  |
| `DSH5215: '{0}' is not a value of '{1}' on '{2}'.` | 枚举值不是该枚举的值。 |  |
| `DSH5278: '{0}' is not spelled like a value of '{1}', and 1.x matched enumerators loosely, so this is '{2}'; a '.dss' writes '{2}'.` | 警告，仅 1.x：枚举值按 1.x 的方式书写。 |  |
| `DSH5284: '{0}' becomes an input of the custom node, which carries a number or a texture, and this argument is {1}.` | Custom 输入收到了 Custom pin 承载不了的值。 |  |
| `DSH5261: Expected 'OutputType' of a Custom expression to be float1 to float4 or MaterialAttributes, found '{0}'.` | Custom 节点的 OutputType 不在可接受的 token 中。 |  |
| `DSH5288: '{0}.{1}' has no 'DefaultValue', so the default written for this parameter is dropped, as 1.x dropped it.` | 警告，仅 1.x：在没有 DefaultValue 的类上写了 DefaultValue。 |  |
| `DSH5279: '{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.` | 警告，仅 1.x：必需的 pin 没有连接。 |  |
| `DSH5250: Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'.` | OutputIndex 不是 ≥ 0 的整数。 |  |
| `DSH5251: Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'.` | Output 既不是带引号的名字也不是一个词。 |  |
| `DSH5252: Expected either 'Output' or 'OutputIndex' on this call, found both.` | 两个输出选择器都写了。 |  |
| `DSH5253: Expected an output selector only on a call that has outputs, found one on the constructor '{0}'.` | 构造器上写了输出选择器。 |  |
| `DSH5201: '{0}.{1}' has no output called '{2}'; its outputs are {3}.` | 类没有这个名字的输出。 |  |
| `DSH5282: This Custom node declares {0} output(s), counted from 0, and this selects output {1}.` | 输出索引超过最后一个输出。 |  |
| `DSH4231: '{0}.{1}' is an output node, not a value: write it as a statement on a line of its own.` | 没有输出的节点被当作值使用。 |  |
| `DSH4373: '{0}' is a property of {1} and needs a literal; this argument is computed at run time.` | 属性值是运行时计算出来的。 |  |
| `DSH5256: Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {0} argument(s).` | SampleTexture2D 不是恰好两个位置参数。 |  |
| `DSH8211: '{0}' is not a material expression class this engine has.` | 构建资产的引擎没有这个类。 |  |
| `DSH8214: Failed to create a '{0}' node.` | 节点创建失败。 |  |
| `DSH8213: '{0}' could not take '{1}' for its '{2}' property. {3}` | 属性写不进去；消息带着 DSH7131–DSH7144 或资产解析器的码。 |  [详解](../parameters/path.md) |
| `DSH8212: '{0}' has no input pin named '{1}'.` | 建好的节点没有以 DSH5291 保留的那个名字的 pin。 |  |

完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

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
        // Class 默认取函数名。
        float pulse = UE.Sine(Input = UE.Time());

        // 显式 Class，按值名写的枚举属性，然后取 Color 输出再 swizzle。
        float3 scene = UE.Expression(Class = "SceneTexture",
                                     SceneTextureId = "PostProcessInput0").Color.rgb;

        // 通过 Path(...) 写对象属性，并按 pin 名连接输入。
        float4 tex = UE.Expression(Class = "TextureSample",
                                   Texture     = Path(Game, "Textures/T_Noise"),
                                   Coordinates = UE.TexCoord(Index = 0));

        // pin 用显示名（True、False 即 A、B），外加一个 bool 属性。
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

<a id="接下来"></a>

## 接下来

- [UE.* 节点](ue.md) — 27 个 1.x 名字的参数表、变换内置与声明形式。
  - [Substrate 节点](substrate.md) — 共用这条绑定规则的姊妹命名空间。
  - [数学内置](math.md) — 无前缀的数值调用面。
  - [Path 资产引用](../parameters/path.md) — 对象属性使用的 Path(...) 语法。
