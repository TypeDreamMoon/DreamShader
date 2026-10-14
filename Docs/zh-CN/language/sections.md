---
title: Section
description: Properties、Inputs、Outputs、Results、Settings、Options、Graph 与 Layout —— 哪个块接受哪个、重复出现时怎么处理，以及 Group 分组作用域。
---

带属性的块，其函数体就是一串 section。一个 section 由名字、可选的 `=`，以及一对花括号里以 `;` 分隔的
语句组成。

```c
<section> := <section-name> [=] { <statement>… } [;]
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

Section 名按**大小写不敏感**匹配，可以任意顺序出现，也可以重复。`=` 是可选的语法糖
（自 1.5.0 起），闭合 `}` 之后的 `;` 也是可选的。下面四种写法是同一个 section：

```c
Properties = { float A = 1.0; }
Properties   { float A = 1.0; }
properties = { float A = 1.0; };
PROPERTIES   { float A = 1.0 }
```

section 内容逐个 token 读取 （自 2.0.0 起）：任意空白都能分隔单词，制表符和空格没有区别；读取器跟踪
`()`、`[]`、`{}`，字符串是一个 token，所以 `Group` 体和方括号元数据块永远不会在错误的地方切断语句。

<a id="支持矩阵"></a>

## 支持矩阵

| Section | `Shader` | `ShaderFunction` | `ShaderLayer` / `Blend` | `VirtualFunction` |
| :-- | :-- | :-- | :-- | :-- |
| `Properties` | 参数节点 | 参数节点 | 参数节点 | **`Inputs` 的别名** |
| `Inputs` | 未知 section | 引脚声明 | 引脚声明，受层规则约束 | 引脚声明 |
| `Outputs` | 声明 **+ 绑定** | 引脚声明 | 引脚声明，受层规则约束 | 引脚声明 |
| `Results` | 未知 section | `Outputs` 的别名 | `Outputs` 的别名 | `Outputs` 的别名 |
| `Settings` | 材质设置 | 三个函数键 | 三个函数键 | `Options` 的别名 |
| `Options` | 未知 section | 未知 section | 未知 section | `Asset` 引用 |
| `Graph` | 必需¹ | 必需 | 必需 | `DSH2247` |
| `Layout` | 支持 | 支持 | 支持 | 未知 section |
| `Code` | `DSH2246` | `DSH2246` | `DSH2246` | `DSH2247` |

1. 除非 `Outputs` 里的语句自己算出值 —— 带初始化式的输出声明 （自 1.3.4 起），或右侧是表达式的绑定
   （自 2.0.0 起）。

"未知 section" 是 `DSH2245`。`Function` 和 `GraphFunction` 完全没有 section —— 它们的 `{ … }` 是原始 HLSL。

<a id="重复出现时的行为"></a>

## 重复出现时的行为

重复的 section 并不总是追加。

| Section | 重复时 |
| :-- | :-- |
| `Properties`、`Inputs`、`Outputs`、`Results` | **追加**到之前的列表 |
| `Settings`、`Options` | **合并**；同一个键写两次时后写的胜出，并给出警告 `DSH3262` （自 2.0.0 起） |
| `Graph` | **覆盖**之前的函数体，警告 `DSH2258` （自 2.0.0 起） |
| `Layout` | **重置** —— 第二个 `Layout` 会把第一个整个丢掉，警告 `DSH2258` |

1.x 对这几种覆盖都不吭声；现在它们照旧生效，只是会说出来。

<a id="properties"></a>

## `Properties`

声明一个块生成到**它自己的图里**的参数节点、常量节点和 `UE.*` 内置节点。在 2.0 里，一个属性就是一个
`uniform`（或 `static const`），元数据写在 `///` 行里；`dsc migrate` 按这个形式改写。

```c
property-declaration := [ const ] <type-token> <name> [ = <default> ] [ [ <metadata> ] ] ;

group-scope          := Group( "<group-name>" ) { { <property-declaration> | <group-scope> }… } [ ; ]
```

`<metadata>` 外面最内层的那对 `[ … ]` 是**字面的** DreamShaderLang 标点，外面那对是"可选"的元语法。
所以带元数据的声明写起来是 `float Roughness = 0.5 [Group="Surface"];`。

