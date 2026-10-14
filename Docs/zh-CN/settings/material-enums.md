---
title: "材质枚举"
description: "材质枚举"
---

<a id="material-enums"></a>

# 材质枚举

> [DreamShader](../index.md) » [设置](index.md) » **材质枚举**

以下是 [`Shader`](../language/shader.md) `Settings` 块的 `ShadingModel`、`BlendMode` / `RenderType` 以及
`MaterialDomain` / `Domain` 键所接受的值的完整拼写集合。

| | |
| :-- | :-- |
| 声明于 | `.dsm` —— 作为 `Shader` `Settings` 键的值 |
| 类型 | 值目录 |
| 生成 | `UMaterial::BlendMode`，材质的着色模型，`UMaterial::MaterialDomain` |

<a id="synopsis"></a>

## 概要

```c
Settings [=] {
    [ShadingModel   = <shading-model>;]
    [{ BlendMode | RenderType }     = <blend-mode>;]
    [{ MaterialDomain | Domain }    = <domain>;]
}
```

<a id="how-a-value-is-matched"></a>

## 值的匹配方式

1. 值先被规范化：去除首尾空白、转为小写，然后删除所有空格、`_` 和 `-`。
2. 先扫描该键对应的项目映射表。**第一个匹配规范化键的条目即胜出并立即返回** —— 项目条目会遮蔽内置拼写，
   且不会继续回落。
3. 只有在没有任何项目条目匹配时，才构建内置表并以相同方式扫描。
4. 若匹配项的值是枚举的 `_MAX` 哨兵，则算作**失败**；因此项目条目中
   指向 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 会让该拼写不再能解析成功。

由于第 1 步，`"Default Lit"`、`"DefaultLit"`、`"default_lit"` 和 `"DEFAULT-LIT"` 是同一个
别名。下面的表列出插件注册的原始拼写；每个拼写的各种分隔符变体
同样有效。

内置表分两个阶段构建：**反射**阶段遍历引擎枚举并剥掉枚举项前缀，然后**显式**阶段
补上手写拼写。显式阶段不会用相同的键覆盖反射阶段的条目，所以有几行显式条目
是保留下来便于阅读的空操作。

<a id="shadingmodel"></a>

## ShadingModel

从 `EMaterialShadingModel` 反射而来，剥掉 `MSM_` 前缀。

| 别名 | 来源 | 引擎枚举项 |
| :-- | :-- | :-- |
| `Unlit` | 反射 | `MSM_Unlit` |
| `DefaultLit` | 反射 | `MSM_DefaultLit` |
| `Subsurface` | 反射 | `MSM_Subsurface` |
| `PreintegratedSkin` | 反射 | `MSM_PreintegratedSkin` |
| `ClearCoat` | 反射 | `MSM_ClearCoat` |
| `SubsurfaceProfile` | 反射 | `MSM_SubsurfaceProfile` |
| `TwoSidedFoliage` | 反射 | `MSM_TwoSidedFoliage` |
| `Hair` | 反射 | `MSM_Hair` |
| `Cloth` | 反射 | `MSM_Cloth` |
| `Eye` | 反射 | `MSM_Eye` |
| `SingleLayerWater` | 反射 | `MSM_SingleLayerWater` |
| `ThinTranslucent` | 反射 | `MSM_ThinTranslucent` |
| `Strata` | 反射 *（自 UE 5.4 起）* | `MSM_Strata` |
| `Default Lit` | 显式别名 | `MSM_DefaultLit` |
| `Lit` | 显式别名 | `MSM_DefaultLit` |
| `Preintegrated Skin` | 显式别名 | `MSM_PreintegratedSkin` |
| `Clear Coat` | 显式别名 | `MSM_ClearCoat` |
| `Subsurface Profile` | 显式别名 | `MSM_SubsurfaceProfile` |
| `Two Sided Foliage` | 显式别名 | `MSM_TwoSidedFoliage` |
| `Single Layer Water` | 显式别名 | `MSM_SingleLayerWater` |
| `Thin Translucent` | 显式别名 | `MSM_ThinTranslucent` |
| `Substrate` | 显式别名 *（自 UE 5.4 起）* | `MSM_Strata` |
| `Strata` | 显式别名 *（自 UE 5.4 起）* —— 空操作，反射阶段已注册该键 | `MSM_Strata` |

在 UE 5.4 – 5.8 上共 **15 个不同的规范化别名**；UE 5.3 上是 **13 个**，因为它既没有 `strata`
也没有 `substrate`。

