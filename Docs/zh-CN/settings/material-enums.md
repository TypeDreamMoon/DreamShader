---
title: 枚举取值
description: 全部可接受的 ShadingModel、BlendMode、Domain 拼写，值的匹配方式，以及项目映射如何扩展或覆盖内置表。
---

三个 `Settings` 键取引擎枚举：`ShadingModel`、`BlendMode`（也可写 `RenderType`）和
`MaterialDomain`（也可写 `Domain`）。它们不走通用的
[反射枚举匹配](material.md#枚举字面量) —— 它们有自己的别名表，本页完整列出。

```c
Settings [=] {
    [ShadingModel                 = <shading-model>;]
    [{ BlendMode | RenderType }   = <blend-mode>;]
    [{ MaterialDomain | Domain }  = <domain>;]
}
```

<a id="值是怎么匹配的"></a>

## 值是怎么匹配的

1. 值先规范化：trim、小写，再删除所有空格、`_` 和 `-`。
2. 先扫描该键的**项目映射表**。第一个规范化键命中即胜出并立即返回 —— 项目条目会覆盖内置拼写，
   不会继续回落。
3. 只有在没有项目条目命中时，才构建并同样扫描内置表。
4. 命中的值若是枚举的 `_MAX` 哨兵，算作**失败**；所以指向 `MSM_MAX` / `BLEND_MAX` / `MD_MAX`
   的项目条目会让那个拼写不再解析成功。

由于第 1 步，`"Default Lit"`、`"DefaultLit"`、`"default_lit"`、`"DEFAULT-LIT"` 是同一个别名。
下面的表列的是插件注册的原始拼写，**每个拼写的各种分隔符变体同样有效**。

内置表分两阶段构建：**反射**阶段遍历引擎枚举并剥掉枚举项前缀，**显式**阶段再补上手写拼写。
显式阶段不会用相同的键覆盖反射阶段的条目，所以有几行显式条目其实是保留下来便于阅读的空操作。

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
| `Strata` | 反射 （自 UE 5.4 起） | `MSM_Strata` |
| `Default Lit` | 显式别名 | `MSM_DefaultLit` |
| `Lit` | 显式别名 | `MSM_DefaultLit` |
| `Preintegrated Skin` | 显式别名 | `MSM_PreintegratedSkin` |
| `Clear Coat` | 显式别名 | `MSM_ClearCoat` |
| `Subsurface Profile` | 显式别名 | `MSM_SubsurfaceProfile` |
| `Two Sided Foliage` | 显式别名 | `MSM_TwoSidedFoliage` |
| `Single Layer Water` | 显式别名 | `MSM_SingleLayerWater` |
| `Thin Translucent` | 显式别名 | `MSM_ThinTranslucent` |
| `Substrate` | 显式别名 （自 UE 5.4 起） | `MSM_Strata` |
| `Strata` | 显式别名 （自 UE 5.4 起） —— 空操作，反射阶段已注册该键 | `MSM_Strata` |

在 UE 5.4 – 5.8 上共 **15 个不同的规范化别名**；UE 5.3 上是 **13 个**，因为它既没有 `strata`
也没有 `substrate`。

被排除的：`MSM_NUM`、`MSM_MAX`，以及被刻意排除的 **`MSM_FromMaterialExpression`**。
写 `ShadingModel = "FromMaterialExpression";` 是错误（`DSH7129`），它不是从图里驱动 shading model 的办法。

**警告：**

`Substrate` / `Strata` 需要 （自 UE 5.4 起）。在 UE 5.3 上它们用自己的码 `DSH7128` 拒绝，而不是通用的
「不支持」错误。见 [Substrate 节点](../builtins/substrate.md)。

<a id="blendmode"></a>

## BlendMode

从 `EBlendMode` 反射而来，剥掉 `BLEND_` 前缀。键也可以写作 `RenderType`。

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

在原版 UE 5.4 – 5.8 上共 **12 个不同的规范化别名**。`TranslucentColoredTransmittance`
是 Substrate 专有枚举项，所以这个数量与引擎版本相关。

被排除的：`BLEND_MAX`，以及两个被引擎标记为 hidden 的枚举项 ——
`BLEND_TranslucentGreyTransmittance` 和 `BLEND_ColoredTransmittanceOnly`。它们都是表中已有值的别名，
所以没有任何值因此不可达。

**警告：**

`TranslucentColoredTransmittance` 挺不过反编译。[反编译器](../tools/decompiler.md)会把
`BLEND_TranslucentColoredTransmittance` 写回成 `"Translucent"`，所以 `UMaterial` → `.dsm` →
`UMaterial` 的往返会降级混合模式。反编译这类材质后请手工补回该设置。

<a id="domain"></a>

## Domain

从 `EMaterialDomain` 反射而来，剥掉 `MD_` 前缀。键也可以写作 `MaterialDomain`；两者同时出现时它胜出。

| 别名 | 来源 | 引擎枚举项 |
| :-- | :-- | :-- |
| `Surface` | 反射 | `MD_Surface` |
| `DeferredDecal` | 反射 | `MD_DeferredDecal` |
| `LightFunction` | 反射 | `MD_LightFunction` |
| `Volume` | 反射 | `MD_Volume` |
| `PostProcess` | 反射 | `MD_PostProcess` |
| `UI` | 反射 | `MD_UI` |
| `RuntimeVirtualTexture` | 反射 —— 引擎里是 hidden，插件刻意保留 | `MD_RuntimeVirtualTexture` |
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

被排除的：`MD_MAX`。其他 hidden 枚举项都会跳过，`MD_RuntimeVirtualTexture` 是唯一刻意的例外 ——
引擎把它标为 hidden，插件保留了它。

<a id="添加项目专属拼写"></a>

## 添加项目专属拼写

[项目设置](project.md)上有三个 `TMap` 属性用来扩展这些表。它们位于
*Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Mappings*，且**默认为空** ——
上面的内置表从不写进这三个 map，所以面板是空的并不代表“没有别名”。

| 设置 | 值类型 | 扩展 |
| :-- | :-- | :-- |
| `ShadingModelMappings` | `EMaterialShadingModel` | `ShadingModel` |
| `BlendModeMappings` | `EBlendMode` | `BlendMode` / `RenderType` |
| `MaterialDomainMappings` | `EMaterialDomain` | `MaterialDomain` / `Domain` |

| 条目 | 效果 |
| :-- | :-- |
| 键规范化后与某个内置别名相同 | **覆盖**内置值，使用项目里的值 |
| 全新的键 | 新增一个可接受的拼写 |
| 任何映射到 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 的键 | 让该拼写**解析失败**，且不会回落到内置表 |

键在规范化后比较，所以条目里的大小写、空格、下划线和连字符都无关紧要。

**警告：**

两个规范化后相同的项目条目，扫描顺序由哈希表决定 —— 谁胜出是未定义的。不要同时添加 `My Model`
和 `my_model`。

<a id="备注"></a>

## 备注

- 反射阶段遍历的是编译进引擎的枚举内容。**改过的引擎会自动贡献它自己的值**：一个新增了 `MSM_Toon`
  的引擎，不用改插件也不用配项目映射，`ShadingModel = "Toon";` 就能用。上面的表描述的是原版
  UE 5.3 – 5.8。
- 插件自己做版本门控的只有 `Strata` / `Substrate`，它们只在 UE 5.4 及以上注册。
- 带 `Hidden` 元数据的值只在保留编辑器数据的构建里被过滤。材质生成本来就是编辑器专用的，
  所以实际生效的正是这种配置。
- `Saved/DreamShader/Bridge/settings.json` 会导出这些表供编辑器补全使用。它按规范化别名去重，
  所以里面有 `"Default Lit"` 而没有 `"DefaultLit"`。**两者都能编译** —— 那份清单是补全用的，
  不是接受集合。
- 不写某个键不会保留上次的值：重新生成的材质会先被重置为 `Opaque` / `DefaultLit` / `Surface`。
  见[材质 Settings](material.md#不写某个设置时会重置成什么)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH7129: Unsupported ShadingModel '%s'.` | 值既不匹配项目映射也不匹配内置别名，或命中了映射到 MSM\_MAX 的条目。 |  |
| `DSH7128: ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | 在 UE 5.3 上值 trim 并折叠大小写后是 Substrate 或 Strata。 |  |
| `DSH7127: Unsupported BlendMode/RenderType '%s'.` | BlendMode / RenderType 的值没有匹配项。 |  |
| `DSH7130: Unsupported MaterialDomain '%s'.` | MaterialDomain / Domain 的值没有匹配项。 |  |

这四条都在校验设置时抛出，早于任何一条设置被应用；它们包在 `DSH8215`（会指出材质）里到达你，这次重建被回滚。

<a id="示例"></a>

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

<a id="下一步"></a>

## 下一步

- [材质 Settings](material.md) —— 这些值所属的键
- [项目设置](project.md) —— 三张映射表
- [Substrate 节点](../builtins/substrate.md) —— UE 5.4+ 的 Substrate 面
