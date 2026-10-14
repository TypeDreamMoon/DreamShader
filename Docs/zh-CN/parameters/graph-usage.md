---
title: "在 Graph 中使用参数"
description: "在 Graph 中使用参数"
---

<a id="using-parameters-in-graph"></a>

# 在 Graph 中使用参数

> [DreamShader](../index.md) » [参数](index.md) » **在 Graph 中使用参数**

声明的属性在 `Graph` 块内如何被使用：作为值、作为 swizzle 后的值，或作为一次
调用，用于连接所生成节点的输入引脚。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 位于 `Graph { … }` 主体、`Outputs` 绑定或 `Outputs` 声明默认值内部 |
| 类别 | 表达式形式 |
| 生成 | 声明生成一个节点；对于在使用处展开的 token，每个不同的使用各生成一个节点 |
| 起始版本 | `1.2.3`（`StaticSwitchParameter` 调用）；`1.4.1`（输入引脚调用形式） |

<a id="synopsis"></a>

## 概要

```c
<name>                                  // value read
<name> . <channels>                     // value read, then a swizzle
<name> ( <pin> = <expression> [ , <pin> = <expression> ] … )   // pin call form
```

引脚调用形式没有位置参数变体：每个参数都必须具名
（[`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259)）。

<a id="value-reads"></a>

## 值读取

属性是以下两种情况之一 *（自 2.0.0 起）* —— 参见
[Parameter node tokens](parameter-nodes.md#the-22-tokens)：

- 一个**声明**（每个紧凑 token、`ScalarParameter`、`StaticBoolParameter`、`VectorParameter`、
  `TextureObjectParameter`）：一个文件作用域的 `uniform` 或 `static const`，无论被读取
  多少次都只有一个节点；
- 一个**在使用处展开**的 token（`StaticSwitchParameter`、两个遮罩、纹理采样
  token）：每次读取或调用都在该处构建节点，且以相同方式书写的使用共享一个
  节点。

属性名在查找时忽略大小写，与 1.x 相同；对仅大小写匹配的声明的读取是
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。同名的 `Graph` 变量会在主体的其余部分隐藏该
声明（[`DSH4251`](../diagnostics/DSH4xxx.md#dsh4251)，警告）。

| 属性类别 | 读取是什么 |
| :-- | :-- |
| 标量声明 | `ScalarParameter` 的值 |
| 2 或 3 分量向量声明 | `VectorParameter` 的 `RG` / `RGB` 输出 |
| 4 分量向量声明、`VectorParameter` | `RGBA` 输出 |
| `const` 声明 | 常量节点的整个值 |
| 纹理声明 | 一个纹理对象 |
| `ChannelMaskParameter` | 其唯一输出，**1** 个分量 |
| `StaticComponentMaskParameter` | 其输出，宽度与其输入相同 |
| 一个纹理采样 token（15–20） | 该采样的 `RGBA` 输出，4 个分量 |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 一个具有多个输出的节点 |
| `StaticSwitchParameter` | 不是值 —— 参见 [下文](#staticswitchparameter) |

驱动此表的分量数量是声明所生成的分量数量，因此 `vec2 P` 读取 `RG`，
而 `VectorParameter P` 读取 `RGBA`。

- 对于读取而言，**声明顺序无关紧要**。一个 `Graph` 可以引用在块中更靠下处声明的
  属性：`Graph` 会在所有其他区段之后被读取。
- 声明在**文件作用域**声明 *（自 2.0.0 起）*：文件中的任何块都可以读取它。
  在使用处展开的 token 属于其自身的块。
- 没有任何读取的属性不会构建任何节点（[`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390)，信息）。
- 一个 `UE.*` 属性是在每个读取它的主体头部声明的局部变量，按 `Properties` 顺序。
  命名另一个 `UE.*` 属性的参数需要该属性在列表中更靠前并由同一
  主体读取；否则该名称未被声明（[`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)）。

Swizzle 作用于读取的值，而不是节点：`Tint.rgb`、`Sample.a`、`UV.yx`。参见
[Swizzle](../graph/swizzle.md)。

<a id="the-pin-call-form"></a>

## 引脚调用形式

```c
vec4 S = BaseTex(Coordinates = UV);
vec4 M = Keep(Input = S);
```

调用在此使用处构建属性的节点，使用其声明的默认值和元数据，并将每个具名参数
连接到输入引脚。*（自 2.0.0 起）* 该节点属于此次调用：具有相同参数的两个调用
共享一个节点，但在别处对同一属性的裸读取是其自身的节点，没有这些连接 —— 1.x 为每个属性缓存一个
节点，并为之后每次读取保留首次调用的连接。

每个参数名会按引擎目录所列的类的输入引脚进行匹配，然后
再与其属性匹配；仅在忽略大小写时匹配的情况是 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)。
在 1.x 源中，对于目录未列出但其值为数字的名称，会被视为以该节点设置命名的引脚
（[`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)，信息）：发射器会将其连接到
活动节点中具有该显示名称的引脚 —— 或具有将标识符不允许的每个字符都替换为 `_` 的显示名称的引脚
—— 或拒绝它（[`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)）。
任何其他未知名称是 [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)。

参数值是普通表达式，会转换为引脚的类型；无法转换的
是 [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)。调用上的输出选择器（`Output = …`、`OutputIndex = …`）
是 [`DSH5265`](../diagnostics/DSH5xxx.md#dsh5265)。

<a id="eligible-parameter-types"></a>

### 符合条件的参数类型

调用形式用于在使用处展开的 token：

```text
ChannelMaskParameter                  TextureSampleParameterCubeArray
StaticComponentMaskParameter          TextureSampleParameterVolume
TextureSampleParameter2D              TextureSampleParameterSubUV
TextureSampleParameter2DArray         RuntimeVirtualTextureSampleParameter
TextureSampleParameterCube            SparseVolumeTextureSampleParameter
```

`StaticSwitchParameter` 有其自己的调用形式，见[下文](#staticswitchparameter)。对
声明的调用 —— 每个紧凑 token、`ScalarParameter`、`StaticBoolParameter`、`VectorParameter`、
`TextureObjectParameter` —— 是 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)：它不命名任何函数。

<a id="pin-names-by-parameter-type"></a>

### 按参数类型列出的引脚名称

目录以引擎为其使用的 C++ 名称列出引脚，当两者不同时，以标识符形式的显示名称作为
别名。*（自 2.0.0 起）* 每个引脚都可以通过这种方式访问 —— 1.x 仅匹配显示
名称，而带有空格或括号的显示名称无法书写。

| 参数类型 | 目录所列的引脚 | 1.x 调用仍可使用的显示名称（`DSH5291`） |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `Input` | — |
| `StaticComponentMaskParameter` | `Input` | — |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | `Coordinates`、`TextureObject`、`MipValue`、`CoordinatesDX`、`CoordinatesDY`、`AutomaticViewMipBiasValue`（别名 `Apply_View_MipBias`） | `[MipValueMode="MipLevel"]` 下的 `MipLevel`、`[MipValueMode="MipBias"]` 下的 `MipBias`、`[MipValueMode="Derivative"]` 下的 `DDX_UVs_` / `DDY_UVs_` |
| `RuntimeVirtualTextureSampleParameter` | `Coordinates`、`WorldPosition`（别名 `World_Position`）、`MipValue`、`DDX`、`DDY` | 节点为其设置所显示的名称 |
| `SparseVolumeTextureSampleParameter` | `Coordinates`、`TextureObject`、`MipValue`、`CoordinatesDX`、`CoordinatesDY` | — |
| `StaticSwitchParameter` | **单独形式**：`A`（别名 `True`）、`B`（别名 `False`） | — |
| 每个声明 | 无调用 | — |

> [!WARNING]
> **在纹理采样*参数*节点上保持 `TextureObject` 未连接。** Unreal 的
> `UMaterialExpressionTextureSampleParameter` 构造函数会清除 `bShowTextureInputPin`，并且已加载
> 的资产会丢弃与隐藏引脚的任何连接。使用默认值或使用
> `[Texture=Path(…)]` 设置该资产。`SparseVolumeTextureSampleParameter` 是例外 —— 它确实暴露该引脚。

<a id="types-that-look-callable-but-are-not"></a>

### 看起来可调用但实际不可调用的类型

| 标记 | 调用失败的原因 |
| :-- | :-- |
| `CurveAtlasRowParameter`、`FontSampleParameter`、`SpriteTextureSampler`、`TextureCollectionParameter`、`SparseVolumeTextureObjectParameter` | 该标记完全没有 2.0 形式（[`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)） |
| `TextureObjectParameter`、紧凑纹理标记 | 纹理对象没有输入引脚；请将其作为值读取并馈送给采样器（调用为 `DSH4208`） |
| `ScalarParameter`、`VectorParameter`、`StaticBoolParameter`、紧凑标记 | 没有可连线的输入引脚（调用为 `DSH4208`） |