被排除的：`MSM_NUM`、`MSM_MAX`，以及被刻意排除的 **`MSM_FromMaterialExpression`**。写
`ShadingModel = "FromMaterialExpression";` 是错误（`DSH7129`），它不是从图里驱动着色模型的办法。

在 UE 5.3 上，`ShadingModel = "Substrate";` 和 `ShadingModel = "Strata";` 会用自己的码
（`DSH7128`）被拒绝，而不是通用的「不支持」错误。见 [诊断](#diagnostics)和
[Substrate 内置项](../builtins/substrate.md)。

<a id="blendmode"></a>

## BlendMode

从 `EBlendMode` 反射而来，剥掉 `BLEND_` 前缀。该键也可以写作 `RenderType`。

| 别名 | 来源 | 引擎枚举项 |
| :-- | :-- | :-- |
| `Opaque` | 反射 | `BLEND_Opaque` |
| `Masked` | 反射 | `BLEND_Masked` |
| `Translucent` | 反射 | `BLEND_Translucent` |
| `Additive` | 反射 | `BLEND_Additive` |
| `Modulate` | 反射 | `BLEND_Modulate` |
| `AlphaComposite` | 反射 | `BLEND_AlphaComposite` |
| `AlphaHoldout` | 反射 | `BLEND_AlphaHoldout` |
| `TranslucentColoredTransmittance` | 反射 | `BLEND_TranslucentColoredTransmittance` |
| `Cutout` | 显式别名 | `BLEND_Masked` |
| `Transparent` | 显式别名 | `BLEND_Translucent` |
| `PremultipliedAlpha` | 显式别名 | `BLEND_AlphaComposite` |
| `Premultiplied` | 显式别名 | `BLEND_AlphaComposite` |

在原版 UE 5.4 – 5.8 构建上共 **12 个不同的规范化别名**。`TranslucentColoredTransmittance` 是
Substrate 专有枚举项，所以这个数量与引擎版本相关。

被排除的：`BLEND_MAX`，以及两个被引擎标记为隐藏的枚举项 ——
`BLEND_TranslucentGreyTransmittance` 和 `BLEND_ColoredTransmittanceOnly`。它们都是表中已有值的别名，
所以没有任何值因此不可达。

> [!NOTE]
> `TranslucentColoredTransmittance` 挺不过反编译。[反编译器](../tools/decompiler.md)会把 `BLEND_TranslucentColoredTransmittance`
> 写回成 `"Translucent"`，所以 `UMaterial` → `.dsm` → `UMaterial` 的往返会降级混合模式。
> 反编译这类材质后请手工补回该设置。

<a id="domain"></a>

## Domain

从 `EMaterialDomain` 反射而来，剥掉 `MD_` 前缀。该键也可以写作
`MaterialDomain`，两者同时出现时它胜出。

| 别名 | 来源 | 引擎枚举项 |
| :-- | :-- | :-- |
| `Surface` | 反射 | `MD_Surface` |
| `DeferredDecal` | 反射 | `MD_DeferredDecal` |
| `LightFunction` | 反射 | `MD_LightFunction` |
| `Volume` | 反射 | `MD_Volume` |
| `PostProcess` | 反射 | `MD_PostProcess` |
| `UI` | 反射 | `MD_UI` |
| `RuntimeVirtualTexture` | 反射 —— 引擎里是隐藏的，插件刻意保留 | `MD_RuntimeVirtualTexture` |
| `DeferredDecal` | 显式别名 —— 空操作，反射阶段已注册该键 | `MD_DeferredDecal` |
| `Decal` | 显式别名 | `MD_DeferredDecal` |
| `Light Function` | 显式别名 | `MD_LightFunction` |
| `Post Process` | 显式别名 | `MD_PostProcess` |
| `UserInterface` | 显式别名 | `MD_UI` |
| `User Interface` | 显式别名 | `MD_UI` |
| `Runtime Virtual Texture` | 显式别名 | `MD_RuntimeVirtualTexture` |
| `VirtualTexture` | 显式别名 | `MD_RuntimeVirtualTexture` |
| `Virtual Texture` | 显式别名 | `MD_RuntimeVirtualTexture` |

共 **10 个不同的规范化别名**，在 UE 5.3 – 5.8 上保持稳定。

被排除的：`MD_MAX`。其他隐藏枚举项都会跳过，`MD_RuntimeVirtualTexture` 是
唯一刻意的例外 —— 引擎把它标记为隐藏，插件保留了它。

<a id="adding-project-specific-spellings"></a>

## 添加项目专属拼写

项目设置上有三个 `TMap<FString, …>` 属性用来扩展这些表：

| 设置 | 值类型 | 扩展 |
| :-- | :-- | :-- |
| `ShadingModelMappings` | `EMaterialShadingModel` | `ShadingModel` |
| `BlendModeMappings` | `EBlendMode` | `BlendMode` / `RenderType` |
| `MaterialDomainMappings` | `EMaterialDomain` | `MaterialDomain` / `Domain` |

它们位于 *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Mappings* 下，且**默认为空** —— 上面的内置表从不写入其中。参见
[项目设置](project.md)。

| 条目 | 效果 |
| :-- | :-- |
| 规范化后与某个内置别名相同的键 | **覆盖**内置值；使用项目里的值 |
| 全新的键 | 新增一个可接受的拼写 |
| 任何映射到 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 的键 | 让该拼写**解析失败**；它不会回落到内置表 |

键在规范化后比较，所以条目里的大小写、空格、下划线和连字符都无关紧要。

> [!WARNING]
> 两个规范化后相同的项目条目，扫描顺序由哈希表决定 —— 谁胜出是未定义的。不要同时添加 `My Model`
> 和 `my_model`。

<a id="notes"></a>

## 备注

- 反射阶段遍历的是编译进引擎的枚举内容。**改过的引擎构建会自动贡献它自己的值**：一个新增了 `MSM_Toon`
  的引擎，不用改插件也不用配项目映射，`ShadingModel = "Toon";` 就能用。上面的表
  描述的是原版 UE 5.3 – 5.8 构建。
- 枚举项成员来自编译后的引擎，所以它取决于引擎版本。插件自己门控的值是 `Strata` / `Substrate`，它只在 UE 5.4 及更新版本上注册它们。
- 带 `Hidden` 元数据的值只在保留编辑器专属数据的构建里被过滤。
  材质生成本来就是编辑器专属的，所以实际生效的正是这种配置。
- `Saved/DreamShader/Bridge/settings.json` 会导出这些表供编辑器补全使用。它
  按规范化别名去重，所以会显示 `"Default Lit"` 而不显示 `"DefaultLit"`。**两者都能
  编译** —— 那份清单是补全用的，不是接受集合。参见
  [工作区](../tools/workspace.md)和 [Bridge](../tools/bridge.md)。
- 不写某个键不会保留上次的值：重新生成的材质会先被重置为
  `Opaque` / `DefaultLit` / `Surface`。参见[Shader 设置](material.md#defaults--what-an-omitted-setting-resets-to)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 抛出时机 |
| :-- | :-- |
| [`DSH7129`](../diagnostics/DSH7xxx.md#dsh7129) | `ShadingModel` 值既不匹配任何项目映射也不匹配任何内置别名，或命中了映射到 `MSM_MAX` 的条目 |
| [`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128) | 在 UE 5.3 上，`ShadingModel` 值 trim 并折叠大小写后是 `Substrate` 或 `Strata` |
| [`DSH7127`](../diagnostics/DSH7xxx.md#dsh7127) | `BlendMode` / `RenderType` 值没有匹配项 |
| [`DSH7130`](../diagnostics/DSH7xxx.md#dsh7130) | `MaterialDomain` / `Domain` 值没有匹配项 |

这四条都在校验设置时抛出，早于任何一条设置被应用；它们包在
[`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)（会指出材质）里到达你，这次重建被回滚。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_EnumSpellings")
{
    Properties { vec3 Tint = vec3(0.9, 0.2, 0.1); }

    Settings {
        Domain       = "User Interface";   // -> MD_UI
        ShadingModel = "Lit";              // -> MSM_DefaultLit
        RenderType   = "Premultiplied";    // -> BLEND_AlphaComposite
    }

    Outputs {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph {
        Color = Tint;
        Alpha = 0.75;
    }
}
```

生成后的材质状态：

```text
MaterialDomain = MD_UI
ShadingModel   = MSM_DefaultLit
BlendMode      = BLEND_AlphaComposite
```

<a id="see-also"></a>

## 另请参见

- [Shader 设置](material.md) —— 这些值所属的特殊键
- [Settings](index.md) —— 块语法和三条规范化规则
- [项目设置](project.md) —— `ShadingModelMappings`、`BlendModeMappings`、`MaterialDomainMappings`
- [后端](backend.md) —— 剩下的那个特殊键
- [Substrate 内置](../builtins/substrate.md) —— UE 5.4+ 的 Substrate 面
- [输出绑定](../language/output-bindings.md) —— `Base.FrontMaterial` 和强制的 Substrate 模型
- [反编译器](../tools/decompiler.md) —— 写回出来的规范拼写
- [工作区](../tools/workspace.md) —— 导出的 `settings.json` 补全清单
- [诊断索引](../diagnostics/index.md) —— 所有诊断码
