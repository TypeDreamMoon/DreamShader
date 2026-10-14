---
title: "变换内置函数"
description: "变换内置函数"
---

<a id="transform-builtins"></a>

# 变换内置函数

> [DreamShader](../index.md) » [内置函数](index.md) » **变换内置函数**

两个改变值坐标基准的 `UE.*` 内置函数：`UE.TransformVector` 用于方向，
`UE.TransformPosition` 用于点。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 在 `Graph { … }` 体内、`Outputs` 绑定表达式内，或 `Outputs` 声明初始化器中 |
| 种类 | 反射节点的 1.x 名称 —— `TransformVector` 是目录中 `Transform` 的别名，`TransformPosition` 是其短名称 |
| 生成 | `UMaterialExpressionTransform` / `UMaterialExpressionTransformPosition` |
| 输出 | 两者均为 `float3` |

基准是引擎枚举的一个值，书写时不带其前缀：`"World"`，或裸词
`World`。在 `.dss` 中会精确匹配。在 1.x 源中，空格、`_` 和 `-` 会被忽略，
可以写出枚举的前缀或作用域，并给出警告
[`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278)：`"world"`、`" WORLD "` 和 `TRANSFORMSOURCE_World` 是
`World`。1.x 自身拥有的名称 —— `AbsoluteWorld`、`Particle`，以及表示位置时的
`CameraRelativeWorld` 和 `FirstPerson` —— 按引擎的名称书写，并给出相同警告（见下文）。
枚举不具有的任何其他值都会导致 [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215)。

---

<a id="uetransformvector"></a>

## UE.TransformVector

在基准之间变换方向向量。仅做旋转 —— 不应用平移，这正是它
不适合用于点的原因。

<a id="synopsis"></a>

### 概要

```c
UE.TransformVector ( { <expression> | Input = <expression> }
                     [ , Source      = "<vector-basis>" ]
                     [ , Destination = "<vector-basis>" ] )
```

<a id="arguments"></a>

### 参数

| 名称 | 必需 | 种类 | 默认值 | 作用 |
| :-- | :-- | :-- | :-- | :-- |
| **`Input`** | 是 | 表达式 | — | 连接到节点的 `Input` 引脚。可以按位置作为参数 0 给出，而不按名称。 |
| `Source` | 否 | 枚举值 | `Tangent` —— 节点的 | 设置 `TransformSourceType` |
| `Destination` | 否 | 枚举值 | `World` —— 节点的 | 设置 `TransformType` |

`Source` 和 `Destination` 接受带引号的字符串或裸词 —— `Source = World` 和
`Source = "World"` 等价。它们是这两个属性在目录中的别名，这两个属性也可以
直接按名称使用。

<a id="source-basis-names"></a>

### 源基准名称

| 拼写 | `EMaterialVectorCoordTransformSource` |
| :-- | :-- |
| `Tangent` | `TRANSFORMSOURCE_Tangent` |
| `Local` | `TRANSFORMSOURCE_Local` |
| `World` | `TRANSFORMSOURCE_World` |
| `View` | `TRANSFORMSOURCE_View` |
| `Camera` | `TRANSFORMSOURCE_Camera` |
| `Instance` | `TRANSFORMSOURCE_Instance` |

<a id="destination-basis-names"></a>

### 目标基准名称

相同的六种拼写，即目标枚举的值。

| 拼写 | `EMaterialVectorCoordTransform` |
| :-- | :-- |
| `Tangent` | `TRANSFORM_Tangent` |
| `Local` | `TRANSFORM_Local` |
| `World` | `TRANSFORM_World` |
| `View` | `TRANSFORM_View` |
| `Camera` | `TRANSFORM_Camera` |
| `Instance` | `TRANSFORM_Instance` |

> [!NOTE]
> 1.x 源在任一列表中还可以为 `World` 写出 `AbsoluteWorld`，为
> `Instance` 写出 `Particle` 或 `InstanceParticle`，大小写不限：前端会写出引擎的名称，并给出警告
> `DSH5278` *（2.0.0 – 2.1.0 曾以 `DSH5215` 拒绝每一个）*。引擎自身的 `ParticleWorld` 值会
> 被隐藏，目录中不会列出它。

---

<a id="uetransformposition"></a>

## UE.TransformPosition

在基准之间变换点，应用平移。

<a id="synopsis-1"></a>

### 概要

```c
UE.TransformPosition ( { <expression> | Input = <expression> }
                       [ , Source                        = "<position-basis>" ]
                       [ , Destination                   = "<position-basis>" ]
                       [ , PeriodicWorldTileSize         = <expression> ]
                       [ , FirstPersonInterpolationAlpha = <expression> ] )
```

<a id="arguments-1"></a>

### 参数

| 名称 | 必需 | 种类 | 默认值 | 作用 |
| :-- | :-- | :-- | :-- | :-- |
| **`Input`** | 是 | 表达式 | — | 连接到节点的 `Input` 引脚。可以按位置作为参数 0 给出。 |
| `Source` | 否 | 枚举值 | `Local` —— 节点的 | 设置 `TransformSourceType` |
| `Destination` | 否 | 枚举值 | `World`，与 1.x 中一样 —— 不是节点的 `Local` | 设置 `TransformType` |
| `PeriodicWorldTileSize` | 否 | 表达式 | 引脚未连接 | 连接到该名称的节点引脚，前提是引擎的类具有该引脚；在 `PeriodicWorld` 基准下有意义 |
| `FirstPersonInterpolationAlpha` | 否 | 表达式 | 引脚未连接 | 连接到该名称的节点引脚，前提是引擎的类具有该引脚；在 `FirstPersonTranslatedWorld` 基准下有意义 |

> [!IMPORTANT]
> 节点自身的 `Destination` 是 `Local`。1.x 写出的是 `World`，前端也是如此：不带
> `Destination` 的调用会转到世界空间，而 [`dsc migrate`](../tools/migrate.md) 会写出
> `Destination = World`，因为对 `.dss` 的调用会得到节点的 `Local`。*（2.0.0 – 2.1.0 未写出
> 默认值，此类调用是一次不改变任何东西的变换。）*

<a id="basis-names"></a>

### 基准名称

一个枚举同时服务于 `Source` 和 `Destination`。

| 拼写 | `EMaterialPositionTransformSource` |
| :-- | :-- |
| `Local` | `TRANSFORMPOSSOURCE_Local` |
| `World` | `TRANSFORMPOSSOURCE_World` |
| `PeriodicWorld` | `TRANSFORMPOSSOURCE_PeriodicWorld` —— 前提是引擎的枚举具有它 |
| `TranslatedWorld` | `TRANSFORMPOSSOURCE_TranslatedWorld` |
| `FirstPersonTranslatedWorld` | `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` —— 前提是引擎的枚举具有它 |
| `View` | `TRANSFORMPOSSOURCE_View` |
| `Camera` | `TRANSFORMPOSSOURCE_Camera` |
| `Instance` | `TRANSFORMPOSSOURCE_Instance` |

1.x 源还可以写出 `AbsoluteWorld`（`World`）、`CameraRelativeWorld`（`TranslatedWorld`）、
`FirstPerson`（`FirstPersonTranslatedWorld`）、`Particle` 和 `InstanceParticle`（`Instance`），
大小写不限：前端会写出引擎的名称，并给出警告 `DSH5278` *（2.0.0 – 2.1.0 曾以 `DSH5215`
拒绝每一个）*。引擎自身的 `Particle` 值会被隐藏且不会在目录中列出。引擎
将 `FirstPersonTranslatedWorld` 标记为不是有效的 *Source*；DreamShader 不检查这一点。

> [!WARNING]
> **运行中的引擎缺少的基准会被报告为未知值，而不是版本错误。**
> 在枚举不具有 `PeriodicWorld` 的引擎上，`UE.TransformPosition(P, Source = "Local", Destination = "PeriodicWorld")` 是 `DSH5215`，
> 即拼写错误会得到的代码。消息会指出失败的属性 ——
> `Source` 对应 `TransformSourceType`，`Destination` 对应 `TransformType` *（自 2.0.0 起）*。

> [!WARNING]
> **两个可选引脚都跟随引擎，且方式相同。** 在类缺少
> `PeriodicWorldTileSize` 或 `FirstPersonInterpolationAlpha` 的引擎上，该参数不是节点的引脚：
> 1.x 源会保留它并给出信息 [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)，而构建该
> 节点会失败并给出 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) *（自 2.0.0 起；1.x 会静默丢弃
> `PeriodicWorldTileSize`，并以自己的错误拒绝 `FirstPersonInterpolationAlpha`）*。要使一份源在
> 各引擎版本间都能构建，请用 `#if DS_ENGINE_MINOR >= …` 保护该参数 —— 参见 [预处理器](../language/preprocessor.md)。

---

<a id="shared-behaviour"></a>

## 共同行为

| 属性 | 值 |
| :-- | :-- |
| 结果 | `float3` —— 目录中两个类已知的宽度 |
| 结果标志 | 不是纹理对象，不是 `MaterialAttributes`，不是 `Substrate` |
| 参数名匹配 | 精确；在 1.x 源中，仅忽略大小写即匹配的名称会被接受并给出 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)。否则要求精确 —— 不剥离分隔符 |
| 节点复用 | 两个完全相同的调用是同一个节点 *（自 2.0.0 起）* |

