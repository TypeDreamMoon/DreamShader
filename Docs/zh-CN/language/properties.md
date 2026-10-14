---
title: Properties 类型
description: Properties 声明接受的全部类型 token —— 39 个紧凑 token、22 个参数节点 token（其中 15 个能构建），它们的默认值、在 Graph 中的读法与调用形式。
---

`Properties` 段里的一条声明会变成生成材质里的一个具名节点 —— 实例可以覆盖的材质参数、常量，或内置输入节点。生成
哪个节点由你写的类型 token 决定：token 选定 Unreal 表达式类、决定默认值的写法，并且对少数几种类型来说，还决定这条
声明能不能在 `Graph` 里被**调用**以连接输入引脚。

命名类型有两种方式。**紧凑 token**（`float`、`vec3`、`Texture2D`）只说明值是什么，由 DreamShader 选节点；
**显式 `*Parameter` token**（`TextureSampleParameter2D`、`ChannelMaskParameter`）直接指定 Unreal 表达式类。
优先用紧凑 token，只有在紧凑集合做不到时才用显式 token。

（自 2.0.0 起） 起 1.x 源码由 legacy 前端读入，它把每个 property 变成 2.0 语言对应的写法：一个 `uniform`、一个
`static const`，或在读取处构建的参数节点。`.dss` 的写法见 [DreamShaderLang 2.0](../language-v2/index.md)；
[`dsc migrate`](../tools/migrate.md) 会替你写出来。

<a id="声明的形状"></a>

## 声明的形状

```c
[const] <type-token> <name> [ = <default-value> ] [ [ <metadata-entry> { ; | , } … ] ] ;
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

`<default-value>` 外面和整个元数据块外面的 `[ … ]` 是表示“可选”的元符号；元数据块自身的方括号是**字面标点**。
带元数据的声明写作 `float A = 1.0 [Group="X"];`。元数据语法见[元数据与分组](../parameters/metadata.md)，外层段落
语法见 [Section](sections.md#properties)。

所有类型 token 都**大小写不敏感**。`FLOAT3`、`Float3`、`float3` 是同一个 token。

<a id="四类-token"></a>

## 四类 token

| 类别 | 数量 | 示例 | 生成的节点 |
| :-- | :-- | :-- | :-- |
| 紧凑标量 | 7 | `float Strength = 1.0;` | `UMaterialExpressionScalarParameter` |
| 紧凑向量 | 27 | `vec3 Tint = vec3(1, 1, 1);` | `UMaterialExpressionVectorParameter` |
| 紧凑纹理 | 5 | `Texture2D Base = Path(Game, "T_X");` | `UMaterialExpressionTextureObjectParameter` |
| 显式 `*Parameter` 节点 | 22，其中 15 个能构建 | `TextureSampleParameter2D Tex;` | 指定的 `UMaterialExpression` 子类 |

还有第五种形式 `UE.<Name>( … ) <name>;`，把引擎输入节点（UV、时间、顶点色、参数集合读取）声明成属性，见
[UE.\* 节点](../builtins/ue.md#properties-声明形式)。

不匹配以上任何一种的 token 报 `DSH3252`；七个没有 2.0 写法的参数节点 token 报 `DSH3253` （自 2.0.0 起）。

<a id="该用哪一种"></a>

### 该用哪一种

| 目标 | 写法 | 原因 |
| :-- | :-- | :-- |
| 美术能在实例上调的浮点数 | `float`、`half`、`int`、`uint`、`bool`（或 `ScalarParameter`） | 七个紧凑标量 token 和 `ScalarParameter` 生成完全相同的 `ScalarParameter` 节点 |
| 颜色或 2/3/4 分量参数 | `float2` … `bvec4` | 声明的分量数决定 `Graph` 读哪个输出（`RG` / `RGB` / `RGBA`） |
| 永远按 RGBA 读的四分量参数 | `VectorParameter` | 固定 4 分量 |
| 自己采样的纹理 | `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | 生成该维度的纹理*对象*参数 |
| 自带采样节点、采样方式可配置的纹理 | `TextureSampleParameter2D` 等 | 拥有 `Coordinates` 和 mip 引脚；接受 `SamplerType` / `MipValueMode` 元数据 |
| 维度不固定的纹理对象 | `TextureObjectParameter` | 维度取自默认资产 （自 1.6.0 起） |
| 编译期分支 | `StaticSwitchParameter` | 必须用调用形式 `N(True = …, False = …)` |
| 不带分支的编译期布尔 | `StaticBoolParameter` | 只是值 |
| 美术不能改的烘焙值 | `const <紧凑 token>` | 生成常量节点 —— 不是参数，实例无法覆盖 |
| 引擎喂进来的节点（UV、时间、相机、顶点色、MPC） | `UE.<Name>( … )` | 见 [UE.\* 节点](../builtins/ue.md) |
| 其他任何 `UMaterialExpression` 类 | `UE.<Class>( … )` | 通用反射构造 —— 见[通用 UE.Expression](../builtins/ue-expression.md) |

