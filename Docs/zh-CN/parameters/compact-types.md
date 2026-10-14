---
title: "紧凑类型标记"
description: "紧凑类型标记"
---

<a id="compact-type-tokens"></a>

# 紧凑类型标记

> [DreamShader](../index.md) » [参数](../../parameters/index.md) » **紧凑类型标记**

`Properties` 声明可以在不指定 Unreal 表达式类的情况下使用的 39 个内置类型标记：7 个标量拼写、27 个向量拼写和 5 个纹理拼写。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 的 `Properties` 区段 |
| 种类 | 参数类型标记 |
| 生成 | `UMaterialExpressionScalarParameter`、`UMaterialExpressionVectorParameter`、`UMaterialExpressionTextureObjectParameter` — 或者在使用 `const` 时生成 `UMaterialExpressionConstant`、`Constant2Vector`、`Constant3Vector`、`Constant4Vector`、`UMaterialExpressionTextureObject` |
| 始于 | `1.2.0`；`VolumeTexture` 始于 `1.3.8`；`const` 始于 `1.2.6` |

<a id="synopsis"></a>

## 概要

```c
[const] { <scalar-token> | <vector-token> | <texture-token> } <name> [ = <default> ] [ [ <metadata> ] ] ;
```

所有标记的**比较不区分大小写**：`FLOAT3`、`Float3` 和 `float3` 是同一个标记。
紧凑属性是该名称的 2.0 类型的 `uniform`（`vec3` 是 `float3`），或者是带有 `const` 的 `static const`。

<a id="scalar-tokens"></a>

## 标量标记

七种拼写，在所有可观察方面都完全相同。

| 标记 | 类型 | 分量数 | 节点（参数） | 节点（`const`） |
| :-- | :-- | :-- | :-- | :-- |
| `float` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `float1` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `half` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `half1` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `int` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `uint` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `bool` | 标量 | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |

`int1`、`uint1` 和 `bool1` 也是标量标记*（始于 2.0.0）*：该标记被读取为该名称的 2.0 类型，而 HLSL 就是这样拼写标量的。

