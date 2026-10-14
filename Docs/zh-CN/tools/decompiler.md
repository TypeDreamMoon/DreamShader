---
title: "反编译导出"
description: "反编译导出"
---

<a id="decompiler"></a>

# 反编译导出

> [DreamShader](../index.md) » [工具](index.md) » **反编译导出**

这个导出器会遍历一个已有的 `UMaterial` 或 `UMaterialFunction` 节点图，写出等价的 DreamShaderLang 源文件。

| | |
| :-- | :-- |
| 接受 | `UMaterial` · `UMaterialFunction` · `UMaterialFunctionMaterialLayer` · `UMaterialFunctionMaterialLayerBlend` |
| 产出 | 一个 `.dsm` 或 `.dsf` 文件，UTF-8 无 BOM |
| 写到 | `<SourceDirectory>/Decompiled/…`，除非显式给出了输出路径 |
| 起始版本 | `1.3.5`（Content Browser 操作） |

> [!IMPORTANT]
> **有两个反编译器。** 本页描述的是 **1.x** 那个，它写 `.dsm` / `.dsf`
> 并留在 `-Format=Legacy` 后面。**2.0 反编译器**才是默认：它把资产的图读进编译器自己的 IR，
> 打印出一个 `.dss` —— 材质实例则是 [`.dsi`](../language-v2/instances.md) ——
> 而且它的输出是*被证明过的*：文本会被重新编译，两份 IR 互相比较。没有图的 Custom Pass 管线
> 回来的是 [`.dsp`](../language-v2/passes.md)，读回后与资产比较*（自 2.1.0 起）*。见本页末尾的
> [2.0 反编译器](#the-20-decompiler)。

> [!NOTE]
> 把 1.x 反编译器当作**迁移的起点**，不是往返保证。它会还原图的结构，以及它能表达的那部分
> 节点状态，其余的留下一条 `// Warning:` 注释。删掉原资产之前，下面列出的
> [已知往返缺口](#known-round-trip-gaps) 正是值得手工核对的那几项。

<a id="invoking-it"></a>

## 调用方式

| 入口 | 位置 | 产出 |
| :-- | :-- | :-- |
| Content Browser | 右键一个 `UMaterial` ▸ *DreamShader* ▸ **Export Legacy .dsm**（2.0.0 之前叫 **Export DSM**） | `.dsm` |
| Content Browser | 右键一个 `UMaterialFunction`、`UMaterialFunctionMaterialLayer` 或 `UMaterialFunctionMaterialLayerBlend` ▸ *DreamShader* ▸ **Export Legacy .dsf**（2.0.0 之前叫 **Export DSF**） | `.dsf` |
| 材质编辑器 | **DreamShader** 工具栏组合按钮 ▸ **Export Legacy .dsm** / **Export Legacy .dsf** | 同上 |
| 命令行 | `-run=DreamShader decompile -Asset=<object path> -Format=Legacy [-Out=<file>]`，或以 `.dsm` / `.dsf` 结尾的 `-Out` | 同上 |

不写 `-Format=Legacy`（也没有 `.dsm` / `.dsf` 的 `-Out`）时，命令行跑的是
[2.0 反编译器](#the-20-decompiler) *（自 2.0.0 起）*；每个 legacy 菜单项旁边的 **Export .dss**
在编辑器里做同样的事。

两个编辑器入口都要求**恰好选中一个**资产，写完文件后用你偏好的编辑器打开它。见
[编辑器集成](editor-integration.md#content-browser-context-menus)和[命令行](commandlet.md)。

进度对话框在 0.25 秒延迟后出现，标题是 `Decompiling Material '{Asset}'...` 或
`Decompiling Material Function '{Asset}'...`，每访问一个节点走一帧。commandlet 运行中不显示该对话框。

<a id="output-paths"></a>

## 输出路径

| 资产类 | 目录 | 扩展名 |
| :-- | :-- | :-- |
| `UMaterial` | `<SourceDirectory>/Decompiled/Materials/` | `.dsm` |
| `UMaterialFunction` | `<SourceDirectory>/Decompiled/Functions/` | `.dsf` |
| `UMaterialFunctionMaterialLayer` | `<SourceDirectory>/Decompiled/Layers/` | `.dsf` |
| `UMaterialFunctionMaterialLayerBlend` | `<SourceDirectory>/Decompiled/LayerBlends/` | `.dsf` |

`<SourceDirectory>` 是 *Source Directory* 项目设置，默认 `DShader`。layer 和
layer blend 的目录与 `Functions` 是分开的，而且类判定是 blend 优先，所以
layer blend 永远不会落进 `Layers`。

在分类目录内部，资产的 **package path** 变成相对文件路径，每个 package 段一层目录：

```text
/Game/Materials/Metal/M_Steel
  →  <Project>/DShader/Decompiled/Materials/Game/Materials/Metal/M_Steel.dsm
```

| 规则 | 细节 |
| :-- | :-- |
| 首尾的 `/` | 从 package 名上剥掉 |
| 非法字符 | 控制字符和 `< > : " / \ | ? *` 变成 `_` |
| 空段 | 目录段变成 `Folder<N>`，最后一段变成 `Asset<N>`，`<N>` 是从 1 开始的序号 |
| 没有 package | 用资产自己的名字作为唯一一段 |

给 commandlet 传 `-Out=` 会覆盖整套计算，路径归一化后精确写到指定位置。编辑器入口不接受覆盖。

<a id="the-generated-asset-name"></a>

### 生成出来的资产名

写在文件**内部**的 `Name=` 不是文件路径。它是
`Decompiled/<Category>/<package segments>`，每段里的 `\ / . :` 都换成 `_`：

```text
/Game/Materials/Metal/M_Steel
  →  Shader(Name="Decompiled/Materials/Game/Materials/Metal/M_Steel")
```

因此重新编译导出的文件会在 `/Game/Decompiled/Materials/…` 下创建一个**新**资产，原件原封不动。
等你准备好接管原路径时，再改 `Name=` 和 `Root=`。见
[资产路径](../generation/asset-paths.md)。

<a id="file-layout"></a>

## 文件结构

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

反编译出来的 `Shader` 的 `Settings` 块总是以 `Domain`、`ShadingModel` 和
`BlendMode` 开头，无条件写出。其他每一项设置只在与 `UMaterial` 类默认值不同时才写出；
能往返的集合见[着色器设置](../settings/material.md)。当反编译到 `Base.FrontMaterial` 绑定时，
着色模型会被强制成 `Substrate`，因为 Substrate 材质自己的着色模型枚举
并不描述它的表面。

`Outputs` 为每个已连接的材质属性声明一个变量并绑定它，顺序固定：
`EmissiveColor`、`BaseColor`、`Metallic`、`Specular`、`Roughness`、`Anisotropy`、`Opacity`、
`OpacityMask`、`Normal`、`Tangent`、`WorldPositionOffset`、`SubsurfaceColor`、`CustomData0`、
`CustomData1`、`AmbientOcclusion`、`Refraction`、`PixelDepthOffset`、`MaterialAttributes`，以及 —— 在
UE 5.4 及更新版本上 —— `FrontMaterial`。未连接的属性整条跳过。

<a id="terminal-output-nodes"></a>

### 终端输出节点

一个 `UMaterialExpressionCustomOutput`（`ThinTranslucentMaterialOutput`、`ClearCoatNormalCustomOutput`、
`VolumetricAdvancedMaterialOutput`、`VertexInterpolator`……）没有输出 pin，所以材质根节点
够不到它 —— 它改为从节点列表里导出，写成
[`Expression( … )` 输出绑定](../language/output-bindings.md#the-expression--pini-target)。每个**已连接**的输入 pin 声明一个
`Outputs` 变量，以 pin 命名，pin 的整棵输入子树写进 `Graph`。写成哪种
形式取决于连了几个 pin：

| 已连接的 pin | 写成 |
| :-- | :-- |
| 1 | 语句形式，`Expression(Class="…").Pin[<i>] = <var>;` |
| 2 个或更多 | [块形式](../language/output-bindings.md#block-form)，`Expression(Class="…") { Pin[<i>] = <var>; … }` *（自 1.9.0 起）* |
| 0 | 什么都不写 —— 节点被丢掉 |

两种写法重新生成的都是同一个节点；多 pin 节点选用块形式，是因为它把这一意图写进语法里，
而不是留给 N 份重复的参数列表去表达。1.9.0 之前反编译成语句形式的文件
仍然会逐字节反编译成语句形式。

`Class=` 带的是**反射**类名（`MaterialExpressionThinTranslucentMaterialOutput`，没有
`U` 前缀），这是 `Expression(Class=…)` 能解析的写法。

> [!WARNING]
> 输出目标节点按类加参数列表去重，所以一个材质里持有**两个**同类的
> 节点就无法往返。反编译器只导出第一个，并记录警告
> `Material has more than one '{Class}' node; output targets are de-duplicated by class, so only the
> first was exported.`

<a id="what-is-exported-faithfully"></a>

## 能忠实还原的部分

节点遍历器对下面每个类都有一条精心写好的分支。其余全部落到
[通用回退](#the-ueexpression-fallback)。

<a id="emitted-as-dreamshaderlang-syntax"></a>

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
| `MaterialFunctionCall` | 在块上方生成一段 [`VirtualFunction`](../language/virtual-function.md) 声明，加上对它的调用 |

未连接的操作数 pin 会退回节点自己的常量属性（如果有的话）—— `Min`、
`Max`、`LinearInterpolate`、`Power` 和 `Clamp` 都会读它们的 `Const*` 值 ——
否则退到字面量 `0.0`。

<a id="emitted-as-a-property-declaration"></a>

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
参数实参和采样器实参一起带上 *（自 1.3.7 起）*。它的
`RGBA` 输出被写进一个具名临时变量一次；其他每个 pin 都变成它的 swizzle。

<a id="emitted-as-a-curated-ueexpression"></a>

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

<a id="the-ueexpression-fallback"></a>

## `UE.Expression` 回退

上面没有分支的类都会被导出成一个通用的
[`UE.Expression`](../builtins/ue-expression.md) 调用，反编译器同时为它记一条警告。

参数列表分两趟构建：

1. 每个**已连接**的输入 pin，按 pin 名命名，按 pin 顺序。
2. 每个值与类默认值不同的**反射字面量属性** *（自 1.3.7 起）*。

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

> [!WARNING]
> **struct、array、map、set 和 delegate 属性会被静默丢弃。** 它们不属于受支持的属性类型，
> 所以一个状态存在于 struct 中的节点会以该状态在类默认值处的形式导出，并且没有任何警告指出具体是哪个属性。
> 首次编译后请手动重新设置这些值，或者将节点保留为 `UE.Expression` 并自行补上缺失的参数。

`OutputType` 总是会被写出，并根据真实输出索引解析。当存在具名输出选择器
（`Output=` / `OutputName=`）时，`OutputIndex` 会被抑制，因为同时携带两者的调用是
[`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252)。参数超过三个、或长度超过 120 个字符的调用会跨多行写出。

<a id="layout-export"></a>

## Layout 导出

由 *Export Decompiled Layout* 项目设置控制，默认**开启**。开启时，文件会带有一个 `Layout` 段：

| 写出的行 | 来自 |
| :-- | :-- |
| `Comment(Name="<text>", X=<x>, Y=<y>, W=<w>, H=<h>, Color=float4(r, g, b, a));` | 每一个编辑器注释框 |
| `Node(Var="<name>", X=<x>, Y=<y>);` | 每一个具名表达式，按 X、再 Y、再名字排序 |

文本以 `DreamShader: ` 开头的注释框会被跳过——那些是生成的标记，不是作者编写的注释。

与这个设置无关，每个表达式还会被归到**最小的**那个包住它的注释框，这些归属会变成对应 `Graph` 语句周围的
`#Region` / `#EndRegion` 指令。关闭 layout 导出会去掉 `Layout` 块，但不会去掉
region。

参见 [Layout](../language/layout.md)。

<a id="diagnostics"></a>

## 诊断

运行期替换写作 `{Placeholder}`。

<a id="warnings-written-into-the-file"></a>

### 写进文件里的警告

每条只写出一次，作为 `// Decompiled from …` 头行下面的 `// Warning: …` 注释。它们
不会让导出失败。

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
| `Material has more than one '{Class}' node; output targets are de-duplicated by class, so only the first was exported.` | 两个同类的[终端输出节点](#terminal-output-nodes) |

<a id="export-failures"></a>

### 导出失败

| 消息 | 原因 |
| :-- | :-- |
| `No Material asset was provided.` | 一个空材质进到了反编译器 |
| `No MaterialFunction asset was provided.` | 一个空函数进到了反编译器 |
| `No asset was provided.` | 一个空资产进到了服务层 |
| `MaterialFunction '{Name}' does not expose any outputs.` | 该函数没有声明任何输出 |
| `DreamShader decompile supports Material and MaterialFunction assets only: {Path}` | 任何其他资产类 |
| `Decompile did not produce source text.` | 反编译报告失败但没有消息 |
| `DreamShader failed to resolve an output file path.` | 算出来的输出路径为空 |
| `DreamShader failed to create output directory '{Directory}'.` | 目录创建失败 |
| `DreamShader failed to write decompiled source '{File}'.` | 文件写入失败 |

<a id="editor-toasts"></a>

### 编辑器 toast

每个 *Export* 菜单项弹的是同一组 toast，2.0 的那几个也一样 *（自 2.0.0 起）*：

| Toast | 原因 |
| :-- | :-- |
| `DreamShader could not find the selected Material.` / `…Material Function.` | 从右键到点击之间资产被卸载了 |
| `DreamShader failed to export '{Asset}': {Reason}` | 反编译失败 |
| *（原始写入错误）* | 文件保存失败 |
| `Exported '{File}' but could not open it.` | 写出来了，但编辑器启动不了 |
| `Exported '{File}'.` | 成功 |

日志：Display 级的 `Exported '{Asset}' to '{File}'.`，以及
Warning 级的 `Failed to export '{Asset}' to a DreamShader source: {Reason}`。

<a id="known-round-trip-gaps"></a>

## 已知往返缺口

1.5.0 的实测行为。每一行都是导出文件还原不了的东西。

| 缺口 | 后果 | 变通办法 |
| :-- | :-- | :-- |
| 材质函数的设置从不写出 | 导出 `UMaterialFunction` 时，`Description`、`ExposeToLibrary`、`LibraryCategories` 和 `UserExposedCaption` 会丢失 | 手写一个 `Settings` 块——见[函数设置](../settings/function.md) |
| 只写出被认可的那组 `UMaterial` 属性 | 之外的属性——例如 `OpacityMaskClipValue`、`NumCustomizedUVs`、半透明光照模式、位移缩放和 Nanite 覆盖——保持类默认值 | 把这些键加进 `Settings`；它们按反射解析——见[着色器设置](../settings/material.md) |
| struct、array、map 和 set 类型的节点属性不参与反射 | 回退成 `UE.Expression` 的节点会丢失那部分状态，而且没有针对属性的警告 | 生成之后在材质上设置该属性，或者扩写导出的调用 |
| 节点注释文本（`Desc`）和节点 `SortPriority` 被丢弃 | 注释气泡和 pin 顺序还原不了 | 手工重新应用 |
| 节点位置取决于一个设置 | *Export Decompiled Layout* 关掉时，重新生成的图改为自动布局 | 保持该设置开启，或者手写 `Layout` |
| 前缀为 `DreamShader: ` 的注释框被丢弃 | 生成的标记按设计不会被重新写出 | 无需处理 |
| 指向 layer 或 layer blend 的 `MaterialFunctionCall` 回退成 `UE.Expression` | 该调用不会被表达成 `VirtualFunction` | 单独导出那个 layer 再调用它 |
| 没有指定函数的 `MaterialFunctionCall` 变成 `0.0` | 该分支被静默常量折叠 | 在原资产里重新指定函数再导出 |
| 环会写出一个默认字面量 | 成环的那条分支求值成常量 | 在原图里打断这个环 |
| 宽度超过四分量的 append 会被掩码收窄 | 分量被丢掉 | 检查写出的 swizzle |
| 1.x 反编译器不支持材质**实例** | `UMaterialInstanceConstant` 会被直接拒绝 | 用 [2.0 反编译器](#the-20-decompiler)，它写 `.dsi` |
| 纹理采样的 `GatherMode` 只在 UE 5.6 及以上往返 | 更老的引擎上该属性被省略 | 无 |
| `bHasPixelAnimation` 只在 UE 5.4 及以上进入写出的 flag 集合 | 更老的引擎上该 flag 被省略 | 无 |
| `Base.FrontMaterial` 和 `Substrate` 这个 shading-model 写法只存在于 UE 5.4 及以上 | 5.4 以下无法有意义地导出 Substrate 材质 | 无 |
| 生成的 `Name=` 指向 `Decompiled/…` | 重新编译会创建第二个资产，而不是替换原件 | 确认源文件可信之后改 `Name=` / `Root=` |
| 在 `Classic` 图布局风格下，大图会跳过自动布局 | 没有 `Layout` 块时，1200 个节点以上的重新生成的图回来可能视觉上毫无秩序；IR 风格（`Blocks`，2.0.0 起的默认）总是布局 | 保持 layout 导出开启，或用 IR 风格——见[图布局](../generation/graph-layout.md#when-layout-runs) |

<a id="example"></a>

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

<a id="the-20-decompiler"></a>

## 2.0 反编译器

```powershell
./dsc.ps1 decompile /Game/Materials/M_Steel                  # writes DShader/Decompiled/.../M_Steel.dss
./dsc.ps1 decompile /Game/Materials/MI_Steel_Worn            # a material instance: MI_Steel_Worn.dsi
./dsc.ps1 decompile -SourceFile DShader/Functions/Lib.dss    # every asset that source builds, in one file
./dsc.ps1 decompile /Game/Materials/M_Steel -Format Legacy   # the 1.x text described above
```

它就是把编译器倒着跑：**图 → IR → AST → 打印器**。

1. *导入。* 每个表达式读回成发射器为它写下的那个 IR 操作；其余的是基于内建目录的反射 `UE.<Class>(...)` 节点。内联 pin mask 按发射器自己的规则变回 swizzle，注释框变成 `#pragma region`，节点位置变成 `#pragma layout` 提示。有两个类把它们的 pin 保留在任何调用都无法命名的数组里，于是被读回成它们所计算的东西：一个 **Convert** 节点（Make / Break FloatN）读回成每个输出由哪些通道拼出（`DXY.xy`、`float3(Gain, 0.25, 0.75)`），一个 **Switch** 读回成引擎为它造出的那些分支（`0.0 == floor(s) ? a : 1.0 == floor(s) ? b : default`，或者 SwitchValue 没接任何东西时那个数字选中的唯一一个输入）—— 用于 case 没有名字的情况。已连接的 case 全都有名字的 Switch 就是 Switch 本身，正如下面的 Landscape 节点：`UE.Switch(SwitchValue = s, Dry = a, Wet = b, Inputs = "((InputName=\"Dry\"),(InputName=\"Wet\"))")`。VertexInterpolator 是一个自定义输出，同时也把一个值传下去，任何读它的地方读到的都是那个值。把 pin 保留在 struct 数组里、并按各自的条目命名的类 —— **LandscapeLayerBlend**、**LandscapeGrassOutput**、**LandscapePhysicalMaterialOutput**，以及 RenderTrace 中形状相同的 PhysicalMaterialOutput —— 用该数组作为详情面板粘贴的文本调用，pin 省略，每个 pin 用节点在标识符形式下显示的名字：`UE.LandscapeLayerBlend(Layer_Grass = g, Height_Rock = h, Layers = "((LayerName=\"Grass\"),...)")`。
2. *提升。* 没人读的节点被剪掉，发射器手工降级的形状被读回来（static bool 上的 StaticSwitch 是 `if` / `?:`，Set-attributes 链是 `m.X = ...`）。Substrate 节点保持它本身那个调用 —— `Substrate.Weight(...)`、`SSSMFP = Substrate.TransmittanceToMFP(...).MFP` —— 所以文本给图上显示的每个节点都点了名。只有可读形式才把 [Substrate 语法糖](../language-v2/substrate.md#decompiling)读回来，在图上精确等价的地方：什么都没设置的 `SubstrateAdd` / `SubstrateWeight` / `SubstrateHorizontalMixing` 是 `A + B`、`A * w` 和 `lerp(A, B, t)`，只喂一个 BSDF、别的什么都不喂的转换节点变成那个 BSDF 上的 `BaseColor = ...` / `Haziness = ...` / `Transmittance = ...`。
3. *书写。* 按显示顺序写 uniform（`@sort` 只在优先级不是该 uniform 的位置时使用），读了两次或被语句命名的值写成局部变量，从节点代码里恢复自定义函数及其辅助函数，layer 和 blend 用它们自己的签名，图里调用的每个函数资产写一个 `extern` 原型。
4. *证明。* 打印出的文本会被再解析一次（不行是 `DSH9089`），往返测试会编译它并比较 IR。

语言表达不了的都会被点名，从不静默丢掉：导入阶段是 `DSH9060`–`DSH9074`（缺失的函数资产、目录外的类、Preview pin 的默认表达式、Custom 节点上的额外 define），书写阶段是 `DSH9075`–`DSH9084`（改了名的标识符、内联写出的值的位置、从其调用推断出的 `extern`）。

下面这些引擎类仍然把 pin 或输出放在文本没有写法的数组里；其中一个类的节点回来是一个缺少那些连线（`DSH9070`）或状态（`DSH9068`）的反射调用，需要手工补完：

| 类 | 丢失的内容 |
| :-- | :-- |
| `MaterialCache` | 属性 pin 以及它的 tag 赋予它的那些输出 |
| `LayerStack` *（实验性）* | 层输入 |
| `Operator` *（仅新翻译器）* | 每个输入及其常量：它们全都存在同一个数组里 |

Substrate 节点按源码使用的名字写出 —— `Substrate.Slab(...)`，目录为该类列出的第一个别名 —— 而不是反射名 `SubstrateSlabBSDF`；两者解析到同一个类。

普通的 `UMaterialInstanceConstant` 写成 `.dsi`：父材质、它覆盖的 `#pragma instance` 键，以及与父材质不同的参数。

<a id="pipelines-dsp"></a>

### 管线（`.dsp`）

*（自 2.1.0 起）* `UDreamPassPipeline` 没有图可导入。它的字段被读回成 [`.dsp`](../language-v2/passes.md) 所绑定的那个 payload —— 即发射器所应用的映射反过来跑 —— 然后打印器把那个 payload 写成 `.dsp`：`#pragma pipeline`、uniform、buffer 和 pass，按资产的顺序，并省略每一个会读回成默认值的键。

```powershell
./dsc.ps1 decompile /Game/Passes/CP_Highlight                # DShader/Decompiled/Pipelines/Game/Passes/CP_Highlight.dsp
./dsc.ps1 decompile -SourceFile DShader/Passes/CP_Highlight.dsp
./dsc.ps1 decompile /Game/Passes/CP_Highlight -Out I:/Work/CP_Highlight.dsp
```

| 途径 | |
| :-- | :-- |
| `dsc decompile -Asset=<pipeline>` | 该管线；文本以 `// Decompiled by DreamShader from <object path>` 开头 |
| `dsc decompile -SourceFile=<file>.dsp` | `.dsp` 构建出的那条管线，不含头部行。先编译源码：在那之前没有可读的资产 |
| bridge 的 `decompile` 请求 | 同一次反编译，供编辑器扩展使用 |
| *Adopt Into Source* | 同一次读取，拼接回管线来源的那个 `.dsp`，而不是打印出来 —— 见 [Divergence](../generation/divergence.md#a-custom-pass-pipeline) |

没有 Content Browser 或 Material Content Browser 条目会反编译管线。

| 输出 | |
| :-- | :-- |
| 默认路径 | `<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp`，按与其他种类相同的分段规则 |
| `-Out` | 必须以 `.dsp` 结尾；任何其他扩展名都是 [`DSH9210`](../diagnostics/DSH9xxx.md)。`-Format=Legacy`，或者以 `.dsm` / `.dsf` 结尾的 `-Out`，会把管线交给 1.x 反编译器，后者会拒绝它：管线没有 1.x 文本 |
| `-KeepAssetPath` | `.dsp` 按它的文件给管线命名，并且没有 `/// @name`，所以只有把文件写到管线的源码所属位置才能保住路径；写到别处是 `DSH9224`（警告），并指明文本会在哪里构建 |
| `-Readable` | 无效果 |
| `Material = "…"` | 当输出文件的源码根目录下的产物中，恰好有一个该名字的材质或实例且就是这一个，并且有 `.dss` 构建它时，用材质的裸名；否则用它的包名，或者对于名字与包名不一致的资产用其对象路径 |
| `Shader = "…"` | 资产持有的那种虚拟路径；相对于构建该管线时所用的 `.dsp` 的路径会被改写成相对于输出文件所在文件夹的路径 |
| Layers | `Layer(...)` 过滤器保留编译器存在 layer 位旁边的那些名字。没有存储名字的位 —— 手工编辑 —— 通过当前的 layer 表读取（`DSH9214`，info） |

文本无法按资产本身那样表达的内容是一条警告，用英文以 `// Warning: DSHnnnn: …` 行的形式写在文件头部，并作为诊断抛出：

| 代码 | 资产持有的 | 文本 |
| :-- | :-- | :-- |
| `DSH9211` | 既没有材质也没有 shader 的全屏 pass | 两者都不点名，且在给定其中一个之前不会构建 |
| `DSH9212` | 两者都有的全屏 pass | 保留材质，那是该 pass 所绘制的东西 |
| `DSH9215` | 项目的 layer 表没有对应名字的 layer 位 | 省略它们 |
| `DSH9216` | 不在任何视图中运行的管线 | 省略 `Views`，于是重新构建会在 `Game \| Editor` 中运行 |
| `DSH9217` | 不检查任何 usage flag 的 mesh pass | 省略 `Usage`，于是重新构建会检查默认集合 |
| `DSH9218` | 默认值不是 2D 纹理的纹理参数 | 将其声明为 `Texture2D`；在默认值变成 2D 纹理之前它可能无法构建 |
| `DSH9219` | 绑定到纹理常量的 `param` | 省略该绑定 |
| `DSH9220` | 自带深度但没有对应 buffer 的 mesh pass | 把 `Own()` 写成空；在指定 `Depth32` buffer 之前它不会构建 |

固定大小 buffer 的 `Scale`（资产可能仍然持有）不会被写出：`Size` 会将其排除。

**重新解析检查。** 文本被交回之前会再次解析，并在不带引擎的情况下进行绑定——
引用按原样读取，所有需要项目的检查都被跳过——然后把它绑定到的载荷与资产的载荷进行比较，只有引擎知道的事实（引用解析成什么、
`.usf` 的 `[numthreads]`）在文本以相同方式拼写引用时取自资产。一段无法解析的文本是 `DSH9221`；
一段无法绑定进管线的文本是 `DSH9222`——资产违反了 `.dsp` 所检查的某条规则，这可能是手动编辑造成的，
也可能是反编译器的问题；一段读回来却是不同管线的文本是 `DSH9223`，会指出第一处差异。三者都是警告：文件按原样写入，
它们不会被复制到文件头部。

<a id="see-also"></a>

## 另请参阅

- [编辑器集成](editor-integration.md) —— *Export .dss* 和 *Export Legacy .dsm* / *.dsf* 菜单项
- [命令行](commandlet.md) —— `-run=DreamShader decompile`、`-Asset` 和 `-Out`
- [UE.Expression](../builtins/ue-expression.md) —— 回退路径写出的那种通用调用
- [Layout](../language/layout.md) —— `Node` 和 `Comment` 指令，以及 `#Region`
- [Shader 设置](../settings/material.md) —— 导出器能写出和写不出的那些设置
- [函数设置](../settings/function.md) —— 反编译出的 `.dsf` 会缺失的那些键
- [VirtualFunction](../language/virtual-function.md) —— 为每个被调用的函数写出的声明
- [VirtualFunction 工具](virtual-function-tools.md) —— 同一个声明构建器，从资产菜单驱动
- [资产路径](../generation/asset-paths.md) —— 写出的 `Name=` 解析成什么
- [图布局](../generation/graph-layout.md) —— 没有 `Layout` 块时会发生什么
- [项目设置](../settings/project.md) —— *Export Decompiled Layout* 和 *Source Directory*
- [Substrate 内置项](../builtins/substrate.md) —— `Base.FrontMaterial` 背后的 UE 5.4 门槛
- [Custom Pass 管线 —— `.dsp`](../language-v2/passes.md) —— 管线反编译出的文本
- [Divergence](../generation/divergence.md#a-custom-pass-pipeline) —— 对在详情面板中编辑过的管线使用 Adopt Into Source
