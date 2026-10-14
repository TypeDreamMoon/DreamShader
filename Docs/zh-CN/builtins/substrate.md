---
title: Substrate 节点
description: Substrate.* 调用命名空间 —— 运行中引擎的每个 Substrate BSDF、组合与工具节点，按名字调用。需要 UE 5.4+。
---

`Substrate.*` 是与 [`UE.*`](ue.md) 并列的调用命名空间，装着 Unreal 的 Substrate BSDF、组合和工具
材质节点。（自 2.0.0 起） 起它就是内置目录的 `Substrate` 命名空间：运行中引擎的每一个 Substrate 类，按名字调用 ——
下面[目录](#目录)里的 24 个名字、目录的短名，以及由任何其他类推导出的名字。

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Glow);
}
```

<a id="可用性"></a>

## 可用性

（自 UE 5.4 起）

绑定器里没有任何按版本编译进或编译出的东西 （自 2.0.0 起）：目录列出运行中引擎的 Substrate 类，而 UE 5.4 之前的
引擎一个都没有。

| 使用面 | 在没有该节点的引擎上 |
| :-- | :-- |
| `Substrate.<Name>(…)` | `DSH5210` —— `Select`（UE 5.6）和 `Toon`（UE 5.8）报 `DSH5294`，消息会写出哪个引擎版本才有 |
| `A + B`、`A * w`、`lerp(A, B, t)`、Slab 的虚拟参数 | `DSH5294`、`DSH4381` |
| 两个 Substrate 值上的运行时分支 | UE 5.6 以下报 `DSH4378` |
| 通用 `UE.*` 调用上的 `OutputType="Substrate"` | 和任何 `OutputType` 一样被丢弃 —— 见 [OutputType](ue-expression.md#outputtype) |
| `Base.FrontMaterial` 输出绑定 | 由目录为该引擎列出的材质属性决定；没有这个名字报 `DSH5200` |
| `Settings = { ShadingModel = "Substrate"; }`（以及 `"Strata"` 写法） | UE 5.4 以下报 `DSH8215`，消息里带 `DSH7128` |

**提示：**

项目还要启用 Substrate（*Project Settings ▸ Engine ▸ Rendering ▸ Substrate*），Substrate 图才能在引擎里编译。
（自 2.0.0 起） 起 DreamShader 会读这个设置：`Shader` 的 `Substrate` 设置 —— `Legacy`、`Bridge` 或 `Native` ——
决定材质在开和不开 Substrate 的工程里怎么建；`Native` 驱动 `FrontMaterial` 而工程没开 Substrate 时报 `DSH4382`。
见[一份源码，两种工程](../language-v2/substrate.md#一份源码两种工程)。

**提示：**

编辑器工具链跟随引擎。UE 5.4 以下，清单文件 `Saved/DreamShader/Bridge/substrate-builtins.json` 会被写成空条目数组、
`supported: false` 和一个 `unsupportedReason` —— 所以编辑器扩展的补全里什么都没有。参见[编辑器工具](../tools/editor-integration.md)。

<a id="语法概要"></a>

## 语法概要

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
Substrate . <Name> ( [ <argument-name> = <expression> ]
                     [ , <argument-name> = <expression> ] …
                     [ , { Output | OutputName } = "<pin-name>" ]
                     [ , OutputIndex = <integer> ] )
```

1.x 源码里前缀**不区分大小写** —— `Substrate` 是 1.x 的类型拼写，类型拼写都忽略大小写读取 —— 只在忽略大小写时
匹配的 `<Name>` 也接受，给出警告 `DSH5276`：`SUBSTRATE.UNLIT(…)` 能解析。参数名同样：精确匹配，或忽略大小写并给出
`DSH5276`；此外不会去掉任何分隔符。

<a id="参数模型"></a>

## 参数模型

`Substrate.*` 调用和任何反射节点调用（[`UE.Expression`](ue-expression.md)）一样，对名字选中的类绑定。