<a id="staticswitchparameter"></a>

## StaticSwitchParameter

```c
vec3 C = UseDetail(True = DetailColor, False = BaseColor);
vec3 D = UseDetail(A = DetailColor, B = BaseColor);
vec3 E = UseDetail(DetailColor, BaseColor);          // positional, in that order
```

| 参数 | 可接受的写法 |
| :-- | :-- |
| true 分支 | `True=`，否则 `A=`，否则为第一个位置参数 |
| false 分支 | `False=`，否则 `B=`，否则为第二个位置参数 |

两个分支都是必需的（[`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258)）；任何其他参数都会被丢弃，并给出一个 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) 警告，并且输出选择器是
`DSH5265`。

这些分支会连接到节点的 `A` 和 `B` 引脚，这些引脚可接受任意宽度。结果宽度与较宽的分支一样，或者与某个分支所携带的材质或 Substrate 值一样。*(自 2.0.0 起)* 1.x 中对分支的检查——不能是纹理对象、不能将 `Substrate` 或 `MaterialAttributes` 与数字混用、分量数量必须相等——不再照此执行：引脚无法接受的分支会 `DSH5214`。

节点的 `DefaultValue` 是声明的默认值（`true` 或 `false`；没有默认值时为节点自身的 `false`），并且该开关会注册为材质的静态参数。

> [!WARNING]
> `StaticSwitchParameter` **不能作为值读取**。单独的 `UseDetail`——或 `UseDetail.r`——
> 不是已声明的名称（[`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)）。必须调用它。

