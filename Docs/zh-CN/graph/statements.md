---
title: 语句与声明
description: Graph 块体里的全部语句形式——声明、赋值、成员写入、调用——以及结束规则、分类顺序、MaterialAttributes 与作用域。
---

`Graph` 块体是一串语句。一共十种形式，除 `if` 外都以 `;` 结束。（自 2.0.0 起） 起块体由 2.0 的语句
解析器逐个记号读入，不再按文本在 `;` 处切分；1.x 没有的写法会被识别出来并带码拒绝，而不是被误读成一个
类型写错的声明。

<a id="语法概要"></a>

## 语法概要

```c
graph-statement :=
      <type> <name> [ = <expression> ] ;                          // declaration
    | <type> <name> [ = <init> ] , <name> [ = <init> ] … ;        // comma declarators
    | <type> <name> = { <expression> , … } ;                      // brace initializer
    | <name> = <expression> ;                                     // assignment
    | <name> . <member> = <expression> ;                          // member write
    | <call-expression> ;                                         // statement-form call
    | if ( <condition> ) { <graph-statement> … }
      [ else { <graph-statement> … } | else if ( … ) { … } … ]
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

<a id="十种形式"></a>

## 十种形式

| # | 形式 | 示例 | 结束符 |
| --: | :-- | :-- | :-- |
| 1 | 声明，无初始值 | `float3 c;` | `;` |
| 2 | 声明 + 表达式初始值 | `float3 c = A * K;` | `;` |
| 3 | 声明 + 花括号初始值 | `vec4 v = {rgb, 1.0};` | `;` |
| 4 | 赋值给已有变量或输出变量 | `Color = Tint;` | `;` |
| 5 | 赋值 + 花括号初始值 | `Color = {r, g, b};` | **拒绝**——`DSH2162` （自 2.0.0 起）；请写 `Color = float3(r, g, b);` |
| 6 | `MaterialAttributes` 成员写入 | `Attrs.BaseColor = Tint;` | `;` |
| 7 | 成员写入 + 花括号初始值 | `Attrs.BaseColor = {1, 0, 0};` | **拒绝**——`DSH2162` （自 2.0.0 起）；请写构造器 |
| 8 | 逗号分隔声明 | `float a = 1, b, c = 3;` | `;` |
| 9 | 语句形式调用 | `F_Split(Src, OutA, OutB);` | `;` |
| 10 | `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | 无——以最后一个块体的 `}` 结束 |

形式 8 会展开成每个声明子一个变量。它们全部共用**第一个**声明子的类型 token，每个声明子报告自己的行列。

<a id="结束与切分"></a>

## 结束与切分

| 规则 | 行为 |
| :-- | :-- |
| 分隔符 | 除 `if` 外每条语句都以 `;` 结束；缺了是 `DSH2154`，报在语句之后的第一个记号上 |
| 连续 `;` | 多出来的 `;` 是空语句，直接丢弃，不产生诊断 |
| 结尾 `;` | 块体**最后一条**语句也**必须**有 `;`：缺了是 `DSH2154` （自 2.0.0 起） |
| `if` 语句 | 以最后一个块体的 `}` 结束，所以 `if` 后面不需要 `;` |
| 位置 | 每条语句、每个表达式都带自己的行列 |
| 嵌套块体 | `if` / `else` 块体是同一文法的块，所以上面每一种形式在分支里都合法，包括嵌套 `if` |

```c
Graph = {
    float a = 1.0;;;             // 空语句被丢弃
    if (a > 0.5) { a = 0.0; }    // } 之后不需要 ;
    float b = a * 2.0;           // 最后一条语句也要 ;（缺了是 DSH2154）
}
```

解析失败的语句会被跳到它所在层级的下一个 `;`，解析器接着读下一条；一次构建会报出块体里的全部错误。

<a id="分类顺序"></a>

## 分类顺序

