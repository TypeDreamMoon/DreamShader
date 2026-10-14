---
title: "类型 token"
description: "类型 token"
---

<a id="type-tokens"></a>

# 类型 token

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **类型 token**

在声明中命名值的形状的封闭标识符集合，以及决定某个声明位置接受其中哪些标识符的按上下文规则。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh` —— 任何声明合法之处 |
| 种类 | 词法类别 |
| 匹配 | 在每个上下文中都**大小写不敏感** |

*(自 2.0.0 起)* 每个类型 token 都过同一张表：1.x 拼法读作对应的 2.0 类型
（`vec3` 和 `Float3` 读作 `float3`，`MaterialAttributes` 读作 `material`，`StaticBool` 读作 `bool`），
不给诊断；不是类型的单词原样保留，由绑定器拒绝
（[`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201)）—— 在 `Properties` 里前端先拒绝它
（[`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)）。2.0 的类型名见
[DreamShaderLang 2.0](../language-v2/index.md#declarations)。

<a id="synopsis"></a>

## 概要

类型 token 是语言里每一种声明形式的第一个元素：

```c
// Properties section
[const] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// Inputs / Outputs / Results of a material function
[opt] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// Shader Outputs variable declaration
<type> <name> [ = <expression> ] ;

// Function / GraphFunction signature
Function [ <type> ] <name> ( [ { in | out } ] <type> <name> , … ) { … }

// Graph declaration
<type> <name> [ = { <expression> | { <brace-initializer> } } ] ;
```

<a id="contexts"></a>

## 上下文

六个声明位置**并不**共用同一个类型集合。每一行是[有效性矩阵](#validity-matrix)的一列。

| 列 | 位置 | 参考 |
| :-- | :-- | :-- |
| `Prop` | `Shader` / `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` 的 `Properties` | [Properties](properties.md) |
| `I/O` | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` 的 `Inputs` / `Outputs` / `Results`（以及在 `VirtualFunction` 内部的 `Properties`，它在那里是 `Inputs` 的别名） | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Fn in` | `Function` / `GraphFunction` 的 `in` 参数 | [Function](function.md)、[GraphFunction](graph-function.md) |
| `Fn out` | `Function` / `GraphFunction` 的 `out` 参数，或声明的返回类型 | [Function](function.md) |
| `Out decl` | `Shader` 的 `Outputs` 区段内部的变量声明 | [Output bindings](output-bindings.md) |
| `Graph` | `Graph` 块内部的声明语句 | [Graph declarations](../graph/declarations.md) |

> [!NOTE]
> *(自 2.0.0 起)* `Prop` 由前端在读区段时检查。其他各列由绑定器对整个文件检查，
> 并由降级到图的那一步在某个类型的值会变成引脚的地方检查 —— 没有引脚能承载的类型在那里被拒绝，
> 就在需要那个引脚的声明或调用处。

<a id="validity-matrix"></a>

## 有效性矩阵

`✔` 接受 · `✘` 拒绝。单元格里写的是拒绝时抛出的诊断码。

| Token | `Prop` | `I/O` | `Fn in` | `Fn out` | `Out decl` | `Graph` |
| :-- | :-: | :-: | :-: | :-: | :-: | :-: |
| 标量和向量：`float`、`half`、`int`、`uint`、`bool` 家族及其 GLSL 拼法 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | ✔ | ✔ ⁽¹⁾ | ✔ ⁽²⁾ | ✘ `DSH6254` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `MaterialAttributes` | ✘ `DSH3252` | ✔ | ✘ `DSH6210` | ✔ | ✔ | ✔ |
| `Substrate` | ✘ `DSH3252` | ✔ ⁽⁵⁾ | ✘ `DSH6253` | ✘ `DSH6253` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `StaticBool` | ✔ ⁽⁶⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `StaticBoolParameter` | ✔ ⁽⁹⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `mat2` `mat3` `mat4`、`float2x2` … `float4x4` | ✘ `DSH3252` | ✘ | ✘ | ✘ | ✘ | ✘ —— 见 [矩阵](#matrices) |

1. `Texture3D` 输入是一个 `VolumeTexture` 引脚。
2. `Function` 的纹理类型 `in` 参数在生成的 HLSL 里还会配一个 `SamplerState <Name>Sampler`
   参数，也就是引擎在 `Custom` 节点的每个纹理输入旁声明的那一个。见 [Function](function.md)。
3. 只能是变量：没有 `Base.` 引脚接受纹理，而 `Substrate` 只接到
   `Base.FrontMaterial`。它没有零值，所以 `Graph` 必须在读取前给它赋值。
4. `Graph` 块里的纹理、`SamplerState` 和 `Substrate` 声明**必须**带
   初始化式 —— 它们没有默认值：
   [`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215)。
