---
title: "UE.* 内置节点"
description: "UE.* 内置节点"
---

<a id="ue-builtins"></a>

# UE.* 内置节点

> [DreamShader](../index.md) » [内置节点](index.md) » **UE.* 内置节点**

`UE.` 命名空间里由 1.x 自己实现的 27 个名字，每个都有一张固定的参数表，以及它特殊处理的三
个名字。自 2.0.0 起，每个 `UE.` 名字 —— 包括这些 —— 都是反射内置目录中的一个节点；这些
1.x 名字保留下来的，是它们的 1.x 参数表，legacy 前端会在绑定器看到调用之前，把这张表应用到
`.dsm` / `.dsf` / `.dsh` 上。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 在 `Graph { … }` 体内（表达式形式）或 `Properties { … }` 段中（声明形式） |
| 种类 | 内置命名空间 |
| 生成 | 每次调用一个 `UMaterialExpression`；相同的调用是同一个节点 |

<a id="synopsis"></a>

## 语法概要

```c
// expression form, inside Graph
UE.<Name> ( [ <argument> [ , <argument> ] … ] )

<argument> := <arg-name> = <expression>

// declaration form, inside Properties
UE.<Name> [ ( <key> = <value> [ , <key> = <value> ] … ) ] <property-name> ;
```

命名空间前缀是精确匹配的 *（自 2.0.0 起）*：在 `Graph` 中，`ue.TexCoord()` 会把 `ue` 读作
变量，并报出 [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)。内置名也是精确匹配的，而在 1.x 源码里，只有在忽略大小写时才
匹配的名字会被接受，并给出警告 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) —— `UE.texcoord()` 就是 `UE.TexCoord()`。参数
名先去除首尾空白再按同样方式匹配，除此之外必须精确 —— `Un_Mirror_U` 不是
`UnMirrorU`。

每个名字都是目录里的一个类：它的短名（去掉 `MaterialExpression` 的类名），或目录所带的 1.x 别名之一 ——
`TexCoord`、`ObjectPosition`、`CameraVector`、
`CameraPosition`、`ReflectionVector`、`ViewportUV`、`TransformVector`。任何其他引擎类都以同样的方式
通过其短名访问（[`UE.Expression`](ue-expression.md)）；运行中的引擎没有对应类的名字会报
[`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)。

> [!WARNING]
> **1.x 名字不读的参数会被丢弃，并给出警告。** 对[名单](#catalogue)里的 27 个名字，legacy 前端会保留
> 下面每个条目的参数表，并丢弃其他所有参数 —— 未知或拼写错误的名字，或位置参数 —— 并给出
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) *（自 2.0.0 起；1.x 不声不响地丢弃它们）*。
> `UE.TexCoord(Indx = 3)` 仍然产生 UV channel 0，并说明原因；`UE.Panner(SpedX = 1)` 按
> 节点默认速度平移，也会说明。唯一保留的位置参数是
> `UE.TransformVector` 和 `UE.TransformPosition` 的索引 0（→ `Input`）。通过了过滤的参数，会像
> 任何节点调用的参数一样按目录绑定。

<a id="output-widths"></a>

## 输出宽度

调用的类型取自目录 —— 引擎对运行中引擎上该类的自身描述（`dsc export-catalog` 会把它写出来）：

- 带掩码的输出和它的掩码一样宽；
- 不带掩码的输出取引擎的值类型；引擎只说「一个 float」时，用 DreamShader 为该类保留的宽度：
  `Time`、`PixelDepth`、`SceneDepth`、`ObjectRadius`、
  `TwoSidedSign`、`PerInstanceRandom`、`PerInstanceFadeAmount` 为 1；`TextureCoordinate`、`Panner` 为 2；
  `ObjectPositionWS`、`ObjectBounds`、`CameraVectorWS`、`CameraPositionWS`、
  `ReflectionVectorWS`、`VertexNormalWS`、`VertexTangentWS`、`PixelNormalWS`、`Transform`、
  `TransformPosition` 为 3；
- 引擎完全没给类型的输出，和它被读入的位置一样宽；
- 多输出的节点要么指名读其中一个（`UE.ScreenPosition().ViewportUV`），要么在它的第一个输出 —— 或各通道输出拼成的值
  （`float4 VC =
  UE.VertexColor();`）—— 恰好合适的地方整体使用。在 1.x 源码里，第一个输出没有固定宽度的
  节点读作那个输出（[`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287)），直接传给 pin 的任何节点也一样 ——
  `UE.TransformPosition(UE.WorldPosition(), …)` 取 `XYZ`，和 1.x 的连法一致
  *（2.0.0 – 2.1.0 对第一个输出宽度固定的节点报 [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201)）*。

这些内置都不会产生纹理对象、`MaterialAttributes` 值或 Substrate 值。
参见[转换](../graph/conversions.md)。

<a id="catalogue"></a>

## 目录

全部 27 个 1.x 名字，按 1.x 注册顺序。*输出*一列是 UE 5.8 目录给出的。

