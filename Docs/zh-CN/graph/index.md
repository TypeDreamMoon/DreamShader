---
title: "Graph"
description: "Graph"
---

<a id="graph"></a>

# Graph

> [DreamShader](../index.md) » **Graph**

块体是一串语句，编译器在**生成阶段按顺序一次性降级**为生成材质中的 `UMaterialExpression` 节点的那个 section。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 位于 `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 内部 |
| 种类 | section |
| 生成 | 目标 `UMaterial` / `UMaterialFunction` 上的 `UMaterialExpression` 节点 |
| section 名 | 按**不区分大小写**匹配；`{` 前的 `=` 可省略 *(自 1.5.0 起)* |

<a id="synopsis"></a>

## 语法概要

```c
Graph [=] {
    <graph-statement> …
}
```

`Graph` 块体并不是嵌入着色器的表达式语言——它是一个*节点构建*程序。其中没有任何东西以代码形式存活到运行时。存活下来的是它构建出的那张图。

要写与 DreamShaderLang 2.0 相同的东西——带循环、`?:` 和 `&&`——请见[2.0 语言](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会为你改写 1.x 文件。

<a id="where-a-graph-block-may-appear"></a>

## `Graph` 块可以出现在哪里

| 块 | `Graph` | `Code` |
| :-- | :-- | :-- |
| [`Shader`](../language/shader.md) | ✔ 必需，除非 `Outputs` section 声明或绑定了东西（[`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)） | ✘ [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) |
| [`ShaderFunction`](../language/shader-function.md) | ✔ —— 没有它块体为空 | ✘ `DSH2246` |
| [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) | ✔ —— 没有它块体为空 | ✘ `DSH2246` |
| [`VirtualFunction`](../language/virtual-function.md) | ✘ [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | ✘ `DSH2247` |
| [`Function` / `GraphFunction`](../language/function.md) | 块体是 HLSL，不是 `Graph` 块 | —— |

> [!WARNING]
> `Code = { … }` 在每个带 `Graph` 的块里都是 `DSH2246`。请使用 `Graph`。

同一套语句语言还以简化形式用在另外两处：

| 位置 | 形态 |
| :-- | :-- |
| `Outputs` 绑定的右侧 | 一条完整表达式，没有语句 |
| `Outputs` 声明的 `= <default>` | 一条表达式：在第一条 `Graph` 语句之前声明的变量的初始值 |

<a id="the-evaluation-model"></a>

## 求值模型

1. **legacy 前端把块体读成 2.0 语法树** *(自 2.0.0 起)*。一个 `Shader` 变成一个材质函数，其函数体依次是：每个 `Outputs` 声明（带它的初始值，没有就是该类型的零值）、`Graph` 语句、以及作为"写给材质"的赋值的 `Outputs` 绑定。函数块的 `Graph` 就是它的 `Inputs` 和 `Outputs` 所声明的那个函数的函数体。
2. **绑定器先给每个名字和表达式定型**，此时还没有任何节点。1.x 没有的写法、或定不了型的写法会带码中止构建——块体永远不会只构建一半。
3. **每条语句自上而下各降级一次。** 没有循环结构，没有 `return`，没有递归，没有重入。第 *n* 条语句只能看到第 `1 … n-1` 条语句产生的值。
4. **每个表达式都是一个有类型的值**：1–4 分量的数（种类为 `float`、`half`、`int`、`uint` 或 `bool`）、纹理、采样器、`MaterialAttributes` 值或 `Substrate` 值。变量名就是绑定到这样一个值上的名字。
5. **降级后的图上跑几道 pass**：常量折叠、合并相同节点、剪掉没人读的节点。然后发射器才创建 `UMaterialExpression` 节点。
6. **`Outputs` 绑定源最后求值**，各自作为一条完整表达式，在块体留下的值上求值。

<a id="consequences-worth-internalising"></a>

### 值得内化的结论

- **没有运行时控制流。** 一个 `if` 在构建期会降级**两个**分支；两边的节点集都会出现在最终材质里，由一个 `If` 节点（条件是静态参数时为 `StaticSwitch`）在每个变量上于两者之间选择。见 [if / else](if.md)。
- **声明出来的变量不是存储。** 给一个名字赋值只是把它重新绑定到另一个节点；原先绑定的节点如果还有别处读取就留在图里，否则被剪掉。
- **相同的子表达式会合并。** 去重 pass 会把运算相同、操作数相同的节点合成一个，所以同一个字面量、同一个 `A + B`、同一个 `UE.TexCoord(Index=0)` 无论写多少遍都只产生**一个**节点。见[节点复用](node-reuse.md)。
- **有些写法自己不产生节点。** 通道选择的开销见 [Swizzle](swizzle.md)。
- **类型会被检查，宽度也是。** 两个宽度不同、又都不是标量的操作数是 [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)；把更宽的值存进更窄的变量会保留前导分量，并给出提示 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)；对编译器而言，`int`、`uint`、`bool` 和 `half` 是各自的种类，在图里都是浮点。见[转换](conversions.md)。
- **不认识的运算符会被拒绝，而不是被丢弃** *(自 2.0.0 起)*。1.x 遇到 `%`、`&&`、`||`、`?:`、`[ ]`、`<<` 之类会静默结束表达式，于是 `a % b` 构建出 `a`；现在每一种都是带码的错误（`DSH2200`–`DSH2222`）。见[不支持的写法](unsupported.md)。