<a id="紧凑标量-token"></a>

## 紧凑标量 token

七种拼写，在所有可观察的方面完全一致：`float`、`float1`、`half`、`half1`、`int`、`uint`、`bool`。都生成
`UMaterialExpressionScalarParameter`；`const` 时生成 `UMaterialExpressionConstant`。`int1`、`uint1` 和 `bool1`
也是标量 token （自 2.0.0 起）：token 按同名的 2.0 类型读取，而 HLSL 就是这样拼写标量的。

**提示：**

`int`、`uint`、`bool`、`half` 在节点层面**不带**任何整数、布尔或精度语义。它们统统生成浮点 `ScalarParameter`，
材质图也不会做任何截断。写它们是为了可读性，不是为了行为。

<a id="标量默认值"></a>

### 标量默认值

```c
float A = 0.5;    float B = -2;    float C = 2.5f;    float D = 1e3;    float E = true;
```

值是数字 —— 可选的符号、最多一个 `.` 的数字、可选的指数（`1e3`、`2.5E-1`）、可选的结尾 `f` —— 或 `true` / `false`
（大小写不限），即 `1.0` 和 `0.0`。其他内容报 `DSH3254`（2.0.0 – 2.1.0 连指数也拒绝，而 1.x 能读；
（自 2.1.1 起） 起恢复）。数字后面紧跟字母，如 `1abc`，连一个 token 都不是：报 `DSH2105` （自 2.0.0 起）
（1.x 读作 `1.0`）。

不写 `= <default>` 时默认值是 `0`。

<a id="紧凑向量-token"></a>

## 紧凑向量 token

27 种拼写：`float2`–`float4`、`half2`–`half4`、`vec2`–`vec4`、`int2`–`int4`、`uint2`–`uint4`、`bool2`–`bool4`、
`ivec2`–`ivec4`、`uvec2`–`uvec4`、`bvec2`–`bvec4`。每一种都生成 `UMaterialExpressionVectorParameter`，其
`DefaultValue` 是完整的 `FLinearColor`；不同的只有**声明的分量数** —— token 结尾的数字 —— 它决定 `Graph` 读取时
命中哪个节点输出。`const` 时按分量数生成 `Constant2Vector`、`Constant3Vector` 或 `Constant4Vector`；3 分量的
`Constant3Vector` 会被强制 `A = 1.0`。

`vec*`、`ivec*`、`uvec*`、`bvec*` 只是 GLSL 风格拼写，行为与 `float*`、`int*`、`uint*`、`bool*` 完全相同。没有单分量
**向量**的紧凑 token：`float1` 和 `half1` 属于标量。

<a id="向量默认值"></a>

### 向量默认值

```c
<anything> ( <part> [ , <part> ] … )
```

| 规则 | 行为 |
| :-- | :-- |
| `(` 之前的文本 | **被忽略** —— `float4(…)`、`vec3(…)`、`(…)`、`banana(…)` 都能解析 |
| 分隔范围 | 从第一个 `(` 到**最后一个** `)` |
| 各部分 | 按每个 `,` 拆分；空的部分被丢弃；每部分是数字（符号、数字、一个 `.`、指数、结尾 `f`），或 `true` / `false` |
| 1 个 `a` | `(a, a, a, 1)` |
| 2 个 `a, b` | `(a, b, 0, 0)` |
| 3 个 `a, b, c` | `(a, b, c, 1)` |
| 4 个 `a, b, c, d` | `(a, b, c, d)` |
| 超过 4 个 | 第 5 个及以后**从不读取**，连解析都不做 |
| 没有 `( … )`，或某部分无法解析 | `DSH3254` |

声明保留这四个值中的前 *N* 个（*N* 是它的分量数），参数节点的 `DefaultValue` 是这 *N* 个值，缺的通道为 `0`、alpha
为 `1`：`float2 T = float2(4, 4)` 是 `(4, 4, 0, 1)` （自 2.0.0 起）（1.x 写成 `(4, 4, 0, 0)`）。

不写 `= <default>` 时，2、3 分量 token 的默认值是 `(0, 0, 0, 1)`，4 分量是 `(0, 0, 0, 0)` （自 2.0.0 起）（1.x
保留节点自己的默认值）。

**警告：**

**声明的分量数从不与字面量的元数做校验。** `float2 P = float4(1, 2, 3, 4);` 和 `float4 P = vec2(1, 2);` 都能解析。
字面量按上表填满四个值，声明的分量数保留前面那几个 —— 于是 `float4 P = vec2(1, 2)` 得到按 RGBA 读取的 `(1, 2, 0, 0)`。
`(` 之前的 token 同样不校验，所以 `flaot3(1, 0, 0)` 这种笔误会被静默接受。字面量的元数和拼写要与声明的 token 一致。

<a id="紧凑纹理-token"></a>

## 紧凑纹理 token

五种拼写，四种不同维度。每一种都生成纹理**对象**参数（`const` 时是 `UMaterialExpressionTextureObject`）—— 节点携带
资产，但不采样它。

