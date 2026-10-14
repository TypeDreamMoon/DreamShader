---
title: if / else
description: Graph 块里的 if / else 是构建期的 select——两个分支都会被构建，每个改动的变量一个条件节点，而真值形式的语义是 != 0 而不是 > 0。
---

`if` / `else` 是 Graph 语言里唯一的控制流语句，而且它不是跳转。两个分支都在生成阶段降级成节点，两边的节点
全部留在最终材质里，再由**每个被改动的变量一个**条件节点——`UMaterialExpressionIf`，条件是静态布尔参数时为
`UMaterialExpressionStaticSwitch`——在两者之间选择。

<a id="语法概要"></a>

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

<condition> := <expression> [ { >= | <= | == | != | > | < } <expression> ]
```

`else if` 链是合法的：它就是放在 `else` 位置上的一个 `if` 语句，所以 `else if` 链是一棵嵌套语句树，而不是
一个扁平列表。

<a id="要求"></a>

## 要求

| 规则 | 细节 |
| :-- | :-- |
| 条件圆括号 | **必需**——`if x > 0 { }` 是 `DSH2157` |
| 块体花括号 | 两个分支都**必需**——不带花括号的单语句块体是 `DSH2209` |
| 语句结束符 | **不需要**——语句以最后一个块体的 `}` 结束，后面不跟 `;`。`}` 之后多余的 `;` 是空语句 |
| 关键字大小写 | **`if` 和 `else` 区分大小写**，和所有关键字一样。类型 token、构造器和 `true` / `false` 不区分；其他名字写错大小写时带警告 `DSH5275` 照用 |
| 块体内容 | 分支里可以写任何一种语句形式：声明、赋值、`MaterialAttributes` 成员写入、调用语句和嵌套 `if` |

**警告：**

`If (x) {}` 和 `ELSE {}` 不是控制流。因为关键字匹配区分大小写，`If (x)` 是对一个叫 `If` 的函数的调用，本该
放这条语句 `;` 的位置出现了 `{`，于是报 `DSH2154`——一个完全不提 `if` 的码。见
[不支持的写法](unsupported.md#大小写写错的关键字)。

<a id="条件"></a>

## 条件

条件是一条表达式，由表达式解析器读取 （自 2.0.0 起）（1.x 在解析之前先按文本在第一个比较运算符处切开）。
比较运算符不属于普通表达式——这里是它们唯一被接受的地方。

| 运算符 | 含义 |
| :-- | :-- |
| `>` | 左大于右 |
| `<` | 左小于右 |
| `>=` | 左大于等于右 |
| `<=` | 左小于等于右 |
| `==` | 左等于右 |
| `!=` | 左不等于右 |
| *（无）* | **真值**形式——见下文 |

条件是这样读的：

1. 整个条件作为一条表达式解析。缺操作数——`if ()`、`if (Mask >)`——是 `DSH2151` （自 2.0.0 起）（1.x 把
   `if (Mask >)` 读成 `if (Mask != 0)`）。
2. 表达式是比较时，它的两边就是两个操作数；其他任何东西都是**真值**形式。
3. 每个操作数都是 1.x 值表达式：第二个比较、`&&` 或 `||` 是 `DSH2210`，1.x 没有的其他运算符照常被拒绝
   （`DSH2200`–`DSH2222`）。
4. 绑定器给操作数定型：两边都必须是数（纹理、`MaterialAttributes` 或 `Substrate` 值是 `DSH4226`），条件必须
   是单个值——两个向量的比较是 `DSH4260`。

<a id="真值语义"></a>

## 真值语义

**警告：**

**`if (x)` 接的是 `x != 0`，而不是 `x > 0`。** *负值*同样为真：`Height = -1.0` 时 `if (Height)` 走的是
then 分支，和 `Height = 1.0` 时完全一样。

```c
// 写法 —— -1.0 和 1.0 都会走 then 分支。
if (Height) { Color = Hot; } else { Color = Cold; }

// 它实际构建的是：
if (Height != 0) { Color = Hot; } else { Color = Cold; }

// 如果你想要的是符号判断，就明写出来：
if (Height > 0) { Color = Hot; } else { Color = Cold; }
```

这与 HLSL / C 的语义以及反编译器的 `!= 0` 约定一致。只要符号有意义，就把比较显式写出来。

真值条件的右操作数是一个值为 `0` 的 `Constant` 节点。

<a id="-与--会被拒绝"></a>

## `&&` 与 `||` 会被拒绝

**警告：**

（自 2.0.0 起） 起**`&&` 和 `||` 是 `DSH2210`**。1.x 会静默丢掉第一个比较之后的一切，所以
`if (a > 0 && b > 0)` 编译成 `if (a > 0)`，不产生任何诊断；依赖过这种行为的 1.x 源文件现在会报错，而不是
悄悄构建出一半的条件。

```c
// 被拒绝（DSH2210）；1.x 构建的是 if (Mask > 0.5)。
if (Mask > 0.5 && Alpha > 0.5) { Color = Hot; } else { Color = Cold; }

// 请改写成嵌套 if：
if (Mask > 0.5) {
    if (Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
} else {
    Color = Cold;
}
```

条件里的 `%`、`?:`、`[ ]`、`~`、`!` 以及移位和位运算符也一样：各自是带码的错误。见
[不支持的写法](unsupported.md#1x-会截断的运算符)。

<a id="节点接线"></a>

## 节点接线

**每个被合并的变量**产生一个条件节点：

| 条件 | 节点 |
| :-- | :-- |
| 两个标量的比较 | `If`：`A` 接左操作数，`B` 接右操作数 |
| 真值形式，或任何其他单个值 | `If`：`A` 接该值，`B` 接值为 `0` 的 `Constant` |
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

不是 `StaticBoolParameter` 的 `bool` 属性——普通的 `StaticBool` 或 `bool` 条目——是动态参数
（自 2.0.0 起），对它的 `if` 是真值形式的 `If`。见 [Properties 类型](../language/properties.md)。

结果与两个分支值中较宽的那个一样宽，永远不是纹理对象。

<a id="两个分支总是都会构建"></a>

## 两个分支总是都会构建

没有死代码消除，也不会在构建期按条件做决定，哪怕条件是常量。降级一条 `if` 依次做这些事：

1. 降级一次条件。
2. 从 `if` 之前的值出发，降级所有 then 语句。
3. 从同样的值出发，降级所有 else 语句。
4. 合并（见下文）。

实际含义：

- **两个分支里所有为被合并值供值的节点都存在于生成材质里。** 运行时永远走不到的分支照样消耗着色器指令。
  `if` 是 select，不是跳转。
- 任一分支内部的错误都会让构建失败，哪怕那是常量条件下永远到不了的分支。每个错误都报在它自己的行列上。
- 分支里声明的变量只属于该分支 （自 2.0.0 起）：不参与合并，`if` 之后这个名字不存在。见
  [语句与声明](statements.md#作用域)。
- 两个分支里以及合并之后的相同节点由去重 pass 合并。见[节点复用](expressions.md#节点复用)。
- 条件的操作数是共享的，但 **N** 个被合并的变量会产生 **N** 个条件节点。
- 在分支内部首次读取的参数在 `if` 之后再读也是同一个节点 （自 2.0.0 起）（1.x 会留下重复的参数节点）。

<a id="合并"></a>

## 合并

一个变量在 `if` **之前**已声明、且任一分支改动了它时，就会被合并。属性永远不参与合并：它们是输入，在单个
分支里读取不改变任何东西。

| 变量 | 结果 |
| :-- | :-- |
| 两个分支都改动了 | 两个值之上一个条件节点 |
| 只有一个分支改动了 | 改动后的值与 `if` 之前的值之上一个条件节点 |
| 在分支内部声明 | 分支结束即丢弃，不参与合并 |
| `if` 之前没有值、只在一个分支里被赋值（例如还没写过的函数输出） | 被标记；之后读取它是 `DSH4372` |
| `MaterialAttributes` 值 | 逐属性合并：两个分支不一致的每个属性一个条件节点 |

**提示：**

每次赋值都会把值转换到变量的声明类型，所以一个变量的两个分支值类型永远相同——合并本身不做转换。

**警告：**

**分支内的声明留在分支里** （自 2.0.0 起）。1.x 会合并分支里声明的名字，要求另一个分支也有它，并在 `if`
之后保留它。现在在一个分支里声明的 `float3 Temp = …;` 在分支结束后就没了，两个分支里各声明一次的同名变量是
两个不相干的变量。`if` 之后的代码需要它时，请在 `if` **之前**声明：

```c
// 'Blend' 只属于各自分支：if 之后的 Color = Blend; 是 DSH4200。
if (Mask > 0.5) { float3 Blend = float3(1.0, 0.0, 0.0); Color = Blend; }
else            { float  Blend = 0.25;                  Color = float3(Blend, Blend, Blend); }

// 一次声明、一种类型，两个分支里都只是赋值。
float3 Blend = float3(0.0, 0.0, 0.0);
if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
else            { Blend = float3(0.25, 0.25, 0.25); }
Color = Blend;
```

<a id="哪些值不能被选择"></a>

## 哪些值不能被选择

| 值种类 | 行为 |
| :-- | :-- |
| 标量 / 向量，1–4 分量 | 正常选择 |
| `MaterialAttributes` | 逐属性选择；两个分支各留下一个不同的**整个**集合是 `DSH4375` |
| 纹理对象、采样器 | 拒绝——`DSH4379` |
| `Substrate` | 静态条件下为 `StaticSwitch`；否则为 `Substrate.Select` 节点 （自 UE 5.6 起），更早的引擎上是 `DSH4378`。两个值是不同种类的 BSDF 时警告 `DSH4380` |

想按纹理分支，请两张都采样再对采样结果做选择，或者把选择交给作用在采样结果上的 `StaticSwitchParameter`。
见[调用](calls.md#staticswitchparameter)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2157: Expected '(' after 'if', found {1}.` | if 后面没有 (。 | 给条件加括号：if (x) { ... } |
| `DSH2152: Expected ')' to close the 'if' condition, found {1}.` | 条件的 ( 从未闭合。 |  |
| `DSH2151: Expected an expression, found {0}.` | if ()，或缺了一个操作数的条件，例如 if (Mask &gt;)。 |  |
| `DSH2209: Expected braces around the body of a 1.x Graph 'if', found a single statement.` | if 或 else 的块体不带花括号。 | 两个分支的花括号都是必需的，哪怕只有一条语句。 |
| `DSH2150: Unexpected end of file while parsing {0}.` | 某个块体的 { 从未闭合；报告位置是文件末尾。 | 找到缺少 } 的块体。 |
| `DSH2154: Expected ';' after an expression statement, found {1}.` | If、Else 等大小写写错的关键字被当作名字和调用读取。 |  [详解](unsupported.md#大小写写错的关键字) |
| `DSH2210: Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file.` | 条件里有第二个比较，或用了 &amp;&amp; / \|\|。 | 改写成嵌套 if，或先算出组合后的掩码再比较。 |
| `DSH2200: Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file.` | 条件操作数里出现了 ! 或 ?:。 |  |
| `DSH2201: Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file.` | 条件操作数里出现了 %、~、移位或位运算符。 |  |
| `DSH2204: Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file.` | 条件里出现了赋值。 |  |
| `DSH4226: {0} expects {1}, and this is {2}.` | 操作数不是数：纹理、MaterialAttributes 或 Substrate 值。 |  |
| `DSH4260: {0} expects {1}, and this is {2}.` | 条件不是单个真假值——例如两个向量的比较。 | 先 swizzle 成单个通道，或比较由它导出的标量。 |
| `DSH4372: '{0}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'.` | if 之前没有值、只在一个分支里被赋值的变量，在 if 之后被读取。 | 在 if 之前给它一个值，或在两个分支里都赋值。 |
| `DSH4375: The two sides of this branch end with a different whole material, and DreamShader chooses between attribute values, not between whole materials; assign the attributes one at a time on both sides, or mix the two materials with UE.BlendMaterialAttributes.` | 两个分支在同一个变量里留下了不同的整个 MaterialAttributes 集合。 | 在两个分支里逐个写属性；或用 UE.BlendMaterialAttributes 混合两个集合。 |
| `DSH4379: A branch can only choose between numbers, bools and static Substrate values, and these are {0} objects, which no material graph node switches; sample each texture first and branch on the samples.` | 两个分支里的纹理或采样器不同。 | 分别采样，再对采样结果分支。 |
| `DSH4378: A branch chooses between two {0} values or between two numbers, and this one has one of each.` | Substrate 值与数混在一起；或在没有 Substrate.Select 的引擎上做运行时 Substrate 分支。 | 让两边是同一种值；旧引擎上改用静态条件或 lerp。 [详解](../builtins/substrate.md) |
| `DSH4380: This branch becomes a 'Substrate.Select', which parameter-blends its two inputs; they are a '{0}' and a '{1}', and the engine may refuse to blend unlike BSDFs.` | 警告：运行时分支在两个不同种类的 BSDF 之间选择。 | 材质能编译就不用管；引擎拒绝时改用静态条件。 |

<a id="示例"></a>

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

两个 `If` 节点都存在，因为 `else if` 就是一个嵌套 `if`；而且不论 `Mask` 运行时取什么值，每个分支的节点都
在材质里。

<a id="参见"></a>

## 参见

- [语句与声明](statements.md) —— 分支里合法的语句形式与声明作用域
- [运算符与转换](expressions.md) —— 操作数所用的文法，以及赋值处的转换
- [调用](calls.md) —— `StaticSwitchParameter`，`if` 的编译期替代方案
- [不支持的写法](unsupported.md) —— 循环、`switch`、`?:` 及拒绝它们的码
