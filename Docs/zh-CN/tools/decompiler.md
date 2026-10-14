---
title: 反编译导出
description: 把已有的材质资产导出成源文件 —— 默认的 2.0 反编译器（.dss / .dsi / .dsp），以及保留下来的 1.x 导出器（.dsm / .dsf）能还原什么、会丢什么。
---

反编译器会遍历一个已有的 `UMaterial` 或 `UMaterialFunction` 节点图，写出等价的 DreamShaderLang 源文件。
这是把一个已经存在的材质迁进这门语言的办法 —— 不用手工重搭。

**提示：**

**有两个反编译器。** （自 2.0.0 起） 默认的是 **2.0 反编译器**：它把资产的图读进编译器自己的 IR，打印成 `.dss` ——
材质实例则是 [`.dsi`](../language-v2/instances.md) —— 而且输出是**被证明过的**：文本会被重新编译、两份 IR 互相比较。
没有图的 Custom Pass 管线回来的是 [`.dsp`](../language-v2/passes.md) （自 2.1.0 起）。见本页末尾的
[2.0 反编译器](#20-反编译器)。本页前面几节描述的是 **1.x 导出器**，它写 `.dsm` / `.dsf`，留在 `-Format=Legacy` 后面。

| 项目 | 值 |
| :-- | :-- |
| 接受 | `UMaterial` · `UMaterialFunction` · `UMaterialFunctionMaterialLayer` · `UMaterialFunctionMaterialLayerBlend`；2.0 反编译器还接受 `UMaterialInstanceConstant` 和 `UDreamPassPipeline` |
| 产出 | 2.0：`.dss` / `.dsi` / `.dsp`；1.x 导出器：`.dsm` 或 `.dsf`。UTF-8 无 BOM |
| 写到 | `<SourceDirectory>/Decompiled/…`，除非显式给了输出路径 |
| 起始版本 | （自 1.3.5 起） Content Browser 操作 |

**警告：**

**1.x 导出器是迁移的起点，不是往返保证。** 它会还原图的结构，以及它能表达的那部分节点状态，
其余的留下一条 `// Warning:` 注释。删掉原资产之前先读[已知缺口](#已知缺口) ——
有好几类节点状态是被丢掉的，而且完全没有针对具体属性的警告。

<a id="怎么调用"></a>

## 怎么调用

| 入口 | 位置 | 产出 |
| :-- | :-- | :-- |
| Content Browser | 右键 `UMaterial` ▸ *DreamShader* ▸ **Export Legacy .dsm**（2.0.0 之前叫 **Export DSM**） | `.dsm` |
| Content Browser | 右键 `UMaterialFunction`、`UMaterialFunctionMaterialLayer` 或 `UMaterialFunctionMaterialLayerBlend` ▸ *DreamShader* ▸ **Export Legacy .dsf**（2.0.0 之前叫 **Export DSF**） | `.dsf` |
| 材质编辑器 | **DreamShader** 工具栏组合按钮 ▸ **Export Legacy .dsm** / **Export Legacy .dsf** | 同上 |
| 命令行 | `-run=DreamShader decompile -Asset=<object path> -Format=Legacy [-Out=<file>]`，或以 `.dsm` / `.dsf` 结尾的 `-Out` | 同上 |
| Content Browser | 右键一个**生成的**资产 ▸ *DreamShader* ▸ **Adopt Into Source** （自 1.8.0 起） | 就地改写该资产自己的 `.dsm` / `.dsf`，旧文件先备份成 `<source>.bak` |

不写 `-Format=Legacy`（也没有 `.dsm` / `.dsf` 的 `-Out`）时，命令行跑的是 [2.0 反编译器](#20-反编译器)；每个 legacy 菜单项旁边的
**Export .dss** 在编辑器里做同样的事 （自 2.0.0 起）。

**提示：**

对 `.dsm` / `.dsf` 源文件，**Adopt Into Source** 用的就是这个导出器，只不过写的是该资产自己的源文件，而不是 `Decompiled/` 下的新文件 ——
它是[手改检测](../generation/regeneration.md)的「资产才是对的」那个出口。因此本页的已知缺口同样适用：
adopt 会把导出器还原不了的东西一起丢掉。源文件声明了不止一个资产时它会**拒绝执行**，因为导出器发射的是
一个块而不是一个翻译单元，从这种文件里 adopt 一个资产会静默删掉其余的 —— 这种情况请用 **Export Legacy .dsm** 再手工合并。
含预处理指令的源文件同样拒绝（`DSH8149`）（自 1.9.0 起），见[手改检测](../generation/regeneration.md#三个出口)。

两个编辑器入口都要求**恰好选中一个**资产，写完文件后用你偏好的编辑器打开它 —— 见
[编辑器工具](editor-integration.md)。无头入口见[命令行](commandlet.md)。

进度对话框在 0.25 秒延迟后出现，标题是 `Decompiling Material '{Asset}'...` 或
`Decompiling Material Function '{Asset}'...`，每访问一个节点走一帧。commandlet 运行中不显示。

<a id="文件写到哪"></a>

## 文件写到哪

| 资产类 | 目录 | 扩展名 |
| :-- | :-- | :-- |
| `UMaterial` | `<SourceDirectory>/Decompiled/Materials/` | `.dsm` |
| `UMaterialFunction` | `<SourceDirectory>/Decompiled/Functions/` | `.dsf` |
| `UMaterialFunctionMaterialLayer` | `<SourceDirectory>/Decompiled/Layers/` | `.dsf` |
| `UMaterialFunctionMaterialLayerBlend` | `<SourceDirectory>/Decompiled/LayerBlends/` | `.dsf` |

`<SourceDirectory>` 是 *Source Directory* 项目设置，默认 `DShader`。Layer 和 layer blend 各有自己的目录，
而且类判定是 blend 优先，所以 layer blend 永远不会落进 `Layers`。

在分类目录内部，资产的 **package path** 变成相对文件路径，每个 package 段一层目录：

```text
/Game/Materials/Metal/M_Steel
  →  <Project>/DShader/Decompiled/Materials/Game/Materials/Metal/M_Steel.dsm
```

| 规则 | 细节 |
| :-- | :-- |
| 首尾的 `/` | 从 package 名上剥掉 |
| 非法字符 | 控制字符和 `< > : " / \ \| ? *` 变成 `_` |
| 空段 | 目录段变成 `Folder<N>`，最后一段变成 `Asset<N>`，`<N>` 是从 1 开始的序号 |
| 没有 package | 用资产自己的名字作为唯一一段 |

给 commandlet 传 `-Out=` 会覆盖整套计算，路径归一化后精确写到指定位置。编辑器入口不接受覆盖。

<a id="文件里的那个名字"></a>

### 文件里的那个名字

写在文件**内部**的 `Name=` 不是文件路径。它是 `Decompiled/<Category>/<package 段>`，
每段里的 `\ / . :` 都换成 `_`：

```text
/Game/Materials/Metal/M_Steel
  →  Shader(Name="Decompiled/Materials/Game/Materials/Metal/M_Steel")
```

**提示：**

因此重新编译导出的文件会在 `/Game/Decompiled/Materials/…` 下创建一个**新**资产，原件原封不动。
这是刻意的：你可以先对比两者再决定。等你准备好接管原路径时，再改 `Name=` 和 `Root=` ——
见[资产路径](../generation/asset-paths.md)。

<a id="文件结构"></a>

## 文件结构

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
// Decompiled from <full object path>
[// Warning: <text>]…

[<VirtualFunction declaration>]…

{ Shader | ShaderFunction | ShaderLayer | ShaderLayerBlend }(Name="<generated name>")
{
    [Properties = { … }]
    [Inputs     = { … }]   // function kinds only
    [Settings   = { … }]   // Shader only
    [Outputs    = { … }]
    [Graph      = { … }]
    [Layout     = { … }]
}
```

| 产出的块类型 | 源资产 |
| :-- | :-- |
| `Shader` | `UMaterial` |
| `ShaderFunction` | `UMaterialFunction` |
| `ShaderLayer` | `UMaterialFunctionMaterialLayer` |
| `ShaderLayerBlend` | `UMaterialFunctionMaterialLayerBlend` |

反编译出来的 `Shader` 的 `Settings` 块总是以 `Domain`、`ShadingModel` 和 `BlendMode` 开头，无条件写出。
其他每一项设置只在与 `UMaterial` 类默认值不同时才写；能往返的集合见
[材质 Settings](../settings/material.md)。当反编译到 `Base.FrontMaterial` 绑定时，shading model 会被强制成
`Substrate`，因为 Substrate 材质自己的 shading-model 枚举并不描述它的表面。

`Outputs` 为每个已连接的材质属性声明一个变量并绑定它，顺序固定：

```text
EmissiveColor, BaseColor, Metallic, Specular, Roughness, Anisotropy, Opacity, OpacityMask,
Normal, Tangent, WorldPositionOffset, SubsurfaceColor, CustomData0, CustomData1,
AmbientOcclusion, Refraction, PixelDepthOffset, MaterialAttributes,
FrontMaterial        (UE 5.4 及以上)
```

未连接的属性整条跳过。

<a id="终端输出节点"></a>

### 终端输出节点

`UMaterialExpressionCustomOutput`（`ThinTranslucentMaterialOutput`、`ClearCoatNormalCustomOutput`、
`VolumetricAdvancedMaterialOutput`、`VertexInterpolator`……）没有输出 pin，材质根节点够不到它 —— 所以它从节点列表里导出，写成
[`Expression( … )` 输出绑定](../generation/outputs.md#expression--pini-目标)。每个**已连接**的输入 pin 声明一个以 pin 命名的
`Outputs` 变量，pin 的整棵输入子树写进 `Graph`。写成哪种形式取决于连了几个 pin：

| 已连接的 pin | 写成 |
| :-- | :-- |
| 1 | 语句形式，`Expression(Class="…").Pin[<i>] = <var>;` |
| 2 个或更多 | [块形式](../generation/outputs.md#块形式)，`Expression(Class="…") { Pin[<i>] = <var>; … }` （自 1.9.0 起） |
| 0 | 什么都不写 —— 节点被丢掉 |

两种写法重新生成的都是同一个节点。`Class=` 写的是**反射**类名（`MaterialExpressionThinTranslucentMaterialOutput`，不带 `U`）。
输出目标节点按类加参数列表去重，所以一个材质里有**两个**同类的这种节点就无法往返：导出器只导出第一个，并记一条警告。

<a id="能忠实还原的部分"></a>

## 能忠实还原的部分

节点遍历器对下面每个类都有一条精心写好的分支。其余全部落到[通用回退](#ueexpression-回退)。

<a id="直接写成-dreamshaderlang-语法"></a>

### 直接写成 DreamShaderLang 语法

| `UMaterialExpression` 类 | 写成 |
| :-- | :-- |
| `Constant` | 浮点字面量 |
| `Constant2Vector` | `float2(x, y)` |
| `Constant3Vector` | `float3(r, g, b)` |
| `Constant4Vector` | `float4(r, g, b, a)` |
| `Add` | `a + b` |
| `Subtract` | `a - b` |
| `Multiply` | `a * b` |
| `Divide` | `a / b` |
| `OneMinus` | `1.0 - x` |
| `LinearInterpolate` | `lerp(a, b, alpha)` |
| `Clamp` | `clamp(x, min, max)` —— 仅当 clamp 模式是默认的双边钳制 |
| `Power` | `pow(base, exponent)` |
| `DotProduct` | `dot(a, b)` |
| `Normalize` | `normalize(v)` |
| `Min` | `min(a, b)` |
| `Max` | `max(a, b)` |
| `Abs` | `abs(x)` |
| `Saturate` | `saturate(x)` |
| `Floor` | `floor(x)` |
| `Ceil` | `ceil(x)` |
| `Frac` | `frac(x)` |
| `SquareRoot` | `sqrt(x)` |
| `Sine` | `sin(x)` —— 仅当 `Period` 为 1 |
| `Cosine` | `cos(x)` —— 仅当 `Period` 为 1 |
| `ComponentMask` | 输入上的一个 swizzle，按 R/G/B/A 标志的顺序拼出 |
| `AppendVector` | `floatN(a, b)`；两个操作数 swizzle 的是同一个基值时合并成单个 swizzle |
| `Time` | `UE.Time()` —— 仅当它既不忽略暂停也不覆盖周期 |
| `Reroute` | 什么都不写 —— 普通 reroute 被穿透追踪，带环检测 |
| `NamedRerouteDeclaration`、`NamedRerouteUsage` | 一个具名 `Graph` 临时变量，被每次使用复用 |
| `FunctionInput` | `Inputs` 段里声明的那个名字 |
| `MaterialFunctionCall` | 在块上方生成一段 [`VirtualFunction`](../language/blocks.md) 声明，加上对它的调用 |

未连接的操作数 pin 会退回节点自己的常量属性（`Min`、`Max`、`LinearInterpolate`、`Power` 和 `Clamp`
都会读它们的 `Const*` 值），再退不成就退到字面量 `0.0`。

**提示：**

上面那些数学内置写法就是[数学内置](../builtins/math.md)里记的那 19 个名字。注意缺口：反编译器没有处理
`UMaterialExpressionFmod` 的分支，所以已有的 `Fmod` 节点会回来成通用的
`UE.Expression(Class="Fmod", …)` 调用，而不是 `fmod(…)`。源码是等价的，只是不是内置写法。

<a id="写成-property-声明"></a>

### 写成 property 声明

这些会变成 `Properties` 段里的条目，并在 `Graph` 中按名字引用。名字会做唯一化处理；
当 DreamShaderLang 标识符不得不与资产的参数名不同时，会补一条 `ParameterName=` 元数据。

| `UMaterialExpression` 类 | 声明 |
| :-- | :-- |
| `ScalarParameter` | `ScalarParameter <Name> = <default>;` |
| `VectorParameter` | `VectorParameter <Name> = float4(r, g, b, a);` |
| `TextureObjectParameter` | `TextureObjectParameter <Name>[ = <asset path>];` |
| `TextureSampleParameter2D` | `TextureSampleParameter2D <Name>[ = <asset path>];` —— **仅当没有输入 pin 被连接** |

只要有任何输入 pin 被连接，`TextureSampleParameter2D` 就改为写成一条精选的 `UE.Expression`，
参数实参和采样器实参一起带上 （自 1.3.7 起）。它的 `RGBA` 输出被写进一个具名临时变量一次，
其他每个 pin 都变成它的 swizzle。

<a id="写成精选的-ueexpression"></a>

### 写成精选的 `UE.Expression`

这些保留手写的参数列表，而不是反射 dump，所以只有真正与节点默认值不同的属性才会出现。

| `UMaterialExpression` 类 | 写出的参数 |
| :-- | :-- |
| `CurveAtlasRowParameter` | 非默认时的 `ParameterName`、`Group`、`SortPriority` 和 `Desc`，然后是 `DefaultValue`、`Curve`、`Atlas`、`UseCustomPrimitiveData` 及 `PrimitiveDataIndex`，以及 time pin 已连接时的 `CurveTime` |
| `StaticComponentMaskParameter` | `Input`、`DefaultR`、`DefaultG`、`DefaultB`、`DefaultA`，以及已设置时的 `ParameterName`。`OutputType` 跟随启用通道数 |
| `StaticSwitchParameter` | `True`、`False`，以及非默认时的 `ParameterName`、`DefaultValue`、`DynamicBranch` |
| `TextureCoordinate` | `CoordinateIndex`、`UTiling`、`VTiling` —— 各自仅在非默认时 |
| `Time` | `bIgnorePause`、`bOverride_Period`、`Period` —— 节点不在默认值时使用 |
| `Sine` / `Cosine` | `Input`、`Period` —— `Period` 不为 1 时使用 |
| `Clamp` | `Input`、`Min`、`Max`、`ClampMode` —— clamp 模式是只钳下限或只钳上限时使用 |
| `Panner` | `Coordinate` 或 `ConstCoordinate`、`Time`，以及 `Speed` 或非零的 `SpeedX` / `SpeedY`，加上 `bFractionalPart` |
| `Rotator` | `Coordinate` 或 `ConstCoordinate`、`Time`，以及非默认时的 `CenterX`、`CenterY`、`Speed` |
| `WorldPosition` | 非默认时的 `WorldPositionShaderOffset` |
| `CameraVectorWS` | *（无）* |
| `ObjectPositionWS` | *（无）* |
| `ScreenPosition` | *（无）* |
| `VertexColor` | *（无）* —— 类型恒为 `float4`，pin 的掩码写成 swizzle |
| `TextureSample` | 纹理、采样器和 mip 参数；`RGBA` 输出只写一次，其他每个 pin 变成它的 swizzle |
| `Custom` | `Code`、`Description`、次级输出用的 `Output`、完整的 `AdditionalOutputs` 列表，以及每个已连接输入一个参数。`OutputType` 取节点自己声明的返回类型，**绝不**取所选输出的类型 |

<a id="ueexpression-回退"></a>

## `UE.Expression` 回退

上面没有分支的类都会被导出成一个通用的 [`UE.Expression`](../builtins/ue-expression.md) 调用，
反编译器同时为它记一条警告。

参数列表分两趟构建：

1. 每个**已连接**的输入 pin，按 pin 名命名，按 pin 顺序。
2. 每个值与类默认值不同的**反射字面量属性** （自 1.3.7 起）。

一个属性只有同时满足下列全部条件，才会在第 2 趟被导出：

| 要求 | 细节 |
| :-- | :-- |
| 不是 deprecated、transient 或 duplicate-transient | |
| 不是 material-expression 输入 | 那些属于第 1 趟 |
| 标记为可编辑 | `CPF_Edit` |
| 不是控制名 | `Class`、`OutputType`、`ResultType`、`Output`、`OutputName`、`OutputIndex` |
| 不是 editor-only 名 | `MaterialExpressionEditorX`、`MaterialExpressionEditorY`、`Desc`、`bCommentBubbleVisible`、`bShowOutputNameOnPin`、`bHidePreviewWindow`、`bCollapsed`、`bShaderInputData`、`SortPriority` |
| 属于受支持的属性类型 | bool、数值、枚举、byte、name、string、text，或对象引用 |
| 与类默认值不同 | 相同的值被省略 |
| 尚未成为参数 | 同名的第一个写入者胜出 |

名字比较是在归一化之后做的，所以 `bTwoSided` 和 `Two Sided` 会冲突。

**警告：**

**struct、array、map、set 和 delegate 属性被静默丢弃。** 它们不属于受支持的属性类型，
所以一个状态存在 struct 里的节点会以类默认值导出，而且没有任何警告指出是哪个属性。
首次编译后手工重设这些值，或者保留 `UE.Expression` 节点并自己补上缺的参数。

`OutputType` 总是写出，按真实输出索引解析。存在具名输出选择器（`Output=` / `OutputName=`）时，
`OutputIndex` 会被抑制，因为同时带两者的调用是 `DSH5252`。参数超过三个、或长度超过 120 字符的调用会拆成多行。

<a id="layout-导出"></a>

## Layout 导出

由 *Export Decompiled Layout* 项目设置控制，默认**开**。打开时文件会带一个 `Layout` 段：

| 写出的行 | 来自 |
| :-- | :-- |
| `Comment(Name="<text>", X=<x>, Y=<y>, W=<w>, H=<h>, Color=float4(r, g, b, a));` | 每一个编辑器注释框 |
| `Node(Var="<name>", X=<x>, Y=<y>);` | 每一个具名表达式，按 X、再 Y、再名字排序 |

文本以 `DreamShader: ` 开头的注释框会被跳过 —— 那些是生成的标记，不是作者写的注释。

与这个设置无关地，每个表达式还会被归到**最小的**那个包住它的注释框，这些归属会变成对应 `Graph` 语句周围的
`#Region` / `#EndRegion` 指令。关掉 layout 导出会去掉 `Layout` 块，但**不会**去掉 region。
见 [Layout 与 #Region](../language/layout.md)。

<a id="诊断"></a>

## 诊断

运行期替换写作 `{Placeholder}`。

<a id="写进文件里的警告"></a>

### 写进文件里的警告

每条只写一次，作为 `// Decompiled from …` 头行下面的 `// Warning: …` 注释。它们都不会让导出失败。

| 消息 | 原因 |
| :-- | :-- |
| `Exported '{Class}' as UE.Expression; review reflected literal properties if the node has editor-only state.` | 一个没有精选分支的节点 |
| `MaterialFunctionCall '{Path}' is not a plain MaterialFunction; it was exported through UE.Expression.` | 调用目标是 layer 或 layer blend，而不是普通材质函数 |
| `A MaterialFunctionCall had no function asset and was exported as a zero literal.` | 调用节点没有指定函数 |
| `Failed to emit VirtualFunction for '{Path}': {Error}` | 被调函数的声明构建失败 |
| `Named reroute usage '{Node}' has no valid declaration; emitted a default literal.` | 作为节点触达的悬空具名 reroute 使用 |
| `Named reroute usage '{Node}' has no valid declaration; emitted its default value.` | 同上，但经由输入 pin 触达 |
| `Detected a recursive graph dependency while decompiling node '{Node}'; emitted a default literal to avoid stack overflow.` | 表达式图里有环 |
| `Detected a recursive reroute dependency while decompiling node '{Node}'; emitted a default literal to avoid stack overflow.` | 经由普通 reroute 的环 |
| `Detected a recursive named reroute dependency for '{Node}'; emitted a default literal to avoid stack overflow.` | 经由具名 reroute 的环 |
| `Append node '{Node}' resolved to {A} + {B} components, which cannot fit a float4; masked its inputs down to {A2} + {B2}. Review the emitted swizzle.` | 操作数分量数之和超过四的 append |
| `Material has more than one '{Class}' node; output targets are de-duplicated by class, so only the first was exported.` | 两个同类的[终端输出节点](#终端输出节点) |

<a id="导出失败"></a>

### 导出失败

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `No Material asset was provided.` | 一个空材质进到了反编译器。 |  |
| `No MaterialFunction asset was provided.` | 一个空函数进到了反编译器。 |  |
| `No asset was provided.` | 一个空资产进到了服务层。 |  |
| `MaterialFunction '{Name}' does not expose any outputs.` | 该函数没有声明输出。 | 给资产加一个输出，或者改为导出调用它的那个材质。 |
| `DreamShader decompile supports Material and MaterialFunction assets only: {Path}` | 1.x 导出器遇到任何其他资产类 —— 包括 UMaterialInstanceConstant。 | 去掉 -Format=Legacy，用 2.0 反编译器（它把实例写成 .dsi）。 |
| `Decompile did not produce source text.` | 反编译报告失败但没有消息。 |  |
| `DreamShader failed to resolve an output file path.` | 算出来的输出路径为空。 |  |
| `DreamShader failed to create output directory '{Directory}'.` | 目录创建失败。 |  |
| `DreamShader failed to write decompiled source '{File}'.` | 文件写入失败。 |  |

<a id="编辑器-toast"></a>

### 编辑器 toast

每个 *Export* 菜单项弹的是同一组 toast，2.0 的那几个也一样 （自 2.0.0 起）：

| Toast | 原因 |
| :-- | :-- |
| `DreamShader could not find the selected Material.` / `…Material Function.` | 从右键到点击之间资产被卸载了 |
| `DreamShader failed to export '{Asset}': {Reason}` | 反编译失败 |
| *（原始写入错误）* | 文件保存失败 |
| `Exported '{File}' but could not open it.` | 写出来了，但编辑器启动不了 |
| `Exported '{File}'.` | 成功 |

日志：Display 级的 `Exported '{Asset}' to '{File}'.`，以及 Warning 级的
`Failed to export '{Asset}' to a DreamShader source: {Reason}`。

<a id="已知缺口"></a>

## 已知缺口

1.x 导出器在 **1.5.0** 的实测行为。每一行都是导出文件还原不了的东西。把源文件当作事实来源之前，
先核对与你的资产相关的那几行。

| 缺口 | 后果 | 变通办法 |
| :-- | :-- | :-- |
| 材质函数的 settings 从不写出 | 导出 `UMaterialFunction` 时 `Description`、`ExposeToLibrary`、`LibraryCategories` 和 `UserExposedCaption` 全部丢失 | 手写一个 `Settings` 块 |
| 只写出被认可的那组 `UMaterial` 属性 | 之外的属性 —— `OpacityMaskClipValue`、`NumCustomizedUVs`、半透明光照模式、位移缩放、Nanite 覆盖 —— 保持类默认值 | 把这些键加进 `Settings`，它们按反射解析 —— 见[材质 Settings](../settings/material.md) |
| struct、array、map、set 类型的节点属性不参与反射 | 回退成 `UE.Expression` 的节点丢掉那部分状态，而且没有针对属性的警告 | 生成之后在材质上设置该属性，或者扩写导出的调用 |
| 节点注释文本（`Desc`）和节点 `SortPriority` 被丢弃 | 注释气泡和 pin 顺序还原不了 | 手工重设 |
| 节点位置取决于一个设置 | *Export Decompiled Layout* 关掉时，重新生成的图改为自动布局 | 保持该设置开启，或者手写 `Layout` |
| 前缀为 `DreamShader: ` 的注释框被丢弃 | 生成的标记不会被重新写出，这是设计如此 | 无需处理 |
| 指向 layer 或 layer blend 的 `MaterialFunctionCall` 回退成 `UE.Expression` | 该调用不会被表达成 `VirtualFunction` | 单独导出那个 layer 再调用它 |
| 没有指定函数的 `MaterialFunctionCall` 变成 `0.0` | 该分支被静默常量折叠 | 在原资产里重新指定函数再导出 |
| 环会写出一个默认字面量 | 成环的那条分支求值成常量 | 在原图里打断这个环 |
| 宽度超过四分量的 append 会被掩码收窄 | 分量被丢掉 | 检查写出的 swizzle |
| 1.x 导出器不支持材质**实例** | `UMaterialInstanceConstant` 会被直接拒绝 | 用 [2.0 反编译器](#20-反编译器)，它写 `.dsi` |
| 纹理采样的 `GatherMode` 只在 UE 5.6 及以上往返 | 更老的引擎上该属性被省略 | 无 |
| `bHasPixelAnimation` 只在 UE 5.4 及以上进入写出的 flag 集合 | 更老的引擎上该 flag 被省略 | 无 |
| `Base.FrontMaterial` 和 `Substrate` 这个 shading-model 写法只存在于 （自 UE 5.4 起） | 5.4 以下无法有意义地导出 Substrate 材质 | 无 |
| 生成的 `Name=` 指向 `Decompiled/…` | 重新编译会创建第二个资产，而不是替换原件 | 确认源文件可信之后改 `Name=` / `Root=` |
| `Classic` 布局风格下大图会跳过自动布局 | 没有 `Layout` 块时，1200 个节点以上的重新生成的图回来可能视觉上毫无秩序；IR 风格（`Blocks`，2.0.0 起的默认）总是布局 | 保持 layout 导出开启，或用 IR 风格 —— 见 [Layout 与 #Region](../language/layout.md) |

<a id="示例"></a>

## 示例

无头导出 `/Game/Materials/M_Steel`，然后看结果：

```powershell
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "I:\Project\Project.uproject" `
    -run=DreamShader decompile -Asset="/Game/Materials/M_Steel" -Format=Legacy `
    -unattended -nopause -nosplash -stdout -log
```

```text
DreamShader decompiled '/Game/Materials/M_Steel.M_Steel' to
'I:/Project/DShader/Decompiled/Materials/Game/Materials/M_Steel.dsm' (legacy).
```

写出的文件：

```c
// Decompiled from /Game/Materials/M_Steel.M_Steel
Shader(Name="Decompiled/Materials/Game/Materials/M_Steel")
{
    Properties = {
        ScalarParameter Roughness_0 = 0.35 [ParameterName="Roughness"];
        VectorParameter Tint = float4(0.8, 0.8, 0.82, 1.0);
    }
    Settings = {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs = {
        float3 BaseColor;
        float Metallic;
        float Roughness;

        Base.BaseColor = BaseColor;
        Base.Metallic  = Metallic;
        Base.Roughness = Roughness;
    }

    Graph = {
        BaseColor = Tint.rgb;
        Metallic  = 1.0;
        Roughness = saturate(Roughness_0);
    }

    Layout = {
        Node(Var="Tint", X=-640, Y=-208);
        Node(Var="Roughness_0", X=-640, Y=48);
    }
}
```

注意第一条 property 上的 `ParameterName="Roughness"`。资产里那个参数就叫 `Roughness`，
但这个标识符在本文件里已经被占用了，于是声明被唯一化成 `Roughness_0`，真实参数名作为元数据保留下来 ——
否则重新生成的材质会以错误的名字暴露参数。见[元数据与分组](../parameters/metadata.md)。

<a id="20-反编译器"></a>

## 2.0 反编译器

（自 2.0.0 起）

```powershell
./dsc.ps1 decompile /Game/Materials/M_Steel                  # writes DShader/Decompiled/.../M_Steel.dss
./dsc.ps1 decompile /Game/Materials/MI_Steel_Worn            # a material instance: MI_Steel_Worn.dsi
./dsc.ps1 decompile -SourceFile DShader/Functions/Lib.dss    # every asset that source builds, in one file
./dsc.ps1 decompile /Game/Materials/M_Steel -Format Legacy   # the 1.x text described above
```

它就是把编译器倒着跑：**图 → IR → AST → 打印器**。编辑器里每个 legacy 导出项旁边的 **Export .dss**（材质实例上是
**Export .dsi**）做同样的事；输出路径规则与上面相同，默认目录是 `<SourceDirectory>/Decompiled/…`，文件名换成 `.dss` / `.dsi`。

1. **导入。** 每个表达式读回成发射器为它写下的那个 IR 操作；其余的是基于内建目录的反射 `UE.<Class>(...)` 调用。
   内联 pin mask 按发射器自己的规则变回 swizzle，注释框变成 `#pragma region`，节点位置变成 `#pragma layout` 提示。
2. **提升。** 没人读的节点被剪掉，发射器手工降级的形状被读回来（static bool 上的 StaticSwitch 是 `if` / `?:`，Set-attributes 链是
   `m.X = ...`）。
3. **书写。** 按显示顺序写 uniform，读了两次或被语句命名的值写成局部变量，从节点代码里恢复 custom 函数及其 helper，layer 和 blend
   用它们自己的签名，图里调用的每个函数资产写一个 `extern` 原型。
4. **证明。** 打印出的文本会被再解析一次（不行是 `DSH9089`），往返测试会编译它并比较 IR。

语言表达不了的东西一律**点名**，从不静默丢掉：导入阶段是 `DSH9060`–`DSH9074`（缺失的函数资产、目录外的类……），书写阶段是
`DSH9075`–`DSH9084`（改了名的标识符、内联值的位置……），写在文件头部，形如 `// Warning: DSHnnnn: …`。
`-KeepAssetPath` 写一行 `/// @name` 让重新构建接管原资产的路径，`-Readable` 优先 HLSL 语法糖而不是精确到类的节点调用 —— 见
[命令行](commandlet.md#decompile--export)。

<a id="数组里的-pin"></a>

### 数组里的 pin

有些引擎类把 pin 或输出放在调用写不出名字的数组里。这几种被读回成它们计算的东西：

| 类 | 写成 |
| :-- | :-- |
| **Convert**（Make / Break FloatN）（自 2.0.1 起） | 每个输出写成拼出它的那些通道 —— `DXY.xy`、`DXY.zw`、`float3(Gain, 0.25, 0.75)` —— 重建成 ComponentMask 和 AppendVector 节点（`DSH9073`）；引擎拒绝的映射被点名（`DSH9063`）。在此之前回来的是一个不带参数的 `UE.Convert()` |
| **Switch**，有未命名的 case （自 2.0.1 起） | 引擎为它造出的那条分支链：`0.0 == floor(s) ? a : 1.0 == floor(s) ? b : default`；SwitchValue 没连时，就是它的数字选中的那一个输入（`DSH9069`）。`DSH9073` 会提示给 case 起名以保住节点 |
| **Switch**，所有已连接的 case 都有名字 （自 2.0.2 起） | Switch 本身：`UE.Switch(SwitchValue = s, Dry = a, Wet = b, Inputs = "((InputName=\"Dry\"),(InputName=\"Wet\"))")` |
| **VertexInterpolator** （自 2.0.1 起） | 任何读它的地方都是 `UE.VertexInterpolator(Input = ...)`；读一个根本没有输出的节点是一个零加 `DSH9072`，而不是反编译失败 |
| **LandscapeLayerBlend**、**LandscapeGrassOutput**、**LandscapePhysicalMaterialOutput** （自 2.0.2 起） | 数组写成详情面板粘贴的那种文本（不含 pin），每个 pin 用节点上显示的名字的标识符形式：`UE.LandscapeLayerBlend(Layer_Grass = g, Height_Rock = h, Layers = "((LayerName=\"Grass\"),...)")`。名字为空、是关键字或与别的 pin 冲突的 pin 仍然以 `DSH9070` 丢掉 |

仍然留在数组里、文本没有写法的：`MaterialCache` 的属性 pin 和输出、`LayerStack`（实验性）的层输入、`Operator`（只在新翻译器里）
的每个输入及其常量。这些节点回来是一个缺了那些连线（`DSH9070`）或状态（`DSH9068`）的反射调用，需要手工补完。

两处小改动：未连接、读自己 `Const*` 孪生属性的 pin，常量写在 pin **自己的位置**上 （自 2.0.2 起） ——
`UE.LinearInterpolate(A = 0.0, B = x, Alpha = t)` 而不是排在所有已连接 pin 之后；catalog 不认识的 `SetMaterialAttributes` pin
现在也会被点名（`DSH9072`），和 `MakeMaterialAttributes` 一样 （自 2.0.1 起）。

<a id="substrate"></a>

### Substrate

（自 2.0.2 起） **每个 Substrate 节点都按名字写出**，文本里搜得到图上显示的那个节点：Transmittance-To-MFP 写成
`SSSMFP = Substrate.TransmittanceToMFP(TransmittanceColor = ..., Thickness = ...).MFP`，Coverage Weight 写成
`Substrate.Weight(A = ..., Weight = ...)`，名字用源码用的那个（`Substrate.Slab(...)`，目录给这个类列出的第一个别名）而不是反射名
`SubstrateSlabBSDF`。只有可读形式（`-Readable`）才把 [Substrate 语法糖](../language-v2/substrate.md)读回来 —— 在图上精确等价的地方：
什么都没设置的 `SubstrateAdd` / `SubstrateWeight` / `SubstrateHorizontalMixing` 是 `A + B`、`A * w` 和 `lerp(A, B, t)`，
只喂一个 BSDF 的转换节点变成那个 BSDF 上的 `BaseColor = ...` / `Haziness = ...` / `Transmittance = ...`。

<a id="材质实例与管线"></a>

### 材质实例与管线

普通的 `UMaterialInstanceConstant` 写成 `.dsi`：父材质、它覆盖的 `#pragma instance` 键，以及与父材质不同的参数 —— 见
[材质实例（.dsi）](../language-v2/instances.md)。`UDreamPassPipeline` 没有图可导入，它的字段被读回成 `.dsp` 绑定的数据、打印成 `.dsp`
（自 2.1.0 起）；只有命令行和 bridge 能反编译管线，Content Browser 里没有对应菜单项。见 [Custom Pass（.dsp）](../language-v2/passes.md)。

<a id="继续阅读"></a>

## 继续阅读

- [编辑器工具](editor-integration.md) — Export .dss / .dsi 与两个 Export Legacy 菜单项。
  - [命令行](commandlet.md) — decompile -Asset、-SourceFile、-Format、-Out 和退出码。
  - [通用 UE.Expression](../builtins/ue-expression.md) — 回退路径写出的那种通用调用。
  - [材质 Settings](../settings/material.md) — 导出器能写进 Settings 的和写不进的。
  - [Layout 与 #Region](../language/layout.md) — Node 和 Comment 指令，以及 region。
  - [资产路径](../generation/asset-paths.md) — 写出的 Name= 解析成什么。