```c
Properties = {
    const float DebugScale = 1.0;                 // UMaterialExpressionConstant
    float Strength         = 1.0;                 // UMaterialExpressionScalarParameter
    vec3  Tint             = vec3(1.0, 1.0, 1.0); // UMaterialExpressionVectorParameter
    UE.TexCoord(Index = 0) UV;                    // 一个内置节点，不是参数
}
```

一条声明从左到右逐个 token 读取 （自 2.0.0 起）：

| 步骤 | 读取 | 缺失或写错时 |
| :-- | :-- | :-- |
| 1 | 可选的 `const`（大小写不限） | — |
| 2 | 类型：一个单词，或者带 `( … )` 参数列表的 `UE.<Function>` | `DSH3250`；`const` 之后是 `DSH3251` |
| 3 | 名字：一个标识符 | `DSH3250` |
| 4 | `=` 之后的默认值：括号之外直到 `[` 或 `;` 的全部 token | `=` 后面什么都没有是 `DSH3254` |
| 5 | 可选的 `[ … ]` 元数据块 | `DSH3255` |
| 6 | `;`，或闭合 section / 分组的 `}` | `DSH3250` |

所以 `UE.TexCoord(Index = 0) UV;` 读成类型 `UE.TexCoord(Index = 0)` 和名字 `UV`，类型和名字之间写制表符也行。

**警告：**

名字必须是标识符 （自 2.0.0 起）。1.x 只检查名字非空，于是 `Properties { float 1Bad = 0; }` 能解析、
却永远引用不到；现在 `1Bad` 是畸形数字（`DSH2105`），这条声明是 `DSH3250`。`in`、`out` 这类保留字也不能当
名字。同一个文件里重名是 `DSH4210`，比较是精确的（1.x 忽略大小写）。

属性节点在 `Graph` **第一次读取**时才建，之后的读取共享它 （自 1.3.2 起）；`Graph` 从没读过的属性不会产生
节点。`Graph` 在块里其他 section 之后才读，所以写在它下面的属性也看得见，声明顺序不影响名字解析 —— 它只影响
下面的自动排序计数器。

| 类型家族 | 生成的节点 |
| :-- | :-- |
| 紧凑标量 / 向量 token（`float`、`vec3`…） | `ScalarParameter` / `VectorParameter` |
| 紧凑纹理 token（`Texture2D`…） | `TextureObjectParameter` |
| 22 个显式参数节点 token | 对应的 `UMaterialExpression` 子类 |
| `UE.<Function>( … )` | 那个内置节点 |

其他 token 是 `DSH3252`。显式 token 里有七个没有 2.0 形式，是 `DSH3253` （自 2.0.0 起）：
`DoubleVectorParameter`、`TextureCollectionParameter`、`CurveAtlasRowParameter`、`DynamicParameter`、
`FontSampleParameter`、`SpriteTextureSampler`、`SparseVolumeTextureObjectParameter`。在这里 `StaticBool` 读作
`bool`，声明的是**动态**布尔参数 （自 2.0.0 起）；静态开关参数要写 `StaticBoolParameter`。`UE.*` 属性后面
写 `= <default>` 是 `DSH3259` —— 参数写在括号里。

纹理默认值除了 `Path(Game, "…")`、`Path("/Game/…")` 和带引号的绝对路径，还接受 Content Browser 的
**Copy Reference** 文本 `Texture2D'/Game/Textures/T_X.T_X'` （自 1.9.0 起）（裸写、加引号、或作为
`Path(…)` 的路径参数都行）。引号前的类和声明的纹理类型不符是 `DSH1044`，根本不是纹理类是 `DSH1043`。

类型 token、`const`、默认值和元数据块的完整说明见 [Properties 类型](properties.md) 和
[元数据与分组](../parameters/metadata.md)。

<a id="groupname----作用域"></a>

### `Group("Name") { … }` 作用域

（自 1.5.0 起）

`Group` 作用域把自己的名字盖到内部每一条声明上，省得在每条声明的元数据里重复写同一个分组。

```c
Properties {
    Group("Surface") {
        ScalarParameter Roughness = 0.5 [Slider(0, 1)];
        VectorParameter BaseColor = float4(1, 1, 1, 1);
    }
}
```