| 内置节点 | `UMaterialExpression` 类 | 输出 | 1.x 参数 |
| :-- | :-- | :-- | :-- |
| [`UE.TexCoord`](#uetexcoord) | `TextureCoordinate` | `float2` | `Index`、`UTiling`、`VTiling`、`UnMirrorU`、`UnMirrorV` |
| [`UE.Time`](#uetime) | `Time` | `float1` | `Period`、`IgnorePause` |
| [`UE.Panner`](#uepanner) | `Panner` | `float2` | `Coordinate`、`Time`、`Speed`、`SpeedX`、`SpeedY`、`FractionalPart` |
| [`UE.WorldPosition`](#ueworldposition) | `WorldPosition` | `XYZ` `float3`，另有 `XY`、`Z` | 无 |
| [`UE.ObjectPositionWS`](#ueobjectpositionws) | `ObjectPositionWS` | `float3` | 无 |
| [`UE.CameraVectorWS`](#uecameravectorws) | `CameraVectorWS` | `float3` | 无 |
| [`UE.VertexNormalWS`](#uevertexnormalws) | `VertexNormalWS` | `float3` | 无 |
| [`UE.VertexTangentWS`](#uevertextangentws) | `VertexTangentWS` | `float3` | 无 |
| [`UE.ScreenPosition`](#uescreenposition) | `ScreenPosition` | `ViewportUV`、`PixelPosition` | 无 |
| [`UE.VertexColor`](#uevertexcolor) | `VertexColor` | `RGB`、`R`、`G`、`B`、`A`；整体是 `float4` | 无 |
| [`UE.PixelDepth`](#uepixeldepth) | `PixelDepth` | `float1` | 无 |
| [`UE.SceneDepth`](#uescenedepth) | `SceneDepth` | `float1` | 无 |
| [`UE.SceneColor`](#uescenecolor) | `SceneColor` | `RGB`、`A`；整体是 `float4` | 无 |
| [`UE.TranslatedWorldPosition`](#uetranslatedworldposition) | `WorldPosition`（相机相对） | 同 `UE.WorldPosition` | 无 |
| [`UE.ObjectPosition`](#ueobjectposition) | `ObjectPositionWS` | `float3` | 无 |
| [`UE.ObjectRadius`](#ueobjectradius) | `ObjectRadius` | `float1` | 无 |
| [`UE.ObjectBounds`](#ueobjectbounds) | `ObjectBounds` | `float3` | 无 |
| [`UE.CameraVector`](#uecameravector) | `CameraVectorWS` | `float3` | 无 |
| [`UE.CameraPosition`](#uecameraposition) | `CameraPositionWS` | `float3` | 无 |
| [`UE.ReflectionVector`](#uereflectionvector) | `ReflectionVectorWS` | `float3` | 无 |
| [`UE.PixelNormalWS`](#uepixelnormalws) | `PixelNormalWS` | `float3` | 无 |
| [`UE.TwoSidedSign`](#uetwosidedsign) | `TwoSidedSign` | `float1` | 无 |
| [`UE.PerInstanceRandom`](#ueperinstancerandom) | `PerInstanceRandom` | `float1` | 无 |
| [`UE.PerInstanceFadeAmount`](#ueperinstancefadeamount) | `PerInstanceFadeAmount` | `float1` | 无 |
| [`UE.ViewportUV`](#ueviewportuv) | `ScreenPosition` | 同 `UE.ScreenPosition` | 无 |
| [`UE.TransformVector`](#uetransformvector) | `Transform` | `float3` | **`Input`**、`Source`、`Destination` |
| [`UE.TransformPosition`](#uetransformposition) | `TransformPosition` | `float3` | **`Input`**、`Source`、`Destination`、`PeriodicWorldTileSize`、`FirstPersonInterpolationAlpha` |

另外有三个名字被单独处理，记录在
[特殊处理的内置节点](#special-cased-builtins)下：`UE.StaticSwitchParameter`、`UE.CollectionParam` /
`UE.CollectionParameter`，以及 `UE.SceneTexture`。

上表中的类名省略了 `MaterialExpression` 前缀。

> [!NOTE]
> 1.x 只在当前编辑器能解析出对应类时才注册 `UE.ObjectPositionWS`、`UE.ObjectPosition`、`UE.ScreenPosition` 和 `UE.ViewportUV`。
> 目录列出运行中引擎的每一个类，所以不再有条件注册 *(自 2.0.0 起)*。

<a id="uetexcoord"></a>

### UE.TexCoord

```c
UE.TexCoord([Index = <int>] [, UTiling = <float>] [, VTiling = <float>]
            [, UnMirrorU = <bool>] [, UnMirrorV = <bool>])
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Index` | 常量 —— 即属性 `CoordinateIndex` | 节点默认值，UV channel 0 | 否 |
| `UTiling` | 常量 | 节点默认值 | 否 |
| `VTiling` | 常量 | 节点默认值 | 否 |
| `UnMirrorU` | 常量 `true` / `false` | 节点默认值 | 否 |
| `UnMirrorV` | 常量 `true` / `false` | 节点默认值 | 否 |

节点 `UMaterialExpressionTextureCoordinate`；输出 `float2`。

`Index` 是目录给 `CoordinateIndex` 的别名。`CoordinateIndex` 本身不在 1.x 参数表上，
所以在这个形式下它被 `DSH5254` 丢弃；[声明形式](#properties-declaration-form)
两者都接受。负的 `Index` 原样写入，不报诊断。

<a id="uetime"></a>

### UE.Time

```c
UE.Time([Period = <float>] [, IgnorePause = <bool>])
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Period` | 常量 | 不写时节点的周期覆盖保持关闭 | 否 |
| `IgnorePause` | 常量 `true` / `false` —— 即属性 `bIgnorePause` | 节点默认值 | 否 |

节点 `UMaterialExpressionTime`；输出 `float1`。写了 `Period` 也会打开节点的周期
覆盖：前端会给调用加上 `bOverride_Period = true`。不做范围检查，负的 `Period`
原样写入 —— [声明形式](#properties-declaration-form)也一样 *(自 2.0.0 起)*。

<a id="uepanner"></a>

### UE.Panner

```c
UE.Panner([Coordinate = <expr>] [, Time = <expr>] [, Speed = <expr>]
          [, SpeedX = <float>] [, SpeedY = <float>] [, FractionalPart = <bool>])
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Coordinate` | 输入 pin | 未连接 | 否 |
| `Time` | 输入 pin | 未连接 | 否 |
| `Speed` | 输入 pin | 未连接 | 否 |
| `SpeedX` | 常量 | 节点默认值 | 否 |
| `SpeedY` | 常量 | 节点默认值 | 否 |
| `FractionalPart` | 常量 `true` / `false` —— 即属性 `bFractionalPart` | 节点默认值 | 否 |

节点 `UMaterialExpressionPanner`；输出 `float2`。`ConstCoordinate` 在这个形式下被 `DSH5254` 丢弃；
[声明形式](#properties-declaration-form)会传下去，按类名调用也会：
`UE.Expression(Class = "Panner", ConstCoordinate = 1)`。

<a id="ueworldposition"></a>

### UE.WorldPosition

```c
UE.WorldPosition()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition` | `XYZ`（`float3`），另有 `XY` 和 `Z` | 无 |

绝对世界位置 —— 节点的 shader-offset 模式保持默认。这里 `ShaderOffsets` 被
`DSH5254` 丢弃；相机相对变体用 [`UE.TranslatedWorldPosition`](#uetranslatedworldposition)，
其他模式用 `UE.Expression(Class = "WorldPosition", WorldPositionShaderOffset = …)`。[属性声明](#properties-declaration-form)确实接受
`ShaderOffsets`。

<a id="ueobjectpositionws"></a>

### UE.ObjectPositionWS

```c
UE.ObjectPositionWS()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | 无 |

`UE.ObjectPosition` 是别名。[声明形式](#properties-declaration-form)接受
`Origin` 参数；表达式形式则用 `DSH5254` 丢弃它。

<a id="uecameravectorws"></a>

### UE.CameraVectorWS

```c
UE.CameraVectorWS()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraVectorWS` | `float3` | 无 |

`UE.CameraVector` 是同一个节点的别名。

<a id="uevertexnormalws"></a>

### UE.VertexNormalWS

```c
UE.VertexNormalWS()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexNormalWS` | `float3` | 无 |

<a id="uevertextangentws"></a>

### UE.VertexTangentWS

```c
UE.VertexTangentWS()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexTangentWS` | `float3` | 无 |

<a id="uescreenposition"></a>

### UE.ScreenPosition

```c
UE.ScreenPosition()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionScreenPosition` | `ViewportUV`、`PixelPosition` | 无 |

该节点有两个输出，引擎没有给它们类型：`UE.ScreenPosition().ViewportUV` 和它被读入的位置一样宽。
在 1.x 源码里当普通值使用时，调用取的是它的第一个输出，
*ViewportUV*（`DSH5287`）。[`UE.ViewportUV`](#ueviewportuv) 是该类的别名。

<a id="uevertexcolor"></a>

### UE.VertexColor

```c
UE.VertexColor()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexColor` | `RGB`、`R`、`G`、`B`、`A`；整体 `float4` | 无 |

这些输出都是同一个颜色的通道视图：`float4 c = UE.VertexColor();` 是整个颜色，
`UE.VertexColor().a` 是 `A` 输出。

<a id="uepixeldepth"></a>

### UE.PixelDepth

```c
UE.PixelDepth()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionPixelDepth` | `float1` | 无 |

针对当前像素的场景读取：该节点创建时不接 UV 或 offset 输入。

<a id="uescenedepth"></a>

### UE.SceneDepth

```c
UE.SceneDepth()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionSceneDepth` | `float1` | 无 |

只有无参数形式；参数被 `DSH5254` 丢弃，且节点的 UV 输入保持
未连接。要连接一个，用 `UE.Expression(Class = "SceneDepth", Input = <uv>)`。

<a id="uescenecolor"></a>

### UE.SceneColor

```c
UE.SceneColor()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionSceneColor` | `RGB`、`A`；整体 `float4` | 无 |

<a id="uetranslatedworldposition"></a>

### UE.TranslatedWorldPosition

```c
UE.TranslatedWorldPosition()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition`，强制使用相机相对 shader-offset 模式 | 同 [`UE.WorldPosition`](#ueworldposition) | 无 |

在 HLSL 中等价于 `GetTranslatedWorldPosition(Parameters)`。前端把调用改写为
`UE.WorldPosition(WorldPositionShaderOffset = CameraRelative)`，同时用 `DSH5254` 丢弃任何参数；
节点的默认模式是 *绝对* 世界位置，这也是它与
[`UE.WorldPosition`](#ueworldposition) 的唯一区别。目录里没有这个名字的类。*(2.0.0 – 2.1.0 把
值写成 `WPT_CameraRelative`，结果没错，但每次调用都带警告 `DSH5278`。)*

<a id="ueobjectposition"></a>

### UE.ObjectPosition

```c
UE.ObjectPosition()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | 无 |

[`UE.ObjectPositionWS`](#ueobjectpositionws) 的别名。

<a id="ueobjectradius"></a>

### UE.ObjectRadius

```c
UE.ObjectRadius()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectRadius` | `float1` | 无 |

<a id="ueobjectbounds"></a>

### UE.ObjectBounds

```c
UE.ObjectBounds()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectBounds` | `float3` | 无 |

<a id="uecameravector"></a>

### UE.CameraVector

```c
UE.CameraVector()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraVectorWS` | `float3` | 无 |

[`UE.CameraVectorWS`](#uecameravectorws) 的别名。

<a id="uecameraposition"></a>

### UE.CameraPosition

```c
UE.CameraPosition()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraPositionWS` | `float3` | 无 |

<a id="uereflectionvector"></a>

### UE.ReflectionVector

```c
UE.ReflectionVector()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionReflectionVectorWS` | `float3` | 无 |

<a id="uepixelnormalws"></a>

### UE.PixelNormalWS

```c
UE.PixelNormalWS()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionPixelNormalWS` | `float3` | 无 |

<a id="uetwosidedsign"></a>

### UE.TwoSidedSign

```c
UE.TwoSidedSign()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionTwoSidedSign` | `float1` | 无 |

<a id="ueperinstancerandom"></a>

### UE.PerInstanceRandom

```c
UE.PerInstanceRandom()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionPerInstanceRandom` | `float1` | 无 |

<a id="ueperinstancefadeamount"></a>

### UE.PerInstanceFadeAmount

```c
UE.PerInstanceFadeAmount()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionPerInstanceFadeAmount` | `float1` | 无 |

<a id="ueviewportuv"></a>

### UE.ViewportUV

```c
UE.ViewportUV()
```

| 节点 | 输出 | 参数 |
| :-- | :-- | :-- |
| `UMaterialExpressionScreenPosition` | 作为 [`UE.ScreenPosition`](#uescreenposition) | 无 |

引擎没有专门的 *ViewportUV* 表达式类；它是第二个名字下的
[`UE.ScreenPosition`](#uescreenposition)——目录里该类的别名——
而 *ViewportUV* 是节点的第一个输出。

<a id="uetransformvector"></a>

### UE.TransformVector

```c
UE.TransformVector({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>])
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Input`** | 输入 pin；可以作为位置 0 给出 | —— | **是** |
| `Source` | 枚举值 —— 即属性 `TransformSourceType` | `Tangent`（节点的） | 否 |
| `Destination` | 枚举值 —— 即属性 `TransformType` | `World`（节点的） | 否 |

节点 `UMaterialExpressionTransform`；输出 `float3`。

坐标系是引擎枚举的一个值，写的时候不带前缀——`Tangent`、`Local`、`World`、
`View`、`Camera`、`Instance`——在 1.x 源码里也可以带前缀（`TRANSFORMSOURCE_World`），
或者在其他大小写下，并给出警告 [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278)。1.x 自己的名字也写成引擎的名字，
并给出同样的警告：`AbsoluteWorld` 是 `World`，
`Particle` 和 `InstanceParticle` 是 `Instance` *(2.0.0 – 2.1.0 以 `DSH5215` 拒绝它们)*。任何
其他值都是 [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215)，它会指出出错的属性——
`TransformSourceType` 对应 `Source`，`TransformType` 对应 `Destination`。完整坐标系参考：
[变换坐标系](transform.md)。

<a id="uetransformposition"></a>

### UE.TransformPosition

```c
UE.TransformPosition({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>]
                     [, PeriodicWorldTileSize = <expr>] [, FirstPersonInterpolationAlpha = <expr>])
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Input`** | 输入 pin；可以作为位置 0 给出 | —— | **是** |
| `Source` | 枚举值 —— 即属性 `TransformSourceType` | `Local`（节点的） | 否 |
| `Destination` | 枚举值 —— 即属性 `TransformType` | `World`，和 1.x 一样 | 否 |
| `PeriodicWorldTileSize` | 输入 pin，引擎的类有这个 pin 时 | 未连接 | 否 |
| `FirstPersonInterpolationAlpha` | 输入 pin，引擎的类有这个 pin 时 | 未连接 | 否 |

节点 `UMaterialExpressionTransformPosition`；输出 `float3`。

坐标系是引擎枚举的一个值，写的时候不带前缀——`Local`、`World`、
`TranslatedWorld`、`View`、`Camera`、`Instance`，以及在有它们的引擎上 `PeriodicWorld`
和 `FirstPersonTranslatedWorld`——在 1.x 源码里也可以带前缀或写其他大小写
（`DSH5278`）。1.x 的名字也写成引擎的名字，并给出同样的警告：`AbsoluteWorld` 是
`World`，`CameraRelativeWorld` 是 `TranslatedWorld`，`FirstPerson` 是 `FirstPersonTranslatedWorld`，
`Particle` 和 `InstanceParticle` 是 `Instance` *(2.0.0 – 2.1.0 以 `DSH5215` 拒绝它们)*。
运行中引擎的枚举没有的值是 `DSH5215`。

> [!WARNING]
> **两个可选 pin 跟随引擎。** 在引擎的类没有某个 pin 的引擎上，该参数不是
> 节点的 pin：在 1.x 源码里它以提示
> [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) 保留，而构建节点时以
> [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) 失败 *(自 2.0.0 起；1.x 静默丢弃 `PeriodicWorldTileSize`
> 并对 `FirstPersonInterpolationAlpha` 报自己的错误)*。

> [!NOTE]
> 节点自己的 `Destination` 是 `Local`；1.x 写的是 `World`，前端也这么写，所以
> 不写 `Destination` 的调用仍然变换到世界空间，[`dsc migrate`](../tools/migrate.md) 会
> 写出 `Destination = World`。*(2.0.0 – 2.1.0 什么都不写，这样的调用会变换到 `Local`。)*

<a id="special-cased-builtins"></a>

## 特殊处理的内置节点

`UE.StaticSwitchParameter` 和 `UE.SceneTexture` 在绑定器看到之前被 legacy 前端改写成
`UE.Expression` 调用。`UE.CollectionParam` 不改写：目录把它当作
`UE.CollectionParameter` 的第二个名字。

<a id="uestaticswitchparameter"></a>

### UE.StaticSwitchParameter

*(自 1.2.3 起)*

```c
UE.StaticSwitchParameter(Name = "<parameter-name>",
                         { True = <expr> | <expr> }, { False = <expr> | <expr> }
                         [, Default = <bool>] [, Group = "<text>"]
                         [, Description = "<text>"] [, SortPriority = <int>])
```

| 参数 | 别名 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- | :-- |
| **`Name`** | `ParameterName` | 带引号的字符串或一个词，去空白后非空 | —— | **是** |
| **`True`** | `A`，位置索引 0 | 值 | —— | **是** |
| **`False`** | `B`，位置索引 1 | 值 | —— | **是** |
| `Default` | `DefaultValue` | `true` / `false` | `false` | 否 |
| `Group` | —— | 带引号的字符串或一个词 | 无 | 否 |
| `Description` | —— | 带引号的字符串或一个词 | 无 | 否 |
| `SortPriority` | —— | 整数 | 节点默认值 | 否 |

调用变成 `UE.Expression(Class = "StaticSwitchParameter", ParameterName = …, DefaultValue = …,
Group = …, Desc = …, SortPriority = …, A = <True>, B = <False>)`；其他任何参数都被
`DSH5254` 丢弃。节点 `UMaterialExpressionStaticSwitchParameter`。

输出跟随分支：和较宽的数值分支一样宽，或者是某个分支携带的 Substrate 或
`MaterialAttributes` 值。两条分支不互相比较
*(自 2.0.0 起)*；任一分支是纹理报 [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)。

参数以「仅编辑器」的静态开关值注册到生成的材质——或
材质函数——上，因此会出现在实例编辑器里。参数相同的两次调用是一个
节点。

> [!NOTE]
> `Group` 和 `Description` 写入节点的 `Group` 和 `Desc` 属性；值
> 既不是带引号的字符串也不是一个词时报 [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224)
> *(自 2.0.0 起；1.x 不报诊断直接丢弃)*。

<a id="uecollectionparam"></a>

### UE.CollectionParam

*(自 1.2.3 起)*

```c
UE.CollectionParameter(Collection = Path(<root>, "<asset>"), Parameter = "<name>"
                       [, Group = "<text>"] [, SortPriority = <int>] [, Desc = "<text>"])
```

`UE.CollectionParam` 和引擎的名字 `UE.CollectionParameter` 是同一个类，在 `Graph`
体和[声明形式](#properties-declaration-form)里都一样。*(2.0.0 – 2.1.0 在 `Graph` 体里以
[`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) 拒绝 `UE.CollectionParam`。)*

| 参数 | 别名 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- | :-- |
| **`Collection`** | `Asset` | `Path(…)` 或 Unreal object path | —— | **是** |
| **`Parameter`** | `ParameterName` | 带引号的字符串或一个词 | —— | **是** |
| `Group` | —— | 带引号的字符串或一个词，引擎的类有它时 | 无 | 否 |
| `SortPriority` | —— | 整数，引擎的类有它时 | 节点默认值 | 否 |
| `Desc` | —— | 带引号的字符串或一个词 | 无 | 否 |

节点 `UMaterialExpressionCollectionParameter`。建节点时加载集合并按名字查参数：集合解析不了或加载不了报
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)，名字既不是集合的标量参数也不是向量参数报
[`DSH8254`](../diagnostics/DSH8xxx.md#dsh8254)。不写 `Collection` 或 `Parameter` 的调用不做检查
*（自 2.0.0 起）*。`Description` 不是这个调用的参数 —— 节点的描述属性叫 `Desc` —— 写了报
[`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)。

引擎没给节点的输出类型，所以值和它被读入的位置一样宽
*（自 2.0.0 起；1.x 从加载的集合读宽度：向量参数 `float4`，标量参数 `float1`）*。
[声明形式](#properties-declaration-form)除非另行指定，否则声明为 `float4`。

> [!WARNING]
> `Group` 和 `SortPriority` 只在运行中引擎的类有它们时才接受。类没有它们的引擎上，
> `Group` 报 `DSH5213`，`SortPriority` —— 一个数字 —— 以 `DSH5291` 保留、
> 建节点时以 `DSH8212` 失败 *（自 2.0.0 起；1.x 在版本门槛以下静默丢弃两者）*。

`Path(…)` 的语法和可用根目录见 [`Path(…)`](../parameters/path.md)。

<a id="uescenetexture"></a>

### UE.SceneTexture

```c
UE.SceneTexture(Id = "<scene-texture-id>")
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Id`** | 枚举值 | —— | **是** |

纯语法糖。前端把调用改写成

```c
UE.Expression(Class = "SceneTexture", SceneTextureId = <Id>)[0]
```

—— `UMaterialExpressionSceneTexture` 的输出 0，*Color*，一个 `float4`。调用必须**恰好**有一个参数，且名字必须是
`Id`；其他任何写法（包括输出选择器）都报 [`DSH5255`](../diagnostics/DSH5xxx.md#dsh5255)。节点的其他属性和它的
`Size` / `InvSize` 输出要用 `UE.Expression(Class = "SceneTexture", …)`。

`Id` 是 `ESceneTextureId` 的值，不带前缀 —— `"PostProcessInput0"`。在 1.x 源码里也接受带前缀的写法
（`"PPI_PostProcessInput0"`）、`ESceneTextureId::` 作用域和其他大小写，并给出 `DSH5278`；显示名或带空格的写法
（`"ppi postprocessinput0"`）报 `DSH5215` *（自 2.0.0 起）*。见[枚举值](ue-expression.md#enum-values)。

<a id="properties-declaration-form"></a>

## Properties 声明形式

`UE.<Name>(…)` 也可以出现在 [`Properties`](../language/properties.md) 段中类型 token 的位置。它不声明
参数：legacy 前端保留这段文本，并在每个读取该 property 的 `Graph` 体开头，声明一个同名局部变量、
用这个调用初始化 —— `float2 UV = UE.TexCoord(Index = 0);` —— 每个这样的体一个，按 property 的顺序。不读它的体不会得到节点。
从那以后这个调用就是普通的 `UE.` 调用，按目录绑定 *（自 2.0.0 起）*。

```c
Properties {
    UE.TexCoord(Index = 0) UV;
    UE.CollectionParam(Collection = Path(Game, "MPC_Weather"), Parameter = "Wind") Wind;
}
```

| 规则 | 声明形式 | 表达式形式 |
| :-- | :-- | :-- |
| `UE` | 任意大小写 | 精确 |
| 参数语法 | `Key = Value` 对，在顶层逗号处拆开；值是带引号的字符串、数字、`true` / `false`、裸词（读作名字：另一个变量或枚举值），其余保留为文本，如 `Path(…)` | 完整表达式，允许嵌套调用 |
| 1.x 参数过滤（`DSH5254`） | 不应用 | 对 27 个名字应用 |
| 未知参数 | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)；数字以 `DSH5291` 保留、建节点时以 `DSH8212` 失败 | 27 个名字上被 `DSH5254` 丢弃 |
| 重复参数，或同一参数的两种写法（`Index` 与 `CoordinateIndex`） | [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | 相同 |
| 没有 `=` 的片段、空键或空值 | 跳过，不报诊断 | —— |
| 行内默认值（`= …`） | [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | 不适用 |
| 元数据 `[ … ]` | 读取但不应用 | 不适用 |
| 改名的参数 | `Description` → `Desc`；在 `CollectionParam` / `CollectionParameter` 上 `Asset` → `Collection`、`Parameter` → `ParameterName`，调用是 `UE.CollectionParameter` | 无 |
| `OutputType` / `ResultType` | 局部变量的类型，不往下传 —— 见[声明的输出宽度](#declared-output-width) | 见 [`OutputType`](output-type.md) |

这个声明形式接受的 1.x 名字：

| `UE.Name` | 节点类 | 参数 | 与表达式形式的差异 |
| :-- | :-- | :-- | :-- |
| `TexCoord` | `TextureCoordinate` | `Index` 或 `CoordinateIndex`、`UTiling`、`VTiling`、`UnMirrorU`、`UnMirrorV` | 接受 `CoordinateIndex`；负索引不检查 *（自 2.0.0 起）* |
| `Time` | `Time` | `Period`、`IgnorePause` | `Period` 不做范围检查，也**不会**打开周期覆盖 —— 要写 `bOverride_Period = true` *（自 2.0.0 起）* |
| `Panner` | `Panner` | `Coordinate`、`Time`、`Speed`（pin：词是另一个变量，数字是 pin 上的常量）、`ConstCoordinate`、`SpeedX`、`SpeedY`、`FractionalPart` | 接受 `ConstCoordinate` |
| `WorldPosition` | `WorldPosition` | `ShaderOffsets` —— 即属性 `WorldPositionShaderOffset` | 接受 `ShaderOffsets` |
| `ObjectPositionWS` | `ObjectPositionWS` | `Origin` —— 即属性 `OriginType` | 接受 `Origin` |
| `CameraVectorWS`、`ScreenPosition`、`VertexColor` | 同名 | 没有自己的参数 | 任何参数都是未知参数 |
| `CollectionParam`、`CollectionParameter` | `CollectionParameter` | `Collection` / `Asset`、`Parameter` / `ParameterName`，以及类有时还有的 `Group`、`SortPriority`、`Description` | |
| *（其他任何名字）* | 同名的类 | 它的 pin 和属性 | 不需要 `OutputType` *（自 2.0.0 起）* |

`ShaderOffsets` 取 `EWorldPositionIncludedOffsets` 不带前缀的值 —— `Default`、
`ExcludeAllShaderOffsets`、`CameraRelative`、`CameraRelativeNoOffsets` —— 其他大小写或带
`WPT_` 前缀时给出 `DSH5278`。`Origin` 取 `Absolute` 和 `CameraRelative`。
不是枚举值的 1.x 写法（`IncludingShaderOffsets`、`Absolute` 和 `NoOffsets` 对应
`ShaderOffsets`，`World` 对应 `Origin`，……）报 `DSH5215` *（自 2.0.0 起）*。

<a id="declared-output-width"></a>

### 声明的输出宽度

局部变量的类型取下列第一个适用的来源：

1. 命名类型的 `OutputType` 或 `ResultType` 参数 —— 见 [`OutputType`](output-type.md)；
2. 名字（忽略大小写比较）：`TexCoord`、`Panner` → `float2`；`Time` → `float`；
   `WorldPosition`、`CameraVectorWS`、`ObjectPositionWS`、`VertexNormalWS`、`VertexTangentWS` →
   `float3`；
3. 其他任何名字 → `float4`：`ScreenPosition`、`VertexColor`、`CollectionParam` /
   `CollectionParameter`，以及名单之外的任何名字 *（自 2.0.0 起；1.x 拒绝此类名字
   除非有 `OutputType`）*。

调用像任何初始化式一样初始化局部变量：标量输出铺满局部变量的
宽度，更宽的输出被截短（[`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)），更窄的
向量 —— 名单外名字上的 `float3` 节点 —— 报
[`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228)。引擎没给类型的输出（如
集合参数的）取局部变量的宽度。给这样的声明写上 `OutputType`；标量
集合参数就是 `OutputType = float1`。

`UE.*` 声明前面的 `const` 会被读取然后忽略 *（自 2.0.0 起；1.x 拒绝）*。

<a id="notes"></a>

## 一些值得知道的细节

- `UE.*` 调用可以直接 swizzle：`UE.TexCoord(Index = 0).x` 就是普通的后缀链。在通道输出节点上，某个输出发布的
  swizzle 就是那个输出（`UE.VertexColor().a`）。参见
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result)。
- 一个图中两次相同的调用是 **一个** 节点，无论它们是哪个内置节点：IR 合并具有相同类、属性和输入的节点
  *（自 2.0.0 起；1.x 只合并通用调用和
  `Substrate.*` 调用）*。参见 [节点复用](../graph/node-reuse.md)。
- 名为 `TexCoord` 的属性、局部变量或参数不会遮蔽 `UE.TexCoord`；名为 `UE` 的会 ——
  这时 `UE.X(…)` 是对那个变量的成员调用 *（自 2.0.0 起）*。参见
  [名字解析](../graph/name-resolution.md)。
- 在 27 个名字上，`OutputType`、`ResultType` 和 `Class` 与任何该名字不读取的参数一样被 `DSH5254`
  丢弃。`Output`、`OutputName` 和 `OutputIndex` 选择节点的一个输出，与
  所有调用一样 *（自 2.0.0 起）*。
- 在 [`GraphFunction`](../language/graph-function.md) 内部，`UE.*` 调用被提升出 HLSL
  体，变成生成的 Custom 节点上的输入 pin。

<a id="diagnostics"></a>

## 诊断

每个代码都列出了它的消息和完整描述，见
[诊断](../diagnostics/index.md) 中各自的页面。

<a id="expression-form"></a>

### 表达式形式

| 代码 | 触发时机 |
| :-- | :-- |
| [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)（警告） | 27 个名字之一不读取的参数 —— 未知、拼错或位置参数 —— 被丢弃 |
| [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)（警告） | 在 1.x 源码中，内置名、参数名或输出名只在忽略大小写时才匹配 |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | 命名空间没写成 `UE` |
| [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) | `UE.` 之后的名字不是运行中引擎的类 |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | 通过了过滤的参数不是该类的任何 pin 或属性 |
| [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)（提示） | 在 1.x 源码中，这样的参数带着值，被保留下来，等建好的节点按名字把它作为 pin |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | 属性收到的不是常量，或文本属性收到的既不是带引号的字符串也不是一个词 |
| [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) | 枚举值 —— 坐标系、scene texture id —— 不是引擎枚举的值 |
| [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278)（警告） | 枚举值按 1.x 方式书写：带前缀或作用域，或其他大小写 |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | 值放不进它连接的 pin |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | 同一个 pin 或属性给了两次 |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279)（警告） | 必需的 pin —— 变换的 `Input` —— 没有连接 |
| [`DSH5255`](../diagnostics/DSH5xxx.md#dsh5255) | `UE.SceneTexture` 不是恰好一个参数，`Id` |
| [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) | 建好的节点没有以 `DSH5291` 保留的那个名字的 pin |
| [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) | 节点无法接受属性值；消息中带有原因（`DSH7132`–`DSH7143`） |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 节点无法创建 |

<a id="uestaticswitchparameter-1"></a>

### `UE.StaticSwitchParameter`

| 代码 | 触发时机 |
| :-- | :-- |
| [`DSH5257`](../diagnostics/DSH5xxx.md#dsh5257) | `Name` 和 `ParameterName` 都没有给出，或者为空 |
| [`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258) | 缺少一条分支、两条分支是同一个参数，或调用带了输出选择器 |
| [`DSH5263`](../diagnostics/DSH5xxx.md#dsh5263) | `Default` / `DefaultValue` 不是 `true` 或 `false` |
| [`DSH5264`](../diagnostics/DSH5xxx.md#dsh5264) | `SortPriority` 不是整数 |
| `DSH5254`（警告） | 其他任何参数被丢弃 |
| `DSH5224` | `Group` 或 `Description` 既不是带引号的字符串也不是一个词 |
| `DSH5214` | 某个分支是纹理 |

<a id="uecollectionparameter"></a>

### `UE.CollectionParameter`

| 代码 | 触发时机 |
| :-- | :-- |
| `DSH5213` | `Description`，或在类没有它的引擎上是 `Group` |
| `DSH5291` / `DSH8212` | 在类没有它的引擎上是 `SortPriority` |
| `DSH5224` | `Parameter` 既不是带引号的字符串也不是一个词 |
| `DSH8213` | 集合无法解析、无法加载，或者不是 `MaterialParameterCollection` |
| `DSH8254` | 集合里没有这个名字的标量或向量参数 |

<a id="declaration-form"></a>

### 声明形式

| 代码 | 触发时机 |
| :-- | :-- |
| [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | 属性名之后跟了默认值，或参数列表没有闭合 |
| [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250) | 内置之后没有属性名 |
| `DSH5210` | 名字不是运行中引擎的类 |
| `DSH5213`，`DSH5291` / `DSH8212` | 该类没有对应 pin 或属性的参数 |
| `DSH4215` | 同一个 pin 或属性给了两次 |
| `DSH5215`，`DSH5278` | 枚举值 —— `ShaderOffsets`、`Origin` —— 与表达式形式相同 |
| `DSH5224` | 不是常量的属性值 |
| `DSH8213`，`DSH8254` | 同 `UE.CollectionParameter` |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_UEBuiltins")
{
    Properties {
        vec3 Tint = vec3(1.0, 0.6, 0.2);
        UE.TexCoord(Index = 0) UV0;
    }

    Settings {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        float2 uv    = UE.Panner(Coordinate = UV0, Time = UE.Time(), SpeedX = 0.1, SpeedY = 0.0);
        float3 wp    = UE.TranslatedWorldPosition();
        float3 nrm   = UE.TransformVector(UE.VertexNormalWS(), Source = "World", Destination = "Tangent");
        float  fade  = UE.PerInstanceFadeAmount();
        float4 vcol  = UE.VertexColor();

        Color = (vec3(uv.x, uv.y, wp.z) + nrm) * Tint * vcol.rgb * fade;
    }
}
```

生成的节点：

```text
TextureCoordinate  (CoordinateIndex 0)        <- UV0        (a local at the head of Graph)
Panner             (Coordinate, Time, SpeedX) <- uv
Time                                          <- Panner.Time
WorldPosition      (camera-relative)          <- wp
VertexNormalWS                                -> Transform
Transform          (World -> Tangent)         <- nrm
PerInstanceFadeAmount                         <- fade
VertexColor                                   <- vcol
```

<a id="see-also"></a>

## 另请参见

- [内置](index.md) — 调用形式以及被调用方如何解析
- [`UE.Expression`](ue-expression.md) — 每个引擎类按其名称
- [`OutputType`](output-type.md) — 在 1.x 源码中该参数仍然起什么作用
- [变换基准](transform.md) — 基准值
- [Substrate](substrate.md) — 同级的命名空间
- [数学内置](math.md) — `pow`、`fmod`、`min`、`max` 以及其他
- [Properties](../language/properties.md) — 声明形式所在的段语法
- [参数节点](../../parameters/parameter-nodes.md) — 显式的 `*Parameter` token
- [`Path(…)`](../parameters/path.md) — `UE.CollectionParameter` 的资产引用
- [节点复用](../graph/node-reuse.md) — 相同的调用与节点
- [转换](../graph/conversions.md) — 加宽与截短
- [`dsc migrate`](../tools/migrate.md) — 每次 1.x 调用的 `.dss` 写法
- [诊断索引](../diagnostics/index.md) — 每个代码