<a id="statements-at-a-glance"></a>

## 语句一览

| 形态 | 示例 | 参考 |
| :-- | :-- | :-- |
| 声明 | `float3 c = A * K;` | [声明](declarations.md) |
| 逗号声明符 | `float a = 1, b, c = 3;` | [声明](declarations.md) |
| 花括号初始化 | `vec4 v = {rgb, 1.0};` | [声明](declarations.md) |
| 赋值 | `Color = Tint;` | [语句](statements.md) |
| 属性成员写入 | `Attrs.BaseColor = Tint;` | [MaterialAttributes](material-attributes.md) |
| 独立调用 | `F_Split(In, OutA, OutB);` | [调用](calls.md) |
| `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | [if / else](if.md) |

带终止规则的完整语法概要表见[语句](statements.md)。

<a id="comments-and-layout"></a>

## 注释与布局

注释由词法器读取 *(自 2.0.0 起)*：`//` 行注释和 `/* */` 块注释。块注释不嵌套，遇到第一个 `*/` 就闭合，始终不闭合的是 [`DSH2102`](../diagnostics/DSH2xxx.md#dsh2102)。字符串字面量里的 `//` 不是注释。每条诊断的行列都是源文件里的位置。

<a id="region--endregion"></a>

## `#Region` / `#EndRegion`

`#Region "Name"` 和 `#EndRegion` 行位于 `Graph` 块体内部，将它们之间的语句所产生的节点归入生成图里的一个注释框。每一行单独成行，是块体里的一条语句*（自 2.0.0 起）*；区域可以嵌套。没有名字的 `#Region`、没有打开的区域却写了 `#EndRegion`、块体结束时仍未关闭的区域以及其他 `#` 行是
[`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216)–[`DSH2219`](../diagnostics/DSH2xxx.md#dsh2219)，预处理器不认识的行是
[`DSH1035`](../diagnostics/DSH1xxx.md#dsh1035)。完整语法与嵌套语义见 [Layout](../language/layout.md)。

<a id="diagnostic-locations"></a>

## 诊断指向哪里

每条诊断都相对**源文件**报告，指向它所说的那个写法：

```text
<file>(<line>,<column>): DSHnnnn: <message>
```

码是契约，措辞可能会变。行列就是那个写法自己的位置，在 section 里和 `if` 块体里都一样*（自 2.0.0 起）*；不再有 `In Graph if body:` 前缀。一个阶段会报告它的全部错误，有错误的阶段会在下一阶段之前停止构建。码的清单见[错误速查](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="DreamShaderTests/Corpus/M_Arithmetic")
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

<a id="pages-in-this-section"></a>

## 本节的页面

| 页面 | 涵盖内容 |
| :-- | :-- |
| [语句](statements.md) | 每种语句形式、结束规则、赋值、表达式语句 |
| [声明](declarations.md) | `T name = expr;`、逗号声明符、花括号初始化器、默认值、作用域 |
| [表达式](expressions.md) | 运算符表、优先级与结合性、一元运算符 |
| [字面量](literals.md) | 数字形式与后缀、`true` / `false`、字符串字面量 |
| [构造器](constructors.md) | 构造器名称、splatting、packing、常量折叠 |
| [Swizzle](swizzle.md) | 通道集合、重排与重复 |
| [转换](conversions.md) | 一个值如何适配它要去的位置 |
| [if / else](if.md) | 条件、比较真值表、分支合并 |
| [MaterialAttributes](material-attributes.md) | 属性值、成员的读取与写入、Substrate 互操作 |
| [调用](calls.md) | 调用 `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction`、参数引脚 |
| [名称解析](name-resolution.md) | 标识符与调用的查找顺序、遮蔽 |
| [节点复用](node-reuse.md) | 相同节点的去重以及用户观察到的现象 |
| [不支持的写法](unsupported.md) | 循环、`return`、三目运算符、`%`、比较、索引——以及拒绝它们的那些码 |

<a id="see-also"></a>

## 另请参阅

- [Shader](../language/shader.md) — 拥有 `Graph` 及其 `Outputs` 的块
- [ShaderFunction](../language/shader-function.md) — 会变成 `UMaterialFunction` 的 `Graph`
- [GraphFunction](../language/graph-function.md) — HLSL 函数体，其中 `UE.*` 调用被提升为引脚
- [类型 token](../language/types.md) — `Graph` 声明接受哪些 token
- [Layout](../language/layout.md) — `#Region` / `#EndRegion` 与节点放置指令
- [UE 内置](../builtins/ue.md) — 可从 `Graph` 调用的完整 `UE.*` 目录
- [数学内置](../builtins/math.md) — `lerp`、`dot`、`saturate`、`frac`、`fmod`、……
- [在 Graph 中读取参数](../../parameters/graph-usage.md) — 裸读取与引脚调用形式
- [Graph 布局](../generation/graph-layout.md) — 生成的节点如何摆放
- [2.0 语言](../language-v2/index.md) — 同一个编译器，带 HLSL 控制流
- [错误速查](../diagnostics/index.md) — 每个码，按阶段分类
