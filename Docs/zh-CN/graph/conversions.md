---
title: "转换"
description: "转换"
---

<a id="conversions"></a>

# 转换

> [DreamShader](../index.md) » [Graph](index.md) » **转换**

将 `Graph` 值适配到其所处位置的类型的规则，以及它们消耗多少节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 适用于 `Graph { … }` 函数体内的每个有类型位置，以及 `Outputs` 绑定 |
| 种类 | 隐式转换规则 |
| 生成 | 数字种类变化或标量广播时**不生成任何内容**；当 1.x 源将较宽的值截断为较窄值时生成一个 `UMaterialExpressionComponentMask`；而当类型已经匹配时不生成任何内容 |

*(自 2.0.0 起)* 1.x 的 `Graph` 函数体由 2.0 绑定器绑定，且每个值都有一个**类型**：1 到 4 个分量的数字，属于同一种类（`float`、`half`、`double`、`int`、`uint`、`bool`），一种纹理类型的纹理，`SamplerState`、`MaterialAttributes` 或 `Substrate`。转换就是绑定器将一个值的类型与它所处位置的类型进行比较。1.x 源可以做而 `.dss` 不可以做的事——将较宽的值截断为较窄值——是遗留规则 L22，并且它对此加以说明。

<a id="synopsis"></a>

## 概要

转换不是由作者编写的；它在值遇到有类型位置时自动应用。

```text
convert( <value>, <place type> ) -> <value> | <error>

<type> := MaterialAttributes
        | Substrate
        | SamplerState
        | Texture( { Texture2D | TextureCube | Texture2DArray | Texture3D | VolumeTexture } )
        | Number( { float | half | double | int | uint | bool }, { 1 | 2 | 3 | 4 } )
```

<a id="where-conversion-applies"></a>

## 转换适用的位置

