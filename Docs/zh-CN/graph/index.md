---
title: Graph 求值模型
description: Graph 块究竟是什么、它如何由 2.0 编译器的 legacy 前端构建成节点，以及它为什么和外层的声明语言不是同一门语言。
---

`Graph` 块是 DreamShaderLang 文件从"描述材质"转为"构建材质"的地方。它的块体是一串语句，编译器在
**生成阶段按顺序降级一次**，产出目标 `UMaterial` 或 `UMaterialFunction` 上的 `UMaterialExpression` 节点。

Graph 块体里的东西不会以代码形式存活到运行时。存活下来的是它构建出的那张图。

**提示：**

自 （自 2.0.0 起） 起，`.dsm` / `.dsf` 由 **legacy 前端**读入，再交给与 `.dss` 相同的 2.0 编译器构建。
本节六页描述的是 1.x `Graph` 语言**今天**被如何构建：1.x 生成器已经删除，它那些静默截断、静默收窄的行为
要么改成了带 `DSHnnnn` 码的错误，要么由编号的 legacy 规则（L19、L22、L26、L27……）照 1.x 的结果构建并
给出提示。想要循环、`?:`、`&&`，请写 [2.0 语言](../language-v2/index.md)；
[`dsc migrate`](../tools/migrate.md) 可以把 1.x 文件改写过去。

<a id="graph-不是声明语言"></a>

## Graph 不是声明语言

`Graph` 外面的东西——`Shader(...)`、`Properties`、`Settings`、`Outputs`、`[元数据]`——由声明解析器处理。
`Graph` 块体则由 2.0 的语句与表达式解析器逐个记号读入，再由一道 legacy 限制检查拒绝 1.x 没有的写法。
两者只是长得像。

| | 声明语言 | Graph 语言 |
| :-- | :-- | :-- |
| 基本单位 | 块、section、键值项 | 以 `;` 结束的语句（`if` 以最后一个 `}` 结束） |
| 关键字 | 块名、section 名 | `if` / `else`；`for`、`return`、`switch` 等 HLSL 关键字能识别，但会被拒绝 |
| 大小写 | section 名与块名各有规则 | 关键字区分大小写；类型 token、构造器、`true` / `false` 不区分；其余名字精确匹配，只差大小写时带警告 `DSH5275` 照用 |
| 字面量 | 声明层的字面量文法 | 2.0 词法器：有后缀、有整数种类；十六进制被拒绝（`DSH2222`） |
| 无法识别的文本 | 会报错 | 同样报错，每种写法一个码 （自 2.0.0 起），见[不支持的写法](unsupported.md) |

一段在 `Properties` 里完全合法的写法，放进 `Graph` 可能含义完全不同。当 `Graph` 块体里出现奇怪行为时，
适用的是这六页的规则，而不是[顶层块](../language/blocks.md)的规则。

<a id="graph-块能出现在哪里"></a>

## `Graph` 块能出现在哪里

| 块 | `Graph` | `Code` |
| :-- | :-- | :-- |
| [`Shader`](../language/blocks.md) | 必须有，除非 `Outputs` section 声明或绑定了东西（否则 `DSH2255`） | 拒绝：`DSH2246` |
| `ShaderFunction` | 可以有；没有时函数体为空 | 拒绝：`DSH2246` |
| `ShaderLayer` / `ShaderLayerBlend` | 可以有；没有时函数体为空 | 拒绝：`DSH2246` |
| `VirtualFunction` | 拒绝：`DSH2247` —— 它声明的是已有资产，不构建新的 | 拒绝：`DSH2247` |
| [`Function` / `GraphFunction`](../language/functions.md) | 函数体是 HLSL，不是 `Graph` 块 | —— |

section 名**不区分大小写**，`{` 前的 `=` 可以省略 （自 1.5.0 起），所以 `Graph = { … }` 和
`Graph { … }` 完全等价。

同一套语句语言还以简化形式用在另外两处：

| 位置 | 形态 |
| :-- | :-- |
| `Outputs` 绑定的右侧 | 一条完整表达式，没有语句 |
| `Outputs` 声明的 `= <default>` | 一条表达式：在第一条 `Graph` 语句之前声明的那个变量的初始值 |

<a id="语法概要"></a>

## 语法概要

