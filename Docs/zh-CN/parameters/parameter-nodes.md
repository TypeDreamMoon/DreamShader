---
title: "参数节点标记"
description: "参数节点标记"
---

<a id="parameter-node-tokens"></a>

# 参数节点标记

> [DreamShader](../index.md) » [参数](index.md) » **参数节点标记**

这 22 个类型标记直接指定 Unreal 参数表达式类的名称，因此一个 `Properties`
声明会生成该精确节点，而不是紧凑的标量 / 向量 / 纹理对象节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 的 `Properties` 部分 |
| 种类 | 参数类型标记 |
| 生成 | 指定的 `UMaterialExpression` 子类 |
| 起始版本 | `1.2.3`；`DynamicParameter` / `CurveAtlasRowParameter` 内联默认值以及纹理采样系列修复于 `1.4.1` |

<a id="synopsis"></a>

## 概要

```c
<parameter-node-token> <name> [ = <default> ] [ [ <metadata> ] ] ;
```

标记按**不区分大小写**方式匹配。该集合是封闭的：恰好为下面这 22 个标记。
`const` 仅对 `ScalarParameter`、`VectorParameter` 和 `TextureObjectParameter`
合法 *(自 2.0.0 起)*；用于任何其他标记时属于 [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)。

<a id="the-22-tokens"></a>

## 这 22 个标记

*(自 2.0.0 起)* 每个标记采用三种 2.0 形式之一：

- **声明** — `ScalarParameter`、`StaticBoolParameter`、`VectorParameter` 和
  `TextureObjectParameter` 是紧凑标记会生成的 `uniform`（`float`、一个 `/// @static`
  `bool`、`float4`、`Texture2D`），无论读取多少次都只有一个参数节点；
- **每次使用展开** — 其余构建时不作为声明的十一个标记：对属性的每次读取或调用都是一次反射 `UE.Expression(Class = "<Token>", ParameterName = …, …)` 调用，
  携带默认值和元数据，相同的使用共享一个节点；
- **无 2.0 写法** — 有七个标记为 `DSH3253`。请将此类材质迁移到 `.dss`，并使用
  [`UE.Expression`](../builtins/ue-expression.md) 构建节点。

| # | 标记 | 2.0 形式 | 生成的类 | `= default` 接受 | `Graph` 使用 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | `ScalarParameter` | 声明 | `UMaterialExpressionScalarParameter` | 标量字面量 | 值 |
| 2 | `StaticBoolParameter` | 声明 | `UMaterialExpressionStaticBoolParameter` | 仅 `true` / `false` | 值 |
| 3 | `StaticSwitchParameter` | 展开 | `UMaterialExpressionStaticSwitchParameter` | 仅 `true` / `false` | **仅调用** — `N(True = …, False = …)` |
| 4 | `VectorParameter` | 声明 | `UMaterialExpressionVectorParameter` | 向量字面量 | 值，4 个分量 |
| 5 | `DoubleVectorParameter` | `DSH3253` | — | — | — |
| 6 | `ChannelMaskParameter` | 展开 | `UMaterialExpressionChannelMaskParameter` | 向量字面量 | `N(Input = …)`，**1** 个分量 |
| 7 | `StaticComponentMaskParameter` | 展开 | `UMaterialExpressionStaticComponentMaskParameter` | 无 — 见下文 | `N(Input = …)` |
| 8 | `CurveAtlasRowParameter` | `DSH3253` | — | — | — |
| 9 | `DynamicParameter` | `DSH3253` | — | — | — |
| 10 | `FontSampleParameter` | `DSH3253` | — | — | — |
| 11 | `SpriteTextureSampler` | `DSH3253` | — | — | — |
| 12 | `TextureObjectParameter` | 声明 | `UMaterialExpressionTextureObjectParameter` | 资产引用 | 值（纹理） |
| 13 | `TextureCollectionParameter` | `DSH3253` | — | — | — |
| 14 | `SparseVolumeTextureObjectParameter` | `DSH3253` | — | — | — |
| 15 | `TextureSampleParameter2D` | 展开 | `UMaterialExpressionTextureSampleParameter2D` | 资产引用 | `N(Coordinates = …)`，按 `RGBA` 读取 |
| 16 | `TextureSampleParameter2DArray` | 展开 | `UMaterialExpressionTextureSampleParameter2DArray` | 资产引用 | 同 15 |
| 17 | `TextureSampleParameterCube` | 展开 | `UMaterialExpressionTextureSampleParameterCube` | 资产引用 | 同 15 |
| 18 | `TextureSampleParameterCubeArray` | 展开 | `UMaterialExpressionTextureSampleParameterCubeArray` | 资产引用 | 同 15 |
| 19 | `TextureSampleParameterVolume` | 展开 | `UMaterialExpressionTextureSampleParameterVolume` | 资产引用 | 同 15 |
| 20 | `TextureSampleParameterSubUV` | 展开 | `UMaterialExpressionTextureSampleParameterSubUV` | 资产引用 | 同 15 |
| 21 | `RuntimeVirtualTextureSampleParameter` | 展开 | `UMaterialExpressionRuntimeVirtualTextureSampleParameter` | 无 — 见下文 | `N(Coordinates = …)`，多个输出 |
| 22 | `SparseVolumeTextureSampleParameter` | 展开 | `UMaterialExpressionSparseVolumeTextureSampleParameter` | 无 — 见下文 | `N(Coordinates = …)`，多个输出 |

