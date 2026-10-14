---
title: "节点复用"
description: "节点复用"
---

<a id="node-reuse"></a>

# 节点复用

> [DreamShader](../index.md) » [图形](index.md) » **节点复用**

公共子表达式消除:两个节点若从相同输入计算出相同结果,在生成的资产中就是同一个节点。

| | |
| :-- | :-- |
| 声明位置 | 并非由作者编写——适用于每个 `Graph { … }` 主体、`Outputs` 绑定表达式和 `Outputs` 声明默认值 |
| 类型 | 构建时行为 |
| 生成 | 无;它*防止*重复的 `UMaterialExpression` 节点 |

*(自 2.0.0 起)* 复用是对编译器中间图形(IR)执行的一次遍历,在整个资产构建完成后运行一次,而不是 1.x 生成器在构建节点时查询的缓存。规则相同——两个具有相同类和相同参数的节点是同一个节点——但它适用于每个节点,因此若干 1.x 从不共享的结构现在会被共享(参见[永不合并的内容](#what-is-never-merged))。

<a id="the-dedupe-pass"></a>

## 去重遍历

遍历运行在**一个**生成资产的图形上:一个 `Shader`,或一个 `ShaderFunction` /
`ShaderLayer` / `ShaderLayerBlend`。一个声明了材质和两个函数资产的 `.dsm` 有三个独立的图形,它们之间不共享任何内容。

| 顺序 | 遍历 | 作用 |
| --: | :-- | :-- |
| 1 | 剪枝 | 丢弃没有任何读取的节点,在合并任何内容之前执行 |
| 2 | 折叠 | 操作数全为常量的运算符、内置函数、构造函数或 swizzle 变为一个常量 |
| 3 | swizzle | 掩码的掩码在保持升序时变为一个掩码;恒等掩码不产生节点(1.x 源除外,参见 [Swizzle](swizzle.md#lowering)) |
| 4 | 去重 | 合并相等的节点 |
| 5 | 剪枝 | 丢弃不再被任何输出到达的内容;没有任何读取的参数是 [`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390) 信息 |

第 2–4 遍会重复执行,最多四次,直到某一轮不再改变任何内容:一次折叠可以使两个节点变得相等,而一次合并可以把两个相等的常量放到折叠器面前。

该遍历**不**局限于某个块。它一次性看到整个图形,因此写在 then 分支中、又写在 else 分支中的子表达式,或者在 `if` 之后写的子表达式,都是同一个节点。

<a id="what-is-merged"></a>

## 合并的内容

当两个节点的每一部分都相等时,它们就是同一个节点:

| 部分 | 比较内容 |
| :-- | :-- |
| 操作 | 节点是什么:运算符、内置函数、常量、参数、反射的 `UE.*` / `Substrate.*` 节点、Custom 节点、函数调用 |
| 类 | 反射节点的表达式类、Custom 节点或调用的函数 |
| 输出 | 数量、名称和类型——布尔常量和浮点常量不同 |
| 属性 | 每个属性,作为名称和值的集合;数字精确到 9 位有效数字 |
| 操作数 | 按顺序,因此 `a * b` 和 `b * a` 是两个节点 |
| 输入引脚 | 作为引脚和值的集合,因此参数的书写顺序无关紧要 |

操作数和引脚通过它们读取的节点进行比较。该遍历从前到后工作,在合并节点本身之前先合并其输入,因此相等性是结构性的:当两个节点以相同方式读取相等的节点时,它们就是相等的。

<a id="values-not-text"></a>

## 是值,不是文本

比较基于节点,绝不基于源文本。变量代表它持有的节点,因此:

- 在两个文本上相同的表达式之间重新赋值变量会产生两个不同的节点;
- 当 `UV3` 持有的节点与 `UV1` 相同时,`UV1.x * 2.0` 和 `UV3.x * 2.0` 是同一个节点;
- 构建相同节点的两种拼写是同一个节点:`F_Tint(a, b)` 和
  `F_Tint(Color = a, Tint = b)`,`1` 和 `1.0`。

大小写不同的名称是同一个声明(伴随警告
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)),因此 `F_Tint(Color = a)` 和 `F_Tint(color = a)` 也是同一个节点。

<a id="what-is-never-merged"></a>

## 永不合并的内容

| 结构 | 原因 |
| :-- | :-- |
| 语句:材质的输出、函数的输出、诸如 `UE.DreamPassOutput` 的自定义输出节点 | 它是资产中的一个位置,而不是一个值 |
| 函数输入(`FunctionInput`) | 它是资产签名的一部分 |
| 在某个属性、引脚或操作数上不同的两个节点 | `UE.TexCoord(Index = 0)` 和 `UE.TexCoord(Index = 1)`;参数的引脚调用与其裸读取 |
| 参数列表与另一个 `Outputs` 表达式目标哪怕相差一个参数 | 参数列表就是节点的标识——当它们意在成为同一个节点时,把引脚写在同一个[块](../language/output-bindings.md#block-form)中 |

其他所有内容在相等时都会被合并 *(自 2.0.0 起)*。这包括 1.x 从不共享的内容:

| 结构 | 1.x | 现在 |
| :-- | :-- | :-- |
| `Function` 和 `GraphFunction` 调用 | 每个调用点一个全新的 `Custom` 节点 | 每个不同的参数列表一个节点 |
| Custom 类的 `UE.Expression` | 从不共享 | 当代码和输入相等时共享 |
| `UE.TexCoord`、`UE.Time`、`UE.Panner`、`UE.TransformVector`、`UE.TransformPosition` 以及无参数状态读取 | 每个调用点一个节点 | 每个不同的参数列表一个节点 |
| `UE.CollectionParam` / `UE.CollectionParameter` | 每个调用点一个节点 | 每个集合和参数一个节点 |
| `MakeMaterialAttributes`、`SetMaterialAttributes` 以及属性读取 | 每个声明、写入和读取一个节点 | 相等时共享 |
| `If` | 每个合并变量每个 `if` 一个节点 | 当条件和两个值都相等时共享 |
| `StaticSwitchParameter` 调用 | 从不共享 | 每对不同的分支一个节点 |
| 参数和属性节点 | 每个作用域一次,在 `if` 之后再次出现 | 每个参数和参数列表一个节点——见下文 |
| Swizzle | 无节点:每个连接上一个掩码 | 每个源和掩码一个 `ComponentMask`,或者没有——参见 [Swizzle](swizzle.md) |

<a id="parameters-and-properties"></a>

### 参数和属性

标量、向量、纹理或静态布尔属性是材质参数,对它的每次读取都是该参数的节点:相等的节点,因此资产中是一个节点。*(自 2.0.0 起)* 1.x 的重复已消失——一个参数在 `if` 分支内首次读取、之后再次读取,是同一个节点。

类型为参数节点(`TextureSampleParameter2D`、`ChannelMaskParameter`……)的属性在每次使用时都以其声明的默认值和元数据写出;具有相同参数的使用是一个节点,而具有不同引脚的[引脚调用](calls.md#input-pin-wiring)是另一个节点,它指向同一个材质参数。

> [!NOTE]
> `Outputs` 表达式目标——`Expression( … ).Pin[i]` 和
> [块形式](../language/output-bindings.md#block-form)——是在读取 `Outputs` 部分时分组,而不是由本遍历分组:每个类和参数列表一致的绑定(参数名忽略
> 大小写、顺序任意、排除引脚索引)填充一个节点的不同引脚,而绑定两次的引脚是
> [`DSH3268`](../diagnostics/DSH3xxx.md#dsh3268)。这正是参数列表的确切拼写至关重要的原因:多一个或少一个参数就是第二个节点,悄无声息地。块形式的存在是为了消除让这种错误变得容易的重复——它只写一次规范,并降低为 N 个不会产生分歧的绑定。

<a id="observable-consequences"></a>

## 可观察的后果

- 一个资产中同一数值的每一次出现都是 **一个** `Constant` 节点：`1` 与 `1.0`、
  `0.5` 与 `.5`。常量比较精确到 9 位有效数字。
- 一个取反 `-x` 是 `Multiply(x, Constant(-1))`。同一个 `-x` 写两次是一个 `Multiply`；两个
  不同的取反各自拥有自己的 `Constant(-1)` *（自 2.0.0 起；1.x 共用同一个）*。一个
  取反常量的写法（`-0.5`）会折叠为一个 `Constant`。
- `A + B` 写两次会产出一个 `Add` 节点——前提是两个操作数都是相同的节点，且通过
  相同的输出读取。
- `UE.TexCoord(Index = 0)` 写五次会产出 **一个** `TextureCoordinate` 节点 *（自 2.0.0 起；
  1.x 会产出五个）*。
- `Src.rgb` 使用十次是一个 `ComponentMask`，或者当 `Src` 具有 `RGB` 输出
  引脚时完全不产生节点。
- `F_Tint(a, b)` 与 `F_Tint(b, a)` 是不同的节点；`F_Tint(Color = a)` 与 `F_Tint(color = a)`
  是同一个节点。
- 由于该遍处理会跨越 `if` 分支，在两个分支中写入的共享子表达式只构建一次，
  且 `If` 的两个输入都读取它。
- 节点 **布局** 在合并后的图上计算，因此合并后的节点只有一个位置，不会留下空隙。
  请参阅[图布局](../generation/graph-layout.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Reuse")
{
    Properties { vec3 Tint = vec3(1.0, 0.5, 0.2); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec2 UV1 = UE.TexCoord(Index = 0);
        vec2 UV2 = UE.TexCoord(Index = 0);   // the same node as UV1
        vec2 UV3 = UV1;                      // a plain alias: no node at all

        float A = UV1.x * 2.0;
        float B = UV3.x * 2.0;               // same node as A
        float C = 2.0 * UV1.x;               // a different node: operands swapped

        Color = Tint * (A + B + C);
    }
}
```

生成的节点：

```text
TextureCoordinate  Index=0        -> UV1, UV2, UV3
ComponentMask      R              -> UV1.x, UV3.x
Constant           2.0            -> shared by all three multiplications
Multiply           UV1.x * 2.0    -> A, B
Multiply           2.0 * UV1.x    -> C
Add                A + B          (A and B are the same value, so this is Add(m, m))
Add                (A+B) + C
VectorParameter    Tint
Multiply           Tint * (...)
```

七条语句产生九个节点 *（自 2.0.0 起）*：两个 `TexCoord` 调用合并为一个节点，而 `.x`
swizzle 无论被读取多少次都是一个 `ComponentMask`。

<a id="see-also"></a>

## 另请参阅

- [表达式](expressions.md) —— 其结果会被合并的运算符
- [Swizzle](swizzle.md) —— swizzle 的代价，以及 1.x 源所保留的恒等掩码
- [构造函数](constructors.md) —— 常量折叠，另一个减少节点数的机制
- [字面量](literals.md) —— 数值字面量如何变成常量
- [调用](calls.md) —— 调用节点及其参数列表
- [`if` / `else`](if.md) —— 为什么复用不以分支为作用域
- [MaterialAttributes](material-attributes.md) —— Make/Set/读取节点
- [图中的参数](../../parameters/graph-usage.md) —— 参数节点是如何生成的
- [图布局](../generation/graph-layout.md) —— 保留下来的节点被放置在何处
- [UE.Expression](../builtins/ue-expression.md) —— `Class=` 参数以及节点的属性
