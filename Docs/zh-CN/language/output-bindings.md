---
title: "输出绑定"
description: "输出绑定"
---

<a id="output-bindings"></a>

# 输出绑定

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **输出绑定**

位于 `Shader` 的 `Outputs` 部分中的语句，用于将图值连接到材质属性输入，或连接到显式创建的 `UMaterialExpression` 上的引脚。

| | |
| :-- | :-- |
| 声明于 | `.dsm` — 仅在 `Shader` 块的 `Outputs` 部分内 |
| 类型 | 语句 |
| 生成 | 到 `UMaterial` 的属性输入、或到所创建输出节点的连接 |

绑定的 2.0 写法是对材质参数的赋值，`m.EmissiveColor = Color;` — 参见 [DreamShaderLang 2.0](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会以该方式重写 `.dsm`。

<a id="synopsis"></a>

## 概要

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}
```

```c
output-declaration := <type> <name> [ = <expression> ] ;

output-binding     := Base. <target> = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) { <pin-binding>… } [;]

pin-binding        := Pin[ <index> ] = <source> ;
```

`<source>` 是一个表达式 *(自 2.0.0 起)*：可以是 1.x 写法中的输出变量名，也可以是 `Graph` 表达式可以是的任何内容 — `Base.FrontMaterial = Substrate.Layer(Coat, Body, 0.01);`。它在 `Graph` 运行之后读取，并遵循 1.x `Graph` 表达式的规则（`DSH2200`–`DSH2222`）。`<index>` 前后的 `[` 和 `]` 是**字面量**。

该部分按其起始标记逐个标记、逐条语句地读取：

| 语句起始于 | 读取为 |
| :-- | :-- |
| 两个词，`<type> <name>` | 输出声明，带或不带初始化器 *(初始化器自 1.3.4 起)* |
| `Base` `.` `<name>` `=` | 材质属性绑定 |
| `Expression` `(` … `)` `.` | 表达式绑定的语句形式 |
| `Expression` `(` … `)` `{` | [块形式](#block-form) *(自 1.9.0 起)* |
| 其他任何内容，包括除 `Expression( … )` 之外的任何内容之后的 `{` | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) — 或当它作为 `Expression( … )` 目标开始时为 [`DSH3267`](../diagnostics/DSH3xxx.md#dsh3267) |

`Expression( … ) { … }` 是 `Outputs` 内部**唯一**会打开花括号的构造，正如 `Group("Name") { … }` 是 [`Properties`](properties.md) 内部的唯一构造。

声明会成为材质入口的局部变量，位于 `Graph` 之前；没有初始化器的声明从零开始，如同 1.x（纹理或 `Substrate` 输出没有零值，必须由 `Graph` 赋值）。绑定会成为 `Graph` 之后对材质的赋值。声明语法见 [Inputs / Outputs / Results](inputs-outputs.md#outputs-in-a-shader)。

<a id="binding-target-kinds"></a>

## 绑定目标种类

| 目标文本起始于 | 种类 | 含义 |
| :-- | :-- | :-- |
| `Base.`（不区分大小写） | 材质属性 | 连接到 `UMaterial` 的某个属性输入 |
| `Expression(`（不区分大小写） | 表达式输入 | 创建一个 `UMaterialExpression` 并连接到其某个输入引脚 |
| 其他任何内容 | — | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) |

<a id="basetarget-catalogue"></a>

## `Base.<target>` 目录

*(自 2.0.0 起)* 名称是引擎自身的名称：引擎材质属性表中每个具有 `EMaterialProperty` 的属性，均使用引擎赋予其的名称，外加下面的 1.x 别名。名称进行**精确**匹配；在 1.x 文件中，仅忽略大小写即可匹配的名称仍会解析，并给出警告 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)（旧规则 L19）。不匹配任何内容的名称会引发 [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200)。

1.x 文档中记载的写法，以及各自解析到的属性和值类型；引擎的表可能包含更多名称，那些名称也会解析：

| `Base.` 写法 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `BaseColor` | `MP_BaseColor` | Float3 |
| `MaterialAttributes`, `Attributes` | `MP_MaterialAttributes` | MaterialAttributes |
| `FrontMaterial` *(自 UE 5.4 起)* | `MP_FrontMaterial` | Substrate |
| `EmissiveColor`, `Emissive` | `MP_EmissiveColor` | Float3 |
| `Opacity` | `MP_Opacity` | Float1 |
| `OpacityMask` | `MP_OpacityMask` | Float1 |
| `Metallic` | `MP_Metallic` | Float1 |
| `Specular` | `MP_Specular` | Float1 |
| `Roughness` | `MP_Roughness` | Float1 |
| `Normal` | `MP_Normal` | Float3 |
| `AmbientOcclusion`, `AO` | `MP_AmbientOcclusion` | Float1 |
| `Refraction` | `MP_Refraction` | Float3 |
| `WorldPositionOffset`, `WPO` | `MP_WorldPositionOffset` | Float3 |
| `PixelDepthOffset`, `PDO` | `MP_PixelDepthOffset` | Float1 |
| `SubsurfaceColor` | `MP_SubsurfaceColor` | Float3 |
| `ClearCoat` | `MP_CustomData0` | Float1 |
| `ClearCoatRoughness` | `MP_CustomData1` | Float1 |
| `CustomData0` | `MP_CustomData0` | Float1 |
| `CustomData1` | `MP_CustomData1` | Float1 |
| `DiffuseColor` | `MP_DiffuseColor` | Float3 |
| `SpecularColor` | `MP_SpecularColor` | Float3 |
| `SurfaceThickness` | `MP_SurfaceThickness` | Float1 |
| `Displacement` | `MP_Displacement` | Float1 |
| `CustomizedUV0` … `CustomizedUV7` | `MP_CustomizedUVs0` … `MP_CustomizedUVs7` | Float2 |
| `Anisotropy` | `MP_Anisotropy` | Float1 |
| `Tangent` | `MP_Tangent` | Float3 |

> [!WARNING]
> 1.x 还接受 `CustomizedUVs0` … `CustomizedUVs7`。引擎将这些属性命名为 `CustomizedUV0` … `CustomizedUV7`，因此 `s` 写法是 `DSH5200` *(自 2.0.0 起)*；请写作 `CustomizedUV<n>`。

> [!NOTE]
> `ClearCoat` 和 `CustomData0` 是同一个材质属性，`ClearCoatRoughness` 和 `CustomData1` 也是如此。在同一个 `Shader` 中绑定一对写法的两者会写入同一个输入两次；后一条语句生效，且不给出诊断。

<a id="fork-only-targets"></a>

### 仅分支版目标

这些属性仅存在于 Moon 引擎分支中；在标准 Unreal 构建中它们是 `DSH5200`。`Mooa…` 别名是该分支重命名前的写法。

| `Base.` 写法 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `MoonEncodedAttribute0`, `MooaEncodedAttribute0` | `MP_MoonEncodedAttribute0` | Float4 |
| `MoonEncodedAttribute1`, `MooaEncodedAttribute1` | `MP_MoonEncodedAttribute1` | Float4 |
| `MoonEncodedAttribute2`, `MooaEncodedAttribute2` | `MP_MoonEncodedAttribute2` | Float4 |
| `MoonEncodedAttribute3`, `MooaEncodedAttribute3` | `MP_MoonEncodedAttribute3` | Float4 |
| `MoonEncodedAttribute4`, `MooaEncodedAttribute4` | `MP_MoonEncodedAttribute4` | Float4 |

<a id="targets-with-side-effects"></a>

## 具有副作用的目标

<a id="basematerialattributes"></a>

### `Base.MaterialAttributes`

*(自 1.2.5 起)*

绑定 `Base.MaterialAttributes` 会设置 `UMaterial::bUseMaterialAttributes = true` — 即**使用材质属性**复选框 — 从而使材质暴露单个属性输入，而不是各个单独的属性输入。所绑定的值必须是 `MaterialAttributes` 值；其他任何值都会引发 [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228)。

有关该值类型、成员写入和读取，请参见 [MaterialAttributes](../graph/material-attributes.md)。

<a id="basefrontmaterial"></a>

### `Base.FrontMaterial`

*(自 UE 5.4 起)*

所绑定的值必须是 Substrate 值，由 [`Substrate.*` 内置函数](../builtins/substrate.md)或 [Substrate 语法糖](../language-v2/substrate.md)生成；其他任何值都会引发 `DSH4228`。

*(自 2.0.0 起)* `FrontMaterial` 是普通属性：1.x 生成器对它施加的额外规则 —— 强制
着色模型为 Substrate、拒绝除 `"Substrate"` 以外的显式 `ShadingModel`、
拒绝它与 `Base.MaterialAttributes` 并列、以及整个表面的 `Custom` 节点路径 —— 均不再适用。在材质需要它的
`Settings` 中写入
`ShadingModel = "Substrate";`（在 UE 5.3 上，该设置为
[`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128)）。针对旧版属性编写的材质在
Substrate 项目中如何读取，由 `Substrate =` 设置决定 —— 请参见
[Substrate sugar](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-)。

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

<a id="the-expression--pini-target"></a>

## `Expression( … ).Pin[i]` 目标

非 `Base` 绑定形式会创建一个任意 `UMaterialExpression` —— 实际上是一个*自定义输出*
节点 —— 并将源连接到其某个输入引脚。

```c
Expression( Class = "<ExpressionClass>" [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
```

目标的两种形式都会成为材质入口中的一个 `UE.Expression(Class = "…", …)` 节点调用，
因此目标受与 `Graph` 中
[`UE.Expression`](../builtins/ue-expression.md) 调用相同的规则约束。

<a id="requirements-in-the-order-they-are-checked"></a>

### 要求，按检查顺序排列

| 要求 | 违反时的代码 |
| :-- | :-- |
| 头部为 `Expression(` `<key> = <value>` [`,` …] `)`，每个值非空 | [`DSH3267`](../diagnostics/DSH3xxx.md#dsh3267) |
| 没有键被写入两次（比较时忽略大小写） | `DSH3267` |
| 存在 `Class` 参数 | `DSH3267` |
| `)` 之后为 `.Pin[<index>] = <source>` 或 `{ … }` 块，索引为整数字面量 | `DSH3267` |
| `=` 后面有内容 | [`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270) |
| 该类是引擎拥有的类 | [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212) |
| 该索引指向该类的某个输入引脚 | [`DSH5285`](../diagnostics/DSH5xxx.md#dsh5285) |
| 其他每个参数都指向该类的某个引脚或属性 | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) |

`Class` 匹配时忽略大小写。下面的复用键会将其他每个键转为小写并去掉每个
值的引号；节点调用按原样携带每个参数。

<a id="class-resolution"></a>

### 类解析

*(自 2.0.0 起)* 类在引擎的表达式目录中按精确名称查找，顺序如下：短名称（`ThinTranslucentMaterialOutput`）、类名称
（`MaterialExpressionThinTranslucentMaterialOutput`）、类路径
（`/Script/Engine.MaterialExpressionThinTranslucentMaterialOutput`）、前面加上
`MaterialExpression` 的短名称，以及带有其 `U`
（`UMaterialExpressionThinTranslucentMaterialOutput`）的 C++ 名称，后者 1.x 会拒绝。在 1.x 文件中，若某名称
仅忽略大小写地与一个类匹配，也能解析，并给出警告 `DSH5276`。其他任何情况都是
`DSH5212`。同样的规则记录在 [`UE.Expression`](../builtins/ue-expression.md) 上。

<a id="other-arguments"></a>

### 其他参数

除 `Class` 以外的每个参数都会与该类进行匹配：先匹配输入引脚，再匹配反射
属性。值会被读取为数字、`true` / `false`、带引号的字符串或裸词，裸词指向
图中的一个值，或者对于枚举属性，指向一个枚举项。

*(自 2.0.0 起)* 指向**输入**引脚的参数会像 `Pin[<index>]` 那样连接该引脚；
1.x 会拒绝它。将同一个引脚双向连接会得到 [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215)。既不是引脚也不是属性的名称会得到
`DSH5213` —— 但在 1.x 文件中针对图值的情况除外，该名称会在节点存在后
按该名称连接（[`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)，信息；
当构建出的节点没有该引脚时为 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)）。属性无法接受的值会得到
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)。

<a id="pin-indices-and-node-reuse"></a>

### 引脚索引与节点复用

- `Pin[<index>]` 是按引擎声明顺序对节点输入列表的从零开始的索引。`Expression(Class="ThinTranslucentMaterialOutput").Pin[0]` 为 `TransmittanceColor`，
  `.Pin[1]` 为 `SurfaceCoverage`。超出末尾的索引会得到 `DSH5285`。
- 节点的每个引脚可以在语句形式和[块](#block-form)形式中绑定**一次**：
  [`DSH3268`](../diagnostics/DSH3xxx.md#dsh3268)。
- 节点在同一个 `Outputs` 区段内按**类加排序后的参数列表去重**。两个
  其 `Expression( … )` 规格完全相同的绑定共享一个节点并绑定不同引脚；
  任何参数不同就会创建第二个节点，同一个头部出现在第二个 `Outputs`
  区段中也是如此。
- 源必须是图中的一个值：没有任何声明指向的名称会得到
  [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)，而引脚无法接受的值会得到
  [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)。必需引脚未连接会得到警告
  [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279)。
- 节点落在哪里是[图形布局](../generation/graph-layout.md)的职责。

此形式最常用于自定义输出时的引脚顺序：

| `Class` | `Pin[i]`，按顺序 |
| :-- | :-- |
| `ThinTranslucentMaterialOutput` | `0` TransmittanceColor、`1` SurfaceCoverage |
| `ClearCoatNormalCustomOutput` | `0` Input |
| `RuntimeVirtualTextureOutput` | `0` BaseColor、`1` Specular、`2` Roughness、`3` Normal、`4` WorldHeight、`5` Opacity、`6` Mask、`7` Displacement、`8` Mask4 |

任何其他 `UMaterialExpressionCustomOutput` 子类也以相同方式工作；其引脚顺序是引擎声明输入的顺序，材质编辑器在节点上从上到下显示该顺序。一个
完整的 `RuntimeVirtualTextureOutput` 文件是[示例 15](../examples/index.md#15-runtime-virtual-texture-sampling-and-writing)。

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;
    float3 CoatNormal;

    // Both statements reuse one ThinTranslucentMaterialOutput node.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[0] = Transmittance;
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;

    // A different class → a second node.
    Expression(Class="ClearCoatNormalCustomOutput").Pin[0] = CoatNormal;
}
```

<a id="block-form"></a>

## 块形式

*(自 1.9.0 起)*

一个引脚众多的终端节点在语句形式中必须为每个引脚重复其整个 `Expression( … )` 规格，
而且由于该规格*就是*去重键，**一个被遗忘的参数会悄无声息地把节点拆成两个**。块形式只需写入一次规格：

```c
Expression( <key> = <value> [, <key> = <value> ]… )
{
    Pin[ <index> ] = <source> ;
    …
}
```

```c
Outputs = {
    float _PhaseG_Out;
    float _PhaseG2_Out;
    float _PhaseBlend_Out;

    Expression(Class="VolumetricAdvancedMaterialOutput",
        PerSamplePhaseEvaluation="false",
        MultiScatteringApproximationOctaveCount=0,
        bGroundContribution="false")
    {
        Pin[0] = _PhaseG_Out;
        Pin[1] = _PhaseG2_Out;
        Pin[2] = _PhaseBlend_Out;
    }
}
```

<a id="the-one-node-guarantee"></a>

### 单节点保证

块形式和语句形式读入为同一种东西：每个复用键对应一个节点调用，该键下的每个
引脚都绑定为其参数之一。一个块的所有引脚共享头部的类和
参数列表，这正好就是[复用键](#pin-indices-and-node-reuse) —— 因此一个包含 *N* 个引脚的块
总是产生**一个**节点。参数列表不会在各个引脚之间发生偏移，因为只有一份
副本。

其他一切都不变：块形式和语句形式在下游无法区分。它们
共享相同的类解析、相同的参数检查、相同的引脚索引检查和相同的
诊断。

<a id="rules"></a>

### 规则

| 规则 | 详情 |
| :-- | :-- |
| 什么可以打开花括号 | 仅 `Expression( … )`。变量或 `Base.` 目标之后的 `{` 是 `DSH3266`；在 `Expression( … ).Pin[i]` 目标之后它是 `DSH3267` |
| 头部形态 | `Expression(` … `)`，且闭合的 `)` 之后没有任何内容——引脚选择器在块内按每条语句书写 |
| 内部语句 | 仅 `Pin[ <index> ] = <source> ;`——否则为 `DSH3267` |
| 注释与空行 | 块内任意位置均可 |
| 空块 | [`DSH3269`](../diagnostics/DSH3xxx.md#dsh3269)——不绑定任何引脚的块不会创建任何内容，因此会被拒绝而非忽略 |
| 未闭合的块 | 解析会一直运行到文件末尾：[`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) |
| 结尾的 `;` | 在闭合的 `}` 之后可选，如同 `Group("Name") { … }` 之后一样 |
| 重复 | 同一个节点可以多次书写一个块，并且可以在同一个 `Outputs` 节内与语句形式的绑定自由混用 |

<a id="mixing-the-two-forms"></a>

### 混用两种形式

一个块和一条在类和参数列表上一致的松散语句寻址的是**同一个**节点，因此它们填充该节点的不同引脚：

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;

    Expression(Class="ThinTranslucentMaterialOutput")
    {
        Pin[0] = Transmittance;
    }

    // Same specification → same node → this fills its other pin.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;
}
```

<a id="each-pin-once"></a>

### 每个引脚一次

“一个引脚只能绑定一次”的规则在读取该节时检查，涵盖两种形式，以类 + 排序后的参数列表 + 引脚索引为键。同一个引脚的第二次绑定是 `DSH3268`，在第二次绑定时报告。

> [!NOTE]
> 参数列表不同的两个块——哪怕只差一个参数——是两个不同的节点，并且各自有自己的 `Pin[0]`。这不是双重绑定，也不会被诊断；这正是使语句形式中的重复变得危险的同一规则。

<a id="diagnostics"></a>

### 诊断

1.x 解析器针对此形式的代码 `DSH3133`–`DSH3137` 不再触发 *(自 2.0.0 起)*：

| 1.x 代码 | 现在触发 |
| :-- | :-- |
| `DSH3133`——除 `Expression( … )` 之外任何内容之后的花括号 | `DSH3266`，或 `Expression( … ).Pin[i]` 目标之后的 `DSH3267` |
| `DSH3134`——未终止的块 | `DSH2150` |
| `DSH3135`——块中除 `Pin[i] = <source>;` 之外的语句 | `DSH3267` |
| `DSH3136`——空块 | `DSH3269` |
| `DSH3137`——引脚被绑定两次 | `DSH3268` |

<a id="the-reserved-name-return"></a>

## 保留名称 `return`

*(自 2.0.0 起)* `return` 是每个文件中的关键字。它不能命名输出声明（`DSH3266`），也不是绑定源：`Base.EmissiveColor = return;` 是 [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151)。1.x 为该名称保留了其全表面 `Custom` 节点路径，而编译器不再具有该路径。

<a id="validation-rules"></a>

## 校验规则

| 条件 | 代码 |
| :-- | :-- |
| 声明的类型不是类型 | [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) |
| 在 `Outputs` 中声明了两次的名称 | [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220) |
| 没有任何内容声明的源名称 | `DSH4200` |
| 材质没有的 `Base.` 名称 | `DSH5200` |
| 材质引脚无法接受的值；更宽的向量不会被收窄到材质引脚中，即使在 1.x 文件中也不会 | `DSH4228` |
| `Expression( … )` 引脚无法接受的值 | `DSH5214` |

对 `Outputs` 声明的名称进行 `Graph` 声明会被读取为对该输出的赋值，这正是 1.x 对其的处理方式。

<a id="notes"></a>

## 说明

- 没有 `Outputs` 节的 `Shader` 在构建时会产生警告 [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256)；既没有 `Graph` 也没有 `Outputs` 中任何内容的会被 [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)。
- 绑定和声明可以自由交错；绑定可以引用同一节中稍后声明的变量。
- 重复的 `Outputs` 节会追加到两个列表。
- 在 `Shader` 中的任何 `Outputs` 语句上都不接受 `[ … ]` 元数据块。
- 每个绑定都会通过一对生成的 `NamedReroute` 进行路由，其名称按照书写时的 `Base.` 名称命名为 `DS_<pin>_<n>`，因此材质根节点在大型图中保持可读。参见 [图布局](../generation/graph-layout.md)。
- 绑定到当前着色模型或混合模式不使用的属性不会被诊断——Unreal 只是让该输入不被读取。请检查材质编辑器中灰显的输入。材质完全没有输入的属性是 [`DSH8217`](../diagnostics/DSH8xxx.md#dsh8217)。

<a id="diagnostics-1"></a>

## 诊断

每个诊断都带有它所针对的语句的行和列。

<a id="reading-the-section"></a>

### 读取该节

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3266` | 该节不是以 `{` 打开的；语句既不是声明也不是绑定；声明不以 `;` 结尾 |
| `DSH3267` | `Expression( … )` 目标格式有误：其头部、其引脚选择器、缺失的 `Class`、重复的键、块中除 `Pin[i] = <source>;` 之外的语句 |
| `DSH3268` | 一个节点的一个引脚被绑定两次 |
| `DSH3269` | `Expression( … ) { }` 块不绑定任何引脚 |
| `DSH3270` | `=` 之后没有任何内容 |
| `DSH2150` | 文件在该节内或块内结束 |
| `DSH2151` | 源不是表达式，例如 `return` |
| [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) | 源或初始化器使用了 1.x `Graph` 表达式不具有的内容 |

<a id="binding"></a>

### 绑定

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4201` | 声明的类型不是类型 |
| `DSH4220` | 名称被声明两次 |
| `DSH4200` | 源没有命名任何已声明的内容 |
| `DSH4228` | 材质引脚无法接受的值 |
| `DSH5200` | 未知的 `Base.` 名称 |
| `DSH5276` | 警告：仅忽略大小写才匹配的 `Base.` 名称、类或参数 |
| `DSH5212` | `Class=` 未命名此引擎的任何表达式类 |
| `DSH5285` | `Pin[i]` 超过了节点的最后一个输入 |
| `DSH5213` | 参数既未命名引脚也未命名该类的属性 |
| `DSH5291` | 信息：在 1.x 文件中，携带图值的未知参数稍后按名称连接 |
| `DSH4215` | 在一次节点调用中一个引脚被连接两次 |
| `DSH5214` | `Expression( … )` 引脚无法接受的值 |
| `DSH5279` | 警告：节点的必需引脚未连接 |

<a id="building-the-asset"></a>

### 构建资产

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH8217` | 材质没有绑定属性的输入 |
| [`DSH8211`](../diagnostics/DSH8xxx.md#dsh8211) | 表达式类在目录中但不在运行的引擎中 |
| `DSH8212` | 按名称连接的引脚在已构建的节点上不存在 |
| `DSH8213` | 反射参数的值无法写入该节点 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 无法创建节点 |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="DreamShaderTests/Corpus/M_Outputs")
{
    Properties = {
        vec3  Tint = vec3(0.4, 0.8, 1.0);
        float A    = 0.75;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = A;
    }
}
```

产生的连接：

```text
UMaterial /Game/DreamShaderTests/Corpus/M_Outputs
  MP_EmissiveColor  <- NamedReroute DS_EmissiveColor_<n>  <- VectorParameter "Tint"  RGB output
  MP_Opacity        <- NamedReroute DS_Opacity_<n>        <- ScalarParameter "A"     R output
  bUseMaterialAttributes = false
```

<a id="see-also"></a>

## 另请参阅

- [Shader](shader.md) — 拥有 `Outputs` 的代码块
- [Inputs / Outputs / Results](inputs-outputs.md) — `Outputs` 的声明侧
- [MaterialAttributes](../graph/material-attributes.md) — 属性值类型与成员写入
- [Substrate builtins](../builtins/substrate.md) — 为 `Base.FrontMaterial` 生成值
- [`UE.Expression`](../builtins/ue-expression.md) — 与 `Properties` 和 `Graph` 中使用的相同 `Class=` 解析
- [Metadata](../parameters/metadata.md) — `Expression( … )` 参数共用的字面量语法
- [Material settings](../settings/material.md) — `ShadingModel`、`BlendMode` 及反射键
- [Shading model / blend mode values](../settings/material-enums.md) — 所有可接受的枚举拼写
- [Graph](../graph/index.md) — 输出变量赋值的位置
- [Graph layout](../generation/graph-layout.md) — 重路由对与 `Material Output` 分组
- [DreamShaderLang 2.0](../language-v2/index.md) — `m.<Attribute> = …`、`.dss` 拼写
- [Diagnostics index](../diagnostics/index.md) — 每个代码
