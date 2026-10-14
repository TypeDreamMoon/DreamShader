---
title: 数学内置
description: 无前缀、按 HLSL 拼写的调用名 —— 44 个 HLSL 名字直接降级为算术材质节点，外加 1.x 源码还能用的四个 GLSL 拼写。
---

无前缀、按 HLSL 拼写的调用名会被直接降级为算术 `UMaterialExpression` 节点。（自 2.0.0 起） 起共有
**44 个 HLSL 名字**（1.x 只有 29 个拼写），外加 1.x 源码仍可使用的四个 GLSL 拼写。大多数调用生成一个节点；
`fwidth`、`rcp`、`rsqrt`、`reflect`、`refract` 背后没有引擎节点，会展开成一小张子图；`round` 生成一个
Custom 节点 （自 2.1.2 起）。

```c
float s     = sin(X);
float sa    = saturate(X);
vec3  mixed = lerp(A, B, sa);
vec3  unit  = normalize(mixed);
float d     = dot(unit, A);
```

它们是裸调用 —— 写 `saturate(x)`，不是 `UE.saturate(x)`。可以写在 `Graph` 体内、`Outputs` 的绑定表达式中，
或 `Outputs` 声明的初始化式中；`.dss` 里凡是能写表达式的地方都能用同样的名字。