| 规则 | 行为 |
| :-- | :-- |
| 参数要**命名** | 五个组合节点例外，它们也按位置接受操作数 （自 2.0.0 起）：`Add(A, B)`、`Weight(A, Weight)`、`HorizontalMix(Background, Foreground, Mix)`、`VerticalLayer(Top, Base, Thickness)`、`Select(A, B, SelectValue)`。其他地方的位置参数报 `DSH5220` |
| **拒绝** `Class=` | `DSH5216` —— 名字就是类 |
| `OutputType=` / `ResultType=` **被丢弃** | 由 legacy 前端丢弃，不报诊断；目录给调用定类型 |
| `Output=` / `OutputName=` | 按名字选择输出 |
| `OutputIndex=` | 按 0 起始的索引选择输出 |
| `Output`/`OutputName` 与 `OutputIndex` 同时出现 | `DSH5252` |
| 其他参数 | 按目录绑定，见下 |

<a id="由反射驱动的绑定"></a>

### 由反射驱动的绑定

对每个剩余参数，绑定器按顺序尝试：

1. **输入 pin** —— 类的 pin 名，或其别名：pin 的显示名写成标识符（`Diffuse_Albedo`），属性的显示名去掉空格也算。
2. **属性** —— 类的可编辑反射属性名或别名；bool 属性也可以去掉开头的 `b`，所以 `UseParameterBlending` 能触达
   `bUseParameterBlending`。
