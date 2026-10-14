---
title: "DreamShaderSettings.h"
description: "DreamShaderSettings.h"
---

<a id="dreamshadersettingsh"></a>

# DreamShaderSettings.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderSettings.h**

项目设置对象、默认后端与图布局枚举，以及为每个 `ShadingModel`、`BlendMode` 和 `Domain` 值拼写提供支持的名称到引擎枚举解析器。

定义于头文件 `DreamShaderSettings.h`。

| | |
| :-- | :-- |
| 模块 | `DreamShader` (Runtime) |
| 包含 | `#include "DreamShaderSettings.h"` |
| 命名空间 | 全局作用域 |
| 导出宏 | `DREAMSHADER_API` 于 `UDreamShaderSettings` |
| 反射 | 2 个 `UENUM`，1 个 `UCLASS` 含 17 个 `UPROPERTY` |
| 引入 | `CoreMinimal.h`、`Engine/DeveloperSettings.h`、`Engine/EngineTypes.h`、`MaterialDomain.h` *（自 1.3.6 起）* |

<a id="edreamshaderdefaultbackend"></a>

## `EDreamShaderDefaultBackend`

```cpp
/** Backend used for source files that do not specify Settings = { Backend = "..." } themselves. */
UENUM()
enum class EDreamShaderDefaultBackend : uint8
{
    Graph,
    Instance,
    ThinCustom,
};
```

| 枚举值 | 值 | 含义 |
| :-- | :-- | :-- |
| `Graph` | `0` | 为每个材质构建一个 `UMaterial` 节点图——完整的 DSL 功能集，一个可见资产。 |
| `Instance` | `1` | `ThinCustom` 的**已弃用**别名 *（自 1.5.0 起）*。保留一个弃用窗口，使现有配置和 `Settings = { Backend = "Instance" }` 源可继续工作。旧式无图宿主实例后端已停用。 |
| `ThinCustom` | `2` | 在隐藏的每材质基础 `UMaterial` 上构建图，并为其生成一个轻量级材质实例。**默认值。** |

> [!WARNING]
> 配置为 `DefaultBackend = Instance` 的项目产生的资产与 `ThinCustom` 完全相同，且没有警告、没有日志行；唯一的提示是工具提示和本文档。写入 `Backend = "Instance"` 的源文件会被告知：它会构建 `ThinCustom`，并带有警告 [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) *（自 2.0.0 起）*。

<a id="resolution-at-compile-time"></a>

### 编译期解析

项目设置由编译管线映射（`Instance` 和 `ThinCustom` 映射到 `ThinCustom`，`Graph` 映射到 `Graph`），然后交给绑定器，后者在其上应用每文件的 `Backend`。

