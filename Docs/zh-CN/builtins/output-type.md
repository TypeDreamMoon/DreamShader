---
title: "OutputType"
description: "OutputType"
---

<a id="outputtype"></a>

# OutputType

> [DreamShader](../index.md) » [内置组件](index.md) » **OutputType**

1.x 调用用它作为参数来说明反射内置组件产生何种类型的值。自 2.0.0 起，内置组件目录会为每个类说明这一点，而 `OutputType` 保留了两项职责：Custom 节点的输出类型，以及 1.x 源码中的宽度提示。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — 作为 `Graph` 中 `UE.*` 调用的参数，或 `UE.*` 属性声明的参数 |
| 类型 | 内置组件参数 |
| 别名 | `ResultType` |

<a id="synopsis"></a>

## 概要

```c
{ OutputType | ResultType } = <type-token>
```

旧版前端在查找 `OutputType` 和 `ResultType` 时忽略大小写，并在绑定器看到调用之前将它们从调用中取出——在 Custom 调用中，它会将其中第一个保留为节点的输出类型，并丢弃第二个。两者都不会作为引脚或属性被绑定。该标记可以带引号或裸写：`OutputType = "float3"` 与 `OutputType = float3` 完全相同。

它在任何地方都不是必需的 *(自 2.0.0 起；1.x 要求每个泛型调用以及其表之外的属性声明都必须提供它)*。`.dss` 没有这样的参数：反射节点以 `UE.<ClassShortName>(Pin = …, Property = …)` 方式调用，其值由目录确定类型。

<a id="where-it-is-read"></a>

## 读取位置