<a id="other-surfaces"></a>

## 其他使用面

- **`UE.*` 属性**像任何其他属性一样读取：先在
  `Properties` 中 `UE.TexCoord(Index = 0) UV;`，然后在 `Graph` 中 `UV`。每个属性都会成为读取它的函数体开头的一个局部变量，并在那里
  构建一次。请参阅 [UE 内置项](../builtins/ue.md)。
- **匿名开关内置项。** `UE.StaticSwitchParameter(Name = …、Default = …、Group = …、
  Description = …、SortPriority = …)` 会就地构建同一个节点，因此静态开关不必
  先在 `Properties` 中声明。`ParameterName` 可以代表 `Name`，`DefaultValue` 可以代表
  `Default`；分支的给出方式如上所述。没有名称
  [`DSH5257`](../diagnostics/DSH5xxx.md#dsh5257)，默认值不是 `true` / `false`
  [`DSH5263`](../diagnostics/DSH5xxx.md#dsh5263)，`SortPriority` 不是整数
  [`DSH5264`](../diagnostics/DSH5xxx.md#dsh5264)；任何其他参数都会被丢弃，并给出 `DSH5254`。
- **ThinCustom 后端**在隐藏的基础材质上构建与 `Graph` 相同的节点图，因此
  属性的到达方式与上述完全一致。请参阅 [后端](../settings/backend.md)。

<a id="notes"></a>

## 说明

- 节点位置来自项目所选的[图布局](../generation/graph-layout.md)
  （*Graph Layout Style*），而不是固定坐标 *(自 2.0.0 起)*。
- 同时匹配 `Graph` 变量和声明的名称会解析为该变量，并带有
  `DSH4251`。请参阅[名称解析](../graph/name-resolution.md)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4200` | 对 `StaticSwitchParameter` 的裸读取，或名称既不是变量也不是属性 |
| `DSH4208` | 对作为声明的属性进行调用 |
| `DSH5259` | 引脚调用形式中的位置参数 |
| `DSH5265` | 对参数或静态开关调用使用 `Output` / `OutputIndex` |
| `DSH5213` | 既未命名引脚也未命名该类属性的参数 |
| `DSH5291` | 命名了目录未列出的引脚的参数（信息；在活动节点上连接） |
| `DSH8212` | 活动节点也没有该名称的引脚 |
| `DSH5276` | 仅在忽略大小写时匹配引脚或属性的参数（警告） |
| `DSH5214` | 引脚类型无法接受的参数 |
| `DSH5258` | 调用静态开关时未提供两个分支 |
| `DSH5254` | 静态开关不会读取的参数（警告；已丢弃） |
| `DSH5257` / `DSH5263` / `DSH5264` | `UE.StaticSwitchParameter` 没有名称 / 默认值不是布尔值 / 排序优先级不是整数 |
| `DSH4251` | `Graph` 变量隐藏了声明（警告） |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_GraphUsage")
{
    Properties = {
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White");

        TextureSampleParameter2D HeightTex = Path(Game, "Textures/T_Height") [
            MipValueMode = "MipLevel"
        ];

        ChannelMaskParameter         Pick = float4(1, 0, 0, 0);
        StaticComponentMaskParameter Keep [DefaultR = true; DefaultG = true; DefaultB = true];

        StaticSwitchParameter UseDetail = true;

        vec3  Tint   = vec3(1.0, 0.8, 0.6);
        float Detail = 0.25;

        UE.TexCoord(Index = 0) UV;
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec4  Albedo = BaseTex(Coordinates = UV);           // Coordinates pin
        vec4  High   = HeightTex(Coordinates = UV,
                                 MipValue    = 2.0);        // the mip level, as MipValueMode says
        vec4  Masked = Keep(Input = Albedo);                // Input pin, 4 components out
        float Chan   = Pick(Input = High);                  // Input pin, 1 component out

        vec3 Plain    = Albedo.rgb * Tint;                  // a variable read + swizzle
        vec3 Detailed = Masked.rgb * Detail;

        Color = UseDetail(True = Detailed, False = Plain);
        Rough = Chan;
    }
}
```

生成的节点：

```text
TextureSampleParameter2D  BaseTex     Coordinates <- TextureCoordinate UV
TextureSampleParameter2D  HeightTex   MipValueMode=TMVM_MipLevel, Coordinates <- UV, MipValue <- Constant(2)
StaticComponentMaskParameter Keep     DefaultR/G/B=true, Input <- BaseTex
ChannelMaskParameter      Pick        Input <- HeightTex
TextureCoordinate         UV          CoordinateIndex=0   (one node, shared by both samplers)
VectorParameter           Tint        read through RGB
ScalarParameter           Detail
StaticSwitchParameter     UseDetail   True <- Multiply(Keep.rgb, Detail), False <- Multiply(BaseTex.rgb, Tint)
```

`MipLevel = 2.0`，即此 `MipValueMode` 下引脚的显示名称，会通过
`DSH5291` 到达同一个引脚。*(自 2.0.0 起)* `Graph` 变量在 `Shader` 中不得被称为 `Base`：那是
`Outputs` 绑定的材质，而该名称的局部变量是
[`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220)。

<a id="see-also"></a>

## 另请参阅

- [参数](index.md) —— 枢纽与决策表
- [参数节点标记](parameter-nodes.md) —— 哪个标记生成哪个节点及其引脚
- [紧凑类型标记](compact-types.md) —— 本页读取的已声明分量数量
- [元数据块](metadata.md) —— 改变存在哪些引脚的键
- [SamplerType](sampler-type.md) —— `MipValueMode` 和其他纹理采样键
- [调用](../graph/calls.md) —— 通用调用语法和命名参数规则
- [Swizzle](../graph/swizzle.md) —— `.rgb`、`.a` 以及读取值上的通道掩码
- [转换](../graph/conversions.md) —— 分量数量兼容性
- [节点复用](../graph/node-reuse.md) —— 两处使用何时共享一个节点
- [名称解析](../graph/name-resolution.md) —— 变量先于属性的查找顺序
- [MaterialAttributes](../graph/material-attributes.md) —— 分支可以携带的值类型
- [UE 内置项](../builtins/ue.md) —— `UE.*` 属性和匿名静态开关内置项
- [后端](../settings/backend.md) —— 两个后端各自构建什么
