---
title: "语句"
description: "语句"
---

<a id="statements"></a>

# 语句

> [DreamShader](../index.md) » [Graph](index.md) » **语句**

`Graph` 块体可以包含的全部语句形式、终结这些语句的规则，以及
解析器如何判定一条给定语句属于哪种形式。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 位于 `Graph` 块内 |
| 类型 | 语句文法 |
| 执行方式 | 在生成时自上而下各执行一次 |

<a id="synopsis"></a>

## 概要

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

<a id="form-table"></a>

## 形式表

| # | 形式 | 示例 | 终结符 | 参考 |
| --: | :-- | :-- | :-- | :-- |
| 1 | 声明，无初始值 | `float3 c;` | `;` | [声明](declarations.md) |
| 2 | 声明 + 表达式初始值 | `float3 c = A * K;` | `;` | [声明](declarations.md) |
| 3 | 声明 + 花括号初始值 | `vec4 v = {rgb, 1.0};` | `;` | [声明](declarations.md) |
| 4 | 赋值给已有变量或输出变量 | `Color = Tint;` | `;` | [下文](#assignment) |
| 5 | 赋值 + 花括号初始值 | `Color = {r, g, b};` | — | **拒绝** —— [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162) *(自 2.0.0 起)*；请写 `Color = float3(r, g, b);` |
| 6 | `MaterialAttributes` 成员写入 | `Attrs.BaseColor = Tint;` | `;` | [MaterialAttributes](material-attributes.md) |
| 7 | 成员写入 + 花括号初始值 | `Attrs.BaseColor = {1, 0, 0};` | — | **拒绝** —— `DSH2162` *(自 2.0.0 起)*；请写一个构造器 |
| 8 | 逗号分隔声明子 | `float a = 1, b, c = 3;` | `;` | [声明](declarations.md) |
| 9 | 语句形式调用 | `F_Split(Src, OutA, OutB);` | `;` | [调用](calls.md) |
| 10 | `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | **无** —— 以其最后一个 `}` 结束 | [if / else](if.md) |

形式 8 会展开成每个声明子一个变量；它们全部共用**第一个**声明子的类型 token，
每个声明子报告自己的源码行和列。

<a id="termination-and-splitting"></a>

## 终结与切分

`Graph` 块体由 2.0 的语句解析器逐个记号读入 *(自 2.0.0 起)*；不再在 `;` 处
按文本切分。

| 规则 | 行为 |
| :-- | :-- |
| 分隔符 | 除 `if` 外每条语句都以 `;` 结束；缺了是 [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154)，报在语句之后的第一个记号上 |
| 重复的 `;` | 每个多出来的 `;` 都是空语句，直接丢弃，不产生诊断 |
| 结尾的 `;` | 块体的最后一条语句也**必须**有：`DSH2154` *(自 2.0.0 起)* |
| `if` 语句 | 以其最后一个块体的 `}` 结束，所以 `if` 不需要 `;` |
| 位置 | 每条语句和每个表达式都带自己的行和列 |
| 嵌套块体 | `if` / `else` 块体是同一文法的块，所以上面**每一种**形式在分支里都合法，包括嵌套的 `if` |

```c
Graph = {
    float a = 1.0;;;             // the empty statements are dropped
    if (a > 0.5) { a = 0.0; }    // no ; after the }
    float b = a * 2.0;           // the final statement needs its ; (DSH2154 without it)
}
```

解析失败的语句会被跳到它所在层级的下一个 `;`，解析器接着处理下一条语句；块体里的
每一个错误都会在同一个构建中报出。

<a id="classification-order"></a>

## 分类顺序

解析器根据语句开头的记号判定其形式。第一条匹配的规则胜出。

| 顺序 | 语句开头为 | 结果 |
| --: | :-- | :-- |
| 1 | 关键字 `if`（**区分大小写**） | 形式 10 |
| 2 | `for`、`while`、`do`、`return`、`break`、`continue`、`discard` | 能解析，随后被拒绝：[`DSH2208`](../diagnostics/DSH2xxx.md#dsh2208) |
| 3 | `switch`、`case`、`default` | [`DSH2160`](../diagnostics/DSH2xxx.md#dsh2160) |
| 4 | `{` | 裸块：[`DSH2220`](../diagnostics/DSH2xxx.md#dsh2220) |
| 5 | `;` | 空语句 |
| 6 | `#Region` / `#EndRegion` 行 | 区域；见 [Layout](../language/layout.md) |
| 7 | 连续两个名字（`float3 c`），前面可带 `static` / `const` | 声明：形式 1–3 和 8。存储关键字是 [`DSH2212`](../diagnostics/DSH2xxx.md#dsh2212) |
| 8 | 其他任何情况 | 表达式：赋值（形式 4–7）、调用（形式 9），或对无人接收的值报 [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) |

> [!WARNING]
> 关键字区分大小写。`If (x) { … }` 不是 `if` 语句：`If` 是一个名字，`If (x)` 是
> 一次调用，而本该放调用语句 `;` 的位置出现了 `{`，于是报 `DSH2154`。见
> [不支持的写法](unsupported.md#wrong-case-keywords)。

> [!NOTE]
> 声明的名字是一个标识符。`float3 A.B = x;` 在 `DSH2154` 处报 `.` *(自 2.0.0 起)*；1.x
> 会声明一个名字字面为 `A.B` 的变量。成员写入前面不带类型：`Attrs.BaseColor = x;`。

<a id="assignment"></a>

## 赋值

形式 4 和 6。目标是一个名字或 `name.member`；其他任何东西 —— 成员的 swizzle、
下标、调用 —— 都是 [`DSH2206`](../diagnostics/DSH2xxx.md#dsh2206)。

```c
<target> = <expression> ;
```

目标按以下顺序解析：

| 顺序 | 目标名称 | 行为 |
| --: | :-- | :-- |
| 1 | `name.member` | `MaterialAttributes` 成员写入 —— 见 [MaterialAttributes](material-attributes.md) |
| 2 | Graph 变量、`Shader` 的 `Outputs` 声明，或函数块的一个输出 | 值被转换到该变量的声明类型 |
| 3 | 一个属性 | [`DSH4229`](../diagnostics/DSH4xxx.md#dsh4229)：属性是输入；请先把它复制到变量里 |
| 4 | 什么都不是 | 在这里以值的类型声明 —— 沿用旧规则 L26，并由提示 [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292) 说明 |

名字先精确查找，然后忽略大小写查找；另一个大小写下的唯一匹配会被采用，
并带警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。见 [名称解析](name-resolution.md)。

规则 4 意味着 `=` 左侧未声明的名字不是错误：

```c
Graph = {
    vec3 Boost = Tint * 2.0;
    Scratch    = Boost;      // DSH5292: declares 'Scratch' as a float3
    Color      = Scratch;    // 'Color' is an Outputs declaration -> converted to its type
}
```

规则 2 的转换取更宽值的**前导分量**，并给出提示
[`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)（旧规则 L22），还会把标量广播到
更宽的目标。它绝不会把 2 分量值加宽到 3 分量：那种情况以及其他任何放不进去的值
都是 [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228)。见 [转换](conversions.md)。

<a id="brace-initializer-assignment"></a>

### 花括号初始值赋值

花括号列表只在作为声明的初始值时才会被读入。在赋值或
成员写入的右侧它是 [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162) *(自 2.0.0 起)*；请改写为
构造器：`Color = float3(r, g, b);`。声明形式及其规则见
[声明](declarations.md#brace-initializers)；构造器规则见
[构造器](constructors.md)。

<a id="expression-statements"></a>

## 表达式语句

形式 9。既不是声明也不是赋值的语句必须是一个**调用**；其他任何
表达式都是 `DSH2211`。

在语句形式下，函数的输出由末尾的参数接收，每个输出一个普通变量
名，按声明顺序。完整的参数规则、out 目标约束以及
值调用形式见 [调用](calls.md)。

语句形式的语句调用支持多输出 `ShaderFunction` / `VirtualFunction`
*（自 1.3.5 起）*；值形式调用支持单输出 `Function` / `GraphFunction` *（自 1.3.1 起）*。

<a id="synthesized-statements"></a>

## 合成语句

每个 `Outputs` 声明 `Shader` 都会成为在材质体开头声明的变量，
排在 `Graph` 的第一条语句之前 *（自 2.0.0 起）*。有初始值时用它自己的
初始值初始化，否则用该类型的零值（`MaterialAttributes` 为空属性集）；没有初始值的纹理或
`Substrate` 输出开始时没有值。这些声明带着它们来源的那一行 `Outputs` 的位置。

在顶层用相同大小写重新声明一个输出的名字的 `Graph` 声明，不会声明第二个变量：
有初始值时它成为对该输出的赋值，没有时直接被丢弃。

| 码 | 何时触发 |
| :-- | :-- |
| [`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270) | 一个 `Outputs` 声明带有 `=` 但后面什么都没有 |
| [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) | 一个 `Outputs` 声明后面既不是 `;` 也不是该段落的闭合 `}` |

一个 `Shader` 只要其 `Outputs` 段落声明或绑定了任何东西，就不需要 `Graph` 段落；
两者都没有则是 [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)。见 [Shader](../language/shader.md)。

<a id="diagnostics"></a>

## 诊断

每条诊断都以 `<file>(<line>,<column>): DSHnnnn: <message>` 报告，位置在该构造本身，
包括 `if` 块体内部。

<a id="parse-time"></a>

### 解析时

| 码 | 何时触发 |
| :-- | :-- |
| `DSH2154` | 一条语句没有以 `;` 结尾——包括块体的最后一条 |
| [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) | 期望一个表达式，却发现别的东西，例如 `= 5;` |
| [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) | 调用或带括号表达式中缺少 `)` |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` 后面没有名字 |
| [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) | `::` 后面没有名字 |
| [`DSH2163`](../diagnostics/DSH2xxx.md#dsh2163) | `,` 后面的声明子不是名字 |
| [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) | 格式错误的数字，例如 `0.5.5` ——见[字面量](literals.md) |
| `DSH2160` | `switch`、`case`、`default` |
| `DSH2162` | 花括号列表被当作值使用（形式 5 和 7） |
| `DSH2200`–`DSH2222` | 1.x 没有的构造——见[不支持的写法](unsupported.md) |

<a id="bind-and-build-time"></a>

### 绑定与构建时

| 码 | 何时触发 |
| :-- | :-- |
| `DSH2206` | `=` 的左侧不是名字也不是 `name.member` |
| [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220) | 一个名字在同一块里被声明两次，或某个分支声明了外层块体的名字 |
| `DSH4228` | 赋值或初始值的值放不进目标的类型 |
| `DSH4229` | 目标是属性 |
| `DSH5289` | *提示：* 更宽的值被截到目标的前导分量 |
| `DSH5292` | *提示：* 一次赋值声明了它未声明的目标 |
| `DSH5275` | *警告：* 一个名字只在忽略大小写时匹配到某个声明 |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | 一个名字匹配不到任何声明 |

跨阶段的完整清单见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

一个块体里写全所有被接受的语句形式：

```c
ShaderFunction(Name="Functions/F_AllForms")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); float K = 2.0; }
    Inputs     = { vec2 UV; }
    Outputs    = { MaterialAttributes Attrs; vec3 Debug; }

    Graph = {
        MaterialAttributes Attrs;              //    names the output: no second variable
        float3 c;                              // 1  declaration, no initializer
        float3 scaled = Tint * K;              // 2  declaration + expression
        vec4   packed = {scaled, 1.0};         // 3  declaration + brace initializer
        c = packed.rgb;                        // 4  assignment
        Debug = float3(0.0, 0.0, 0.0);         //    (form 5 is refused: write the constructor)
        Attrs.BaseColor = c;                   // 6  member write
        Attrs.Roughness = 0.35;                //    (form 7 is refused: write the value)
        float a = 1, b, d = 3;                 // 8  comma declarators

        if (K > 1.0) {                         // 10 if / else
            Debug = c * a;
        } else {
            Debug = c * b;
        }
    }
}
```

<a id="see-also"></a>

## 参见

- [Graph](index.md) —— 求值模型与段落语法
- [声明](declarations.md) —— 类型 token、默认值、逗号声明子、花括号初始值、作用域
- [表达式](expressions.md) —— 运算符、优先级、结合性
- [if / else](if.md) —— 条件、真值表、分支合并
- [调用](calls.md) —— 值形式与语句形式调用、out 目标、命名参数
- [MaterialAttributes](material-attributes.md) —— 成员读取与写入
- [转换](conversions.md) ——“转换到声明类型”具体做什么
- [名字解析](name-resolution.md) —— 裸标识符如何查找
- [不支持的写法](unsupported.md) —— `for`、`while`、`return`、`+=`、三元运算符及其码
- [节点复用](node-reuse.md) —— 为什么两条相同语句可以产生一个节点
- [输出绑定](../language/output-bindings.md) —— `Outputs` 声明及其初始值
- [诊断索引](../diagnostics/index.md) —— 按阶段列出的所有码
