---
title: "图形布局"
description: "图形布局"
---

<a id="graph-layout"></a>

# 图形布局

> [DreamShader](../index.md) » [生成](index.md) » **图形布局**

该阶段为刚刚完成编译所创建的节点分配编辑器坐标和注释框。

| | |
| :-- | :-- |
| 运行于 | `UMaterial`（包括隐藏的 ThinCustom 基类）和 `UMaterialFunction` |
| 运行时机 | 在图形构建完成且输出连接之后；在材质重新编译之前 |
| 源代码控制 | 可选的 [`Layout`](../language/layout.md) 段以及 `#Region` / `#EndRegion` |
| 产出 | 节点位置、`UMaterialExpressionComment` 框、命名重路由（`Blocks` 下的 `DS_<variable>`，`Classic` 下的 `DS_Shared_*`） |

具体是哪个阶段，由项目的 [图形布局样式](#layout-styles) 决定。`Blocks`（默认样式）以及在编译器 IR 上计算的另外两种样式，都在该标题下描述。**布局运行时机**、**显式布局**及其后的各节描述 `Classic`，即 1.x 阶段，它作用于已完成的图形，项目仍可选择使用。

<a id="when-layout-runs"></a>

## 布局运行时机

*仅 `Classic`。* 在 IR 上计算的样式没有此保护：它在节点存在之前就已布局，并且始终运行。

| 条件 | 行为 |
| :-- | :-- |
| 图形中表达式少于 2 个 | 跳过 |
| 表达式达到 **1200 个或更多**，且没有 `Layout` 段 | 跳过，并输出一条日志 |
| 存在 `Layout` 段 | 显式路径 |
| 其他情况 | 自动路径 |

大型图形跳过时记录以下日志：

```text
Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions
will be used.
```

该阈值针对的是已完成图形的表达式数量，而不是源代码长度。它仅在自动路径上检查——显式的 `Layout` 段在任何节点数量下都会被遵循。

> [!NOTE]
> **自 `2.0.0` 起，布局始终运行。** 它过去可以被跳过：在[预览](../../tools/preview.md)中或尚未实体化的材质上看到的图形是**未布局**的——节点停留在构建阶段留下的高耸单列中——只有在资产被实体化、烘焙或由命令行工具生成之后，布局才会出现。曾添加了 *Lay Out In-Memory Graphs* 项目设置以重新开启布局，默认开启；`2.0.0` 移除了该设置，也随之移除了跳过行为。唯一剩下的退出方式是上文的大型图形保护。参见[临时材质](ephemeral.md)和[项目设置](../settings/project.md)。
>
> *（自 2.0.0 起）* 该阶段在每次编译时为每个节点报告一个慢任务帧。在 1.x 中，交互式编译会以*静默*方式运行它，每个块一帧；2.0 的发射器不使用该模式。

<a id="layout-styles"></a>

## 布局样式

*（自 2.0.0 起）* **项目设置 ▸ DreamShader ▸ 图形布局样式**选择用于布局生成图形的样式。`Blocks` 为默认样式。图形会在下次重建其源代码时采用所选样式；节点位置不属于[差异摘要](divergence.md)的一部分，因此以其他样式构建的资产永远不会被读取为手工编辑。

| 样式 | 读取 | 简述 |
| :-- | :-- | :-- |
| `Blocks` *（默认）* | 编译器的 IR | 按源代码顺序排列的一页框，每个 `#pragma region` 或语句序列一个框；没有连线离开框——见下文 |
| `Classic` | 已完成的引擎图形 | 1.x 布局：从材质的输出恢复出的块，它们之间由 `DS_Shared_*` 命名重路由连接 |
| `SourceBands` | 编译器的 IR | 源代码每条语句一个水平带，按文本顺序从上到下排列；在每个带内，是一棵从语句的值向左生长的整洁树 |
| `Layered` | 编译器的 IR | 整个图形按距输出的距离分层（Sugiyama 算法）：通过重心排序减少交叉，行对齐到其邻居的中位数 |

<a id="blocks"></a>

### 块

这是像人手工布局那样绘制图形的方式：每个步骤一个框，框按阅读顺序排列，只要一个值从一个框传递到另一个框，就使用命名重路由。

- **一个框是一个区域，或一段语句。** 某个最内层 `#pragma region`（1.x 中的 `Region`）的连续语句组成一个框，以该区域的名称作为标题；不在任何区域内的语句被切成最多四十个节点的段，以它们定义的第一个和最后一个变量命名。包含其他区域的区域是一个包围其内部框的框，因此嵌套框永远不会重叠。函数的输出是它们自己的一个框，位于页面的右侧。
- **在一个框内**，节点是一幅小型的分层绘图，从左到右排列。
- **没有连线离开框。** 另一个框读取的值会在其旁边得到一个命名重路由声明——`DS_<variable>`，或对于没有变量命名的值使用 `DS_Shared_<n>`——并在读取它的每个框中有一个使用。常量不会被重路由：它在被读取的地方重新写入。驱动材质的值或函数输出，通过在任何样式中都立于每个输出前方的 `DS_<output>` 对离开其框；使用项立在材质节点旁边的一列中。
- **页面**将框像段落中的单词一样打包——从左到右，然后换到下一行——按源代码顺序排列，页面宽度约为高度的 1.8 倍。

重路由和重复的常量是生成图形的节点，正如 `Classic` 的 `DS_Shared_*` 重路由那样：反编译器会透过它们查看，[图形一致性比较器](../contributing/testing.md)也是如此。

<a id="what-the-ir-styles-share"></a>

### IR 样式的共同点

- **列就是距离。** 节点的列是它到某个根的最长路径——材质的输出、函数的输出、语句节点；在 `Blocks` 中，则是到其框内没有其他东西读取它的位置——因此每条连线都从左到右运行，从不向后。
- **`#pragma layout` 被精确地遵循到单位。** `Node` 提示会将命名值的节点移动到给定位置，并携带挂在它上面的节点；`Comment` 提示会按其描述的那个框原样传递。
- **`#pragma region` 是其节点周围的一个框**，嵌套区域位于其父区域内，每个都有一定圈的填充，以免标题发生碰撞。
- **`SourceBands` 和 `Layered` 不插入重路由。** 跨越六列或更多列的连线是一条*长边*：它会被计入 `dump-layout` JSON，并保留为连线。`Blocks` 是进行重路由的样式。
- **IR 不知道的节点会被堆叠在其所有者旁边**：一个 IR 节点可以变成多个引擎表达式（带遮罩的参数、带坐标的采样），额外的那些会被放置在 IR 值所在列左侧的一列中。
- **确定性。** 相同的 IR 在任何机器上都会给出相同的坐标；一切都对齐到 16 单位的网格。

选择之前先看看——这会为一个源代码绘制每种 IR 样式，不构建任何东西，只需一秒钟：

```powershell
./dsc.ps1 dump-layout DShader/Materials/M_Foo.dss        # …/Saved/DreamShader/Layout/**.svg, one per product and style
./dsc.ps1 dump-layout -All -Style Blocks -Json
```

图中的尺寸是根据 IR 估算的（节点的引脚和标题）；在实际编译中会使用引擎自己的节点尺寸，因此间距会略有不同，而排列不会。

<a id="explicit-layout"></a>

## 显式布局

一个 `Layout` 段固定命名变量并声明注释框。完整的语法、参数表和解析诊断位于[布局](../language/layout.md)页面；本节介绍放置阶段如何处理它们。

```c
Layout = {
    Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400);
    Node(Var="UV", X=-1100, Y=-120);
}
```

当 `Layout` 块与生成的变量匹配了至少一个 `Node(Var=…)`，**或者**声明了至少一个 `Comment(…)` 时，会采用显式路径。然后，按顺序：

1. **固定节点。** 每个 `Node(Var=…)` 都精确写入其表达式的编辑器 X/Y 坐标，并在节点存在时写入图表节点的位置。未命名任何生成变量的 `Var` 会被静默忽略。
2. **注释框。** 每个 `Comment(…)` 会在字面量 X/Y 处创建一个 `UMaterialExpressionComment`，并使用给定的 W/H。空白或仅含空白字符的 `Name` 会被跳过。
3. **未固定节点** 通过从其已定位的相邻节点进行迭代传播来放置。
4. **重路由** 会为跨越块边界的*远距离*连接插入。
5. **材质根节点** 会被放置在其所消费的输出附近，然后馈送输出的重路由用法会在 `Material Output` 框下重新分组。

<a id="comment-box-properties"></a>

### 注释框属性

| 属性 | 显式 `Comment(…)` | 自动框 |
| :-- | :-- | :-- |
| 文本 | `DreamShader: ` + `Name` | `DreamShader: ` + 块标题 |
| 位置 | 字面量 `X`，`Y` | 块边界减去内边距 (110, 90) |
| 尺寸 | `max(120, W)` × `max(80, H)` | `max(420, width + 220)` × `max(240, height + 180)` |
| 颜色 | 给定时的 `Color=float4(…)`，否则为 `(0.10, 0.16, 0.22, 0.35)` | `(0.10, 0.16, 0.22, 0.35)` |
| 字体大小 | 24 | 24 |
| 分组模式 | 开启 | 开启 |
| 注释气泡 | 在细节面板中可见，带颜色 | 在细节面板中可见，带颜色 |

> [!WARNING]
> DreamShader 创建的每个注释框，其文本都以字面量 `DreamShader: ` 为前缀，
> 而这个前缀正是重新生成时删除的内容。写入 `Comment(Name="Sampling", …)` 会产生一个
> 读取为 `DreamShader: Sampling` 的框；在材质编辑器中将其重命名为不带
> 前缀的内容，可使其在下次重建后保留——并且其旁边会出现第二个框。参见
> [重新生成](regeneration.md#what-survives)。

<a id="placing-the-unpinned-nodes"></a>

### 放置未固定节点

传播会持续运行，直到不再有移动，上限为 `max(32, pending count)` 次遍历。

| 已知相邻节点 | 位置 |
| :-- | :-- |
| 依赖项和使用者两者 | 它们之间的中点 |
| 仅使用者 | `consumerX - (nodeWidth + 140)`，相同 Y |
| 仅依赖项 | `dependencyX + 360`，相同 Y |
| 两者都没有，从未有 | 回退列 |

冲突通过将位置分桶到 80 × 80 的槽位来解决；一个槽位的第 *n* 个占用者
在垂直方向上偏移 `±((n + 1) / 2) * (nodeHeight + 40)`，符号交替——即节点自身的
[估算高度](#node-footprints)，因此同一槽位中的两个节点无论是什么都能相互避开。

任何仍未放置的内容都会进入 `bounds.MinX - 480` 处的回退列（当没有任何内容
被定位时为 `-1200`），起始于 `bounds.MaxY + 240`（或 `-620`），并按每个节点的高度
加 40 逐步增大 Y。

<a id="automatic-layout"></a>

## 自动布局

在没有 `Layout` 部分的情况下，该遍历会将图表划分为**块**，独立地布局每个块，
并将块向下堆叠。

<a id="block-construction"></a>

### 块构建

块按此顺序构建，然后按排序键进行稳定排序，升序。

| 顺序 | 块 | 标题 | 排序键 |
| :-- | :-- | :-- | :-- |
| 1 | 每个不同的 `#Region` 名称一个 | 区域名称 | 区域顺序 |
| 2 | *（材质）* 每个**已连接**的材质属性输入一个 | `Output: <PropertyName>` | 材质属性枚举索引 |
| 3 | *（材质函数）* 每个 `UMaterialExpressionFunctionOutput` 一个 | `Output: <OutputName>`，或未命名时为 `Output: FunctionOutput` | `1000 + declaration order` |
| 4 | 每个没有使用者且尚未是接收器的表达式 | `Generated Outputs` | `100000` |
| 5 | *（仅当以上内容都未产生块时）* 所有内容 | `Graph` | `0` |
| 6 | 被多个块使用且没有块拥有的表达式 | `Shared Inputs` | `-1000`，最先插入 |

恰好出现在一个块中的表达式会立即分配给该块。有歧义的表达式在最多 16 次遍历中通过
“哪个块已经拥有我最多的直接使用者”来解决；剩余项采用其第一个候选。每个 `NamedRerouteDeclaration` 最终都会被强制放入其
来源的块中。

<a id="cross-block-reroutes"></a>

### 跨块重路由

任何跨越块边界的输入都会通过一个共享的命名重路由重新布线。

| 节点 | 名称 | 位置 |
| :-- | :-- | :-- |
| `UMaterialExpressionNamedRerouteDeclaration` | `DS_Shared_<n>`，`n` 从 0 开始计数 | `source.X + 360`，`source.Y` |
| `UMaterialExpressionNamedRerouteUsage` | 每个（声明，使用者块）对一个 | `consumer.X - 360`，按 `±((slot + 1) / 2) * 80` 在垂直方向上展开 |

输入通道掩码在重新布线过程中得到保留。在自动路径上，每个交叉连接都会被桥接；
在显式路径上，仅**远距离**连接会被桥接，其中远距离意味着 `|ΔX| ≥ 900` 或
`|ΔY| ≥ 540`。显式路径还会在插入任何内容之前提前退出，除非它构建了至少两个块
并为至少两个表达式分配了所有者。

<a id="node-footprints"></a>

### 节点占用空间

以下每个步骤都使用真实节点尺寸，而不是扁平的占用空间。生成时没有可测量的
控件——命令行工具没有编辑器 UI，即使在编辑器中，图表的
节点控件也尚不存在——因此尺寸是根据 `SGraphNodeMaterialBase`
所组装的内容*估算*出来的。

| 项 | 贡献 |
| :-- | :-- |
| 标题栏 | `46`，加上来自 `GetCaption` 的每条额外说明行 `20` |
| 引脚行 | `28 × max(inputCount, outputCount)` |
| 表达式预览 | `124` 当 `UMaterialExpression::ShouldShowPreview()` —— 即 `!bHidePreviewWindow && !bCollapsed` |
| 主体内边距 | `22` |
| 宽度 | `max(48 + captionChars × 10, 56 + (inputNameChars + outputNameChars) × 8)`，限制在 `[140, 520]` |

输出引脚名称只有在设置了 `bShowOutputNameOnPin` 时才计入宽度。此估算
故意偏高：高估只会让图表更宽松，而低估会使节点
重叠。测量结果会在一次布局遍历期间进行记忆化。

这就是防止 `TextureSample`——一个因预览缩略图而高度约为 `Constant` 两倍的
节点——被堆叠在其相邻节点之上，并且使注释框足够大，
能够实际容纳它们所圈出的内容的原因。

<a id="placement-inside-a-block"></a>

### 块内放置

一种分层的、Sugiyama 风格布局。

1. 按每个表达式到其消费者最长路径进行**分级**：汇点为等级 0，
   `rank(e) = max(rank(consumer) + 1)`。循环会解析为等级 0。
2. 为每个跨越一个以上等级的边分配**通道**。从等级 `r` 到等级 `r + n` 的边会得到
   `n - 1` 个占位槽位，每跨过一列一个，并在其端点之间链接。没有它们，
   步骤 3 对长边就是盲的 —— 它只会比较相隔一个等级的相邻节点 —— 这正是
   即使每一列本身都整齐，图形仍然读起来像一团线的原因。一条通道
   预留 `34` 的垂直空间，这样线路就有了走廊，而不是横穿任何
   恰好挡路的东西。
3. **减少交叉**：6 遍，每遍先朝输出方向扫描，按每一列消费者的平均位置对每一列排序，
   然后朝输入方向扫描，按其依赖项排序。
   通道与真实节点同等参与。重心在整列排序之前
   就为整列计算，因此排序不会取决于排序如何配对元素；并列时按创建顺序
   决定。
4. 按真实高度**堆叠**每一列，使用 `60` 的间隙，并以块的中线居中。
5. **拉直**：4 遍，交替方向。每遍把每个槽位朝已固定一侧相邻槽位的平均中心拉，
   然后用保序回归修复该列 ——
   即仍然保持该列顺序及其最小间隙的最近中心集合。这
   正是把链变成一条直的水平线而不是阶梯的原因。
6. **分配 X**：一列的宽度与其最宽节点相同，节点在列内**右对齐**，
   这样一列的输出会对齐，通往下一列的连线也会保持简短。列 `0` 的最宽
   节点其左边缘位于 `X = 900`；每一后续列位于前一列左边缘
   左侧 `150` 处。

<a id="packing-the-blocks"></a>

### 打包块

块过去堆叠在一列中，这会把一个有多个连接输出的材质变成一条
高达数万单位的带子 —— 只有在完全缩小时才可读。现在它们会先围绕
自己的原点布局，然后被打包进列中。

| | |
| :-- | :-- |
| 列高度预算 | `max(2400, sqrt(totalBlockArea / 1.6))` |
| 一列中的下一个块 | 前一个块的 `bounds.MaxY + 420` |
| 下一列 | 前一列最宽的块 `+ 520` |
| 第一个块 | `Y = -620` |

当添加一个块会超出预算时，它会被换到新的一列，除非该列为
空 —— 一个比整个预算还高的块会独占一列，而不是被拆分。若
块足够少、能放入一列，结果就是旧的自上而下堆叠。

注释框只在打包之后绘制：在移动之前放置的框会被它本应包含的
节点留在原地。

<a id="the-material-root"></a>

### 材质根节点

有两遍按顺序处理根节点：

1. `RootX = combinedBounds.MaxX + 520`，`RootY = (MinY + MaxY) / 2 - 240`。材质图形的根
   节点位置也会被写入。
2. 随后，为输出供给的命名重路由**用法**会从它们的计算块中抽出，
   并堆叠在 `bodyBounds.MaxX + 420` 处的单列中，间距为最高用法节点的高度
   `+ 40`（绝不少于 130）；根节点再向右移动 `widestUsage + 360`；并且
   整个组会获得自己的注释框，标题为 `Material Output`。根节点按自身 `420` 高度
   在该列中居中，而不是固定的 240。

先把这些用法从计算块中移除，正是保持各属性框紧凑的原因。

<a id="layout-constants"></a>

## 布局常量

| 常量 | 值 | 适用于 |
| :-- | :-- | :-- |
| 输出列 X | `900` | 最宽等级 0 节点的左边缘，以及 `FunctionOutput` 节点 |
| 列间隙 | `150` | 两列之间的间隔 —— 线路走廊 |
| 行间隙 | `60` | 一列中两个堆叠节点之间的间隔 |
| 通道高度 | `34` | 一条长边每跨过一列所预留的垂直空间 |
| 块间隙 Y | `420` | 同一列中两个块之间的间隔 |
| 块间隙 X | `520` | 两个块列之间的间隔 |
| 第一个块顶部 | `-620` | 第一个块的 Y |
| 列高度预算 | `max(2400, sqrt(area / 1.6))` | 块打包 |
| 节点占用 | 按节点估算，宽度 `[140, 520]` | 边界与间距 |
| 回退占用 | `320 × 150` | 测量空表达式 |
| 根节点占用 | `320 × 420` | 材质结果节点 |
| 注释填充 | `110` X，`90` Y | 自动框 |
| 最小自动框 | `420 × 240` | 自动框 |
| 最小显式框 | `120 × 80` | `Comment(…)` |
| 重路由偏移 | `360` | 声明位于源右侧，用法位于消费者左侧 |
| 重路由扇出 | `±((slot + 1) / 2) * 80` | 堆叠用法 |
| 根节点间隔 | `520` | 根节点位于图形边界右侧 |
| 输出用法列 | `bodyBounds.MaxX + 420`，间距 `max(130, tallest + 40)`，根节点 `+ widest + 360` | `Material Output` 组 |
| 远距阈值 | `|ΔX| ≥ 900` 或 `|ΔY| ≥ 540` | 仅显式路径重路由 |
| 减少交叉遍数 | `6` | 每块 |
| 拉直遍数 | `4` | 每块 |
| 所有者分配遍数 | `≤ 16` | 有歧义的表达式 |
| 传播遍数 | `max(32, pending)` | 显式路径，未固定节点 |
| 大图形阈值 | `1200` | 自动布局退出 |

> [!NOTE]
> 这些常量不属于稳定接口的一部分。生成的资源布局可能在任何
> 版本中改变；`.dsm` 中没有任何内容依赖它们，除非通过 `Layout` 节，其固定绝对
> 坐标，因此不受影响。

<a id="output-block-titles"></a>

## `Output:` 块标题

材质属性块标题来自一张固定表。下面的每个名称都会产生
`Output: <name>`；未列出的任何内容都会产生 `Output: MaterialProperty<enum index>`。

`BaseColor`，`MaterialAttributes`，`EmissiveColor`，`Opacity`，`OpacityMask`，`Metallic`，
`Specular`，`Roughness`，`Normal`，`AmbientOcclusion`，`Refraction`，`WorldPositionOffset`，
`PixelDepthOffset`，`SubsurfaceColor`，`CustomData0`，`CustomData1`，`DiffuseColor`，
`SpecularColor`，`SurfaceThickness`，`Displacement`，`CustomizedUV0`，`CustomizedUV1`，
`CustomizedUV2`，`CustomizedUV3`，`CustomizedUV4`，`CustomizedUV5`，`CustomizedUV6`，
`CustomizedUV7`，`Anisotropy`，`Tangent`。

定义了 `MOON_ENGINE` 的自定义引擎构建会额外增加五个：
`MoonEncodedAttribute0` … `MoonEncodedAttribute4`（重命名前的 `MooaEncodedAttribute…` 拼写是
可接受的别名）。标准 UE 5.3 – 5.8 不会。

<a id="construction-time-positions"></a>

## 构建时位置

这些是布局被跳过时节点所携带的坐标 —— 少于两个节点的图形，或者在 `Classic` 下具有 1200 个或更多节点且没有 `Layout` 节的图形。注意其形状：X 固定，Y 只
按节点逐个向下步进，因此被跳过的图形是一根高列，线路横穿其上，而不是糟糕的布局。

| 节点 | X | 首个 Y | Y 步进 |
| :-- | :-- | :-- | :-- |
| 发射器创建的每个节点，按其创建顺序 | `0` | `-120` | `+180` |

*（自 2.0.0 起）* 1.x 的按种类位置表 —— 整个表面的 `Custom` 节点、
`MakeMaterialAttributes` 种子、属性节点、`FunctionInput` / `FunctionOutput` —— 随着 1.x
生成器一起消失了：发射器以相同方式放置它创建的每个节点，而布局遍会重写它们。

每个 `Outputs` 绑定在构造时也会创建一个命名重路由对：声明位于
`source.X + 420`、`source.Y`，使用位于 `X = 720`、`Y = -120 + max(routeIndex, 0) * 180`。
声明的名称为 `DS_<sanitized route name>`，当路由被索引时再加上 `_<index>`；
路由名称为空时回退到 `DS_Output`。

<a id="notes"></a>

## 说明

- `Graph` 块中的 `#Region "Name"` / `#EndRegion` 本身不会移动任何内容——它们为自动路径命名
  块，并为 IR 样式命名框。区域可以嵌套；没有名称的 `#Region` 是
  [`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216)，而不成对的
  [`DSH2217`](../diagnostics/DSH2xxx.md#dsh2217) / [`DSH2218`](../diagnostics/DSH2xxx.md#dsh2218)。
  语法见 [Layout](../language/layout.md) 页面。
- 只包含不匹配任何内容的 `Node(Var=…)` 条目且没有 `Comment(…)` 的 `Layout` 段
  **不会**启用显式路径——而是运行自动路径。
- 同一块中的第二个 `Layout` 段会完全替换第一个，并给出警告
  [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) *(自 2.0.0 起)*。
- 重新生成时不会保留布局位置。`Layout` 段是让节点位置保持
  稳定的唯一方式。请参见[重新生成](regeneration.md)。
- [反编译器](../tools/decompiler.md) 会以完全相同的解析格式输出 `Layout` 段，因此
  手动排列的材质可以导出并重新导入，且排列保持不变。这由
  项目设置 **Export Decompiled Layout** 控制，默认开启。
- 自动布局会报告一个慢任务，标题为 `Laying out DreamShader material graph...`，每个节点
  一帧，内容为 `Positioning node {N} of {Total}...`。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Layout")
{
    Properties {
        vec3  Tint      = vec3(1.0, 0.4, 0.1);
        float Intensity = 2.0;
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }
    Graph {
        #Region "Colour"
        Color = Tint * Intensity;
        #EndRegion
        #Region "Alpha"
        Alpha = Intensity * 0.5;
        #EndRegion
    }
}
```

自动布局产生：

```text
comment  DreamShader: Shared Inputs        sort key   -1000
comment  DreamShader: Colour               sort key   region order
comment  DreamShader: Alpha                sort key   region order
comment  DreamShader: Output: EmissiveColor
comment  DreamShader: Output: Opacity
comment  DreamShader: Material Output      root + the output reroute usages
reroute  DS_Shared_0                       Intensity, used by two blocks
```

改为固定一个节点：

```c
    Layout = {
        Comment(Name="Colour", X=-1200, Y=-200, W=900, H=400);
        Node(Var="Color", X=-600, Y=-120);
    }
```

```text
comment  DreamShader: Colour               at (-1200, -200), 900 x 400
node     Color                             at (-600, -120), exactly
                                           every other node propagated from its neighbours
```

<a id="see-also"></a>

## 另请参见

- [Layout](../language/layout.md) — `Node` / `Comment` 语法、参数和解析诊断
- [重新生成](regeneration.md) — 为何不保留位置，以及哪些注释会保留
- [临时材质](ephemeral.md) — 两种 ThinCustom 状态；布局在两者中都会运行
- [图表](../graph/index.md) — 创建所放置节点的语句
- [节点重用](../graph/node-reuse.md) — 为何重复的子表达式会成为一个要放置的节点
- [输出绑定](../language/output-bindings.md) — 创建输出重路由的绑定
- [反编译器](../tools/decompiler.md) — 将现有材质的布局导出回源代码
- [项目设置](../settings/project.md) — **Export Decompiled Layout**
- [生成](index.md) — 布局在管线中的位置
