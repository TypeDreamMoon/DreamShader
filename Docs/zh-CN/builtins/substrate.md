---
title: "Substrate.*"
description: "Substrate.*"
---

<a id="substrate"></a>

# Substrate.*

> [DreamShader](../index.md) » [内置节点](index.md) » **`Substrate.*`**

与 [`UE.*`](ue.md) 并列的调用命名空间，装着 Unreal 的 Substrate BSDF、组合与工具
材质节点。（自 2.0.0 起）它是内置目录的 `Substrate` 命名空间：运行中引擎拥有的每一个 Substrate 类，
按名字调用。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` —— 在 `Graph { … }` 体内、`Outputs` 绑定表达式里，或 `Outputs` 声明初始化器中 |
| 类型 | 内置调用命名空间 |
| 生成 | 每次调用一个 `UMaterialExpressionSubstrate*` 节点；完全相同的调用是同一个节点 |
| 名字 | 运行中引擎的每一个 `UMaterialExpressionSubstrate*` 类 —— [目录](#catalogue)里的 24 个名字、目录的短名，以及由任何其他类推导出的名字 *(自 2.0.0 起)* |
| 需要 | 一个带 Substrate 节点的引擎：**UE 5.4 或更新**；`Substrate.Select` 自 UE 5.6 起 |

<a id="availability"></a>

## 可用性

绑定器里没有任何按版本编译进或编译出的东西 *(自 2.0.0 起)*：目录列出运行中引擎的
Substrate 类，而 UE 5.4 之前的引擎一个都没有。

| 使用面 | 在没有该节点的引擎上 |
| :-- | :-- |
| `Substrate.<Name>(…)` | [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) —— 对 `Select`（UE 5.6）和 `Toon`（UE 5.8）报 [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294)，消息会写出哪个引擎版本才有 |
| `A + B`、`A * w`、`lerp(A, B, t)`、Slab 的虚拟参数 | `DSH5294`、[`DSH4381`](../diagnostics/DSH4xxx.md#dsh4381) |
| 两个 Substrate 值上的运行时分支 | UE 5.6 以下报 [`DSH4378`](../diagnostics/DSH4xxx.md#dsh4378) |
| 通用 `UE.*` 调用上的 `OutputType="Substrate"` | 和任何 `OutputType` 一样被丢弃 —— 见 [`OutputType`](output-type.md) |
| `Base.FrontMaterial` 输出绑定 | 由目录为该引擎列出的材质属性决定；没有这个名字报 [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200) |
| `Settings = { ShadingModel = "Substrate"; }`（以及 `"Strata"` 写法） | UE 5.4 以下报 [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)，消息里带 [`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128) |

> [!NOTE]
> 项目还必须启用 Substrate（*Project Settings ▸ Engine ▸ Rendering ▸
> Substrate*），Substrate 图才能在引擎里编译。DreamShader 会读这个设置
> *(自 2.0.0 起)*：`Shader` 的 `Substrate` 设置 —— `Legacy`、`Bridge` 或 `Native` —— 说明
> 材质在开或不开它时的工程里怎么建，而 `Native` 在 Substrate 关闭时驱动 `FrontMaterial` 会报 [`DSH4382`](../diagnostics/DSH4xxx.md#dsh4382)。见
> [一份源码，两种工程](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-)。

> [!NOTE]
> 编辑器工具链跟随引擎。UE 5.4 以下，Bridge 清单
> `Saved/DreamShader/Bridge/substrate-builtins.json` 被写成空条目数组、
> `supported: false` 和一个 `unsupportedReason` —— 所以编辑器扩展的补全里
> 什么都没有。参见 [Bridge](../../tools/bridge.md)。

<a id="synopsis"></a>

## 语法概要

```c
Substrate . <Name> ( [ <argument-name> = <expression> ]
                     [ , <argument-name> = <expression> ] …
                     [ , { Output | OutputName } = "<pin-name>" ]
                     [ , OutputIndex = <integer> ] )
```

`Substrate` 和 `.` 是字面量；`[ … ]`、`{ a | b }` 和 `…` 是元记号。

在 1.x 源码里前缀**不区分大小写**匹配 —— `Substrate` 是 1.x 的类型拼写，
类型拼写都忽略大小写读取 —— 只在忽略大小写时匹配的 `<Name>` 也接受，给出
警告 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)：`SUBSTRATE.UNLIT(…)` 能解析。参数名
同样匹配 —— 精确匹配，或忽略大小写并给出 `DSH5276` —— 此外是精确的：不
去掉任何分隔符。