| # | 位置 | 类型取自 | 没有任何内容适配时的代码 |
| :-- | :-- | :-- | :-- |
| 1 | 带初始化器的声明 —— `float3 c = A;` | 声明的类型 | [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) |
| 2 | 对 Graph 变量的赋值 —— `c = A;` | 变量声明的类型 | `DSH4228` |
| 3 | 对匹配 `Outputs` 声明的名称的赋值 —— `Color = A;` | 输出声明的类型 | `DSH4228` |
| 4 | `MaterialAttributes` 成员写入 —— `Attrs.BaseColor = A;` | 属性自身的类型 | `DSH4228` |
| 5 | 函数 / `GraphFunction` / `ShaderFunction` / `VirtualFunction` 输入参数 | 输入声明的类型 | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 6 | `UE.*` / `Substrate.*` 引脚，纹理采样的坐标 | 引擎目录中该引脚的类型 | [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) |
| 7 | `if` 条件 | 一个真或假的值 | [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260) |
| 8 | `Shader` 的 `Outputs` 绑定 —— `Base.EmissiveColor = Color;` | 材质属性的类型 | `DSH4228`，且绝不截断（见 [窄化](#narrowing)） |
| 9 | `+ - * /` 的操作数 | 最宽的操作数：标量会扩展到它，其他内容不改变宽度 | `DSH4226` |

在任何情况下，对未声明任何内容的名称的赋值都会以值自身的类型来声明它（遗留规则 L26，信息 [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292)）；没有什么需要转换。

一个在两个分支中都对变量赋值的 `if` 会合并两个变量声明类型的值，因此合并本身不转换任何内容 *(自 2.0.0 起)*；见 [`if` / `else`](if.md)。

<a id="rule-order"></a>

## 规则顺序

规则按此确切顺序进行测试；第一个匹配的规则决定结果。

| # | 值 → 位置 | 行为 | 节点 |
| :-- | :-- | :-- | :-- |
| 1 | 相同的类型 | **原样**通过 | 无 |
| 2 | 相同宽度但另一种类的数字（`int` → `float`，`float` → `bool`，……） | 种类变化；图中的值不变 | 无 |
| 3 | **从标量拓宽** —— 标量进入较宽的数字 | 广播：标量被连接到值被读取的位置，材质节点会复制它 *(自 2.0.0 起)* | 无；当标量为常量时有一个 `ConstantNVector` |
| 4 | 较窄的向量进入节点引脚（2 进入 `float3` 引脚，……） | 按原样采用：节点如何处理较少的分量是节点自己的事 | 无 |
| 5 | **窄化**，在 1.x 源中 —— 较宽的向量进入较窄的位置 | 其前导分量（`r`、`rg`、`rgb`），由信息 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) 说明 *(自 2.0.0 起)* | 一个 `ComponentMask`，或源节点恰好为这些通道拥有的输出引脚；在节点引脚处则无 |
| 6 | 具有多个输出的节点进入数字 | 当该输出的类型适配时，使用其默认输出；在 1.x 源中，目录未给其第一个输出指定宽度的节点会被读取为该输出，且在节点引脚处任何节点都是（信息 [`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287)；*2.0.0 – 2.1.0 拒绝第二种*）；否则 [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | 无 |
| 7 | 其他任何情况 | 以该位置的代码拒绝 | — |

规则 7 涵盖了拓宽**非**标量的值——输入 2 进入 3 或 4，输入 3 进入 4，在节点引脚以外的任何地方——以及每种类型族的更改。

<a id="numeric-conversion-matrix"></a>

### 数字转换矩阵

在 1.x 源中，输入宽度在侧面，预期宽度在顶部。在节点引脚处，矩阵中的两个错误被接受（规则 4），并且截断不生成节点。

| 输入 \ 预期 | 1 | 2 | 3 | 4 |
| :-- | :-- | :-- | :-- | :-- |
| **1** | 不变 | 广播，无节点 | 广播，无节点 | 广播，无节点 |
| **2** | `.r`，`DSH5289` | 不变 | **错误** | **错误** |
| **3** | `.r`，`DSH5289` | `.rg`，`DSH5289` | 不变 | **错误** |
| **4** | `.r`，`DSH5289` | `.rg`，`DSH5289` | `.rgb`，`DSH5289` | 不变 |

<a id="kind-conversion-matrix"></a>

### 种类转换矩阵

“错误”是该位置的代码（见第一个表格）。

| 预期 \ 输入 | 数字 | MaterialAttributes | Substrate | 纹理 |
| :-- | :-- | :-- | :-- | :-- |
| **MaterialAttributes** | 错误 | 通过 | 错误 | 错误 |
| **Substrate** | 错误 | 错误 | 通过 | 错误 |
| **Texture(T)** | 错误 | 错误 | 错误 | 若纹理类型为 `T` 则通过；接受任何纹理的引脚接受任何纹理 |
| **Number(N)** | 上方的数字矩阵 | 错误 | 错误 | 错误 |

纹理只转换为其自身的纹理类型：`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D` 和 `VolumeTexture` 是五种类型，而 `SamplerState` 是其自身的类型，不是纹理 *(自 2.0.0 起)*。`TextureCube` 永远不会转换为 `Texture2D`。

<a id="narrowing"></a>

## 窄化

在 1.x 源中，比其位置更宽的值会保留其前导分量（遗留规则 L22）。这种截断不再静默 *(自 2.0.0 起)*：每一次都是信息 `DSH5289`，因此源上的 `dsc check` 会列出它们，而 `dsc migrate` 会写出 swizzle。它消耗 swizzle 所消耗的内容：一个 `ComponentMask`，除非源节点恰好为这些前导通道拥有一个输出引脚——见 [Swizzle](swizzle.md)。

```c
vec4  Src   = vec4(0.1, 0.2, 0.3, 0.4);
float3 Rgb  = Src;      // Src.rgb — DSH5289; channel A is dropped
float  R    = Src;      // Src.r   — DSH5289
```

显式写出 swizzle（`float3 Rgb = Src.rgb;`）会记录意图，构建相同的节点，并使该信息消失。

窄化**仅**适用于上述位置 1–7。它被有意地**不**应用于：

| 上下文 | 取而代之的行为 |
| :-- | :-- |
| 二元运算符操作数 | 拒绝 —— `DSH4226` |
| 构造函数参数 | 拒绝 —— [`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222) |
| Swizzle 边界 | 拒绝 —— [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230)：swizzle 绝不能超过基础宽度 |
| `Shader` 的 `Outputs` 绑定（`Base.Opacity = Color4;`） | 拒绝 —— `DSH4228`，这是 1.x 也拒绝截断的唯一位置 |
| `.dss` 源 | 以该位置的代码拒绝 |

