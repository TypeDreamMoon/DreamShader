---
title: "DreamShaderMaterialInstance.h"
description: "DreamShaderMaterialInstance.h"
---

<a id="dreamshadermaterialinstanceh"></a>

# DreamShaderMaterialInstance.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderMaterialInstance.h**

`UDreamShaderMaterialInstance` —— ThinCustom 后端生成的可寻址资产：一个常量材质实例，其父级是隐藏的、携带真实节点图的每材质基础 `UMaterial`。

定义于头文件 `DreamShaderMaterialInstance.h`。

| | |
| :-- | :-- |
| 模块 | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderMaterialInstance.h"` |
| 命名空间 | 全局作用域 |
| 基类 | `UMaterialInstanceConstant` |
| 导出宏 | `DREAMSHADER_API` |
| 起始版本 | `1.5.0` —— ThinCustom 后端 |
| 引入 | `CoreMinimal.h`、`Materials/MaterialInstanceConstant.h` |

<a id="synopsis"></a>

## 概要

```cpp
UCLASS(ClassGroup = DreamShader)
class DREAMSHADER_API UDreamShaderMaterialInstance : public UMaterialInstanceConstant
{
    GENERATED_BODY()

public:
    /** DreamShader source file this instance was generated from. */
    UPROPERTY(VisibleAnywhere, Category = "DreamShader")
    FString SourceFilePath;

    /** Hash of the source text (the regeneration skip check compares against it). */
    UPROPERTY(VisibleAnywhere, Category = "DreamShader")
    FString SourceHash;

    //~ Begin UMaterialInstance interface
    virtual bool HasOverridenBaseProperties() const override;
    //~ End UMaterialInstance interface

    //~ Begin UObject interface
    virtual bool IsAsset() const override;
    //~ End UObject interface
};
```

该类除这两个重写之外没有添加任何方法，没有构造函数，也没有 `PostLoad`。其他一切——参数、设置、域、场景读取——都位于隐藏的基类上，作为普通表达式和材质属性存在，由引擎原生枚举。

<a id="properties"></a>

## 属性

