---
title: "着色器设置"
description: "着色器设置"
---

<a id="shader-settings"></a>

# 着色器设置

> [DreamShader](../index.md) » [设置](index.md) » **着色器设置**

[`Shader`](../language/shader.md) 块的 `Settings` 段：少数几个手工处理的键，外加一个反射解析器，它会把其他任何键写入生成的 `UMaterial`。

| | |
| :-- | :-- |
| 声明位置 | `.dsm` —— 位于 `Shader( … ) { … }` 块内 |
| 类型 | 段 |
| 生成 | 在生成的 `UMaterial` 上写入属性（在 ThinCustom 后端下，写入隐藏的基础材质） |

<a id="synopsis"></a>

## 概要

```c
Shader(Name = "<asset-path>")
{
    Settings [=] {
        [BlendMode     = <blend-mode>;]     // or RenderType
        [ShadingModel  = <shading-model>;]
        [MaterialDomain = <domain>;]        // or Domain
        [Backend       = { Graph | ThinCustom };]
        [Substrate     = { Legacy | Bridge | Native };]
        [<property-name> = <literal>;] …
    }
}
```

块语法、注释处理、语句拆分和引号规则对每个 `Settings` 段都是通用的，其规定见[设置](index.md#how-a-statement-is-parsed)。自 2.0.0 起，一个键就是一个名字：嵌套路径或下标属于 [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261)。

这些键和值就是 2.0 `#pragma material(…)` 的那些；一个 `Shader` 的 `Settings` 变成一个。

<a id="special-keys"></a>

## 特殊键

这些键永远不会到达反射解析器。发射器会跳过六个名字——在修剪、转为小写**并**删除空格、`_` 和 `-` 之后——其结果为以下之一：

```text
blendmode   rendertype   shadingmodel   materialdomain   domain   backend
```

| 规范键 | 同义键 | 取值语法 | 键缺失时的值 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| **`BlendMode`** | `RenderType` | [混合模式拼写](material-enums.md#blendmode)之一 | `Opaque` | 设置 `UMaterial::BlendMode` |
| **`ShadingModel`** | — | [着色模型拼写](material-enums.md#shadingmodel)之一 | `DefaultLit` | 调用 `SetShadingModel` |
| **`MaterialDomain`** | `Domain` | [域拼写](material-enums.md#domain)之一 | `Surface` | 设置 `UMaterial::MaterialDomain` |
| **`Backend`** | — | `Graph` 或 `ThinCustom`；`Instance` 是 `ThinCustom` 的旧拼写 | 项目的 *Default Compiler Backend* | 选择物化策略——见[后端](backend.md) |

还有一个键由编译器读取，永远不会到达材质*（自 2.0.0 起）*：

| 键 | 取值语法 | 键缺失时的值 | 效果 |
| :-- | :-- | :-- | :-- |
| **`Substrate`** | `Legacy`、`Bridge`、`Native` | `Legacy` | 在启用了 Substrate 的项目中，如何读取按旧属性编写的材质——`Bridge` 把 Surface 材质的着色属性折叠到 `FrontMaterial` 上的一个 `Substrate.ShadingModels` 节点中。见 [Substrate 语法糖](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-)。未知模式会 [`DSH7232`](../diagnostics/DSH7xxx.md#dsh7232)。 |

在绑定源码时，`Backend` 和 `Substrate` 会从设置中取出，匹配时忽略大小写；其余的在构建材质时应用。

当规范键和它的同义键同时存在时，**规范**键胜出：`BlendMode` 胜过 `RenderType`，`MaterialDomain` 胜过 `Domain`。这种冲突没有诊断。

这些键的拼写只通过修剪并转小写来查找，因此 `blendmode`、`BlendMode` 和 `BLENDMODE` 都可以。

> [!WARNING]
> 仅以下划线区别于特殊键的拼写会被**静默丢弃**。
> `Blend_Mode = "Translucent";` 不会设置混合模式：对 `BlendMode` 的直接探测匹配不到
> 所存储的键 `blend_mode`，而反射遍历会跳过它，因为其去分隔符后的形式
> 在特殊键列表上。没有错误，没有警告，没有效果。`Render_Type`、
> `Shading_Model`、`Material_Domain` 和 `Back_end` 同理。请写成不带分隔符的名字。（自 2.0.0 起，
> 空格或连字符不能充当键中的分隔符：[`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261)。）

<a id="application-order"></a>

### 应用顺序

1. 材质被重置为[默认值](#defaults--what-an-omitted-setting-resets-to)。
2. 校验**整个**块——解析每个特殊值，并把每个通用值写入一个一次性的临时 `UMaterial`。任何一个坏值都会在真正的材质被触及之前停止构建，并且这次重建会被回滚。
3. 先 `BlendMode`，再 `ShadingModel`，再 `MaterialDomain`；然后 `bUsedWithVolumetricCloud` 跟随域。
4. 按设置的迭代顺序写入通用键。

> [!NOTE]
> **两个通用键之间不保证任何顺序**，因此不要依赖某个键先于另一个键被写入。特殊键总是先于任何通用键写入，因此像 `TranslucencyLightingMode` 这样与 `BlendMode` 相邻的通用属性会看到最终的混合模式。

`Backend` 在任何材质存在之前就被消费——无法识别的 `Backend` 值是绑定错误，它会在应用 `Settings` 其余部分之前停止编译，因此该文件不会报告本次构建的任何设置诊断。

<a id="interaction-with-basefrontmaterial"></a>

### 与 `Base.FrontMaterial` 的交互

*（自 2.0.0 起）* 对 `Base.FrontMaterial` 绑定的两项 1.x 检查——一个不是 `Substrate` 或 `Strata` 的显式 `ShadingModel`，以及在同一个 `Shader` 上同时有 `Base.FrontMaterial` 和 `Base.MaterialAttributes`——在 2.0 中没有诊断，并且编译器不会强制把着色模型设为 Substrate：显式的 `ShadingModel` 会按所写的方式应用。见[输出绑定](../language/output-bindings.md)、[Substrate 内置项](../builtins/substrate.md)，以及关于 `Substrate = Bridge` 的 [Substrate 语法糖](../language-v2/substrate.md)。

<a id="the-reflection-resolver"></a>

## 反射解析器

每个非特殊的键都会由 Unreal 反射针对生成的材质进行解析。

| # | 步骤 | 详情 |
| --: | :-- | :-- |
| 1 | 在方括号深度为 0 处按 `.` 将键拆分为多个段 | 自 2.0.0 起，1.x 的键就是一个段（见[嵌套和索引路径](#nested-and-indexed-paths)） |
| 2 | 解析每个段可选的末尾 `[<integer>]` | 同上 |
| 3 | 通过[别名表](#alias-table)映射段名 | 在字段扫描之前**按段**应用 |
| 4 | 在当前结构体上扫描 `TFieldIterator<FProperty>`，**包括超类** | 因此 `UMaterial`、`UMaterialInterface` 和 `UObject` 属性都可达 |
| 5 | 向下进入 | 非末尾段必须是 `FStructProperty`；遍历在该结构体内部继续 |
| 6 | 写入值 | 根据解析出的属性的 C++ 类型进行解析——见[取值语法](#value-grammar) |

没有该名称的属性会 [`DSH7118`](../diagnostics/DSH7xxx.md#dsh7118)。

<a id="property-name-matching"></a>

### 属性名匹配

当以下三者中的任意一个在删除空格、`_` 和 `-` 并转为小写之后等于该段时，该段就匹配一个属性：

| 规则 | 示例 |
| :-- | :-- |
| 原始的 `FProperty` 名称 | `TwoSided` ← `TwoSided`、`two_sided`、`TWOSIDED` |
| 属性名**去掉开头的 `b`**，当该名称是 `b` 后跟一个大写字母时 | `bFullyRough` ← `FullyRough`；`bIsSky` ← `IsSky`；`bIsThinSurface` ← `IsThinSurface` |
| 属性的 `DisplayName` 元数据 | 引擎为该属性声明的任何内容 |

去掉 `b` 的规则是单向且宽松的：完整名称仍然匹配，因此 `bFullyRough = true;` 和 `FullyRough = true;` 都会解析到同一个属性。名称以小写 `b` 后跟非大写字符开头的属性（例如 `bias`）不会被去掉 b 前缀。

<a id="nested-and-indexed-paths"></a>

### 嵌套和索引路径

解析器本身仍会遍历 `A.B`（结构体属性的成员）、`A[N]`（定长 C 数组的元素 `N`）以及两者的任意组合。*（自 2.0.0 起）* 1.x 的 `Settings` 键无法到达它们：旧版前端只读取一个名称，并在 `.` 或 `[` 处报出 `DSH3261`。解析器仍有两项检查可达：

| 写法 | 代码 |
| :-- | :-- |
| 命名定长数组（`ArrayDim` 大于 1）却不写下标，例如 `PhysicalMaterialMap = …` | [`DSH7121`](../diagnostics/DSH7xxx.md#dsh7121) |
| 没有属性使用的名称 | `DSH7118` |

`TArray`、`TMap` 和 `TSet` 属性根本不能按下标访问 —— 它们落入[取值语法](#value-grammar)中的 `ImportText` 兜底分支。

<a id="alias-table"></a>

### 别名表

字段扫描之前会按每个路径段应用十个固定的键别名。别名键按去分隔符、小写后的形式比较，所以 `Lighting_Mode` 和 `LIGHTINGMODE` 都命中第一行。

| 别名 | 解析为 |
| :-- | :-- |
| `LightingMode` | `TranslucencyLightingMode` |
| `TranslucentLightingMode` | `TranslucencyLightingMode` |
| `RefractionMode` | `RefractionMethod` |
| `PhysicalMaterial` | `PhysMaterial` |
| `PhysicalMaterialMask` | `PhysMaterialMask` |
| `Lightmass` | `LightmassSettings` |
| `MobileSeparateTranslucency` | `bEnableMobileSeparateTranslucency` |
| `AlwaysEvaluateWorldPositionOffset` | `bAlwaysEvaluateWorldPositionOffset` |
| `ResponsiveAA` | `bEnableResponsiveAA` |
| `ThinSurface` | `bIsThinSurface` |

<a id="value-grammar"></a>

## 取值语法

解析出的属性的 C++ 类型决定值文本怎么解析。值此时已经被解析器去过引号，所以 `true` 和 `"true"` 是同一个输入。

| 属性类型 | 接受的字面量 | 失败时的码 |
| :-- | :-- | :-- |
| `bool` | `true` / `false`，不区分大小写，trim 后 | [`DSH7132`](../diagnostics/DSH7xxx.md#dsh7132) |
| `int32` | 有符号整数字面量 | [`DSH7133`](../diagnostics/DSH7xxx.md#dsh7133) |
| `uint32` | `[0, 4294967295]` 内的整数 | [`DSH7134`](../diagnostics/DSH7xxx.md#dsh7134) |
| `float` | 数值字面量 —— 这里 `true` / `false` 不算数字 | [`DSH7135`](../diagnostics/DSH7xxx.md#dsh7135) |
| `double` | 同 `float` | [`DSH7136`](../diagnostics/DSH7xxx.md#dsh7136) |
| `FString` | 任意文本，trim 后写入 —— **从不失败** | —— |
| `FName` | 任意文本，trim 后写入 —— **从不失败** | —— |
| 对象引用 | `Path( … )`、`/Game/Textures/T_Noise` 这样的绝对对象路径，或 `Class'…'` 引用 | [`DSH7137`](../diagnostics/DSH7xxx.md#dsh7137)（不是引用）、[`DSH7138`](../diagnostics/DSH7xxx.md#dsh7138)（加载不了）、[`DSH7139`](../diagnostics/DSH7xxx.md#dsh7139)（类不对），或解析器自己的码 —— 见 [Path](../parameters/path.md#diagnostics) |
| `enum class`（`FEnumProperty`） | [枚举字面量](#enum-literals) | [`DSH7140`](../diagnostics/DSH7xxx.md#dsh7140) |
| `uint8` 枚举（带枚举的 `FByteProperty`） | [枚举字面量](#enum-literals) | [`DSH7141`](../diagnostics/DSH7xxx.md#dsh7141) |
| 普通 `uint8` | `[0, 255]` 内的整数 | [`DSH7142`](../diagnostics/DSH7xxx.md#dsh7142) |
| 其他 | Unreal 的结构体字面量文本，如 `(R=1.0,G=0.0,B=0.0,A=1.0)` | [`DSH7143`](../diagnostics/DSH7xxx.md#dsh7143) |

数字按 Unreal 的 `LexTryParseString` 读，遇到第一个用不了的字符就停：`OpacityMaskClipValue = "0.5abc";` 是 `0.5`。

类型的消息在校验块时被包成 [`DSH7125`](../diagnostics/DSH7xxx.md#dsh7125)（真实材质随后拒绝时是 [`DSH7126`](../diagnostics/DSH7xxx.md#dsh7126)），再以 [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215) 到达你，它指出材质并引用这个码。

> [!NOTE]
> 如果对象类型属性的类派生自 `UTexture`，**且**属性名恰好是 `Texture` 或 `TextureObject`，那么资产加载失败时会写入 `nullptr` 而不是报错。
> 这是材质表达式的惯例；这里同样适用。

当对象类型的值是引用 —— 以 `Path(` 或 `/` 开头，或以 `'` 结尾 —— 且解析失败时，`DSH7125` 句号之后那句就是解析器的消息。

<a id="enum-literals"></a>

### 枚举字面量

枚举类型的值在 trim、小写并删除所有空格、`_`、`-`、`:`、`.` 和 `/` 之后匹配。带 `Hidden` 元数据的值会被跳过。候选可以是以下任意一种：

| 形式 | `ETranslucencyLightingMode::TLM_Surface` 的示例 |
| :-- | :-- |
| 枚举短名 | `TLM_Surface` |
| 全限定名 | `ETranslucencyLightingMode::TLM_Surface` |
| display name | `Surface` |
| 短名去掉第一个 `_` 及之前部分 | `Surface` |

这套匹配与 [Material enums](material-enums.md) 里描述的 `ShadingModel` / `BlendMode` / `Domain` 别名表是**两回事**；那三个键永远走不到这段代码。

<a id="path---values"></a>

### `Path( … )` 取值

| 形式 | 含义 |
| :-- | :-- |
| `Path("/Game/Foo/Bar")` | 绝对对象路径，单实参 |
| `Path(Game, "Foo/Bar")` | `/Game/Foo/Bar` |
| `Path(Engine, "Foo/Bar")` | `/Engine/Foo/Bar` |
| `Path(Plugin.PluginName, "Foo/Bar")` | 指定插件的内容根 |
| `/Game/Foo/Bar` | 裸的绝对路径，不带 `Path( … )` 包裹 |

反斜杠会被规范化为 `/`，周围的引号会被去除。完整的根目录清单及其错误码见 [Path](../parameters/path.md)。

<a id="validation"></a>

## 校验

在写入真正的材质之前，每个通用键值对都会先应用到一份在 transient 包中创建的临时探针 `UMaterial` 上。探针写入和真实写入用的是同一段代码，所以校验通过的值一定能应用成功。这一阶段独有的失败只有 [`DSH7124`](../diagnostics/DSH7xxx.md#dsh7124)：探针创建不出来。

<a id="defaults--what-an-omitted-setting-resets-to"></a>

## 默认值 —— 不写某个设置时会重置成什么

生成的材质会在应用 `Settings` 之前立即被重置。因此没写的键**不是**“沿用上次生成的值”，而是取下表的值。

| 属性 | 重置值 |
| :-- | :-- |
| `BlendMode` | `Opaque` |
| `MaterialDomain` | `Surface` |
| shading model | `DefaultLit` |
| `TwoSided` | `false` |
| `bUseMaterialAttributes` | `false` |
| `OpacityMaskClipValue` | `0.3333` |
| `Wireframe` | `false` |
| `DitheredLODTransition` | `false` |
| `DitherOpacityMask` | `false` |
| `bAllowNegativeEmissiveColor` | `false` |
| `bCastDynamicShadowAsMasked` | `false` |
| `bCastRayTracedShadows` | `true` |
| `bEnableResponsiveAA` | `false` |
| `bScreenSpaceReflections` | `false` |
| `bContactShadows` | `false` |
| `bDisableDepthTest` | `false` |
| `bOutputTranslucentVelocity` | `false` |
| `bWriteOnlyAlpha` | `false` |
| `BlendableOutputAlpha` | `false` |
| `TranslucencyLightingMode` | `TLM_VolumetricNonDirectional` |
| `bTangentSpaceNormal` | `true` |
| `bAlwaysEvaluateWorldPositionOffset` | `false` |
| `bFullyRough` | `false` |
| `bIsSky` | `false` |
| `bIsThinSurface` | `false` |
| `MaterialDecalResponse` | `MDR_ColorNormalRoughness` |
| `bHasPixelAnimation` | `false` *（自 UE 5.4 起）* |
| `NumCustomizedUVs` | `0` |
| `bUsedWithVolumetricCloud` | 当 `Domain="Volume"` 时为 `true`，否则为 `false` |

其余所有 `UMaterial` 属性都保持新构造材质所具有的值。

`bUsedWithVolumetricCloud` 是上表中唯一一个不是常量的条目：它由 domain 决定。一个
`Volume` domain 的材质是体积材质，而体积云渲染器会拒绝一个
未为此标记的材质 —— 所以你永远不需要手写这个标志。显式写出它可以
覆盖 domain 的选择；一个只喂给体积雾的 `Volume` 材质可以用 `bUsedWithVolumetricCloud = "false";` 把
标志重新关掉，并省下云的着色器排列组合。

<a id="the-round-trip-set"></a>

## 往返集合

[反编译器](../tools/decompiler.md)会无条件写出 `Domain`、`ShadingModel` 和 `BlendMode`，
另外还会在与 `UMaterial` 类默认值不同时写出以下各项。这
是保证能挺过一次 `UMaterial` → `.dsm` → `UMaterial` 往返的集合；它是
`Settings` 所接受内容的子集，而不是对其的限制。

**布尔值（40 个，按写出顺序）**

```text
TwoSided                    Wireframe                     DitheredLODTransition
DitherOpacityMask           bAllowNegativeEmissiveColor   bCastDynamicShadowAsMasked
bEnableResponsiveAA         bScreenSpaceReflections       bContactShadows
bDisableDepthTest           bOutputTranslucentVelocity    bTangentSpaceNormal
bFullyRough                 bIsSky                        bIsThinSurface
bHasPixelAnimation          bUsedWithSkeletalMesh         bUsedWithMorphTargets
bUsedWithClothing           bUsedWithNanite               bUsedWithEditorCompositing
bUsedWithParticleSprites    bUsedWithBeamTrails           bUsedWithMeshParticles
bUsedWithNiagaraSprites     bUsedWithNiagaraRibbons       bUsedWithNiagaraMeshParticles
bUsedWithGeometryCache      bUsedWithStaticLighting       bUsedWithSplineMeshes
bUsedWithInstancedStaticMeshes                            bUsedWithGeometryCollections
bUsedWithHairStrands        bUsedWithWater                bUsedWithVirtualHeightfieldMesh
bCastRayTracedShadows       bWriteOnlyAlpha               BlendableOutputAlpha
bAlwaysEvaluateWorldPositionOffset                        bUsedWithVolumetricCloud
```

`bHasPixelAnimation` 仅在 UE 5.4 及更新版本上写出。

`bUsedWithVolumetricCloud` 是唯一一个不与 `UMaterial` 类默认值比较的布尔值：它
会与 domain 会赋予它的值进行比较（见上表）。在一个 `Volume` domain
材质上，无需写出该行的值是 `true`，因此被写出的是 `false` —— 这
正是让一个仅用于雾的 `Volume` 材质不会带着开启的标志回来的原因。

**枚举（1 个）** —— `MaterialDecalResponse`。

可达但反编译器从不写出、从而在往返中丢失的：
`OpacityMaskClipValue`、`NumCustomizedUVs`、`TranslucencyLightingMode`、`RefractionMethod`、
`RefractionDepthBias`、`TranslucencyPass`、`ShadingRate`、`FloatPrecisionMode`、`BlendableLocation`、
`BlendablePriority`、`bIsBlendable`、`UserSceneTexture`、`StencilCompare`、`StencilRefValue`、
`bEnableStencilTest`、`MaxWorldPositionOffsetDisplacement`、`PhysMaterial`、`PhysMaterialMask`，以及
解析器能按名称触达的其他所有引擎属性。确切的可用性取决于引擎版本
—— 解析器是对引擎自身属性集的纯反射。

<a id="notes"></a>

## 备注

- 反射的面是**引擎的**，不是插件的。自定义或修改过的引擎构建
  会通过同一个解析器暴露它自己的属性和自己的枚举值，所以本手册的
  表格只描述原版 UE 5.3 – 5.8 的面。
- 在 [ThinCustom 后端](backend.md)下，每个设置都落在**隐藏的基础 `UMaterial`** 上，
  而不是写出的 `UDreamShaderMaterialInstance` 上。从实例上读取混合模式看到的是
  继承来的值。
- [`ShaderFunction`](function.md) 上的 `Settings` 块**不**共享这种行为。那里
  只读取四个键，而本页上的每个键在那里都是一个
  [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) 警告。
- 跨多个 `Settings` 段的重复键会合并，后者胜出，并给出
  一个 [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) 警告。见
  [设置](index.md#how-a-statement-is-parsed)。
- 一个设置失败会对该材质只报告一次，即 `DSH8215`；消息会指出材质
  并引用反射写入器的代码和消息。

<a id="diagnostics"></a>

## 诊断

因为键在到达发射器时会被转为小写，这些消息中引用的键是
**小写**拼写，而不是源码写出的。

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH7127`](../diagnostics/DSH7xxx.md#dsh7127) | `BlendMode` / `RenderType` 值没有匹配到任何别名 |
| [`DSH7129`](../diagnostics/DSH7xxx.md#dsh7129) | `ShadingModel` 值没有匹配到任何别名 |
| [`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128) | 该值在 trim 并折叠大小写后在 UE 5.3 上为 `Substrate` 或 `Strata` |
| [`DSH7130`](../diagnostics/DSH7xxx.md#dsh7130) | `MaterialDomain` / `Domain` 值没有匹配到任何别名 |
| `DSH7118` | 没有属性匹配 —— 常见的“未知键”错误 |
| `DSH7121` | 命名了一个定长数组属性却没有下标 |
| `DSH7124` | 探针材质无法分配 |
| `DSH7125` / `DSH7126` | 字面量写入在探针 / 真实材质上失败；其后那句话是来自[取值语法](#value-grammar)的类型特定代码的消息 |
| `DSH8215` | 上述每个代码都会在其中到达你的构建期诊断 |
| [`DSH7200`](../diagnostics/DSH7xxx.md#dsh7200) | 一个键在 `#pragma material` 行中被设置了两次（1.x 的 `Settings` 块会报告 `DSH3262`） |
| `DSH7232` | `Substrate` 不是 `Legacy`、`Bridge` 或 `Native` |
| `DSH3261` | 键不是一个名称 —— 包括嵌套路径或下标 |

`Backend` 值在源码被绑定时检查 —— 见 [后端](backend.md#diagnostics)。
`DSH7112`–`DSH7117`、`DSH7119`、`DSH7120`、`DSH7122` 和 `DSH7123` 涉及嵌套和带下标的路径以及
解析器自身的不变量；1.x 键不会触及它们。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_ShaderSettings", Root="Game")
{
    Properties {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; Slider(0, 1)];
    }

    Settings {
        // Special keys.
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Masked";

        // Reflected booleans; the b-prefix is optional.
        TwoSided   = true;
        FullyRough = true;
        bIsSky     = false;

        // Reflected scalar and enum.
        OpacityMaskClipValue = 0.25;
        MaterialDecalResponse = "ColorNormalRoughness";

        // Alias -> TranslucencyLightingMode, matched by display name.
        LightingMode = "Surface";

        // Object reference.
        PhysicalMaterial = Path(Engine, "EngineMaterials/DefaultPhysicalMaterial");
    }

    Outputs {
        vec3  Color;
        float Rough;
        float Mask;
        Base.BaseColor      = Color;
        Base.Roughness      = Rough;
        Base.OpacityMask    = Mask;
    }

    Graph {
        Color = BaseColor.rgb;
        Rough = Roughness;
        Mask  = BaseColor.a;
    }
}
```

生成的材质状态：

```text
BlendMode                = BLEND_Masked
MaterialDomain           = MD_Surface
ShadingModel             = MSM_DefaultLit
TwoSided                 = true
bFullyRough              = true
bIsSky                   = false
OpacityMaskClipValue     = 0.25
MaterialDecalResponse    = MDR_ColorNormalRoughness
TranslucencyLightingMode = TLM_Surface
PhysMaterial             = /Engine/EngineMaterials/DefaultPhysicalMaterial
```

<a id="see-also"></a>

## 另见

- [设置](index.md) —— 每种块类型共享的块语法和键规范化
- [材质枚举](material-enums.md) —— 每个可接受的 `ShadingModel`、`BlendMode` 和 `Domain` 值
- [后端](backend.md) —— 物化键
- [函数设置](function.md) —— 为什么这一切都不适用于 `ShaderFunction` 内部
- [项目设置](project.md) —— 扩展枚举拼写的映射映射
- [着色器](../language/shader.md) —— 外层块
- [输出绑定](../language/output-bindings.md) —— `Base.FrontMaterial` 和 `Base.MaterialAttributes`
- [路径](../parameters/path.md) —— 对象类型设置使用的 `Path( … )` 语法
- [元数据](../parameters/metadata.md) —— 参数上类似的反射属性块
- [反编译器](../tools/decompiler.md) —— 哪些设置能挺过往返
- [重新生成](../generation/regeneration.md) —— 一次重建会重置什么
- [诊断索引](../diagnostics/index.md) —— 每个代码
