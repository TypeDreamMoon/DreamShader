---
title: "SamplerType"
description: "SamplerType"
---

<a id="samplertype"></a>

# SamplerType

> [DreamShader](../index.md) » [Parameters](index.md) » **SamplerType**

用于设置纹理节点如何解释其纹理的元数据键：位于生成的 `UMaterialExpressionTextureBase` 子类上的 `EMaterialSamplerType`。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf` — 纹理值 `Properties` 声明上的 `[ … ]` 元数据块 |
| 种类 | 元数据键 |
| 生成 | 对生成的节点上 `SamplerType` 的写入 |
| 起始版本 | `1.2.4` |

<a id="synopsis"></a>

## 概要

```c
<texture-token> <name> [ = <default> ] [ SamplerType = "<value>" [ ; <entry> … ] ] ;
```

*(自 2.0.0 起)* `SamplerType` 在声明上有自己的一条路径：在紧凑纹理标记或 `TextureObjectParameter` 上，它会成为 2.0 的 `/// @sampler` 指令（去掉 `SAMPLERTYPE_` 前缀），并由发射器的[枚举字面量匹配器](metadata.md#enum-literals)解析（当没有匹配时使用 [`DSH8235`](../diagnostics/DSH8xxx.md#dsh8235)；空值则为 [`DSH7222`](../diagnostics/DSH7xxx.md#dsh7222)）。在纹理采样标记上（会在其使用处展开），它是反射节点的参数，在源绑定时会根据引擎目录进行检查。

适用于生成的节点派生自 `UMaterialExpressionTextureBase` 的每个声明：