| 规则 | 细节 |
| :-- | :-- |
| 关键字 | `Group`，大小写不敏感 |
| 参数 | 紧跟关键字的 `(`，里面恰好一个双引号字符串，然后 `)` |
| 名字 | 那个字符串去空白后的内容，必须非空 —— 否则是 `DSH3260` |
| 函数体 | `{ … }` |
| 终止符 | 闭合 `}` 之后的单个 `;` 可选 |

`Group(…)` 是 `Properties` 里**唯一**可以打开 `{` 的构造。其他任何 `{` 都是 `DSH3250`；`}` 缺失的分组会一直读到
文件末尾（`DSH2150`）。

作用域可以嵌套任意深度，嵌套作用域的有效名字是外层名字、一个 `|`、内层名字 —— 这正是 Unreal 自己的子分类
写法。名字里手写的 `|` 原样保留：

```c
Properties {
    Group("Surface") {
        float A = 0;                    // Group = "Surface"
        Group("Detail") {
            float B = 0;                // Group = "Surface|Detail"
            Group("Micro") {
                float C = 0;            // Group = "Surface|Detail|Micro"
            }
        }
    }
}
```

声明上写的显式键总是胜出。只有当成员的元数据里既没写 `Group` 也没写 `Category` 时，继承来的分组才会生效：

```c
Group("Surface") {
    float A = 0;                        // Group = "Surface"    （继承）
    float B = 0 [Group="Override"];     // Group = "Override"   （显式胜出）
    float C = 0 [Category="Other"];     // Group = "Other"      （Category 是别名）
}
```

<a id="自动-sortpriority"></a>

### 自动 `SortPriority`

分组作用域里的成员按声明顺序自动编号。

