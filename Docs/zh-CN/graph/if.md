---
title: "if / else"
description: "if / else"
---

<a id="if--else"></a>

# if / else

> [DreamShader](../index.md) » [Graph](index.md) » **if**

`Graph` 块里唯一的控制流语句：一个构建期构造，它把**两个**分支都求值成材质节点，并按每个变量
用一个条件节点在两个结果之间选择。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 在 `Graph { … }` 块体内部，包括在另一个 `if` 或 `else` 块体内部 |
| 种类 | 语句 |
| 生成 | **每个变量一个条件节点**，即分支改动的那些变量：`UMaterialExpressionIf`，条件是静态布尔参数时为 `UMaterialExpressionStaticSwitch` |

<a id="synopsis"></a>

## 语法概要

```c
if ( <condition> )
{
    <graph-statement> …
}
[ else
{
    <graph-statement> …
} ]
```

```c
if ( <condition> ) { … }
else if ( <condition> ) { … }
…
[ else { … } ]
```

```c
<condition> := <expression> [ { >= | <= | == | != | > | < } <expression> ]
```

`if`、`else`、`( )`、`{ }` 是 DreamShaderLang 的字面标点。`[ … ]`、`{ a | b }` 和 `…` 是
元记号，永远不会被键入。

<a id="requirements"></a>

## 要求