5. `Substrate` 输入是函数的 Substrate 引脚；Substrate 值来自 `Substrate.*`
   节点，引擎从 UE 5.4 起才有它。
6. *(自 2.0.0 起)* 读作 `bool`，所以 `Properties` 里的 `StaticBool X = true;` 声明的是**动态**
   布尔参数，而不是静态的；1.x 在那里拒绝这个 token。静态开关参数请写 `StaticBoolParameter`。
7. 这两个 token 的输入都是函数的 `StaticBool` 引脚。
8. 读作 `bool`。
9. 在 `Properties` 里，`StaticBoolParameter` 是**参数节点 token**，不是类型 token：它
   生成一个 `UMaterialExpressionStaticBoolParameter`，默认值只接受 `true` / `false`
   （[`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254)）。见
   [Parameter nodes](../../parameters/parameter-nodes.md)。

*(自 2.0.0 起)* `SamplerState` 是独立的类型；1.x 在接受它的地方都把它读成 `Texture2D`。
它在 `Properties` 里被拒绝（`DSH3252`），作为材质函数输入被拒绝
（[`DSH4364`](../diagnostics/DSH4xxx.md#dsh4364)），作为 `Function` 结果被拒绝（`DSH6254`），而一个 `Graph`
的这种声明需要初始化式（`DSH2215`）。

<a id="counts-at-a-glance"></a>

### 数量一览

| 家族 | Token | 数量 |
| :-- | :-- | --: |
| 标量（1 分量） | `float` `half` `double` `int` `uint` `bool`，以及各自带 `1` 后缀的写法（`float1` …） | 12 |
| 向量（2 / 3 / 4 分量） | `float2..4` `half2..4` `double2..4` `int2..4` `uint2..4` `bool2..4` `vec2..4` `ivec2..4` `uvec2..4` `bvec2..4` | 30 |
| 纹理 | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | 5 |
| 不透明 / 其他 | `SamplerState` `MaterialAttributes` `Substrate` `StaticBool` `StaticBoolParameter` | 5 |

> [!NOTE]
> 没有 `vec1`、`ivec1`、`uvec1` 或 `bvec1`，也没有 `half` 的 GLSL 向量拼法。
> 单分量的 GLSL 风格写法根本不存在；请用 `float` 或 `float1`。

<a id="glsl-aliases"></a>

## GLSL 别名

`vec` / `ivec` / `uvec` / `bvec` 拼法在其对应的
`float` / `int` / `uint` / `bool` 拼法被接受的每个位置都是一等 token —— 在 `Properties`、`Inputs`、`Outputs`、`Graph`
声明和 `Graph` 构造器里都是 —— 并读作那个拼法。

| GLSL 拼法 | 等价于 |
| :-- | :-- |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` |

<a id="glsl-identifier-rewrites-in-function-bodies"></a>

### `Function` 函数体中的 GLSL 标识符改写

`Function` / `GraphFunction` 的**签名**按上面的表读每个类型 token，而**函数体**则会把每个标识符过同一张映射表的超集。两者都按小写化的整个标识符匹配，因此改写是大小写不敏感的。

| 被改写的标识符 | 变成 | 签名类型中 | 函数体文本中 |
| :-- | :-- | :-: | :-: |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` | ✔ | ✔ |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` | ✔ | ✔ |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` | ✔ | ✔ |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` | ✔ | ✔ |
| `mat2` | `float2x2` | ✔ | ✔ |
| `mat3` | `float3x3` | ✔ | ✔ |
| `mat4` | `float4x4` | ✔ | ✔ |
| `mix` | `lerp` | ✘ | ✔ |
| `fract` | `frac` | ✘ | ✔ |
| `mod` | `fmod` | ✘ | ✔ |

签名 15 个别名，函数体 18 个。改写感知注释和字符串。它**只**作用于 `Function` 和 `GraphFunction` 的函数体 —— `Graph` 块、`Shader` 和 `ShaderFunction` 的函数体都**不**改写。在 `Graph` 块中，`mix`、`fract` 和 `mod` 调用 `lerp`、`frac` 和 `fmod`，并给出警告 [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) *（自 2.0.0 起）*（旧规则 L2）。见 [数学内置](../builtins/math.md)。

> [!WARNING]
> 函数体改写按整个标识符匹配，忽略大小写。`Function` 或 `GraphFunction` 函数体里名为 `Mix`、`Mod`、`Fract`、`Vec3`、`Mat4` … 的 helper、局部变量或结构体成员会被静默改名成 `lerp`、`fmod`、`frac`、`float3`、`float4x4`。没有任何诊断；失败表现为 HLSL 编译错误，或者悄悄算错。请改名。

<a id="matrices"></a>

## 矩阵

**图里没有矩阵值。** `mat2` / `mat3` / `mat4` 和 `float2x2` / `float3x3` / `float4x4` 在 `Properties` 里被拒绝（`DSH3252`），在其他任何地方，一个矩阵类型的值要变成节点或引脚时就是 [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361)。

不过 `mat2` / `mat3` / `mat4` 在 `Function` 签名里*词法上*是被接受的，读成 `float2x2` … `float4x4`。于是声明能解析通过，调用时才失败：

```c
Function float3 Rotate(in mat3 basis, in vec3 v) { return mul(basis, v); }
// parses; a Graph call that passes a matrix to 'basis' is DSH4361
```

矩阵形状的运算请用 transform 内置 —— 见 [UE.TransformVector / UE.TransformPosition](../builtins/transform.md) —— 或者整段写在 HLSL 函数体里。

<a id="whitespace-inside-a-token"></a>

## Token 内部的空白

*（自 2.0.0 起）* 类型 token 是一个单词。1.x 在部分上下文里会去掉 token 内部的空格，于是 `float 3` 读成 `float3`、`Material Attributes` 读成 `MaterialAttributes`；现在它们是两个单词，这条声明解析不了。

<a id="generated-hlsl-spelling"></a>

## 生成的 HLSL 拼写

当 `Function` 降级成生成的 `.ush` helper 时，每个声明的类型按 2.0 拼法写进 HLSL 签名：

| 声明的 token | 产出的 HLSL |
| :-- | :-- |
| `VolumeTexture` | `Texture3D` |
| `MaterialAttributes` | `FMaterialAttributes` —— 只能作为结果；作为输入是 `DSH6210` |
| `StaticBool`、`StaticBoolParameter` | `bool` *（自 2.0.0 起）* |
| GLSL 拼法或其他大小写（`vec3`、`Float3`） | 2.0 拼法（`float3`） |
| 其他一切 | 原样输出 |

*（自 2.0.0 起）* 1.x 把 `MaterialAttributes`、`StaticBool` 和 `StaticBoolParameter` 原样写进签名，产出 Unreal 编译不了的 helper。

<a id="component-count-derivation"></a>

## 分量数推导

token 的宽度就是它的数字后缀：`…2` 到 `…4` 是那么多分量的向量，`…1` 或没有数字是标量 —— `float1` 是 HLSL 里标量的拼法 —— `…NxM` 是矩阵。

<a id="removed"></a>

## 已移除

| Token | 状态 | 替代 |
| :-- | :-- | :-- |
| `Scalar` | **已移除** | `float` |
| `Color` | **已移除** | `float4` / `vec4` |
| `Vector` | **已移除** | `float2` … `float4` / `vec2` … `vec4` |

这些名字没有在任何地方被特殊处理；它们和其他不是类型的单词一样被拒绝 —— `Properties` 块里是 `DSH3252`，其他地方是 `DSH4201`。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3252` | token 在 `Properties` 中不被接受 |
| [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) | 一个没有 2.0 形式的 `Properties` 参数节点 token |
| `DSH4201` | `Properties` 之外类型位置上的单词不是类型 |
| `DSH2215` | `Graph` 块里的纹理、`SamplerState` 或 `Substrate` 声明没有初始化式 |
| `DSH4361` | 一个矩阵值要变成节点或引脚 |
| `DSH4364` | 材质函数输入用了没有函数引脚能承载的类型，比如 `SamplerState` |
| `DSH6210` | `Function` / `GraphFunction` 把 `MaterialAttributes` 当输入 |
| [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | `Function` / `GraphFunction` 取用或返回 `Substrate` |
| `DSH6254` | `Function` / `GraphFunction` 返回纹理或采样器 |
| [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) | 一种值赋到了声明为另一种的地方 —— 数字赋给 `MaterialAttributes` 或纹理变量，纹理赋给数字 |
| `DSH5277` | 警告：`Graph` 块里的 `mix`、`fract` 或 `mod` |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

每个家族各取一个 token，各自放在接受它的上下文里：

```c
ShaderFunction(Name="Functions/F_Types")
{
    Properties = {
        float     Strength = 1.0;              // scalar token
        vec3      Tint     = vec3(1, 1, 1);    // GLSL vector spelling
        Texture2D Noise    = Path(Game, "Textures/T_Noise");
    }

    Inputs = {
        vec2                   UV;            // numeric
        opt StaticBool         UseTint;       // a StaticBool pin
        opt MaterialAttributes InAttrs;       // a MaterialAttributes pin
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        Texture2D Src = Noise;                 // texture declaration needs an initializer
        vec4 Sampled  = SampleTexture2D(Src, UV);
        float3 Base   = Sampled.rgb * Strength;
        OutColor      = Base * Tint;
    }
}
```

```c
// Function signature tokens are read as their 2.0 spelling, then checked like any declaration.
Function float Luma(in vec3 color)          // vec3 -> float3 in the generated HLSL
{
    return dot(color, float3(0.299, 0.587, 0.114));
}
```

<a id="see-also"></a>

## 另见

- [Properties](properties.md) —— `Properties` section 的语法以及它接受哪些 token
- [Inputs / Outputs / Results](inputs-outputs.md) —— 材质函数的带类型参数 section
- [Output bindings](output-bindings.md) —— `Shader` `Outputs` 声明与 `Base.*` 目标
- [Function](function.md) —— 签名语法、`in` / `out`、返回类型、生成的 HLSL
- [GraphFunction](graph-function.md) —— `UE.*` 提升函数形式
- [Keywords](keywords.md) —— 完整的关键字索引和已废弃的拼写
- [Graph declarations](../graph/declarations.md) —— 在 `Graph` 块内声明值
- [Conversions](../graph/conversions.md) —— 强转、扩宽、静默收窄
- [Constructors](../graph/constructors.md) —— 构造器名称
- [MaterialAttributes](../graph/material-attributes.md) —— 读取和写入属性成员
- [Compact parameter types](../parameters/compact-types.md) —— 每个紧凑 token 生成哪种节点
- [Parameter nodes](../../parameters/parameter-nodes.md) —— 22 个显式 `*Parameter` token
- [OutputType / ResultType](../builtins/output-type.md) —— `UE.*` 调用接受的 token 集
- [Substrate builtins](../builtins/substrate.md) —— UE 5.4+ 的 `Substrate.*` 接口
- [DreamShaderLang 2.0](../language-v2/index.md) —— `.dss` 写出的类型名
- [Diagnostics index](../diagnostics/index.md) —— 每一个代码
