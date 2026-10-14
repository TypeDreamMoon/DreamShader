---
title: 材质实例
description: .dsi —— 把材质实例写成源码。一个父材质、它覆盖的属性和它设置的参数，并对照父材质真实的参数表做检查。
---

一个 `.dsi` 文件就是一个 `UMaterialInstanceConstant`：一个父材质、几个被覆盖的属性，以及它设置的参数。
它没有图，所以没有函数、没有 `#include`、也没有 `#pragma material` —— 只有一条 pragma 和若干 `uniform` 行。

| 项目 | 值 |
| :-- | :-- |
| 产物 | 一个 `UMaterialInstanceConstant`，以文件名命名，落在同位置的 `.dss` 会放产物的地方 |
| 检查依据 | 父材质**真实**的参数 —— 名字、类型、是否静态 |
| 起始版本 | （自 2.0.0 起） |

```hlsl
// MI_HeroSkin.dsi
#pragma instance(Parent = "M_Skin", BlendMode = Masked, TwoSided = true)

uniform float Roughness = 0.35;
uniform float3 Tint = float3(1.0, 0.82, 0.76);

/// @static
uniform bool UseDetail = true;

/// @default /Game/Characters/Hero/T_Hero_Skin_D
uniform Texture2D Albedo;

/// @name Base Color Boost
uniform float BaseColorBoost = 1.2;
```

<a id="父材质"></a>

## 父材质

`Parent` 要么是一个**裸名字** —— 同一个源码根下由某个 DreamShader 源文件生成的材质或另一个实例 ——
要么是一个**对象路径**（`/Game/Materials/M_Skin`），指向任意材质或实例，不论是不是 DreamShader 生成的。

来自源码的父材质，如果资产不存在或比源码旧，会先被编译，所以构建实例时永远不会看到过期的参数表 ——
（自 2.1.2 起） 起整条祖先链都这样刷新。实例的实例可以用；成环的链会被拒绝。

<a id="覆盖"></a>

## 覆盖

一条覆盖就是一个 `uniform`，**按父材质声明该参数的方式来声明**，带上它要设成的值。

| 父参数 | 覆盖写法 |
| :-- | :-- |
| 标量 | `uniform float Gain = 2.0;` —— `int` 与 `bool` 参数也是标量 |
| 向量 | `uniform float3 Tint = float3(0, 1, 0);` —— `float3` 保留父材质的 alpha，`float4` 四个分量都设；父材质改了，没覆盖的分量跟着变 （自 2.1.2 起） |
| 静态开关 | `/// @static` + `uniform bool UseDetail = false;` |
| 静态分量遮罩 | `/// @static` + `uniform bool4 Mask = bool4(true, false, false, false);` |
| 纹理 | `/// @default <资产>` + `uniform Texture2D Albedo;` —— 不写初始值；`/// @default None` 表示清空。立方体、体积、数组纹理写 `TextureCube`、`Texture3D`、`Texture2DArray` |
| 字体、Runtime Virtual Texture、Sparse Volume Texture、纹理 / 参数集合 | `/// @default <资产>` + 对应类型的 `uniform` |

| 写错了什么 | 诊断码 |
| :-- | :-- |
| 父材质没有的名字 | `DSH7258` —— 只差大小写时是 `DSH7264`（引擎会悄悄什么都不设） |
| 类型与父材质不一致 | `DSH7259` |
| 编译器折不成常量的值 | `DSH7265` |
| `@group`、`@sort`、`@slider`、`@desc` —— 它们属于父材质 | `DSH7262`，警告 |
| 只存在于材质层或层混合上的参数 | `DSH7268` |

名字不是标识符的参数，用任意标识符声明，再加 `/// @name <真实名字>`。从文件里删掉的覆盖，
下次构建时也会从资产里删掉：实例是按文件**重建**的，不是打补丁。

<a id="从编辑器来回到编辑器去"></a>

## 从编辑器来，回到编辑器去

- Material Content Browser 的 **New Instance** 会在父材质源文件旁边写出一个 `.dsi`。
- `dsc decompile /Game/Materials/MI_Steel_Worn` 写出的 `.dsi` 只包含与父材质不同的部分。
- 被手改过的生成实例就是一次普通的[手改分歧](../generation/regeneration.md)；**Adopt Into Source**
  会逐个值地改写 `.dsi`，注释和顺序都保留。声明成 `bool`、`int` 或 `uint` 的标量，当前值装不下时改写成 `float`，
  装得下就保留原声明 （自 2.1.2 起）。
- （自 2.1.2 起） 反编译与 Adopt 不再丢东西：`TextureCollection` 引用、`double4` 的精度、立方体 / 体积 / 数组纹理都原样保留。
- （自 2.1.2 起） 生成的实例重新加载之后不再被当成手改：引擎加载时补上的覆盖项 `ExpressionGUID` 不计入摘要，
  其余字段（值、纹理）照旧计入，改了值仍然是手改。