3. **虚拟参数** —— 在有对应 pin 的 BSDF 上，`BaseColor` / `Metallic` / `Specular`、`Haziness`、
   `Transmittance` / `Thickness` 和 `IOR` 会变成 pin 前面一个转换节点的输入 （自 2.0.0 起）。见
   [Slab 上的旧参数](../language-v2/substrate.md#slab-上的旧参数)。
4. 1.x 源码里，以上都不是、但带着值的名字以 `DSH5291` 保留，建节点时按名字连接（节点没有这个 pin 时 `DSH8212`）。
5. 都不匹配 → `DSH5213`。

pin 名优先于同名属性。值按 pin 的类型检查：Substrate pin 上的数值、数值 pin 上的 Substrate 值、`MaterialAttributes`
pin 上任何不是 material 的值，都报 `DSH5214`。

**提示：**

**名字带空格的引擎 pin 现在能触达** （自 2.0.0 起）：显示为 `Diffuse Albedo` 的 pin 写作 `Diffuse_Albedo`；它的
属性名 —— `DiffuseAlbedo`，也就是下面各表列出的名字 —— 照旧可用。

**提示：**

**可接受的参数名跟随引擎，而不是插件。** 目录读自运行中引擎的 `UMaterialExpressionSubstrate*` 类，所以引擎版本增删
某个输入，它就随之出现或消失，插件不需要改动。下面各表列出这些类暴露的输入；**补全**列标记的是 DreamShader 发布到
编辑器补全和 Bridge 清单的精选子集，那个子集*是*插件固定的。同一个类上任何可编辑的非输入属性 —— 枚举、浮点、
布尔 —— 即使这里没有列出，也同样可以作为字面量参数设置。

<a id="选择输出"></a>

### 选择输出

`Output=` / `OutputName=` 按目录列出的输出名精确匹配，**包括带空格的名字**；1.x 源码里只差大小写的匹配也接受，给出
`DSH5276`。没有这个输出报 `DSH5201`。`OutputIndex=` 取 0 起始的索引（超过最后一个报 `DSH5282`）。只有下面的工具节点
有多个输出。

<a id="目录"></a>

## 目录

*Substrate 输出*标记输出是 Substrate 材质值、可绑定到 `Base.FrontMaterial` 的类；标为**否**的四个是工具节点，返回普通数值。

| `Substrate.<Name>` | `UMaterialExpression` 类 | Substrate 输出 |
| :-- | :-- | :-- |
| `ShadingModels` | `UMaterialExpressionSubstrateShadingModels` | 是 |
| `Slab` | `UMaterialExpressionSubstrateSlabBSDF` | 是 |
| `SimpleClearCoat` | `UMaterialExpressionSubstrateSimpleClearCoatBSDF` | 是 |
| `VolumetricFogCloud` | `UMaterialExpressionSubstrateVolumetricFogCloudBSDF` | 是 |
| `Unlit` | `UMaterialExpressionSubstrateUnlitBSDF` | 是 |
| `Hair` | `UMaterialExpressionSubstrateHairBSDF` | 是 |
| `Eye` | `UMaterialExpressionSubstrateEyeBSDF` | 是 |
| `SingleLayerWater` | `UMaterialExpressionSubstrateSingleLayerWaterBSDF` | 是 |
| `LightFunction` | `UMaterialExpressionSubstrateLightFunction` | 是 |
| `PostProcess` | `UMaterialExpressionSubstratePostProcess` | 是 |
| `UI` | `UMaterialExpressionSubstrateUI` | 是 |
| `ConvertMaterialAttributes` | `UMaterialExpressionSubstrateConvertMaterialAttributes` | 是 |
| `ConvertToDecal` | `UMaterialExpressionSubstrateConvertToDecal` | 是 |
| `HorizontalMix` | `UMaterialExpressionSubstrateHorizontalMixing` | 是 |
| `HorizontalMixing` —— `HorizontalMix` 的别名 | `UMaterialExpressionSubstrateHorizontalMixing` | 是 |
| `VerticalLayer` | `UMaterialExpressionSubstrateVerticalLayering` | 是 |
| `VerticalLayering` —— `VerticalLayer` 的别名 | `UMaterialExpressionSubstrateVerticalLayering` | 是 |
| `Add` | `UMaterialExpressionSubstrateAdd` | 是 |
| `Weight` | `UMaterialExpressionSubstrateWeight` | 是 |
| `Select` （自 UE 5.6 起） | `UMaterialExpressionSubstrateSelect` | 是 |
| `TransmittanceToMFP` | `UMaterialExpressionSubstrateTransmittanceToMFP` | **否** |
| `MetalnessToDiffuseAlbedoF0` | `UMaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0` | **否** |
| `HazinessToSecondaryRoughness` | `UMaterialExpressionSubstrateHazinessToSecondaryRoughness` | **否** |
| `ThinFilm` | `UMaterialExpressionSubstrateThinFilm` | **否** |

两对别名是完全的重复 —— 同一个类、同样的输入、同样的输出类型，两种写法都没有被弃用。（自 2.0.0 起） 起又多了两个
短名 —— `Mix` 即 `HorizontalMix`、`Layer` 即 `VerticalLayer` —— 而且每个类都能用目录短名（`SubstrateSlabBSDF`）调用，
也能用类名去掉前面的 `MaterialExpressionSubstrate` 和后面的 `BSDF` 调用；这就是调用本表没有列出的类的方式：UE 5.8 的
`UMaterialExpressionSubstrateToonBSDF` 是 `Substrate.Toon`。

<a id="输出类型"></a>

## 输出类型

目录按引擎给每个输出定类型：BSDF 或组合节点产出 `Substrate` 值；工具节点产出数值，宽度按引擎给每个输出的类型 —— 引擎
没给类型时，和它被读入的位置一样宽。多输出的节点要指名读取一个输出。

`Substrate` 值可以：

- 赋给 `Substrate` 类型的 `Graph` 变量或 `Outputs` 声明；
- 传给另一个 `Substrate.*` 节点或 [`UE.Expression`](ue-expression.md) 节点的 Substrate 类型输入；
- 用 `+` 组合（`Substrate.Add`）、用 `* <标量>` 加权（`Substrate.Weight`）、用 `lerp(A, B, t)` 混合
  （`Substrate.HorizontalMix`）（自 2.0.0 起） —— 见[运算符与 lerp](../language-v2/substrate.md#运算符与-lerp)；
- 由静态开关选择，或由运行时 `if` 选择 —— 后者变成 `Substrate.Select` （自 2.0.0 起） —— 见
  [Substrate 值上的分支](../language-v2/substrate.md#substrate-值上的分支)；
- 绑定到 `Base.FrontMaterial`。

Substrate 值上的其他运算符 —— `-`、`/`、`A * B`、复合赋值 —— 以及 Substrate 值与数值之间的 `lerp` 报 `DSH5293`；
把 Substrate 值传给其他数学内置报 `DSH4226`。HLSL Custom 节点 —— `Function` —— 既不能接收也不能返回 Substrate 值
（`DSH6253`）。

**提示：**

主材质靠「两个 closure 上的静态开关」从静态参数里选着色变体：`UMaterialExpressionStaticSwitchParameter` 在引擎构建材质
拓扑树时就决出布尔值、只下降到选中的分支，所以翻译器看到的仍是一个拓扑。
[`UE.StaticSwitchParameter`](ue.md#uestaticswitchparameter) 不比较两条分支，所以一边是 Substrate 值、
一边是数值时 DreamShader 不会报错。

<a id="节点参考"></a>

## 节点参考

<a id="bsdf"></a>

### BSDF

<a id="substrateshadingmodels"></a>

#### Substrate.ShadingModels

用 Substrate 材质表达的传统着色模型表面。编辑器补全不为这个包装节点提供任何参数；下面所有参数仍然可用。

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
| `ShadingModel` （自 2.0.1 起） | 着色模型，`UE.ShadingModel(ShadingModel = …)`；不连时用节点的 `ShadingModelOverride` | — |

**提示：**

引擎把 `ShadingModel` pin 声明为 `FShadingModelMaterialInput` —— 材质自身 pin 的那种类型 —— 2.0.1 之前目录只把
`FExpressionInput` 和 `FMaterialAttributesInput` 当作 pin，所以调用连不上它，反编译器会丢掉这条连线（`DSH9070`），
`Substrate = Bridge` 也会漏掉计算出来的着色模型（`m.ShadingModel = UE.ShadingModel(...)`）。现在这一类都是 pin；它不是
必需的。

<a id="substrateslab"></a>

#### Substrate.Slab

通用的 Substrate BSDF slab —— 通常从它开始。

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

<a id="substratesimpleclearcoat"></a>

#### Substrate.SimpleClearCoat

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

<a id="substrateunlit"></a>

#### Substrate.Unlit

只有自发光的 BSDF。最小的完整 Substrate 表面。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `EmissiveColor` | 数值 | 是 |
| `TransmittanceColor` | 数值 | — |
| `Normal` | 数值 | — |

<a id="substratehair"></a>

#### Substrate.Hair

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | 是 |
| `Scatter` | 数值 | 是 |
| `Specular` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `Backlit` | 数值 | 是 |
| `Tangent` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |

<a id="substrateeye"></a>

#### Substrate.Eye

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `DiffuseColor` | 数值 | 是 |
| `Roughness` | 数值 | 是 |
| `CorneaNormal` | 数值 | 是 |
| `IrisNormal` | 数值 | 是 |
| `IrisPlaneNormal` | 数值 | 是 |
| `IrisMask` | 数值 | 是 |
| `IrisDistance` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |

<a id="substratesinglelayerwater"></a>

#### Substrate.SingleLayerWater

| 参数 | 值类型 | 补全 |
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

<a id="substratevolumetricfogcloud"></a>

#### Substrate.VolumetricFogCloud

用于体积雾和云材质的参与介质 BSDF。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Albedo` | 数值 | 是 |
| `Extinction` | 数值 | 是 |
| `EmissiveColor` | 数值 | 是 |
| `AmbientOcclusion` | 数值 | 是 |

<a id="域输出节点"></a>

### 域输出节点

<a id="substratelightfunction"></a>

#### Substrate.LightFunction

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |

<a id="substratepostprocess"></a>

#### Substrate.PostProcess

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |
| `Opacity` | 数值 | 是 |

<a id="substrateui"></a>

#### Substrate.UI

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Color` | 数值 | 是 |
| `Opacity` | 数值 | 是 |

<a id="转换"></a>

### 转换

<a id="substrateconvertmaterialattributes"></a>

#### Substrate.ConvertMaterialAttributes

把 `MaterialAttributes` 值转换成 Substrate 材质。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `MaterialAttributes` | **`MaterialAttributes`** | 是 |
| `Attributes` —— `MaterialAttributes` 的别名 | **`MaterialAttributes`** | — |
| `WaterScatteringCoefficients` | 数值 | 是 |
| `WaterAbsorptionCoefficients` | 数值 | 是 |
| `WaterPhaseG` | 数值 | 是 |
| `ColorScaleBehindWater` | 数值 | 是 |

`MaterialAttributes` 是反射属性名，`Attributes` 是引擎给同一个输入起的 pin 名；两者都绑定 input 0。传数值进去报 `DSH5214`。

<a id="substrateconverttodecal"></a>

#### Substrate.ConvertToDecal

把 Substrate 材质转换成贴花材质。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `DecalMaterial` | **Substrate** | 是 |
| `Coverage` | 数值 | 是 |

<a id="组合"></a>

### 组合

<a id="substratehorizontalmix"></a>

#### Substrate.HorizontalMix

在屏幕空间水平混合两个 Substrate 材质。也可以写成 **`Substrate.HorizontalMixing`** 或 `Substrate.Mix`。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Background` | **Substrate** | 是 |
| `Foreground` | **Substrate** | 是 |
| `Mix` | 数值 | 是 |

<a id="substrateverticallayer"></a>

#### Substrate.VerticalLayer

把一个 Substrate 材质叠在另一个之上。也可以写成 **`Substrate.VerticalLayering`** 或 `Substrate.Layer`。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Top` | **Substrate** | 是 |
| `Base` | **Substrate** | 是 |
| `Thickness` | 数值 | 是 |

<a id="substrateadd"></a>

#### Substrate.Add

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `B` | **Substrate** | 是 |

<a id="substrateweight"></a>

#### Substrate.Weight

缩放一个 Substrate 材质的贡献。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `Weight` | 数值 | 是 |

<a id="substrateselect"></a>

#### Substrate.Select

（自 UE 5.6 起）

在两个 Substrate 材质之间做选择。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `A` | **Substrate** | 是 |
| `B` | **Substrate** | 是 |
| `SelectValue` | 数值 | 是 |

<a id="工具节点"></a>

### 工具节点

这四个返回普通数值而不是 Substrate 值，也是唯一有多个输出的节点。用 `Output=` 或 `OutputIndex=` 选择。

<a id="substratetransmittancetomfp"></a>

#### Substrate.TransmittanceToMFP

把透射颜色和厚度转换成平均自由程参数化。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `TransmittanceColor` | 数值 | 是 |
| `Thickness` | 数值 | 是 |

| 输出索引 | 名字 |
| :-- | :-- |
| 0（默认） | `MFP` |
| 1 | `Thickness` |

<a id="substratemetalnesstodiffusealbedof0"></a>

#### Substrate.MetalnessToDiffuseAlbedoF0

把传统的 base color / metallic / specular 三元组转换成 slab 参数化。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `BaseColor` | 数值 | 是 |
| `Metallic` | 数值 | 是 |
| `Specular` | 数值 | 是 |

| 输出索引 | 名字 |
| :-- | :-- |
| 0（默认） | `DiffuseAlbedo` |
| 1 | `F0` |

<a id="substratehazinesstosecondaryroughness"></a>

#### Substrate.HazinessToSecondaryRoughness

把 haziness 控制量转换成第二粗糙度波瓣。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `BaseRoughness` | 数值 | 是 |
| `Haziness` | 数值 | 是 |

| 输出索引 | 名字 |
| :-- | :-- |
| 0（默认） | `Second Roughness` |
| 1 | `Second Roughness Weight` |

输出名里带空格，作为整体名字匹配：`Output = "Second Roughness Weight"`。

<a id="substratethinfilm"></a>

#### Substrate.ThinFilm

计算薄膜干涉的高光颜色。

| 参数 | 值类型 | 补全 |
| :-- | :-- | :-- |
| `Normal` | 数值 | 是 |
| `F0` | 数值 | 是 |
| `F90` | 数值 | 是 |
| `Thickness` | 数值 | 是 |
| `IOR` | 数值 | 是 |

| 输出索引 | 名字 |
| :-- | :-- |
| 0（默认） | `Specular Color` |
| 1 | `Edge Specular Color` |

<a id="绑定到-basefrontmaterial"></a>

## 绑定到 Base.FrontMaterial

Substrate 材质经由 [`Base.FrontMaterial`](../generation/outputs.md#basefrontmaterial) 输出绑定进入生成的 `UMaterial`。

```c
Outputs = {
    Substrate Surface;              // 声明类型 token；不区分大小写，没有别的写法
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

| 规则 | 行为 |
| :-- | :-- |
| 声明类型 token | 只有 `Substrate` 一种写法，大小写不限。`Strata` **不是**类型 token |
| 绑定表达式 | `Base.FrontMaterial = …` 的右边可以是一个表达式（`Base.FrontMaterial = Substrate.Layer(Coat, Body);`）（自 2.0.0 起） |
| 着色模型 | DreamShader 不为这个绑定设置任何着色模型 （自 2.0.0 起）（1.x 强制设为 Substrate）；`Settings` 里的 `ShadingModel` 照写应用 |
| 值的来源 | 带 `Substrate.*` 节点的 `Graph` 块；`Function` —— HLSL Custom 节点 —— 产生不了（`DSH6253`） |
| 值的种类 | 数值绑定到 `Base.FrontMaterial`，或 Substrate 值绑定到数值 pin，都放不进去（`DSH5214` / `DSH4228`） |
| 引擎 | 目录列出了 `FrontMaterial` 属性的引擎；否则 `DSH5200` |

**提示：**

`Strata` 是改名前的写法。它作为 `Settings = { ShadingModel = … }` 的值是可接受的 —— 同一个着色模型的别名 —— 但永远
不能作为类型 token，也不能作为调用命名空间。参见[枚举取值](../settings/material-enums.md)。

<a id="一些值得知道的细节"></a>

## 一些值得知道的细节

- **每个 Substrate 类都是一个名字。** [目录](#目录)里没有行的类按推导出的名字调用 —— UE 5.8 上的 `Substrate.Toon` ——
  或通过 `UE.Expression(Class = "SubstrateToonBSDF", …)` 按类调用，`SubstrateToonBSDF`、`MaterialExpressionSubstrateToonBSDF`、
  `UMaterialExpressionSubstrateToonBSDF` 和完整的 `/Script/Engine.…` 路径都能解析 （自 2.0.0 起）。参见
  [类解析](ue-expression.md#类解析)。
- **`UE.*` 的名字在这个命名空间里不适用。** 没有 `Substrate.TexCoord(…)` 这种东西；命名空间里没有的名字报 `DSH5210`。
- Substrate 节点**参与**节点复用：两个参数值相同的 `Substrate.Slab(…)` 调用是一个节点。参见
  [节点复用](../graph/expressions.md#节点复用)。
- [`GraphFunction`](../language/functions.md) 体里的 `Substrate.` 调用不会被提升成节点输入 —— 没有 Custom 节点输入能承载
  Substrate 值；它留在 HLSL 文本里，并给出警告 `DSH6316`。
- [反编译器](../tools/decompiler.md)能把已有的 Substrate 图导回源码，并从每条连接的写掩码推导通道 swizzle
  （自 1.5.0 起）。（自 2.0.2 起） 起它把每个 Substrate 节点**按名字**写出 —— slab 的 SSS MFP 上的 Transmittance-To-MFP 写成
  `SSSMFP = Substrate.TransmittanceToMFP(TransmittanceColor = ..., Thickness = ...).MFP`、Coverage Weight 写成
  `Substrate.Weight(A = ..., Weight = ...)`，而不是图上等价、文本里却搜不到的语法糖（`Slab * w`、`+`、`lerp`）。语法糖只在可读
  形式（`-Readable`、Bridge 的 `readable`）里读回；用语法糖写的源码照常编译。
- 完整的 `Substrate.*` 面会导出给编辑器工具链，写入 `Saved/DreamShader/Bridge/substrate-builtins.json`（schema
  `DreamShader.SubstrateBuiltins`，version 1），每个名字一条，含 `qualifiedName`、`className`、`outputType`、
  `isSubstrateOutput` 和精选的 `parameters`。参见[编辑器工具](../tools/editor-integration.md)。

<a id="诊断"></a>

## 诊断

<a id="调用点"></a>

### 调用点

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5210: '{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive.` | 名字不是运行中引擎的 Substrate 类 —— UE 5.4 以下每个 Substrate.\* 调用都是如此。 | 低于 UE 5.4 无法使用 Substrate，请改用传统着色模型。 |
| `DSH5294: 'Substrate.{0}' is a node Unreal Engine has from {1} on; this engine does not have it.` | 节点从更新的引擎才有（Select、Toon），或运算符、虚拟参数需要这个引擎没有的节点。 |  |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告，仅 1.x：名字、参数或输出只在忽略大小写时匹配。 |  |
| `DSH5216: 'Substrate.' already names the node, so it takes no 'Class' argument.` | 写了 Class=。 |  |
| `DSH5220: '{0}.{1}' takes named arguments: write 'Pin = value'.` | BSDF 上的位置参数（DSH5221：超出组合节点的参数顺序）。 | 所有参数都写名字。 |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 参数不是类的 pin、属性或虚拟参数。 |  |
| `DSH5214: {0} expects {1}, and this is {2}.` | pin 收到了错误种类的值 —— Substrate pin 上的数值、数值 pin 上的 Substrate 值、MaterialAttributes pin 上的非 material 值。 |  |
| `DSH4215: The '{0}' argument of '{1}' is given twice.` | 同一个参数给了两次。 |  |
| `DSH5295: '{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them.` | 虚拟参数彼此冲突，或与 pin 冲突（DSH5296：缺少配套的参数）。 |  [详解](../language-v2/substrate.md#slab-上的旧参数) |
| `DSH5201: '{0}.{1}' has no output called '{2}'; its outputs are {3}.` | 指定名字的输出不存在。 | 工具节点的输出名可能带空格，例如 "Second Roughness Weight"。 |
| `DSH5282: This Custom node declares {0} output(s), counted from 0, and this selects output {1}.` | OutputIndex 超过最后一个输出。 |  |
| `DSH8213: '{0}' could not take '{1}' for its '{2}' property. {3}` | 属性值写不进去。 |  |

<a id="管线其他环节中的-substrate-值"></a>

### 管线其他环节中的 Substrate 值

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5293: A Substrate value has no compound assignment; write 'S = S + T' (Substrate.Add) or 'S = S * w' (Substrate.Weight).` | + 和 \* 标量之外的运算符、复合赋值，或 Substrate 值与数值之间的 lerp。 | 分层用 Substrate.Layer，混合用 lerp，A += B 写成 A = A + B。 [详解](../language-v2/substrate.md#运算符与-lerp) |
| `DSH4226: {0} expects {1}, and this is {2}.` | 把 Substrate 值传给了其他数学内置。 |  [详解](math.md) |
| `DSH5297: '{0}' has been assigned a whole Substrate value since it was declared, and that value has no members; build a new value, or write the members before the assignment.` | 按成员逐步构建的 Substrate 值写错了（DSH5298、DSH5299、DSH4383 同属这一类）。 |  [详解](../language-v2/substrate.md#按成员逐步构建) |
| `DSH4378: A branch chooses between two {0} values or between two numbers, and this one has one of each.` | 分支一边是 Substrate 值一边是数值，或 UE 5.6 以下的 Substrate 值运行时分支。 |  [详解](../language-v2/substrate.md#substrate-值上的分支) |
| `DSH4380: This branch becomes a 'Substrate.Select', which parameter-blends its two inputs; they are a '{0}' and a '{1}', and the engine may refuse to blend unlike BSDFs.` | 警告：运行时分支在两个不同的 BSDF 类之间选择。 |  |
| `DSH4381: '{0}' is converted by the node 'Substrate.{1}', and this engine has no such node; give the pins it feeds directly.` | 虚拟参数需要的转换节点在这个引擎上不存在。 |  |
| `DSH4382: This material drives FrontMaterial and says 'Substrate = Native', and Substrate is off in this project; the engine could not compile the asset. Turn Substrate on, or write 'Substrate = Bridge' and the legacy attributes.` | Substrate = Native 驱动 FrontMaterial，而工程没开 Substrate。 |  [详解](../settings/material.md) |
| `DSH6253: '{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.` | Function 接收或返回 Substrate 值。 |  [详解](../language/functions.md) |
| `DSH6316: A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text.` | 警告：GraphFunction 体里的 Substrate. 调用留在 HLSL 文本里。 |  |
| `DSH5200: A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.` | 目录里没有 FrontMaterial 属性的引擎上用了 Base.FrontMaterial。 |  |
| `DSH8215: A material setting on '{0}' was refused. {1}` | UE 5.4 以下写了 ShadingModel = Substrate 或 Strata（消息带 DSH7128）。 |  [详解](../settings/material-enums.md) |

完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

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
        // 工具节点：两个数值输出，按名字选择。
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

（自 2.0.0 起） 起同一张图可以写成
`Surface = Substrate.Slab(BaseColor = BaseColor, Metallic = Metallic, Specular = Specular, Roughness = Rough) + Substrate.Unlit(EmissiveColor = Glow);`
—— 虚拟参数把转换节点放在 slab 前面，`+` 就是 `Substrate.Add`。

<a id="接下来"></a>

## 接下来

- [Substrate 语法糖](../language-v2/substrate.md) — 运算符、lerp、分支、虚拟参数、按成员构建，以及 Substrate = 设置。
  - [通用 UE.Expression](ue-expression.md) — 每个节点调用共用的绑定规则。
  - [输出绑定](../generation/outputs.md) — Base.FrontMaterial 与完整的绑定目标列表。
  - [枚举取值](../settings/material-enums.md) — ShadingModel = "Substrate" / "Strata"。