展开标记的类会在引擎的节点目录中查找；此引擎没有的类属于 [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212)。`Graph` 使用情况在此概述；
引脚表位于[在 Graph 中使用参数](graph-usage.md#pin-names-by-parameter-type)。

<a id="type-specific-metadata-slots"></a>

## 类型特定的元数据槽

每个标记还接受通用元数据键（`Group`、`Description`、`SortPriority`、
`Slider(min, max)`、`ParameterName`）以及其类的任何反射 UPROPERTY。实际中重要的
类特定槽：

| 标记 | 绑定类特定 UPROPERTY 的元数据键 |
| :-- | :-- |
| `ScalarParameter` | `SliderMin`、`SliderMax`、`PrimitiveDataIndex` |
| `StaticBoolParameter` | — |
| `StaticSwitchParameter` | — |
| `VectorParameter` | `UseCustomPrimitiveData`（绑定 `bUseCustomPrimitiveData`）、`PrimitiveDataIndex`、`ChannelNames` |
| `ChannelMaskParameter` | `MaskChannel`（枚举 `EChannelMaskParameterColor`） |
| `StaticComponentMaskParameter` | `DefaultR`、`DefaultG`、`DefaultB`、`DefaultA`（布尔值） |
| `TextureObjectParameter` | `Texture`、`SamplerType`、`IsDefaultMeshpaintTexture` |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV`（标记 15–20） | `Texture`、`SamplerType`、`SamplerSource`、`MipValueMode`、`GatherMode` *(自 UE 5.6 起)*、`AutomaticViewMipBias`、`ConstCoordinate`、`ConstMipValue`、`IsDefaultMeshpaintTexture`；`TextureSampleParameterSubUV` 会添加 `bBlend` |
| `RuntimeVirtualTextureSampleParameter` | `VirtualTexture`、`MaterialType`、`bSinglePhysicalSpace`、`bAdaptive`、`MipValueMode`、`TextureAddressMode` |
| `SparseVolumeTextureSampleParameter` | `SparseVolumeTexture`、`MipValueMode`、`SamplerSource`、`ConstMipValue` |

布尔 UPROPERTY 匹配时开头的 `b` 可选，因此 `[bBlend=true]` 和 `[Blend=true]`
都会绑定 `bBlend`。类不具有的键如何报告取决于形式：声明上为警告，
展开标记上为错误 — 参见[元数据](metadata.md#reflected-property-passthrough)。

<a id="which-default-parser-a-token-uses"></a>

## 标记使用哪种默认解析器

| 标记 | 接受 | 否则 |
| :-- | :-- | :-- |
| `ScalarParameter` | 标量字面量，依据[紧凑类型标记](compact-types.md#scalar-default-grammar) | [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) |
| `StaticBoolParameter`、`StaticSwitchParameter` | 仅 `true` / `false` | `DSH3254` |
| `VectorParameter`、`ChannelMaskParameter` | 向量字面量，依据[紧凑类型标记](compact-types.md#vector-default-grammar) | `VectorParameter` 上 `DSH3254`；在 `ChannelMaskParameter` 上，不可读的字面量会被丢弃且不产生诊断 |
| `TextureObjectParameter` | 一个 [`Path(…)`](path.md) 资产引用 | 在构建材质时解析：[`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271) / [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218) |
| 纹理采样标记 15–20 | 一个 [`Path(…)`](path.md) 资产引用 | 在构建材质时解析：[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)，并引用解析器的代码 |
| `StaticComponentMaskParameter` | 无 — 该类没有 `DefaultValue` | 默认值会被丢弃并给出 [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288)；请写 `[DefaultR = true; …]` |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 无 | 默认值会被丢弃且**不产生诊断** *(自 2.0.0 起)* |