解析器按语句开头的记号判断它是哪种形式，第一条匹配的规则胜出。

| 顺序 | 语句开头 | 结果 |
| --: | :-- | :-- |
| 1 | 关键字 `if`（**区分大小写**） | 形式 10 |
| 2 | `for`、`while`、`do`、`return`、`break`、`continue`、`discard` | 能解析，随后被拒绝：`DSH2208` |
| 3 | `switch`、`case`、`default` | `DSH2160` |
| 4 | `{` | 裸块：`DSH2220` |
| 5 | `;` | 空语句 |
| 6 | `#Region` / `#EndRegion` 行 | 区域；见 [Layout 与 #Region](../language/layout.md) |
| 7 | 连续两个名字（`float3 c`），前面可带 `static` / `const` | 声明：形式 1–3 和 8。存储关键字是 `DSH2212` |
| 8 | 其他 | 表达式：赋值（形式 4–7）、调用（形式 9），或无人接收的值 `DSH2211` |

**警告：**

关键字区分大小写。`If (x) { … }` 不是 `if` 语句：`If` 是一个名字，`If (x)` 是一次调用，本该放调用语句
`;` 的位置出现了 `{`，于是报 `DSH2154`。见[不支持的写法](unsupported.md#大小写写错的关键字)。

**提示：**

声明的名字是一个标识符。`float3 A.B = x;` 在 `.` 处报 `DSH2154` （自 2.0.0 起）；1.x 会声明一个
名字字面为 `A.B` 的变量。成员写入前面不带类型：`Attrs.BaseColor = x;`。

<a id="声明"></a>

## 声明

```c
<type> <name> [ = { <expression> | <brace-initializer> } ]
     [ , <name> [ = { <expression> | <brace-initializer> } ] ] … ;

brace-initializer := { [ <expression> [ , <expression> ] … ] }
```

<a id="接受的类型-token"></a>

### 接受的类型 token

| Token | 分量数 | 说明 |
| :-- | --: | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | 1 | |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | 2 | |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | 3 | |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | 4 | |
| `MaterialAttributes` | —— | 见[下文](#materialattributes-成员写入) |
| `Substrate` | —— | （自 UE 5.4 起） 必须带初始值 |
| `StaticBool` `StaticBoolParameter` | 1 | 一个 `bool` 变量 （自 1.6.0 起） |
| `Texture2D` | —— | 必须带初始值 |
| `TextureCube` | —— | 必须带初始值 |
| `Texture2DArray` | —— | 必须带初始值 |
| `Texture3D` `VolumeTexture` | —— | 必须带初始值 |
| `SamplerState` | —— | 独立的采样器类型，不再等同 `Texture2D` （自 2.0.0 起）；必须带初始值 |

每一行都不区分大小写（`Float3`、`VEC3`、`materialattributes`）。类型 token 是**一个**记号
（自 2.0.0 起）：`float 3` 和 `Material Attributes` 不再被读成类型。`vec*`、`ivec*`、`uvec*`、`bvec*`
分别读作 `float*`、`int*`、`uint*`、`bool*`。

`int`、`uint`、`bool` 和 `half` 对编译器是各自的种类——两个整数构造器之间的 `/` 是 `DSH4243`，条件是一个
`bool`——但在生成图里全是浮点。矩阵拼法（`mat3`、`float3x3`）能解析，可图里没有矩阵值：`DSH4361`。完整
目录见[类型与值](../language/types.md)。

<a id="不带初始值的声明"></a>

### 不带初始值的声明

1.x 把未设置的变量当作零，所以 legacy 前端会给 `<type> <name>;` 补一个初始值：

| 声明类型 | 初始值 |
| :-- | :-- |
| 标量（1 分量） | `0`（`bool` 为 `false`） |
| 向量（2–4 分量） | `floatN(0, …)`——每个分量一个零，种类同声明 |
| `MaterialAttributes` | 一个空属性集（`MakeMaterialAttributes`） |
| `Substrate` | 无——`DSH2215` |
| 任意纹理类型、`SamplerState` | 无——`DSH2215` |
| 无法解析的 token | `DSH4201` |

```c
float3 c;              // float3(0, 0, 0)
MaterialAttributes A;  // 空属性集，可以直接写成员
Texture2D T;           // DSH2215：必须带初始值
```

<a id="带初始值的声明"></a>

### 带初始值的声明

先对初始值求值，再转换到声明类型：

| 顺序 | 步骤 |
| --: | :-- |
| 1 | 类型 token 必须能解析，否则 `DSH4201` |
| 2 | 标量广播到向量类型的每个分量 |
| 3 | 更宽的向量取它的**前导**分量——legacy 规则 L22，给出提示 `DSH5289` |
| 4 | 其他放不进去的——更窄的向量、把纹理放进数——是 `DSH4228` |

步骤 4 意味着 2 分量值永远不会被加宽成 3 分量。见[转换](expressions.md#转换)。

**警告：**

**声明宽度不再被"权威宽度"覆盖** （自 2.0.0 起）。1.x 让某些值（`UE.TexCoord`、`UE.CameraVectorWS`、
常量折叠的构造器、`dot`……）无视声明保持自己的宽度，于是 `float2 dir = UE.CameraVectorWS();` 静默地让
`dir` 有三个分量。现在声明类型就是变量的类型：`dir` 是 `float2`，取值的前两个分量，并给出 `DSH5289`。
依赖过这种覆盖的 1.x 源文件，构建出的图会和 1.9.x 不同，而且没有任何东西会报告这一点——请重新构建这类
材质并检查结果。

<a id="逗号分隔声明"></a>

### 逗号分隔声明

```c
float a = 1, b, c = 3;
```

| 规则 | 行为 |
| :-- | :-- |
| 识别条件 | 一个声明的声明子后面跟着 `,` 和另一个名字 |
| 共用类型 | 所有声明子都用**第一个**声明子的类型；`,` 后面再写类型 token 会被读成下一个声明子的名字，其后的内容是 `DSH2154` |
| 声明子名字 | 必须是标识符，否则 `DSH2163` |
| 初始值 | 每个声明子可以有自己的 `= <expression>` 或 `= { … }`，也可以没有 |
| 源码位置 | 每个声明子报告自己的行列 （自 2.0.0 起） |

上例声明了三个 1 分量值：`a` 初始化为 `1`，`b` 为 `0`，`c` 为 `3`。

<a id="花括号初始值"></a>

### 花括号初始值

声明的花括号初始值会被改写成声明类型的构造器调用：`T x = { a, b };` 就是 `T x = T(a, b);`。所以它**完全
就是**构造器调用，继承[构造器](expressions.md#构造器)的全部规则：只能用位置参数（`DSH4225`）、
单个标量会广播到所有分量、多个参数的分量数之和必须正好等于类型宽度（`DSH4222`）。

`{}` 是特例：它等于没有初始值，变量得到该类型的零值。

目标类型永远是声明的类型 token。其他任何位置的花括号列表——赋值或成员写入的右侧、调用参数——都是
`DSH2162` （自 2.0.0 起）。纹理、`Substrate` 和 `MaterialAttributes` 没有构造器：
`Texture2D t = {x};` 是 `DSH4223`。

**警告：**

花括号初始值不能嵌套。`float4 m = {{1,2},{3,4}};` 是 `DSH2214`；请写
`float4(float2(1, 2), float2(3, 4))`。

```c
Graph = {
    float r = 1.0;
    float g = 0.5;
    float b = 0.2;
    vec3 rgb  = {r, g, b};      // -> vec3(r, g, b)
    vec4 rgba = {rgb, 1.0};     // -> vec4(rgb, 1.0)，混合宽度打包
    Color = rgba.rgb;
}
```

<a id="重复声明"></a>

### 重复声明

在同一个块里声明一个已经声明过的名字是 `DSH4220`。在 `if` / `else` 块体里声明外层块体已经声明过的名字，
以及在 `Shader` 里声明名为 `Base`（材质本身）的变量，也是 `DSH4220`。

比较是**精确**的 （自 2.0.0 起）：`float a = 1; float A = 2;` 声明了两个变量，而 1.x 会报重复声明。
读 `a` 或 `A` 各自找到自己的变量；写成两者都不精确匹配的大小写时，只有唯一的不区分大小写匹配才会被采用，
并带警告 `DSH5275`。

给已有名字赋值不是重复声明——省略类型 token 就是重新赋值。

<a id="赋值"></a>

## 赋值

形式 4 和 6。目标是一个名字或 `name.member`；其他任何东西——成员的 swizzle、下标、调用——都是 `DSH2206`。

```c
<target> = <expression> ;
```

目标按以下顺序解析：

| 顺序 | 目标 | 行为 |
| --: | :-- | :-- |
| 1 | `name.member` | `MaterialAttributes` 成员写入——见[下文](#materialattributes-成员写入) |
| 2 | Graph 变量、`Shader` 的 `Outputs` 声明、函数类块的输出 | 值被转换到该变量的声明类型 |
| 3 | 属性 | `DSH4229`：属性是输入，请先复制到变量里 |
| 4 | 什么都不是 | 在这里以值自身的类型声明——legacy 规则 L26，给出提示 `DSH5292` |

名字先精确查找，再不区分大小写；另一种大小写的唯一匹配会被采用，并带警告 `DSH5275`——它**就是**那个
声明，而不是第二个变量 （自 2.0.0 起）。

规则 4 意味着 `=` 左侧出现未声明的名字不是错误：

```c
Graph = {
    vec3 Boost = Tint * 2.0;
    Scratch    = Boost;      // DSH5292：把 'Scratch' 声明为 float3
    Color      = Scratch;    // 'Color' 是 Outputs 声明 -> 转换到它的类型
}
```

规则 2 的转换会取更宽值的**前导分量**并给出提示 `DSH5289`（legacy 规则 L22），也会把标量广播到更宽的
目标。它绝不会把 2 分量加宽到 3 分量：那种情况以及其他放不进去的值都是 `DSH4228`。见
[转换](expressions.md#转换)。

<a id="表达式语句"></a>

## 表达式语句

形式 9。既不是声明也不是赋值的语句必须是一个**调用**，其他表达式都是 `DSH2211`。

语句形式下，函数的输出由末尾的参数接收，每个输出一个变量名，按声明顺序。调用任何东西都可以：一个没人
接收其值的调用——内置、构造器、参数——会被接受，它产生的节点因为没人读而被剪掉 （自 2.0.0 起）。
完整参数规则、out 目标约束和值形式见[调用](calls.md)。

多输出 `ShaderFunction` / `VirtualFunction` 的语句调用自 （自 1.3.5 起） 起可用；单输出
`Function` / `GraphFunction` 的值调用自 （自 1.3.1 起） 起可用。

<a id="materialattributes-成员写入"></a>

## `MaterialAttributes` 成员写入

形式 6 写入 `MaterialAttributes` 值的一个属性。这是一个类似结构体的 Graph 值类型 （自 1.2.5 起），
它把整个材质的属性集合当作一条连线携带。它自成一种类型，不是标量也不是向量；参与算术和作为构造器参数都是
`DSH4226`，只能从另一个 `MaterialAttributes` 值赋值，其他都是 `DSH4228`。

| 写法 | 效果 |
| :-- | :-- |
| `MaterialAttributes Attrs;` | 空集：一个所有输入都未连接的 `MakeMaterialAttributes`。与 `Texture2D`、`Substrate` 不同，它**不**需要初始值 |
| `MaterialAttributes Attrs = {};` | 与上一行完全相同 |
| `MaterialAttributes B = A;` | `B` 取 `A` 的值，不产生节点。这是值拷贝，不是别名：之后的 `B.Roughness = r;` 只改 `B` |
| `MaterialAttributes Attrs = F_Layer(uv);` | 任何值为 `MaterialAttributes` 的表达式——函数调用、`StaticSwitchParameter` 调用 |

**警告：**

**在 `Shader` 里，`Outputs` 声明会创建变量** （自 2.0.0 起）。每个 `Outputs` 声明都在材质函数体开头
声明，`MaterialAttributes` 输出从空集开始，所以不在 `Graph` 里声明也能直接写 `Attrs.BaseColor = Tint;`。
`Graph` 里的 `MaterialAttributes Attrs;` 指的是同一个变量，不增加任何东西。在函数类块里，
`MaterialAttributes` 输出就是函数的输出：写给它的属性组成它的集合，读一个从没写过的属性是 `DSH4370`。

```c
Outputs = { MaterialAttributes Attrs; Base.MaterialAttributes = Attrs; }
Graph   = { Attrs.BaseColor = Tint; Attrs.Roughness = 0.35; }
```

<a id="成员列表"></a>

### 成员列表

属性表来自引擎 （自 2.0.0 起）：本引擎没有的属性是 `DSH5200`。成员名先精确匹配（名字或别名），再不区分
大小写，后者带警告 `DSH5276`。别名是完全等价的同义词。

| 成员 | 别名 | 分量数 |
| :-- | :-- | :-- |
| `BaseColor` | —— | 3 |
| `Metallic` | —— | 1 |
| `Specular` | —— | 1 |
| `Roughness` | —— | 1 |
| `Anisotropy` | —— | 1 |
| `EmissiveColor` | `Emissive` | 3 |
| `Opacity` | —— | 1 |
| `OpacityMask` | —— | 1 |
| `Normal` | —— | 3 |
| `Tangent` | —— | 3 |
| `WorldPositionOffset` | `WPO` | 3 |
| `SubsurfaceColor` | —— | 3 |
| `CustomData0` | `ClearCoat` | 1 |
| `CustomData1` | `ClearCoatRoughness` | 1 |
| `AmbientOcclusion` | `AO` | 1 |
| `Refraction` | —— | 3 |
| `CustomizedUV0` … `CustomizedUV7` | —— | 2 |
| `PixelDepthOffset` | `PDO` | 1 |
| `Displacement` | —— | 1 |
| `DiffuseColor` | —— | 3 |
| `SpecularColor` | —— | 3 |
| `SurfaceThickness` | —— | 1 |
| `FrontMaterial` （自 UE 5.4 起） | —— | 一个 `Substrate` 值 （自 2.0.0 起） |
| `MaterialAttributes` | `Attributes` | 整个集合 |

1.x 的别名 `CustomizedUVs0` … `CustomizedUVs7` 不在别名表里 （自 2.0.0 起），请写 `CustomizedUV0` …
`CustomizedUV7`。`MaterialAttributes`（别名 `Attributes`）可以作为成员解析 （自 2.0.0 起）：它就是整个
集合，也是绑定 `Base.MaterialAttributes = Attrs;` 写的东西。要替换一个 Graph 变量的整个集合，直接给变量
赋值：`Attrs = Other;`。

<a id="读取成员"></a>

### 读取成员

读取本值里块体已经写过的属性，得到的就是写进去的值，不产生节点。其他读取来自每个值一个、被该值所有读取
共享的 `BreakMaterialAttributes` （自 2.0.0 起）（1.x 每次读取一个 Break 节点）。必须经过 Break 节点的
读取，要求引擎的 Break 节点发布了该属性，否则是 `DSH8216`——这取决于引擎构建。没有任何东西可读的值——函数
的 `MaterialAttributes` 输出在写入之前——是 `DSH4370`。

读取结果是一个带成员分量数的普通数值，所以可以再 swizzle：`Attrs.BaseColor.r` 是属性读取后跟通道选择。

<a id="写入成员"></a>

### 写入成员

成员写入记录该属性的值，之后对同一属性的写入会替换它。节点在值被**使用**的地方才产生——传给函数或引脚、
赋给输出、绑定到材质：在该值的集合之上一个 `SetMaterialAttributes`，每个写过的属性一个引脚
（自 2.0.0 起）（1.x 每次写入链接一个 Set 节点）。底下没有集合的值——函数的输出——改为一个带写入引脚的
`MakeMaterialAttributes`。

| 规则 | 失败时的码 |
| :-- | :-- |
| 目标是 `name.member`；成员的成员不是目标 | `DSH2206` |
| 基名必须已声明 | `DSH4200` |
| 基变量必须持有 `MaterialAttributes` 值 | 数值上的成员按 swizzle 读，`DSH4230`；纹理上 `DSH4206`；`Substrate` 值上 `DSH4207` |
| 成员名必须能解析 | `DSH5200` |
| 值必须放得进成员的类型 | `DSH4228`；更宽的向量取前导分量并给出提示 `DSH5289` |

**提示：**

`Attrs.BaseColor.r = 1.0;` 是 `DSH2206`。1.x Graph 里没有按通道写属性的写法——请先构建出完整的值。
`Attrs.BaseColor = {1.0, 0.35, 0.1};` 是 `DSH2162` （自 2.0.0 起）：请写
`Attrs.BaseColor = float3(1.0, 0.35, 0.1);`。

<a id="传递属性值"></a>

### 传递属性值

| 场合 | 行为 |
| :-- | :-- |
| `Base.MaterialAttributes = Attrs;` | 把值绑定到材质，并打开 *Use Material Attributes* |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` 的输入输出 | 接受。参数本身必须已经是 `MaterialAttributes` 值，不做转换（输入是 `DSH4226`，其他类型的输出变量是 `DSH4218`） |
| `GraphFunction` 结果类型 | 接受 |
| `if` / `else` | 逐属性合并：两个分支不一致的每个属性一个条件节点。两个分支各赋一个不同的**整个**集合是 `DSH4375`——见 [if / else](if.md#哪些值不能被选择) |
| `StaticSwitchParameter` 调用分支 | 两个分支都是属性值时接受 |

`Substrate` 是另一种值类型，不是属性集，需要 UE 5.4+；两者永远不会互相转换（`DSH4228`）。`Attrs.FrontMaterial`
在 UE 5.4+ 上能解析，类型是 **`Substrate`** （自 2.0.0 起）（1.x 把它当作一个数）。同一个 `Shader` 里同时
绑定 `Base.MaterialAttributes` 和 `Base.FrontMaterial` 不再被拒绝 （自 2.0.0 起）：两者都会接上，而前者
打开的 *Use Material Attributes* 让材质读属性集而不是各个引脚。见 [Substrate](../builtins/substrate.md)。

<a id="作用域"></a>

## 作用域

每个 `if` 和 `else` 块体都是有自己作用域的块 （自 2.0.0 起）；1.x 整个块体只有一张值表。

| 情况 | 结果 |
| :-- | :-- |
| 在块体层声明 | 对块体剩余部分可见。在 `Shader` 里这个块体还包含排在 `Graph` 语句之前的 `Outputs` 声明 |
| 在 `if` 之**前**声明、在分支里赋值 | 两个值合并成一个条件节点，`if` 之后变量持有合并后的值 |
| 在分支**内部**声明 | 分支结束即丢弃；不参与合并，`if` 之后再读是 `DSH4200` |
| 分支声明了外层块体已声明的名字 | `DSH4220` |
| 在分支里读取属性 | 属性是文件作用域的输入，永远不是分支值 |

**警告：**

1.x 会合并两个分支里都声明了的名字，并允许在 `if` 之后读取它。现在这样的名字只属于各自的分支。请在 `if`
**之前**声明变量，在每个分支里赋值：

```c
float3 Blend = float3(0.0, 0.0, 0.0);
if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
else            { Blend = float3(0.25, 0.25, 0.25); }
Color = Blend;
```

分支局部的名字可以在 `if` 之后再次声明：它的作用域已经结束了。完整合并规则见
[if / else](if.md#合并)。

<a id="合成语句"></a>

## 合成语句

`Shader` 的每个 `Outputs` 声明都会成为材质函数体开头声明的变量，排在第一条 `Graph` 语句之前
（自 2.0.0 起）。有初始值就用它，没有就用该类型的零值（`MaterialAttributes` 为空集）；没有初始值的纹理或
`Substrate` 输出起初没有值。这些声明带着它们来源的那一行 `Outputs` 的位置。

在 `Graph` 顶层用相同大小写再声明一个输出的名字，不会声明第二个变量：带初始值时变成对输出的赋值，不带时
直接丢弃。

| 码 | 何时 |
| :-- | :-- |
| `DSH3270` | `Outputs` 声明有 `=` 但后面什么都没有 |
| `DSH3266` | `Outputs` 声明后面既不是 `;` 也不是 section 的闭合 `}` |

`Outputs` section 声明或绑定了东西的 `Shader` 不需要 `Graph` section；两者都没有是 `DSH2255`。见
[输出绑定](../generation/outputs.md)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2154: Expected ';' after a variable declaration, found {1}.` | 语句没有以 ; 结束——包括块体最后一条语句、大小写写错的 If，以及名字不是单个标识符的声明。报告的位置是下一行的第一个记号。 | 补上 ;；错误在报告位置的上一行。 |
| `DSH2151: Expected an expression, found {0}.` | 该出现值的地方出现了不能开始一个值的记号，例如 = 5;。 | 补上操作数，或删掉多余的记号。 |
| `DSH2163: Expected a variable name, found {1}.` | 逗号声明里 , 之后的声明子不是名字。 | 只写一个类型 token，后面跟名字：float a = 1, b, c; |
| `DSH2162: An initializer list is only allowed as a variable initializer, not as a general expression.` | 花括号列表出现在声明初始值以外的地方——赋值或成员写入的右侧、调用参数（形式 5 和 7）。 | 改写成构造器：float3(r, g, b)。 [详解](expressions.md#构造器) |
| `DSH2214: Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'.` | 花括号初始值里嵌套了花括号列表。 | 写构造器：float4(float2(1, 2), float2(3, 4))。 |
| `DSH2206: Expected a variable or 'variable.member' on the left of a 1.x Graph assignment, found '{0}'.` | = 左侧既不是名字也不是 name.member——例如 Attrs.BaseColor.r = 1.0;。 | 给普通变量赋值，在右侧用构造器拼出完整的值。 |
| `DSH2212: Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file.` | Graph 变量上写了 static、const 或 uniform。 | 去掉关键字，或把值声明到 Properties 里。 |
| `DSH2213: Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file.` | 数组声明子，例如 float w\[4\];。 | 改用多个变量，或把代码移到 Function 或 .dss。 |
| `DSH2215: Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type.` | 纹理、SamplerState 或 Substrate 变量没有初始值。 | 给变量一个初始值。 |
| `DSH4201: '{0}' is not a type; did you mean '{1}'? Type names are case-sensitive.` | 声明的类型 token 无法解析。 |  [详解](../language/types.md) |
| `DSH4220: '{0}' is already declared in this block.` | 同一块里重复声明；分支声明了外层块体已声明的名字；或在 Shader 里声明 Base。 | 去掉类型 token 改成赋值，或换一个名字。 |
| `DSH4228: {0} expects {1}, and this is {2}.` | 赋值或初始值放不进目标类型——例如把 float2 加宽成 float3，或把纹理放进数。 | 用 swizzle 或构造器调整宽度：float3(v, 0.0)。 [详解](expressions.md#转换) |
| `DSH4229: A 'uniform' is an input and a 'static const' is a constant; neither can be assigned to. Copy it into a local first.` | 赋值目标是一个属性。 | 先把属性复制到 Graph 变量里。 |
| `DSH5289: {0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.` | 提示（legacy 规则 L22）：更宽的值被截成目标的前导分量。 | 不必改；显式写出 swizzle（Tint.rgb）可以消除提示。 |
| `DSH5292: '{0}' is not declared, and as in 1.x this assignment declares it, as a local of type {1}.` | 提示（legacy 规则 L26）：赋值声明了一个任何地方都没声明过的名字。 | 不必改；dsc migrate 会写出声明。 |
| `DSH5275: '{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告（legacy 规则 L19）：名字只在忽略大小写时匹配到某个声明。 | 按声明的拼写写名字。 |
| `DSH4200: '{0}' is not declared; did you mean '{1}'? Names are case-sensitive.` | 名字匹配不到任何声明，或忽略大小写时匹配到多个；也包括在 if 之后读取分支局部变量。 |  |
| `DSH5200: A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.` | MaterialAttributes 成员名不是本引擎的属性。 | 使用引擎拼写；1.x 的 CustomizedUVsN 请改写成 CustomizedUVN。 |
| `DSH4370: '{0}' is read before anything wrote it; a material attribute has no value until this function assigns one.` | 从一个还没有任何可读内容的值读取成员——例如函数输出在写入之前。 | 先写属性再读。 |
| `DSH8216: BreakMaterialAttributes does not publish the attribute '{0}', so it cannot be read from a material that came through a pin.` | 读取要经过 Break 节点，而本引擎的 Break 节点没有发布该属性（如 FrontMaterial、SurfaceThickness）。 | 在值还在构建时读取，或把值和材质一起传过去。 |
| `DSH2255: Expected a Graph section in the Shader '{0}', found none.` | Shader 既没有 Graph，Outputs 也没有声明或绑定任何东西。 | 加上 Graph = { ... }。 |

跨阶段的完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

一个块体里写全所有被接受的语句形式：

```c
ShaderFunction(Name="Functions/F_AllForms")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); float K = 2.0; }
    Inputs     = { vec2 UV; }
    Outputs    = { MaterialAttributes Attrs; vec3 Debug; }

    Graph = {
        MaterialAttributes Attrs;              //    指的就是输出，不会声明第二个变量
        float3 c;                              // 1  声明，无初始值
        float3 scaled = Tint * K;              // 2  声明 + 表达式
        vec4   packed = {scaled, 1.0};         // 3  声明 + 花括号初始值
        c = packed.rgb;                        // 4  赋值
        Debug = float3(0.0, 0.0, 0.0);         //    （形式 5 被拒绝：请写构造器）
        Attrs.BaseColor = c;                   // 6  成员写入
        Attrs.Roughness = 0.35;                //    （形式 7 被拒绝：直接写值）
        float a = 1, b, d = 3;                 // 8  逗号分隔声明

        if (K > 1.0) {                         // 10 if / else
            Debug = c * a;
        } else {
            Debug = c * b;
        }
    }
}
```

这里唯一缺席的是形式 9，语句形式调用——它需要一个被调方，所以有自己独立的一页。

<a id="参见"></a>

## 参见

- [运算符与转换](expressions.md) —— `=` 右侧可以写什么，以及赋值处的转换
- [if / else](if.md) —— 形式 10 的完整规则与合并
- [调用](calls.md) —— 形式 9、out 目标与参数规则
- [不支持的写法](unsupported.md) —— `return`、`for`、`+=` 及它们的码
- [输出绑定](../generation/outputs.md) —— `Outputs` 声明与初始值
