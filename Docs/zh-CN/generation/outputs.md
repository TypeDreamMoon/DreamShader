---
title: "输出绑定"
description: "Outputs 段——声明输出变量、Base.<target> 清单，以及 Expression(...).Pin[i] 与 Expression(...) { … } 两种形式。"
---

`Shader` 的 `Outputs` 段同时干两件事：

1. 它**声明** `Graph` 将要写入的变量，并且
2. 它将值**绑定**到 Unreal 材质属性输入，或绑定到显式创建的
   `UMaterialExpression` 节点上的引脚。

真正到达材质的是绑定，而不是声明。一个带有 `Graph` 但没有 `Outputs`
段的 `Shader` 会带着警告 `DSH2256` 构建，且没有任何东西连接到材质；而一个没有 `Graph`、其
`Outputs` 也什么都不计算的 `DSH2255`（自 2.0.0 起）。

在 2.0 中，绑定是对材质参数的赋值，`m.EmissiveColor = Color;` —— 参见
[DreamShaderLang 2.0](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会以那种方式重写一个
`.dsm`。

<a id="synopsis"></a>

## 语法概要

| 记法 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换为真实值；尖括号本身不用输入。 | `Name = <string>` |
| `[ x ]` | 可选——整组可以省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 选择——从用 \| 分隔的备选项中恰好取一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 重复——前一项可以出现任意次数。 | `<property-declaration> …` |

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}

output-declaration := <type> <name> [ = <expression> ] ;

output-binding     := Base. <target> = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) { <pin-binding>… } [;]