> [!NOTE]
> `int`、`uint`、`bool` 和 `half` 在节点级别**不**携带任何整数、布尔或精度语义。在每种情况下生成的节点都是浮点 `ScalarParameter`，并且材质图不执行任何截断。整数标记唯一存在的地方是 `Graph` 中的直接整数*构造函数*调用——请参见[转换](../graph/conversions.md#float-int-and-bool)。

<a id="scalar-default-grammar"></a>

### 标量默认值语法

```c
float A = 0.5;      float B = -2;      float C = 2.5f;      float D = true;
```

该值是一个数字——可选符号、数字（最多一个小数点 `.`）、可选指数（`1e3`、`2.5E-1`）、可选尾随 `f`——或者是 `true` / `false`（任意大小写），它们分别代表 `1.0` 和 `0.0`。其他任何内容都是 [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) *（2.0.0 – 2.1.0 也拒绝指数，而 1.x 是可以读取的）*。粘在字母上的数字，例如 `1abc`，甚至不是一个标记：它是 [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *（始于 2.0.0；1.x 将其读取为 `1.0`）*。

没有 `= <default>` 时，默认值为 `0`。

<a id="vector-tokens"></a>

## 向量标记

27 种拼写。每一种都会生成一个 `UMaterialExpressionVectorParameter`，其 `DefaultValue` 是一个完整的 `FLinearColor`；只有声明中的分量数不同，而正是它决定 `Graph` 读取以哪个节点输出为目标。

该数量就是标记结尾的数字。

| 标记 | 分量数 | 节点（参数） | 节点（`const`） |
| :-- | :-- | :-- | :-- |
| `float2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `float3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `float4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `half2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `half3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `half4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `vec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `vec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `vec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `int2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `int3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `int4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `uint2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `uint3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `uint4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `bool2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `bool3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `bool4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `ivec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `ivec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `ivec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `uvec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `uvec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `uvec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `bvec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `bvec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `bvec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |

分量数为 3 的 `const` 声明会在生成的 `Constant3Vector` 上强制启用 `A = 1.0`。

<a id="vector-default-grammar"></a>

### 向量默认值语法

```c
<anything> ( <part> [ , <part> ] … )
```

| 规则 | 行为 |
| :-- | :-- |
| `(` 之前的文本 | **忽略**——`float4(…)`、`vec3(…)`、`(…)` 和 `banana(…)` 都能解析 |
| 分隔符 | 从第一个 `(` 到**最后一个** `)` |
| 各部分 | 在每个 `,` 处拆分；空部分被丢弃；每个部分是一个数字（符号、数字、一个小数点 `.`、一个指数、一个尾随 `f`），或者是 `true` / `false`（不区分大小写） |
| 1 个部分 `a` | `(a, a, a, 1)` |
| 2 个部分 `a, b` | `(a, b, 0, 0)` |
| 3 个部分 `a, b, c` | `(a, b, c, 1)` |
| 4 个部分 `a, b, c, d` | `(a, b, c, d)` |
| 超过 4 个部分 | 第 5 个及之后的部分**永远不会被读取**——甚至不会被解析，因此那里出现无法解析的部分也不算是错误 |
| 没有 `( … )`，或有无法解析的部分 | [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) |

声明会保留这四个值中的前 *N* 个，*N* 即其分量数，而参数节点的 `DefaultValue` 就是这 *N* 个值，缺失的通道为 `0`、alpha 为 `1`：
`float2 T = float2(4, 4)` 是 `(4, 4, 0, 1)` *（始于 2.0.0；1.x 写入的是 `(4, 4, 0, 0)`）*。

没有 `= <default>` 时，对于 2 或 3 分量的标记，默认值为 `(0, 0, 0, 1)`，而对于 4 分量的标记则为 `(0, 0, 0, 0)` *（始于 2.0.0）*。

> [!WARNING]
> **声明中的分量数从不会与字面量的元数进行核对。** `float2 P = float4(1, 2, 3, 4);`
> 和 `float4 P = vec2(1, 2);` 都能解析，且没有任何诊断。字面量按上述表格填充四个值，
> 而声明的分量数保留前几个——因此 `float4 P = vec2(1, 2)` 会得到按 RGBA 读取的
> `(1, 2, 0, 0)`，而 `float2 P = float4(1, 2, 3, 4)` 会得到 `(1, 2)`。请让字面量的元数
> 与声明的标记保持一致。

> [!WARNING]
> `(` 之前的标记同样不会被校验，因此像 `flaot3(1, 0, 0)` 这样的拼写错误会被
> 静默接受为向量字面量。只有*声明本身*的类型标记会被检查。

<a id="texture-tokens"></a>

## 纹理标记

五种拼写，四种不同的维度。

| 标记 | 纹理类型 | 显式维度 | 节点（参数） | 节点（`const`） |
| :-- | :-- | :-- | :-- | :-- |
| `Texture2D` | `Texture2D` | 是 | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `TextureCube` | `TextureCube` | 是 | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `Texture2DArray` | `Texture2DArray` | 是 | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `Texture3D` | `VolumeTexture` | 是 | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `VolumeTexture` | `VolumeTexture` | 是 | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |

`Texture3D` 和 `VolumeTexture` 是完全同义的。

紧凑纹理声明会生成一个纹理**对象**参数——而不是采样节点。要对其进行采样，
需通过 `TextureSample` 内建函数从 `Graph` 中调用它，或改为声明一个
[`TextureSampleParameter2D`](../../parameters/parameter-nodes.md)。

<a id="texture-default-grammar"></a>

### 纹理默认值文法

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("/Game/Textures/T_X");
Texture2D C = "/Game/Textures/T_X";            // bare quoted absolute path (since 1.5.0)
```

完整文法以及所有根拼写见 [Path(…)](path.md)。默认值在构建材质时解析：一个无法解析的引用会
产生 [`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271)，其消息引用了解析器自身的代码；而一个
解析后无法加载任何内容的引用会产生 [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218)。`const`
纹理的默认值则改为写入其 `TextureObject` 节点的 `Texture` 属性，此时一个
无法加载的资产会被写入为空，且没有任何诊断信息——参见
[Metadata](../../parameters/metadata.md#object-properties)。

<a id="defaults-when-no-asset-is-assigned"></a>

### 未分配资产时的默认值

| 声明的纹理类型 | 回退资产 |
| :-- | :-- |
| `Texture2D` | 节点自身的默认值，`/Engine/EngineResources/DefaultTexture` |
| `TextureCube` | `/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube` |
| `VolumeTexture` | `/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture` |
| `Texture2DArray` | **不存在** |

> [!WARNING]
> `Texture2DArray` 没有引擎回退。一个声明为 `Texture2DArray` 但没有 `= Path(…)` 的属性会保留
> 节点的 2D 默认纹理，而它不是数组，编译器不会给出任何提示 *（自 2.0.0 起；1.x
> 会拒绝该声明）*。请显式分配一个数组资产。

<a id="dimension-validation"></a>

### 维度校验

*（自 2.0.0 起）* 默认值加载的资产**不会**根据声明的维度进行校验。会被校验的，
在任何东西加载之前，是内容浏览器引用在其 `Class'…'`
外壳中指定的类：另一维度的纹理类会产生 [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)，
而 DreamShader 知道但不是纹理的类（`MaterialFunction`、`CurveLinearColor`、`Font`……）会产生
[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)。参见 [Path(…)](../../parameters/path.md#class-checking)。

资产被分配后，除非声明中指定 `SamplerType`，否则会运行 `AutoSetSampleType()`，因此
采样器类型会跟随该资产——参见 [SamplerType](../../parameters/sampler-type.md)。

> [!NOTE]
> 纹理采样标记会获得相同的外壳检查，而 `TextureObjectParameter` 只会获得其
> “非纹理”那一半检查。参见
> [Parameter node tokens](../../parameters/parameter-nodes.md#dimension-validation-asymmetry)。

<a id="tokens-that-are-not-valid-in-properties"></a>

## 在 `Properties` 中无效的标记

这些是在其他上下文中真实存在的 DreamShaderLang 类型标记。在 `Properties` 声明中，其中
每一个都会产生 [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)，除了 `StaticBool`。

| 标记 | 实际上在何处有效 |
| :-- | :-- |
| `MaterialAttributes` | 材质函数的 `Inputs` / `Outputs` / `Results`；`Shader` `Outputs` 声明；`Function` 签名 |
| `Substrate` | 同上，且仅在 UE 5.4 或更新版本中 |
| `SamplerState` | `Inputs` / `Outputs` / 函数签名，在这些位置它是 `Texture2D` 的别名 |
| `StaticBool` | 材质函数的 `Inputs` / `Outputs`。在 `Properties` 中它会被接受 *（自 2.0.0 起）* 并读作 `bool`，这会产生一个 `ScalarParameter`，而不是静态布尔值：请写 `StaticBoolParameter` |
| `mat2` | 仅在 `Function` 签名或 `Code` 体中，解析器会将其规范化为 `float2x2`；随后生成阶段会拒绝矩阵类型 |
| `mat3` | 同上，规范化为 `float3x3` |
| `mat4` | 同上，规范化为 `float4x4` |

`Inputs` / `Outputs` 标记集记录在
[Inputs / Outputs / Results](../language/inputs-outputs.md) 中，完整的跨上下文矩阵记录在
[Type tokens](../language/types.md) 中。

<a id="notes"></a>

## 说明

- `vec*`、`ivec*`、`uvec*` 和 `bvec*` 只是 GLSL 风格的拼写；它们的行为分别与
  `float*`、`int*`、`uint*` 和 `bool*` 完全相同。
- 不存在针对 1 分量*向量*的紧凑标记：`float1` 和 `half1` 是标量标记。
- 紧凑纹理属性在 `Graph` 中是一个纹理值，从其节点的输出 0 读取。
- 分量数不是 2 或 3 的 `const` 向量会使用 `Constant4Vector` 分支；由于
  所有被接受的向量标记都是 2、3 或 4 分量，该分支只会被 4 分量触发。
- `const` 属性只保留常量节点所拥有的内容 *（自 2.0.0 起）*。`Description` 和
  `SamplerType` 会到达 `const` 纹理的 `TextureObject` 节点。在 `const` 标量或向量上，
  描述会被丢弃，而 `Group`、`SortPriority` 和 `Slider`——无论
  是显式写出，还是从 `Group(…)` 作用域继承——都会产生 [`DSH7224`](../diagnostics/DSH7xxx.md#dsh7224) 警告。
- 从 `Graph` 读取的 `const` **向量**是其常量节点的整个值，而不是命名的
  `R` / `RG` / `RGB` 分量输出。参见 [Using parameters in Graph](../../parameters/graph-usage.md#value-reads)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3252` | 该标记不是紧凑标记之一，不是 `*Parameter` 标记，且不以 `UE.` 开头 |
| `DSH3254` | 标量默认值既不是数字也不是 `true` / `false`；向量默认值没有带括号的分量列表，或含有既不是 |
| `DSH2105` | 数字与字母粘连（`1abc`） |
| `DSH1043` / `DSH1044` | `Class'…'` 默认值指定的类不是纹理 / 是另一维度的纹理 |
| `DSH8271` | 纹理默认值无法解析为对象路径 |
| `DSH8218` | 纹理默认值解析成功但无法加载 |
| [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) | 在不可能是常量的 `*Parameter` 标记上使用了 `const` |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建节点（内部） |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_CompactTypes")
{
    Properties = {
        const float Gamma = 2.2;                       // Constant

        float  Strength = 1.0;                         // ScalarParameter, reads R
        float2 Tiling   = float2(4.0, 4.0);            // VectorParameter, reads RG
        vec3   Tint     = vec3(1.0, 0.5, 0.2);         // VectorParameter, reads RGB
        vec4   Overlay  = vec4(0.0, 0.0, 0.0, 1.0);    // VectorParameter, reads RGBA

        // A texture *object* parameter: it carries the asset, it does not sample it.
        Texture2D BaseTex = Path(Game, "Textures/T_White");
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        vec2 UV = UE.TexCoord(Index = 0) * Tiling;
        Color = Tint * Strength * Gamma + Overlay.rgb + vec3(UV.x, UV.y, 0.0);
    }
}
```

生成的节点：

```text
Constant(2.2)                        Gamma
ScalarParameter                      Strength    DefaultValue = 1.0
VectorParameter                      Tiling      DefaultValue = (4, 4, 0, 1)     read through RG
VectorParameter                      Tint        DefaultValue = (1, 0.5, 0.2, 1) read through RGB
VectorParameter                      Overlay     DefaultValue = (0, 0, 0, 1)     read through RGBA
TextureCoordinate                    UV          CoordinateIndex = 0
```

`BaseTex` 在此不会生成任何内容：一个无人读取的属性不会产生节点，构建过程会说明这一点
（[`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390)，信息）。纹理对象参数必须
被某物消费——一个采样节点，或
[`ShaderFunction`](../language/shader-function.md) 的 `Texture2D` 输入。要声明一个会采样自身的纹理，请使用
[`TextureSampleParameter2D`](../../parameters/parameter-nodes.md)。

<a id="see-also"></a>

## 另请参阅

- [参数](../../parameters/index.md) — 中心节点与决策表
- [参数节点标记](../../parameters/parameter-nodes.md) — 22 个显式 `*Parameter` 标记
- [元数据块](metadata.md) — `[ … ]` 条目、`Slider(min, max)`、反射属性
- [Path(…)](path.md) — 纹理默认资产引用
- [SamplerType](../../parameters/sampler-type.md) — 采样器类型推断与覆盖
- [在图表中使用参数](../../parameters/graph-usage.md) — 声明的组件计数读取哪个节点输出
- [属性（章节）](../language/properties.md) — 所属章节的语法
- [类型标记](../language/types.md) — 完整的跨上下文标记矩阵
- [构造函数](../graph/constructors.md) — 这些字面量所类似的一侧构造函数形式 `Graph`
- [转换](../graph/conversions.md) — `Graph` 中的组件计数规则