| `Settings = { Backend = … }` | `DefaultBackend` | 解析后的后端 |
| :-- | :-- | :-- |
| *（缺失）* | `Instance` | `ThinCustom` |
| *（缺失）* | `ThinCustom` | `ThinCustom` |
| *（缺失）* | `Graph` | `Graph` |
| `"Instance"` | *（任意）* | `ThinCustom`，警告 `DSH7204` *（自 2.0.0 起）* |
| `"ThinCustom"` | *（任意）* | `ThinCustom` |
| `"Graph"` | *（任意）* | `Graph` |
| `""`（空字符串） | *（任意）* | 错误 [`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201) *（自 2.0.0 起；1.x 将其视为 `Graph`）* |
| 其他任何内容 | *（任意）* | 错误 [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202) |

`Backend` 值在比较时会忽略大小写，并去除首尾空白和引号。参见[后端](../settings/backend.md)。

<a id="edreamshadergraphlayoutstyle-since-200"></a>

## `EDreamShaderGraphLayoutStyle` *（自 2.0.0 起）*

```cpp
/** How the nodes of a generated graph are placed. */
UENUM()
enum class EDreamShaderGraphLayoutStyle : uint8
{
    Classic,
    Blocks,
    SourceBands,
    Layered,
};
```

| 枚举值 | 值 | 含义 |
| :-- | :-- | :-- |
| `Classic` | `0` | 1.x 布局，在完成的图上计算。 |
| `Blocks` | `1` | 每个 `#pragma region` 或语句序列一个框，按源顺序排列；框之间使用命名重路由。**默认值。** |
| `SourceBands` | `2` | 每条源语句一个水平带；不插入重路由。 |
| `Layered` | `3` | 整个图的一个分层绘制；不插入重路由。 |

三种非 `Classic` 样式在编译器的 IR 上计算。节点位置不属于生成资产摘要的一部分。参见[图布局](../generation/graph-layout.md)。

<a id="udreamshadersettings"></a>

## `UDreamShaderSettings`

```cpp
UCLASS(Config=Engine, DefaultConfig, meta=(DisplayName="DreamShader"))
class DREAMSHADER_API UDreamShaderSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UDreamShaderSettings();

    virtual FName GetContainerName() const override;
    virtual FName GetCategoryName() const override;
    virtual FName GetSectionName() const override;
#if WITH_EDITOR
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
    bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
    bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;

    static FString NormalizeMappingKey(const FString& InName);
    static void BuildDefaultShadingModelMappings(TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings);
    static void BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings);
    static void BuildDefaultMaterialDomainMappings(TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings);

    virtual void PostInitProperties() override;

    // 17 UPROPERTYs — see below.

private:
    static FString NormalizeShadingModelKey(const FString& InName);
    static FString NormalizeBlendModeKey(const FString& InName);
    static FString NormalizeMaterialDomainKey(const FString& InName);
};
```

通过 `GetDefault<UDreamShaderSettings>()` 获取它——它是一个 `UDeveloperSettings`，因此该类默认对象*就是*活动配置实例。没有子系统，模块上也没有 getter。

<a id="panel-location"></a>

### 面板位置

全部五个面板重写均为内联，且返回常量。

| 重写 | 返回 | 可用性 |
| :-- | :-- | :-- |
| `GetContainerName()` | `TEXT("Project")` | 始终 |
| `GetCategoryName()` | `TEXT("DreamPlugin")` | 始终 |
| `GetSectionName()` | `TEXT("DreamShader")` | 始终 |
| `GetSectionText()` | `"Dream Shader"` | 仅 `WITH_EDITOR` |
| `GetSectionDescription()` | `"Dream Shader Settings"` | 仅 `WITH_EDITOR` |

因此面板位于 **Project Settings ▸ DreamPlugin ▸ Dream Shader**——而不是 *Plugins* 下。`UCLASS` 说明符 `Config=Engine, DefaultConfig` 会将每个 `Config` 属性持久化到项目的 `Config/DefaultEngine.ini` 中的 `[/Script/DreamShader.DreamShaderSettings]` 节。

<a id="constructor"></a>

### 构造函数

`UDreamShaderSettings::UDreamShaderSettings()` 恰好设置两个成员：

```cpp
SourceDirectory.Path          = TEXT("DShader");
GeneratedShaderDirectory.Path = TEXT("Intermediate/DreamShader/GeneratedShaders");
```

> [!NOTE]
> **三个 `*Mappings` 映射初始为空并保持为空**，除非用户添加条目。内置别名目录从不实体化到其中；它们会在每次 `TryResolve*` 调用时重新构建到一个一次性的本地映射中。空的 *Mappings* 面板并不意味着“不识别任何别名”。

<a id="properties"></a>

### 属性

每个 `UPROPERTY` 都是 `Config, EditAnywhere`。

| 属性 | 类型 | 类别 | 默认值 | 额外元数据 |
| :-- | :-- | :-- | :-- | :-- |
| `ShadingModelMappings` | `TMap<FString, TEnumAsByte<EMaterialShadingModel>>` | `Mappings` | *空* | — |
| `BlendModeMappings` | `TMap<FString, TEnumAsByte<EBlendMode>>` | `Mappings` | *空* | — |
| `MaterialDomainMappings` | `TMap<FString, TEnumAsByte<EMaterialDomain>>` | `Mappings` | *空* | — |
| `SourceDirectory` | `FDirectoryPath` | `Paths` | `DShader` | `RelativeToGameDir` |
| `GeneratedShaderDirectory` | `FDirectoryPath` | `Paths` | `Intermediate/DreamShader/GeneratedShaders` | `RelativeToGameDir` |
| `bScanPluginSourceDirectories` | `bool` | `Paths` | `true` | `DisplayName="Scan Plugin Source Directories"`、`ToolTip` |
| `DefaultBackend` | `EDreamShaderDefaultBackend` | `Compiler` | `ThinCustom` | `DisplayName="Default Compiler Backend"`、`ToolTip` |
| `bShowEphemeralMaterials` | `bool` | `Compiler` | `false` | `DisplayName="Show Ephemeral Materials"`、`ToolTip` |
| `PreprocessorDefines` | `TMap<FString, FString>` | `Compiler` | *空* | `DisplayName="Preprocessor Defines"`、`ToolTip` |
| `GraphLayoutStyle` | `EDreamShaderGraphLayoutStyle` | `Compiler` | `Blocks` | `DisplayName="Graph Layout Style"`、`ToolTip` |
| `bAutoCompileOnSave` | `bool` | `Compiler` | `true` | — |
| `SaveDebounceSeconds` | `float` | `Compiler` | `0.25f` | `ClampMin="0.05"`、`ClampMax="10.0"`、`UIMin="0.05"`、`UIMax="2.0"` |
| `bVerboseLogs` | `bool` | `Compiler` | `false` | — |
| `bExportDecompiledLayout` | `bool` | `Decompiler` | `true` | — |
| `bOpenInNewWindow` | `bool` | `Editor` | `true` | — |
| `InstanceSubfolder` | `FString` | `Editor` | `TEXT("Instances")` | `DisplayName="Material Instance Subfolder"`、`ToolTip` |
| `bSyncSourceReferencesOnAssetRename` | `bool` | `Editor` | `true` | `DisplayName="Sync Source References On Asset Rename"`、`ToolTip` |

每个属性的*作用*见[项目设置](../settings/project.md)；本页记录其声明。

> [!NOTE]
> 这里刻意**没有持久化开/关切换**。头文件中记录了原因：`ThinCustom`
> 产品在编辑器中默认是 Ephemeral 的——源文件才是创作界面——并且在显式操作、cook 或创建子实例时才会实体化。`Graph` 材质
> 和材质函数完全没有 Ephemeral 状态。`DefaultBackend` 是唯一的编译器
> 开关。参见 [Ephemeral 材质](../generation/ephemeral.md)。

<a id="normalizemappingkey"></a>

## `NormalizeMappingKey`

```cpp
static FString NormalizeMappingKey(const FString& InName);
```

按顺序应用：`TrimStartAndEndInline()`、`ToLowerInline()`，然后移除每个 `" "`、`"_"`
和 `"-"`。

| 输入 | 结果 |
| :-- | :-- |
| `"Two Sided Foliage"` | `"twosidedfoliage"` |
| `"two-sided_foliage"` | `"twosidedfoliage"` |
| `"  TWOSIDEDFOLIAGE  "` | `"twosidedfoliage"` |
| `"Runtime Virtual Texture"` | `"runtimevirtualtexture"` |

因此别名匹配不区分大小写、空格、下划线和连字符。

> [!WARNING]
> 不要将其与 [`UE::DreamShader::NormalizeSettingKey`](types.md#normalizesettingkey) 混淆，
> 后者只做修剪和小写化。`NormalizeMappingKey` 应用于设置**值**（枚举
> 别名）；`NormalizeSettingKey` 应用于设置**键**。参见
> [归一化器比较](dreamshader-module.md#the-four-normalizers-compared)。

三个私有转发函数 `NormalizeShadingModelKey`、`NormalizeBlendModeKey` 和
`NormalizeMaterialDomainKey` 调用 `NormalizeMappingKey` 且不做任何额外处理。

<a id="the-resolvers"></a>

## 解析器

```cpp
bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;
```

三者共享同一个算法：

| 步骤 | 操作 |
| :-- | :-- |
| 1 | 用 `NormalizeMappingKey` 归一化 `InName`。 |
| 2 | 线性扫描**用户配置**的映射，扫描时对每个已存储的键进行归一化。一旦匹配，写入 `OutValue` 并返回 `OutValue != <ENUM>_MAX`。 |
| 3 | 如果没有任何匹配，则将默认目录构建到一个**全新的局部映射**中，并以相同方式扫描。 |
| 4 | 返回 `false`。`OutValue` 保持不变。 |

| 方面 | 行为 |
| :-- | :-- |
| 优先级 | 用户映射始终优先；第一个归一化键匹配会立即返回。 |
| 禁用别名 | 映射到 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 的条目会**用该哨兵值覆盖 `OutValue`** 并返回 `false`。它不会回退到内置表。 |
| 歧义 | 两个键归一化后相同的用户条目按 `TMap` 迭代顺序解析，该顺序未指定。 |
| 失败时的 `OutValue` | 除上述 `MAX` 哨兵情况外，步骤 1–4 不会改动它。 |
| 常量性 | `const`；在 CDO 上安全。 |

> [!NOTE]
> 步骤 3 会在**用户映射的每次未命中**时重建整个默认目录——包括对 `StaticEnum` 的完整遍历
> 以及逐索引的元数据查询。由于用户映射默认为空，
> 这实际上等于每次调用都会发生。在生成频率下这没问题；不要在
> 逐节点或逐帧循环中调用这些函数。

<a id="building-the-default-catalogues"></a>

## 构建默认目录

```cpp
static void BuildDefaultShadingModelMappings(TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings);
static void BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings);
static void BuildDefaultMaterialDomainMappings(TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings);
```

每个函数都会向 `OutMappings` 追加内容——它**不会**清空它——分两个阶段进行。

<a id="phase-1-reflected"></a>

### 阶段 1：反射

逐索引遍历 `StaticEnum<T>()`。对于每个索引：

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 读取值和名称字符串。 |
| 2 | 读取 `Hidden` 元数据——**仅在 `WITH_EDITORONLY_DATA` 下**。 |
| 3 | 应用按枚举的跳过谓词；如果它返回 `true`，则 `continue`。 |
| 4 | 构建别名：取名称，丢弃直到最后一个 `:`（含）之前的所有内容（作用域枚举支持），然后 `RemoveFromStart(<Prefix>, ESearchCase::CaseSensitive)`。 |
| 5 | 以追加方式添加它。 |

> [!WARNING]
> `UEnum` 元数据仅限编辑器。在非编辑器和发行版构建中 `bHidden` 被硬编码为
> `false`，因此不会因为隐藏而丢弃任何内容，只有跳过谓词中基于名称和值的部分
> 会生效。因此发行版构建可以识别一些编辑器
> 拒绝的额外别名。这是刻意的修复：`UEnum::HasMetaData` 在这些配置中无法编译。

阶段 2 通过同一个追加式辅助函数添加字面量别名。

<a id="additive-only-semantics"></a>

### 仅追加语义

当别名修剪后为空，**或**当映射已经包含那个完全相同的
`FString` 键时，辅助函数会跳过该别名——这是一次精确、区分大小写、未归一化的键比较。因为反射阶段
先运行，所以与反射别名逐字符相同的显式别名会被静默忽略。

<a id="ematerialshadingmodel--prefix-msm_"></a>

### `EMaterialShadingModel` — 前缀 `MSM_`

跳过谓词：

| 条件 | 结果 |
| :-- | :-- |
| 名称为 `Strata` 或 `MSM_Strata`，或值为 `MSM_Strata` | 当 `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`（UE ≥ 5.4）时保留；否则丢弃。**此测试最先运行并短路 `Hidden` 检查。** |
| `Hidden` 元数据 | 跳过 |
| 名称为 `NUM`、`MSM_NUM`、`MAX`、`MSM_MAX`、`FromMaterialExpression` 或 `MSM_FromMaterialExpression` | 跳过 |
| 值为 `MSM_NUM`、`MSM_MAX` 或 `MSM_FromMaterialExpression` | 跳过 |

反射别名——12 个，在 UE ≥ 5.4 上再加 `Strata`：

| | | | |
| :-- | :-- | :-- | :-- |
| `Unlit` | `DefaultLit` | `Subsurface` | `PreintegratedSkin` |
| `ClearCoat` | `SubsurfaceProfile` | `TwoSidedFoliage` | `Hair` |
| `Cloth` | `Eye` | `SingleLayerWater` | `ThinTranslucent` |
| `Strata` *（自 UE 5.4 起）* | | | |

显式别名——8 个，在 UE ≥ 5.4 上再加 2 个：

| 别名 | 值 | 备注 |
| :-- | :-- | :-- |
| `Default Lit` | `MSM_DefaultLit` | |
| `Lit` | `MSM_DefaultLit` | |
| `Preintegrated Skin` | `MSM_PreintegratedSkin` | |
| `Clear Coat` | `MSM_ClearCoat` | |
| `Subsurface Profile` | `MSM_SubsurfaceProfile` | |
| `Two Sided Foliage` | `MSM_TwoSidedFoliage` | |
| `Single Layer Water` | `MSM_SingleLayerWater` | |
| `Thin Translucent` | `MSM_ThinTranslucent` | |
| `Substrate` | `MSM_Strata` | *（仅 UE 5.4+）* |
| `Strata` | `MSM_Strata` | *（仅 UE 5.4+）*；**无操作**——反射阶段已经添加了完全相同的键 |

<a id="eblendmode--prefix-blend_"></a>

### `EBlendMode` — 前缀 `BLEND_`

跳过谓词：`Hidden` 元数据，或名称为 `MAX` 或 `BLEND_MAX`，或值为 `BLEND_MAX`。

反射别名——8 个：

| | | | |
| :-- | :-- | :-- | :-- |
| `Opaque` | `Masked` | `Translucent` | `Additive` |
| `Modulate` | `AlphaComposite` | `AlphaHoldout` | `TranslucentColoredTransmittance` |

`BLEND_TranslucentGreyTransmittance` 和 `BLEND_ColoredTransmittanceOnly` 是引擎值别名，
标记为 `UMETA(Hidden)`，并且在编辑器构建中会被 `Hidden` 测试丢弃。

显式别名——4 个：

| 别名 | 值 |
| :-- | :-- |
| `Cutout` | `BLEND_Masked` |
| `Transparent` | `BLEND_Translucent` |
| `PremultipliedAlpha` | `BLEND_AlphaComposite` |
| `Premultiplied` | `BLEND_AlphaComposite` |

<a id="ematerialdomain--prefix-md_"></a>

### `EMaterialDomain` — 前缀 `MD_`

跳过谓词：`Hidden` 元数据**除非值为 `MD_RuntimeVirtualTexture`**，或名称为
`MAX` 或 `MD_MAX`，或值为 `MD_MAX`。这个例外是刻意的——引擎将
`MD_RuntimeVirtualTexture` 标记为隐藏，而 DreamShader 保持其可用。

反射别名——7 个：

| | | | |
| :-- | :-- | :-- | :-- |
| `Surface` | `DeferredDecal` | `LightFunction` | `Volume` |
| `PostProcess` | `UI` | `RuntimeVirtualTexture` | |

显式别名——写入 9 个，生效 8 个：

| 别名 | 值 | 备注 |
| :-- | :-- | :-- |
| `DeferredDecal` | `MD_DeferredDecal` | **无操作** — 反射键完全相同 |
| `Decal` | `MD_DeferredDecal` | |
| `Light Function` | `MD_LightFunction` | |
| `Post Process` | `MD_PostProcess` | |
| `UserInterface` | `MD_UI` | |
| `User Interface` | `MD_UI` | |
| `Runtime Virtual Texture` | `MD_RuntimeVirtualTexture` | |
| `VirtualTexture` | `MD_RuntimeVirtualTexture` | |
| `Virtual Texture` | `MD_RuntimeVirtualTexture` | |

> [!NOTE]
> 带空格的别名是**查找冗余**的：`NormalizeMappingKey` 会去除空格，因此
> `Two Sided Foliage` 和 `TwoSidedFoliage` 会归一化为反射阶段
> 已覆盖的同一个键。它们作为不同的映射条目存在，以便 Project Settings 面板和
> 编辑器扩展所使用的桥接清单能够提供人类可读的拼写。

面向用户的目录（包括推荐使用哪些拼写）位于
[材质枚举](../settings/material-enums.md)。

<a id="notes"></a>

## 说明

- 反射阶段枚举的是**引擎**的枚举，因此确切的反射集合取决于
  引擎版本所定义的内容。上表列出的是当前引擎上的结果；插件自身按版本控制的唯一条目是 `Strata`。
- 全程使用 `TEnumAsByte<T>`，因为 `TMap` 中 `UPROPERTY` 的值类型必须可反射，
  而这些引擎枚举不是 `enum class`。
- `PostEditChangeProperty`（仅编辑器）对一个属性作出反应：编辑 `PreprocessorDefines`
  会提升定义修订号，之后 `Super` 才会广播该更改。其他所有设置更改均由
  编辑器模块自身的面板钩子处理。
- `PostInitProperties` 会在仅存在旧键时，将 2.0 之前的配置键 `bShowInMemoryMaterialsInContentBrowser` 合并到
  `bShowEphemeralMaterials` 中 *(自 2.0.0 起)*。
- 该头文件包含了 `MaterialDomain.h` *(自 1.3.6 起)*，因此包含
  `DreamShaderSettings.h` 的翻译单元无需自行添加包含即可获得 `EMaterialDomain`。

<a id="example"></a>

## 示例

将用户书写的拼写解析为引擎枚举：

```cpp
#include "DreamShaderSettings.h"

bool ApplyShadingModelSetting(UMaterial* Material, const FString& InValue, FString& OutError)
{
    const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();

    EMaterialShadingModel ShadingModel = MSM_DefaultLit;
    if (!Settings->TryResolveShadingModel(InValue, ShadingModel))
    {
        OutError = FString::Printf(TEXT("Unknown ShadingModel '%s'."), *InValue);
        return false;
    }

    Material->SetShadingModel(ShadingModel);
    return true;
}
```

以下四个调用均成功，并选择同一个着色模型：

```cpp
ApplyShadingModelSetting(M, TEXT("TwoSidedFoliage"),   Error);
ApplyShadingModelSetting(M, TEXT("Two Sided Foliage"), Error);
ApplyShadingModelSetting(M, TEXT("two-sided_foliage"), Error);
ApplyShadingModelSetting(M, TEXT("  TWOSIDEDFOLIAGE"), Error);
```

在没有设置对象的情况下枚举内置目录——工作区服务
就是这样导出别名清单的：

```cpp
TMap<FString, TEnumAsByte<EBlendMode>> Aliases;
UDreamShaderSettings::BuildDefaultBlendModeMappings(Aliases);   // 12 entries: 8 reflected + 4 explicit
```

<a id="see-also"></a>

## 另请参阅

- [项目设置](../settings/project.md) — 从用户角度的同一对象及其效果
- [材质枚举](../settings/material-enums.md) — 完整的可接受值目录
- [后端](../settings/backend.md) — `DefaultBackend` 如何与逐文件的 `Backend` 交互
- [`DreamShaderTypes.h`](types.md) — `NormalizeSettingKey`，另一个归一化器
- [`DreamShaderModule.h`](dreamshader-module.md) — 使用这两个路径设置的目录辅助工具
- [`DreamShaderMaterialInstance.h`](material-instance.md) — 其 `IsAsset()` 读取 `bShowEphemeralMaterials` 的类
- [`DreamShaderVersionCompat.h`](version-compat.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`，即 `Strata` 门控
- [着色器设置](../settings/material.md) — 这些解析器所服务的 `Settings` 键
- [工作区](../../tools/workspace.md) — 导出的别名清单和 `bridge.db`