| Token | 纹理类型 |
| :-- | :-- |
| `Texture2D` | `Texture2D` |
| `TextureCube` | `TextureCube` |
| `Texture2DArray` | `Texture2DArray` |
| `Texture3D` | `VolumeTexture` （自 1.3.8 起） |
| `VolumeTexture` | `VolumeTexture` （自 1.3.8 起） |

`Texture3D` 和 `VolumeTexture` 是严格同义词。要采样它们，在 `Graph` 里交给采样调用，或者直接声明
`TextureSampleParameter2D`。

<a id="纹理默认值"></a>

### 纹理默认值

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("/Game/Textures/T_X");
Texture2D C = "/Game/Textures/T_X";                          // 裸引号绝对路径
Texture2D D = /Script/Engine.Texture2D'/Game/Textures/T_X.T_X';  // 直接粘贴 Copy Reference
```

裸引号形式自 （自 1.5.0 起） 起可用；内容浏览器 **Copy Reference** 复制出的 `Class'…'` 引用可以原样粘贴
（自 1.9.0 起）。全部写法见 [Path 资产引用](../parameters/path.md)。默认值在构建材质时解析：解析不出路径报
`DSH8271`（消息里引用解析器自己的码），解析出来却加载不了报 `DSH8218`。`const` 纹理的默认值写到它的
`TextureObject` 节点的 `Texture` 属性上，加载失败时写入空、不报诊断 —— 见[元数据与分组](../parameters/metadata.md#对象属性)。

<a id="未指定资产时的默认值"></a>

### 未指定资产时的默认值

| 声明的纹理类型 | 回退资产 |
| :-- | :-- |
| `Texture2D` | 节点自己的默认值 `/Engine/EngineResources/DefaultTexture` |
| `TextureCube` | `/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube` |
| `VolumeTexture` | `/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture` |
| `Texture2DArray` | **不存在** |

**警告：**

`Texture2DArray` 没有引擎回退资产。没写 `= Path(…)` 的 `Texture2DArray` 会保留节点那张 2D 默认纹理 —— 不是数组 ——
而编译器什么都不说 （自 2.0.0 起）（1.x 拒绝这条声明）。请显式指定一个数组资产。

<a id="维度校验"></a>

### 维度校验

（自 2.0.0 起） 默认值加载出的资产**不再**和声明的维度比对。在加载之前检查的，是内容浏览器引用的 `Class'…'`
外壳里写的类：另一维度的纹理类报 `DSH1044`，DreamShader 知道不是纹理的类（`MaterialFunction`、`CurveLinearColor`、
`Font`……）报 `DSH1043`。见 [Path 资产引用](../parameters/path.md#类检查)。

资产赋上之后，除非声明写了 `SamplerType`，否则会执行 `AutoSetSampleType()`，采样类型跟随资产 —— 见
[SamplerType](../parameters/metadata.md#samplertype)。

<a id="const-声明"></a>

## `const` 声明

（自 1.2.6 起） `const` 声明生成常量节点而不是参数：材质参数列表里没有它的名字，实例也无法覆盖。

```c
const float Gamma = 2.2;                       // Constant
const vec3  Sky   = vec3(0.2, 0.4, 0.9);       // Constant3Vector
const Texture2D Lut = Path(Game, "T_Lut");     // TextureObject
```

`const` 可以和 39 个紧凑 token 搭配，（自 2.0.0 起） 起也可以和 `ScalarParameter`、`VectorParameter`、
`TextureObjectParameter` 搭配 —— 它们分别建出和 `const float`、`const float4`、`const Texture2D` 相同的常量。其他
`*Parameter` token 上的 `const` 报 `DSH3253`；`UE.*` 声明上的 `const` 被忽略 （自 2.0.0 起）。

`const` property 只保留常量节点有的东西 （自 2.0.0 起）：`Description` 和 `SamplerType` 能到达 `const` 纹理的
`TextureObject` 节点；`const` 标量或向量上的描述被丢弃，而 `Group`、`SortPriority`、`Slider` —— 写的或从
`Group(…)` 作用域继承的 —— 都是 `DSH7224` 警告。`Graph` 读取 `const` 向量得到常量节点的整个值，不走
`RG` / `RGB` / `RGBA` 命名输出。

<a id="参数节点-token"></a>

## 参数节点 token

22 个直接指定 Unreal 参数表达式类的 token，集合是封闭的。（自 2.0.0 起） 起每个 token 取三种 2.0 形式之一：

- **声明** —— `ScalarParameter`、`StaticBoolParameter`、`VectorParameter`、`TextureObjectParameter` 就是对应紧凑
  token 会生成的 `uniform`（`float`、`/// @static` 的 `bool`、`float4`、`Texture2D`），不论读多少次都只有一个参数节点；
- **在每次使用处展开** —— 其余能构建的十一个不是声明：每次读取或调用都是一个带默认值和元数据的反射调用
  `UE.Expression(Class = "<Token>", ParameterName = …, …)`，写法相同的使用共享一个节点；
