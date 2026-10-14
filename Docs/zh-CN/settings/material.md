---
title: 材质 Settings
description: Settings 块 —— 语句怎么解析、Shader 的特殊键、能触达任意 UMaterial 属性的反射解析器、重新生成时会被重置成什么，以及材质函数的四个键。
---

`Settings` 把 `<key> = <value>;` 写到外层块生成的资产上。在 `Shader` 块里这个资产是 `UMaterial`，本页主要描述的就是
你可以往里写什么；材质函数块上的 `Settings` 完全是另一回事，见[材质函数的 Settings](#材质函数的-settings)。

**提示：**

**`Shader` 的 `Settings` 不是一份固定的受支持键列表。** 只有少数键是手写处理的，其余任何键都通过 Unreal 反射解析到
生成的 `UMaterial` 上。任何表格都无法穷尽这个面，因为那是引擎的属性集合，不是插件的。

```text
Settings key
   ├── Backend, Substrate
   │      └── 绑定源码时由编译器读取；永远到不了材质
   ├── blendmode rendertype shadingmodel materialdomain domain
   │      └── 手写处理：值经项目的别名表解析
   └── anything else
          └── 别名表  →  UMaterial / UMaterialInterface / UObject 属性查找
                           （名字、去掉 b 前缀的名字，或 DisplayName）
                       →  按属性的 C++ 类型解析值
```

`Shader` 的 `Settings` 就是 2.0 的 `#pragma material(…)` —— 见 [DreamShaderLang 2.0](../language-v2/index.md)。

<a id="块的形状"></a>

## 块的形状

```c
Settings [=] { <setting-statement> … }

<setting-statement> := <name> = <value> ;
<name>              := <identifier>
<value>             := <到下一个 ; 为止的 token> | "<text>"
```

段名与 `{ … }` 之间的 `=` 是可选糖 （自 1.5.0 起）。最后一条语句后的 `;` 可省略。

（自 2.0.0 起） legacy 前端用和文件其余部分相同的 token 读取 `Settings` 段：

| # | 步骤 | 后果 |
| --: | :-- | :-- |
| 1 | `//` 和 `/* … */` 注释是空白 | 注释可以出现在任意两个 token 之间，包括语句中间 |
| 2 | 一条语句是名字、`=` 和值；值一直到 `( )`、`[ ]`、`{ }` 深度为 0 的下一个 `;`，或到结束本段的 `}` | `Color = (R=1,G=0,B=0);` 是一条语句；空语句被跳过，所以最后的 `;` 可省略 |
| 3 | 名字是**一个**标识符 | `Foo.Bar = 1;` 和 `Foo[2] = 1;` 报 `DSH3261`，没有 `=` 的语句、`=` 后没有值也是 |
| 4 | 值恰好是一个字符串字面量时取其内容（转义由词法分析器处理）；其他值就是写下的文本 | **所有 setting 的引号都是可选的** |
| 5 | 同一个名字写两次（忽略大小写比较），保留后写的值 | 警告 `DSH3262` |

所以 `TwoSided = true;` 和 `TwoSided = "true";` 完全相同，`Domain = Surface;` 与 `Domain = "Surface";` 行为一致。一个块里
可以有**多个** `Settings` 段；它们合并成一个列表，同一个键写两次保留最后一个值，并给出 `DSH3262` （自 2.0.0 起）。

**警告：**

（自 2.0.0 起） 名字只能是**一个标识符**。1.x 还能把嵌套路径（`Lightmass.DiffuseBoost`）和定长数组下标
（`PhysicalMaterialMap[2]`）当作 `Shader` 的键；legacy 前端在 `.` 或 `[` 处停下并报 `DSH3261`。

<a id="特殊键"></a>

## 特殊键

下面这些键永远不会走到反射解析器。发射器跳过这六个名字 —— 在 trim、小写**并且**删除空格、`_`、`-` 之后比较：

```text
blendmode   rendertype   shadingmodel   materialdomain   domain   backend
```

| 规范键 | 同义键 | 取值 | 不写时的值 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| `BlendMode` | `RenderType` | [混合模式拼写](material-enums.md#blendmode)之一 | `Opaque` | 设置 `UMaterial::BlendMode` |
| `ShadingModel` | —— | [ShadingModel 拼写](material-enums.md#shadingmodel)之一 | `DefaultLit` | 调用 `SetShadingModel` |
| `MaterialDomain` | `Domain` | [Domain 拼写](material-enums.md#domain)之一 | `Surface` | 设置 `UMaterial::MaterialDomain` |
| `Backend` | —— | `Graph` 或 `ThinCustom`；`Instance` 是 `ThinCustom` 的旧写法 | 项目的 *Default Compiler Backend* | 选择物化策略 —— 见 [Backend](backend.md) |

另有一个键由编译器读取、永远到不了材质 （自 2.0.0 起）：

| 键 | 取值 | 不写时的值 | 效果 |
| :-- | :-- | :-- | :-- |
| `Substrate` | `Legacy`、`Bridge`、`Native` | `Legacy` | 按旧属性写的材质在开了 Substrate 的工程里怎么读 —— `Bridge` 把 Surface 材质的着色属性折叠进 `FrontMaterial` 上的一个 `Substrate.ShadingModels` 节点。见[一份源码，两种工程](../language-v2/substrate.md#一份源码两种工程)。未知模式报 `DSH7232` |

`Backend` 和 `Substrate` 在绑定源码时取出（忽略大小写匹配）；其余在构建材质时应用。规范键与同义键同时出现时，**规范键**
胜出：`BlendMode` 压过 `RenderType`，`MaterialDomain` 压过 `Domain`，没有诊断。这几个键的拼写只做 trim 加小写匹配，所以
`blendmode`、`BlendMode`、`BLENDMODE` 都行。

**警告：**

**只在下划线上与特殊键不同的拼写会被静默丢弃。** `Blend_Mode = "Translucent";` 并不会设置混合模式：直接探测 `BlendMode`
匹配不到存储的键 `blend_mode`，而反射循环又因为它去分隔符后的形式在特殊键名单上而跳过它。没有错误，没有警告，也没有效果。
`Render_Type`、`Shading_Model`、`Material_Domain`、`Back_end` 同理。这些名字请不带分隔符地写。（（自 2.0.0 起） 起
键里根本不能有空格或连字符：`DSH3261`。）

<a id="应用顺序"></a>

### 应用顺序

1. 材质被重置为[默认值](#不写某个设置时会重置成什么)。
2. 先校验**整个**块 —— 解析每个特殊值，并把每个通用值写到一份临时 `UMaterial` 上。只要有一个值不合法，就在真正的材质被
   改动之前中止，这次重建被回滚。
3. 先 `BlendMode`，再 `ShadingModel`，再 `MaterialDomain`；然后 `bUsedWithVolumetricCloud` 跟随 domain。
4. 通用键，按设置的迭代顺序。

**提示：**

**两个通用键之间没有任何顺序保证。** 特殊键总是先写，所以像 `TranslucencyLightingMode` 这种与混合模式相关的属性看到的是
最终的混合模式。

`Backend` 在任何材质对象存在之前就被消费 —— 无法识别的 `Backend` 值是绑定错误，编译在应用 `Settings` 其余部分之前就停下，
因此该文件不会再报构建期的设置诊断。

<a id="与-basefrontmaterial-的关系"></a>

### 与 `Base.FrontMaterial` 的关系

（自 2.0.0 起） 1.x 对 `Base.FrontMaterial` 绑定的两条检查 —— 显式写了不是 `Substrate` 或 `Strata` 的 `ShadingModel`，
以及同一个 `Shader` 上同时有 `Base.FrontMaterial` 和 `Base.MaterialAttributes` —— 在 2.0 里没有诊断，编译器也不再把
着色模型强制设为 Substrate：显式写的 `ShadingModel` 照写应用。见[输出绑定](../generation/outputs.md#basefrontmaterial)、
[Substrate 节点](../builtins/substrate.md)，以及 `Substrate = Bridge` 的 [Substrate 语法糖](../language-v2/substrate.md)。

<a id="反射解析器"></a>

## 反射解析器

所有非特殊键都通过 Unreal 反射解析到生成的材质上：键名先过[别名表](#别名表)，然后在 `UMaterial` 上扫描
`TFieldIterator<FProperty>`，**包含父类** —— 所以 `UMaterial`、`UMaterialInterface` 和 `UObject` 的属性都可达 —— 最后按
解析出的属性的 C++ 类型写入值。没有属性匹配报 `DSH7118`。

解析器本身仍能走 `A.B`（结构体属性的成员）和 `A[N]`（定长 C 数组的元素），但 1.x 的 `Settings` 键已经到不了它们（见上）。
仍然够得着的检查是：命名定长数组属性（`ArrayDim` 大于 1）却不写下标，比如 `PhysicalMaterialMap = …`，报 `DSH7121`。
`TArray`、`TMap`、`TSet` 属性不能按下标访问 —— 它们落到[取值语法](#取值语法)里的 import 文本兜底分支。

<a id="属性名匹配"></a>

### 属性名匹配

以下三者中任意一个在删除空格、`_`、`-` 并小写后与键相等，就算匹配：

| 规则 | 示例 |
| :-- | :-- |
| 原始 `FProperty` 名 | `TwoSided` ← `TwoSided`、`two_sided`、`TWOSIDED` |
| **去掉开头 `b` 后的**属性名，前提是名字是 `b` 加一个大写字母 | `bFullyRough` ← `FullyRough`；`bIsSky` ← `IsSky`；`bIsThinSurface` ← `IsThinSurface` |
| 属性的 `DisplayName` 元数据 | 引擎为该属性声明的任何值 |

去 `b` 规则是单向且宽松的：完整名字同样能匹配，所以 `bFullyRough = true;` 和 `FullyRough = true;` 解析到同一个属性。

<a id="别名表"></a>

### 别名表

字段扫描之前会应用十个固定的键别名，按去分隔符、小写后的形式比较，所以 `Lighting_Mode` 和 `LIGHTINGMODE` 都命中第一行。

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

<a id="取值语法"></a>

## 取值语法

解析出的属性的 C++ 类型决定值文本怎么解析。值此时已经去过引号，所以 `true` 和 `"true"` 是同一个输入。

| 属性类型 | 接受的字面量 | 失败时的码 |
| :-- | :-- | :-- |
| `bool` | `true` / `false`，不区分大小写 | `DSH7132` |
| `int32` | 有符号整数字面量 | `DSH7133` |
| `uint32` | `[0, 4294967295]` 内的整数 | `DSH7134` |
| `float` | 数值字面量 —— 这里 `true` / `false` 不算数字 | `DSH7135` |
| `double` | 同 `float` | `DSH7136` |
| `FString` | 任意文本，trim 后写入 —— **从不失败** | —— |
| `FName` | 任意文本，trim 后写入 —— **从不失败** | —— |
| 对象引用 | `Path( … )`、`/Game/Textures/T_Noise` 这样的绝对对象路径，或 `Class'…'` 引用 | `DSH7137`（不是引用）、`DSH7138`（加载不了）、`DSH7139`（类不对），或解析器自己的码 —— 见 [Path 资产引用](../parameters/path.md#诊断) |
| `enum class`（`FEnumProperty`） | [枚举字面量](#枚举字面量) | `DSH7140` |
| 以枚举为底的 `uint8` | [枚举字面量](#枚举字面量) | `DSH7141` |
| 普通 `uint8` | `[0, 255]` 内的整数 | `DSH7142` |
| 其他 | Unreal 的结构体字面量文本，如 `(R=1.0,G=0.0,B=0.0,A=1.0)` | `DSH7143` |

数字按 Unreal 的 `LexTryParseString` 读，遇到第一个用不了的字符就停：`OpacityMaskClipValue = "0.5abc";` 是 `0.5`。类型的
消息在校验块时被包成 `DSH7125`（真实材质随后拒绝时是 `DSH7126`），再以 `DSH8215` 到达你，`DSH8215` 指出材质并引用这个码。

**提示：**

如果对象类型属性的类派生自 `UTexture`，**且**属性名恰好是 `Texture` 或 `TextureObject`，那么资产加载失败时会写入 `nullptr`
而不是报错。这是材质表达式的惯例，这里同样适用。

<a id="枚举字面量"></a>

### 枚举字面量

枚举类型的值在 trim、小写并删除所有空格、`_`、`-`、`:`、`.`、`/` 之后匹配。带 `Hidden` 元数据的值会被跳过。候选可以是
枚举短名（`TLM_Surface`）、全限定名（`ETranslucencyLightingMode::TLM_Surface`）、display name（`Surface`），或短名去掉第一个
`_` 及之前部分（`Surface`）。

这套匹配与[枚举取值](material-enums.md)里 `ShadingModel` / `BlendMode` / `Domain` 的别名表是**两回事** —— 那三个键走不到这里。

<a id="path---取值"></a>

### `Path( … )` 取值

| 形式 | 含义 |
| :-- | :-- |
| `Path("/Game/Foo/Bar")` | 绝对对象路径，单实参 |
| `Path(Game, "Foo/Bar")` | `/Game/Foo/Bar` |
| `Path(Engine, "Foo/Bar")` | `/Engine/Foo/Bar` |
| `Path(Plugin.PluginName, "Foo/Bar")` | 指定插件的内容根 |
| `/Game/Foo/Bar` | 裸的绝对路径，不带 `Path( … )` |

完整的 root 目录及其错误码见 [Path 资产引用](../parameters/path.md)。对象类型的值是引用（以 `Path(` 或 `/` 开头，或以 `'`
结尾）却解析失败时，`DSH7125` 句号之后那句话就是解析器的消息。

<a id="校验"></a>

## 校验

在写入真正的材质之前，每个通用键值对都会先应用到一份临时探针 `UMaterial` 上。探针写入和真实写入用的是同一段代码，所以
校验通过的值一定能应用成功。这一阶段独有的失败只有 `DSH7124`：探针创建不出来。

<a id="不写某个设置时会重置成什么"></a>

## 不写某个设置时会重置成什么

生成的材质会在应用 `Settings` 之前被重置。因此没写的键**不是**“沿用上次生成的值”，而是取下表的值。

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
| `bHasPixelAnimation` （自 UE 5.4 起） | `false` |
| `NumCustomizedUVs` | `0` |
| `bUsedWithVolumetricCloud` （自 1.8.0 起） | `Domain="Volume"` 时为 `true`，否则 `false` |

`bUsedWithVolumetricCloud` 是上表里唯一**不是常量**的一项：由 domain 决定。`Volume` domain 的材质就是体积材质，而体积云
渲染器会拒绝一个没打这个标志的材质 —— 所以你永远不需要手写它。显式写出来可以覆盖 domain 的选择：一个只喂体积雾的
`Volume` 材质可以用 `bUsedWithVolumetricCloud = "false";` 把标志关掉，省下云的 shader 排列。

其余 `UMaterial` 属性保持新构造材质的样子。凡是必须在重新生成后仍然存在的设置，都要写在 `Settings` 里，而不是在生成的
资产上手改。

<a id="哪些设置能挺过反编译往返"></a>

## 哪些设置能挺过反编译往返

[反编译器](../tools/decompiler.md)无条件写出 `Domain`、`ShadingModel` 和 `BlendMode`，另外只在与 `UMaterial` 类默认值不同
时写出下面这些。这是往返能保证保留的集合；它是 `Settings` 可接受范围的子集，而不是上限。

**布尔（40 个，按写出顺序）**

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

`bHasPixelAnimation` 只在 （自 UE 5.4 起） 上写出。`bUsedWithVolumetricCloud` 是这里唯一**不**与 `UMaterial` 类默认值
比较的布尔：它比较的是 domain 会给它的值，所以 `Domain="Volume"` 材质上真正被写出的是 `false`。

**枚举（1 个）** —— `MaterialDecalResponse`。

可达但从不写出、因而在往返中丢失的：`OpacityMaskClipValue`、`NumCustomizedUVs`、`TranslucencyLightingMode`、
`RefractionMethod`、`RefractionDepthBias`、`TranslucencyPass`、`ShadingRate`、`FloatPrecisionMode`、`BlendableLocation`、
`BlendablePriority`、`bIsBlendable`、`UserSceneTexture`、`StencilCompare`、`StencilRefValue`、`bEnableStencilTest`、
`MaxWorldPositionOffsetDisplacement`、`PhysMaterial`、`PhysMaterialMask`，以及解析器能触达的其他所有引擎属性。

<a id="材质函数的-settings"></a>

## 材质函数的 Settings

`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend`（以及弃用的 `MaterialLayer` / `MaterialLayerBlend`）上的 `Settings`
与上面这一切**毫无关系**。那里只读四个键，它们向材质函数库描述生成的 `UMaterialFunction`；在 2.0 里它们是导出函数的
`/// @desc` 和 `/// @library` 指令。

| 键 | 设置 | 取值 | 不写时的值 |
| :-- | :-- | :-- | :-- |
| `Description` | `UMaterialFunction::Description` | 自由文本 | 清空为空字符串 |
| `UserExposedCaption` | **什么都不设置** （自 2.0.0 起）：警告 `DSH3264` 说明它不被应用，资产的标题保持原样 | 自由文本 | —— |
| `ExposeToLibrary` | `UMaterialFunction::bExposeToLibrary`，与 `LibraryCategories` 一起 | `true` / `false`，不区分大小写；其他值是警告 `DSH3265`，该设置被忽略 | `false` |
| `LibraryCategories` | `UMaterialFunction::LibraryCategoriesText`，与 `ExposeToLibrary` 一起 | 逗号分隔列表，每项 trim，空项丢弃 | 清空分类列表 |

```c
Settings = {
    Description        = "Multiplies a colour by a tint.";
    ExposeToLibrary    = true;
    LibraryCategories  = "DreamShader, Color";
}
```

（自 2.0.0 起） `ExposeToLibrary` 和 `LibraryCategories` 合起来是**一个**设置 —— 2.0 的 `/// @library <categories>`：
只有 `ExposeToLibrary` 为 `true` **并且** `LibraryCategories` 非空时，函数才以这些分类进入函数库。单独的
`ExposeToLibrary = true;` 不会把函数放进库里，没有 `ExposeToLibrary = true;` 的 `LibraryCategories` 被丢弃。
`LibraryCategories` 解析前先清空，所以总是替换资产的分类而不是追加：`LibraryCategories = "A,,  B ,";` 恰好得到 `A` 和 `B`。

这里的键忽略大小写匹配，但下划线**不会**被折叠 —— `Expose_To_Library` 不是 `ExposeToLibrary`。不写的键会被重置：省略
`ExposeToLibrary` 就设为 `false`，省略 `Description` 就清空；在生成资产上手改的值在下次重新生成时丢失 —— 2.0 根本不写的
标题除外。

**警告：**

**其他任何键都被忽略，并给出警告 `DSH3263`** （自 2.0.0 起）（1.x 什么都不说）。只查这四个名字：`Backend`、`Domain`、
`ShadingModel`、`BlendMode`、`TwoSided` 以及本页其他所有 `Shader` 键在 `ShaderFunction` 块里**什么都不做**，把这四个键拼错
—— `Descriptions`、`Expose_To_Library`、`LibraryCategory` —— 也是同一个警告。

**警告：**

反编译器把 `UMaterialFunction` 导出成 `.dsf` 时不会写出 `Settings` 块。这些值在那次往返中全部丢失，必须手工补回。

和这些设置错误最容易混淆的检查：材质函数至少要有一个输出（`DSH4315`）；`ShaderLayer` 或 `ShaderLayerBlend` 要有
`MaterialAttributes` 输出（`DSH3278`）；`ShaderLayer` 的唯一输入和输出就是这个材质（`DSH6204`）；`ShaderLayerBlend` 除了材质
输出只有输入，且至少一个是 `MaterialAttributes`（`DSH6205`）；函数体段是 `Graph` 不是 `Code`（`DSH2246`）。

`VirtualFunction` 块也接受名为 `Settings` 的段，但那是 `Options` 的同义词，键集完全不同 —— `Asset`（必需）和 `Description`。
`Function` 和 `GraphFunction` 根本没有 `Settings` 段。

<a id="备注"></a>

## 备注

- 反射的面是**引擎的**，不是插件的。自定义或修改过的引擎会通过同一个解析器暴露它自己的属性和枚举值，所以本页只描述原版
  UE 5.3 – 5.8 的面。
- 在 [ThinCustom 后端](backend.md)下，所有设置都落在**隐藏的基础 `UMaterial`** 上，而不是导出的实例上。从实例上
  读混合模式看到的是继承来的值。
- 键在到达发射器时被小写，所以构建期诊断里引用的键是**小写**拼写，不是源码里写的。
- 设置失败对每个材质只报一次，即 `DSH8215`；消息指出材质并引用反射写入器的码和消息。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3261: Expected a setting name such as 'BlendMode', found {0}.` | Settings 后面没有 {、语句不以名字开头、名字后没有 =（键里有 . 或 \[ 时就是这样），或 = 后没有值。 | 一个键只写一个名字；嵌套路径和下标在 1.x 键里已经不可用。 |
| `DSH3262: The setting '{0}' is written twice; the later value wins, as it did in 1.x.` | 警告：同一个名字写了两次（忽略大小写比较）。 |  |
| `DSH7127: Unsupported BlendMode/RenderType '%s'.` | BlendMode / RenderType 的值没有匹配到任何别名。 |  [详解](material-enums.md) |
| `DSH7129: Unsupported ShadingModel '%s'.` | ShadingModel 的值没有匹配到任何别名。 |  [详解](material-enums.md) |
| `DSH7128: ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | 在 UE 5.3 上值 trim 并折叠大小写后是 Substrate 或 Strata。 |  |
| `DSH7130: Unsupported MaterialDomain '%s'.` | MaterialDomain / Domain 的值没有匹配到任何别名。 |  [详解](material-enums.md) |
| `DSH7118: Unsupported material setting '%s'.` | 没有属性匹配这个键 —— 常见的未知键错误。 | 到 UMaterial 上核对属性名，或确认该键是否需要别名。 |
| `DSH7121: Setting '%s' requires an explicit [index].` | 命名了定长数组属性却没有下标。 |  |
| `DSH7124: Failed to create a transient material for Settings validation.` | 探针材质无法分配。 |  |
| `DSH7125: Invalid value '%s' for setting '%s'. %s` | 字面量写到探针上失败（DSH7126：写到真实材质上失败）；句号之后是按类型的码和消息。 |  |
| `DSH8215: A material setting on '{0}' was refused. {1}` | 以上每个构建期码都包在它里面到达你。 |  |
| `DSH7232: 'Substrate = {0}' is not a Substrate mode; the modes are 'Legacy', 'Bridge' and 'Native'.` | Substrate 不是 Legacy、Bridge 或 Native。 |  |
| `DSH7200: '{0}' is set twice by '#pragma material'; it was already set on line {1}.` | .dss 里同一个键被 #pragma material 设了两次（1.x 的 Settings 块报 DSH3262）。 |  |
| `DSH3263: '{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end.` | 警告：材质函数块上四个键以外的键。 |  |
| `DSH3264: 'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration.` | 警告：材质函数上写了 UserExposedCaption。 |  |
| `DSH3265: Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end.` | 警告：ExposeToLibrary 不是布尔字面量，该设置被忽略。 |  |

`Backend` 的值在绑定源码时检查 —— 见 [Backend](backend.md#诊断)。完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_ShaderSettings", Root="Game")
{
    Properties {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; Slider(0, 1)];
    }

    Settings {
        // 特殊键。
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Masked";

        // 反射布尔；b 前缀可写可不写。
        TwoSided   = true;
        FullyRough = true;
        bIsSky     = false;

        // 反射的数值与枚举。
        OpacityMaskClipValue  = 0.25;
        MaterialDecalResponse = "ColorNormalRoughness";

        // 别名 -> TranslucencyLightingMode，按 display name 匹配。
        LightingMode = "Surface";

        // 对象引用。
        PhysicalMaterial = Path(Engine, "EngineMaterials/DefaultPhysicalMaterial");
    }

    Outputs {
        vec3  Color;
        float Rough;
        float Mask;
        Base.BaseColor   = Color;
        Base.Roughness   = Rough;
        Base.OpacityMask = Mask;
    }

    Graph {
        Color = BaseColor.rgb;
        Rough = Roughness;
        Mask  = BaseColor.a;
    }
}
```

生成后的材质状态：

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

<a id="下一步"></a>

## 下一步

- [枚举取值](material-enums.md) —— 全部可接受的 `ShadingModel`、`BlendMode`、`Domain` 拼写
- [Backend](backend.md) —— 物化策略，以及每种后端产出什么
- [项目设置](project.md) —— 扩展枚举拼写的映射表
- [重新生成](../generation/regeneration.md) —— 一次重建会重置什么
