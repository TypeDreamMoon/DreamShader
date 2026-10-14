---
title: Layout 与 #Region
description: 用 Layout = { Node(...); Comment(...); } 固定生成节点的位置，用 #Region 给图语句分组，以及它们在各种布局风格下的效果。
---

`Layout` 固定生成节点的位置并声明注释框。`#Region` 给图语句分组，好让布局有东西可以圈。两者都不会改变
着色器数学。

它们写在不同的地方：`Layout` 是和 `Graph` 并列的 section；`#Region` 指令写在 `Graph` 函数体**内部**。

读这个 section 的是哪一趟布局，由工程设置 **Graph Layout Style** 决定 （自 2.0.0 起）（见
[DreamShaderLang 2.0](../language-v2/index.md#region-与布局)）。默认的 `Blocks` 以及其他基于编译器 IR 的风格，在自己的
排布之上服从每一条 `Node` 和 `Comment`；`Classic` 是 1.x 的布局，按 1.x 的方式使用这个 section —— 即下面
[显式布局与自动布局](#显式布局与自动布局) 所描述的。`.dss` 把同样的语句写成 `#pragma layout(Node, …)` /
`#pragma layout(Comment, …)`。

<a id="语法概览"></a>

## 语法概览

```c
Layout [=]
{
    Node( Var = "<variable>", X = <int>, Y = <int> ) ;
    Comment( Name = "<title>", X = <int>, Y = <int>, W = <int>, H = <int>
             [, Color = float4( <r>, <g>, <b>, <a> )] ) ;
    …
}
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

语句是调用，每条后面的 `;` 可选。语句名和参数键都按大小写不敏感匹配，块前的 `=` 是可选的
（自 1.5.0 起）。`Layout` 在 `Shader`、`ShaderFunction`、`ShaderLayer` 和 `ShaderLayerBlend` 中被
接受，在 `VirtualFunction` 中**不被**接受（`DSH2245`）—— 那个块不生成图。

**警告：**

每个参数都是 `Key = Value` 对。位置式写法 `Node("Tint", X=0, Y=0)` **不被**接受，是 `DSH3274`。名字参数在
`Node` 上叫 `Var`，在 `Comment` 上叫 `Name`。

<a id="node"></a>

## `Node`

把一个已经生成出来的表达式钉到确切位置上。

| 参数 | 必需 | 类型 |
| :-- | :-- | :-- |
| `Var` | 是 | 文本 |
| `X` | 是 | 整数 |
| `Y` | 是 | 整数 |

`Var` 指的是源码里的一个值。会被记录的名字有三类：

| 名字类别 | 记录时机 |
| :-- | :-- |
| `Graph` 语句的目标变量 | 该语句产出了节点 —— 声明的局部变量和被赋值的输出变量都算 |
| `Properties` 声明名 | 图**确实读取**了这个属性 |
| `Inputs` / `Outputs` 声明名 | 仅限 `ShaderFunction`、`ShaderLayer` 和 `ShaderLayerBlend`，那里每一条都会变成 `FunctionInput` / `FunctionOutput` 节点 |

`MaterialExpressionEditorX/Y` 和编辑器图节点的 `NodePosX/Y` 都会被写入，所以位置在重新打开材质后依然
保留。

**提示：**

匹配不到任何东西的 `Var` 会被**静默忽略** —— 无论是拼写错误，还是钉了一个图从没读过的属性，都没有诊断。

<a id="comment"></a>

## `Comment`

在确切矩形上创建一个注释框。

| 参数 | 必需 | 类型 | 作用 |
| :-- | :-- | :-- | :-- |
| `Name` | 是 | 文本 | 框标题 —— 见[说明](#说明)。 |
| `X` | 是 | 整数 | 左边缘。 |
| `Y` | 是 | 整数 | 上边缘。 |
| `W` | 是 | 整数 | 宽度；`Classic` 下至少 `120`，IR 风格下至少 `64`。 |
| `H` | 是 | 整数 | 高度；`Classic` 下至少 `80`，IR 风格下至少 `64`。 |
| `Color` | 否 | `float4` 字面量 | 框颜色；不写就用布局自己的默认色。 |

**警告：**

`W` 和 `H` 是**必填参数**。`Comment(Name="X", X=0, Y=0)` 是 `DSH3275` —— 参数缺失和格式错误报的是同一个码。

`Color` 字面量遵循通用的向量字面量语法：`(` 之前的 token 被忽略，所以 `float4(…)`、`vec4(…)` 和 `(…)`
都能解析。一个分量铺成 `(x, x, x, 1)`，两个给 `(x, y, 0, 0)`，三个给 `(x, y, z, 1)`，第四个之后的分量
被忽略。其他写法是 `DSH3276`。

生成的注释框始终使用 `FontSize = 24` 和分组模式，所以拖动框会带动它圈住的节点。

<a id="参数解析"></a>

## 参数解析

| 规则 | 细节 |
| :-- | :-- |
| 语句形态 | `<Name>( <key> = <value> [, <key> = <value> ]… )` —— 其他写法是 `DSH3274` |
| 语句名 | 一个标识符，按大小写不敏感与 `Node`、`Comment` 比较；其他名字是 `DSH3274` |
| 键匹配 | 大小写不敏感 |
| 值处理 | 单个带引号的字符串取它的文本；其他取 token 的文本，整数参数必须读成可选正负号加数字 |
| 重复键 | `DSH3274` —— 不同于 `Settings`，那里是后写的键胜出 |
| 空值 | `DSH3274` |
| 必需参数缺失或为空、坐标不是整数 | `DSH3275` |
| 注释 | 任意两个 token 之间都可以 |

既不是必需参数也不是 `Color` 的参数会交给布局，由它带警告 `DSH7230` 忽略；`Node` 上的 `Color` 也一样。

<a id="坐标系"></a>

## 坐标系

位置是 Unreal 材质图编辑器坐标：**X 向右增大，Y 向下增大**，单位和编辑器节点位置相同。

手工摆放节点时，几个 `Classic` 常量可以作参照：

| 位置 | X |
| :-- | :-- |
| 输出列：材质旁边最宽的节点，以及 `FunctionOutput` 节点 | `900` |
| 输出绑定的 reroute 使用点（构造时） | `720` |
| 材质根节点 | 图边界右侧 `520` |

各列从输出列向左排，所以负 X 是"上游"，材质根节点位于所有节点右侧。1.x 按节点种类给的构造位置（属性节点
`-800`、`Expression( … )` 目标 `1200`…）已经没有了 （自 2.0.0 起）：每个节点都在同一列创建，再由布局摆放。

<a id="显式布局与自动布局"></a>

## 显式布局与自动布局

在 `Classic` 下，`Layout` 块不是在自动布局之上叠加 —— 它**替换**掉那套排序算法。

| 条件 | 结果 |
| :-- | :-- |
| 至少一条 `Node` 匹配到了被记录的变量，**或者**至少声明了一条 `Comment` | 走显式布局 |
| 有 `Layout` 块，但没有 `Node` 匹配上、也没有 `Comment` | 自动布局照常运行，就当这个块不存在 |
| 没有 `Layout` 块 | 自动布局运行 |

在显式路径上，块里没点名的表达式仍然会被摆放：位置从已经定位的邻居迭代传播 —— 位于已知消费者和已知
依赖的中点、已知消费者左边 `360`、或已知依赖右边 `360` —— 重合槽位有扇出避让。仍然没被摆放的，会进入
所有已定位节点左侧的兜底列。

在 IR 风格下，被钉住的节点移到指定位置，风格摆在它周围的节点跟着一起移动。

**提示：**

**布局总是会运行** （自 2.0.0 起），Ephemeral（内存中的）材质和保存了的材质一样；唯一的例外是 `Classic`
风格的大图保护（见下）。1.9.x 及以前在内存模式下会完全跳过布局。

**提示：**

第二个 `Layout` section 会**替换**第一个，而不是追加，并给出警告 `DSH2258` （自 2.0.0 起）。一个块体里只有
最后一个 `Layout` 有效。

<a id="region--endregion"></a>

## `#Region` / `#EndRegion`

Region 指令住在 **`Graph` 函数体文本**里，不在 `Layout` 里。它们给一段图语句命名，布局把每个 region 变成一个
注释框。

```c
Graph = {
    #Region "Surface"
    Color = BaseColor.rgb;
    Rough = Roughness;
    #EndRegion

    #Region "Emissive"
    Glow = Tint * Intensity;
    #EndRegion
}
```

| 规则 | 细节 |
| :-- | :-- |
| 识别 | `#` 后面紧跟的单词是 `Region` 或 `EndRegion`，大小写不敏感。`.dsm` / `.dsf` 里其他 `#` 行先被预处理器拒绝（`DSH1035`）；到达 1.x `Graph` 体的其他 `#` 行（`.dsh` 里的 `#pragma` 可能走到这里）是 `DSH2219` |
| 名字 | 本行剩余部分，去引号并去空白；`#Region` 上是**必需**的 —— 否则 `DSH2216` |
| 嵌套 | region 可以嵌套；没有打开的 region 时出现 `#EndRegion` 是 `DSH2217`，`Graph` 体结束时还没闭合的 region 是 `DSH2218` |
| 位置 | 指令和其他 token 一样，诊断保留真实的行列 |

在 `Blocks` 下，每个 region 是一个独立的框，含有 region 的 region 是套在它们外面的框。在 `Classic` 下，region
内部的语句会给它产出的变量打上 region 名标签：自动布局路径上每个 region 变成一个注释框；显式路径上 region 名
参与决定跨块 reroute 插在哪里的块边界。

**提示：**

[预处理器](lexical.md#预处理器) 比解析器先看到每一个 `#` 行 （自 1.9.0 起），它明确把
`region` / `endregion`（任意大小写）当作要透传的行，所以 `#Region`、`#region`、`#REGION` 都原样到达解析器，
region 指令不受条件编译影响。

`#Region` 名和 `Layout` 的 `Comment` 名互相独立。声明一个矩形恰好圈住某个 region 节点的 `Comment` 并不会把两者
合并。

<a id="说明"></a>

## 说明

- **注释标题。** 在 `Classic` 下每个框都显示为 `DreamShader: <Name>`，所以 `Comment(Name="Sampling", …)`
  读作 `DreamShader: Sampling`。在 IR 风格下 `Comment` 保留原样的标题，风格为 region 和块生成的框才带这个前缀。
  这个前缀标记"这是编译器自己的框"；见 [重新生成](../generation/regeneration.md)。
- `Name` 为空或只有空白的 `Comment` 是 `DSH3275`，所以永远不会为它创建框。
- [反编译器](../tools/decompiler.md) 把节点位置和自由注释框写回成 `#pragma layout` 行，或用 `-Format Legacy`
  写成 `Layout` 块，所以材质可以导出、编辑、再生成而保住位置。是否输出由 **Export Decompiled Layout**
  项目设置控制，默认开启。
- `Classic` 的自动布局会在超大图上放弃：表达式数量达到或超过 1200 时它会记录
  `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.`
  并保留构建时的位置。`Layout` 块正是控制这类图的手段。

<a id="示例"></a>

## 示例

```c
Shader(Name="Materials/M_Layout", Root="Game")
{
    Properties {
        VectorParameter Tint      = float4(0.4, 0.8, 1.0, 1.0);
        ScalarParameter Intensity = 2.0 [Slider(0, 10)];
    }

    Outputs {
        float3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        #Region "Emissive"
        float3 Boosted = Tint.rgb * Intensity;
        Color = Boosted + vec3(0.05, 0.05, 0.05);
        #EndRegion
    }

    Layout {
        Comment(Name="Emissive", X=-1300, Y=-260, W=1100, H=520,
                Color=float4(0.10, 0.22, 0.16, 0.35));
        Node(Var="Tint",      X=-1200, Y=-160);
        Node(Var="Intensity", X=-1200, Y=  60);
        Node(Var="Boosted",   X= -800, Y=-160);
        Node(Var="Color",     X= -400, Y= -60);
    }
}
```

`Classic` 下生成的图：

```text
Comment               "DreamShader: Emissive"   at (-1300, -260)  size 1100 x 520
Tint                  VectorParameter           at (-1200, -160)
Intensity             ScalarParameter           at (-1200,   60)
Boosted               Multiply                  at ( -800, -160)
Color                 Add                       at ( -400,  -60)
DS_EmissiveColor_<n>  NamedReroute              positioned by propagation
```

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3274: Expected 'Node(...)' or 'Comment(...)' in the Layout section, found {0}.` | Layout 里出现了 Node(...) / Comment(...) 之外的东西：section 没用 { 打开、语句不是 &lt;Name&gt;( … )、参数不是 Key = Value（老的位置式 Node("Tint", …) 也在其中）、重复的键、空值、没闭合的 (。 | 写成 Node(Var = "Name", X = 0, Y = 0); 或 Comment(Name = "Title", X = 0, Y = 0, W = 400, H = 200);。 |
| `DSH3275: Expected the argument '{0}' in '{1}(...)', found none.` | 缺了必需参数（Node 的 Var、X、Y；Comment 的 Name、X、Y、W、H），或者为空，或者坐标不是整数 —— Comment 的 W 和 H 最常触发。 | 补上参数；位置和尺寸是整数。 |
| `DSH3276: Expected 'Color' to be a vector literal such as '(0.1, 0.16, 0.22, 0.35)', found '{0}'.` | Comment 的 Color 不是向量字面量。 | 写成 Color = (0.1, 0.16, 0.22, 0.35)。 |
| `DSH2258: The section '{0}' is written twice; the later one wins, as it did in 1.x.` | 同一个块里有第二个 Layout section（警告）。 | 把两个合并成一个。 [详解](sections.md) |
| `DSH7230: '#pragma layout' expects a whole number for '{0}'; '{1}' was ignored.` | 布局用不上的参数（包括 Node 上的 Color）被忽略（警告）。 | 删掉这个参数。 |
| `DSH2216: Expected a name after '#Region', found none.` | #Region 后面什么都没写。 | 写成 #Region "Name"。 |
| `DSH2217: Expected a '#Region' before this '#EndRegion', found none open.` | 不成对的 #EndRegion。 | 删掉它，或补上它本该闭合的 #Region。 |
| `DSH2218: Expected '#EndRegion' to close the region '{0}' before the end of the Graph body, found none.` | Graph 体结束时还有未闭合的 region。 | 在 Graph 体的 } 之前用 #EndRegion 闭合。 |
| `DSH2219: Expected '#Region' or '#EndRegion' as the only '#' line in a 1.x Graph body, found '#{0}'.` | 1.x Graph 体里出现了 #Region / #EndRegion 之外的 # 行 —— 通常是 .dsh 里的 #pragma 或 #include。 | import 写在文件顶部；#pragma 属于 .dss。 |

<a id="继续阅读"></a>

## 继续阅读

- [Section](sections.md) — 为什么第二个 Layout 会替换第一个。
  - [DreamShaderLang 2.0](../language-v2/index.md) — 布局风格与 #pragma layout。
  - [重新生成](../generation/regeneration.md) — 重建时什么能幸存，以及 DreamShader: 注释规则。
  - [反编译导出](../tools/decompiler.md) — 把已有材质的布局反向导出。
