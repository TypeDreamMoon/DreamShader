---
title: "字面量"
description: "字面量"
---

<a id="literals"></a>

# 字面量

> [DreamShader](../index.md) » [图表](index.md) » **字面量**

一个 `Graph` 表达式可以包含三种字面量形式：数值字面量、字符串字面量，以及布尔字面量
`true` / `false`。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 位于 `Graph { … }` 主体、`Outputs` 绑定表达式或 `Outputs` 声明默认值中 |
| 种类 | 表达式主项 |
| 生成 | `UMaterialExpressionConstant`（数值和布尔）；字符串字面量本身不生成任何内容 |

一个 1.x 的 `Graph` 主体由 DreamShaderLang 2.0 词法分析器读取*（自 2.0.0 起）*：下面的词法规则
是 2.0 的规则，而少数读取结果不同的 1.x 写法会被拒绝，并给出说明 1.x 行为的错误码。

<a id="synopsis"></a>

## 概要

```c
<number-literal> := { <digit> | . } … [ { e | E } [ { + | - } ] <digit> … ] [ <suffix> … ]
<suffix>         := { f | F | h | H | u | U | l | L }
<string-literal> := " <character> … "
<bool-literal>   := { true | false }
```

数值字面量以数字开头，或以紧跟数字的 `.` 开头。`[ … ]`、
`{ a | b }` 和 `…` 是元记号；`.`、`e`、`+`、`-` 和 `"` 是字面字符。

<a id="numeric-literals"></a>

## 数值字面量

没有小数部分、指数或浮点后缀的字面量对编译器来说是**整数**——
`int`，或带有 `u` 的 `uint` *（自 2.0.0 起）*；其他所有情况都是 `float`（带 `h` 的 `half`）。在
生成的图表中，每个字面量都是同一回事：一个单分量浮点 `Constant`。