名字**精确匹配** （自 2.0.0 起）。1.x 源码仍享有 1.x 的宽松规则，但每一条都会用诊断说明 —— 见
[名称解析](#名称解析)。参数可以按位置给，也可以按节点 pin 名命名 —— 见[命名参数](#命名参数)。

**警告：**

**函数不能和内置同名** （自 2.0.0 起）。名为 `lerp`、`clamp`、`dot`、`min`、`max`、`pow`、`abs`，
或本页任何 HLSL 名字、GLSL 拼写的 `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction`、
`.dss` 函数，在声明处就报 `DSH6206`。1.9.x 及以前这样的函数能编译，却在 `Graph` 块里静默地调不到 ——
内置节点胜出。

<a id="语法概要"></a>

## 语法概要

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
{ abs | acos | asin | atan | ceil | cos | ddx | ddy | exp | exp2 | floor | frac | fwidth | length
| log | log2 | log10 | normalize | rcp | round | rsqrt | saturate | sign | sin | sqrt | tan | trunc } ( <x> )
{ atan2 | cross | distance | dot | fmod | max | min | pow | reflect | step } ( <x> , <y> )
{ clamp | lerp | refract | smoothstep } ( <x> , <y> , <z> )
```

每个 `<x>` / `<y>` / `<z>` 都是任意 [Graph 表达式](../graph/expressions.md)。`sinh`、`cosh`、`tanh` 也是名字，
但会被拒绝 —— 见[这里没有的东西](#这里没有的东西)。

<a id="目录"></a>

## 目录

每个 HLSL 名字一行。*结果*是编译器给调用的类型；*最宽操作数*指所有参数里最宽的那个，标量会广播到它。

| 写法 | 参数个数 | 降级为 | 按序连接的输入 pin | 结果 |
| :-- | :-- | :-- | :-- | :-- |
| `abs` | 1 | `UMaterialExpressionAbs` | `Input` | 参数的类型 |
| `acos` （自 1.6.0 起） | 1 | `UMaterialExpressionArccosine` | `Input` | 参数的类型 |
| `asin` （自 1.6.0 起） | 1 | `UMaterialExpressionArcsine` | `Input` | 参数的类型 |
| `atan` （自 1.6.0 起） | 1 | `UMaterialExpressionArctangent` | `Input` | 参数的类型 |
| `atan2` （自 1.6.0 起） | 2 | `UMaterialExpressionArctangent2` | `Y`、`X` | 最宽操作数 |
| `ceil` | 1 | `UMaterialExpressionCeil` | `Input` | 参数的类型 |
| `clamp` | 3 | `UMaterialExpressionClamp` | `Input`、`Min`、`Max` | 最宽操作数 |
| `cos` | 1 | `UMaterialExpressionCosine` | `Input` | 参数的类型 |
| `cross` （自 1.6.0 起） | 2 | `UMaterialExpressionCrossProduct` | `A`、`B` | **恒为 `float3`** |
| `ddx` （自 2.0.0 起） | 1 | `UMaterialExpressionDDX` | `Value` | 参数的类型 |
| `ddy` （自 2.0.0 起） | 1 | `UMaterialExpressionDDY` | `Value` | 参数的类型 |
| `distance` （自 2.0.0 起） | 2 | `UMaterialExpressionDistance` | `A`、`B` | **恒为 `float`** |
| `dot` | 2 | `UMaterialExpressionDotProduct` | `A`、`B` | **恒为 `float`** |
| `exp` （自 2.0.0 起） | 1 | `UMaterialExpressionExponential` | `Input` | 参数的类型 |
| `exp2` （自 2.0.0 起） | 1 | `UMaterialExpressionExponential2` | `Input` | 参数的类型 |
| `floor` | 1 | `UMaterialExpressionFloor` | `Input` | 参数的类型 |
| `fmod` （自 1.5.0 起） | 2 | `UMaterialExpressionFmod` | `A` ← 被除数，`B` ← 除数 | 最宽操作数 |
| `frac` | 1 | `UMaterialExpressionFrac` | `Input` | 参数的类型 |
| `fwidth` （自 2.0.0 起） | 1 | **子图**：`abs(ddx(x)) + abs(ddy(x))` | —— | 参数的类型 |
| `length` （自 1.6.0 起） | 1 | `UMaterialExpressionLength` | `Input` | **恒为 `float`** |
| `lerp` | 3 | `UMaterialExpressionLinearInterpolate` | `A`、`B`、`Alpha` | 最宽操作数 |
| `log` （自 2.0.0 起） | 1 | `UMaterialExpressionLogarithm` | `Input` | 参数的类型 |
| `log2` （自 2.0.0 起） | 1 | `UMaterialExpressionLogarithm2` | `X` | 参数的类型 |
| `log10` （自 2.0.0 起） | 1 | `UMaterialExpressionLogarithm10` | `X` | 参数的类型 |
| `max` | 2 | `UMaterialExpressionMax` | `A`、`B` | 最宽操作数 |
| `min` | 2 | `UMaterialExpressionMin` | `A`、`B` | 最宽操作数 |
| `normalize` | 1 | `UMaterialExpressionNormalize` | **`VectorInput`** | 参数的类型 |
| `pow` | 2 | `UMaterialExpressionPower` | `Base`、`Exponent` | 最宽操作数 |
| `rcp` （自 2.0.0 起） | 1 | **子图**：`ConstA = 1` 的 `Divide` | —— | 参数的类型 |
| `reflect` （自 1.6.0 起） | 2 | **4 节点子图** —— 见[下文](#reflectrefract) | —— | **恒为 `float3`** |
| `refract` （自 1.6.0 起） | 3 | **14 节点子图** —— 见[下文](#reflectrefract) | —— | **恒为 `float3`** |
| `round` （自 2.0.0 起） | 1 | 生成的 `UMaterialExpressionCustom`，HLSL `round` （自 2.1.2 起） | `Input` | 参数的类型 |
| `rsqrt` （自 2.0.0 起） | 1 | **子图**：`SquareRoot`，再接 `ConstA = 1` 的 `Divide` | —— | 参数的类型 |
| `saturate` | 1 | `UMaterialExpressionSaturate` | `Input` | 参数的类型 |
| `sign` （自 2.0.0 起） | 1 | `UMaterialExpressionSign` | `Input` | 参数的类型 |
| `sin` | 1 | `UMaterialExpressionSine` | `Input` | 参数的类型 |
| `smoothstep` （自 1.6.0 起） | 3 | `UMaterialExpressionSmoothStep` | `Min`、`Max`、`Value` | 最宽操作数 |
| `sqrt` | 1 | `UMaterialExpressionSquareRoot` | `Input` | 参数的类型 |
| `step` （自 1.6.0 起） | 2 | `UMaterialExpressionStep` | `Y` ← 参数 1（edge），`X` ← 参数 2（value） | 最宽操作数 |
| `tan` （自 2.0.0 起） | 1 | `UMaterialExpressionTangent` | `Input` | 参数的类型 |
| `trunc` （自 2.0.0 起） | 1 | `UMaterialExpressionTruncate` | `Input` | 参数的类型 |

<a id="glsl-拼写"></a>

### GLSL 拼写

1.x 的四个 GLSL 拼写在 1.x 源码里仍按对应的 HLSL 名字读取，并给出警告 `DSH5277`：`mix` → `lerp`、
`fract` → `frac`、`mod` → `fmod`、`inversesqrt` → `rsqrt`。在 `.dss` 里它们是错误 `DSH4250`。
[`dsc migrate`](../tools/migrate.md) 会把它们改写成 HLSL 名字。（自 2.0.0 起） 之前
这几对是不带诊断的等价别名。

在 [`Function`](../language/functions.md) 的 HLSL 体内部，标识符 `mod` 会连同其他 GLSL 名字一起被静默改写成
`fmod`（忽略大小写）。

<a id="参数规则"></a>

## 参数规则

下列规则对上面每个内置节点都一样，1.x 源码和 `.dss` 也一样。

| # | 规则 | 违反时 |
| :-- | :-- | :-- |
| 1 | 参数个数精确 —— 没有默认值、没有可选参数、没有变参 | `DSH4224` |
| 2 | 参数按位置给，或用节点某个输入 pin 的名字命名；位置参数填第一个还没填的操作数。`.dss` 里命名参数之后再出现位置参数是 `DSH2158` | 未知名字 `DSH4216`；同一 pin 给两次 `DSH4215`；给出的参数之间漏了一个 `DSH4217` |
| 3 | 每个参数都作为完整 `Graph` 表达式求值，允许嵌套内置调用 | 内层表达式在自己的位置报自己的错 |
| 4 | 每个参数都是数值（或 bool，读作 0 / 1）—— 不能是纹理对象、`MaterialAttributes` 值或 `Substrate` 值 | `DSH4226` |
| 5 | **检查**分量数 （自 2.0.0 起）：最宽操作数决定宽度，标量广播到它，更窄的向量不会被加宽 | `DSH4226` |
| 6 | `cross`、`reflect`、`refract` 取 `float3` 操作数 | 更窄的向量是 `DSH4226`；更宽的向量在 1.x 源码里被截成三个分量并给出提示 `DSH5289`（1.x 就这么做），在 `.dss` 里是 `DSH4226` |
| 7 | `/` 的两个操作数都是整数构造器调用时拒绝 —— 材质图没有整数除法；其他 `/` 都是浮点除法，`7 / 2` 也一样 | `DSH4243` |

1.9.x 及以前，规则 4 – 6 都不是编译期检查：`dot(vec3Value, vec2Value)` 一声不吭地通过，之后在 Unreal
自己的材质翻译阶段失败。

<a id="名称解析"></a>

## 名称解析

裸名调用按以下顺序解析。完整的查找顺序见[调用](../graph/calls.md#分发顺序)。

| # | 候选 | 参考 |
| :-- | :-- | :-- |
| 1 | 1.x `Graph` 中由 legacy 前端在读入调用时改写的写法：`SampleTexture2D`、已声明 property 的 pin 调用形式、`UE.SceneTexture` 和其他 1.x 调用形状 | [`UE.*` 节点](ue.md) · [Properties 类型](../language/properties.md#在-graph-中使用参数) |
| 2 | 类型名 —— `float3(…)`、`vec4(…)` 这样的构造器 | [构造器](../graph/expressions.md#构造器) |
| 3 | 以 `UE.`、`Substrate.` 开头的被调用者 | [`UE.*` 节点](ue.md) · [Substrate](substrate.md) |
| 4 | `Texture2DSample`、`Texture2DSampleLevel` | |
| 5 | **数学内置 —— 本页**，按精确的 HLSL 名字；然后是 GLSL 拼写（1.x 中 `DSH5277`，`.dss` 中 `DSH4250`） | —— |
| 6 | 本文件声明或导入的函数 —— `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction`、`.dss` 函数 | [调用](../graph/calls.md) |
| 7 | 仅 1.x 源码：忽略大小写才匹配的内置或函数，并给出警告 `DSH5275` | —— |

写错的内置名 —— `saturte(x)` —— 报 `DSH4208`；若有只差大小写的已声明名字，消息会附带 *did you mean*。

<a id="命名参数"></a>

## 命名参数

（自 2.0.0 起） 命名参数指定节点的某个输入 pin —— 也就是目录里*按序连接的输入 pin* 一列 —— 并填入那个
操作数，不论它写在参数列表的哪个位置：

```c
float t = lerp(A, B, Alpha = Mask);     // A、B 按位置；Alpha 按名字
float c = clamp(X, Max = 1.0, Min = 0.0);
```

pin 名是引擎的，不一定是 HLSL 的：`pow` 是 `Base` / `Exponent`，`step` 是 `Y`（edge）/ `X`（value），
`normalize` 是 `VectorInput`。节点没有的名字报 `DSH4216`，消息会列出可用的 pin。没有节点的内置
（`fwidth`、`rcp`、`rsqrt`、`reflect`、`refract`）只接受位置参数。

**提示：**

1.9.x 及以前，给数学内置传命名参数报的是**参数个数**错误 —— `saturate(Input = X)` 会以「expects exactly
1 argument」失败。现在它会选中那个 pin。

<a id="逐个内置节点的说明"></a>

## 逐个内置节点的说明

<a id="clamp"></a>

### clamp

`clamp(Input, Min, Max)` 会连接全部三个参数，并把节点的 `ClampMode` 保持在默认值 `CMODE_Clamp`。
要用 `CMODE_ClampMin` 或 `CMODE_ClampMax`，直接调用节点本身：

```c
UE.Clamp(Input = x, Min = a, ClampMode = CMODE_ClampMin)
```

见[通用 UE.Expression](ue-expression.md)。

<a id="dotlengthdistancecross"></a>

### dot、length、distance、cross

结果宽度固定的内置：`dot`、`length`、`distance` 恒为 1 个分量，`cross` 恒为 3 个，与参数无关 ——
所以 `float d = dot(A, B);` 不需要 swizzle。

<a id="fmod"></a>

### fmod

参数 1 是被除数，参数 2 是除数，分别连到节点的 `A` 和 `B` pin。`mod` 是 GLSL 拼写，见
[GLSL 拼写](#glsl-拼写)。

<a id="lerp"></a>

### lerp

结果宽度是三个参数里最宽的那个 —— 包括 `Alpha`：用标量 `Alpha` 混合两个 `float3` 得到 `float3`，`Alpha`
被广播。两个 Substrate 值上的 `lerp(A, B, t)` 则是 `Substrate.HorizontalMixing` —— 见
[Substrate 语法糖](../language-v2/substrate.md#运算符与-lerp)。

<a id="minmax"></a>

### min、max

这两个名字共用一套类型规则，区别只在选择的节点类。

<a id="normalize"></a>

### normalize

唯一一个单输入 pin 不叫 `Input` 的内置：`UMaterialExpressionNormalize` 把它命名为 `VectorInput`。按位置调用
（`normalize(N)`）时看不出差别，命名参数则必须用这个名字。

<a id="sincos"></a>

### sin、cos

两者都把节点的 `Period` 属性保持在默认值。要用非默认周期，直接调用节点：`UE.Sine(Input = x, Period = 2.0)`。

默认周期是 1，所以这两个图内置按**周期**计：`sin(0.25)` 是 1，`cos(0.25)` 是 0（在浮点精度内）。
常量表达式也按同样的周期折叠 （自 2.1.2 起） —— 此前常量折叠按弧度计算，于是 `sin(0.25)` 和
`T = 0.25` 时的 `sin(T)` 结果不同。`@custom` HLSL 体里的 `sin` / `cos` 照常按弧度。

<a id="round"></a>

### round

（自 2.1.2 起）

图内置和常量折叠都按 HLSL 的「舍入到偶数」规则处理恰好一半的值：`round(0.5)` 是 0，`round(2.5)` 是 2，
`round(-1.5)` 是 -2。

内置降级成一个生成的 Custom 节点，对 uniform 和变化的输入都用 HLSL `round` 求值。这样绕开了原生
`UE.Round(Input = x)` 节点的 preshader：在 UE 5.8 上它会把 uniform 的半值向上舍入（`0.5` 变成 1），于是同一个值
因来源不同而舍入结果不同。需要原生行为时用反射写法 `UE.Round(Input = x)`，它保留原生节点。反编译会把未改动的
生成节点还原成 `round`；原生 Round 节点仍是 `UE.Round`。`dump-ir` 显示语言层的运算，`dump-graph` 显示生成的
Custom 节点。

**警告：**

代价：引擎从不在 shader 之前求值 Custom 节点，所以对 uniform 值（比如参数）取 `round` 会**逐像素**执行（一条
指令），其下游的数学也随之逐像素，而不再折叠进材质 uniform 每次 draw 算一次。常量参数仍在编译期折叠。比起半值
的处理，这更要紧时，用 `UE.Round(Input = x)` 保留原生节点及其 preshader。

<a id="step"></a>

### step

`step(edge, x)` 返回 `x >= edge ? 1 : 0`，和 HLSL 一致。`UMaterialExpressionStep` 的 pin 命名是反过来的 ——
`Y` 是 edge、`X` 是 value —— 所以参数 1 连到 `Y`、参数 2 连到 `X`。

<a id="smoothstep"></a>

### smoothstep

`smoothstep(min, max, x)`，参数顺序同 HLSL，直接连到节点的 `Min`、`Max` 和 `Value` pin。

<a id="asinacosatanatan2"></a>

### asin、acos、atan、atan2

四个反三角节点。`atan2(y, x)` 的参数顺序同 HLSL，而节点的 pin 本来就叫 `Y` 和 `X`，所以映射是直接的。

**提示：**

引擎还带了 `ArcsineFast`、`ArccosineFast`、`ArctangentFast` 和 `Arctangent2Fast` —— 在有限输入范围上
成立的更便宜的近似。它们没有内置写法，直接调用节点：`UE.ArcsineFast(Input = x)`。

<a id="fwidthrcprsqrt"></a>

### fwidth、rcp、rsqrt

（自 2.0.0 起）

三者都没有对应的引擎节点。`fwidth(x)` 按 HLSL 内建函数的定义建成 `abs(ddx(x)) + abs(ddy(x))`；`rcp(x)` 是
`ConstA = 1` 的 `Divide`；`rsqrt(x)` 是 `SquareRoot` 接一个这样的 `Divide`。

<a id="reflectrefract"></a>

### reflect、refract

Unreal 没有 `Reflect` 或 `Refract` 表达式，所以两者都降级成 HLSL 对它们的定义，返回给调用方的是那张子图的
最后一个节点。

`reflect(i, n)` 展开成 `i - 2 * dot(i, n) * n` —— 四个节点（`DotProduct`、两个 `Multiply`、`Subtract`）。
字面量 `2` 骑在 `Multiply` 的 `ConstB` 上，不额外花一个 `Constant` 节点。

`refract(i, n, eta)` 展开成完整的 HLSL 定义 —— 十四个节点：

```text
k = 1 - eta*eta * (1 - dot(n, i)^2)
k < 0 ? 0 : eta*i - (eta*dot(n, i) + sqrt(k)) * n
```

全内反射的判断是一个 `If` 节点，所以两侧都会被翻译、再选其一；对负 `k` 取 `sqrt` 只落在被丢弃的那一侧，
和 HLSL 完全一样。`k == 0` 仍然满足公式（`sqrt(0) == 0`）并走折射侧。零分支写成 `i * 0` 而不是常量，
是为了让它的宽度永远等于 `i` 的宽度 —— `If` 要求两个分支一致。

**提示：**

两者都是**按构造就很贵**的。周围代码本来就是 HLSL 时，写在 [`Function`](../language/functions.md) 体里，
一个节点就够了。

<a id="节点复用"></a>

## 节点复用

两个相同操作数上的相同调用 —— 写了两次的 `sin(X)` —— 只产生一个节点：IR 在发射任何东西之前做结构去重。
节点位置和其他节点一样来自图布局，而不是固定坐标。参见[节点复用](../graph/expressions.md#节点复用)。

<a id="这里没有的东西"></a>

## 这里没有的东西

- **`sinh`、`cosh`、`tanh` 被拒绝**，报 `DSH4246`：材质图没有双曲节点。写在 [`Function`](../language/functions.md)
  体（或 `.dss` 的 `/// @custom` 函数）里，那里着色器编译器有它们。
- **这个调用面上没有矩阵。** Unreal 材质图根本没有矩阵值类型 —— 没有 `float3x3` 值，也没有 `mul(M, v)`；
  常量折叠之后仍然存在的矩阵报 `DSH4361`。空间转换用
  [`UE.TransformVector` / `UE.TransformPosition`](ue.md#变换内置)；真正的矩阵运算写在
  `Function` 的 HLSL 体里，那里 `float4x4` 和 `mul` 就是 HLSL。
- 在 `Function` 的 HLSL 体内部，这些名字*根本不*由编译器处理 —— 函数体原样输出，适用的是 HLSL 自己的内建函数。

**提示：**

[反编译器](../tools/decompiler.md)会写回这些写法。它的 2.0 写出器（`.dss`，默认）对目录中类的节点，只要
输入 pin 全部连上、其他属性都是默认值，就写成内置 —— `Period` 非默认的 `Sine` 仍是 `UE.Sine(...)` ——
并且在没有别的东西读入时，能认出 `fwidth`、`rcp`、`rsqrt`、`reflect`、`refract` 降级成的子图。旧写出器
（`-Format Legacy`、*Export Legacy* 菜单项）保留 1.x 的映射表。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH4224: '{0}' takes {1} arguments and {2} were given.` | 参数个数不对。 | 按目录里的参数个数写。 |
| `DSH4216: '{0}' takes its arguments in order and has no argument called '{1}'.` | 命名参数不是节点的任何输入 pin。消息会列出可用的 pin 名。 | 改正名字，或改用位置参数。 |
| `DSH4215: The '{0}' argument of '{1}' is given twice.` | 同一个 pin 被填了两次 —— 命名参数重复了位置参数已经填过的操作数。 | 删掉重复的那个。 |
| `DSH4217: '{0}' is missing its '{1}' argument.` | 给出的参数之间漏了一个操作数。 |  |
| `DSH2158: A positional argument cannot follow a named argument; give this argument a name too.` | 仅 .dss：命名参数之后又出现位置参数。 | 把剩下的参数也写上名字，或把命名参数挪到最后。 |
| `DSH4226: {0} expects {1}, and this is {2}.` | 参数不是数值（纹理、material、Substrate 值），或宽度与调用不符 —— 更窄的向量不会被加宽。 | 用 swizzle 或构造器把宽度对齐；标量可以广播到任意宽度。 |
| `DSH5289: {0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.` | 提示，仅 1.x：更宽的向量被截成调用所要的宽度。 | 不必改；写出 swizzle 会更清楚，dsc migrate 会替你写。 |
| `DSH4243: Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.` | 整数除法。1.x Graph 里只有两个整数构造器调用相除才算。 |  [详解](../graph/expressions.md#整数除法) |
| `DSH4246: The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it.` | 调用了 sinh、cosh 或 tanh。 | 移到 Function / @custom 函数体里。 |
| `DSH4361: This expression is {0} and the material graph has no matrices; compute it inside the custom body instead.` | 常量折叠之后仍有矩阵值。 |  [详解](../language/functions.md) |
| `DSH5277: '{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'.` | 警告，仅 1.x：mix、fract、mod 或 inversesqrt。 | 写 HLSL 名字。 |
| `DSH4250: '{0}' is the GLSL spelling; this language is HLSL, so write '{1}'.` | .dss 里用了 GLSL 拼写。 | 写 lerp、frac、fmod、rsqrt。 |
| `DSH5275: '{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告，仅 1.x：名字只在忽略大小写时才和内置或函数匹配。 | 按声明的拼写写。 |
| `DSH4208: '{0}' is not a function, a builtin or a struct.` | 名字既不是内置、类型、节点，也不是函数 —— 写错的内置名落到这里。 |  [详解](../graph/calls.md) |
| `DSH6206: '{0}' is a builtin operation and cannot be redeclared; rename the function.` | 函数与数学内置或 GLSL 拼写同名。 | 给函数改名。 |
| `DSH8214: Failed to create a '{0}' node.` | 发射器建不出内置降级成的子图中的某个节点。 |  |

完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_MathBuiltins")
{
    Properties = {
        float X = 0.5;
        vec3  A = vec3(1.0, 0.0, 0.0);
        vec3  B = vec3(0.0, 1.0, 0.0);
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        float s     = sin(X);
        float c     = cos(X);
        float cl    = clamp(X, 0.0, 1.0);
        float sa    = saturate(X);
        vec3  mixed = lerp(A, B, Alpha = sa);
        vec3  unit  = normalize(mixed);
        float d     = dot(unit, A);
        Color = mixed * (s + c + cl) + unit * d;
    }
}
```

生成的节点：

```text
Sine(X)                       -> s
Cosine(X)                     -> c
Clamp(X, 0.0, 1.0)            -> cl
Saturate(X)                   -> sa
LinearInterpolate(A, B, sa)   -> mixed     (float3: the widest operand)
Normalize(mixed)              -> unit      (float3)
DotProduct(unit, A)           -> d         (float, always)
Add / Multiply chain          -> Color
```

<a id="接下来"></a>

## 接下来

- [运算符与转换](../graph/expressions.md) — 运算符、优先级、宽度，以及这些调用遵循的操作数规则。
  - [通用 UE.Expression](ue-expression.md) — 按类名触达任何材质节点，包括 UE.Round、UE.Sine。
  - [UE.* 节点](ue.md) — 1.x 的 27 个 UE.* 名字与变换内置。
  - [函数](../language/functions.md) — HLSL 函数体，那里可以用完整的内建函数集。