pin-binding        := Pin[ <index> ] = <source> ;
```

`<source>` 是一个**表达式**（自 2.0.0 起）：可以是 1.x 写法中的输出变量名，也可以是
`Graph` 表达式可以是的任何东西——`Base.FrontMaterial = Substrate.Layer(Coat, Body, 0.01);`。它在
`Graph` 运行之后被读取，并遵循 1.x `Graph` 表达式的规则（`DSH2200`–`DSH2222`）。
`<index>` 两侧的 `[` 和 `]` 是**字面量**。

**注意：**

在整个 1.x 中，绑定的右侧是一个**名字**，而 `Base.EmissiveColor = Tint * Strength;`
不是绑定。自 2.0 起它就是一个表达式；在 `Graph` 中计算值并绑定该变量
仍然是最易读的形式。

该段按 token 逐个读取，一次一条语句，依据其开头的 token 分类：

| 语句开头 | 读作 |
| :-- | :-- |
| 两个词，`<type> <name>` | 输出声明，带或不带初始化器（自 1.3.4 起） |
| `Base` `.` `<name>` `=` | 材质属性绑定 |
| `Expression` `(` … `)` `.` | 表达式绑定的语句形式 |
| `Expression` `(` … `)` `{` | [块形式](#block-form)（自 1.9.0 起） |
| 其他任何内容，包括除 `Expression( … )` 之外的任何内容后面跟的 `{` | `DSH3266`；当它以 `Expression( … )` 目标开头时是 `DSH3267` |

`Expression( … ) { … }` 是 `Outputs` 内**唯一**能开花括号的构造，正如
`Group("Name") { … }` 是 `Properties` 内唯一的一个。

声明会成为材质入口的一个局部变量，位于 `Graph` 之前；没有初始化器的声明
从零开始，和 1.x 一样（纹理或 `Substrate` 输出没有零值，必须由
`Graph` 赋值）。绑定会成为 `Graph` 之后对材质的一次赋值。

<a id="binding-target-kinds"></a>

## 绑定目标种类

| 目标文本开头 | 种类 | 含义 |
| :-- | :-- | :-- |
| `Base.`（不区分大小写） | 材质属性 | 连接到 `UMaterial` 的某个属性输入 |
| `Expression(`（不区分大小写） | 表达式输入 | 创建一个 `UMaterialExpression` 并连接到它的某个输入引脚 |
| 其他任何内容 | — | `DSH3266` |

<a id="the-basetarget-catalogue"></a>

## `Base.<target>` 清单

（自 2.0.0 起）这些名字就是引擎自己的名字：引擎材质属性表中每一个具有 `EMaterialProperty` 的属性，
都以其引擎给定的名字，加上下面的 1.x 别名。名字**精确**匹配；在 1.x 文件中，仅忽略大小写才匹配的名字仍然能解析，
但会给出警告 `DSH5276`（旧规则 L19）。什么都不匹配的名字是 `DSH5200`。

1.x 文档中记录的拼写，以及各自解析到的属性和值类型；引擎的表可能
包含更多名字，那些也能解析：

| `Base.` 拼写 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `BaseColor` | `MP_BaseColor` | Float3 |
| `MaterialAttributes`、`Attributes` | `MP_MaterialAttributes` | MaterialAttributes |
| `FrontMaterial`（自 UE 5.4 起） | `MP_FrontMaterial` | Substrate |
| `EmissiveColor`、`Emissive` | `MP_EmissiveColor` | Float3 |
| `Opacity` | `MP_Opacity` | Float1 |
| `OpacityMask` | `MP_OpacityMask` | Float1 |
| `Metallic` | `MP_Metallic` | Float1 |
| `Specular` | `MP_Specular` | Float1 |
| `Roughness` | `MP_Roughness` | Float1 |
| `Normal` | `MP_Normal` | Float3 |
| `AmbientOcclusion`、`AO` | `MP_AmbientOcclusion` | Float1 |
| `Refraction` | `MP_Refraction` | Float3 |
| `WorldPositionOffset`、`WPO` | `MP_WorldPositionOffset` | Float3 |
| `PixelDepthOffset`、`PDO` | `MP_PixelDepthOffset` | Float1 |
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

**警告：**

1.x 还接受 `CustomizedUVs0` … `CustomizedUVs7`。引擎将这些属性命名为
`CustomizedUV0` … `CustomizedUV7`，因此 `s` 拼写是 `DSH5200`（自 2.0.0 起）；请写
`CustomizedUV<n>`。

**注意：**

`ClearCoat` 和 `CustomData0` 是**同一个**材质属性，`ClearCoatRoughness` 和
`CustomData1` 也是。在同一个 `Shader` 中绑定一对中的两种拼写会写入同一个输入两次；
后面的语句生效，且没有诊断信息。

<a id="fork-only-targets"></a>

### 仅 fork 可用的目标

这些属性只存在于 Moon 引擎分支；在原版 Unreal 构建上它们是 `DSH5200`。
`Mooa…` 拼写是该分支改名前的名字，仍作为别名接受。

| `Base.` 拼写 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `MoonEncodedAttribute0`、`MooaEncodedAttribute0` | `MP_MoonEncodedAttribute0` | Float4 |
| `MoonEncodedAttribute1`、`MooaEncodedAttribute1` | `MP_MoonEncodedAttribute1` | Float4 |
| `MoonEncodedAttribute2`、`MooaEncodedAttribute2` | `MP_MoonEncodedAttribute2` | Float4 |
| `MoonEncodedAttribute3`、`MooaEncodedAttribute3` | `MP_MoonEncodedAttribute3` | Float4 |
| `MoonEncodedAttribute4`、`MooaEncodedAttribute4` | `MP_MoonEncodedAttribute4` | Float4 |

<a id="targets-with-side-effects"></a>

## 带副作用的目标

<a id="basematerialattributes"></a>

### `Base.MaterialAttributes`

（自 1.2.5 起）

绑定 `Base.MaterialAttributes` 会设置 `UMaterial::bUseMaterialAttributes = true` —— 即 **Use Material
Attributes** 复选框 —— 于是材质暴露单一的 attributes 输入，而不是各个独立的
属性输入。你永远不需要自己勾选它，也无法在材质编辑器中取消勾选：每次
重新生成都会重置并重新应用它。被绑定的值必须是 `MaterialAttributes` 值；其他任何内容
都是 `DSH4228`。

<a id="basefrontmaterial"></a>

### `Base.FrontMaterial`

(自 UE 5.4 起)

被绑定的值必须是 Substrate 值，由 [`Substrate.*` 内置节点](../builtins/substrate.md)或
[Substrate 语法糖](../language-v2/substrate.md)产生；其他任何东西都是 `DSH4228`。

(自 2.0.0 起) `FrontMaterial` 是一个**普通属性**：1.x 生成器对它的额外规则——
强制把着色模型设为 Substrate、拒绝 `"Substrate"` 之外的显式 `ShadingModel`、
拒绝它与 `Base.MaterialAttributes` 并存，以及整表面 `Custom` 节点路径——都不再适用。
在材质需要的地方于 `Settings` 中写入 `ShadingModel = "Substrate";`（在 UE 5.3 上该设置是
`DSH7128`）。按旧属性写的材质在 Substrate 工程中如何读取，由
`Substrate =` 设置决定——见 [Substrate 语法糖](../language-v2/substrate.md)。

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

有些 Unreal 输出根本不是材质属性输入，而是存在于图中的*自定义输出*节点。非 `Base` 的绑定形式会创建这样一个节点，
并把源连接到它的某个输入 pin 上。

```c
Expression( Class = "<ExpressionClass>" [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
```

(自 2.0.0 起) 目标的两种形式都变成材质入口中的一次 `UE.Expression(Class = "…", …)` 节点调用，
所以一个目标的绑定规则与 `Graph` 中的
[`UE.Expression`](../builtins/ue-expression.md) 调用相同。

<a id="requirements-in-the-order-they-are-checked"></a>

### 要求，按检查顺序

| 要求 | 违反时的代码 |
| :-- | :-- |
| 头部是 `Expression(` `<key> = <value>` [`,` …] `)`，每个值非空 | `DSH3267` |
| 没有键被写两次（比较时忽略大小写） | `DSH3267` |
| 存在 `Class` 参数 | `DSH3267` |
| `)` 之后是 `.Pin[<index>] = <source>` 或 `{ … }` 块，索引为整数字面量 | `DSH3267` |
| `=` 之后有内容 | `DSH3270` |
| 该类是引擎拥有的类 | `DSH5212` |
| 索引指向该类的某个输入 pin | `DSH5285` |
| 其他每个参数都指向该类的某个 pin 或属性 | `DSH5213` |

`Class` 匹配时忽略大小写。下面的复用键会把其他每个键转为小写、并去掉每个值的引号；
节点调用按原样携带每个参数。

<a id="class-resolution"></a>

### 类解析

(自 2.0.0 起) 类在引擎的表达式目录中按精确名字查找，顺序如下：短名（`ThinTranslucentMaterialOutput`）、类名
（`MaterialExpressionThinTranslucentMaterialOutput`）、类路径（`/Script/Engine.MaterialExpressionThinTranslucentMaterialOutput`）、在前面加上
`MaterialExpression` 的短名，以及带其 `U` 的 C++ 名
（`UMaterialExpressionThinTranslucentMaterialOutput`），1.x 拒绝后者。在 1.x 文件中，只有忽略大小写才匹配某个类的名字也能解析，并给出警告 `DSH5276`。其余任何情况都是
`DSH5212`。同样的规则记录在 [UE.Expression](../builtins/ue-expression.md) 上。

<a id="other-arguments"></a>

### 其他参数

除 `Class` 之外的每个参数都按类进行匹配：先匹配输入 pin，再匹配反射
属性。值按数字、`true` / `false`、带引号的字符串或裸词读取；裸词指向
图中的某个值，或者对于枚举属性来说指向某个枚举项。

(自 2.0.0 起) 指向**输入** pin 的参数会连接该 pin，效果和 `Pin[<index>]` 一样；
1.x 会拒绝它。用两种方式连接同一个 pin 是 `DSH4215`。既不是 pin 也不是属性的名字是
`DSH5213`——例外是 1.x 文件中的图值，节点存在后按该名字连接
（`DSH5291`，info；当建好的节点没有该 pin 时是 `DSH8212`）。属性接受不了的值是
`DSH8213`。

<a id="pin-indices-and-node-reuse"></a>

### Pin 索引与节点复用

- `Pin[<index>]` 是节点输入列表的零基索引，顺序按引擎声明它们的顺序。`Expression(Class="ThinTranslucentMaterialOutput").Pin[0]` 是 `TransmittanceColor`，
  `.Pin[1]` 是 `SurfaceCoverage`。超出末尾的索引是 `DSH5285`。
- 一个节点的每个 pin 可以绑定**一次**，语句形式和[块](#block-form)形式合计：
  `DSH3268`。
- 节点在同一个 `Outputs` 区段内按**类加上排序后的参数列表**去重。两个
  其 `Expression( … )` 规格完全相同的绑定共用一个节点、绑定不同的 pin；
  任何一个参数不同都会创建第二个节点，同一个头部写在第二个 `Outputs`
  区段里也会。
- 源必须是图中的值：没有东西声明的名字是 `DSH4200`，pin
  接受不了的值是 `DSH5214`。必需的 pin 未连接是警告 `DSH5279`。
- 节点落在哪里由图布局决定。

这种形式最常用于的自定义输出的 pin 顺序：

| `Class` | `Pin[i]`，按顺序 |
| :-- | :-- |
| `ThinTranslucentMaterialOutput` | `0` TransmittanceColor、`1` SurfaceCoverage |
| `ClearCoatNormalCustomOutput` | `0` Input |
| `RuntimeVirtualTextureOutput` | `0` BaseColor、`1` Specular、`2` Roughness、`3` Normal、`4` WorldHeight、`5` Opacity、`6` Mask、`7` Displacement、`8` Mask4 |

其他任何 `UMaterialExpressionCustomOutput` 子类都以同样方式工作；它的 pin 顺序就是
引擎声明输入的顺序，也就是材质编辑器在节点上从上到下显示的顺序。

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;
    float3 CoatNormal;

    // Both statements reuse one ThinTranslucentMaterialOutput node.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[0] = Transmittance;
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;

    // A different class -> a second node.
    Expression(Class="ClearCoatNormalCustomOutput").Pin[0] = CoatNormal;
}
```

<a id="block-form"></a>

## 块形式

(自 1.9.0 起)

一个有很多 pin 的终端节点，在语句形式中必须为每个 pin 重复整份 `Expression( … )` 规格，
而由于该规格*就是*去重键，**漏掉一个参数会悄无声息地把节点劈成两个**。块形式只写一次规格：

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

块中的所有 pin 共享头部的类和参数列表，这正好就是复用键——所以一个有 *N* 个 pin 的块
**永远**只产生**一个**节点。其他什么都不变：两种形式在下游无法区分，具有相同的类解析、参数检查、pin 索引检查和诊断。

| 规则 | 细节 |
| :-- | :-- |
| 什么可以打开花括号 | 只有 `Expression( … )`。变量或 `Base.` 目标后面的 `{` 是 `DSH3266`；`Expression( … ).Pin[i]` 目标后面的是 `DSH3267` |
| 头部形状 | `Expression(` … `)`，右花括号 `)` 之后什么都没有——pin 选择器在块内按语句书写 |
| 块内语句 | 只能是 `Pin[ <index> ] = <source> ;`——否则 `DSH3267` |
| 注释和空行 | 块内任何位置都允许 |
| 空块 | `DSH3269`——不绑定任何 pin 的块什么都不会创建，所以会被拒绝而不是忽略 |
| 未闭合的块 | 解析一直跑到文件末尾：`DSH2150` |
| 结尾的 `;` | 在右花括号 `}` 后可选，和 `Group("Name") { … }` 之后一样 |
| 重复 | 在同一个 `Outputs` 区段内，同一个节点可以写多个块，并与它的语句形式绑定自由混用 |

类和参数列表一致的块与散写语句指向**同一个**节点，所以它们填充它的不同 pin：

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;

    Expression(Class="ThinTranslucentMaterialOutput")
    {
        Pin[0] = Transmittance;
    }

    // Same specification -> same node -> this fills its other pin.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;
}
```

**注意：**

两个参数列表不同（哪怕只差一个参数）的块，就是两个不同的节点，各有各自的 `Pin[0]`。这不算双重绑定，也不会被诊断；正是同一条规则让语句形式中的重复写法变得危险。这种形式在 1.x 解析器中的代码 `DSH3133`–`DSH3137` 不再报出（自 2.0.0 起）；上文的 `DSH3266`–`DSH3269` 和 `DSH2150` 取代了它们。

<a id="the-reserved-name-return"></a>

## 保留名 `return`

（自 2.0.0 起）`return` 在每个文件中都是关键字。它不能作为输出声明的名称（`DSH3266`），也不是绑定源：`Base.EmissiveColor = return;` 是 `DSH2151`。1.x 曾为它的整表面 `Custom` 节点路径保留该名称，而编译器已不再有该路径。

<a id="validation-rules"></a>

## 校验规则

| 条件 | 代码 |
| :-- | :-- |
| 声明的类型不是类型 | `DSH4201` |
| 在 `Outputs` 中重复声明的名称 | `DSH4220` |
| 没有任何声明的源名称 | `DSH4200` |
| 材质不具备的 `Base.` 名称 | `DSH5200` |
| 材质 pin 无法接受的值；更宽的向量不会收窄进材质 pin，即使在 1.x 文件中也不会 | `DSH4228` |
| `Expression( … )` pin 无法接受的值 | `DSH5214` |

对 `Outputs` 所声明名称的 `Graph` 声明，会被读作对该输出的赋值，这正是 1.x 的处理方式。

<a id="notes"></a>

## 说明

- 绑定和声明可以自由穿插；绑定可以引用同一部分中稍后声明的变量。
- 重复的 `Outputs` 部分会追加到两个列表中。
- 在 `Shader` 中，任何 `Outputs` 语句都不接受 `[ … ]` 元数据块。
- 每条绑定都会经过一组生成的 `NamedReroute` 对，该对按照写下的 `Base.` 名称命名为 `DS_<pin>_<n>`，从而使材质根节点在大图中保持可读。
- 绑定到当前着色模型或混合模式不使用的属性**不会**被诊断——Unreal 只是不读取该输入。请查看材质编辑器中变灰的输入。材质完全没有对应输入的属性是 `DSH8217`。

<a id="common-errors"></a>

## 常见错误

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5200: A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.` | Base. 后面的名称不是任何材质属性——包括 1.x 的 CustomizedUVs&lt;n&gt; 写法。 | 对照上面的目录修正拼写；将 CustomizedUVs&lt;n&gt; 写作 CustomizedUV&lt;n&gt;。 |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 某个 Base. 名称、类或参数仅在忽略大小写时才匹配（警告）。 | 使用引擎的大小写。 |
| `DSH3266: Expected ';' after the output declaration '{0}', found {1}.` | 该部分未用 { 打开，声明缺少 ;，或某一行既不是声明也不是绑定（包括将 return 作为声明名）。 |  |
| `DSH3267: Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.` | 格式错误的 Expression(...) 目标：其头部、pin 选择器、缺少的 Class、重复的键，或块中出现了非 Pin[i] = &lt;source&gt;; 的语句。 |  |
| `DSH3268: Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.` | 同一节点的同一 pin 在语句形式和块形式中被绑定了两次。 |  |
| `DSH3269: Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.` | 块形式没有绑定任何 pin。 |  |
| `DSH3270: Expected a source after 'Pin[{0}] =', found none.` | 一个 = 后面没有任何内容。 |  |
| `DSH2151: Expected an expression, found {0}.` | 源不是表达式——例如 Base.EmissiveColor = return;。 |  |
| `DSH4228: {0} expects {1}, and this is {2}.` | 材质 pin 无法接受的值：在 Base.MaterialAttributes 上使用非 attributes 值，在 Base.FrontMaterial 上使用非 Substrate 值，或宽度错误。 | [详情](../builtins/substrate.md) |
| `DSH5212: '{0}' is not a material expression class this engine has.` | Class= 未命名任何类。 | [详情](../builtins/ue-expression.md) |
| `DSH5285: '{0}.{1}' has {2} input pin(s), counted from 0, and this argument connects pin {3}.` | pin 索引超出了节点的输入列表。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 某个参数既不是 pin，也不是该类的属性。 |  |
| `DSH5214: {0} expects {1}, and this is {2}.` | Expression(...) 的 pin 无法接受的值。 |  |
| `DSH5279: '{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.` | 节点的必需 pin 未连接（警告）。 |  |
| `DSH7128: ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | 在 UE 5.3 上 ShadingModel = Substrate。 |  |
| `DSH8217: This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for.` | 该属性存在，但此材质——按其 domain、混合模式和着色模型——没有对应输入。 |  |
| `DSH2256: The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.` | 有 Graph 且没有 Outputs（警告）。材质仍会构建，但没有任何连线。 | 至少添加一条 Base.&lt;Attribute&gt; = &lt;variable&gt;; 绑定。 |

跨各阶段的完整列表见[诊断](../diagnostics/index.md)。

<a id="worked-example"></a>

## 完整示例

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

生成的连接：

```text
UMaterial /Game/DreamShaderTests/Corpus/M_Outputs
  MP_EmissiveColor  <- NamedReroute DS_EmissiveColor_<n>  <- VectorParameter "Tint"  RGB output
  MP_Opacity        <- NamedReroute DS_Opacity_<n>        <- ScalarParameter "A"     R output
  bUseMaterialAttributes = false
```

<a id="where-next"></a>

## 继续阅读

  - [生成流程](index.md)
  - [Substrate 节点](../builtins/substrate.md)
  - [UE.Expression](../builtins/ue-expression.md)
  - [材质 Settings](../settings/material.md)
  - [Section](../language/sections.md)