- **没有 2.0 写法** —— 七个 token 报 `DSH3253`。把这样的材质挪到 `.dss`，用 [`UE.Expression`](../builtins/ue-expression.md)
  建节点。

| # | Token | 2.0 形式 | 生成的类 | `= default` 接受 | `Graph` 用法 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | `ScalarParameter` | 声明 | `UMaterialExpressionScalarParameter` | 标量字面量 | 值 |
| 2 | `StaticBoolParameter` | 声明 | `UMaterialExpressionStaticBoolParameter` | 仅 `true` / `false` | 值 |
| 3 | `StaticSwitchParameter` | 展开 | `UMaterialExpressionStaticSwitchParameter` | 仅 `true` / `false` | **只能调用** —— `N(True = …, False = …)` |
| 4 | `VectorParameter` | 声明 | `UMaterialExpressionVectorParameter` | 向量字面量 | 值，4 分量 |
| 5 | `DoubleVectorParameter` | `DSH3253` | —— | —— | —— |
| 6 | `ChannelMaskParameter` | 展开 | `UMaterialExpressionChannelMaskParameter` | 向量字面量 | `N(Input = …)`，**1** 分量 |
| 7 | `StaticComponentMaskParameter` | 展开 | `UMaterialExpressionStaticComponentMaskParameter` | 无 —— 见下 | `N(Input = …)` |
| 8 | `CurveAtlasRowParameter` | `DSH3253` | —— | —— | —— |
| 9 | `DynamicParameter` | `DSH3253` | —— | —— | —— |
| 10 | `FontSampleParameter` | `DSH3253` | —— | —— | —— |
| 11 | `SpriteTextureSampler` | `DSH3253` | —— | —— | —— |
| 12 | `TextureObjectParameter` | 声明 | `UMaterialExpressionTextureObjectParameter` | 资产引用 | 值（纹理） |
| 13 | `TextureCollectionParameter` | `DSH3253` | —— | —— | —— |
| 14 | `SparseVolumeTextureObjectParameter` | `DSH3253` | —— | —— | —— |
| 15 | `TextureSampleParameter2D` | 展开 | `UMaterialExpressionTextureSampleParameter2D` | 资产引用 | `N(Coordinates = …)`，读作 `RGBA` |
| 16 | `TextureSampleParameter2DArray` | 展开 | `UMaterialExpressionTextureSampleParameter2DArray` | 资产引用 | 同 15 |
| 17 | `TextureSampleParameterCube` | 展开 | `UMaterialExpressionTextureSampleParameterCube` | 资产引用 | 同 15 |
| 18 | `TextureSampleParameterCubeArray` | 展开 | `UMaterialExpressionTextureSampleParameterCubeArray` | 资产引用 | 同 15 |
| 19 | `TextureSampleParameterVolume` | 展开 | `UMaterialExpressionTextureSampleParameterVolume` | 资产引用 | 同 15 |
| 20 | `TextureSampleParameterSubUV` | 展开 | `UMaterialExpressionTextureSampleParameterSubUV` | 资产引用 | 同 15 |
| 21 | `RuntimeVirtualTextureSampleParameter` | 展开 | `UMaterialExpressionRuntimeVirtualTextureSampleParameter` | 无 —— 见下 | `N(Coordinates = …)`，多个输出 |
| 22 | `SparseVolumeTextureSampleParameter` | 展开 | `UMaterialExpressionSparseVolumeTextureSampleParameter` | 无 —— 见下 | `N(Coordinates = …)`，多个输出 |

展开 token 的类在引擎节点目录里查找；这个引擎没有的类报 `DSH5212`。

<a id="每个-token-的专属元数据键"></a>

### 每个 token 的专属元数据键

每个 token 都接受通用元数据键（`Group`、`Description`、`SortPriority`、`Slider(min, max)`、`ParameterName`）以及它的类
上任何反射 UPROPERTY。实践中常用的类专属键：

