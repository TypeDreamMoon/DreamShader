---
title: UE.* 节点
description: 1.x 自己实现的 27 个 UE.* 名字及其参数表、两个变换内置的坐标系、三个特殊处理的名字，以及 Properties 声明形式。
---

`UE.` 命名空间是 DreamShaderLang 创建 Unreal 材质节点的入口。其中 27 个名字是 1.x **自己实现**的，每个都有一张
固定的参数表；另有三个名字 —— `UE.StaticSwitchParameter`、`UE.CollectionParam` 和 `UE.SceneTexture` ——
被特殊处理。

（自 2.0.0 起） 起每个 `UE.` 名字（包括这 27 个）都是**内置目录**里的一个节点 —— 目录就是运行中引擎的
材质表达式类，靠反射读出（`dsc export-catalog` 可以把它写出来）。这 27 个名字保留下来的是它们的 1.x 参数表：
legacy 前端在绑定器看到调用之前，把这张表应用到 `.dsm` / `.dsf` / `.dsh` 上。本页没有列出的名字照样能用 ——
任何引擎类都按短名（去掉 `MaterialExpression` 的类名）调用，规则见[通用 UE.Expression](ue-expression.md)；
运行中的引擎没有对应类的名字报 `DSH5210`。

名字的匹配：

| 部分 | 规则 |
| :-- | :-- |
| `UE.` 前缀 | 精确 （自 2.0.0 起） —— `ue.TexCoord()` 把 `ue` 读成变量，报 `DSH4200` |
| 内置名 | 精确；1.x 源码里只在忽略大小写时匹配的名字也接受，给出警告 `DSH5276` —— `UE.texcoord()` 就是 `UE.TexCoord()` |
| 参数名 | 先去首尾空白，再按同样规则匹配；除此之外必须精确 —— `Un_Mirror_U` 不是 `UnMirrorU` |

每个名字都是目录里的一个类：它的短名，或目录带着的 1.x 别名之一 —— `TexCoord`、`ObjectPosition`、
`CameraVector`、`CameraPosition`、`ReflectionVector`、`ViewportUV`、`TransformVector`。

<a id="看目录之前先读这两条"></a>

## 看目录之前先读这两条

**警告：**