| 规则 | 细节 |
| :-- | :-- |
| 条件圆括号 | **必需。** `if x > 0 { }` 是 [`DSH2157`](../diagnostics/DSH2xxx.md#dsh2157)。 |
| 块体花括号 | 两个分支都**必需**。不带花括号的单语句块体是 [`DSH2209`](../diagnostics/DSH2xxx.md#dsh2209)。 |
| 语句结束符 | **不需要。** 语句以最后一个块体的 `}` 结束，所以后面不跟 `;`。`}` 之后多余的 `;` 是空语句。 |
| 关键字大小写 | **`if` 和 `else` 区分大小写匹配**，和所有关键字一样。类型 token、构造器和 `true` / `false` 不区分大小写；其他名字写错大小写时带警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。 |
| 块体内容 | 分支里任何一种语句形式都合法：声明、赋值、`MaterialAttributes` 成员写入、独立调用和嵌套 `if` 语句。 |
| `else if` | `else` 位置上的一个 `if` 语句。因此 `else if` 链是一棵嵌套语句树，而不是一个扁平列表。 |

> [!WARNING]
> `If (x) {}` 和 `ELSE {}` **不是**控制流。因为关键字匹配区分大小写，
> `If (x)` 是对一个叫 `If` 的函数的调用，而本该放这条语句 `;` 的位置出现 `{` 时，
> 报 [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) —— 一个完全不提 `if` 的码。见
> [不支持的写法](unsupported.md#wrong-case-keywords)。

<a id="condition"></a>

## 条件

条件是一条表达式，由表达式解析器读取 *（自 2.0.0 起；1.x 在解析任何一边之前先按文本在第一个比较运算符处切开）*。

<a id="operators"></a>

### 运算符

恰好存在六个比较运算符，而且只在这里。

| 运算符 | 含义 |
| :-- | :-- |
| `>` | 左大于右 |
| `<` | 左小于右 |
| `>=` | 左大于等于右 |
| `<=` | 左小于等于右 |
| `==` | 左等于右 |
| `!=` | 左不等于右 |
| *（无）* | **真值**形式——见下文 |

<a id="how-the-condition-is-read"></a>

### 条件是这样读的

1. 整个条件作为一条表达式解析。缺操作数——`if ()`、`if (Mask >)`——是
   [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) *（自 2.0.0 起；1.x 把 `if (Mask >)` 读成
   `if (Mask != 0)`）*。
2. 表达式是比较时，它的两边就是两个操作数。其他任何东西都是**真值**形式。
3. 每个操作数都是 1.x 值表达式：第二个比较、`&&` 或 `||` 是
   [`DSH2210`](../diagnostics/DSH2xxx.md#dsh2210)，1.x 没有的其他运算符照常在其他地方被拒绝（`DSH2200`–`DSH2222`，见
   [不支持的写法](unsupported.md)）。
4. 绑定器给操作数定型：两边都必须是数（纹理、`MaterialAttributes` 或 `Substrate` 值是 [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)），
   条件必须是单个值——两个向量的比较是 [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260)。

> [!WARNING]
> **`&&` 和 `||` 会被拒绝** *（自 2.0.0 起）*。1.x 会静默丢掉第一个
> 比较之后的一切，所以 `if (a > 0 && b > 0)` 编译成 `if (a > 0)`，不产生任何诊断。它现在
> 是 `DSH2210`。
>
> ```c
> // 被拒绝（DSH2210）；1.x 构建的是 if (Mask > 0.5)。
> if (Mask > 0.5 && Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
>
> // 请改写成嵌套 if：
> if (Mask > 0.5) {
>     if (Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
> } else {
>     Color = Cold;
> }
> ```
>
> 条件里的 `%`、`?:`、`[ ]`、`~`、`!` 以及移位和位运算符也一样：
> 各自是带码的错误。见
> [不支持的写法](unsupported.md#operators-1x-truncated)。

<a id="truthy-semantics"></a>

## 真值语义

`if (x)` 接的是 **`x != 0`**，而不是 `x > 0`。

| 形式 | 右操作数 | 走 then 分支的条件 |
| :-- | :-- | :-- |
| `if (x)` | 一个持有 `0` 的 `Constant` 节点 | `x != 0`，即 `x > 0` **或** `x < 0` |

> [!WARNING]
> **负值**为真。`if (Height)` 对 `Height = -1.0` 选择 then 分支，和它对 `Height = 1.0` 时
> 完全一样。这与 HLSL/C 语义以及反编译器的 `!= 0` 约定一致，
> 但它不是 `if (Height > 0)` 的意思。符号有意义时请把比较显式写出来。

<a id="node-wiring"></a>

## 节点接线

每个被合并的变量产生一个条件节点。

| 条件 | 节点 |
| :-- | :-- |
| 两个标量的比较 | `If`：`A` 接左操作数，`B` 接右操作数 |
| 真值形式，或任何其他单个值 | `If`：`A` 接该值，`B` 接一个值为 `0` 的 `Constant` |
| `StaticBoolParameter` 属性，或 `StaticBool` 函数输入 | `StaticSwitch`：`Value` 接参数，`A`（true）接 then 值，`B`（false）接 else 值 |

`If` 的三个结果引脚由分支值接入：

| 运算符 | `AGreaterThanB` | `AEqualsB` | `ALessThanB` |
| :-- | :-- | :-- | :-- |
| `>` | then | else | else |
| `<` | else | else | then |
| `>=` | then | then | else |
| `<=` | else | then | then |
| `==` | else | then | else |
| `!=` | then | else | then |
| 真值 | then | else | then |

不是 `StaticBoolParameter` 的 `bool` 属性——一个普通的 `StaticBool` 或 `bool` 条目——是
动态参数 *（自 2.0.0 起）*，对它的 `if` 是真值形式的 `If`。见
[Properties](../language/properties.md)。

结果与两个分支值中较宽的那个一样宽。它永远不是纹理对象。

<a id="both-branches-are-always-built"></a>

## 两个分支总是都会构建

没有死代码消除，也不会在构建期按条件做决定，哪怕条件是常量。降级一条 `if` 语句依次做
以下所有事：

1. 将条件降级一次。
2. 从 `if` 之前的值出发，降级每一条 then 语句。
3. 从相同的值出发，降级每一条 else 语句。
4. 合并（见下文）。

后果：

- **两个分支中为某个被合并值供值的每个节点都存在于生成的材质中。** 运行时永远不会走到的分支仍然消耗着色器指令。`if` 是 select，不是跳转。
- 任一分支内部的错误都会导致构建失败，哪怕常量条件永远到不了该分支。每个错误都报在它自己的行和列上。
- 在分支内部声明的变量只属于该分支 *(自 2.0.0 起)*：它不参与合并，而且在 `if` 之后这个名字是未知的。见[声明](declarations.md#scope)。
- 两个分支中以及合并之后的相同节点由去重 pass 合并。见[节点复用](node-reuse.md)。
- 条件的操作数是共享的，但 **N** 个被合并的变量会产生 **N** 个条件节点。

<a id="the-merge"></a>

## 合并

一个变量在 `if` **之前**声明，且任一分支改动了它时，就会被合并。属性永远不参与合并：它们是输入，在单个分支里读取它不改变任何东西。

| 变量 | 结果 |
| :-- | :-- |
| 两个分支都改动 | 两个值之上一个条件节点 |
| 只有一个分支改动 | 改动后的值与 `if` 之前的值之上一个条件节点 |
| 在分支内部声明 | 分支结束时丢弃，不合并任何东西 |
| 在 `if` 之前没有值，且只在一个分支里被赋值（例如尚未写入的函数输出） | 被标记；之后读取它是 [`DSH4372`](../diagnostics/DSH4xxx.md#dsh4372) |
| `MaterialAttributes` 值 | 逐属性合并，两个分支不一致的每个属性一个条件节点 |

> [!NOTE]
> 变量的值在每次赋值时都会转换到其声明类型，所以同一个变量的两个分支值类型总是相同。

> [!WARNING]
> **分支内的声明留在其分支中** *(自 2.0.0 起)*。1.x 会合并分支中声明的名字，要求另一个分支也有它，并在 `if` 之后保留它。现在在一个分支中声明的 `float3 Temp = …;` 在该分支结束后就没了，两个分支中声明的同名变量是两个不相干的变量。当之后的代码需要它时，请在 `if` **之前**声明该变量：
>
> ```c
> // 'Blend' 只属于各自分支：if 之后的 Color = Blend; 是 DSH4200。
> if (Mask > 0.5) { float3 Blend = float3(1.0, 0.0, 0.0); Color = Blend; }
> else            { float  Blend = 0.25;                  Color = float3(Blend, Blend, Blend); }
>
> // 一次声明、一种类型，两个分支里都只是赋值。
> float3 Blend = float3(0.0, 0.0, 0.0);
> if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
> else            { Blend = float3(0.25, 0.25, 0.25); }
> Color = Blend;
> ```

<a id="what-cannot-be-selected"></a>

## 哪些值不能被选择

| 值种类 | 行为 |
| :-- | :-- |
| 标量 / 向量（1–4 分量） | 正常选择 |
| `MaterialAttributes` | 逐属性选择；两个分支各留下一个不同的**整个**集合是 [`DSH4375`](../diagnostics/DSH4xxx.md#dsh4375) |
| 纹理对象、采样器 | 拒绝——[`DSH4379`](../diagnostics/DSH4xxx.md#dsh4379) |
| `Substrate` | 静态条件下为 `StaticSwitch`；否则为 `Substrate.Select` 节点（UE 5.6+），更早的引擎上是 [`DSH4378`](../diagnostics/DSH4xxx.md#dsh4378)。两个值是不同种类的 BSDF 时，[`DSH4380`](../diagnostics/DSH4xxx.md#dsh4380) 会发出警告 |

<a id="diagnostics"></a>

## 诊断

<a id="statement-shape"></a>

### 语句形态

| 条件 | 代码 |
| :-- | :-- |
| `if` 之后没有 `(` | `DSH2157` |
| 条件的 `(` 从未闭合 | [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) |
| `if ()`，或缺了一个操作数的条件 | `DSH2151` |
| `if` 或 `else` 之后不带花括号的块体 | `DSH2209` |
| 块体的 `{` 从未闭合 | [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150)，报告位置是文件末尾 |
| `If`、`Else` 及其他大小写写错的关键字 | `DSH2154` |

<a id="condition-1"></a>

### 条件

| 条件 | 代码 |
| :-- | :-- |
| 第二个比较、`&&` 或 `\|\|` | `DSH2210` |
| `!`、`?:` | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) |
| `%`、`~`、移位或位运算符 | [`DSH2201`](../diagnostics/DSH2xxx.md#dsh2201) |
| 条件里出现赋值 | [`DSH2204`](../diagnostics/DSH2xxx.md#dsh2204) |
| 不是数的操作数 | `DSH4226` |
| 两个向量的比较 | `DSH4260` |

<a id="merge"></a>

### 合并

| 条件 | 代码 |
| :-- | :-- |
| 在 `if` 之前没有值、只在一个分支里被赋值的变量，在其之后被读取 | `DSH4372` |
| 两个分支在同一个变量里留下不同的整个 `MaterialAttributes` 集合 | `DSH4375` |
| 两个分支里的纹理或采样器不同 | `DSH4379` |
| `Substrate` 值与数混在一起，或在没有 `Substrate.Select` 的引擎上做运行时 `Substrate` 分支 | `DSH4378` |
| *警告：* `Substrate.Select` 混合了两种不同种类的 BSDF | `DSH4380` |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_IfElse")
{
    Properties {
        float Mask = 0.6;
        vec3  Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        if (Mask > 0.5) {
            Color = Tint;
        } else if (Mask > 0.25) {
            Color = Tint * 0.5;
        } else {
            Color = vec3(0.0, 0.0, 0.0);
        }
    }
}
```

生成的节点：

```text
VectorParameter  Tint
ScalarParameter  Mask
Constant         0.5                 (shared)
Constant         0.25
Multiply         Tint * 0.5
Constant3Vector  (0,0,0)             (folded literal)
If               A=Mask B=0.25  ->   inner else-if merge of 'Color'
If               A=Mask B=0.5   ->   outer merge of 'Color'
```

两个 `If` 节点都存在，因为 `else if` 是一个嵌套的 `if` 语句，而且不论 `Mask` 运行时取什么值，每个分支的节点都在材质里。

<a id="see-also"></a>

## 参见

- [语句](statements.md) —— 每一种语句形式，包括分支里合法的那些
- [表达式](expressions.md) —— 解析条件操作数所用的运算符文法
- [不支持的写法](unsupported.md) —— 循环、`switch`、`?:`，以及拒绝它们的代码
- [声明](declarations.md) —— 声明作用域与重复声明规则
- [转换](conversions.md) —— 每次赋值所应用的转换
- [节点复用](node-reuse.md) —— 为什么分支子表达式会折叠成共享节点
- [MaterialAttributes](material-attributes.md) —— 用 `if` 选择属性值
- [名称解析](name-resolution.md) —— 为什么分支内部读取的参数不是分支输出
- [调用](calls.md) —— 从分支内部调用函数
- [诊断索引](../diagnostics/index.md) —— 按阶段排列的每个代码
