---
title: "材质实例 —— `.dsi`"
description: "材质实例 —— `.dsi`"
---

<a id="material-instances--dsi"></a>

# 材质实例 —— `.dsi`

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **材质实例**

一个 `.dsi` 文件就是一个 `UMaterialInstanceConstant`：一个父材质、几个被覆盖的属性，以及它设置的参数。它没有图，所以没有函数、没有 `#include`、也没有 `#pragma material` —— 只有一条 pragma 和若干 `uniform` 行。

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

资产以文件名命名，落在同位置的 `.dss` 会放其产物的地方。

<a id="the-parent"></a>

## 父材质

`Parent` 要么是

- **一个裸名字** —— 同一个源码根下由某个 DreamShader 源文件构建的材质或另一个实例（没有时为 `DSH8261`，有多个时为 `DSH8262`），要么是
- **一个对象路径**（`/Game/Materials/M_Skin`），指向任意材质或实例，不论是不是 DreamShader 的。

来自源码的父材质，如果其资产不存在或比源码旧，会先被编译（`DSH8264`），所以构建实例时永远不会看到过期的参数表。实例的实例可以用；成环的 `Parent` 链会被 `DSH8263`。

<a id="overrides"></a>

## 覆盖

一条覆盖就是一个 `uniform`，**按父材质声明该参数的方式来声明**，带上它要设成的值。文件会对照父材质真实的参数做检查：未知的名字是 `DSH7258`（只有大小写不同时是 `DSH7264` —— 引擎会悄悄什么都不设），类型不同则是 `DSH7259`。

| 父参数 | 覆盖 |
| :-- | :-- |
| 标量 | `uniform float Gain = 2.0;`（`int` 和 `bool` 参数也是标量：`uniform bool Lit = false;`） |
| 向量 | `uniform float3 Tint = float3(0, 1, 0);` —— `float3` 保留父材质的 alpha，`float4` 四个分量都设 |
| 静态开关 | `/// @static` + `uniform bool UseDetail = false;` |
| 静态分量遮罩 | `/// @static` + `uniform bool4 Mask = bool4(true, false, false, false);` |
| 纹理 | `/// @default <asset>` + `uniform Texture2D Albedo;` —— 不写初始值；`/// @default None` 表示清空 |
| 字体 | `/// @default <font asset>`、`/// @page <n>`、`uniform Font Glyphs;` |
| 运行时虚拟纹理、稀疏体积纹理、纹理集合、参数集合 | `/// @default <asset>` + `uniform RuntimeVirtualTexture Ground;`（以及 `SparseVolumeTexture`、`TextureCollection`、`ParameterCollection`） |

值必须是编译器能折成常量的东西 —— 字面量或字面量之间的算术运算（`DSH7265`）。名字不是标识符的参数，用任意标识符声明，再加 `/// @name <real name>`。元数据（`@group`、`@sort`、`@slider`、`@desc`）属于父材质，会被忽略并给一个警告（`DSH7262`）。

只有**全局**参数可以被设置。只存在于材质层或层混合上的参数是 `DSH7268`。

从文件里删掉的覆盖，下次构建时也会从资产里删掉：实例是按文件重建的，不是打补丁。

<a id="keys-of-pragma-instance"></a>

## `#pragma instance` 的键

除了 `Parent`，键就是实例可以覆盖的材质属性。设置一个键会写入该值**以及**引擎与之配对的覆盖标志；不再出现在文件里的键会回到“未覆盖”状态。

| 键 | 它们在引擎中的位置 |
| :-- | :-- |
| `BlendMode`、`ShadingModel`、`TwoSided`、`bIsThinSurface`、`DitheredLODTransition`、`OpacityMaskClipValue`、`bCastDynamicShadowAsMasked`、`bOutputTranslucentVelocity`、`bHasPixelAnimation`、`bEnableTessellation`、`DisplacementScaling`、`MaxWorldPositionOffsetDisplacement`、…… | `FMaterialInstanceBasePropertyOverrides` 的字段 —— 这个引擎版本有多少就是多少 |
| `PhysMaterial`、`PhysMaterialMask`、`SubsurfaceProfile`、`SpecularProfile`、`BlendableLocation`、`BlendablePriority`、`NaniteOverrideMaterial`（以及某个引擎分支自己的，比如 `ToonProfile`） | 实例自身的属性 |

枚举值不带前缀（`Translucent`，而不是 `BLEND_Translucent`），资产写成对象路径或 `None`。未知的键是 `DSH8249`，错误的值是 `DSH8250`。`Backend`、`UsageFlags`、`bOverride...` 标志以及原始参数数组会被拒绝，并给出原因（`DSH8251`）。

<a id="tools"></a>

## 工具

- Material Content Browser 中的 **New Instance** 会按所选父材质写出模板。
- `dsc compile MI_X.dsi`、`dsc check`、`dsc dump-ir` 和 `dsc index` 接受一个 `.dsi`。
- `dsc decompile /Game/.../MI_X` 为普通材质实例写出一个 `.dsi`：即与父材质不同的部分。语言无法表述的覆盖会被点名并略去（`DSH9101`、`DSH9104`、`DSH9105`）。
- 对手改过的实例执行 **Adopt Into Source** 会就地逐值改写 `.dsi`，因此文件中的注释和顺序得以保留。声明成 `bool`、`int` 或 `uint` 的标量，如果其当前值无法用该类型表示，就改成 `float`；能表示的值保留原有声明。
- `dump-graph` 会把一个实例按其父材质、覆盖和键导出。

<a id="see-also"></a>

## 另见

- [2.0 语言](index.md)
- [诊断 `DSH7250`–`DSH7270`](../diagnostics/DSH7xxx.md)、[`DSH8240`–`DSH8265`](../diagnostics/DSH8xxx.md)、[`DSH9100`–`DSH9109`](../diagnostics/DSH9xxx.md)