> [!WARNING]
> **`RuntimeVirtualTextureSampleParameter` 或 `SparseVolumeTextureSampleParameter` 默认值会被
> 忽略。** 对任一标记使用 `= Path(…)` 不会触及该节点的任何属性，而且不会有任何提示。请改为
> 通过元数据绑定资产 — `[VirtualTexture = Path(…)]`、`[SparseVolumeTexture = Path(…)]`。

<a id="what-the-default-is-written-to"></a>

### 默认值写入到何处

| 情况 | 结果 |
| :-- | :-- |
| 无 `= <default>` | 不写入任何内容；展开节点保留其引擎默认值，声明则保留其紧凑对应项的默认值（见[紧凑类型标记](compact-types.md)） |
| 声明标记 | `uniform` 的默认值，与紧凑对应项完全一致 |
| `StaticSwitchParameter` | `DefaultValue = true` 或 `false` |
| `ChannelMaskParameter` | `DefaultValue` = 字面量的四个值 |
| 纹理采样标记 15–20 | 节点的 `Texture` 属性。可以解析但无法加载的资产会以空值写入，且不会产生诊断，节点随后采用引擎的默认纹理 |

<a id="texture-dimension-inference"></a>

## 纹理维度推断

纹理采样标记通过 `TextureSampleParameter` 之后的后缀确定维度：

| 标记 | 维度 |
| :-- | :-- |
| `TextureSampleParameter2D` | `Texture2D` |
| `TextureSampleParameter2DArray` | `Texture2DArray` |
| `TextureSampleParameterCube` | `TextureCube` |
| `TextureSampleParameterCubeArray` | 无 — 任意纹理 |
| `TextureSampleParameterVolume` | `VolumeTexture` |
| `TextureSampleParameterSubUV` | `Texture2D` |
| `RuntimeVirtualTextureSampleParameter` | 不判断 |
| `SparseVolumeTextureSampleParameter` | 不判断 |

*(自 2.0.0 起)* 后缀按整体比较，因此 `TextureSampleParameterCubeArray` 不再被视为立方体贴图。维度只用于一件事：判断 `Class'…'` 默认值所指定的类。

`TextureObjectParameter` 声明**没有**维度：它采用其默认值所加载资产的维度 — 立方体贴图、2D 数组或体积纹理，否则为 `Texture2D` — 因此它可以接受其中任意一种 *(自 1.6.0 起)*。

<a id="dimension-validation-asymmetry"></a>

### 维度验证的不对称性

| 标记系列 | 检查内容 |
| :-- | :-- |
| 紧凑 `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | `Class'…'` shell：另一种维度为 [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)，非纹理类为 [`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)。不检查加载的资产 *(自 2.0.0 起)* |
| `TextureObjectParameter` | 仅检查 shell 的“不是纹理”部分 (`DSH1043`) |
| 纹理采样标记 15–17、19、20 | 检查 shell，与紧凑标记相同 |
| `TextureSampleParameterCubeArray` | 仅检查 shell 的“不是纹理”部分 |
| `RuntimeVirtualTextureSampleParameter`、`SparseVolumeTextureSampleParameter` | 不检查；不读取其默认值 |

> [!WARNING]
> 通过其普通对象路径被赋予普通 2D 纹理的 `TextureSampleParameterCube` 在生成时不会产生
> DreamShader 诊断；只有 `Texture2D'…'` shell 才会暴露这种不匹配
> (`DSH1044`)。否则，这种不匹配会在之后作为材质上的 Unreal 着色器编译错误出现。
> 请赋予正确的维度。

<a id="notes"></a>

## 说明

- **对于每个具有 2.0 形式的标记，默认值都是可选的。** 测试
  `DreamShader.Lang.ParameterExpressions.ParseAll` 覆盖全部 15 个标记，覆盖带内联默认值和不带内联默认值的 `ScalarParameter`、
  `VectorParameter` 与 `TextureObjectParameter`，并检查
  其余七个标记均为 `DSH3253`。
- **没有纹理的采样器参数仍然可以编译。** 当展开节点派生自
  `UMaterialExpressionTextureBase` 且其 `Texture` 为空时，会为
  `UMaterialExpressionTextureSampleParameter` 子类调用 `SetDefaultTexture()`。否则，没有默认值的采样器参数
  会以 *Missing input Texture* 失败。运行时虚拟纹理和稀疏体积节点并非
  此类子类，仍然需要绑定其资产。
- **显式 `SamplerType` 始终优先。** `AutoSetSampleType()` 仅在源
  未提供 `SamplerType` 的节点上运行。参见 [SamplerType](sampler-type.md)。