<a id="argument-model"></a>

## 参数模型

`Substrate.*` 调用和任何反射节点调用（[`UE.Expression`](ue-expression.md)）一样，
对名字选中的类绑定。

| 规则 | 行为 |
| :-- | :-- |
| 参数要**命名** | 五个组合节点例外，它们也按位置接受操作数 *(自 2.0.0 起)*：`Add(A, B)`、`Weight(A, Weight)`、`HorizontalMix(Background, Foreground, Mix)`、`VerticalLayer(Top, Base, Thickness)`、`Select(A, B, SelectValue)`。其他地方的位置参数报 [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) |
| `Class=` **被拒绝** | [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216) —— 名字就是类 |
| `OutputType=` / `ResultType=` **被丢弃** | 由 legacy 前端丢弃，不报诊断；目录给调用定类型 |
| `Output=` / `OutputName=` | 按名字选择输出 |
| `OutputIndex=` | 按 0 起始的索引选择输出 |
| `Output`/`OutputName` 与 `OutputIndex` 同时出现 | [`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252) |
| 其他任何参数 | 按目录绑定 —— 见下 |

<a id="reflection-driven-binding"></a>

### 由反射驱动的绑定

对每个剩余参数，绑定器按顺序尝试：

1. **输入 pin** —— 类的 pin 名，或其别名：pin 的显示名写成标识符形式
   （`Diffuse_Albedo`），属性的显示名去掉空格也算。
2. **属性** —— 类的可编辑反射属性名或别名；bool 属性还可以去掉开头的 `b`，
   所以 `UseParameterBlending` 能触达 `bUseParameterBlending`。
3. **虚拟参数** —— 在有这些 pin 的 BSDF 上，`BaseColor` / `Metallic` / `Specular`、
   `Haziness`、`Transmittance` / `Thickness` 和 `IOR` 会变成那些 pin
   前面一个转换节点的输入 *(自 2.0.0 起)*。见
   [Slab 上的旧参数](../language-v2/substrate.md#legacy-parameters-on-a-slab)。
4. 在 1.x 源码里，以上都不是、但带着值的名字以
   [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) 保留，建节点时按名字连接
   （节点没有这个 pin 时 [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)）。
5. 都不匹配 → [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)。

pin 名匹配优先于同名属性。值按 pin 的类型检查：Substrate pin 上的
数值、数值 pin 上的 Substrate 值、以及 `MaterialAttributes` pin 上任何不是 material 的值，
都报 [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)。

> [!NOTE]
> **名字带空格的引擎 pin 能触达** *(自 2.0.0 起)*：显示为
> `Diffuse Albedo` 的 pin 写作 `Diffuse_Albedo`，它的属性名 —— `DiffuseAlbedo`，也就是下面各表
> 列出的名字 —— 照旧可用。

> [!NOTE]
> **可接受的参数名跟随引擎，而不是插件。** 目录读自运行中
> 引擎的 `UMaterialExpressionSubstrate*` 类，所以引擎版本增删的某个输入
> 会随之出现或消失，插件不需要改动。下面各表列出这些类暴露的输入；
> **补全**列标记的是 DreamShader 发布到编辑器补全和
> Bridge 清单的精选子集，那个子集*是*插件固定的。同一个类上任何可编辑的非输入属性 ——
> 枚举、浮点、布尔 —— 即使这里没有列出，也同样可以作为字面量参数设置。

<a id="selecting-an-output"></a>

### 选择输出

`Output=` / `OutputName=` 按目录列出的输出名精确匹配，包括带空格的名字；
在 1.x 源码里只差大小写的匹配也接受，给出 `DSH5276`。没有这个输出报
[`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201)。`OutputIndex=` 取 0 起始的索引
（超过最后一个报 [`DSH5282`](../diagnostics/DSH5xxx.md#dsh5282)）。只有下面的工具节点有多个输出。

<a id="catalogue"></a>

## 目录

*Substrate 输出*标记输出是 Substrate 材质值、可绑定到 `Base.FrontMaterial` 的类；标为**否**的四个是工具节点，返回普通数值。

| `Substrate.<Name>` | `UMaterialExpression` 类 | Substrate 输出 | 条目 |
| :-- | :-- | :-- | :-- |
| `ShadingModels` | `UMaterialExpressionSubstrateShadingModels` | 是 | [↓](#substrateshadingmodels) |
| `Slab` | `UMaterialExpressionSubstrateSlabBSDF` | 是 | [↓](#substrateslab) |
| `SimpleClearCoat` | `UMaterialExpressionSubstrateSimpleClearCoatBSDF` | 是 | [↓](#substratesimpleclearcoat) |
| `VolumetricFogCloud` | `UMaterialExpressionSubstrateVolumetricFogCloudBSDF` | 是 | [↓](#substratevolumetricfogcloud) |
| `Unlit` | `UMaterialExpressionSubstrateUnlitBSDF` | 是 | [↓](#substrateunlit) |
| `Hair` | `UMaterialExpressionSubstrateHairBSDF` | 是 | [↓](#substratehair) |
| `Eye` | `UMaterialExpressionSubstrateEyeBSDF` | 是 | [↓](#substrateeye) |
| `SingleLayerWater` | `UMaterialExpressionSubstrateSingleLayerWaterBSDF` | 是 | [↓](#substratesinglelayerwater) |
| `LightFunction` | `UMaterialExpressionSubstrateLightFunction` | 是 | [↓](#substratelightfunction) |
| `PostProcess` | `UMaterialExpressionSubstratePostProcess` | 是 | [↓](#substratepostprocess) |
| `UI` | `UMaterialExpressionSubstrateUI` | 是 | [↓](#substrateui) |
| `ConvertMaterialAttributes` | `UMaterialExpressionSubstrateConvertMaterialAttributes` | 是 | [↓](#substrateconvertmaterialattributes) |
| `ConvertToDecal` | `UMaterialExpressionSubstrateConvertToDecal` | 是 | [↓](#substrateconverttodecal) |
| `HorizontalMix` | `UMaterialExpressionSubstrateHorizontalMixing` | 是 | [↓](#substratehorizontalmix) |
| `HorizontalMixing` **—— ``HorizontalMix`` 的别名** | `UMaterialExpressionSubstrateHorizontalMixing` | 是 | [↓](#substratehorizontalmix) |
| `VerticalLayer` | `UMaterialExpressionSubstrateVerticalLayering` | 是 | [↓](#substrateverticallayer) |
| `VerticalLayering` **—— ``VerticalLayer`` 的别名** | `UMaterialExpressionSubstrateVerticalLayering` | 是 | [↓](#substrateverticallayer) |
| `Add` | `UMaterialExpressionSubstrateAdd` | 是 | [↓](#substrateadd) |
| `Weight` | `UMaterialExpressionSubstrateWeight` | 是 | [↓](#substrateweight) |
| `Select` *(UE 5.6)* | `UMaterialExpressionSubstrateSelect` | 是 | [↓](#substrateselect) |
| `TransmittanceToMFP` | `UMaterialExpressionSubstrateTransmittanceToMFP` | **否** | [↓](#substratetransmittancetomfp) |
| `MetalnessToDiffuseAlbedoF0` | `UMaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0` | **否** | [↓](#substratemetalnesstodiffusealbedof0) |
| `HazinessToSecondaryRoughness` | `UMaterialExpressionSubstrateHazinessToSecondaryRoughness` | **否** | [↓](#substratehazinesstosecondaryroughness) |
| `ThinFilm` | `UMaterialExpressionSubstrateThinFilm` | **否** | [↓](#substratethinfilm) |

两对别名是完全的重复：同一个类、同样的输入、同样的输出类型，两种写法都没有被弃用。自 2.0.0 起又多了两个短名
—— `Mix` 即 `HorizontalMix`、`Layer` 即
`VerticalLayer` —— 而且每个类都能用目录短名（`SubstrateSlabBSDF`）调用，
也能用类名去掉前面的 `MaterialExpressionSubstrate` 和后面的 `BSDF` 调用；这就是调用本表没有列出的类的方式：UE 5.8 的
`UMaterialExpressionSubstrateToonBSDF` 是
`Substrate.Toon`。

<a id="output-typing"></a>

## 输出类型

目录按引擎给每个输出定类型：BSDF 或组合节点产出 ``Substrate`` 值；工具节点产出数值，宽度按引擎给每个输出的类型 —— 引擎
没给类型时，和它被读入的位置一样宽。多输出的节点要指名读取一个输出。

一个 ``Substrate`` 值可以：

- 赋给 ``Substrate`` 类型的 ``Graph`` 变量或 ``Outputs`` 声明；
- 传给另一个 ``Substrate.*`` 节点或
  [`UE.Expression`](ue-expression.md) 节点的 Substrate 类型输入；
- 用 `+`（`Substrate.Add`）组合、用 `* <scalar>`（`Substrate.Weight`）加权、用
  `lerp(A, B, t)`（`Substrate.HorizontalMix`）混合 *(自 2.0.0 起)* —— 见
  [运算符与 `lerp`](../language-v2/substrate.md#operators-and-lerp)；
- 由静态开关选择，或由运行时 ``if`` 选择 —— 后者变成 ``Substrate.Select``
  *(自 2.0.0 起)* —— 见 [Substrate 值上的 `if`](../language-v2/substrate.md#if-and--over-substrate-values)；
- 绑定到 ``Base.FrontMaterial``。

Substrate 值上的其他运算符 —— ``-``、``/``、``A * B``、复合赋值 —— 以及 Substrate 值
与数值之间的 ``lerp`` 报 [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293)；把 Substrate 值传给其他数学内置报
[`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)。HLSL Custom 节点 ——
一个 ``Function`` —— 既不能接收也不能返回 Substrate 值（[`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253)）。

> [!NOTE]
> 主材质靠「两个 closure 上的静态开关」从静态参数里选着色变体：``UMaterialExpressionStaticSwitchParameter`` 在引擎构建材质
> 拓扑树时就决出布尔值、只下降到选中的分支，所以翻译器看到的仍是一个拓扑。
> [`UE.StaticSwitchParameter`](ue.md#uestaticswitchparameter) 不比较两条分支，
> 所以一边是 Substrate 值、一边是数值时 DreamShader 不会报错。

---

<a id="node-reference"></a>

## 节点参考

<a id="substrateshadingmodels"></a>

### Substrate.ShadingModels

用 Substrate 材质表达的传统着色模型表面。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | — |
| `Metallic` | 数值 | — |
| `Specular` | 数值 | — |
| `Roughness` | 数值 | — |
| `Anisotropy` | 数值 | — |
| `EmissiveColor` | 数值 | — |
| `Normal` | 数值 | — |
| `Tangent` | 数值 | — |
| `SubSurfaceColor` | 数值 | — |
| `ClearCoat` | 数值 | — |
| `ClearCoatRoughness` | 数值 | — |
| `Opacity` | 数值 | — |
| `TransmittanceColor` | 数值 | — |
| `WaterScatteringCoefficients` | 数值 | — |
| `WaterAbsorptionCoefficients` | 数值 | — |
| `WaterPhaseG` | 数值 | — |
| `ColorScaleBehindWater` | 数值 | — |
| `ClearCoatNormal` | 数值 | — |
| `CustomTangent` | 数值 | — |
| `ThinTranslucentSurfaceCoverage` | 数值 | — |
| `ShadingModel` *(自 2.0.1 起)* | 一个着色模型，``UE.ShadingModel(ShadingModel = …)``；不连时用节点的 ``ShadingModelOverride`` | — |

输出：Substrate 值。编辑器补全不为这个包装节点提供任何参数。

<a id="substrateslab"></a>

### Substrate.Slab

通用的 Substrate BSDF slab。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `DiffuseAlbedo` | 数值 | 是 |
| `F0` | 数值 | 是 |
| `F90` | 数值 | — |
| `Roughness` | 数值 | 是 |
| `Anisotropy` | 数值 | — |
| `Normal` | 数值 | 是 |
| `Tangent` | 数值 | — |
| `SSSMFP` | 数值 | — |
| `SSSMFPScale` | 数值 | — |
| `SSSPhaseAnisotropy` | 数值 | — |
| `EmissiveColor` | 数值 | — |
| `SecondRoughness` | 数值 | — |
| `SecondRoughnessWeight` | 数值 | — |
| `FuzzRoughness` | 数值 | — |
| `FuzzAmount` | 数值 | — |
| `FuzzColor` | 数值 | — |
| `GlintValue` | 数值 | — |
| `GlintUV` | 数值 | — |

输出：Substrate 值。

<a id="substratesimpleclearcoat"></a>

### Substrate.SimpleClearCoat

带固定第二层 clear coat 波瓣的 slab。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `DiffuseAlbedo` | 数值 | 是 |
| `F0` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `ClearCoatCoverage` | 数值 | 是 |
| `ClearCoatRoughness` | 数值 | 是 |
| `Normal` | 数值 | 是 |
| `EmissiveColor` | 数值 | — |
| `BottomNormal` | 数值 | — |

输出：Substrate 值。

<a id="substratevolumetricfogcloud"></a>

### Substrate.VolumetricFogCloud

用于体积雾和云材质的参与介质 BSDF。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Albedo` | 数值 | 是 |
| `Extinction` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |
| `AmbientOcclusion` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateunlit"></a>

### Substrate.Unlit

仅自发光 BSDF。最小的完整 Substrate 表面。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `EmissiveColor` | 数值 | 是 |
| `TransmittanceColor` | 数值 | — |
| `Normal` | 数值 | — |

输出：Substrate 值。

<a id="substratehair"></a>

### Substrate.Hair

头发 BSDF。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | 是 |
| `Scatter` | 数值 | 是 |
| `Specular` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `Backlit` | 数值 | 是 |
| `Tangent` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateeye"></a>

### Substrate.Eye

眼睛 BSDF。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `DiffuseColor` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `CorneaNormal` | 数值 | 是 |
| `IrisNormal` | 数值 | 是 |
| `IrisPlaneNormal` | 数值 | 是 |
| `IrisMask` | 数值 | 是 |
| `IrisDistance` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |

输出：Substrate 值。

<a id="substratesinglelayerwater"></a>

### Substrate.SingleLayerWater

单层水 BSDF。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | 是 |
| `Metallic` | 数值 | 是 |
| `Specular` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `Normal` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |
| `TopMaterialOpacity` | 数值 | 是 |
| `WaterAlbedo` | 数值 | 是 |
| `WaterExtinction` | 数值 | 是 |
| `WaterPhaseG` | 数值 | 是 |
| `ColorScaleBehindWater` | 数值 | 是 |

输出：Substrate 值。

<a id="substratelightfunction"></a>

### Substrate.LightFunction

用于光照函数材质的 Substrate 输出节点。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |

输出：Substrate 值。

<a id="substratepostprocess"></a>

### Substrate.PostProcess

用于后处理材质的 Substrate 输出节点。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |
| `Opacity` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateui"></a>

### Substrate.UI

用于 UI 域材质的 Substrate 输出节点。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |
| `Opacity` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateconvertmaterialattributes"></a>

### Substrate.ConvertMaterialAttributes

把一个 `MaterialAttributes` 值转换成 Substrate 材质。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `MaterialAttributes` | **`MaterialAttributes`** | 是 |
| `Attributes` —— `MaterialAttributes` 的别名 | **`MaterialAttributes`** | — |
| `WaterScatteringCoefficients` | 数值 | 是 |
| `WaterAbsorptionCoefficients` | 数值 | 是 |
| `WaterPhaseG` | 数值 | 是 |
| `ColorScaleBehindWater` | 数值 | 是 |

输出：Substrate 值。

`MaterialAttributes` 是反射属性名，`Attributes` 是引擎给同一个输入起的 pin 名；两者都绑定 input 0。传数值进去是
[`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)。参见
[`MaterialAttributes`](../graph/material-attributes.md)。

<a id="substrateconverttodecal"></a>

### Substrate.ConvertToDecal

把 Substrate 材质转换成贴花材质。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `DecalMaterial` | **Substrate** | 是 |
| `Coverage` | 数值 | 是 |

输出：Substrate 值。

<a id="substratehorizontalmix"></a>

### Substrate.HorizontalMix

在屏幕空间水平混合两个 Substrate 材质。也可以写成
**`Substrate.HorizontalMixing`** —— 两个名字可以互换。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Background` | **Substrate** | 是 |
| `Foreground` | **Substrate** | 是 |
| `Mix` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateverticallayer"></a>

### Substrate.VerticalLayer

把一个 Substrate 材质叠在另一个之上。也可以写成 **`Substrate.VerticalLayering`** —— 两个
名字可以互换。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Top` | **Substrate** | 是 |
| `Base` | **Substrate** | 是 |
| `Thickness` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateadd"></a>

### Substrate.Add

相加两个 Substrate 材质。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `B` | **Substrate** | 是 |

输出：Substrate 值。

<a id="substrateweight"></a>

### Substrate.Weight

缩放一个 Substrate 材质的贡献。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `Weight` | 数值 | 是 |

输出：Substrate 值。

<a id="substrateselect"></a>

### Substrate.Select

在两个 Substrate 材质之间做静态选择。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `B` | **Substrate** | 是 |
| `SelectValue` | 数值 | 是 |

输出：Substrate 值。

<a id="substratetransmittancetomfp"></a>

### Substrate.TransmittanceToMFP

工具。把透射颜色和厚度转换成平均自由程参数化。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `TransmittanceColor` | 数值 | 是 |
| `Thickness` | 数值 | 是 |

输出 —— 数值，可用 `Output=` 或 `OutputIndex=` 选择：

| 索引 | 名字 |
| :-- | :-- |
| 0（默认） | `MFP` |
| 1 | `Thickness` |

<a id="substratemetalnesstodiffusealbedof0"></a>

### Substrate.MetalnessToDiffuseAlbedoF0

工具。把传统的 base color / metallic / specular 三元组转换成 slab 参数化。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | 是 |
| `Metallic` | 数值 | 是 |
| `Specular` | 数值 | 是 |

输出 —— 数值：

| 索引 | 名字 |
| :-- | :-- |
| 0（默认） | `DiffuseAlbedo` |
| 1 | `F0` |

<a id="substratehazinesstosecondaryroughness"></a>

### Substrate.HazinessToSecondaryRoughness

工具。把 haziness 控制量转换成第二粗糙度波瓣。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `BaseRoughness` | 数值 | 是 |
| `Haziness` | 数值 | 是 |

输出 —— 数值：

| 索引 | 名字 |
| :-- | :-- |
| 0（默认） | `Second Roughness` |
| 1 | `Second Roughness Weight` |

<a id="substratethinfilm"></a>

### Substrate.ThinFilm

工具。计算薄膜干涉的高光颜色。

| 参数 | 值 | 补全 |
| :-- | :-- | :-- |
| `Normal` | 数值 | 是 |
| `F0` | 数值 | 是 |
| `F90` | 数值 | 是 |
| `Thickness` | 数值 | 是 |
| `IOR` | 数值 | 是 |

输出 —— 数值：

| 索引 | 名字 |
| :-- | :-- |
| 0（默认） | `Specular Color` |
| 1 | `Edge Specular Color` |

---

<a id="binding-a-substrate-value-to-basefrontmaterial"></a>

## 绑定 Substrate 值到 Base.FrontMaterial

Substrate 材质经由 `Base.FrontMaterial` 输出绑定进入生成的 `UMaterial`。

```c
Outputs = {
    Substrate Surface;              // declared-type token; case-insensitive, no other spelling
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

| 规则 | 行为 |
| :-- | :-- |
| 声明类型 token | 只有 `Substrate` 一种拼写，大小写不限。`Strata` **不是**类型 token。 |
| 绑定表达式 | `Base.FrontMaterial = …` 的右侧可以是它自己的表达式（`Base.FrontMaterial = Substrate.Layer(Coat, Body);`）*（自 2.0.0 起）* |
| 着色模型 | DreamShader 不为该绑定设置任何着色模型 *（自 2.0.0 起；1.x 强制设为 Substrate）*；`Settings` `ShadingModel` 照写应用 |
| 值的来源 | 带 `Substrate.*` 节点的 `Graph` 块；`Function` —— 即 HLSL Custom 节点 —— 无法产生该值（`DSH6253`） |
| 值的种类 | 绑定到 `Base.FrontMaterial` 的数值，或绑定到数值 pin 的 Substrate 值，都不适用（`DSH5214` / `DSH4228`） |
| 引擎 | 目录中列出了 `FrontMaterial` 属性的引擎；否则 `DSH5200` |

> [!NOTE]
> `Strata` 是改名前的拼写。它被接受为 `Settings = { ShadingModel = … }` 值
> （同一着色模型的别名），但永远不能作为类型 token，也不能作为调用命名空间。
> 参见[枚举取值](../settings/material-enums.md)。

<a id="notes"></a>

## 说明

- **每个 Substrate 类都是一个名字。** 在[目录](#catalogue)中没有行的类，按派生的名字调用 ——
  UE 5.8 上为 `Substrate.Toon` —— 或通过
  `UE.Expression(Class = "SubstrateToonBSDF", …)` 按其类调用，后者可解析 `SubstrateToonBSDF`、
  `MaterialExpressionSubstrateToonBSDF`、`UMaterialExpressionSubstrateToonBSDF` 以及完整的
  `/Script/Engine.…` 路径 *（自 2.0.0 起）*。参见 [`UE.Expression`](ue-expression.md#class-resolution)。
- **`UE.*` 名称在此命名空间内不适用。** `Substrate.TexCoord(…)` 并不存在；该命名空间
  没有的名字属于 [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)。
- Substrate 节点**参与节点复用**：两个参数值完全相同的 `Substrate.Slab(…)` 调用是同一个节点。参见[节点复用](../graph/node-reuse.md)。
- [`GraphFunction`](../language/graph-function.md) 体内的 `Substrate.` 调用不会被提升为
  节点输入，因为没有 Custom 节点输入承载 Substrate 值；它留在 HLSL 文本中，并给出警告
  [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316)。
- [反编译器](../tools/decompiler.md)能把现有 Substrate 图导出回源码，
  并根据每条连接的写掩码推导通道 swizzle *（自 1.5.0 起）*，且按源码使用的名字写出每个节点
  —— 参见[反编译](../language-v2/substrate.md#decompiling)。
- `Substrate.*` 接口导出给编辑器工具，以 `Saved/DreamShader/Bridge/substrate-builtins.json`
  （schema `DreamShader.SubstrateBuiltins`，版本 1），每个名字一条，含其 `qualifiedName`、`className`、`outputType`、
  `isSubstrateOutput` 和精选的 `parameters`。参见 [Bridge](../../tools/bridge.md)。

<a id="diagnostics"></a>

## 诊断

每个代码及其消息和完整说明都列在
[诊断](../diagnostics/index.md)中的各自页面上。

<a id="call-site"></a>

### 调用点

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) | 名字不是当前运行引擎的 Substrate 类 —— UE 5.4 以下的每次 `Substrate.*` 调用 |
| [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294) | 节点从更新的引擎才有（`Select`、`Toon`），或运算符、虚拟参数需要此引擎没有的节点 |
| [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)（警告） | 在 1.x 源码中，名字、参数或输出只在忽略大小写时匹配 |
| [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216) | 提供了 `Class=` |
| [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220)、[`DSH5221`](../diagnostics/DSH5xxx.md#dsh5221) | BSDF 上的位置参数，或超出组合节点的参数顺序 |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | 参数不是该类的 pin、属性或虚拟参数 |
| `DSH5291`（信息）、`DSH8212` | 在 1.x 源码中，此类带值的参数，以及不带该 pin 的已构建节点 |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | 给 pin 提供了错误种类的值 |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | 给属性提供了非常量的内容 |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | 某个参数被给了两次 |
| [`DSH5295`](../diagnostics/DSH5xxx.md#dsh5295)、[`DSH5296`](../diagnostics/DSH5xxx.md#dsh5296) | 虚拟参数彼此冲突或与 pin 冲突，或缺少配套项 |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279)（警告） | 在 1.x 源码中，必需的 pin 未连接 |
| `DSH5250`–`DSH5252`、`DSH5201`、`DSH5282` | 输出选择器 —— 参见 [`UE.Expression`](ue-expression.md#selecting-an-output) |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 节点无法创建 |
| [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) | 属性值无法写入 |

<a id="substrate-values-elsewhere-in-the-pipeline"></a>

### 管线其他环节中的 Substrate 值

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293) | `+` 和 `* <scalar>` 之外的运算符、复合赋值，或 Substrate 值与数值的 `lerp` |
| [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) | 其他数学内置函数被传入 Substrate 值 |
| [`DSH5297`](../diagnostics/DSH5xxx.md#dsh5297)–[`DSH5299`](../diagnostics/DSH5xxx.md#dsh5299)、[`DSH4383`](../diagnostics/DSH4xxx.md#dsh4383) | 按成员逐步构建的 Substrate 值 —— 参见[2.0 页面](../language-v2/substrate.md#building-a-value-member-by-member) |
| [`DSH4378`](../diagnostics/DSH4xxx.md#dsh4378) | 在 Substrate 值与数值之间的分支，或 UE 5.6 以下对 Substrate 值的运行时分支 |
| [`DSH4380`](../diagnostics/DSH4xxx.md#dsh4380)（警告） | 运行时分支在两个不同 BSDF 类之间选择 |
| [`DSH4381`](../diagnostics/DSH4xxx.md#dsh4381) | 此引擎缺少虚拟参数的转换节点 |
| [`DSH4382`](../diagnostics/DSH4xxx.md#dsh4382) | `Substrate = Native` 驱动 `FrontMaterial`，且项目中未开启 Substrate |
| [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | `Function` 接收或返回 Substrate 值 |
| [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316)（警告） | `GraphFunction` 体内的 `Substrate.` 调用留在 HLSL 文本中 |
| [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200) | `Base.FrontMaterial` 用在目录中没有该属性的引擎上 |
| [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215) | UE 5.4 以下使用 `ShadingModel = "Substrate"` 或 `"Strata"`（消息携带 `DSH7128`） |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Substrate")
{
    Properties = {
        vec3  BaseColor = vec3(0.6, 0.1, 0.1);
        float Metallic  = 0.0;
        float Specular  = 0.5;
        float Rough     = 0.3;
        vec3  Glow      = vec3(0.1, 0.6, 1.0);
    }

    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        // Utility node: two numeric outputs, selected by name.
        vec3 Albedo = Substrate.MetalnessToDiffuseAlbedoF0(
            BaseColor = BaseColor, Metallic = Metallic, Specular = Specular,
            Output = "DiffuseAlbedo");
        vec3 F0 = Substrate.MetalnessToDiffuseAlbedoF0(
            BaseColor = BaseColor, Metallic = Metallic, Specular = Specular,
            Output = "F0");

        Substrate Body = Substrate.Slab(
            DiffuseAlbedo = Albedo,
            F0            = F0,
            Roughness     = Rough);

        Substrate Emissive = Substrate.Unlit(EmissiveColor = Glow);

        Surface = Substrate.Add(A = Body, B = Emissive);
    }
}
```

生成的节点：

```text
SubstrateMetalnessToDiffuseAlbedoF0   -> output "DiffuseAlbedo"  -> Albedo
                                      -> output "F0"             -> F0
                                         (one node, read for both)
SubstrateSlabBSDF                     -> Body                    (Substrate value)
SubstrateUnlitBSDF                    -> Emissive                (Substrate value)
SubstrateAdd                          -> Surface                 (Substrate value)
```

自 2.0.0 起，同一张图可写为
`Surface = Substrate.Slab(BaseColor = BaseColor, Metallic = Metallic, Specular = Specular, Roughness = Rough) + Substrate.Unlit(EmissiveColor = Glow);`
—— 虚拟参数把转换节点放在 slab 前面，`+` 就是 `Substrate.Add`。

<a id="see-also"></a>

## 另请参见

- [内置](index.md) — `Graph` 内可用的调用接口
- [`UE.*` 目录](ue.md) — 同级的命名空间
- [`UE.Expression`](ue-expression.md) — 每次节点调用共用的绑定规则
- [Substrate 语法糖](../language-v2/substrate.md) — 运算符、`lerp`、分支、虚拟参数、构建器、`Substrate =`
- [`OutputType` 值](output-type.md) — 在这个命名空间上被舍弃
- [数学内置](math.md) — 无前缀的数值调用接口
- [变换内置](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [输出绑定](../language/output-bindings.md) — `Base.FrontMaterial` 与完整的绑定目标列表
- [`MaterialAttributes`](../graph/material-attributes.md) — `ConvertMaterialAttributes` 消费的值类型
- [类型](../language/types.md) — `Substrate` 声明类型 token
- [枚举值](../settings/material-enums.md) — `ShadingModel = "Substrate"` / `"Strata"`
- [调用](../graph/calls.md) — 命名参数语法
- [节点复用](../graph/node-reuse.md) — 相同的调用与节点
- [Bridge](../../tools/bridge.md) — `substrate-builtins.json` 与编辑器补全清单
- [反编译器](../tools/decompiler.md) — 把已有的 Substrate 图导回源码
- [诊断索引](../diagnostics/index.md) — 每一种代码