| Token | 绑定类专属 UPROPERTY 的键 |
| :-- | :-- |
| `ScalarParameter` | `SliderMin`、`SliderMax`、`PrimitiveDataIndex` |
| `VectorParameter` | `UseCustomPrimitiveData`（绑定 `bUseCustomPrimitiveData`）、`PrimitiveDataIndex`、`ChannelNames` |
| `ChannelMaskParameter` | `MaskChannel`（枚举 `EChannelMaskParameterColor`） |
| `StaticComponentMaskParameter` | `DefaultR`、`DefaultG`、`DefaultB`、`DefaultA`（布尔） |
| `TextureObjectParameter` | `Texture`、`SamplerType`、`IsDefaultMeshpaintTexture` |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV`（15–20） | `Texture`、`SamplerType`、`SamplerSource`、`MipValueMode`、`GatherMode` （自 UE 5.6 起）、`AutomaticViewMipBias`、`ConstCoordinate`、`ConstMipValue`、`IsDefaultMeshpaintTexture`；`TextureSampleParameterSubUV` 另有 `bBlend` |
| `RuntimeVirtualTextureSampleParameter` | `VirtualTexture`、`MaterialType`、`bSinglePhysicalSpace`、`bAdaptive`、`MipValueMode`、`TextureAddressMode` |
| `SparseVolumeTextureSampleParameter` | `SparseVolumeTexture`、`MipValueMode`、`SamplerSource`、`ConstMipValue` |

声明和展开 token 对类上没有的键报法不同 —— 见[元数据与分组](../parameters/metadata.md#反射属性透传)。

<a id="默认值走哪个解析器"></a>

### 默认值走哪个解析器

| Token | 接受 | 否则 |
| :-- | :-- | :-- |
| `ScalarParameter` | 标量字面量 | `DSH3254` |
| `StaticBoolParameter`、`StaticSwitchParameter` | 仅 `true` / `false` | `DSH3254` |
| `VectorParameter`、`ChannelMaskParameter` | 向量字面量 | `VectorParameter` 上 `DSH3254`；`ChannelMaskParameter` 上读不出的字面量不报诊断直接丢弃 |
| `TextureObjectParameter` | [`Path(…)`](../parameters/path.md) 资产引用 | 构建材质时解析：`DSH8271` / `DSH8218` |
| 纹理采样 token 15–20 | `Path(…)` 资产引用 | 构建材质时解析：`DSH8213`，引用解析器的码 |
| `StaticComponentMaskParameter` | 无 —— 类没有 `DefaultValue` | 默认值被 `DSH5288` 丢弃；写 `[DefaultR = true; …]` |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 无 | 默认值**不报诊断**直接丢弃 （自 2.0.0 起） |

**警告：**

**`RuntimeVirtualTextureSampleParameter` 和 `SparseVolumeTextureSampleParameter` 的默认值会被忽略。** 这两个 token 上的
`= Path(…)` 到不了节点的任何属性，而且没有任何提示。改用元数据绑定资产 —— `[VirtualTexture = Path(…)]`、
`[SparseVolumeTexture = Path(…)]`。

纹理采样 token 的默认值写到节点的 `Texture` 属性；能解析但加载不了的资产写入空、不报诊断，节点随后取引擎默认纹理。
**没有纹理的采样参数照样能编译**：对 `UMaterialExpressionTextureSampleParameter` 的子类会调用 `SetDefaultTexture()`。
runtime virtual texture 和 sparse volume 节点不是这种子类，仍必须绑定资产。

<a id="维度推断"></a>

### 维度推断

纹理采样 token 按 `TextureSampleParameter` 之后的后缀确定维度：`2D` → `Texture2D`、`2DArray` → `Texture2DArray`、
`Cube` → `TextureCube`、`CubeArray` → 无（任何纹理）、`Volume` → `VolumeTexture`、`SubUV` → `Texture2D`；RVT 和 SVT
不判断。后缀整体比较 （自 2.0.0 起），所以 `TextureSampleParameterCubeArray` 不再被当成 cube。维度只用于一件事：
判断 `Class'…'` 默认值里写的类。

`TextureObjectParameter` **不**声明维度：它取默认值加载出的资产的维度 —— cube、2D 数组或 volume，否则 `Texture2D` ——
因此它能接受其中任何一种 （自 1.6.0 起）。

| Token 家族 | 检查什么 |
| :-- | :-- |
| 紧凑 `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | `Class'…'` 外壳：另一维度 `DSH1044`，非纹理类 `DSH1043`。加载出的资产不检查 （自 2.0.0 起） |
| `TextureObjectParameter` | 只检查外壳的「不是纹理」那一半（`DSH1043`） |
| 纹理采样 token 15–17、19、20 | 外壳，同紧凑 token |
| `TextureSampleParameterCubeArray` | 只检查外壳的「不是纹理」那一半 |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 不检查；默认值不被读取 |

**警告：**

用普通对象路径给 `TextureSampleParameterCube` 指定一张 2D 贴图，DreamShader 不会报任何诊断；只有 `Texture2D'…'` 外壳会
暴露不匹配（`DSH1044`）。否则错误会在稍后由 Unreal 着色器编译器抛出。请指定正确维度的资产。

<a id="在-graph-中使用参数"></a>

## 在 Graph 中使用参数

<a id="值读取"></a>

### 值读取

property 是两种东西之一 （自 2.0.0 起）：

- **声明**（所有紧凑 token、`ScalarParameter`、`StaticBoolParameter`、`VectorParameter`、`TextureObjectParameter`）：
  文件作用域的 `uniform` 或 `static const`，不论读多少次都是一个节点；
- **在每次使用处展开**的 token（`StaticSwitchParameter`、两个 mask、纹理采样 token）：每次读取或调用都在那里建节点，
  写法相同的使用共享一个节点。

property 名按忽略大小写查找，和 1.x 一样；对声明的读取只在大小写上匹配时给出 `DSH5275`。同名的 `Graph` 变量会在
函数体余下部分遮蔽声明（`DSH4251`，警告）。