- `StaticSwitchParameter` 不能作为值读取。裸引用不是已声明的名称
  ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200))；它必须被调用。参见
  [在 Graph 中使用参数](graph-usage.md#staticswitchparameter)。
- **`Group(…)` 作用域不会为展开标记盖章** *(自 2.0.0 起)*：此类成员不会从该作用域获得
  分组，也不会自动获得 `SortPriority`，并且不会占用计数器槽位。请在其自身的元数据块中写入
  `Group` / `SortPriority`。参见
  [元数据](metadata.md#group-scopes-and-the-sortpriority-counter)。
- [反编译器](../tools/decompiler.md) 在每次导出时都会显式输出 `SamplerType` 和纹理采样元数据键，
  即使它们处于默认值，因此反编译 → 重新编译的往返过程是稳定的。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3253` | 七个没有 2.0 形式的标记之一，或在 `ScalarParameter`、`VectorParameter`、`TextureObjectParameter` 之外的标记上使用 `const` |
| `DSH3254` | 在 `StaticBoolParameter` 或 `StaticSwitchParameter` 上使用了非 `true`/`false` 默认值，或在 `ScalarParameter` 或 `VectorParameter` 上使用了无法解析的默认值 |
| `DSH5288` | 在 `StaticComponentMaskParameter` 上使用了默认值，而它没有 `DefaultValue`（警告；默认值被丢弃） |
| `DSH5212` | 引擎中没有用于展开标记的类 |
| `DSH1043` / `DSH1044` | `Class'…'` 默认值指定了非纹理 / 另一种维度的纹理 |
| `DSH8271` / `DSH8218` | `TextureObjectParameter` 默认值无法解析 / 无法加载 |
| `DSH8213` | 展开节点无法接受某个值：无法解析的纹理采样默认值、类型错误的元数据值 |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | 展开标记的元数据键既不是其类的引脚也不是其属性 |
| [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) | 声明标记的元数据键不是其类的属性（警告；不写入该值） |

完整列表位于[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_ParameterNodes")
{
    Properties = {
        Group("Surface") {
            TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White") [
                SamplerType  = "LinearColor";
                SamplerSource = "FromTextureAsset";
                MipValueMode = "None";
                AutomaticViewMipBias = true;
            ];
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Tint      = float4(1.0, 0.8, 0.6, 1.0);
        }

        Group("Masks") {
            ChannelMaskParameter         Pick = float4(1, 0, 0, 0) [MaskChannel = "Red"];
            StaticComponentMaskParameter Keep = float4(1, 1, 0, 0);
        }

        StaticSwitchParameter UseDetail = true [Group="Switches"];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec2  UV     = UE.TexCoord(Index = 0);
        vec4  Sample = BaseTex(Coordinates = UV);
        vec4  Masked = Keep(Input = Sample);
        float Chan   = Pick(Input = Sample);

        Color = UseDetail(True = Masked.rgb, False = Tint.rgb);
        Rough = Roughness * Chan;
    }
}
```

生成的节点：

```text
TextureSampleParameter2D      BaseTex    SamplerType=LinearColor
ScalarParameter               Roughness  Group="Surface" SortPriority=0  SliderMin=0 SliderMax=1
VectorParameter               Tint       Group="Surface" SortPriority=10
ChannelMaskParameter          Pick       MaskChannel=Red
StaticComponentMaskParameter  Keep
StaticSwitchParameter         UseDetail  Group="Switches"
TextureCoordinate             UV         CoordinateIndex=0
```

`BaseTex`、`Pick` 和 `Keep` 是展开标记，因此它们的 `Group(…)` 作用域不会为其盖章，
`Roughness` 占用计数器槽位 0。`Keep` 的 `float4` 默认值会随 `DSH5288` 被丢弃：请为该遮罩写入
`[DefaultR = true; DefaultG = true]`。

<a id="see-also"></a>

## 另请参见

- [参数](index.md) — 中心页面与决策表
- [紧凑类型标记](compact-types.md) — 39 个内置标记及其所受检查
- [元数据块](metadata.md) — `[MaskChannel=…]` 及所有其他键的写入方式
- [SamplerType](sampler-type.md) — 每个采样器类型值，以及 `SamplerSource`
- [Path(…)](path.md) — 这些默认值所使用的资产引用语法
- [在 Graph 中使用参数](graph-usage.md) — 读取、引脚调用形式，以及 `StaticSwitchParameter`
- [属性（章节）](../language/properties.md) — 外层章节语法
- [UE.Expression](../builtins/ue-expression.md) — 没有标记的类的通用反射形式
- [反编译器](../tools/decompiler.md) — `UMaterial` 导出会生成这些标记中的哪些
- [测试](../contributing/testing.md) — 固定此表的参数矩阵夹具
