---
title: "数学内置"
description: "数学内置"
---

<a id="math-builtins"></a>

# 数学内置

> [DreamShader](../index.md) » [内置](index.md) » **数学内置**

无前缀、按 HLSL 拼写的调用名，会直接降级为算术 `UMaterialExpression` 节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 位于 `Graph { … }` 体内、`Outputs` 绑定表达式，或 `Outputs` 声明初始化式中；`.dss` 中凡能写表达式的地方都能用同样的名字 |
| 类型 | 内置调用表面 —— 编译器的核心运算 |
| 生成 | 每次调用一个 `UMaterialExpression`，按名字选择 —— 见[目录](#catalogue)。有五个名字 —— `fwidth`、`rcp`、`rsqrt`、`reflect`、`refract` —— 背后没有节点，会生成一张小型的子图 |
| 拼写 | 44 个 HLSL 名字，外加 1.x 源码仍可使用的四个 GLSL 拼写 *（自 2.0.0 起；1.9.x 及以前为 29 个拼写）* |
| 命名空间 | 无 —— 这些是裸调用，`saturate(x)`，而非 `UE.saturate(x)` |

<a id="synopsis"></a>

## 语法概要

```c
{ abs | acos | asin | atan | ceil | cos | ddx | ddy | exp | exp2 | floor | frac | fwidth | length
| log | log2 | log10 | normalize | rcp | round | rsqrt | saturate | sign | sin | sqrt | tan | trunc } ( <x> )
{ atan2 | cross | distance | dot | fmod | max | min | pow | reflect | step } ( <x> , <y> )
{ clamp | lerp | refract | smoothstep } ( <x> , <y> , <z> )
```

`( )` 和 `,` 是 DreamShaderLang 的字面标点；`{ a | b }` 是元表示法，永远不会
被输入。每个 `<x>` / `<y>` / `<z>` 都是任意 [Graph 表达式](../graph/expressions.md)。`sinh`、`cosh`
和 `tanh` 也是名字，但会被拒绝 —— 见[备注](#notes)。

名字按**精确**匹配 *（自 2.0.0 起）*。1.x 源码仍享有 1.x 的宽松规则，每一条都会用诊断说明 ——
见[名称解析](#name-resolution)。参数可以按位置给，也可以按节点 pin 名命名 —— 见[命名参数](#named-arguments)。

<a id="catalogue"></a>

## 目录

每个 HLSL 名字一行。*结果*是编译器给调用的类型；*最宽操作数*指所有参数里最宽的那个，标量会广播到它。

| 写法 | 参数个数 | 降级为 | 按参数顺序连接的输入 pin | 结果 |
| :-- | :-- | :-- | :-- | :-- |
| `abs` | 1 | `UMaterialExpressionAbs` | `Input` | 参数的类型 |
| `acos` *（自 1.6.0 起）* | 1 | `UMaterialExpressionArccosine` | `Input` | 参数的类型 |
| `asin` *（自 1.6.0 起）* | 1 | `UMaterialExpressionArcsine` | `Input` | 参数的类型 |
| `atan` *（自 1.6.0 起）* | 1 | `UMaterialExpressionArctangent` | `Input` | 参数的类型 |
| `atan2` *（自 1.6.0 起）* | 2 | `UMaterialExpressionArctangent2` | `Y`、`X` | 最宽操作数 |
| `ceil` | 1 | `UMaterialExpressionCeil` | `Input` | 参数的类型 |
| `clamp` | 3 | `UMaterialExpressionClamp` | `Input`、`Min`、`Max` | 最宽操作数 |
| `cos` | 1 | `UMaterialExpressionCosine` | `Input` | 参数的类型 |
| `cross` *（自 1.6.0 起）* | 2 | `UMaterialExpressionCrossProduct` | `A`、`B` | **恒为 `float3`** |
| `ddx` *（自 2.0.0 起）* | 1 | `UMaterialExpressionDDX` | `Value` | 参数的类型 |
| `ddy` *（自 2.0.0 起）* | 1 | `UMaterialExpressionDDY` | `Value` | 参数的类型 |
| `distance` *（自 2.0.0 起）* | 2 | `UMaterialExpressionDistance` | `A`、`B` | **恒为 `float`** |
| `dot` | 2 | `UMaterialExpressionDotProduct` | `A`、`B` | **恒为 `float`** |
| `exp` *（自 2.0.0 起）* | 1 | `UMaterialExpressionExponential` | `Input` | 参数的类型 |
| `exp2` *（自 2.0.0 起）* | 1 | `UMaterialExpressionExponential2` | `Input` | 参数的类型 |
| `floor` | 1 | `UMaterialExpressionFloor` | `Input` | 参数的类型 |
| `fmod` *（自 1.5.0 起）* | 2 | `UMaterialExpressionFmod` | `A` ← 被除数，`B` ← 除数 | 最宽操作数 |
| `frac` | 1 | `UMaterialExpressionFrac` | `Input` | 参数的类型 |
| `fwidth` *（自 2.0.0 起）* | 1 | **一张子图**：`abs(ddx(x)) + abs(ddy(x))` | —— | 参数的类型 |
| `length` *（自 1.6.0 起）* | 1 | `UMaterialExpressionLength` | `Input` | **恒为 `float`** |
| `lerp` | 3 | `UMaterialExpressionLinearInterpolate` | `A`、`B`、`Alpha` | 最宽操作数 |
| `log` *（自 2.0.0 起）* | 1 | `UMaterialExpressionLogarithm` | `Input` | 参数的类型 |
| `log2` *（自 2.0.0 起）* | 1 | `UMaterialExpressionLogarithm2` | `X` | 参数的类型 |
| `log10` *（自 2.0.0 起）* | 1 | `UMaterialExpressionLogarithm10` | `X` | 参数的类型 |
| `max` | 2 | `UMaterialExpressionMax` | `A`、`B` | 最宽操作数 |
| `min` | 2 | `UMaterialExpressionMin` | `A`、`B` | 最宽操作数 |
| `normalize` | 1 | `UMaterialExpressionNormalize` | **`VectorInput`** | 参数的类型 |
| `pow` | 2 | `UMaterialExpressionPower` | `Base`、`Exponent` | 最宽操作数 |
| `rcp` *（自 2.0.0 起）* | 1 | **一张子图**：`Divide` 带 `ConstA = 1` | —— | 参数的类型 |
| `reflect` *（自 1.6.0 起）* | 2 | **4 节点子图** —— 见[下文](#reflect-refract) | —— | **恒为 `float3`** |
| `refract` *（自 1.6.0 起）* | 3 | **14 节点子图** —— 见[下文](#reflect-refract) | —— | **恒为 `float3`** |
| `round` *（自 2.0.0 起）* | 1 | 生成的 `UMaterialExpressionCustom`，带 HLSL `round` | `Input` | 参数的类型 |
| `rsqrt` *（自 2.0.0 起）* | 1 | **一张子图**：`SquareRoot`，然后 `Divide` 带 `ConstA = 1` | —— | 参数的类型 |
| `saturate` | 1 | `UMaterialExpressionSaturate` | `Input` | 参数的类型 |
| `sign` *（自 2.0.0 起）* | 1 | `UMaterialExpressionSign` | `Input` | 参数的类型 |
| `sin` | 1 | `UMaterialExpressionSine` | `Input` | 参数的类型 |
| `smoothstep` *（自 1.6.0 起）* | 3 | `UMaterialExpressionSmoothStep` | `Min`、`Max`、`Value` | 最宽操作数 |
| `sqrt` | 1 | `UMaterialExpressionSquareRoot` | `Input` | 参数的类型 |
| `step` *（自 1.6.0 起）* | 2 | `UMaterialExpressionStep` | `Y` ← 参数 1（edge），`X` ← 参数 2（value） | 最宽操作数 |
| `tan` *（自 2.0.0 起）* | 1 | `UMaterialExpressionTangent` | `Input` | 参数的类型 |
| `trunc` *（自 2.0.0 起）* | 1 | `UMaterialExpressionTruncate` | `Input` | 参数的类型 |

GLSL 拼写在 1.x 源码里按 HLSL 名字读取，并给出警告
[`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277)：`mix` → `lerp`、`fract` → `frac`、`mod` → `fmod`，
`inversesqrt` → `rsqrt`。在 `.dss` 里它们是 [`DSH4250`](../diagnostics/DSH4xxx.md#dsh4250)。
[`dsc migrate`](../tools/migrate.md) 会把它们改写回来。

<a id="argument-rules"></a>

## 参数规则

下列规则对上面每个内置都一样，1.x 源码和 `.dss` 也一样。

| # | 规则 | 违反后果 |
| :-- | :-- | :-- |
| 1 | 参数个数精确 —— 没有默认值、没有可选参数、没有变参 | [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224) |
| 2 | 参数按位置给出，或以节点某个输入 pin 命名；位置参数填充第一个尚未填充的操作数。在 `.dss` 中，命名参数之后的位置参数是 [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) | 未知名字是 [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216)，同一 pin 给定两次是 [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215)，给定参数之间漏掉一个 pin 是 [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217) |
| 3 | 每个参数都作为完整的 Graph 表达式求值，包括嵌套的内置调用 | 内层表达式在自身位置报告自身的错误 |
| 4 | 每个参数都是数值（或 bool，读作 0 / 1）—— 纹理对象、`MaterialAttributes` 值或 `Substrate` 值不是 | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 5 | **会检查**分量数 *（自 2.0.0 起）*：最宽的操作数设定宽度，标量广播到该宽度，更窄的向量不会加宽 | `DSH4226` |
| 6 | `cross`、`reflect` 和 `refract` 接受 `float3` 操作数 | 更窄的向量是 `DSH4226`；更宽的向量在 1.x 源码里被截为三个分量并给出提示 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) —— 这正是 1.x 的做法 —— 而在 `.dss` 中是 `DSH4226` |
| 7 | `/` 的两个操作数都是整数构造器调用时会被拒绝 —— 材质图中没有整数除法；其他任何 `/` 都是浮点除法，包括 `7 / 2` | [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) |

在 1.9.x 及以前，规则 4 – 6 都不是编译检查：`dot(vec3Value, vec2Value)` 会被无声接受，
随后在 Unreal 自身的材质翻译阶段失败。

<a id="name-resolution"></a>

## 名称解析

对裸名的调用按以下顺序解析。完整的查找顺序见
[名称解析](../graph/name-resolution.md)。

| # | 候选 | 参考 |
| :-- | :-- | :-- |
| 1 | 在 1.x `Graph` 中，由 legacy 前端在读入调用时改写的写法：`SampleTexture2D`、已声明 property 的 pin 调用形式、`UE.SceneTexture` 以及其他 1.x 调用形状 | [`UE.*` 目录](ue.md) · [在 `Graph` 中使用参数](../../parameters/graph-usage.md) |
| 2 | 类型名 —— 诸如 `float3(…)`、`vec4(…)` 的构造器 | [构造器](../graph/constructors.md) |
| 3 | 以 `UE.` 和 `Substrate.` 为前缀的被调用者 | [`UE.*` 目录](ue.md) · [`Substrate.*`](substrate.md) |
| 4 | `Texture2DSample`、`Texture2DSampleLevel` | |
| 5 | **数学内置 —— 本页**，按精确的 HLSL 名字；然后是 GLSL 拼写（1.x 中为 `DSH5277`，`.dss` 中为 `DSH4250`） | — |
| 6 | 本文件声明或导入的函数 —— `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction`、`.dss` 函数 | [调用](../graph/calls.md) |
| 7 | 仅限 1.x 源码：名称**忽略大小写**才匹配的内置或函数，并给出警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) | — |

> [!IMPORTANT]
> **函数不能与内置同名** *（自 2.0.0 起）*。名为 `lerp`、`clamp`、`dot`、`min`、`max`、
> `pow`、`abs` —— 本页任何 HLSL 名字或 GLSL 拼写 —— 的 `Function`、`GraphFunction`、
> `ShaderFunction`、`VirtualFunction` 或 `.dss` 函数会在其声明处
> [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206)。在 1.9.x 及以前，这样的函数
> 可以编译，却从 `Graph` 块中静默地不可达，因为内置胜出。

拼错的内置名 —— `saturte(x)` —— 是 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)，当某个已声明名字仅大小写不同时
会附带 *did you mean*。

<a id="named-arguments"></a>

## 命名参数

*（自 2.0.0 起）* 命名参数指定节点的某个输入 pin —— 即目录中的 *Input pins* 列 —— 并填充该操作数，无论它在列表中处于什么位置：

```c
float t = lerp(A, B, Alpha = Mask);     // A, B by position; Alpha by name
float c = clamp(X, Max = 1.0, Min = 0.0);
```

pin 名是引擎的，因此不总是 HLSL 的：`pow` 是 `Base` / `Exponent`，`step`
是 `Y`（edge）/ `X`（value），`normalize` 是 `VectorInput`。节点没有的名字是
[`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216)，它会列出可用的 pin。没有节点的内置
（`fwidth`、`rcp`、`rsqrt`、`reflect`、`refract`）只接受按顺序给出的参数。

> [!NOTE]
> 在 1.9.x 及以前，数学内置上的命名参数会被报告为**参数个数**错误 ——
> `saturate(Input = X)` 会以“expects exactly 1 argument”消息失败。现在它会选中那个 pin。

<a id="per-builtin-notes"></a>

## 逐个内置节点的说明

<a id="clamp"></a>

### clamp

`clamp(Input, Min, Max)` 会连接全部三个参数，并将节点的 `ClampMode` 保持在其默认值
`CMODE_Clamp`。要使用 `CMODE_ClampMin` 或 `CMODE_ClampMax`，直接调用节点本身：
`UE.Clamp(Input = x, Min = a, ClampMode = CMODE_ClampMin)`。见 [`UE.Expression`](ue-expression.md)。

<a id="dot-length-distance-cross"></a>

### dot、length、distance、cross

结果宽度固定的内置：`dot`、`length` 和 `distance` 恒为一个分量，`cross`
恒为三个，与参数无关 —— 因此 `float d = dot(A, B);` 不需要 swizzle。

<a id="fmod"></a>

### fmod

参数 1 是被除数，参数 2 是除数；它们分别连接到节点的 `A` 和 `B` pin。

`mod` 是 GLSL 拼写。在 `Graph` 块中，1.x 源码仍可写它，并给出警告
`DSH5277`。在 [`Function`](../language/function.md) HLSL 体内部，标识符 `mod` 会被改写
为 `fmod` —— 静默地、忽略大小写 —— 与其他 GLSL 名字一样。

<a id="lerp"></a>

### lerp

结果的宽度与三个参数中最宽的那个相同，包括 `Alpha`：标量 `Alpha`
混合两个 `float3` 值会得到 `float3`，并被广播到该宽度。在两个 Substrate 值上，
`lerp(A, B, t)` 则是 `Substrate.HorizontalMixing` —— 见
[Substrate 语法糖](../language-v2/substrate.md)。

<a id="min-max"></a>

### min、max

这两个名字共用一套类型规则，区别只在于选择的节点类。

<a id="normalize"></a>

### normalize

唯一一个其单一输入 pin 不叫 `Input` 的内置：`UMaterialExpressionNormalize` 将它命名为
`VectorInput`。这一差异在按位置调用时（`normalize(N)`）不可见，而它正是
命名参数必须使用的名字。

<a id="sin-cos"></a>

### sin、cos

两者都将节点的 `Period` 属性保持为默认值。要使用非默认周期，请直接调用节点：
`UE.Sine(Input = x, Period = 2.0)`。

默认周期为 1，因此这些图内置按周期计：`sin(0.25)` 是 1，`cos(0.25)` 是 0
（在浮点精度内）。常量表达式使用与图节点相同的周期。
在 `@custom` HLSL 体内部，`sin` 和 `cos` 照常按弧度计算。

<a id="round"></a>

### round

图内置和常量折叠在恰好一半的值上遵循 [HLSL 的舍入到偶数规则](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-round)：`round(0.5)` 是 0，
`round(2.5)` 是 2，而 `round(-1.5)` 是 -2。

生成的 Custom 节点对 uniform 和变化的输入都用 HLSL `round` 求值。这样绕开了原生
`UE.Round(Input = x)` 节点的 preshader：在 UE 5.8 上它会把半值向上舍入
（`0.5` 变成 1）。需要原生行为时用反射写法。反编译会把未改动的
生成节点还原成 `round`；原生 Round 节点仍是 `UE.Round`。
`dump-ir` 显示语言层的运算，`dump-graph` 显示生成的 Custom 节点。

代价：引擎从不在 shader 之前求值 Custom 节点，所以对 uniform 值（比如参数）取 `round` 会
逐像素执行（一条指令），其下游的数学也随之逐像素，而不再折叠进材质 uniform 每次 draw 算一次。
常量参数仍在编译期折叠。比起半值的处理，这更要紧时，`UE.Round(Input = x)` 保留原生节点及其 preshader。

<a id="step"></a>

### step

`step(edge, x)` 返回 `x >= edge ? 1 : 0`，和 HLSL 一致。`UMaterialExpressionStep` 的 pin 命名是反过来的 ——
`Y` 是 edge、`X` 是 value —— 所以参数 1 连到 `Y`、
参数 2 连到 `X`。

<a id="smoothstep"></a>

### smoothstep

`smoothstep(min, max, x)`，参数顺序同 HLSL，直接连到节点的 `Min`、`Max` 和
`Value` pin。

<a id="asin-acos-atan-atan2"></a>

### asin、acos、atan、atan2

四个反三角节点。`atan2(y, x)` 的参数顺序同 HLSL，而节点的 pin 本来就叫 `Y` 和
`X`，所以映射是直接的。

> [!NOTE]
> 引擎还带了 `ArcsineFast`、`ArccosineFast`、`ArctangentFast` 和 `Arctangent2Fast` ——
> 在有限输入范围上成立的更便宜的近似。它们没有内置写法，直接调用节点：`UE.ArcsineFast(Input = x)`。

<a id="fwidth-rcp-rsqrt"></a>

### fwidth、rcp、rsqrt

三者都没有对应的引擎节点。`fwidth(x)` 按 HLSL 内建函数的定义建成 `abs(ddx(x)) + abs(ddy(x))`；
`rcp(x)` 是 `Divide`，其 `ConstA = 1`；`rsqrt(x)` 是
`SquareRoot` 接一个这样的 `Divide`。

<a id="reflect-refract"></a>

### reflect、refract

Unreal 没有 `Reflect` 或 `Refract` `UMaterialExpression`，所以两者都降级成 HLSL
对它们的定义，返回给调用方的是那张子图的最后一个节点。

`reflect(i, n)` 展开成 `i - 2 * dot(i, n) * n` —— 四个节点（`DotProduct`、两个 `Multiply`、
`Subtract`）。字面量 `2` 骑在 `Multiply` 的 `ConstB` 上，
不额外花一个 `Constant` 节点。

`refract(i, n, eta)` 展开成完整的 HLSL 定义 —— 十四个节点：

```text
k = 1 - eta*eta * (1 - dot(n, i)^2)
k < 0 ? 0 : eta*i - (eta*dot(n, i) + sqrt(k)) * n
```

全内反射的判断是一个 `If` 节点，所以两侧都会被翻译、再选其一；对负 `k` 取
`sqrt` 只落在被丢弃的那一侧，和 HLSL 完全一样。`k == 0`
仍然满足公式（`sqrt(0) == 0`）并走折射侧。零分支写成 `i * 0` 而不是常量，
是为了让它的宽度永远等于 `i` 的宽度 —— `If` 要求它的两个分支一致。

> [!NOTE]
> 两者都是按构造就很贵的。周围代码本来就是 HLSL 时，写在
> [`Function`](../language/function.md) 体里，一个节点就够了。

<a id="notes"></a>

## 注意事项

- 两个相同操作数上的相同调用 —— 写了两次的 `sin(X)` —— 只产生一个节点：IR 在发射任何东西之前
  做结构去重并合并它们。参见
  [节点复用](../graph/node-reuse.md)。
- 节点位置和其他节点一样来自 [图布局](../generation/graph-layout.md)。
- **`sinh`、`cosh` 和 `tanh` 被拒绝**，报 [`DSH4246`](../diagnostics/DSH4xxx.md#dsh4246)：
  材质图没有双曲节点。写在 [`Function`](../language/function.md) 体
  （或 `.dss` 里的 `/// @custom` 函数）里，那里着色器编译器有它们。
- **这个调用面上没有矩阵。** Unreal 材质图没有矩阵值类型 —— 没有
  `float3x3` 值，也没有 `mul(M, v)` —— 而常量折叠之后仍然存在的矩阵是
  [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361)。空间转换节点是
  [Transform 内置](transform.md)。真正的矩阵运算，写在
  [`Function`](../language/function.md) 的 HLSL 体里，那里 `float4x4` 和 `mul` 就是 HLSL。
- 在 `Function` 的 HLSL 体内部，这些名字*根本不*由编译器处理；函数体
  原样输出，适用的是 HLSL 自己的内建函数。
- [反编译器](../tools/decompiler.md)会写回这些写法。它的 2.0 写出器（`.dss`，
  默认）对目录中类的节点，只要输入 pin 全部连上、其他属性都是默认值，就打印内置 ——
  `Sine` 带非默认 `Period` 时
  仍是 `UE.Sine(...)` —— 并且在没有别的东西读入它们时，能认出 `fwidth`、`rcp`、
  `rsqrt`、`reflect` 和 `refract` 降级成的子图。旧写出器
  （`-Format Legacy`，*Export Legacy* 菜单项）保留 1.x 的映射表。

<a id="diagnostics"></a>

## 诊断

| 代码 | 何时触发 |
| :-- | :-- |
| [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224) | 参数个数错误 |
| [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) | 命名参数不是节点的任何输入 pin |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | 同一个 pin 被给了两次 |
| [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217) | 给出的参数之间漏了一个 |
| [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) | *（`.dss`）* 命名参数之后又出现位置参数 |
| [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) | 参数不是数值，或宽度与调用不符 |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) | *（提示，仅 1.x）* 更宽的向量被截成调用所要的宽度 |
| [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) | 整数除法 |
| [`DSH4246`](../diagnostics/DSH4xxx.md#dsh4246) | `sinh`、`cosh` 或 `tanh` |
| [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) | *（警告，仅 1.x）* 一个 GLSL 拼写 |
| [`DSH4250`](../diagnostics/DSH4xxx.md#dsh4250) | `.dss` 里用了 GLSL 拼写 |
| [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) | *（警告，仅 1.x）* 只在忽略大小写时才和内置匹配的名字 |
| [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208) | 名字既不是内置、类型、节点，也不是函数 |
| [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) | 函数与内置同名 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器建不出内置降级成的子图中的某个节点 |

完整清单见[诊断](../diagnostics/README.md)。

<a id="example"></a>

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

<a id="see-also"></a>

## 另见

- [内置](index.md) — `Graph` 内可用的调用面
- [`UE.*` 目录](ue.md) — 每一个具名材质节点内置
- [`UE.Expression`](ue-expression.md) — 任何 `UMaterialExpression`，按其类调用
- [变换内置](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [`Substrate.*`](substrate.md) — Substrate 节点包装器（UE 5.4+）
- [`DreamShaderBuiltins.ush`](hlsl-library.md) — 随附的 HLSL 辅助头文件
- [表达式与运算符](../graph/expressions.md) — `+ - * /`、优先级、操作数规则
- [构造器](../graph/constructors.md) — 在内置之前解析的类型名
- [转换](../graph/conversions.md) — 拓宽与收窄
- [调用](../graph/calls.md) — 调用语法、命名参数、out 参数
- [名称解析](../graph/name-resolution.md) — 完整的查找顺序
- [节点复用](../graph/node-reuse.md) — 为何重复调用只产生一个节点
- [不支持的构造](../graph/unsupported.md) — `%` 及其他缺失的运算符
- [`Function`](../language/function.md) — HLSL 函数体，完整的内建函数集在此适用
- [诊断](../diagnostics/README.md) — 每一个代码
