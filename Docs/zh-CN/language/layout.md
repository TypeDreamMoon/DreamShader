---
title: "布局"
description: "布局"
---

<a id="layout"></a>

# 布局

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **布局**

一个用于固定生成节点位置并在生成的材质图中声明注释框的 section。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf` — 位于 `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` 内 |
| 种类 | section |
| 生成 | 节点位置，以及每条 `Comment` 语句对应一个 `UMaterialExpressionComment` |
| 不接受于 | `VirtualFunction` |

*(自 2.0.0 起)* 由哪一趟处理读取该 section 取决于工程的
[图布局风格](../generation/graph-layout.md#layout-styles)。`Blocks`（默认）以及在编译器 IR 上计算的其他风格，会在各自的
排布之上服从每一条 `Node` 和 `Comment` 语句。`Classic` 是 1.x 的布局，按 1.x 的方式使用该
section，即
[显式布局与自动布局](#explicit-layout-versus-automatic-layout) 所描述的方式。`.dss` 写出的语句与 `#pragma layout(Node, …)` / `#pragma layout(Comment, …)` 相同 —— 见
[DreamShaderLang 2.0](../language-v2/index.md#declarations)。

<a id="synopsis"></a>

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

语句是调用，每条后面可选跟一个 `;`。语句名和所有参数键都按大小写不敏感匹配。`{ … }` 块之前的 `=` 是可选的 *(自 1.5.0 起)*。

<a id="node"></a>

## `Node`

把一个已经生成出来的表达式钉到确切位置上。

| 参数 | 必需 | 类型 | 默认值 |
| :-- | :-- | :-- | :-- |
| **`Var`** | 是 | 文本 | — |
| **`X`** | 是 | 整数 | — |
| **`Y`** | 是 | 整数 | — |

`Var` 指的是源码里的一个值。会被记录的名字有三类：

| 名字类别 | 记录时机 |
| :-- | :-- |
| `Graph` 语句的目标变量 | 该语句产出了节点 —— 声明的局部变量和被赋值的输出变量都算 |
| [`Properties`](properties.md) 声明名 | 图确实读取了该属性 |
| `Inputs` / `Outputs` 声明名 | 仅限 `ShaderFunction`、`ShaderLayer` 和 `ShaderLayerBlend`，其中每一条都会变成 `FunctionInput` / `FunctionOutput` 节点 |

`MaterialExpressionEditorX/Y` 和编辑器图节点的 `NodePosX/Y` 都会被写入，所以位置在重新打开材质后依然保留。

> [!NOTE]
> 匹配不到任何东西的 `Var` 会被**静默忽略** —— 无论是拼写错误，还是钉了一个图从未读取过的属性，
> 都没有诊断。

<a id="comment"></a>

## `Comment`

在确切矩形上创建一个注释框。

| 参数 | 必需 | 类型 | 作用 |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | 文本 | 框标题 —— 见[说明](#notes)。 |
| **`X`** | 是 | 整数 | 左边缘。 |
| **`Y`** | 是 | 整数 | 上边缘。 |
| **`W`** | 是 | 整数 | 宽度；`Classic` 下至少 `120`，IR 风格下至少 `64`。 |
| **`H`** | 是 | 整数 | 高度；`Classic` 下至少 `80`，IR 风格下至少 `64`。 |
| `Color` | 否 | `float4` 字面量 | 框颜色；不写就用布局自己的默认色。 |

> [!WARNING]
> `W` 和 `H` 是**必填参数**。`Comment(Name="X", X=0, Y=0)` 是
> [`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275)，参数缺失和格式错误都报这个码。

`Color` 字面量遵循通用的向量字面量语法：`(` 之前的 token 被忽略，所以
`float4(…)`、`vec4(…)` 和 `(…)` 都能解析。一个分量铺成 `(x, x, x, 1)`，两个给
`(x, y, 0, 0)`，三个给 `(x, y, z, 1)`，第四个之后的分量被忽略。其他写法是
[`DSH3276`](../diagnostics/DSH3xxx.md#dsh3276)。

生成的注释框始终使用 `FontSize = 24` 和分组模式，所以拖动框会带动它圈住的节点。

<a id="argument-parsing"></a>

## 参数解析

| 规则 | 细节 |
| :-- | :-- |
| 语句形态 | `<Name>( <key> = <value> [, <key> = <value> ]… )` —— 其他写法是 [`DSH3274`](../diagnostics/DSH3xxx.md#dsh3274) |
| 语句名 | 一个标识符，按大小写不敏感与 `Node` 和 `Comment` 比较；其他名字是 `DSH3274` |
| 键匹配 | 大小写不敏感 |
| 值处理 | 单个带引号的字符串取它的文本；其他取 token 的文本，整数参数必须读成可选正负号加数字 |
| 重复键 | `DSH3274` —— 不同于 `Settings`，那里是后写的键胜出 |
| 空值 | `DSH3274` |
| 必需参数缺失或为空、坐标不是整数 | `DSH3275` |
| 注释 | 任意两个 token 之间都可以 |

既不是必需参数也不是 `Color` 的参数会交给布局，由它带警告 [`DSH7230`](../diagnostics/DSH7xxx.md#dsh7230) 忽略；`Node` 上的 `Color` 也一样。

<a id="coordinate-space"></a>

## 坐标系

位置是 Unreal 材质图编辑器坐标：**X 向右增大，Y 向下增大**，单位和编辑器节点位置相同。

手工摆放节点时，几个 `Classic` 常量可以作参照：

| 位置 | X |
| :-- | :-- |
| 输出列：材质旁边最宽的节点，以及 `FunctionOutput` 节点 | `900` |
| 输出绑定的 reroute 使用点（构造时） | `720` |
| 材质根节点 | 图边界右侧 `520` |

各列从输出列向左排，所以负 X 是“上游”，材质根节点位于所有节点右侧。完整表格以及 IR 风格自己的间距见
[图布局](../generation/graph-layout.md#layout-constants)。*(自 2.0.0 起)* 1.x 按节点种类给的构造位置（属性节点 `-800`、`Expression( … )` 目标 `1200` 等）已经没有了：
每个节点都在同一列创建，再由布局摆放。

<a id="explicit-layout-versus-automatic-layout"></a>

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

> [!NOTE]
> **布局总是会运行** *(自 2.0.0 起)*，Ephemeral 材质和保存了的材质一样；唯一的例外是 `Classic`
> 风格的大图保护。见
> [图布局](../generation/graph-layout.md#when-layout-runs)。

> [!NOTE]
> 第二个 `Layout` 区段会**替换**第一个，而不是追加，并带有警告
> [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) *（自 2.0.0 起）*。一个块体里只有最后一个
> `Layout` 块有效。

<a id="region--endregion"></a>

## `#Region` / `#EndRegion`

区域指令位于 **`Graph` 函数体文本**中，而不是在 `Layout` 中。它们为一个图语句范围命名；布局会把每个区域变成一个注释框。

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
| 识别 | `#` 行中紧随 `#` 之后的单词为 `Region` 或 `EndRegion`，按大小写不敏感匹配。`.dsm` / `.dsf` 中的另一个 `#` 行会先被[预处理器](preprocessor.md#what-is-not-a-directive)拒绝（`DSH1035`）；若某行到达 1.x 的 `Graph` 函数体（例如 `#pragma` 在 `.dsh` 中那样），则为 [`DSH2219`](../diagnostics/DSH2xxx.md#dsh2219) |
| 名称 | 该行的其余部分，去除引号并修剪空白；在 `#Region` 上为必需项 —— [`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216) |
| 嵌套 | 区域可以嵌套；在没有打开区域时出现 `#EndRegion` 属于 [`DSH2217`](../diagnostics/DSH2xxx.md#dsh2217)，在函数体末尾仍未关闭的区域属于 [`DSH2218`](../diagnostics/DSH2xxx.md#dsh2218) |
| 位置 | 指令与其他任何 token 一样，因此每条诊断都保留其真实的行和列 |

在 `Blocks` 下，区域是独立的框，而包含区域的区域则是套在它们外围的框。在 `Classic` 下，区域内的语句会用它生成的变量打上区域名标签；在自动路径上，每个区域成为一个注释框，而在显式路径上，区域名贡献用于决定在何处插入跨块重路由的块边界。

> [!NOTE]
> [预处理器](preprocessor.md)会先于解析器看到每一行 `#` *（自 1.9.0 起）*，并明确将 `region` / `endregion` 命名为要透传的行 —— 无论如何，都与此处匹配的方式完全一致。因此 `#Region`、`#region` 和 `#REGION` 都会原样到达解析器，区域指令不受条件编译影响。

> [!NOTE]
> `#Region` 名称和 `Layout` `Comment` 名称相互独立。声明一个矩形恰好包含某区域节点的 `Comment` 并不会将两者合并。

<a id="notes"></a>

## 说明

- **注释标题。** 在 `Classic` 下，每个框的标题都是 `DreamShader: <Name>`，因此
  `Comment(Name="Sampling", …)` 读作 `DreamShader: Sampling`。在 IR 风格下，`Comment` 保持
  其书写时的标题，而样式为区域和块生成的框则会带有该前缀。该
  前缀标记一个框是编译器自身的；见[重新生成](../generation/regeneration.md)。
- `Comment` 的 `Name` 为空或空白时属于 `DSH3275`，因此永远不会为它创建任何框。
- [反编译器](../tools/decompiler.md)会将位置和自由注释框写回为
  `#pragma layout` 行，或写为带 `-Format Legacy` 的 `Layout` 块，因此材质可以导出、
  编辑并重新生成，且位置保持不变。是否输出由 **Export Decompiled
  Layout** 项目设置控制，默认开启。见[项目设置](../settings/project.md)。
- `Classic` 自动布局会在非常大的图上放弃 —— 在达到或超过 1200 个表达式时，它会记录
  `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.`
  并保留构建时的位置。`Layout` 块是控制这些
  图的方式。见[图布局](../generation/graph-layout.md)。
- `Layout` 在 `VirtualFunction` 内不被接受
  （[`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245)）；该块不生成图。

<a id="diagnostics"></a>

## 诊断

每条诊断都带有它所涉及语句或指令的行和列。

<a id="layout-statements"></a>

### `Layout` 语句

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3274` | 区段不是以 `{` 打开；语句不是 `<Name>( … )`；参数不是 `Key = Value`；键被写了两次；未闭合的 `(`；名称不是 `Node` 或 `Comment` |
| `DSH3275` | 必需参数缺失或为空，或坐标不是整数 |
| `DSH3276` | `Color` 不是向量字面量 |
| `DSH2258` | 警告：第二个 `Layout` 区段 |
| `DSH7230` | 警告：布局用不到的参数，已被忽略 |

<a id="region-directives"></a>

### `#Region` 指令

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH2216` | `#Region` 没有名称 |
| `DSH2217` | `#EndRegion` 时没有打开 `#Region` |
| `DSH2218` | 区域在 `Graph` 函数体末尾仍未关闭 |
| `DSH2219` | 到达 `Graph` 函数体的另一行 `#` |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

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

生成的图，在 `Classic` 下：

```text
Comment      "DreamShader: Emissive"   at (-1300, -260)  size 1100 x 520
Tint         VectorParameter           at (-1200, -160)
Intensity    ScalarParameter           at (-1200,   60)
Boosted      Multiply                  at ( -800, -160)
Color        Add                       at ( -400,  -60)
DS_EmissiveColor_<n>  NamedReroute     positioned by propagation
```

<a id="see-also"></a>

## 另请参阅

- [Shader](shader.md) —— 其图由 `Layout` 确定位置的块
- [ShaderFunction](shader-function.md) —— 材质函数块也接受 `Layout`
- [Graph](../graph/index.md) —— 编写 `#Region` 指令的位置
- [预处理器](preprocessor.md) —— 另一种 `#` 语法，以及如何区分两者
- [声明](../graph/declarations.md) —— 哪些语句会注册可命名的变量
- [属性](properties.md) —— 属性名也是有效的 `Node` `Var` 目标
- [输出绑定](output-bindings.md) —— 为每个绑定创建的重路由对
- [图布局](../generation/graph-layout.md) —— 布局样式及其块、常量和限制
- [重新生成](../generation/regeneration.md) —— `DreamShader: ` 注释前缀规则
- [临时材质](../generation/ephemeral.md) —— 仅存在于内存中的材质
- [反编译器](../tools/decompiler.md) —— 从现有材质往返导出位置
- [项目设置](../settings/project.md) —— **Export Decompiled Layout**
- [诊断索引](../diagnostics/index.md) —— 每个代码