| 写法 | 种类 | 值 | 结果 |
| :-- | :-- | :-- | :-- |
| `1` | `int` | 1.0 | `Constant`，1 个分量 |
| `1.0` | `float` | 1.0 | 与 `1` **相同**的 `Constant` 节点 |
| `0.5` | `float` | 0.5 | `Constant` |
| `.5` | `float` | 0.5 | `Constant` —— 开头加 `.` 是合法的 |
| `1.` | `float` | 1.0 | `Constant` —— 末尾加 `.` 是合法的 |
| `1e-3` | `float` | 0.001 | `Constant` |
| `2.5E+2` | `float` | 250.0 | `Constant` |
| `0.55f` | `float` | 0.55 | `Constant` |
| `0.5h` | `half` | 0.5 | `Constant` |
| `3u` | `uint` | 3.0 | `Constant` —— 无符号整数 *（自 2.0.0 起）* |
| `0.5.5` | — | — | [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *（自 2.0.0 起；1.x 会静默读取为 0.5）* |
| `0x10` | — | — | [`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) —— 十六进制字面量，1.x 从不将其作为十六进制读取 |
| `1.0fx` | — | — | `DSH2105` —— 字母属于该数字 |
| `3ul` | `uint` | 3.0 | `Constant` —— `u` 和 `l` 都是整数后缀 *（自 2.0.0 起）* |

词法细节：

- 一个数字是一个记号：数字、至多一个 `.`、至多一个指数，然后是后缀字母。任何
  仍粘连在其后的内容——另一个 `.`、字母、后缀之后的数字——都属于同一个记号，并使其
  成为 [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *（自 2.0.0 起；1.x 保留最长有效前缀并
  丢弃其余部分）*。因此 `0.5.5`、`0..5`、`1abc` 和 `1.0fx` 是错误，而不是数字。
- 指数需要数字：`1e` 和 `1e+` 是 `DSH2105`。指数的符号可以是 `+` 或 `-`，
  且仅在紧跟 `e`/`E` 之后时才会被读取。
- 十六进制字面量（`0x1F`）对词法分析器来说是整数，且在 1.x 的 `Graph` 中会被拒绝
  （[`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222)）。没有二进制或八进制表示法，也没有数字
  分隔符。
- 开头的 `-` 不属于字面量；它是[一元负号运算符](expressions.md#unary-operators)。
  取负的字面量会折叠成一个 `Constant`。

<a id="numeric-suffixes"></a>

## 数值后缀

后缀字母保留在字面量的文本中，并决定其种类；值则是它们前面的数字。

| 后缀 | HLSL/GLSL 含义 | 在 DreamShaderLang 中的效果 |
| :-- | :-- | :-- |
| `f`、`F` | float | 一个 `float` 字面量 |
| `h`、`H` | half | 一个 `half` 字面量 |
| `u`、`U` | unsigned int | 一个 `uint` 字面量 |
| `l`、`L` | long | 一个整数字面量 |

规则：

- 一个数字后面可以跟多个后缀字母。浮点字母与整数类型字母组合
  （`1fu`）会 `DSH2105`，在带有小数部分或指数的字面量上使用整数类型字母
  （`1.0u`、`1e3l`）或在十六进制字面量上使用浮点字母也是如此。
- 字面量的种类只对编译器有意义——参见[整数与浮点](#integer-and-float)。
  图表中两者都得到一个浮点 `Constant`。

<a id="integer-and-float"></a>

## 整数与浮点

| 问题 | 回答 |
| :-- | :-- |
| `1` 与 `1.0` 不同吗？ | 对编译器来说，是的：`1` 是一个 `int` *（自 2.0.0 起）*。在图表中则不然：两者都是一个持有 1 的 `Constant` 节点。 |
| `3u` 会产生整数吗？ | 会，产生一个 `uint` *（自 2.0.0 起）*。 |
| `int x = 7;` 会产生整数吗？ | 会：`x` 是一个 `int` 变量 *（自 2.0.0 起）*。它在图表中的值是浮点 7。 |
| 什么使一个值成为整数？ | 一个整数字面量、一个 `int` / `uint` 声明，或一个整数[构造函数](constructors.md#integer-constructors)。 |
| 整数种类有什么作用？ | 在 1.x 的 `Graph` 中，没有任何可见效果：`7 / 2` 是 3.5，与 1.x 中一样 *（2.0.0 – 2.1.0 会以 [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) 拒绝它）*，只有 `int(7) / int(2)` 会被拒绝。在 `.dss` 中，同样的 `7 / 2` 是 `DSH4243`，因此 [`dsc migrate`](../tools/migrate.md) 会写入 `float(7) / 2`。参见[整数除法](expressions.md#integer-division)。 |
| 有整数算术或截断吗？ | 没有。`int x = 7.9;` 持有 7.9，而在材质图表中 `7.0 / 2` 是 3.5。 |

<a id="string-literals"></a>

## 字符串字面量

字符串字面量在进行词法分析时转义会被解码，并且**永远不会求值为一个值**。它仅
在参数处理器读取字面量*文本*而不求值时才是合法的——例如设置节点属性的命名 `UE.*` /
`Substrate.*` 参数、`Output=` / `OutputName=` 输出选择器，
或 `Path(…)` 资产引用。任何需要值的地方都会是
[`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202)。

| 转义 | 产生 |
| :-- | :-- |
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | 无 —— 该字符被丢弃 |
| `\<any other character>` | [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) *（自 2.0.0 起；1.x 会丢弃反斜杠）*；反斜杠保留在值中 |

> [!NOTE]
> 字符串永远不会跨行。在其行结束前没有闭合 `"` 的字符串会被
> [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103) *（自 2.0.0 起；1.x 会一直读取到输入末尾）*，
> 而文件的其余部分仍会被读取并报告各自的错误。

合法位置：

```c
vec4 Scene = UE.SceneTexture(Id = "PostProcessInput0");
```

无效位置（任何需要值的位置）：

```c
float x = "0.5";   // DSH4202
```

<a id="boolean-literals"></a>

## 布尔字面量

`true` 和 `false` *（自 1.6.0 起）* 在解析该行时即被读取为字面量，且不区分大小写，早于
任何名称查找 *（自 2.0.0 起；1.x 在变量和属性之后解析它们）*。

| | |
| :-- | :-- |
| 拼写 | **不区分大小写**匹配 — `true`、`True`、`TRUE`、`false`、`False`、`FALSE` |
| 类型 | `bool`，1 个分量 |
| 节点 | 一个持有 1 或 0 的 `Constant` *（自 2.0.0 起；1.x 构建的是 `StaticBool`）* |
| 复用 | 相等的常量是同一个节点，因此图中每个值至多有一个节点 |

```c
StaticBool Enabled = true;     // a bool variable
float      On      = TRUE;     // any case; a bool used as a number is 1
```

> [!WARNING]
> 由于 `true` / `false` 在任何名称**之前**被读取，因此名为
> `True` 或 `False` 的 Graph 变量或已声明属性（不区分大小写）无法被读取：该名称的每次使用都是该字面量。请避免使用这些
> 名称。参见[名称解析](name-resolution.md#shadowing)。

<a id="notes"></a>

## 备注

- 相等的数字字面量在整个图中是同一个 `Constant` 节点：[去重遍](node-reuse.md)
  会以 9 位有效数字比较值，因此写五次 `0.5`，或 `1` 和 `1.0`，都是同一个节点。
- 当构造函数的**每个**参数都是常量时，构造函数会折叠为一个
  `Constant2Vector` / `Constant3Vector` / `Constant4Vector` 节点，而不是 `Constant` 个节点加上
  `AppendVector` 个节点——包括整数构造函数 *（自 2.0.0 起）*。参见
  [构造函数](constructors.md#constant-folding)。
- 一个字面量宽度为一个分量；任何值的宽度都不会与其类型不同 *（自 2.0.0 起）*。参见
  [转换](conversions.md#authoritative-component-counts)。
- 此处的字面量规则是 `Graph` 表达式的规则。`Properties`、`Settings` 和元数据块的声明级规则见[词法元素](../language/lexical.md)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH2105` | 格式错误的数字：第二个 `.`、没有数字的指数、浮点数与整数后缀同时存在、小数部分带整数后缀、末尾粘连字母。 |
| `DSH2222` | 1.x `Graph` 中的十六进制字面量。 |
| `DSH4243` | 两个整数构造函数调用之间的 `/`。两个字面量整数之间是浮点除法。 |
| `DSH2103` | 字符串字面量在其所在行没有结束的 `"`。 |
| `DSH2104` | 字符串字面量中的未知转义。 |
| `DSH4202` | 在需要值的位置出现字符串字面量。 |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | 一个既不是变量也不是声明的标识符。 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建 `Constant` 节点。 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Literals", Root="Game")
{
    Properties { ScalarParameter K = 0.5; }

    Settings {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs {
        float Rough;
        vec3 Color;
        Base.Roughness = Rough;
        Base.EmissiveColor = Color;
    }

    Graph {
        Rough = 0.55f * K;              // a float literal with its suffix
        float H     = .5;               // leading dot
        float Small = 1e-3;             // exponent
        float One   = 1;                // an int literal: the same Constant node as the 1.0 below
        Color = vec3(H * K, Small, 1.0) * One;
    }
}
```

生成的节点：

```text
ScalarParameter K
Constant(0.55), Multiply             -> Base.Roughness
Constant(0.5), Multiply              -> H * K
Constant(0.001)                      -> Small
Constant(1.0)                        -> One, and the third vec3 component: one node
AppendVector, AppendVector           -> vec3(H * K, Small, 1.0)
Multiply                             -> Base.EmissiveColor
```

<a id="see-also"></a>

## 另见

- [表达式](expressions.md) — 运算符、优先级以及字面量可以出现的位置
- [构造函数](constructors.md) — 全常量构造函数调用的常量折叠
- [转换](conversions.md) — 1 分量字面量如何加宽到向量目标
- [词法元素](../language/lexical.md) — 注释、标识符以及声明级字面量规则
- [类型](../language/types.md) — 完整的类型标记目录
- [名称解析](name-resolution.md) — 查找顺序，以及为什么 `true` 无法被遮蔽
- [节点复用](node-reuse.md) — 常量去重
- [泛型 `UE.Expression`](../builtins/ue-expression.md) — 字面量可以填充的命名字符串参数