**1.x 名字不读的参数会被丢弃 —— 现在会给警告。** 对[完整名单](#完整名单)里的 27 个名字，legacy 前端保留
下面各条目的参数表，其余参数 —— 未知名字、拼写错误、位置参数 —— 一律丢弃并给出 `DSH5254`
（自 2.0.0 起）（1.x 不声不响地丢）。`UE.TexCoord(Indx = 3)` 仍然产生 UV channel 0，但会说明原因；
`UE.Panner(SpedX = 1)` 按节点默认速度平移，也会说明。唯一保留的位置参数是 `UE.TransformVector` 和
`UE.TransformPosition` 的位置 0（→ `Input`）；`UE.StaticSwitchParameter` 在位置 0 和 1 读 `True` 与
`False`。通过了过滤的参数，和任何节点调用的参数一样按目录绑定。

**提示：**

**两个相同的调用就是一个节点**，不论哪个内置 （自 2.0.0 起）：IR 会合并类、属性和输入都相同的节点。
1.x 只对通用调用和 `Substrate.*` 调用去重，在五个地方写 `UE.TexCoord(Index = 0)` 会生成五个节点；现在只有一个。
参见[节点复用](../graph/expressions.md#节点复用)。

<a id="语法概要"></a>

## 语法概要

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
// 表达式形式，写在 Graph 中
UE.<Name> ( [ <argument> [ , <argument> ] … ] )

<argument> := <arg-name> = <expression>

// 声明形式，写在 Properties 中
UE.<Name> [ ( <key> = <value> [ , <key> = <value> ] … ) ] <property-name> ;
```

<a id="输出宽度"></a>

## 输出宽度

调用的类型取自目录 —— 引擎在当前版本上对这个类的描述：

- 带掩码的输出和它的掩码一样宽；
- 不带掩码的输出取引擎的值类型；引擎只说「一个 float」时，用 DreamShader 为这个类记下的宽度：1 —— `Time`、
  `PixelDepth`、`SceneDepth`、`ObjectRadius`、`TwoSidedSign`、`PerInstanceRandom`、`PerInstanceFadeAmount`；
  2 —— `TextureCoordinate`、`Panner`；3 —— `ObjectPositionWS`、`ObjectBounds`、`CameraVectorWS`、
  `CameraPositionWS`、`ReflectionVectorWS`、`VertexNormalWS`、`VertexTangentWS`、`PixelNormalWS`、`Transform`、
  `TransformPosition`；
- 引擎完全没给类型的输出，和它被读入的位置一样宽；
- 多输出的节点要么指名读其中一个（`UE.ScreenPosition().ViewportUV`），要么在它的第一个输出 —— 或各通道输出拼成
  的整个值（`float4 VC = UE.VertexColor();`）—— 恰好合适的地方整体使用。1.x 源码里，第一个输出没有固定宽度的
  节点读作那个输出（`DSH5287`）；直接传给 pin 的任何节点也一样 —— `UE.TransformPosition(UE.WorldPosition(), …)`
  取 `XYZ`，和 1.x 的连法一致 （自 2.1.1 起）（2.0.0 – 2.1.0 对第一个输出宽度固定的节点报 `DSH5201`）。

这些内置都不会产生纹理对象、`MaterialAttributes` 值或 Substrate 值。

<a id="完整名单"></a>

## 完整名单

全部 27 个 1.x 名字，按 1.x 的注册顺序。*输出*一列是 UE 5.8 目录给出的。类名省略了 `UMaterialExpression` 前缀。

| 内置节点 | `UMaterialExpression` 类 | 输出 | 1.x 参数 |
| :-- | :-- | :-- | :-- |
| `UE.TexCoord` | `TextureCoordinate` | `float2` | `Index`、`UTiling`、`VTiling`、`UnMirrorU`、`UnMirrorV` |
| `UE.Time` | `Time` | `float1` | `Period`、`IgnorePause` |
| `UE.Panner` | `Panner` | `float2` | `Coordinate`、`Time`、`Speed`、`SpeedX`、`SpeedY`、`FractionalPart` |
| `UE.WorldPosition` | `WorldPosition` | `XYZ`（`float3`），另有 `XY`、`Z` | 无 |
| `UE.ObjectPositionWS` | `ObjectPositionWS` | `float3` | 无 |
| `UE.CameraVectorWS` | `CameraVectorWS` | `float3` | 无 |
| `UE.VertexNormalWS` | `VertexNormalWS` | `float3` | 无 |
| `UE.VertexTangentWS` | `VertexTangentWS` | `float3` | 无 |
| `UE.ScreenPosition` | `ScreenPosition` | `ViewportUV`、`PixelPosition` | 无 |
| `UE.VertexColor` | `VertexColor` | `RGB`、`R`、`G`、`B`、`A`；整体是 `float4` | 无 |
| `UE.PixelDepth` | `PixelDepth` | `float1` | 无 |
| `UE.SceneDepth` | `SceneDepth` | `float1` | 无 |
| `UE.SceneColor` | `SceneColor` | `RGB`、`A`；整体是 `float4` | 无 |
| `UE.TranslatedWorldPosition` | `WorldPosition`，相机相对 | 同 `UE.WorldPosition` | 无 |
| `UE.ObjectPosition` | `ObjectPositionWS` | `float3` | 无 |
| `UE.ObjectRadius` | `ObjectRadius` | `float1` | 无 |
| `UE.ObjectBounds` | `ObjectBounds` | `float3` | 无 |
| `UE.CameraVector` | `CameraVectorWS` | `float3` | 无 |
| `UE.CameraPosition` | `CameraPositionWS` | `float3` | 无 |
| `UE.ReflectionVector` | `ReflectionVectorWS` | `float3` | 无 |
| `UE.PixelNormalWS` | `PixelNormalWS` | `float3` | 无 |
| `UE.TwoSidedSign` | `TwoSidedSign` | `float1` | 无 |
| `UE.PerInstanceRandom` | `PerInstanceRandom` | `float1` | 无 |
| `UE.PerInstanceFadeAmount` | `PerInstanceFadeAmount` | `float1` | 无 |
| `UE.ViewportUV` | `ScreenPosition` | 同 `UE.ScreenPosition` | 无 |
| `UE.TransformVector` | `Transform` | `float3` | **`Input`**、`Source`、`Destination` |
| `UE.TransformPosition` | `TransformPosition` | `float3` | **`Input`**、`Source`、`Destination`、`PeriodicWorldTileSize`、`FirstPersonInterpolationAlpha` |

**提示：**

1.x 只在当前编辑器能解析出对应类时才注册 `UE.ObjectPositionWS`、`UE.ObjectPosition`、`UE.ScreenPosition` 和
`UE.ViewportUV`。目录列出运行中引擎的每一个类，所以不再有条件注册 （自 2.0.0 起）。

<a id="别名"></a>

### 别名

名单里有四个名字是已有条目的第二种写法，创建的节点类相同。

| 别名 | 等同于 | 区别 |
| :-- | :-- | :-- |
| `UE.ObjectPosition` | `UE.ObjectPositionWS` | 无 |
| `UE.CameraVector` | `UE.CameraVectorWS` | 无 |
| `UE.ViewportUV` | `UE.ScreenPosition` | 无 —— 引擎没有专门的 ViewportUV 类，*ViewportUV* 是 `ScreenPosition` 的第一个输出 |
| `UE.TranslatedWorldPosition` | `UE.WorldPosition` | 前端把调用改写成 `UE.WorldPosition(WorldPositionShaderOffset = CameraRelative)`，而节点默认是绝对世界位置；目录里没有这个名字的类 |

**提示：**

2.0.0 – 2.1.0 把 `UE.TranslatedWorldPosition` 的值写成 `WPT_CameraRelative` —— 结果没错，但每次调用都带一条
`DSH5278` 警告。（自 2.1.1 起） 起不再警告。

<a id="带参数的内置节点"></a>

## 带参数的内置节点

<a id="uetexcoord"></a>

### UE.TexCoord

```c
float2 uv    = UE.TexCoord(Index = 0);
float2 tiled = UE.TexCoord(Index = 0, UTiling = 2.0, VTiling = 2.0);
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Index` | 常量 —— 即属性 `CoordinateIndex` | 节点默认值，UV channel 0 | 否 |
| `UTiling` | 常量 | 节点默认值 | 否 |
| `VTiling` | 常量 | 节点默认值 | 否 |
| `UnMirrorU` | 常量 `true` / `false` | 节点默认值 | 否 |
| `UnMirrorV` | 常量 `true` / `false` | 节点默认值 | 否 |

`Index` 是目录给 `CoordinateIndex` 的别名。`CoordinateIndex` 本身不在 1.x 参数表上，所以在这个形式下它被
`DSH5254` 丢弃；[声明形式](#properties-声明形式)两者都接受。负的 `Index` 原样写入，不报诊断。

<a id="uetime"></a>

### UE.Time

```c
float t      = UE.Time();
float looped = UE.Time(Period = 2.0, IgnorePause = true);
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Period` | 常量 | 不写时节点的周期覆盖保持关闭 | 否 |
| `IgnorePause` | 常量 `true` / `false` —— 即属性 `bIgnorePause` | 节点默认值 | 否 |

写了 `Period` 也会打开节点的周期覆盖：前端会给调用加上 `bOverride_Period = true`。不做范围检查，负的 `Period`
原样写入 —— 声明形式也一样 （自 2.0.0 起）。

<a id="uepanner"></a>

### UE.Panner

```c
float2 panned = UE.Panner(Coordinate = UE.TexCoord(Index = 0),
                          Time       = UE.Time(),
                          SpeedX     = 0.1,
                          SpeedY     = 0.0);
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| `Coordinate` | 输入 pin | 未连接 | 否 |
| `Time` | 输入 pin | 未连接 | 否 |
| `Speed` | 输入 pin | 未连接 | 否 |
| `SpeedX` | 常量 | 节点默认值 | 否 |
| `SpeedY` | 常量 | 节点默认值 | 否 |
| `FractionalPart` | 常量 `true` / `false` —— 即属性 `bFractionalPart` | 节点默认值 | 否 |

`ConstCoordinate` 在这个形式下被 `DSH5254` 丢弃；声明形式会传下去，按类名调用也会：
`UE.Expression(Class = "Panner", ConstCoordinate = 1)`。

<a id="变换内置"></a>

## 变换内置

改变值所在坐标系的两个内置：`UE.TransformVector` 用于**方向**，`UE.TransformPosition` 用于**点**。两者输出都是
`float3`，`Input` 都必需，可以按名字给，也可以作为位置 0 给。

坐标系是引擎枚举的一个值，写的时候不带前缀：`"World"` 或裸词 `World`。`.dss` 里精确匹配。1.x 源码里忽略大小写、
空格、`_` 和 `-`，也可以写枚举前缀或作用域，并给出警告 `DSH5278`：`"world"`、`" WORLD "` 和 `TRANSFORMSOURCE_World`
都是 `World`。枚举里没有的值报 `DSH5215`，消息会指出出错的属性 —— `Source` 对应 `TransformSourceType`，
`Destination` 对应 `TransformType`。

**警告：**

**`Source` 和 `Destination` 一定要写全名。** 这两个名字只读 1.x 参数表里的参数，其他一律被 `DSH5254` 丢弃：
`UE.TransformVector(V, Src = "World")` 用的是默认 `Source`（`Tangent`），因为参数拼成了 `Src`；第二个位置参数 ——
`UE.TransformVector(V, "World")` —— 也会被丢弃，只有位置 0 会被读取。没给 `Input`（名字和位置 0 都没有）是警告
`DSH5279`，pin 保持未连接 （自 2.0.0 起）（1.x 拒绝该调用）。

<a id="uetransformvector"></a>

### UE.TransformVector

在不同坐标系之间变换**方向**。只做旋转、不做平移，这也正是它不适合用于点的原因。

```c
vec3 worldNormal = UE.TransformVector(LocalNormal, Source = "Tangent", Destination = "World");
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Input`** | 输入 pin；也可以作为位置 0 给出 | —— | **是** |
| `Source` | 枚举值 —— 即属性 `TransformSourceType` | `Tangent`（节点的） | 否 |
| `Destination` | 枚举值 —— 即属性 `TransformType` | `World`（节点的） | 否 |

两侧接受同样的六个写法：

| 写法 | `EMaterialVectorCoordTransformSource` | `EMaterialVectorCoordTransform` |
| :-- | :-- | :-- |
| `Tangent` | `TRANSFORMSOURCE_Tangent` | `TRANSFORM_Tangent` |
| `Local` | `TRANSFORMSOURCE_Local` | `TRANSFORM_Local` |
| `World` | `TRANSFORMSOURCE_World` | `TRANSFORM_World` |
| `View` | `TRANSFORMSOURCE_View` | `TRANSFORM_View` |
| `Camera` | `TRANSFORMSOURCE_Camera` | `TRANSFORM_Camera` |
| `Instance` | `TRANSFORMSOURCE_Instance` | `TRANSFORM_Instance` |

1.x 源码还可以在任一侧写 `AbsoluteWorld`（即 `World`），以及 `Particle` 或 `InstanceParticle`（即 `Instance`），
大小写不限：前端写成引擎的名字，并给出警告 `DSH5278` （自 2.1.1 起）（2.0.0 – 2.1.0 对它们报 `DSH5215`）。
引擎自己的 `ParticleWorld` 值是隐藏的，目录里没有。

<a id="uetransformposition"></a>

### UE.TransformPosition

在不同坐标系之间变换**点**，会应用平移。

```c
vec3 viewP = UE.TransformPosition(UE.WorldPosition(),
                                  Source      = "World",
                                  Destination = "TranslatedWorld");
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Input`** | 输入 pin；也可以作为位置 0 给出 | —— | **是** |
| `Source` | 枚举值 —— 即属性 `TransformSourceType` | `Local`（节点的） | 否 |
| `Destination` | 枚举值 —— 即属性 `TransformType` | `World`，和 1.x 一样 —— 不是节点的 `Local` | 否 |
| `PeriodicWorldTileSize` | 输入 pin，引擎的类有这个 pin 时 | 未连接 | 否 |
| `FirstPersonInterpolationAlpha` | 输入 pin，引擎的类有这个 pin 时 | 未连接 | 否 |

**警告：**

**节点自己的 `Destination` 是 `Local`。** 1.x 写的是 `World`，前端现在也这么写 （自 2.1.1 起）：不写
`Destination` 的调用变换到世界空间，[`dsc migrate`](../tools/migrate.md) 会把 `Destination = World` 写出来，
因为 `.dss` 调用拿到的是节点的 `Local`。2.0.0 – 2.1.0 不写默认值，这样的调用变成一个什么都不变的变换。

`Source` 和 `Destination` 共用一个枚举：

| 写法 | `EMaterialPositionTransformSource` |
| :-- | :-- |
| `Local` | `TRANSFORMPOSSOURCE_Local` |
| `World` | `TRANSFORMPOSSOURCE_World` |
| `PeriodicWorld` | `TRANSFORMPOSSOURCE_PeriodicWorld` —— 引擎枚举有它时 |
| `TranslatedWorld` | `TRANSFORMPOSSOURCE_TranslatedWorld` |
| `FirstPersonTranslatedWorld` | `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` —— 引擎枚举有它时 |
| `View` | `TRANSFORMPOSSOURCE_View` |
| `Camera` | `TRANSFORMPOSSOURCE_Camera` |
| `Instance` | `TRANSFORMPOSSOURCE_Instance` |

1.x 源码还可以写 `AbsoluteWorld`（`World`）、`CameraRelativeWorld`（`TranslatedWorld`）、`FirstPerson`
（`FirstPersonTranslatedWorld`）、`Particle` 和 `InstanceParticle`（`Instance`），大小写不限：前端写成引擎的名字，
并给出警告 `DSH5278` （自 2.1.1 起）（2.0.0 – 2.1.0 报 `DSH5215`）。引擎把 `FirstPersonTranslatedWorld`
标为不能作 *Source*，DreamShader 不检查这一点。

**警告：**

**运行中的引擎没有的坐标系报成「未知值」，而不是版本错误。** 在枚举里没有 `PeriodicWorld` 的引擎上，
`UE.TransformPosition(P, Destination = "PeriodicWorld")` 报 `DSH5215` —— 和拼写错误同一个码。

**两个可选 pin 跟随引擎，而且行为一致。** 引擎的类没有 `PeriodicWorldTileSize` 或
`FirstPersonInterpolationAlpha` 时，这个参数就不是节点的 pin：1.x 源码以提示 `DSH5291` 保留它，建节点时以
`DSH8212` 失败 （自 2.0.0 起）（1.x 静默丢弃前者、对后者报自己的错误）。要让一份源码在多个引擎版本上都能
构建，用 `#if DS_ENGINE_MINOR >= …` 把参数包起来 —— 见[预处理器](../language/lexical.md#预处理器)。

<a id="不带参数的内置节点"></a>

## 不带参数的内置节点

27 个里有 22 个没有自己的参数；给出的参数都被 `DSH5254` 丢弃，节点的输入 pin 全部保持未连接。

<a id="位置与几何"></a>

### 位置与几何

| 调用 | 输出 | 说明 |
| :-- | :-- | :-- |
| `UE.WorldPosition()` | `XYZ`（`float3`），另有 `XY`、`Z` | 绝对世界位置；节点的 shader-offset 模式保持默认 |
| `UE.TranslatedWorldPosition()` | 同上 | 同一个节点，相机相对模式；等价于 HLSL 的 `GetTranslatedWorldPosition(Parameters)` |
| `UE.ObjectPositionWS()` | `float3` | —— |
| `UE.ObjectPosition()` | `float3` | `UE.ObjectPositionWS` 的别名 |
| `UE.ObjectRadius()` | `float1` | —— |
| `UE.ObjectBounds()` | `float3` | —— |
| `UE.CameraPosition()` | `float3` | `CameraPositionWS` |

`WorldPosition` 的其他 shader-offset 模式按类名调用：
`UE.Expression(Class = "WorldPosition", WorldPositionShaderOffset = …)`。[声明形式](#properties-声明形式)接受
`ShaderOffsets`。

<a id="向量与法线"></a>

### 向量与法线

| 调用 | 输出 | 说明 |
| :-- | :-- | :-- |
| `UE.CameraVectorWS()` | `float3` | —— |
| `UE.CameraVector()` | `float3` | `UE.CameraVectorWS` 的别名 |
| `UE.VertexNormalWS()` | `float3` | —— |
| `UE.VertexTangentWS()` | `float3` | —— |
| `UE.PixelNormalWS()` | `float3` | —— |
| `UE.ReflectionVector()` | `float3` | `ReflectionVectorWS` |
| `UE.TwoSidedSign()` | `float1` | —— |

<a id="屏幕与场景"></a>

### 屏幕与场景

| 调用 | 输出 | 说明 |
| :-- | :-- | :-- |
| `UE.ScreenPosition()` | `ViewportUV`、`PixelPosition` | 两个输出引擎都没给类型：`UE.ScreenPosition().ViewportUV` 和它被读入的位置一样宽。1.x 源码里当普通值使用时是第一个输出 *ViewportUV*（`DSH5287`） |
| `UE.ViewportUV()` | 同 `UE.ScreenPosition` | 别名 |
| `UE.PixelDepth()` | `float1` | 当前像素的场景读取；不接 UV 也不接 offset 输入 |
| `UE.SceneDepth()` | `float1` | 节点的 UV 输入保持未连接 |
| `UE.SceneColor()` | `RGB`、`A`；整体 `float4` | —— |

要给场景深度读取接一个 UV，按类名调用：`UE.Expression(Class = "SceneDepth", Input = <uv>)`。

<a id="逐顶点与逐实例数据"></a>

### 逐顶点与逐实例数据

| 调用 | 输出 |
| :-- | :-- |
| `UE.VertexColor()` | `RGB`、`R`、`G`、`B`、`A`；整体 `float4` |
| `UE.PerInstanceRandom()` | `float1` |
| `UE.PerInstanceFadeAmount()` | `float1` |

`VertexColor` 的输出都是同一个颜色的通道视图：`float4 c = UE.VertexColor();` 是整个颜色，
`UE.VertexColor().a` 是 `A` 输出。

<a id="特殊处理的内置节点"></a>

## 特殊处理的内置节点

`UE.StaticSwitchParameter` 和 `UE.SceneTexture` 在绑定器看到之前被 legacy 前端改写成 `UE.Expression` 调用。
`UE.CollectionParam` 不改写：目录把它当作 `UE.CollectionParameter` 的第二个名字。

<a id="uestaticswitchparameter"></a>

### UE.StaticSwitchParameter

（自 1.2.3 起）

创建一个会出现在材质实例编辑器里的静态开关，美术不用改源码就能切换分支。

```c
vec3 color = UE.StaticSwitchParameter(Name        = "UseDetail",
                                      True        = detailColor,
                                      False       = baseColor,
                                      Default     = true,
                                      Group       = "Switches",
                                      Description = "Choose the detail branch");
```

| 参数 | 别名 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- | :-- |
| **`Name`** | `ParameterName` | 带引号的字符串或一个词，去空白后非空 | —— | **是** |
| **`True`** | `A`、位置 0 | 值 | —— | **是** |
| **`False`** | `B`、位置 1 | 值 | —— | **是** |
| `Default` | `DefaultValue` | `true` / `false` | `false` | 否 |
| `Group` | —— | 带引号的字符串或一个词 | 无 | 否 |
| `Description` | —— | 带引号的字符串或一个词 | 无 | 否 |
| `SortPriority` | —— | 整数 | 节点默认值 | 否 |

调用变成

```c
UE.Expression(Class = "StaticSwitchParameter", ParameterName = …, DefaultValue = …, Group = …,
              Desc = …, SortPriority = …, A = <True>, B = <False>)
```

其他参数被 `DSH5254` 丢弃。

输出跟随分支：和较宽的数值分支一样宽，或者是某个分支携带的 Substrate / `MaterialAttributes` 值。两条分支**不再**
互相比较 （自 2.0.0 起）；任一分支是纹理报 `DSH5214`。参数以「仅编辑器」的静态开关值注册到生成的材质 ——
或材质函数 —— 上，因此会出现在实例编辑器里。参数相同的两次调用是一个节点。

**提示：**

`Group` 和 `Description` 写入节点的 `Group` 和 `Desc` 属性；值既不是带引号的字符串也不是一个词时报 `DSH5224`
（自 2.0.0 起）（1.x 不报诊断直接丢弃）。

<a id="uecollectionparam"></a>

### UE.CollectionParam

（自 1.2.3 起）

从 `UMaterialParameterCollection` 中读取标量或向量参数。

```c
float wind = UE.CollectionParam(Collection = Path(Game, "Collections/MPC_Wind"),
                                Parameter  = "WindStrength");
```

`UE.CollectionParam` 和引擎的名字 `UE.CollectionParameter` 是同一个类，在 `Graph` 体和[声明形式](#properties-声明形式)
里都一样 （自 2.1.1 起）。2.0.0 – 2.1.0 在 `Graph` 体里对 `UE.CollectionParam` 报 `DSH5210`。

| 参数 | 别名 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- | :-- |
| **`Collection`** | `Asset` | `Path(…)` 或 Unreal object path | —— | **是** |
| **`Parameter`** | `ParameterName` | 带引号的字符串或一个词 | —— | **是** |
| `Group` | —— | 带引号的字符串或一个词，引擎的类有它时 | 无 | 否 |
| `SortPriority` | —— | 整数，引擎的类有它时 | 节点默认值 | 否 |
| `Desc` | —— | 带引号的字符串或一个词 | 无 | 否 |

建节点时加载集合并按名字查参数：集合解析不了或加载不了报 `DSH8213`，名字既不是集合的标量参数也不是向量参数报
`DSH8254`。不写 `Collection` 或 `Parameter` 的调用不做检查 （自 2.0.0 起）。`Description` 不是这个调用的
参数 —— 节点的描述属性叫 `Desc` —— 写了报 `DSH5213`。

引擎没给节点的输出类型，所以值和它被读入的位置一样宽 （自 2.0.0 起）（1.x 从加载的集合读宽度：向量参数
`float4`、标量参数 `float1`）。声明形式除非另行指定，否则声明为 `float4`。

**警告：**

`Group` 和 `SortPriority` 只在运行中引擎的类有它们时才接受。类没有时，`Group` 报 `DSH5213`，`SortPriority` ——
一个数字 —— 以 `DSH5291` 保留、建节点时以 `DSH8212` 失败 （自 2.0.0 起）（1.x 在版本门槛以下静默丢弃两者）。

`Path(…)` 的语法和可用根目录见 [Path 资产引用](../parameters/path.md)。

<a id="uescenetexture"></a>

### UE.SceneTexture

纯语法糖。

```c
float4 scene = UE.SceneTexture(Id = "PostProcessInput0");
```

| 参数 | 类型 | 默认值 | 必需 |
| :-- | :-- | :-- | :-- |
| **`Id`** | 枚举值 | —— | **是** |

前端把调用改写成

```c
UE.Expression(Class = "SceneTexture", SceneTextureId = <Id>)[0]
```

—— `UMaterialExpressionSceneTexture` 的输出 0，*Color*，一个 `float4`。调用必须**恰好**有一个参数，且名字必须是
`Id`；其他任何写法（包括输出选择器）都报 `DSH5255`。节点的其他属性和它的 `Size` / `InvSize` 输出要用
`UE.Expression(Class = "SceneTexture", …)`。

`Id` 是 `ESceneTextureId` 的值，不带前缀 —— `"PostProcessInput0"`。1.x 源码里也接受带前缀的写法
（`"PPI_PostProcessInput0"`）、`ESceneTextureId::` 作用域和其他大小写，并给出 `DSH5278`；显示名或带空格的写法
（`"ppi postprocessinput0"`）报 `DSH5215` （自 2.0.0 起）。见[枚举值](ue-expression.md#枚举值)。

<a id="properties-声明形式"></a>

## Properties 声明形式

`UE.<Name>(…)` 也可以出现在 [`Properties`](../language/sections.md#properties) 段中类型 token 的位置。它不声明
参数：legacy 前端保留这段文本，并在每个读取该 property 的 `Graph` 体开头，声明一个同名局部变量、用这个调用初始化 ——
`float2 UV = UE.TexCoord(Index = 0);` —— 每个这样的体一个，按 property 的顺序。不读它的体不会得到节点。从那以后
这个调用就是普通的 `UE.` 调用，按目录绑定 （自 2.0.0 起）。

```c
Properties {
    UE.TexCoord(Index = 0) UV;
    UE.CollectionParam(Collection = Path(Game, "MPC_Weather"), Parameter = "Wind") Wind;
}

Graph {
    Color = vec3(UV.x, UV.y, Wind);
}
```

| 规则 | 声明形式 | 表达式形式 |
| :-- | :-- | :-- |
| `UE` | 任意大小写 | 精确 |
| 参数语法 | 在顶层逗号处拆开的 `Key = Value` 对；值是带引号的字符串、数字、`true` / `false`、裸词（读作名字：另一个变量或枚举值），其余保留为文本（如 `Path(…)`） | 完整表达式，允许嵌套调用 |
| 1.x 参数过滤（`DSH5254`） | 不应用 | 对 27 个名字应用 |
| 未知参数 | `DSH5213`；数字以 `DSH5291` 保留、建节点时以 `DSH8212` 失败 | 27 个名字上被 `DSH5254` 丢弃 |
| 重复参数，或同一参数的两种写法（`Index` 与 `CoordinateIndex`） | `DSH4215` | 相同 |
| 没有 `=` 的片段、空键或空值 | 跳过，不报诊断 | —— |
| 行内默认值（`= …`） | `DSH3259` | 不适用 |
| 元数据 `[ … ]` | 读取但不应用 | 不适用 |
| 改名的参数 | `Description` → `Desc`；`CollectionParam` / `CollectionParameter` 上 `Asset` → `Collection`、`Parameter` → `ParameterName`，调用是 `UE.CollectionParameter` | 无 |
| `OutputType` / `ResultType` | 局部变量的类型，不往下传 —— 见[声明的输出宽度](#声明的输出宽度) | 见 [OutputType](ue-expression.md#outputtype) |

最容易踩的是行内默认值。参数要写在括号里：

```c
// 正确
UE.TexCoord(Index = 0) UV;

// 错误：DSH3259
UE.TexCoord UV = 0;
```

<a id="可识别的名字"></a>

### 可识别的名字

| `UE.Name` | 节点类 | 参数 | 与表达式形式的差异 |
| :-- | :-- | :-- | :-- |
| `TexCoord` | `TextureCoordinate` | `Index` 或 `CoordinateIndex`、`UTiling`、`VTiling`、`UnMirrorU`、`UnMirrorV` | 接受 `CoordinateIndex`；负索引不检查 （自 2.0.0 起） |
| `Time` | `Time` | `Period`、`IgnorePause` | `Period` 不做范围检查，也**不会**打开周期覆盖 —— 要写 `bOverride_Period = true` （自 2.0.0 起） |
| `Panner` | `Panner` | `Coordinate`、`Time`、`Speed`（pin：词是另一个变量，数字是 pin 上的常量）、`ConstCoordinate`、`SpeedX`、`SpeedY`、`FractionalPart` | 接受 `ConstCoordinate` |
| `WorldPosition` | `WorldPosition` | `ShaderOffsets` —— 即属性 `WorldPositionShaderOffset` | 接受 `ShaderOffsets` |
| `ObjectPositionWS` | `ObjectPositionWS` | `Origin` —— 即属性 `OriginType` | 接受 `Origin` |
| `CameraVectorWS`、`ScreenPosition`、`VertexColor` | 同名 | 没有自己的参数 | 任何参数都是未知参数 |
| `CollectionParam`、`CollectionParameter` | `CollectionParameter` | `Collection` / `Asset`、`Parameter` / `ParameterName`，类有时还有 `Group`、`SortPriority`、`Description` | |
| *其他任何名字* | 同名的类 | 它的 pin 和属性 | 不需要 `OutputType` （自 2.0.0 起） |

`ShaderOffsets` 取 `EWorldPositionIncludedOffsets` 不带前缀的值 —— `Default`、`ExcludeAllShaderOffsets`、
`CameraRelative`、`CameraRelativeNoOffsets` —— 其他大小写或带 `WPT_` 前缀时给出 `DSH5278`。`Origin` 取
`Absolute` 和 `CameraRelative`。不是枚举值的 1.x 写法（`ShaderOffsets` 的 `IncludingShaderOffsets`、`Absolute`、
`NoOffsets`，`Origin` 的 `World` 等）报 `DSH5215` （自 2.0.0 起）。

<a id="声明的输出宽度"></a>

### 声明的输出宽度

局部变量的类型取下列第一个适用的来源：

1. 指定类型的 `OutputType` 或 `ResultType` 参数 —— 见 [OutputType](ue-expression.md#outputtype)；
2. 名字（忽略大小写）：`TexCoord`、`Panner` → `float2`；`Time` → `float`；`WorldPosition`、`CameraVectorWS`、
   `ObjectPositionWS`、`VertexNormalWS`、`VertexTangentWS` → `float3`；
3. 其他名字 → `float4`：`ScreenPosition`、`VertexColor`、`CollectionParam` / `CollectionParameter`，以及名单外的
   任何名字 （自 2.0.0 起）（1.x 对名单外又没写 `OutputType` 的名字直接拒绝）。

调用像任何初始化式一样初始化局部变量：标量输出铺满局部变量的宽度，更宽的输出被截短（`DSH5289`），更窄的向量 ——
名单外名字上的 `float3` 节点 —— 报 `DSH4228`。引擎没给类型的输出（如集合参数）取局部变量的宽度。给这样的声明写上
`OutputType`；标量集合参数就是 `OutputType = float1`。

`UE.*` 声明前面的 `const` 会被读取然后忽略 （自 2.0.0 起）（1.x 拒绝）。

<a id="一些值得知道的细节"></a>

## 一些值得知道的细节

- `UE.*` 调用可以直接 swizzle —— `UE.TexCoord(Index = 0).x` 就是普通的后缀链。在通道输出节点上，某个输出发布的
  swizzle 就是那个输出（`UE.VertexColor().a`）。参见[运算符与转换](../graph/expressions.md#swizzle)。
- 名为 `TexCoord` 的 property、局部变量或参数不会遮蔽 `UE.TexCoord`；名为 `UE` 的会 —— 这时 `UE.X(…)` 是对那个
  变量的成员调用 （自 2.0.0 起）。
- 在 27 个名字上，`OutputType`、`ResultType` 和 `Class` 与任何不读的参数一样被 `DSH5254` 丢弃。`Output`、
  `OutputName` 和 `OutputIndex` 选择节点的一个输出，和其他所有调用一样 （自 2.0.0 起）。
- 在 [`GraphFunction`](../language/functions.md#graphfunction-与-ue-提升) 内部，`UE.*` 调用会被提升出 HLSL 体，
  变成生成的 Custom 节点上的输入 pin。
- 插件附带一个 HLSL 头文件 `/Plugin/DreamShader/DreamShaderBuiltins.ush`，其中的 `DS_*` 宏和函数与本页大多数节点
  对应（`DS_TIME`、`DS_WorldPosition(Parameters)`……）。插件自己从不 include 它，它只在手写的
  [`Function`](../language/functions.md) HLSL 体里有用；要把 `#include` 写在函数体最前面。`DS_TexCoord` 和
  `DS_VertexColor` 依赖如今已没人安排的翻译副作用，请改用 `UE.TexCoord` 和 `UE.VertexColor`。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5254: '{0}' takes no positional argument here; 1.x ignored it, so it is dropped.` | 警告：27 个名字之一不读的参数 —— 未知、拼错或位置参数 —— 被丢弃。 | 对照条目检查拼写。 |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告，仅 1.x：内置名、参数名或输出名只在忽略大小写时匹配。 | 按引擎的拼写写。 |
| `DSH4200: '{0}' is not declared; did you mean '{1}'? Names are case-sensitive.` | 命名空间没写成 UE。 |  |
| `DSH5210: '{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive.` | UE. 之后的名字不是运行中引擎的类。 |  [详解](ue-expression.md) |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 通过了过滤的参数不是该类的任何 pin 或属性；或 CollectionParameter 上写了 Description。 |  |
| `DSH5291: '{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties.` | 提示，仅 1.x：这样的参数带着值，被保留下来，等建好的节点按名字连接。 |  |
| `DSH8212: '{0}' has no input pin named '{1}'.` | 建好的节点没有以 DSH5291 保留的那个名字的 pin —— 比如当前引擎的类没有 PeriodicWorldTileSize。 | 用 #if DS\_ENGINE\_MINOR 把参数包起来。 |
| `DSH5224: '{0}' takes a name or an asset path; write it as a quoted string.` | 属性收到的不是常量，或文本属性收到的既不是带引号的字符串也不是一个词。 |  |
| `DSH5215: '{0}' is not a value of '{1}' on '{2}'.` | 枚举值 —— 坐标系、scene texture id —— 不是引擎枚举的值，包括当前引擎没有的坐标系。 | 写枚举值不带前缀的名字。 |
| `DSH5278: '{0}' is the 1.x name of the '{1}' space of '{2}'; a '.dss' writes '{1}'.` | 警告：枚举值按 1.x 方式书写 —— 其他大小写、带前缀，或只有 1.x 才有的名字，如 AbsoluteWorld。 | 写引擎的名字；dsc migrate 会替你写。 |
| `DSH5214: {0} expects {1}, and this is {2}.` | 值放不进它连接的 pin —— 比如静态开关的某个分支是纹理。 |  |
| `DSH4215: The '{0}' argument of '{1}' is given twice.` | 同一个 pin 或属性给了两次。 |  |
| `DSH5279: '{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.` | 警告：必需的 pin —— 变换的 Input —— 没有连接。 | 用 Input = … 或位置 0 传值。 |
| `DSH5287: '{0}' has more than one output and is read as its first, '{1}', which is what 1.x did; a '.dss' names the output.` | 提示，仅 1.x：多输出节点当普通值使用，读作第一个输出。 |  |
| `DSH5201: '{0}.{1}' has no output called '{2}'; its outputs are {3}.` | 选择的输出名不存在。 |  |
| `DSH5255: Expected 'UE.SceneTexture' to take exactly one argument, 'Id = ...', found {0} argument(s).` | UE.SceneTexture 的参数不是恰好一个 Id。 |  |
| `DSH5257: Expected 'UE.StaticSwitchParameter' to name its parameter with 'Name = "..."', found no name.` | 没写 Name 或 ParameterName，或为空。 |  |
| `DSH5258: Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.` | 缺一条分支、两条分支是同一个参数，或带了输出选择器。 |  |
| `DSH5263: Expected 'Default' of 'UE.StaticSwitchParameter' to be true or false, found '{0}'.` | Default / DefaultValue 不是 true 或 false。 |  |
| `DSH5264: Expected 'SortPriority' of 'UE.StaticSwitchParameter' to be a whole number, found '{0}'.` | SortPriority 不是整数。 |  |
| `DSH8213: '{0}' could not take '{1}' for its '{2}' property. {3}` | 节点无法接受属性值 —— 包括集合解析不了、加载不了或不是 MaterialParameterCollection。 |  [详解](../parameters/path.md) |
| `DSH8254: The material parameter collection '{0}' has no parameter called '{1}'.` | 集合里没有这个名字的标量或向量参数。 |  |
| `DSH8214: Failed to create a '{0}' node.` | 节点创建失败。 |  |
| `DSH3259: Expected ')' to close the arguments of 'UE.{0}', found {1}.` | 声明形式：属性名之后跟了默认值，或参数列表没有闭合。 | 把参数写进 UE.Name(…) 的括号里。 |
| `DSH3250: Expected a property type and a name, found {0}.` | 声明形式：内置之后没有属性名。 |  |
| `DSH4228: {0} expects {1}, and this is {2}.` | 声明形式：局部变量的类型接不住节点的值 —— 比节点更窄的向量。 | 写 OutputType。 |

完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

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
        float2 uv   = UE.Panner(Coordinate = UV0, Time = UE.Time(), SpeedX = 0.1, SpeedY = 0.0);
        float3 wp   = UE.TranslatedWorldPosition();
        float3 nrm  = UE.TransformVector(UE.VertexNormalWS(), Source = "World", Destination = "Tangent");
        float  fade = UE.PerInstanceFadeAmount();
        float4 vcol = UE.VertexColor();

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

<a id="接下来"></a>

## 接下来

- [通用 UE.Expression](ue-expression.md) — 按类名调用任意引擎节点，以及 OutputType、输出选择和 Custom 节点。
  - [数学内置](math.md) — pow、lerp、saturate 等无前缀调用。
  - [Substrate 节点](substrate.md) — Substrate.* 命名空间，UE 5.4+。
  - [Path 资产引用](../parameters/path.md) — UE.CollectionParam 使用的 Path(...) 语法。