| 属性 | 类型 | 说明符 | 写入方式 | 含义 |
| :-- | :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `VisibleAnywhere`、`Category="DreamShader"` | **绝对规范化**源路径，由 [`NormalizeSourceFilePath`](dreamshader-module.md#normalizesourcefilepath) 生成 | 此实例生成自的源文件。由实例工厂和编译器服务读回，并显示在细节面板中。 |
| `SourceHash` | `FString` | `VisibleAnywhere`、`Category="DreamShader"` | **构建键**的 8 位十六进制 CRC32，格式为 `%08x`：预处理后的源文件及其包含的头文件，再加上插件和引擎版本、改变构建内容的设置，以及源文件读取的宏定义（[缓存](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash)） | 仅供参考。请参阅下方警告。 |

两者在细节面板中均为只读——`VisibleAnywhere`，而非 `EditAnywhere`——且都不是 `Config` 或 `Transient`。

> [!WARNING]
> `SourceHash` 上的头文件注释说“重新生成跳过检查会与其进行比较”。这并不精确。跳过检查读取的是**包元数据**键 `DreamShader.SourceFile` 和 `DreamShader.SourceHash`，而不是这些 `UPROPERTY`。值虽然一致，但两条路径不同：元数据存储的是**相对项目**的源路径，而 `SourceFilePath` 存储的是**绝对**路径。不要基于这些属性实现你自己的跳过逻辑。请参阅[缓存](../generation/caching.md)。

<a id="hasoverridenbaseproperties"></a>

## `HasOverridenBaseProperties`

```cpp
virtual bool HasOverridenBaseProperties() const override;
```

| | |
| :-- | :-- |
| 返回 | 当 `Cast<UMaterial>(Parent) != nullptr` 时返回 `true` —— 即此实例的直接父级是基础 `UMaterial`，这使其成为其继承链的**根** |
| 否则 | 遵从 `Super::HasOverridenBaseProperties()` |

引擎在每次加载时都会在 `InitStaticPermutation` 内部根据此重新计算 `bHasStaticPermutationResource`，因此该测试必须是持久状态而不是瞬态标志——所以使用父类测试而非存储的布尔值。该模式与 `ULandscapeMaterialInstanceConstant` 一致。

| 链中位置 | `HasOverridenBaseProperties()` | 着色器映射 |
| :-- | :-- | :-- |
| 生成的实例（父级为隐藏基础 `UMaterial`） | `true` | 拥有自己的静态排列着色器映射 |
| 以该实例为父级的子 `UMaterialInstanceConstant` | 标准行为 | **共享**根的已编译映射 |

这正是重写的意义所在：基于一个 DreamShader 材质创建的任意数量的颜色和参数变体都会复用单个已编译的着色器映射，而不是各自编译自己的映射，从而为大量创建实例的项目限定了着色器映射的数量。有关实例创建操作，请参阅[材质内容浏览器](../../tools/material-browser.md)。

<a id="isasset"></a>

## `IsAsset`

```cpp
virtual bool IsAsset() const override;
```

| | |
| :-- | :-- |
| 返回 | 当所属包具有 `PKG_NewlyCreated` **且** `bShowEphemeralMaterials` 为 `false` 时返回 `false` |
| 否则 | 遵从 `Super::IsAsset()` |

返回 `false` 会将对象从内容浏览器、资产注册表的活动对象迭代器发现路径以及保存选择器中移除——这也可以防止意外的显式*保存*将仅内存的材质物化到磁盘。对象路径引用仍然正常解析；只有枚举会受到影响。持久化的实例没有 `PKG_NewlyCreated` 标志，其行为与任何其他资产相同。

> [!NOTE]
> 该设置在**每次调用**时都会通过 `GetDefault<UDreamShaderSettings>()` 读取。切换*显示临时材质*会立即改变结果，无需重新加载；编辑器会为每个仅内存实例重新广播资产创建和移除，以便浏览器刷新。请参阅[项目设置](../settings/project.md)。

<a id="role-in-the-thincustom-result"></a>

## 在 ThinCustom 结果中的角色

一次 Materialized ThinCustom 编译会生成**一个包**，其中包含：

```text
/Game/Materials/M_Emissive                      UDreamShaderMaterialInstance   (the addressable asset)
  └─ MB_DreamThinBase_M_Emissive                UMaterial                      (subobject, hidden base)
       └─ the generated node graph, parameters, settings, output bindings
```

Ephemeral 模式会将隐藏基础排除在该包之外：它是瞬态包中的一个瞬态 `UMaterial`，命名为 `MB_DreamThinBase_` 加上经过 [`SanitizeIdentifier`](dreamshader-module.md#sanitizeidentifier)（`MB_DreamThinBase__Game_Materials_M_Emissive`）处理后的实例包路径，因此位于不同文件夹中、叶名称相同的两个实例永远不会共享同一个基础。请参阅[临时材质](../generation/ephemeral.md)。

| 关注点 | 所在位置 |
| :-- | :-- |
| 节点图、`Properties` 节点、输出绑定、`Settings`（着色模型、混合模式、域、标志） | 隐藏基础 `UMaterial` |
| 资产标识、包、可寻址对象路径 | `UDreamShaderMaterialInstance` |
| 静态排列着色器映射 | 实例，作为链根 |
| 手工编写的参数覆盖 | 实例——在重建之前读取，并在重建之后按名称和种类写回 *(自 1.9.0 起)* |

> [!WARNING]
> 在 ThinCustom 后端下重新生成会重建隐藏基础的图并清除实例的参数值；覆盖会先被捕获，随后按名称恢复。如果源中某个覆盖的参数被重命名或移除，该覆盖将被丢弃，并会作为 [`DSH8155`](../diagnostics/DSH8xxx.md#dsh8155) 报告一次。为重建绝不能触碰的值创建一个**子**实例。请参阅[重新生成](../generation/regeneration.md#parameter-overrides-on-a-generated-instance)。

该实例使用 `NewObject<UDreamShaderMaterialInstance>(InstancePackage, FName(*AssetName), RF_Public | RF_Standalone)` 创建，或者当此类的实例已经占据目标路径时原地复用。该路径上的任何其他对象都会被拒绝而不是替换（[`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106)），不携带 DreamShader 源元数据的此类已保存实例也是如此（[`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107)）。

<a id="notes"></a>

## 注意事项

- `ClassGroup = DreamShader` 将此类放在类选择器中它自己的组内。没有 `BlueprintType`，没有 `Blueprintable`，也没有为手动创建注册的工厂——该类只会由编译器的资产工厂实例化。
- 该类位于 **Runtime** 模块中，而不是编辑器模块，因为烘焙构建必须能够加载烘焙时物化的实例。
- 两个重写都不查询源属性。`SourceFilePath` 和 `SourceHash` 对引擎行为没有任何影响；它们只为工具和细节面板而存在。
- 在 `Graph` 后端下完全不会生成实例——目标路径上的资产是一个普通的 `UMaterial`。请参阅[后端](../settings/backend.md)。

<a id="example"></a>

## 示例

查找选中资产背后的源文件：

```cpp
#include "DreamShaderMaterialInstance.h"

FString GetDreamShaderSource(const UMaterialInterface* Material)
{
    if (const UDreamShaderMaterialInstance* Instance = Cast<UDreamShaderMaterialInstance>(Material))
    {
        return Instance->SourceFilePath;   // absolute, '/' separators
    }
    return FString();
}
```

生成实例的细节面板视图：

```text
Parent                 MB_DreamThinBase_M_Emissive
DreamShader
  Source File Path     I:/Project/DShader/Materials/M_Emissive.dsm
  Source Hash          9f2c41ab
```

<a id="see-also"></a>

## 另请参阅

- [临时材质](../generation/ephemeral.md) — 隐藏的基础、可见性与具现化
- [后端](../settings/backend.md) — `ThinCustom` 与 `Graph`，以及已弃用的 `Instance` 别名
- [重新生成](../generation/regeneration.md) — 重建会在实例上销毁什么
- [缓存](../generation/caching.md) — 跳过检查实际使用的包元数据键
- [项目设置](../settings/project.md) — `IsAsset()` 背后的可见性开关
- [`DreamShaderSettings.h`](settings.md) — C++ 中的 `bShowEphemeralMaterials`
- [材质内容浏览器](../../tools/material-browser.md) — 创建子实例并进行具现化
- [`Shader`](../language/shader.md) — 生成此资产的代码块
- [资产路径](../generation/asset-paths.md) — 包的落盘位置