| 声明 | 节点 | `SamplerType` 是否适用 |
| :-- | :-- | :-- |
| `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | `TextureObjectParameter` | 是 |
| `const Texture2D` 及其他四个紧凑纹理标记 | `TextureObject` | 是 |
| `TextureObjectParameter` | `TextureObjectParameter` | 是 |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | 匹配的采样参数 | 是 |
| `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | — | 否 — 这些类没有 `SamplerType`（[`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)） |
| `SpriteTextureSampler`, `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | — | 该标记没有 2.0 形式（[`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)） |

<a id="values"></a>

## 值

17 个值。每一行是一个枚举项；行中的所有四种拼写都会选择它，比较时会去除空格、`_`、`-`、`:`、`.` 和 `/` 并忽略大小写 — 因此 `"linear color"`、`"Linear-Color"` 和 `"linearcolor"` 与 `LinearColor` 是同一个值。

| 值 | 枚举常量 | 完全限定名 | `DisplayName` 拼写 |
| :-- | :-- | :-- | :-- |
| `Color` | `SAMPLERTYPE_Color` | `EMaterialSamplerType::SAMPLERTYPE_Color` | `Color` |
| `Grayscale` | `SAMPLERTYPE_Grayscale` | `EMaterialSamplerType::SAMPLERTYPE_Grayscale` | `Grayscale` |
| `Alpha` | `SAMPLERTYPE_Alpha` | `EMaterialSamplerType::SAMPLERTYPE_Alpha` | `Alpha` |
| `Normal` | `SAMPLERTYPE_Normal` | `EMaterialSamplerType::SAMPLERTYPE_Normal` | `Normal` |
| `Masks` | `SAMPLERTYPE_Masks` | `EMaterialSamplerType::SAMPLERTYPE_Masks` | `Masks` |
| `DistanceFieldFont` | `SAMPLERTYPE_DistanceFieldFont` | `EMaterialSamplerType::SAMPLERTYPE_DistanceFieldFont` | `Distance Field Font` |
| `LinearColor` | `SAMPLERTYPE_LinearColor` | `EMaterialSamplerType::SAMPLERTYPE_LinearColor` | `Linear Color` |
| `LinearGrayscale` | `SAMPLERTYPE_LinearGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_LinearGrayscale` | `Linear Grayscale` |
| `Data` | `SAMPLERTYPE_Data` | `EMaterialSamplerType::SAMPLERTYPE_Data` | `Data` |
| `External` | `SAMPLERTYPE_External` | `EMaterialSamplerType::SAMPLERTYPE_External` | `External` |
| `VirtualColor` | `SAMPLERTYPE_VirtualColor` | `EMaterialSamplerType::SAMPLERTYPE_VirtualColor` | `Virtual Color` |
| `VirtualGrayscale` | `SAMPLERTYPE_VirtualGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_VirtualGrayscale` | `Virtual Grayscale` |
| `VirtualAlpha` | `SAMPLERTYPE_VirtualAlpha` | `EMaterialSamplerType::SAMPLERTYPE_VirtualAlpha` | `Virtual Alpha` |
| `VirtualNormal` | `SAMPLERTYPE_VirtualNormal` | `EMaterialSamplerType::SAMPLERTYPE_VirtualNormal` | `Virtual Normal` |
| `VirtualMasks` | `SAMPLERTYPE_VirtualMasks` | `EMaterialSamplerType::SAMPLERTYPE_VirtualMasks` | **`Virtual Mask`** |
| `VirtualLinearColor` | `SAMPLERTYPE_VirtualLinearColor` | `EMaterialSamplerType::SAMPLERTYPE_VirtualLinearColor` | `Virtual Linear Color` |
| `VirtualLinearGrayscale` | `SAMPLERTYPE_VirtualLinearGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_VirtualLinearGrayscale` | `Virtual Linear Grayscale` |

`DistanceFieldFont` 或 `External` 没有虚拟对应项。

> [!NOTE]
> 在**纹理采样标记**上，该值会根据引擎目录列出的 17 个枚举项名称进行检查 *(自 2.0.0 起)*。若能规范化为其中之一（`SAMPLERTYPE_Normal`、`linear color`），则会以 [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) 警告被接受；而 `DisplayName` 若不是枚举项名称 — 例如 `Virtual Mask` — 则会被 [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215)。在声明上，四种拼写都可用。

> [!NOTE]
> 枚举项列表以哨兵值 `SAMPLERTYPE_MAX` 结尾。它不携带 `Hidden` 元数据，因此在声明上 `[SamplerType="SAMPLERTYPE_MAX"]` 和 `[SamplerType="MAX"]` 会直接解析而不会报错，并写入一个并非采样器类型的值。它不是可用值。纹理采样标记所检查的目录不列出它（`DSH5215`）。

<a id="inference-and-override-order"></a>

## 推断与覆盖顺序

| 步骤 | 发生的情况 |
| :-- | :-- |
| 1 | 创建节点并分配其纹理资产 — 来自 `= Path(…)`、来自所声明维度的引擎回退资产，或对于没有资产的采样器参数由 `SetDefaultTexture()` 分配 |
| 2 | 如果源未给出 `SamplerType`，`AutoSetSampleType()` 会根据所分配纹理的压缩设置和 sRGB 标志推断出一个值 |
| 3 | 如果源给出了该值，则写入该值 |

显式的 `SamplerType` **始终优先**：具有该值的节点完全不会运行推断。省略该键意味着该值会跟随资产 — 如果资产的压缩设置发生变化，该值也会变化。

> [!NOTE]
> [反编译器](../tools/decompiler.md) 始终显式发出 `SamplerType`，即使它与推断值相同，因此当资产稍后被重新压缩时，`UMaterial` → `.dsm` → `UMaterial` 的往返过程不会产生偏移。

<a id="samplersource"></a>

## SamplerSource

`UMaterialExpressionTextureSample` 子类上的伴随键 — 采样使用哪个采样器状态。它仅适用于纹理采样标记，因此其值会如上所述根据目录进行检查。

| 值 | 枚举常量 | `DisplayName` 拼写 | 含义 |
| :-- | :-- | :-- | :-- |
| `FromTextureAsset` | `SSM_FromTextureAsset` | `From texture asset` | 从纹理获取采样器；会占用着色器有限的采样器槽位之一 |
| `Wrap_WorldGroupSettings` | `SSM_Wrap_WorldGroupSettings` | `Shared: Wrap` | 共享采样器，环绕寻址，过滤器来自世界纹理组；不占用槽位 |
| `Clamp_WorldGroupSettings` | `SSM_Clamp_WorldGroupSettings` | `Shared: Clamp` | 共享采样器，钳制寻址，过滤器来自世界纹理组；不占用槽位 |
| `TerrainWeightmapGroupSettings` | `SSM_TerrainWeightmapGroupSettings` | — | **不可选择** — 标记为 `UMETA(Hidden)`，且不在目录中 |

默认值为 `FromTextureAsset`。写入枚举项名称 — `[SamplerSource="Wrap_WorldGroupSettings"]`；`SSM_Wrap_WorldGroupSettings` 会以 `DSH5278` 被接受。显示名称 `Shared: Wrap` 和 `Shared: Clamp` 在目录中不会规范化为任何内容，且会被 `DSH5215` *(自 2.0.0 起；1.x 接受它们)*。

<a id="related-texture-sample-keys"></a>

## 相关纹理采样键

以下是反编译器在每个纹理采样参数上发出的键，以及它视为默认值的值。它们都是普通的反射元数据。

| 键 | 类型 | 默认值 |
| :-- | :-- | :-- |
| `SamplerType` | `EMaterialSamplerType` | 从资产派生 |
| `SamplerSource` | `ESamplerSourceMode` | `FromTextureAsset` |
| `MipValueMode` | `ETextureMipValueMode` | `None` |
| `GatherMode` *(自 UE 5.6 起)* | enum | `None` |
| `AutomaticViewMipBias` | bool | `true` |
| `ConstCoordinate` | `uint8` | `0` |
| `ConstMipValue` | `int32` | `-1` |
| `IsDefaultMeshpaintTexture` | bool | `false` |
| `SortPriority` | `int32` | `32` |

`MipValueMode` 是唯一会改变节点 *引脚* 名称的键，因而也会改变
[调用形式](graph-usage.md#pin-names-by-parameter-type)在活动节点上可以匹配哪些名称。

<a id="dimension-validation-and-inference"></a>

## 维度验证与推断

`SamplerType` 描述的是解释方式；纹理的 **维度** 是另一项独立的检查。

| 声明 | 是否声明维度 | 检查内容 |
| :-- | :-- | :-- |
| `Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` | 是 | 另一个维度的 `Class'…'` 壳（[`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)）；不检查已加载的资产 *(自 2.0.0 起)* |
| 同样五个标记的 `const` 形式 | 是 | 同上 |
| `TextureObjectParameter` | 否 | 维度取自已加载的资产 |
| `TextureSampleParameter2D` / `2DArray` / `Cube` / `Volume` / `SubUV` | 是，来自标记的后缀 | 与紧凑标记一样检查壳 |
| `TextureSampleParameterCubeArray` | 否 | 仅检查壳是否命名了一个纹理（[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)） |

`TextureObjectParameter` 的维度由其默认值所加载的资产读出：`UTextureCube` →
`TextureCube`，`UTexture2DArray` → `Texture2DArray`，`UVolumeTexture` → `VolumeTexture`，其他任何值——
或者默认值未加载——`Texture2D`。

> [!WARNING]
> 一个 `TextureSampleParameterCube` 若通过纯对象路径指向 2D 纹理，会干净地生成，
> 随后在 Unreal 的着色器编译器中失败。参见
> [参数节点标记](parameter-nodes.md#dimension-validation-asymmetry)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH8235` | 声明的 `SamplerType` 在四种拼写下均不匹配任何枚举值 |
| `DSH7222` | 声明的 `SamplerType` 为空 |
| `DSH5215` | 纹理采样标记的 `SamplerType` 或 `SamplerSource` 不是目录列出的枚举值 |
| `DSH5278` | 纹理采样标记的值是某枚举值的 1.x 拼写（警告） |
| `DSH5213` | 节点类没有 `SamplerType`（或没有 `SamplerSource`） |
| `DSH1043` / `DSH1044` | 纹理默认值的 `Class'…'` 命名了非纹理 / 另一维度的纹理 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_SamplerType")
{
    Properties = {
        // Explicit: linear data, shared sampler, no slot consumed.
        TextureSampleParameter2D MaskMap = Path(Game, "Textures/T_Mask") [
            SamplerType   = "LinearColor";
            SamplerSource = "Wrap_WorldGroupSettings";
            MipValueMode  = "None";
        ];

        // Inferred: SamplerType follows the asset's compression settings.
        TextureSampleParameter2D BaseMap = Path(Game, "Textures/T_Base");

        // A 1.x spelling: accepted, with DSH5278.
        TextureSampleParameterCube Env = Path(Engine, "EngineResources/DefaultTextureCube") [
            SamplerType = "linear color"
        ];

        const Texture2D Lut = Path(Game, "Textures/T_Lut") [SamplerType = "Data"];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.BaseColor = Color; }

    Graph = {
        vec2 UV = UE.TexCoord(Index = 0);
        vec4 B  = BaseMap(Coordinates = UV);
        vec4 M  = MaskMap(Coordinates = UV);
        Color = B.rgb * M.r;
    }
}
```

应用后的值：

```text
MaskMap  SamplerType = SAMPLERTYPE_LinearColor  SamplerSource = SSM_Wrap_WorldGroupSettings
BaseMap  SamplerType = <AutoSetSampleType from /Game/Textures/T_Base>
Env      SamplerType = SAMPLERTYPE_LinearColor  ("linear color" normalizes to LinearColor; DSH5278)
Lut      SamplerType = SAMPLERTYPE_Data         on a TextureObject (const) node
```

`Env` 和 `Lut` 在函数体读取它们时应用；此 `Graph` 不读取，因此它们不构建任何东西。

<a id="see-also"></a>

## 另请参阅

- [参数](index.md) — 枢纽与决策表
- [元数据块](metadata.md) — 此键遵循的枚举字面量规则
- [参数节点标记](parameter-nodes.md) — 哪些标记会公开 `SamplerType`
- [紧凑类型标记](compact-types.md) — 五个紧凑纹理标记及其回退资产
- [Path(…)](path.md) — 资产引用语法
- [在图表中使用参数](graph-usage.md) — `MipValueMode` 如何改变可调用的引脚名称
- [反编译器](../tools/decompiler.md) — 这些键的往返保证