```c
Graph [=] {
    <graph-statement> …
}
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

<a id="求值模型"></a>

## 求值模型

1. **legacy 前端把块体读成 2.0 语法树** （自 2.0.0 起）。一个 `Shader` 变成一个材质函数，函数体依次是：
   每个 `Outputs` 声明（带它的初始值，没有就是该类型的零值）、`Graph` 语句、以及作为"写给材质"的赋值的
   `Outputs` 绑定。函数类块的 `Graph` 就是它的 `Inputs` / `Outputs` 所声明的那个函数的函数体。
2. **绑定器先给每个名字和表达式定型**，此时还没有任何节点。1.x 没有的写法、或定不了型的写法会带码中止
   构建——块体永远不会只构建一半。
3. **每条语句自上而下各降级一次。** 没有循环、没有 `return`、没有递归、没有重入。第 *n* 条语句只能看到
   第 1 … *n*−1 条语句产生的值。
4. **每个表达式都是一个有类型的值**：1–4 分量的数（种类为 `float`、`half`、`int`、`uint` 或 `bool`）、
   纹理、采样器、`MaterialAttributes` 值或 `Substrate` 值。变量名就是绑定到这样一个值上的名字。
5. **降级后的图上跑几道 pass**：常量折叠、合并相同节点、剪掉没人读的节点。然后发射器才创建
   `UMaterialExpression` 节点。
6. **`Outputs` 绑定源最后求值**，各自作为一条完整表达式，在块体留下的值上求值。

<a id="由此推出的结论"></a>

## 由此推出的结论

<a id="没有运行时控制流"></a>

### 没有运行时控制流

`if` 在构建期会降级**两个**分支。两边的节点都会留在最终材质里，每个被改动的变量由一个条件节点（`If`；
条件是静态布尔参数时为 `StaticSwitch`）在两者之间选择。`if` 是 select，不是跳转——永远走不到的分支照样
消耗指令。见 [if / else](if.md)。

<a id="声明出来的变量不是存储"></a>

### 声明出来的变量不是存储

给一个名字赋值只是把它重新绑定到另一个节点。原先绑定的节点如果还有别处读取就留在图里，否则被剪掉。
`a = a + 1;` 是重新接线，`a++;` 则是错误 `DSH2207`。

<a id="相同的子表达式会合并"></a>

### 相同的子表达式会合并

去重 pass 会把运算相同、操作数相同的节点合成一个，所以同一个字面量、同一个 `A + B`、同一个
`UE.TexCoord(Index = 0)` 无论写多少遍都只产生**一个**节点。这道 pass 看的是整张图，不按块划分作用域。
（自 2.0.0 起） 起，1.x 从不复用的东西——`Function` 调用、`UE.TexCoord` 这类语法糖内置、
`MaterialAttributes` 节点、`If` 节点——相等时也会合并。见[节点复用](expressions.md#节点复用)。

<a id="有些写法自己不产生节点"></a>

### 有些写法自己不产生节点

分组用的圆括号、`+x`、标量广播都不产生节点；swizzle 的开销见
[Swizzle](expressions.md#swizzle)。

<a id="类型会被检查宽度也是"></a>

### 类型会被检查，宽度也是

两个宽度不同、又都不是标量的操作数是 `DSH4226`；把更宽的值存进更窄的变量会保留前导分量，并给出提示
`DSH5289`；`int`、`uint`、`bool` 和 `half` 对编译器是各自的种类，在图里都是浮点。`7 / 2` 仍是 `3.5`
（legacy 规则 L27）。见[转换](expressions.md#转换)。

<a id="不认识的运算符会报错不再被丢弃"></a>

### 不认识的运算符会报错，不再被丢弃

1.x 遇到 `%`、`&&`、`||`、`?:`、`[ ]`、`<<` 之类会静默结束表达式，于是 `a % b` 构建出的是 `a`。
（自 2.0.0 起） 起每一种都是带码的错误（`DSH2200`–`DSH2222`）。见[不支持的写法](unsupported.md)。

<a id="注释与-region"></a>

## 注释与 `#Region`

注释由词法器读取 （自 2.0.0 起）：`//` 行注释和 `/* */` 块注释。块注释不嵌套，遇到第一个 `*/` 就闭合，
始终不闭合的是 `DSH2102`。字符串字面量里的 `//` 不是注释。每条诊断的行列都是源文件里的位置。

`#Region "Name"` / `#EndRegion` 行把它们之间的语句所产生的节点归入生成图里的一个注释框。每一行单独成行，
是块体里的一条语句 （自 2.0.0 起），可以嵌套。没有名字的 `#Region`、没有打开的区域却写了
`#EndRegion`、块体结束时仍未关闭的区域以及其他 `#` 行是 `DSH2216`–`DSH2219`，预处理器不认识的行是
`DSH1035`。完整语法见 [Layout 与 #Region](../language/layout.md)。

<a id="诊断指向哪里"></a>

## 诊断指向哪里

每条诊断都相对**源文件**报告，指向它所说的那个写法：

```text
<file>(<line>,<column>): DSHnnnn: <message>
```

码是契约，措辞可能会变。行列就是那个写法自己的位置，在 section 里和 `if` 块体里都一样
（自 2.0.0 起）——不再有 `In Graph if body:` 前缀。一个阶段会报告它的全部错误，有错误的阶段会在下一
阶段之前停止构建。码的清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_Arithmetic")
{
    Properties = {
        vec3  A = vec3(1.0, 0.5, 0.2);
        vec3  B = vec3(0.1, 0.2, 0.3);
        float K = 2.0;
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec3 Sum    = A + B;
        vec3 Diff   = A - B;
        vec3 Scaled = A * K;
        vec3 Ratio  = A / K;
        Color = Sum + Diff - Scaled + Ratio;
    }
}
```

生成的节点：

```text
VectorParameter A, VectorParameter B, ScalarParameter K   (property nodes)
Add       (A, B)          -> Sum
Subtract  (A, B)          -> Diff
Multiply  (A, K)          -> Scaled
Divide    (A, K)          -> Ratio
Add       (Sum, Diff)
Subtract  (.., Scaled)
Add       (.., Ratio)     -> Base.EmissiveColor
```

五条语句，七个节点，外加它们读取的三个属性节点。没有控制流，除了写出来的顺序之外没有别的执行顺序，
运行时也不残留任何东西。

<a id="接下来读什么"></a>

## 接下来读什么

- [语句与声明](statements.md)
  - [运算符与转换](expressions.md)
  - [if / else](if.md)
  - [调用](calls.md)
  - [不支持的写法](unsupported.md)