| property 种类 | 读到的是 |
| :-- | :-- |
| 标量声明 | `ScalarParameter` 的值 |
| 2、3 分量向量声明 | `VectorParameter` 的 `RG` / `RGB` 输出 |
| 4 分量向量声明、`VectorParameter` | `RGBA` 输出 |
| `const` 声明 | 常量节点的整个值 |
| 纹理声明 | 纹理对象 |
| `ChannelMaskParameter` | 它的唯一输出，**1** 分量 |
| `StaticComponentMaskParameter` | 它的输出，和输入一样宽 |
| 纹理采样 token（15–20） | 采样的 `RGBA` 输出，4 分量 |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 多输出的节点 |
| `StaticSwitchParameter` | 不是值 —— 见[下文](#staticswitchparameter) |

所以 `vec2 P` 读 `RG`，而 `VectorParameter P` 读 `RGBA`。

- **声明顺序不限制读取。** `Graph` 可以读后面才声明的 property。例外是 `UE.*` property：它是每个读取它的函数体
  开头按 `Properties` 顺序声明的局部变量，所以一个参数里引用另一个 `UE.*` property 时，那个 property 要排在前面并且被
  同一个函数体读取，否则名字未声明（`DSH4200`）。
- 声明位于**文件作用域** （自 2.0.0 起）：文件里任何块都能读它。展开 token 属于自己的块。
- 没有任何东西读取的 property 不建节点（`DSH4390`，提示）。

swizzle 作用在读出的值上，不作用在节点上：`Tint.rgb`、`Sample.a`、`UV.yx`。

<a id="引脚调用形式"></a>

### 引脚调用形式

（自 1.4.1 起）

```c
vec4 S = BaseTex(Coordinates = UV);
vec4 M = Keep(Input = S);
```

调用在此处用声明的默认值和元数据建出 property 的节点，并把每个命名参数连到一个输入引脚。（自 2.0.0 起） 节点
属于这次调用：参数相同的两次调用共享一个节点，但别处对同一个 property 的裸读取是另一个节点、不带这些连线 —— 1.x 每个
property 缓存一个节点，后来的读取都带着第一次调用的连接。

没有位置参数写法：每个参数都必须命名（`DSH5259`）。参数名先和引擎目录列出的输入引脚比对，再和属性比对；只在忽略
大小写时匹配给出 `DSH5276`。1.x 源码里，目录没列出、但值是数字的名字被当作以节点设置命名的引脚（`DSH5291`，提示）：
发射器把它连到实际节点上那个显示名 —— 或把标识符放不下的字符换成 `_` 之后的显示名 —— 的引脚，否则报 `DSH8212`。其他
未知名字报 `DSH5213`。参数值是普通表达式，转换成引脚的类型，转换不了报 `DSH5214`。调用上的输出选择器（`Output = …`、
`OutputIndex = …`）报 `DSH5265`。

调用形式只适用于在使用处展开的 token：

```text
ChannelMaskParameter                  TextureSampleParameterCubeArray
StaticComponentMaskParameter          TextureSampleParameterVolume
TextureSampleParameter2D              TextureSampleParameterSubUV
TextureSampleParameter2DArray         RuntimeVirtualTextureSampleParameter
TextureSampleParameterCube            SparseVolumeTextureSampleParameter
```

对声明 —— 所有紧凑 token、`ScalarParameter`、`StaticBoolParameter`、`VectorParameter`、`TextureObjectParameter` ——
做调用报 `DSH4208`：它不是函数名。

**警告：**

**紧凑 `Texture2D` 不能被调用。** 纹理对象没有输入引脚。要么把它当值读出来喂给采样器，要么一开始就声明
`TextureSampleParameter2D`。

<a id="各参数类型的引脚名"></a>

### 各参数类型的引脚名

目录用引擎的 C++ 名列出引脚，两者不同时把显示名的标识符形式作为别名。（自 2.0.0 起） 起每个引脚都能这样写到 ——
1.x 只匹配显示名，含空格或括号的那些写不出来。

| 参数类型 | 目录列出的引脚 | 1.x 调用仍可用的显示名（`DSH5291`） |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `Input` | —— |
| `StaticComponentMaskParameter` | `Input` | —— |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | `Coordinates`、`TextureObject`、`MipValue`、`CoordinatesDX`、`CoordinatesDY`、`AutomaticViewMipBiasValue`（别名 `Apply_View_MipBias`） | `[MipValueMode="MipLevel"]` 下的 `MipLevel`、`[MipValueMode="MipBias"]` 下的 `MipBias`、`[MipValueMode="Derivative"]` 下的 `DDX_UVs_` / `DDY_UVs_` |
| `RuntimeVirtualTextureSampleParameter` | `Coordinates`、`WorldPosition`（别名 `World_Position`）、`MipValue`、`DDX`、`DDY` | 节点按其设置显示的名字 |
| `SparseVolumeTextureSampleParameter` | `Coordinates`、`TextureObject`、`MipValue`、`CoordinatesDX`、`CoordinatesDY` | —— |
| `StaticSwitchParameter` | **单独的形式**：`A`（别名 `True`）、`B`（别名 `False`） | —— |

**警告：**

**纹理采样*参数*节点上的 `TextureObject` 不要连。** Unreal 的 `UMaterialExpressionTextureSampleParameter` 构造函数清掉了
`bShowTextureInputPin`，资产加载时会丢掉连到隐藏引脚的连线。用默认值或 `[Texture=Path(…)]` 设置资产。
`SparseVolumeTextureSampleParameter` 是例外 —— 它确实暴露该引脚。

<a id="staticswitchparameter"></a>

### StaticSwitchParameter

静态开关有专属调用形式，而且**不能当作值读取**：

```c
vec3 C = UseDetail(True = DetailColor, False = BaseColor);
vec3 D = UseDetail(A = DetailColor, B = BaseColor);
vec3 E = UseDetail(DetailColor, BaseColor);          // 位置参数，按此顺序
```

true 分支依次取 `True=`、`A=`、第一个位置参数；false 分支依次取 `False=`、`B=`、第二个位置参数。两个分支都必填
（`DSH5258`）；其他参数被 `DSH5254` 丢弃，输出选择器报 `DSH5265`。分支连到节点的 `A` 和 `B` 引脚，任何宽度都行；
结果和较宽的分支一样宽，或是某个分支携带的 material / Substrate 值。（自 2.0.0 起） 1.x 对分支的检查 —— 不许
纹理对象、不许 `Substrate` 或 `MaterialAttributes` 与数值混用、分量数相等 —— 不再单独做：引脚接不了的分支报 `DSH5214`。

节点的 `DefaultValue` 是声明的默认值（`true` 或 `false`；没写时是节点自己的 `false`），开关注册为材质的静态参数。
裸写 `UseDetail`（或 `UseDetail.r`）不是已声明的名字，报 `DSH4200`。

不想先在 `Properties` 里声明时，用匿名形式
[`UE.StaticSwitchParameter`](../builtins/ue.md#uestaticswitchparameter) 就地建同一个节点。

<a id="在-properties-中无效的-token"></a>

## 在 `Properties` 中无效的 token

这些在别处是真实存在的 DreamShaderLang 类型 token，但写在 `Properties` 声明里都报 `DSH3252` —— `StaticBool` 除外。

| Token | 有效位置 |
| :-- | :-- |
| `MaterialAttributes` | 材质函数的 `Inputs` / `Outputs` / `Results`；`Shader` 的 `Outputs` 声明；`Function` 签名 |
| `Substrate` | 同上，且仅限 （自 UE 5.4 起） |
| `SamplerState` | `Inputs` / `Outputs` / 函数签名，在那里是 `Texture2D` 的别名 |
| `StaticBool` | 材质函数的 `Inputs` / `Outputs`。在 `Properties` 里它被接受 （自 2.0.0 起），但读作 `bool` —— 生成的是**动态**的 `ScalarParameter`，不是静态布尔：要静态布尔就写 `StaticBoolParameter` |
| `mat2` / `mat3` / `mat4` | 只在 `Function` 签名或 `Code` 体中，解析器把它们规范成 `float2x2` / `float3x3` / `float4x4`；生成阶段随后拒绝矩阵类型 |

完整的跨上下文矩阵见[类型与值](types.md)。

<a id="命名规则"></a>

## 命名规则

- **property 名是标识符** （自 2.0.0 起）。`float 1Bad = 0;` 报 `DSH2105`（数字格式错误）和 `DSH3250`。
- **成为声明的 property 位于文件作用域** （自 2.0.0 起）。同名的两个 property —— 同一个块里，或同一文件的两个块里 ——
  报 `DSH4210`。名字精确比较。1.x「函数的 property 不能和输入同名」的规则在 2.0 里没有对应检查。
- `[ParameterName="…"]` 只改变材质暴露给实例和蓝图的名字；`Graph` 使用的仍是声明的标识符。
- 在 `VirtualFunction` 里，`Properties` 是 `Inputs` 的同义词，用的是带类型参数的语法，不是这一套。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3252: Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.` | token 不是紧凑 token、不是 \*Parameter token，也不以 UE. 开头。 | 对照上面的表检查拼写。 |
| `DSH3253: Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.` | 七个没有 2.0 形式的 token 之一，或 ScalarParameter / VectorParameter / TextureObjectParameter 之外的 token 上写了 const。 |  [详解](../builtins/ue-expression.md) |
| `DSH3250: Expected a property type and a name, found {0}.` | 语句不以类型和名字开头、缺名字，或 property 后面没有 ;。 |  |
| `DSH3251: Expected a property type after 'const', found {0}.` | const 后面没有类型。 |  |
| `DSH3254: Expected a default value after '{0} =', found {1}.` | = 后面没有默认值，或默认值这个 token 接受不了 —— 标量不是数字或 true/false，向量没有括号分量列表，静态开关不是 true/false。 |  |
| `DSH2105: Malformed number literal '{0}'.` | 数字后面紧跟字母（1abc），或 property 名以数字开头。 |  |
| `DSH1043: Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}.` | Class'…' 默认值写的类不是纹理。 |  [详解](../parameters/path.md) |
| `DSH1044: Asset reference is written as '{0}', but this property is declared as {1}.` | Class'…' 默认值写的是另一维度的纹理类。 |  [详解](../parameters/path.md) |
| `DSH8271: The default texture reference '{0}' of parameter '{1}' does not resolve to an asset path. {2}` | 纹理默认值解析不出资产路径。 |  [详解](../parameters/path.md) |
| `DSH8218: The default texture for parameter '{0}' could not be loaded from '{1}'.` | 纹理默认值解析出来了但加载不了。 |  |
| `DSH5212: '{0}' is not a material expression class this engine has.` | 引擎没有展开 token 的类。 |  |
| `DSH5288: '{0}.{1}' has no 'DefaultValue', so the default written for this parameter is dropped, as 1.x dropped it.` | 警告：StaticComponentMaskParameter 上写了默认值。 | 写 \[DefaultR = true; …\]。 |
| `DSH4390: '{0}' is declared but nothing reads it, so it is not in the generated material.` | 提示：没有任何东西读取这个 property。 |  |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | 同一文件里两个 property 同名。 |  |
| `DSH5259: Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument.` | 引脚调用形式里用了位置参数。 |  |
| `DSH5265: Expected the parameter call '{0}' to take only its inputs, found an output selector.` | 参数或静态开关调用上写了 Output / OutputIndex。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 参数既不是类的引脚也不是属性。 | 对照上面的引脚名表，或改用元数据。 |
| `DSH5291: '{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties.` | 提示，仅 1.x：参数名是目录没列出的引脚，在实际节点上连接。 |  |
| `DSH8212: '{0}' has no input pin named '{1}'.` | 实际节点上也没有这个名字的引脚。 |  |
| `DSH5214: {0} expects {1}, and this is {2}.` | 引脚类型接不住这个参数。 |  |
| `DSH5258: Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.` | 调用静态开关时缺一条分支。 |  |
| `DSH4208: '{0}' is not a function, a builtin or a struct.` | 对声明形式的 property 做了调用。 |  |
| `DSH4200: '{0}' is not declared; did you mean '{1}'? Names are case-sensitive.` | 裸读取 StaticSwitchParameter，或名字既不是变量也不是 property。 |  [详解](../graph/calls.md) |
| `DSH4251: '{0}' hides the file-scope declaration of the same name for the rest of this function.` | 警告：Graph 变量遮蔽了声明。 |  |
| `DSH7224: '@{0}' means nothing here; it belongs on {1}.` | 警告：const 标量或向量上的 Group、SortPriority 或 Slider。 |  |

按阶段罗列的完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_ParameterNodes")
{
    Properties = {
        const float Gamma = 2.2;                       // Constant

        Group("Surface") {
            TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White") [
                SamplerType   = "LinearColor";
                SamplerSource = "FromTextureAsset";
                MipValueMode  = "None";
            ];
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Tint      = float4(1.0, 0.8, 0.6, 1.0);
        }

        Group("Masks") {
            ChannelMaskParameter         Pick = float4(1, 0, 0, 0) [MaskChannel = "Red"];
            StaticComponentMaskParameter Keep [DefaultR = true; DefaultG = true];
        }

        StaticSwitchParameter UseDetail = true [Group="Switches"];

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
        vec4  Sample = BaseTex(Coordinates = UV);
        vec4  Masked = Keep(Input = Sample);
        float Chan   = Pick(Input = Sample);

        Color = UseDetail(True = Masked.rgb, False = Tint.rgb) * Gamma;
        Rough = Roughness * Chan;
    }
}
```

生成的节点：

```text
Constant(2.2)                 Gamma
TextureSampleParameter2D      BaseTex    SamplerType=LinearColor
ScalarParameter               Roughness  Group="Surface" SortPriority=0  SliderMin=0 SliderMax=1
VectorParameter               Tint       Group="Surface" SortPriority=10
ChannelMaskParameter          Pick       MaskChannel=Red
StaticComponentMaskParameter  Keep       DefaultR/G=true
StaticSwitchParameter         UseDetail  Group="Switches"
TextureCoordinate             UV         CoordinateIndex=0
```

`BaseTex`、`Pick`、`Keep` 是展开 token，所以 `Group(…)` 作用域不会给它们打标记 （自 2.0.0 起），`Roughness`
拿到计数器槽位 0。

<a id="下一步"></a>

## 下一步

- [元数据与分组](../parameters/metadata.md) —— `[ … ]` 块、`Slider(…)`、`SamplerType`、分组与排序
- [Path 资产引用](../parameters/path.md) —— 每种 `Path(…)` 写法、Copy Reference 与错误码
- [UE.\* 节点](../builtins/ue.md) —— 把引擎输入节点声明成属性
- [调用](../graph/calls.md) —— 这些形式所属的通用调用语法