| 位置 | 标记 | 效果 |
| :-- | :-- | :-- |
| `Graph` 中的 `UE.Expression(Class = "Custom", …)` | 下面的 Custom 标记集合 | **节点的输出类型**——写入节点，决定 HLSL 返回类型 |
| `Graph` 中的任何其他 `UE.*` 调用 | 标量或向量类型拼写 | 在具有**一个数值输出**的节点上：该值的类型，即 1.x 赋予它的类型，节点整体接线。否则丢弃 |
| `UE.<Name>(…)` 属性声明 | 任意类型拼写 | 该声明所变成的局部变量的类型——参见 [声明的输出宽度](ue.md#declared-output-width) |
| `Graph` 中 [`UE.*` 目录](ue.md#catalogue) 的 27 个 1.x 名称之一 | — | 随 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) 一起丢弃，如同该名称不读取的任何参数 |
| `Substrate.*` | — | 无诊断信息地丢弃 |

不符合上述任何情况的标记——非 Custom 调用中的纹理、`MaterialAttributes`、`Substrate` 或 `SamplerState` 标记，矩阵，拼写错误——都会被无诊断信息地丢弃 *(自 2.0.0 起；1.x 会报告不支持的标记)*，并由目录确定值的类型。

> [!NOTE]
> 宽度提示遵循 1.x 的行为，即当目录会构建另一个图时：
> `float Cam = UE.Expression(Class = "CameraPositionWS", OutputType = "float")` 将 `Cam` 类型设为
> `float`，并在读取 `Cam` 的任何位置连接节点的整体输出，这正是 1.x 图
> 所为。在引擎以与提示相同方式确定类型的节点上，它不会改变任何东西。

<a id="accepted-tokens"></a>

## 接受的标记

<a id="on-a-custom-node"></a>

### 在 Custom 节点上

比较时忽略大小写并移除所有空格。其他任何内容都是 [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261)。

| 标记 | Custom 输出类型 |
| :-- | :-- |
| `float`、`float1`、`half`、`half1` | `Float1` |
| `float2`、`vec2`、`half2` | `Float2` |
| `float3`、`vec3`、`half3` | `Float3` |
| `float4`、`vec4`、`half4` | `Float4` |
| `MaterialAttributes` — 带引号 | `MaterialAttributes` |

*(自 2.0.0 起)* `int`、`uint`、`bool` 及其向量、`ivec*`、`uvec*`、`bvec*`、纹理标记、`StaticBool` 和 `Substrate` 在此处为 `DSH5261`。裸写的 `MaterialAttributes` 会被读取为类型 `material`，同样为 `DSH5261`；请为其加引号。

<a id="on-any-other-call-and-on-a-property-declaration"></a>

### 在任何其他调用上，以及在属性声明上

标记是 1.x 类型拼写，比较时忽略大小写：

| 标记 | 类型 | 图调用 | 属性声明 |
| :-- | :-- | :-: | :-: |
| `float`、`half`、`double`、`int`、`uint`、`bool`，以及各自带 `1` | 标量 | ✔ | ✔ |
| 同样内容带 `2`、`3`、`4`；`vec2`–`vec4`、`ivec2`–`ivec4`、`uvec2`–`uvec4`、`bvec2`–`bvec4` | 向量 | ✔ | ✔ |
| `StaticBool`、`StaticBoolParameter` | 读取为 `bool` | ✔ | ✔ |
| `Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` | 纹理 | 丢弃 | ✔ — 维度保留 |
| `MaterialAttributes` | `material` | 丢弃 | ✔ |
| `Substrate`、`SamplerState` | | 丢弃 | ✔ |
| `mat2`–`mat4`、`float3x3` 及其他矩阵 | 矩阵 | 丢弃 | 会读取，但没有节点的值适配矩阵局部变量 |

在 `Graph` 调用上，标记会被修剪但保留内部空格（`" float 4 "` 不是标记）；在属性声明上，会先移除所有空格。下划线和短横线从不移除：`float_4` 和 `Material-Attributes` 不是拼写。

<a id="output-mask-pseudo-names"></a>

## 输出掩码伪名称

与 `OutputType` 不同：`Output` / `OutputName` 参数选择多输出节点的**哪一个**输出被读取，并在调用上成为 `.Name`。该名称是目录为该类列出的输出之一。引擎未命名的输出，在类具有多个输出时，会列在其掩码的通道之下——`R`、`G`、`B`、`A`、`RG`、`RGB`、`RGBA`——除非该类已使用该名称。单个输出保留空名称，并作为调用的值。

| 节点 | 目录列出的输出（UE 5.8） |
| :-- | :-- |
| `VertexColor` | `RGB`、`R`、`G`、`B`、`A` |
| `TextureSample` | `RGB`、`R`、`G`、`B`、`A`、`RGBA` |
| `SceneColor` | `RGB`、`A` |
| `SceneTexture` | `Color`、`Size`、`InvSize` |

名称按精确方式比较；在 1.x 源码中，仅忽略大小写匹配的名称会被接受，并给出 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)。没有任何输出使用的名称是 [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) *(自 2.0.0 起；1.x 会按固定顺序对每个未命名输出测试七个掩码名称)*。在输出全都是同一个值的通道视图的节点上，一个输出内部的 swizzle 就是该输出（`UE.VertexColor().a`）。完整规则：[`UE.Expression`](ue-expression.md#selecting-an-output)。

> [!NOTE]
> 这些不是 `OutputType` 值，`OutputType` 标记永远不是有效的 `Output` 选择器。
> `UE.Expression(Class = "BreakMaterialAttributes", Output = "BaseColor")` 按输出*名称*选择；
> `UE.VertexColor(Output = "RGB")` 选择其掩码命名的输出。

<a id="notes"></a>

## 备注

- **宽度来自目录。** `UE.Expression(Class = "WorldPosition")` 是一个节点，其第一个输出 `XYZ` 是 `float3`；无需也不查阅任何 token。参见[结果类型与组件数量](ue-expression.md#result-type-and-component-count)。
- 仅因 `OutputType` 拼写方式不同而有所区别的两次调用——`float3` 与 `vec3`——是一个节点：IR 合并的是节点，而非拼写 *(自 2.0.0 起)*。参见[节点复用](../graph/node-reuse.md)。
- 此 token 集是 1.x 类型词汇表。`Properties` 部分还接受参数节点 token；参见[类型](../language/types.md)、[紧凑类型](../parameters/compact-types.md)和[输入/输出/结果](../language/inputs-outputs.md)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261) | Custom 节点的 `OutputType` 不属于上述 token 之一 |
| [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)（警告） | 在 27 个 1.x 名称之一上使用 `OutputType` / `ResultType`，已丢弃 |
| [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) | 属性声明的类型不接受节点的值——比节点产生的向量更窄 |
| [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | 一个 `Output` 名称并非该节点的输出 |

每个代码都在[诊断](../diagnostics/index.md)中其对应页面有描述。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_OutputTypes")
{
    Properties {
        // Declaration surface: the type of the local `Radius`.
        UE.Expression(Class = "ObjectRadius", OutputType = "float1") Radius;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // Not needed: the catalog gives TextureCoordinate a float2.
        float2 uv = UE.Expression(Class = "TextureCoordinate", OutputType = "float2");

        // ResultType is the alias.
        float t = UE.Expression(Class = "Time", ResultType = "float1");

        // The node's own output type on a Custom node.
        float3 tinted = UE.Expression(Class = "Custom", OutputType = "float3",
                                      Code = "return In * 0.5f;", In = vec3(uv.x, uv.y, t));

        // An output the catalog names after its mask.
        float3 vcol = UE.Expression(Class = "VertexColor", Output = "RGB");

        Color = tinted * vcol * Radius;
    }
}
```

<a id="see-also"></a>

## 另见

- [`UE.Expression`](ue-expression.md)——此参数所属的调用形式
- [`UE.*` 目录](ue.md)——那些 1.x 名称，会丢弃此参数
- [内置项](index.md)——调用界面
- [Substrate](substrate.md)——丢弃此参数之处
- [类型](../language/types.md)——该语言的声明类型 token
- [紧凑类型](../parameters/compact-types.md)——参数类型 token
- [输入/输出/结果](../language/inputs-outputs.md)——函数参数类型集
- [转换](../graph/conversions.md)——宽度以及值如何适配
- [节点复用](../graph/node-reuse.md)——相同的节点
- [诊断索引](../diagnostics/index.md)——每个代码