<a id="widening"></a>

## 拓宽

**唯一**的拓宽规则是标量广播。一个单分量值无需节点即可满足更宽的位置：
该标量本身会在读取该值的任何位置被连接，它所到达的材质节点会
对其进行复制 *（自 2.0.0 起；1.x 会构建一条 `AppendVector` 链）*。常量标量会被折叠进
`ConstantNVector`。

```c
float  K   = 0.5;
vec3   All = K;         // no node: K is connected wherever All is read
```

**没有**零填充，也**没有**部分拓宽。`float3 v = SomeFloat2;` 就是 `DSH4228`。使用
构造函数来说明额外通道包含什么：`float3 v = float3(SomeFloat2, 0.0);`。唯一
接受更窄向量的地方是节点的引脚（规则 4）。

<a id="float-int-and-bool"></a>

## Float、int 和 bool

编译器知道每个数字的种类 *（自 2.0.0 起）*；生成的图却不知道，因为
材质图中的每个值都是 float。因此种类的变化不会对图产生任何改变。

| 标记族 | 对转换而言意味着什么 |
| :-- | :-- |
| `float`、`float1..4`、`half`、`half1..4`、`vec2..4` | 种类为 `float` 或 `half` 的 1 / 2 / 3 / 4 个分量 |
| `int`、`int2..4`、`ivec2..4` | 种类为 `int` 的同样的 1 / 2 / 3 / 4 个分量 — 不截断，不舍入 |
| `uint`、`uint2..4`、`uvec2..4` | 种类为 `uint` — 不做范围钳制，不做符号处理 |
| `bool`、`bool2..4`、`bvec2..4` | 种类为 `bool` — 不归一化到 0/1 |
| `StaticBool`、`StaticBoolParameter` | 作为 `bool` 读取，1 个分量 |

后果：

- `int x = 7.9;` 存储 7.9。如果想要截断，请使用 `floor(…)`。
- `bool b = 0.5;` 存储 0.5。不存在到 0 或 1 的转换。
- 显式图转换遵循同样的规则：`(int)0.5` 和 `(bool)0.5` 都保留 0.5，
  包括当常量表达式由绑定器求值时。使用 `trunc(...)` 丢弃
  小数部分，或使用 `value != 0` 生成 0/1。`@custom` 主体内部的转换是 HLSL 转换。