| 规则 | 取值 |
| :-- | :-- |
| 计数器起点 | `0` |
| 计数器步长 | `10` |
| 计数器范围 | **同一个 `Properties` section 内所有分组共用一个计数器**，不是每组一个 |
| 显式 `SortPriority` / `Sort` | 胜出，并且**不占用槽位** |
| 未分组（顶层）的声明 | 既不自动编号，也不会被赋予分组 |

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                    // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);     // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99];  // SortPriority = 99，不占槽位
        ScalarParameter D = 2.0;                    // SortPriority = 20（计数器接着走）
    }
    ScalarParameter Loose = 3.0;                    // 无分组，不自动排序
}
```

**提示：**

计数器是**按 `Properties` section** 播种一次的。一个块声明了两次 `Properties`，第二个 section 会拿到
一个从 `0` 重新开始的新计数器，于是两个 section 里的两个分组可能撞上相同的排序值。

当分组作用域和显式元数据都没提供值时，参数取 `32` —— 引擎给参数节点的默认值，也就是 1.x 留下的值。

<a id="元数据"></a>

### 元数据

尾随的 `[ … ]` 块里，条目以 `;` 或 `,` 分隔，键忽略大小写匹配，同一个键写两次是 `DSH3256`。

| 键 | 别名 | 值 |
| :-- | :-- | :-- |
| `Group` | `Category` | 字符串 |
| `Description` | `Desc`、`Tooltip` | 字符串 |
| `SortPriority` | `Sort` | 整数 —— 否则是 `DSH3258` |
| `Slider(min, max)` | — | 不带 `=` 的简写；格式错或给了两次范围是 `DSH3257` |
| `ParameterName` | — | 覆盖材质参数名 |
| `SamplerType` | — | 采样器类型；`SAMPLERTYPE_` 前缀会被去掉 |
| *其他任何键* | — | 通过反射写到节点的同名属性上 |

表里没列的键都通过反射写到节点上 （自 2.0.0 起）：参数节点类没有的键是警告 `DSH8210`、值不写入（1.x
直接拒绝），属性接受不了的值是 `DSH8213`。滑条范围只属于标量参数，写在向量、纹理或布尔参数上是一条警告
`DSH7233`，范围被丢弃 （自 2.1.0 起）。完整语法见 [元数据与分组](../parameters/metadata.md)。

<a id="inputsoutputs-与-results"></a>

## `Inputs`、`Outputs` 与 `Results`

类型化参数 section。它们声明生成的材质函数的引脚，或者 `VirtualFunction` 所描述的接口。

```c
parameter-declaration := [ opt ] <type> <name> [ = <default-expression> ] [ [ <metadata> ] ] ;
```

`Results` 是 `Outputs` 的纯同义词 —— 它追加进同一个列表，没有任何警告。

| 成员 | 必需 | 说明 |
| :-- | :-- | :-- |
| `opt` （自 1.2.3 起） | 否 | 在 Unreal 中把输入标记为可选（`bUsePreviewValueAsDefault`），大小写不限。 |
| `<type>` | 是 | 一个单词，见 [类型与值](types.md)。 |
| `<name>` | 是 | 一个标识符，会成为引脚名。 |
| `= <default>` | 否 | 输入的默认值，同时让它变成可选 （自 2.0.0 起）。对输出无意义。 |
| `[ <metadata> ]` | 否 | 只有 `Description` / `Desc` / `Tooltip` 和 `SortPriority` / `Sort` 有效。 |

不符合 `[opt] <type> <name> [= <default>] [[ … ]] ;` 的语句是 `DSH3271`，`=` 后面为空也是。`opt`、类型和名字
之间任意空白都行 （自 2.0.0 起）；类型是一个单词，`Material Attributes X;` 或 `float 4 Colour;` 是
`DSH3271`。不是类型的单词是 `DSH4201`；同一个 section 里两个同名参数是 `DSH4214`。

**提示：**

`in` 和 `out` 在这些 section 里**不是**限定符。它们只存在于
[`Function` / `GraphFunction`](functions.md) 的签名形式里，那是另一套语法。写
`Inputs = { in float X; }` 是 `DSH3271`：`in` 是关键字，不是类型。

<a id="输入默认值"></a>

### 输入默认值

输入上的默认值是调用方不连线时引脚取的值，并让这个输入变成可选 —— 1.x 只给一个预览值，引脚仍然必需
（自 2.0.0 起）。

| 情况 | 行为 |
| :-- | :-- |
| 默认值能折叠成常量 | 写进 `PreviewValue` |
| 其他任何情况 | 作为**图表达式**构建，接到输入的 `Preview` 引脚上 |

正是这条图表达式路径，让默认值可以引用块自身生成的节点 （自 1.2.6 起）：

```c
ShaderFunction(Name="Functions/F_Sample")
{
    Properties = {
        const Texture2D PreviewTex = Path(Engine, "EngineResources/DefaultTexture");
    }
    Inputs = {
        opt Texture2D Tex = PreviewTex;      // 预览图，不是字面量
        opt float     Mix = 0.5;             // 常量 → PreviewValue
    }
    Outputs = { vec4 OutColor; }
    Graph   = { OutColor = Tex(Coordinates = UE.TexCoord(Index = 0)) * Mix; }
}
```

默认值按赋值的规则转换到输入的类型：装不下是 `DSH4226`，更宽的向量被截到前几个分量并给出 info `DSH5289`。
`StaticBool` 输入的默认值会变成接在 `Preview` 引脚上的 `StaticBool` 节点，因为引擎对它忽略 `PreviewValue`。

<a id="输出"></a>

### 输出

- 材质函数 `Outputs` / `Results` 条目上的 `= <expression>` 被**丢弃**并给出警告 `DSH3273`；`opt` 写在输出上是
  警告 `DSH3272`。函数输出只由 `Graph` 驱动。这不适用于 `Shader` 的 `Outputs`，那里的初始化式有意义。
- 名字恰好叫 `Result` 的第一个输出是函数的返回值，从 0 开始；其余每个输出都必须在 `Graph` 里被赋值
  （`DSH6211`）。函数至少要声明一个输出（`DSH4315`），`VirtualFunction` 也一样（`DSH6313`）。
- `MaterialAttributes` 输出从空材质开始，所以 `Attrs.BaseColor = …;` 这种成员写入有目标；层的输出从流入的
  材质开始。

<a id="元数据与排序"></a>

### 元数据与排序

只有 `Description` / `Desc` / `Tooltip` 和 `SortPriority` / `Sort` 有效；其他键（包括 `Group`）被接受后**静默
忽略** —— 函数输入输出节点上没有分组字段，也不做反射写入。一个 section 的引脚按 `SortPriority` 稳定排序（没写的
按声明序号算），资产上得到连续的优先级 `0, 1, 2, …` （自 2.0.0 起）。引脚 GUID 按名字跨重新生成缓存，所以
重新生成 `ShaderFunction` 不会弄断已有的调用点。

<a id="properties-与-inputs-的区别"></a>

### `Properties` 与 `Inputs` 的区别

两者都往生成的函数里放东西，但它们是产出不同节点的不同语法。

| | `Properties` | `Inputs` |
| :-- | :-- | :-- |
| 语法 | `[const] <type> <name> …` | `[opt] <type> <name> …` |
| 产出 | 函数图*内部*的参数、常量或 `UE.*` 节点 | 函数接口*上*的 `FunctionInput` 引脚 |
| 对调用方可见性 | 作为材质参数出现在任何使用该函数的材质上 | 作为可连线的输入引脚 |

<a id="shader-里的-outputs"></a>

## `Shader` 里的 `Outputs`

`Shader` 的 `Outputs` 用一段函数体填**两个**列表，逐条语句按开头的 token 分类：

| 语句形态 | 归类为 |
| :-- | :-- |
| `<type> <name> ;` | 输出变量声明；和 1.x 一样从 0 开始 |
| `<type> <name> = <expr> ;` | **带初始化式**的输出声明 （自 1.3.4 起） |
| `Base.<target> = <source> ;` | 材质属性绑定 |
| `Expression( … ).Pin[<i>] = <source> ;` | 表达式输入绑定 |
| `Expression( … ) { Pin[<i>] = <source> ; … }` | 块形式 （自 1.9.0 起） —— 一个终端节点上的多个引脚 |
| 其他任何东西 | `DSH3266`（以 `Expression( … )` 开头的畸形目标是 `DSH3267`） |

```c
Outputs = {
    vec3  Color;
    float Alpha;

    Base.EmissiveColor = Color;
    Base.Opacity       = Alpha;
}
```

- 声明和绑定可以自由交错；绑定可以引用同一 section 中后面才声明的变量。
- `<source>` 是一个表达式 （自 2.0.0 起）：1.x 写法里的输出变量名，或者 `Graph` 表达式能写的任何东西，在
  `Graph` 运行之后求值。`=` 后面为空是 `DSH3270`。
- `return` 是关键字 （自 2.0.0 起）：它不能做输出声明的名字（`DSH3266`），也不是绑定源。
- `Shader` 的 `Outputs` 语句上**不接受 `[ … ]` 元数据块**（`DSH3266`）。同名声明两次是 `DSH4220`。

**块形式**把 `Expression( … )` 的说明只写一次。说明本身就是节点去重的键，语句形式里每个引脚重复一遍、漏写一个
参数就会把节点悄悄拆成两个；块形式里的 *N* 个引脚永远落在**一个**节点上：

```c
Outputs = {
    float _PhaseG_Out;
    float _PhaseG2_Out;

    Expression(Class="VolumetricAdvancedMaterialOutput", PerSamplePhaseEvaluation="false")
    {
        Pin[0] = _PhaseG_Out;
        Pin[1] = _PhaseG2_Out;
    }
}
```

块内只能写 `Pin[<i>] = <source>;`（否则 `DSH3267`）；空块是 `DSH3269`；同一个节点的同一个引脚绑两次是
`DSH3268`，跨块形式和语句形式都算。块形式和说明完全相同的语句形式指向同一个节点，可以混用。

完整的 `Base.*` 目录和 `Expression( … ).Pin[i]` 规则见 [输出绑定](../generation/outputs.md)。

<a id="settings-与-options"></a>

## `Settings` 与 `Options`

两者共用一套语句语法：`<Key> = <Value> ;`。键忽略大小写匹配、按原样保存；值一直到括号之外的 `;`，单个带引号的
字符串取它的文本（转义已解析），其他取 token 的文本。键不是单词、缺 `=` 或缺值是 `DSH3261`；同一个键写两次时
后写的胜出并给出警告 `DSH3262` （自 2.0.0 起）。

| 块 | `Settings` 的含义 |
| :-- | :-- |
| `Shader` | 材质设置 —— 特殊键加上反射写入的 `UMaterial` 属性。见 [材质 Settings](../settings/material.md)。 |
| `ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` | 三个键有效：`Description`、`ExposeToLibrary`、`LibraryCategories`。 |
| `VirtualFunction` | `Options` 的别名：`Asset` 是资产引用，`Description` 作为声明的描述保留，其他键读取后忽略。 |

**提示：**

那三个材质函数键每次构建都会应用，**缺省时会被重置**，所以从源码里删掉一个键就等于从资产上删掉它。
`LibraryCategories` 按逗号切分，每项去空白，空项丢弃。`ExposeToLibrary` 只有为 `true` **且**
`LibraryCategories` 非空时才会置位 （自 2.0.0 起）；它的值不是 `true` / `false` 时是警告 `DSH3265`，按 `false`
处理。`UserExposedCaption` 不再写入资产，给出警告 `DSH3264`（值保留给 `dsc migrate`）（自 2.0.0 起）。其他键
是警告 `DSH3263` 并被忽略。

<a id="graph"></a>

## `Graph`

节点图的函数体。它和块里其他 section 在同一次解析中读取、但排在它们之后，由 2.0 的语句与表达式解析器处理
（自 2.0.0 起）；1.x `Graph` 没有的写法会被点名拒绝（`DSH2200`–`DSH2222`）。注释在这里照常识别，
`#Region` 指令只在这里有效，`#if` 预处理指令在这里也能用（`Graph` 是 DreamShaderLang，不是 HLSL）。