> [!WARNING]
> **这些名称在 1.x 中未读取的参数会被丢弃，并伴有警告。** 只保留上文记录的这些名称；
> 其他任何内容都会随
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) 一起被丢弃 *(自 2.0.0 起；1.x 会毫无提示地丢弃它)*。
> `UE.TransformVector(V, Src = "World")` 使用默认的 `Source`、`Tangent`，因为参数
> 拼写为 `Src` 而不是 `Source`；第二个位置参数——`UE.TransformVector(V, "World")`——
> 也会被丢弃，因为只读取位置 0。始终完整命名 `Source` 和 `Destination`。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279)（警告） | 没有 `Input`，无论是按名称还是位于位置 0：必需的引脚未连接 *(自 2.0.0 起：1.x 会拒绝该调用)* |
| `DSH5254`（警告） | 除已记录参数之外的参数，或位置 0 之后的位置参数被丢弃 |
| `DSH5215` | 某个基础是正在运行的引擎枚举中不存在的值 |
| `DSH5278`（警告） | 某个基础按 1.x 方式拼写——另一种大小写、前缀、空格，或只有 1.x 才有的名称，例如 `AbsoluteWorld` |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | `Source` 或 `Destination` 既不是带引号的字符串也不是单词 |
| `DSH5276`（警告） | 某个参数名仅忽略大小写后匹配 |
| `DSH5291`（信息）、`DSH8212` | 正在运行的引擎类不具有的可选引脚 |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | 某个引脚被赋予它无法接受的值 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 无法创建材质节点 |