- 将 `float4` 赋值给 `int3` 会像 `float4` → `float3`（`DSH5289`）一样收窄。
- 种类在一个地方有影响：两个整数
  [构造函数](constructors.md#integer-constructors)调用之间的 `/` 是
  [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)。如果任一侧是整数字面量（`7`）或 `int` 变量，
  则与 1.x 一样是浮点除法。参见
  [整数除法](expressions.md#integer-division)。

<a id="authoritative-component-counts"></a>

## 权威分量数

*（自 2.0.0 起）* 不存在。1.x 允许某些值——常量折叠的构造函数、`dot`、
已知宽度表的 `UE.*` 内置项——携带一个“权威”宽度，该宽度会覆盖声明
并引导运算符。现在每个值都具有其类型的宽度：

| 值 | 宽度 |
| :-- | :-- |
| 变量 | 其声明时所用的宽度 — 始终被遵循 |
| 字面量 | 1 |
| 构造函数，无论是否折叠 | 其类型的宽度 |
| 内置项 | 其操作数中最宽的，或该内置项返回的宽度（`dot`、`length`、`distance`：1） |
| `UE.*` / `Substrate.*` 节点输出 | 引擎目录赋予该输出的宽度；引擎未给出宽度的输出遵循该节点最宽的输入，或者，当没有比标量更宽的输入时，遵循该值所去往的位置 |
| Swizzle | 其通道数量 |

因此 `float2 dir = UE.CameraVectorWS();` 是一个 `float2`——前两个分量 `DSH5289`——而
1.x 存储三个。两个不同宽度的操作数只能通过标量广播相遇；其他任何情况都是
`DSH4226`（位置 9），并且运算符从不自行拓宽或收窄操作数。

<a id="notes"></a>

## 说明

- 转换绝不会把数字变成 `MaterialAttributes`、纹理或 `Substrate` 值，也不会把一种
  纹理类型变成另一种。
- 声明纹理、`SamplerState` 或 `Substrate` 变量而不提供初始值设定项是
  [`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215)，因为没有可用的默认值来起始。
  标量和向量从零开始。参见[声明](declarations.md)。
- `Outputs` 声明是其声明类型的普通变量：对它的每次赋值都在
  写入处检查，而不是在图构建完成后检查一次。
- 一个 `MaterialAttributes` 值到达不承载它的引脚或函数输入时，会在图构建时
  成为 [`DSH4371`](../diagnostics/DSH4xxx.md#dsh4371)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH4228` | 值不适合它所存储到的变量、输出或属性（位置 1–4 和 8）。 |
| `DSH4226` | 参数或操作数不适合（位置 5 和 9）。 |
| `DSH5214` | 值不适合节点的引脚（位置 6）。 |
| `DSH4260` | 条件不是一个真或假的值（位置 7）。 |
| `DSH5289` | *（信息）* 更宽的值被截取为其前导分量（规则 5）。 |
| `DSH5287` | *（信息）* 具有多个输出的节点被读取为其第一个输出（规则 6）。 |
| `DSH5201` | 具有多个输出的节点在未指定某一个的情况下被用作值，且其默认输出不适合。 |
| `DSH5292` | *（信息）* 赋值声明了一个未在任何地方声明的名称。 |
| `DSH2215` | 纹理、采样器或 `Substrate` 变量没有初始值设定项。 |
| `DSH4371` | 一个 `MaterialAttributes` 值到达不承载它的引脚或输入。 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Conversions", Root="Game")
{
    Properties {
        VectorParameter Tint = float4(1.0, 2.0, 3.0, 999.0);
        ScalarParameter K    = 0.5;
    }

    Settings { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs {
        float3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        float3 dir    = UE.CameraVectorWS();  // a float3 output
        float3 tinted = Tint;                 // narrowing: Tint.rgb, info DSH5289
        float3 lit    = K;                    // scalar broadcast: no node
        Color = dir * tinted + lit;
    }
}
```

将最后一条语句替换为 `Color = dir * Tint;` 会失败并报 `DSH4226`：`float4` 参数
使乘积为四个分量宽，而运算符不会拓宽 `float3` 操作数来
匹配。

<a id="see-also"></a>

## 另请参阅

- [表达式](expressions.md) — 运算符操作数规则和整数除法检查
- [构造函数](constructors.md) — 改变值宽度的显式方式
- [Swizzle](swizzle.md) — 收窄的显式方式，以及收窄所付出的节点代价
- [声明](declarations.md) — 声明类型标记、默认值、重复声明
- [`if` / `else`](if.md) — 分支合并及其要求的类型
- [MaterialAttributes](material-attributes.md) — 逐属性类型
- [调用](calls.md) — 针对已声明输入类型的参数转换
- [类型](../language/types.md) — 完整的类型标记目录
- [`UE.*` 内置项](../builtins/ue.md) — 内置项的输出类型
- [`dsc migrate`](../tools/migrate.md) — 规则 L22 及其写入的 swizzle
- [诊断索引](../diagnostics/index.md) — 按流水线阶段列出的每个代码