```c
Graph = {
    vec2 UV  = UE.TexCoord(Index = 0);
    vec4 Tex = BaseTex(Coordinates = UV);
    Color = Tex.rgb * Tint;
}
```

语句和表达式语言见 [Graph 求值模型](../graph/index.md)。

<a id="layout"></a>

## `Layout`

固定生成节点的位置并声明注释框。见 [Layout 与 #Region](layout.md)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2245: Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.` | 块不认识的 section 名 —— 消息按块的种类列出它接受的 section。 | 检查拼写，以及前一个 section 是否已经闭合。 [详解](blocks.md) |
| `DSH3250: Expected a property type and a name, found {0}.` | Properties 的一行不是以类型和名字开头：section 没用 { 打开、缺 ;、名字不是标识符，或者出现了不是 Group 作用域的 {。 | 检查上一行的 ;，以及类型是不是一个单词。 |
| `DSH3251: Expected a property type after 'const', found {0}.` | const 后面没有类型。 | 写成 const float K = 1.0;。 |
| `DSH3252: Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.` | 类型位置上的单词不是 1.x 属性能用的类型 —— MaterialAttributes、Substrate、SamplerState 和矩阵都在其中。 | 用值类型、纹理类型或参数节点类。 [详解](types.md) |
| `DSH3253: Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.` | 七个没有 2.0 形式的参数节点 token 之一，或者 const 写在不能当常量的参数节点 token 前。 | 改成普通值或参数；其他节点类把材质迁到 .dss，在使用处写 UE.&lt;Class&gt;(...)。 |
| `DSH3254: Expected a default value after '{0} =', found {1}.` | = 后面没有东西，或者默认值不是这个类型能读的形式。 | 写成数字、构造器、true / false 或资产引用，或者去掉 =。 [详解](lexical.md) |
| `DSH3255: Expected ']' to close the metadata block, found {0}.` | 元数据块没有闭合，或者某个条目不是 Key = value。 | 用 \] 闭合，每个条目以 ; 结尾。 [详解](../parameters/metadata.md) |
| `DSH3256: Expected the metadata key '{0}' once, found it again.` | 同一个 \[ … \] 块里同一个键写了两次。 | 保留一个。 |
| `DSH3257: Expected 'Slider(min, max)' with two numbers, found '{0}'.` | Slider 不是两个数字的形式，或者范围给了两次。 | 只写一次，例如 Slider(0.0, 4.0)。 |
| `DSH3258: Expected 'SortPriority' to be a whole number, found '{0}'.` | SortPriority（或 Sort）不是整数。 | 写整数：SortPriority = 10;。 |
| `DSH3259: Expected ')' to close the arguments of 'UE.{0}', found {1}.` | UE.\* 属性的参数列表没闭合，或者把参数写成了名字后面的默认值。 | 闭合 )，参数写在括号里：float2 UV = UE.TexCoord(Index = 1);。 |
| `DSH3260: Expected a name inside 'Group("...")', found an empty string.` | Group 的名字是空的。 | 给分组起个名字，或者去掉这个块。 |
| `DSH3261: Expected a setting name such as 'BlendMode', found {0}.` | Settings 或 Options 的一行不是以设置名开头：缺键、缺 = 或缺值。 | 检查上一行有没有漏 ;。 |
| `DSH3262: The setting '{0}' is written twice; the later value wins, as it did in 1.x.` | 同一个设置写了两次（警告）。 | 删掉其中一个。 |
| `DSH3263: '{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end.` | 不是这种块的设置 —— 比如写在 ShaderFunction 里的材质设置（警告）。 | 对照文档检查拼写，或删掉这一行。 [详解](../settings/material.md) |
| `DSH3264: 'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration.` | 函数上设置了 UserExposedCaption（警告）。新资产没有 caption，之前构建写过的保持不变。 | 构建不用改；需要 caption 就迁移后在资产上手动设置。 |
| `DSH3265: Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end.` | ExposeToLibrary 既不是 true 也不是 false（警告）。 | 写 ExposeToLibrary = true;。 |
| `DSH3271: Expected a parameter type and name such as 'float Amount', found {0}.` | Inputs / Outputs / Results 的一行不是 \[opt\] &lt;Type&gt; &lt;Name&gt; \[= default\] \[ \[metadata\] \];，或者 = 后面为空 —— 两个单词的类型、in / out 都会走到这里。 | 检查类型拼写和上一行的 ;。 |
| `DSH3272: 'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end.` | opt 写在了输出上（警告）。 | 删掉它。 |
| `DSH3273: A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped.` | 材质函数的输出带了默认值（警告）。 | 删掉默认值，或者在 Graph 里赋值。 |
| `DSH3266: Expected ';' after the output declaration '{0}', found {1}.` | Shader 的 Outputs 里有一行既不是输出声明，也不是 Base.&lt;Attribute&gt; = …; 或 Expression(...) 绑定；或者声明缺 ;、带了元数据块、用 return 做名字。 | 每行以 ; 结尾，并使用这三种形式之一。 [详解](../generation/outputs.md) |
| `DSH3267: Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.` | Expression(...) 目标格式不对：头部、引脚选择器、缺 Class、重复的键，或者块里出现了 Pin\[i\] = …; 之外的语句。 | 按索引写引脚：Pin\[0\] = Color;。 |
| `DSH3268: Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.` | 同一个节点的同一个引脚绑定了两次，跨块形式和语句形式都算。 | 每个引脚只保留一次绑定。 |
| `DSH3269: Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.` | Expression(...) 块里一个引脚都没绑定。 | 至少写一条 Pin\[&lt;index&gt;\] = &lt;source&gt;;。 |
| `DSH3270: Expected a source after 'Pin[{0}] =', found none.` | Outputs 里某个 = 后面什么都没有。 | 写上 Graph 里的变量或一个常量。 |
| `DSH4201: '{0}' is not a type; did you mean '{1}'? Type names are case-sensitive.` | Properties 之外类型位置上的单词不是类型。 | 检查拼写。 [详解](types.md) |
| `DSH4214: '{0}' is declared twice in the parameter list of '{1}'.` | 两个输入或输出同名。 | 给其中一个改名。 |
| `DSH4220: '{0}' is already declared in this block.` | Shader 的 Outputs 里同一个名字声明了两次。 | 给其中一个改名。 |
| `DSH6211: '{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter.` | Graph 从来没给某个输出赋值。 | 在 Graph 里给它赋值，或者删掉这个输出。 |
| `DSH4315: Product {0} is a {1} and produces nothing; a material function needs at least one FunctionOutput.` | 材质函数、层或混合一个输出都没有。 | 至少声明一个输出。 |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | 同一个文件里两个属性（或属性和别的声明）同名，精确比较。 | 给其中一个改名。 |
| `DSH7233: A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{0}', which has no slider, so the range is ignored.` | 滑条范围写在了向量、纹理或布尔参数上（警告）。 | 删掉范围，或者把它放在标量参数上。 [详解](../parameters/metadata.md) |

<a id="继续阅读"></a>

## 继续阅读

- [类型与值](types.md) — 哪个类型 token 在哪个 section 里合法。
  - [Properties 类型](properties.md) — 紧凑类型、const，以及 22 个显式参数节点。
  - [元数据与分组](../parameters/metadata.md) — [ … ] 块、Slider(…) 与反射透传。
  - [输出绑定](../generation/outputs.md) — Base.* 目录与 Expression( … ).Pin[i]。