求值 `Input`、`PeriodicWorldTileSize` 或 `FirstPersonInterpolationAlpha` 表达式会报告
内部表达式自身的诊断。每个代码都在
[诊断](../diagnostics/index.md) 中的对应页面上进行了描述。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Transforms")
{
    Properties = {
        vec3 TangentNormal = vec3(0.0, 0.0, 1.0);
    }
    Outputs  = {
        vec3 N;
        vec3 Emissive;
        Base.Normal        = N;
        Base.EmissiveColor = Emissive;
    }
    Graph = {
        // Direction: tangent space -> world space (both node defaults, written out for clarity).
        N = UE.TransformVector(TangentNormal, Source = "Tangent", Destination = "World");

        // Point: absolute world space -> camera-relative world space.
        vec3 WorldP = UE.WorldPosition();
        vec3 ViewP  = UE.TransformPosition(WorldP, Source = "World", Destination = "TranslatedWorld");

        Emissive = normalize(ViewP) * 0.5 + vec3(0.5, 0.5, 0.5);
    }
}
```

生成的节点：

```text
Transform          Input=TangentNormal  TransformSourceType=TRANSFORMSOURCE_Tangent
                                        TransformType=TRANSFORM_World          -> N        (3 components)
WorldPosition                                                                  -> WorldP   (output XYZ)
TransformPosition  Input=WorldP         TransformSourceType=TRANSFORMPOSSOURCE_World
                                        TransformType=TRANSFORMPOSSOURCE_TranslatedWorld
                                                                               -> ViewP    (3 components)
Normalize / Multiply / Add chain                                               -> Emissive
```

<a id="see-also"></a>

## 另请参阅

- [内置函数](index.md) — `Graph` 内可用的调用接口
- [`UE.*` 目录](ue.md) — 其他 25 个 1.x 名称及其参数
- [`UE.Expression`](ue-expression.md) — 按类获取任何 `UMaterialExpression`，以及枚举值
- [数学内置函数](math.md) — `normalize`、`dot` 以及其余无前缀调用接口
- [`Substrate.*`](substrate.md) — Substrate 节点
- [调用](../graph/calls.md) — 调用语法、命名参数、位置参数
- [表达式与运算符](../graph/expressions.md) — 参数表达式可以包含的内容
- [转换](../graph/conversions.md) — 宽度以及值如何适配
- [节点重用](../graph/node-reuse.md) — 相同节点
- [输出绑定](../language/output-bindings.md) — `Base.Normal` 以及其他绑定目标
- [诊断索引](../diagnostics/index.md) — 每个代码
