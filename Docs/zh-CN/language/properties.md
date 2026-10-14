---
title: "Properties 属性"
description: "Properties 属性"
---

<a id="properties"></a>

# 属性

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **属性**

一个声明块的段落，用于声明该块生成到其材质图中的参数节点、常量节点和 `UE.*` 内置节点。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 位于 `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 内部 |
| 种类 | 段落 |
| 生成 | 图实际读取的那些声明所对应的节点 |
| 别名 | 在 `VirtualFunction` 内部，`Properties` 是 **`Inputs` 的同义词** —— 见[说明](#notes) |

在 2.0 中，属性是一个 `uniform`（或一个 `static const`），其元数据写在 `///` 行中 —— 见
[DreamShaderLang 2.0](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会按这种方式重写一个段落。

<a id="synopsis"></a>

## 语法概要

```c
Properties [=]
{
    <property-declaration>…
    <group-scope>…
}
```

```c
property-declaration := [ const ] <type-token> <name> [ = <default> ] [ [ <metadata> ] ] ;

group-scope          := Group( "<group-name>" ) { { <property-declaration> | <group-scope> }… } [ ; ]
```

在 `property-declaration` 中，`[ … ]` 围绕 `<metadata>` 的最内层一对是**字面的
DreamShaderLang 标点**；外层一对是表示「可选」的元语法。因此，带元数据块的声明形如 `float Roughness = 0.5 [Group="Surface"];`。

段落名与其 `{ … }` 块之间的 `=` 是可选的语法糖 *（自 1.5.0 起）*。段落名按大小写不敏感匹配。段落结束 `}` 之后的 `;` 是可选的，最后一条声明之前的 `;` 也是可选的。

<a id="declaration-members"></a>

## 声明成员

| 成员 | 必需 | 形式 | 说明 |
| :-- | :-- | :-- | :-- |
| `const` | 否 | 关键字 | 生成常量节点而不是参数节点。大小写不限。*（自 1.2.6 起）* |
| **`<type-token>`** | 是 | token | 选择生成的节点类以及默认值语法。见[类型 token](#type-tokens)。 |
| **`<name>`** | 是 | 标识符 | `Graph` 读取的标识符，并且 —— 除非 `[ParameterName=…]` 覆盖它 —— 也是材质参数名。 |
| `= <default>` | 否 | 字面量 | 初始值。可接受的字面量语法由类型 token 决定，而不是由字面量的形状决定。 |
| `[ <metadata> ]` | 否 | 块 | 分组、排序优先级、描述、滑块边界，以及任意反射的 `UMaterialExpression` 属性。见[元数据](../parameters/metadata.md)。 |

<a id="parse-order"></a>

### 解析顺序

*（自 2.0.0 起）* 声明从左到右逐个 token 读取：

| 步骤 | 读取 | 缺失或错误时 |
| :-- | :-- | :-- |
| 1 | 可选的 `const` | —— |
| 2 | 类型：一个词，或带其 `( … )` 实参列表的 `UE.<Function>` | [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250)；在 `const` 之后为 [`DSH3251`](../diagnostics/DSH3xxx.md#dsh3251) |
| 3 | 名称：一个标识符 | `DSH3250` |
| 4 | 在 `=` 之后，默认值：直到括号外出现 `[` 或 `;` 之前的每一个 token | `=` 之后没有任何内容时为 [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) |
| 5 | 可选的 `[ … ]` 元数据块 | [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255) |
| 6 | `;`，或结束段落或分组的 `}` | `DSH3250` |

任意类型的空白都会分隔这些词，因此类型与名称之间用制表符也可以，而带空格实参列表的 `UE.*`
内置类型也会被读作一个类型：

```c
UE.TexCoord(Index = 0) UV;      // type = UE.TexCoord(Index = 0), name = UV
```

> [!NOTE]
> *（自 2.0.0 起）* 名称必须是标识符。1.x 只检查其非空，因此
> `Properties { float 1Bad = 0; }` 能解析但不可达；现在 `1Bad` 是格式错误的数字
> （[`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)），并且该声明为 `DSH3250`。像
> `in` 或 `out` 这样的关键字也不能作名称。

<a id="type-tokens"></a>

## 类型 token

可识别四个 token 家族，全部按大小写不敏感匹配。完整的目录位于各自的页面：

| 家族 | Token | 生成的节点 | 参考 |
| :-- | :-- | :-- | :-- |
| 紧凑标量 | `float` `half` `double` `int` `uint` `bool`，以及各自带 `1` 后缀的版本（`float1` …） | `UMaterialExpressionScalarParameter` | [紧凑类型](../parameters/compact-types.md) |
| 紧凑向量 | `float2..4` `half2..4` `double2..4` `int2..4` `uint2..4` `bool2..4` `vec2..4` `ivec2..4` `uvec2..4` `bvec2..4` | `UMaterialExpressionVectorParameter` | [紧凑类型](../parameters/compact-types.md) |
| 紧凑纹理 | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` —— 5 个 token | `UMaterialExpressionTextureObjectParameter` | [紧凑类型](../parameters/compact-types.md) |
| 显式参数节点 | 22 个 `*Parameter` / 采样器 token | 指定的 `UMaterialExpression` 子类 | [参数节点](../../parameters/parameter-nodes.md) |
| `UE.<Function>[( … )]` | 任意内置名称，带实参列表 | 该内置的节点，或在给出 `OutputType=` 时使用反射节点 | [UE 内置](../builtins/ue.md)、[`UE.Expression`](../builtins/ue-expression.md) |

其他任何内容都是 [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)。在 22 个显式 token 中，有七个没有
2.0 形式，并且为 [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) *（自 2.0.0 起）*：
`DoubleVectorParameter`、`TextureCollectionParameter`、`CurveAtlasRowParameter`、`DynamicParameter`、
`FontSampleParameter`、`SpriteTextureSampler`、`SparseVolumeTextureObjectParameter`。其余之中，
`ScalarParameter`、`VectorParameter`、`StaticBoolParameter` 和 `TextureObjectParameter` 像紧凑 token 一样声明一个
参数；剩下的十一个在每次读取该属性时都会创建一个节点，与 1.x 一样。

> [!NOTE]
> `Properties` token 集合与 `Inputs` / `Outputs` token 集合**并不**相同。
> `MaterialAttributes`、`Substrate` 和 `SamplerState` 是有效的参数类型，但**不是**有效的
> `Properties` 类型；矩阵拼写也不行。这里 `StaticBool` 读作 `bool`
> *（自 2.0.0 起）*，因此它声明一个动态 bool 参数；静态开关参数是
> `StaticBoolParameter`。各上下文的有效性矩阵见[类型](types.md)。

在 `UE.*` 内置属性上内联 `= <default>` 是
[`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) —— 其参数要放在圆括号内。

<a id="texture-defaults"></a>

### 纹理默认值

纹理 token 的 `= <default>` 是一个资产引用。可接受四种写法，且四种都会
解析到同一个对象路径：

```c
Texture2D A = Path(Game, "Textures/T_X");                      // root + relative path
Texture2D B = Path("/Game/Textures/T_X");                      // absolute path
Texture2D C = "/Game/Textures/T_X";                            // bare quoted absolute path
Texture2D D = Texture2D'/Game/Textures/T_X.T_X';               // Content Browser "Copy Reference" (since 1.9.0)
```

最后一种形式就是 **Copy Reference** 放到剪贴板上的内容，采用
`/Script/Engine.Texture2D'…'` 或较旧的 `Texture2D'…'` 拼写，并且可以裸粘贴、加
引号粘贴，或作为 `Path(…)` 的路径实参粘贴。引号前面的类会与
声明的纹理类型核对：把 `Texture2D'…'` 粘贴到 `TextureSampleParameterVolume` 会按
名称被拒绝（[`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)；完全不是纹理的类为
[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)），而不是留到加载时失败。无法解析的引用为
[`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271)，解析后没有可加载内容的引用为
[`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218)。见 [`Path(…)`](../parameters/path.md)。

<a id="const-properties"></a>

## `const` 属性

*（自 1.2.6 起）*

`const` 用常量节点替换参数节点，因此该值会被烘焙进材质，
不会出现在实例的参数列表中。

| 声明的类型 | 生成的节点 |
| :-- | :-- |
| 标量 | `UMaterialExpressionConstant` |
| 向量，2 个分量 | `UMaterialExpressionConstant2Vector` |
| 向量，3 个分量 | `UMaterialExpressionConstant3Vector` |
| 向量，4 个分量 | `UMaterialExpressionConstant4Vector` |
| 纹理 | `UMaterialExpressionTextureObject` |

规则：

- `const` 可与紧凑的标量、向量和纹理 token 搭配使用，并且*（自 2.0.0 起）*可与
  `ScalarParameter`、`VectorParameter` 和 `TextureObjectParameter` 搭配使用，它们按上述方式读取。与任何
  其他参数节点 token 搭配时，它会被解析为 `DSH3253`。
- 没有默认值的 `const` 为零。没有默认值的 `const` 纹理会生成一个 `TextureObject` 节点，
  其自身没有写入任何纹理*（自 2.0.0 起）*：1.x 会回退到引擎该维度的默认资产，
  并拒绝 `Texture2DArray`。
- 常量的值由编译器决定：将其与其他常量组合的表达式可能会折叠
  为一个常量节点。
- **`const` 向量在 `Graph` 中按其整个节点输出（索引 0）读取**，而非 `const`
  向量参数则通过与其分量数匹配的命名输出读取（`R`、`RG`、`RGB`、
  `RGBA`）。请参阅[在图（Graph）中读取参数](../../parameters/graph-usage.md)。

```c
Properties = {
    const float DebugScale = 1.0;                // UMaterialExpressionConstant
    float Strength         = 1.0;                // UMaterialExpressionScalarParameter
    vec3  Tint             = vec3(1.0, 1.0, 1.0);// UMaterialExpressionVectorParameter
}
```

<a id="groupname----scope-blocks"></a>

## `Group("Name") { … }` 作用域块

*（自 1.5.0 起）*

`Group` 作用域会将其名称印到它包含的每一个声明上，因此共享的分组不必
在每个声明的元数据中重复。

```c
Properties {
    Group("Surface") {
        ScalarParameter Roughness = 0.5 [Slider(0, 1)];
        VectorParameter BaseColor = float4(1, 1, 1, 1);
    }
}
```

<a id="head-grammar"></a>

### 头部语法

| 规则 | 详情 |
| :-- | :-- |
| 关键字 | `Group`，不区分大小写匹配 |
| 参数列表 | 紧跟在关键字之后的 `(`，其中恰好包含一个双引号字符串，然后是 `)` |
| 名称 | 该字符串，去除首尾空白；它必须非空 —— [`DSH3260`](../diagnostics/DSH3xxx.md#dsh3260) |
| 主体 | `{ … }` |
| 终止符 | 闭合 `}` 之后的单个 `;` 是可选的 |

`Group(…)` 是**唯一**可以在 `Properties` 中开启 `{` 的构造。任何其他 `{` 都是一个
诊断（`DSH3250`），而缺少 `}` 的分组会一直运行到文件末尾
（[`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150)）。

<a id="nesting-and--composition"></a>

### 嵌套与 `|` 组合

作用域可以嵌套到任意深度。嵌套作用域的有效分组名称是外层名称、一个 `|`，以及
内层名称 —— 这正是 Unreal 自身用于 `Group` 属性的子类目语法：

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

在分组名称中输入的字面 `|` 会原样传递，因此
`Group("Manual|Literal") { … }` 会精确产生 `Manual|Literal`。

<a id="inheritance-precedence"></a>

### 继承优先级

继承的分组**仅当**该成员的元数据块既未输入 `Group` 也未输入 `Category` 时，才会应用到该成员。
声明上的显式键始终优先：

```c
Group("Surface") {
    float A = 0;                        // Group = "Surface"     (inherited)
    float B = 0 [Group="Override"];     // Group = "Override"    (explicit wins)
    float C = 0 [Category="Other"];     // Group = "Other"       (Category is the alias)
}
```

<a id="automatic-sortpriority"></a>

### 自动 `SortPriority`

分组作用域的成员按声明顺序自动编号。

| 规则 | 值 |
| :-- | :-- |
| 计数器起始值 | `0` |
| 计数器步长 | `10` |
| 计数器作用域 | **同一个 `Properties` 区段中每个分组共享一个计数器**，而不是每个分组一个 |
| 显式 `SortPriority` / `Sort` | 优先，并且**不消耗槽位** |
| 未分组（顶层）声明 | 从不自动编号，也从不分配分组 |

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                    // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);     // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99];  // SortPriority = 99, no slot consumed
        ScalarParameter D = 2.0;                    // SortPriority = 20  (counter continued)
    }
    ScalarParameter Loose = 3.0;                    // no group, no auto sort
}
```

> [!NOTE]
> 计数器在每个 `Properties` 区段中初始化一次。一个块声明了两次 `Properties` 时，
> 第二个区段中的计数器会从 `0` 重新开始，因此两个区段中的两个分组可能最终
> 得到重叠的排序优先级。

当没有分组作用域且没有显式元数据提供值时，参数会得到 `32`，即引擎自身
对参数节点的默认值 —— 也是 1.x 保留的值。

<a id="metadata"></a>

## 元数据

末尾的 `[ … ]` 块与带类型参数的各节共享。其可识别的键为：

| 键 | 别名 | 值 |
| :-- | :-- | :-- |
| `Group` | `Category` | 字符串 |
| `Description` | `Desc`、`Tooltip` | 字符串 |
| `SortPriority` | `Sort` | 整数 —— 否则 [`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) |
| `Slider(min, max)` | — | 不带 `=` 的简写；与 `SliderMin` + `SliderMax` 相同的范围 —— 格式错误或给出两次时为 [`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257) |
| `ParameterName` | — | 字符串；覆盖材质参数名称 |
| `SamplerType` | — | 采样器类型；`SAMPLERTYPE_` 前缀会被丢弃 |
| *其他任何内容* | — | 写入同名的反射 `UMaterialExpression` 属性 |

条目由 `;` 或 `,` 分隔，键不区分大小写匹配：写入两次的键为
[`DSH3256`](../diagnostics/DSH3xxx.md#dsh3256)。对标量参数之外的任何内容设置滑块范围都会被
忽略并给出警告 [`DSH7233`](../diagnostics/DSH7xxx.md#dsh7233)。完整语法、反射
规则以及枚举值的拼写见 [元数据](../parameters/metadata.md)。

<a id="ordering-and-repetition"></a>

## 排序与重复

- 语句以 `;` 结尾。读取器会跟踪 `()`、`[]` 和 `{}`，而字符串是一个 token，因此
  `Group` 主体和方括号元数据块绝不会在错误的位置拆分语句。
- 注释可以位于 token 之间的任何位置。
- 各节可以在块内**以任意顺序**出现，重复的 `Properties` 节
  会**追加**到前一节。在节级别没有“声明两次”诊断。
- **声明顺序不影响 `Graph` 中的名称解析**：`Graph` 会在块的每个
  其他节之后读取，因此在其下方声明的属性仍然可见。参数在图首次读取它的地方构建，
  并被之后的每次读取共享，而没有任何读取的属性不产生节点。十一个没有参数形式的参数节点 token 是例外：每次
  读取都是它自己的节点，与 1.x 一样。
- 声明顺序*确实*决定自动 `SortPriority` 计数器。
- `UE.*` 属性在读取它的 `Graph` 主体头部成为一个值，仅在
  主体读取它时构建，按声明顺序。
- 在一个文件中声明两次的名称为 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。该检查
  精确比较名称*（自 2.0.0 起）*；1.x 忽略大小写进行比较。

<a id="notes"></a>

## 说明

- **`Properties` 在 `VirtualFunction` 里含义不同。** 在那里它是
  [`Inputs`](inputs-outputs.md) 的别名，使用带类型参数的语法（`opt`，没有 `const`，没有节点
  token），而不是本页的声明语法。见 [VirtualFunction](virtual-function.md)。
- property 节点落在哪里是 [graph 布局](../generation/graph-layout.md) 的事；
  [`Layout`](layout.md) 区段可以钉住它们。
- *(自 2.0.0 起)* 上表没有点名的每个元数据键都由反射写到节点上。在参数上，节点类没有的键会给出警告
  [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210)，值不会被写入 —— 1.x 会拒绝它，
  `Group`、`SortPriority` 和 `Desc` 除外；属性接受不了的值会给出
  [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)。在没有参数形式的十一个参数节点 token 上，这些键是节点的参数，
  未知的键会给出 [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) —— 或者，当其值是数字时，给出信息
  [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)，随后在构建出的节点没有该引脚时给出 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)。
- 重新生成会破坏对生成节点的手工编辑。见
  [重新生成](../generation/regeneration.md)。

<a id="diagnostics"></a>

## 诊断

每条诊断都带有它所针对的声明的行和列。

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3250` | 区段没有用 `{` 打开；语句没有类型和名字；声明没有以 `;` 结束；一个不是 `Group` 作用域的 `{` |
| `DSH3251` | `const` 后面没有类型 |
| `DSH3252` | 类型既不是紧凑 token、不是参数节点 token，也不是 `UE.*` |
| `DSH3253` | 没有 2.0 形式的参数节点 token，或 `const` 后面跟着一个不能作常量的参数节点 token |
| `DSH3254` | `=` 后面没有东西，或类型读不了的默认值：不是数字或 `true` / `false` 的标量、不是 `name(a, b, c, d)` 字面量的向量、不是 `true` / `false` 的静态开关 |
| `DSH3255` | 未闭合的元数据块，或不是 `Key = Value` 的条目 |
| `DSH3256` | 写了两次的元数据键 |
| `DSH3257` | 格式错误的 `Slider(…)`，或给了两次的滑块范围 |
| `DSH3258` | 不是整数的 `SortPriority` |
| `DSH3259` | 未闭合的 `UE.*` 参数列表，或在 `UE.*` 属性之后写的默认值 |
| `DSH3260` | `Group("")` |
| `DSH2150` | 文件在区段或分组内结束 |
| `DSH1043`、`DSH1044` | 纹理默认值的 `Class'…'` 外壳不是纹理，或不是声明的纹理类型 |
| `DSH4210` | 声明了两次的名字 |
| `DSH7233` | 警告：没有滑块的参数上有滑块范围 |
| `DSH8210` | 警告：参数节点没有的元数据键 |
| `DSH8213` | 节点属性接受不了的元数据值 |
| `DSH8218`、`DSH8271` | 加载不了或解析不了的纹理默认值 |

完整列表见 [诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Materials/M_Surface", Root="Game")
{
    Properties {
        Group("Surface") {
            VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0);   // SortPriority 0
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];          // SortPriority 10
            Group("Advanced") {
                ScalarParameter Metallic = 0.0;                       // Group "Surface|Advanced", 20
            }
        }

        Group("Textures") {
            TextureSampleParameter2D AlbedoMap = Path(Game, "Textures/T_White_Linear") [
                SamplerType="LinearColor";
                MipValueMode="None";
                AutomaticViewMipBias=true;
            ];                                                        // SortPriority 30
        }

        const float DebugScale = 1.0;                                 // ungrouped, not auto-sorted
        UE.TexCoord(Index = 0) UV;                                    // ungrouped
    }

    Outputs {
        float3 Color;
        float  Rough;
        float  Metal;

        Base.BaseColor  = Color;
        Base.Roughness  = Rough;
        Base.Metallic   = Metal;
    }

    Graph {
        float4 Albedo = AlbedoMap(Coordinates = UV);
        Color = Albedo.rgb * BaseColor.rgb * DebugScale;
        Rough = Roughness;
        Metal = Metallic;
    }
}
```

生成的参数组织：

```text
Surface           SortPriority  0   BaseColor   VectorParameter
Surface           SortPriority 10   Roughness   ScalarParameter, slider 0..1
Surface|Advanced  SortPriority 20   Metallic    ScalarParameter
Textures          SortPriority 30   AlbedoMap   TextureSampleParameter2D

DebugScale  -> UMaterialExpressionConstant           not a parameter; ungrouped, not auto-sorted
UV          -> UMaterialExpressionTextureCoordinate  ungrouped, not auto-sorted
```

<a id="see-also"></a>

## 另见

- [Shader](shader.md) —— `Properties` 成为材质参数的块
- [ShaderFunction](shader-function.md) —— 函数局部的 `Properties` *(自 1.2.6 起)*
- [VirtualFunction](virtual-function.md) —— `Properties` 改为别名 `Inputs` 的地方
- [输入 / 输出 / 结果](inputs-outputs.md) —— 带类型参数的区段语法
- [类型](types.md) —— 完整的类型 token 目录和逐上下文有效性矩阵
- [紧凑类型](../parameters/compact-types.md) —— 每个紧凑 token 及它生成的节点
- [参数节点](../../parameters/parameter-nodes.md) —— 全部 22 个显式 `*Parameter` token
- [元数据](../parameters/metadata.md) —— `[ … ]` 块、`Slider(…)`、反射透传
- [采样器类型](../../parameters/sampler-type.md) —— `SamplerType` 的值和拼写
- [`Path(...)`](../parameters/path.md) —— 纹理默认值的资产引用语法
- [在 Graph 中读取参数](../../parameters/graph-usage.md) —— 裸读取和引脚调用形式
- [UE 内置](../builtins/ue.md) —— 每个可作为属性类型接受的 `UE.*` 名字
- [输出绑定](output-bindings.md) —— 把 graph 值连到材质属性
- [布局](layout.md) —— 钉住生成的节点位置
- [DreamShaderLang 2.0](../language-v2/index.md) —— `uniform` 和 `///` 元数据、`.dss` 拼写
- [诊断索引](../diagnostics/index.md) —— 每个代码
