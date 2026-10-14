---
title: "表达式"
description: "表达式"
---

<a id="expressions"></a>

# 表达式

> [DreamShader](../index.md) » [图](index.md) » **表达式**

`Graph` 块中产生值的文法：字面量、标识符、调用、成员访问以及四个算术运算符，求值后成为 `UMaterialExpression` 节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 位于 `Graph { … }` 块体中、`Outputs` 绑定表达式中，或 `Outputs` 声明默认值中 |
| 种类 | 表达式文法 |
| 生成 | `UMaterialExpressionAdd`、`UMaterialExpressionSubtract`、`UMaterialExpressionMultiply`、`UMaterialExpressionDivide` —— 每个二元运算符一个节点；相同的节点会被合并（见[说明](#notes)） |

<a id="synopsis"></a>

## 语法概要

```c
<expression>     := <additive>
<additive>       := <multiplicative> [ { + | - } <multiplicative> ] …
<multiplicative> := <unary> [ { * | / } <unary> ] …
<unary>          := { + | - } <unary> | <postfix>
<postfix>        := <primary> [ <postfix-op> ] …
<postfix-op>     := . <identifier> | :: <identifier> | ( [ <argument> [ , <argument> ] … ] )
<primary>        := <identifier> | <number-literal> | <string-literal> | ( <additive> )
<argument>       := [ <identifier> = ] <additive>
```

`( ) , . :: = + - * /` 是 DreamShaderLang 的字面标点。`[ … ]`、`{ a | b }` 和 `…` 是元记号，永远不会被输入。

这是 1.x 文法。legacy 前端用 2.0 文法解析表达式，后者是它的超集，并会把上面文法之外的一切带码拒绝 *（自 2.0.0 起）* —— 见[不存在的运算符](#operators-that-do-not-exist)。

<a id="precedence-and-associativity"></a>

## 优先级与结合性

绑定最紧的排在最前。

| 层级 | 运算符 | 元数 | 结合性 | 说明 |
| :-- | :-- | :-- | :-- | :-- |
| 1 | `f(…)` 调用、`.member`、`::name` | 后缀 | 左 | 可自由串联：`A::F(x).rgb.b` |
| 2 | `+` `-` | 一元前缀 | 右 | 可递归：`- -x` 能解析且合法。不带空格的 `--x` 是自减运算符，[`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207) |
| 3 | `*` `/` | 二元 | 左 | `a / b / c` 是 `(a / b) / c` |
| 4 | `+` `-` | 二元 | 左 | `a - b - c` 是 `(a - b) - c` |
| —— | `( … )` 分组 | —— | —— | 覆盖上面各层 |

<a id="operators-that-do-not-exist"></a>

## 不存在的运算符

1.x 表达式文法里没有别的运算符。下面每一种都能解析，随后被拒绝：

| 类别 | 写法 | 码 |
| :-- | :-- | :-- |
| 取模 | `%` —— 请用 `fmod` 内置 | [`DSH2201`](../diagnostics/DSH2xxx.md#dsh2201) |
| 比较 | `==` `!=` `<` `>` `<=` `>=` —— **只**在 `if` 条件里合法 | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) |
| 逻辑 | `&&` `\|\|` `!` | `DSH2200` |
| 位运算 / 移位 | `&` `\|` `^` `~` `<<` `>>` | `DSH2201` |
| 条件 | `? :` | `DSH2200` |
| 自增 / 自减 | `++` `--` | `DSH2207` |
| 复合赋值 | `+=` `-=` `*=` `/=` 等其余 | [`DSH2205`](../diagnostics/DSH2xxx.md#dsh2205) |
| 表达式内赋值 | `=`（命名调用参数除外） | [`DSH2204`](../diagnostics/DSH2xxx.md#dsh2204) |
| 下标 | `[ ]` —— 请用 [swizzle](swizzle.md) | [`DSH2202`](../diagnostics/DSH2xxx.md#dsh2202) |
| 强制转换 | `(float3)x` —— 请用构造器 | [`DSH2203`](../diagnostics/DSH2xxx.md#dsh2203) |
| 逗号运算符 | `,`（调用参数表除外） | 没有自己的码：语句在那里结束，后面的内容通常是 [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) |
| 矩阵类型 / 运算符 | 不存在；见[构造器](constructors.md#notes) | 矩阵值是 [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) |

完整清单及应改写的写法见[不支持的写法](unsupported.md)。

> [!WARNING]
> **1.x 会静默截断这些写法。** 它的分词器把每个不认识的字符（`%`、`!`、`<`、`>`、`&`、`|`、`^`、`~`、`?`、`[` ……）都映射成输入结束，于是解析器保留它前面的表达式、无声地丢掉其余部分。*（自 2.0.0 起）* 起整条表达式都会被读完，运算符被拒绝：
>
> | 写法 | 1.x 构建出的 | 现在的码 |
> | :-- | :-- | :-- |
> | `a % b` | `a` | `DSH2201` |
> | `a && b` | `a` | `DSH2200` |
> | `a ? b : c` | `a` | `DSH2200` |
> | `v[0]` | `v` | `DSH2202` |
> | `a << 2` | `a` | `DSH2201` |
>
> 根本不是记号的字符（`@`、`$`、反引号）是 [`DSH2101`](../diagnostics/DSH2xxx.md#dsh2101)。不再需要靠给可疑表达式加括号来让截断暴露出来。

<a id="operand-rules"></a>

## 操作数规则

绑定器对 `+ - * /` 的两个操作数做这些检查：

| # | 检查 | 失败时 |
| :-- | :-- | :-- |
| 1 | 两个操作数都是数或 bool——不是纹理、采样器或 `MaterialAttributes` 值 | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 2 | `Substrate` 操作数：两个 `Substrate` 值之间的 `+` 和乘以标量的 `*` 分别构建 `Substrate.Add` / `Substrate.Weight` *（自 2.0.0 起；1.x 两者都拒绝）*；其他运算符 | [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293) |
| 3 | 宽度一致：结果与最宽的操作数一样宽，标量操作数广播到该宽度，更窄的向量不会 | `DSH4226` |
| 4 | 对 `/`：两个操作数不能都是整数构造器调用 | [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) —— 见[整数除法](#integer-division) |

运算符从不收窄操作数：`float3 * float4` 是 `DSH4226`，这样通道永远不会被静默丢掉。*（自 2.0.0 起）* 起它也不再加宽操作数：1.x 在某些情况下会把一个操作数加宽到“权威”值的宽度；现在操作数都有类型，标量广播是两种宽度相遇的唯一方式。见[转换](conversions.md)。

这里没有单独的标量提升步骤：标量操作数原样传给材质节点，由 Unreal 自行复制，所以 `A * K` 中 `A` 是 `vec3`、`K` 是 `float` 时只产生一个 `Multiply` 节点，而不是 `AppendVector` 广播。

<a id="result-of-a-binary-operator"></a>

## 二元运算符的结果

| 属性 | 取值 |
| :-- | :-- |
| 节点 | `Add` / `Subtract` / `Multiply` / `Divide`（每个运算符） |
| 宽度 | 最宽操作数的宽度 |
| 种类 | 按 HLSL 规则提升——`int` 与 `float` 得到 `float`；在图里每个值都是浮点 |
| 全是常量的操作数 | 由常量折叠 pass 折叠成一个 `Constant` 节点 *（自 2.0.0 起）* |

<a id="unary-operators"></a>

## 一元运算符

| 形式 | 降级为 | 节点开销 | 结果 |
| :-- | :-- | :-- | :-- |
| `+x` | 恒等 | 无节点 | `x` 原样返回 |
| `-x` | `Multiply(x, Constant(-1))` | 一个 `Multiply` 和一个值为 `-1` 的 `Constant` | `x` 的类型 |

`!x` 是 `DSH2200`，`~x` 是 `DSH2201`，`++x` 和 `--x` 是 `DSH2207`。

取负常量会被折叠：`-2.0` 变成一个 `Constant`，持有 `-2` *（自 2.0.0 起）*，常量上的任何算术运算也是如此。见[构造器](constructors.md#constant-folding)。

<a id="integer-division"></a>

## 整数除法

`/` 是唯一带有额外类型规则的运算符：当**两个**操作数都是整数构造器调用（`int`、`int2..4`、`ivec2..4`、`uint`、`uint2..4`、`uvec2..4`）时，它是 [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)。材质图没有整数除法，所以编译器拒绝在 HLSL 的截断结果与图的分数结果之间做出选择。

其他所有 `/` 都是图的浮点除法，与 1.x 一样，1.x 把每个数字字面量都定成浮点——即使在编译器看来两边都是整数也一样：整数字面量（`7`）、`int` / `uint` 变量或属性（legacy 规则 L27）。[`dsc migrate`](../tools/migrate.md) 把这样的除法写成 `float(7) / 2`，因为 `.dss` 拒绝整数之间的任何 `/`。

| 表达式 | 结果 |
| :-- | :-- |
| `int(7) / int(2)` | `DSH4243` |
| `int(7) / 2` | `3.5` *（2.0.0 – 2.1.0 以 `DSH4243` 拒绝）* |
| `7 / 2` | `3.5` *（2.0.0 – 2.1.0 以 `DSH4243` 拒绝）* |
| `7.0 / 2` | `3.5` |
| `float(int(7)) / int(2)` | `3.5` —— 一个操作数是浮点 |
| `int a = 7; int b = 2; a / b` | `3.5` *（2.0.0 – 2.1.0 以 `DSH4243` 拒绝）* |

> [!NOTE]
> 对编译器来说，`int`、`uint`、`bool` 和 `half` 是各自的种类，这条规则正是这个区别的用途。在生成的图里它们全都是浮点——没有整数运算，也没有截断。见[转换](conversions.md)。

<a id="compound-assignment"></a>

## 复合赋值

`+= -= *= /=` 不是 1.x 运算符。*（自 2.0.0 起）* 每一个都是 [`DSH2205`](../diagnostics/DSH2xxx.md#dsh2205)，无论空格如何。

| 写法 | 1.x 读作 | 现在的代码 |
| :-- | :-- | :-- |
| `a += b;` | 声明一个类型为 `a` 的变量 `+` | `DSH2205` |
| `a+=b;` | 赋值给一个名为 `a+` 的新变量；`a` 不变，无消息 | `DSH2205` |

请写出展开形式：`a = a + b;`。

<a id="increment-and-decrement"></a>

## 自增与自减

`++` 和 `--` 不是 1.x 运算符。*（自 2.0.0 起）* 每一种形式都是 [`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207)：`a++;`、`a--;`、`++a;`、`--a;`，以及更大表达式内部的 `x = --a;`，1.x 把后者读作 `-(-a)`。请改写为 `a = a + 1;`。

<a id="notes"></a>

## 说明

- 相同的子表达式在降级后会被合并：去重 pass 对每一组运算相同、操作数相同且顺序相同的节点只保留一个节点，所以写两次的 `A + B` 只产生一个 `Add`，而 `B + A` 是第二个。常量也以同样方式合并。见[节点复用](node-reuse.md)。
- 分组括号不产生任何东西；它们只影响解析顺序。
- 调用可以直接 swizzle：`UE.TexCoord().x` 是一个调用后跟成员访问的后缀链。见[Swizzle](swizzle.md)。
- `::` 是被调用路径中的命名空间分隔符：`N::F` 指名 `Namespace(Name="N")` 的函数 `F`。它后面必须跟一个名字（[`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260)）。见[名称解析](name-resolution.md)。
- 命名调用参数（`Coordinates = uv`）是参数语法的一部分，不是赋值运算符。见[调用](calls.md)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) | 需要操作数却发现了别的东西，例如 `x = ;` 或 `a * )` |
| [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) | 调用或括号表达式缺少 `)`；也用于参数之间缺少 `,` |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` 后面不是名字 |
| `DSH5260` | `::` 后面不是名字 |
| `DSH2101` | 不是记号的字符 |
| `DSH2200` | 在 `if` 条件之外出现比较、逻辑运算符或 `?:` |
| `DSH2201` | `%`、移位或位运算符 |
| `DSH2202` | `[ ]` |
| `DSH2203` | C 风格强制转换 |
| `DSH2204` | 表达式内部的赋值 |
| `DSH2205` | 复合赋值 |
| `DSH2207` | `++` 或 `--` |
| `DSH4226` | 操作数不是数，或宽度不一致 |
| `DSH4243` | `/` 的两个操作数都是整数构造器调用 |
| `DSH5293` | `Substrate` 值没有的运算符 |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | 操作数名字未声明 |
| [`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202) | 字符串被用作值 |
| [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) | 表达式语句不是调用或赋值，例如 `a + b;` |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Expressions")
{
    Properties {
        vec3 A = vec3(1.0, 0.5, 0.2);
        vec3 B = vec3(0.1, 0.2, 0.3);
        float K = 2.0;
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph {
        vec3 Sum    = A + B;
        vec3 Diff   = A - B;
        vec3 Scaled = A * K;
        vec3 Ratio  = A / K;
        vec3 Neg    = -Scaled;
        Color = (Sum + Diff - Scaled + Ratio) * 0.25 + Neg;
    }
}
```

生成的节点：

```text
VectorParameter A, VectorParameter B             (property nodes)
ScalarParameter K                                (property node)
Add(A, B)                                        -> Sum
Subtract(A, B)                                   -> Diff
Multiply(A, K)                                   -> Scaled
Divide(A, K)                                     -> Ratio
Multiply(Scaled, Constant(-1))                   -> Neg
Add / Subtract / Add chain, Multiply(..., 0.25), Add(..., Neg)
```

<a id="see-also"></a>

## 参见

- [语句](statements.md) —— 表达式可以出现的语句形式
- [不支持的写法](unsupported.md) —— 所有被拒绝的写法及其代码
- [字面量](literals.md) —— 数值、字符串和布尔字面量形式
- [构造器](constructors.md) —— `float3(…)`、`vec4(…)` 和整数构造器
- [Swizzle](swizzle.md) —— `.rgb`、`.bgr`、通道掩码
- [转换](conversions.md) —— 加宽、收窄和种类
- [`if` / `else`](if.md) —— 唯一接受比较运算符的地方
- [调用](calls.md) —— 调用语法、命名参数、out 参数
- [名称解析](name-resolution.md) —— 标识符或被调用者如何查找
- [节点复用](node-reuse.md) —— 重复的子表达式为什么只产生一个节点
- [数学内置](../builtins/math.md) —— `fmod`、`pow`、`min`、`max` 及其余
