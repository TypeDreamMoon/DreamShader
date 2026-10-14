---
title: "DreamShaderVersionCompat.h"
description: "DreamShaderVersionCompat.h"
---

<a id="dreamshaderversioncompath"></a>

# DreamShaderVersionCompat.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderVersionCompat.h**

十三个预处理宏，用于控制插件中所有依赖引擎版本的行为——此外，在该头文件之外还有一个模块定义：Custom Pass 开关，[`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass)。本页是手册中所有 *(自 UE 5.x 起)* 标记的唯一事实来源。

定义于头文件 `DreamShaderVersionCompat.h`。

| | |
| :-- | :-- |
| 模块 | `DreamShader`（Runtime），由 `DreamShader` 与 `DreamShaderEditor` 包含。`DreamShaderLang` 无法包含它——它仅依赖 `Core`，且没有可供判断的引擎版本 |
| 包含 | `#include "DreamShaderVersionCompat.h"` |
| 内容 | 13 个宏。**无类型、无函数、无命名空间。** |
| 依赖 | `Misc/CoreDelegates.h`（用于 `DREAMSHADER_POST_ENGINE_INIT_DELEGATE`）、`Runtime/Launch/Resources/Version.h` |
| 已验证引擎 | UE `5.3` – `5.8`（Win64） |

<a id="synopsis"></a>

## 概要

所有其他宏都基于的版本运算。使用它的功能开关位于下方[宏](#macros)一节。

```cpp
#include "Misc/CoreDelegates.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef DREAMSHADER_UE_MAJOR
#define DREAMSHADER_UE_MAJOR ENGINE_MAJOR_VERSION
#endif

#ifndef DREAMSHADER_UE_MINOR
#define DREAMSHADER_UE_MINOR ENGINE_MINOR_VERSION
#endif

#ifndef DREAMSHADER_UE_PATCH
#define DREAMSHADER_UE_PATCH ENGINE_PATCH_VERSION
#endif

#ifndef DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#define DREAMSHADER_WITH_SUBSTRATE_BUILTINS (DREAMSHADER_UE_MAJOR > 5 || (DREAMSHADER_UE_MAJOR == 5 && DREAMSHADER_UE_MINOR >= 4))
#endif

#define DREAMSHADER_UE_VERSION_AT_LEAST(MajorVersion, MinorVersion) \
    (DREAMSHADER_UE_MAJOR > (MajorVersion) || (DREAMSHADER_UE_MAJOR == (MajorVersion) && DREAMSHADER_UE_MINOR >= (MinorVersion)))

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
#define DREAMSHADER_ALLOW_SHRINKING_NO EAllowShrinking::No
#else
#define DREAMSHADER_ALLOW_SHRINKING_NO false
#endif
```

<a id="macros"></a>

## 宏

| 宏 | 展开为 | 可覆盖 | 用途 |
| :-- | :-- | :-- | :-- |
| `DREAMSHADER_UE_MAJOR` | `ENGINE_MAJOR_VERSION` | **是** —— 由 `#ifndef` 保护 | 所有其他测试所基于的主版本号。 |
| `DREAMSHADER_UE_MINOR` | `ENGINE_MINOR_VERSION` | **是** —— 由 `#ifndef` 保护 | 次版本号。 |
| `DREAMSHADER_UE_PATCH` | `ENGINE_PATCH_VERSION` | **是** —— 由 `#ifndef` 保护 | 仅由预处理器读取，作为内置定义 `DS_ENGINE_PATCH`。没有 C++ 版本测试使用它。 |
| `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` | `(MAJOR > 5 \|\| (MAJOR == 5 && MINOR >= 4))` —— 即 **UE ≥ 5.4** | **是** —— 由 `#ifndef` 保护 | 唯一的 Substrate 功能开关。 |
| `DREAMSHADER_WITH_MOON_ENGINE` | 当引擎的 `SceneTypes.h` 声明 `MP_MoonEncodedAttribute0` 时为 `1`，否则为 `0` | **是** —— 由 `#ifndef` 保护，且 `DreamShader.Build.cs` 根据该探测将其设为 `PublicDefinition` | Moon Engine 的额外材质属性。并非版本测试：该探测读取受保护代码所需的枚举值，因此对任何包含它的分支都有效。 |
| `DREAMSHADER_UE_VERSION_AT_LEAST(Major, Minor)` | `(MAJOR > Major \|\| (MAJOR == Major && MINOR >= Minor))` | 否 —— 无条件 `#define` | 通用的“至少该引擎版本”测试。 |
| `DREAMSHADER_ALLOW_SHRINKING_NO` | UE ≥ 5.4 时为 `EAllowShrinking::No`，低于时为 `false` | 否 —— 由 `#if` 选择 | 可移植性兼容层，详见下文。 |
| `DREAMSHADER_POST_ENGINE_INIT_DELEGATE()` | UE ≥ 5.8 时为 `FCoreDelegates::GetOnPostEngineInit()`，否则为下方的 `OnPostEngineInit` 数据成员 | 否 —— 由 `#if` 选择 | 5.8 新增了访问器并弃用了成员，因此直接命名任一者都会破坏另一个引擎。 |
| `DREAMSHADER_THUMBNAIL_PRIM_SHADERBALL` | UE ≥ 5.8 时为 `TPT_ShaderBall`，低于时为 `TPT_Sphere` | 否 —— 由 `#if` 选择 | `EThumbnailPrimType` 在 5.8 之前止于 `TPT_Cylinder`；预览会回退到球体。 |
| `DREAMSHADER_MATERIAL_AGGREGATE_HANDLES_SUBSTRATE` | **UE ≥ 5.8** | 否 —— 无条件 `#define` | `MaterialValueTypeToMaterialAggregateAttributeType` 是否有针对 `MCT_Substrate` 的分支。低于 5.8 时它会 `checkf(false)`，因此清单导出器和内置目录的反射都不会为 `UMaterialExpressionAggregate` 发起该调用。 |
| `DREAMSHADER_WITH_SCALAR_PARAMETER_CONTROL_TYPE` | **UE ≥ 5.7** | 否 —— 无条件 `#define` | `UMaterialExpressionScalarParameter` 是否具有 `ControlType`、`Enumeration` 和 `EnumerationIndex`。 |
| `DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER` | **UE ≥ 5.7** | 否 —— 无条件 `#define` | 哪个头文件声明了 `FMaterialParameterInfo`：5.7 起为 `Materials/MaterialParameters.h`，之前为 `MaterialTypes.h`。 |
| `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS` *(自 2.0.0 起)* | 当引擎的 `EMaterialParameterType` 具有 `ParameterCollection` 枚举值时 `1`，否则为 `0` | **是** —— 由 `#ifndef` 保护（回退到 UE ≥ 5.8），且 `DreamShader.Build.cs` 根据该探测将其设为 `PublicDefinition` | 材质实例是否可以覆盖参数集合。询问声明该枚举的头文件，因为 `case` 标签无法询问该类型。 |

六个受 `#ifndef` 保护的宏可由构建目标预先定义——例如在 5.4+ 引擎上通过定义 `DREAMSHADER_WITH_SUBSTRATE_BUILTINS=0` 来排除 Substrate 路径，或在测试时强制某个版本分支。`DreamShader.Build.cs` 正是利用这一点，根据其头文件探测来设置 `DREAMSHADER_WITH_MOON_ENGINE` 和 `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS`。该插件仅有的其他定义是 `DREAMSHADER_WITH_CUSTOM_PASS`（`DreamShaderPass.Build.cs` 的一个 `PublicDefinitions` 条目，本头文件并未定义它，见[下文](#dreamshader_with_custom_pass)），以及 `MOON_ENGINE=1`（当引擎的 `SceneTypes.h` 声明 `MP_MoonEncodedAttribute0` 时由 `DreamShaderEditor.Build.cs` 添加的一个 `PrivateDefinitions` 条目，且没有任何源文件读取它）。

> [!NOTE]
> `DREAMSHADER_ALLOW_SHRINKING_NO` **没有任何行为差异**。它存在仅仅是因为 UE 5.4 将 `TArray::RemoveAt`、`TArray::Pop`、`FString::RightChopInline` 和 `FString::LeftChopInline` 末尾的 `bool bAllowShrinking` 参数改成了 `EAllowShrinking` 枚举。两种写法都表示“不要缩减分配”。它在整个插件中使用，并非调节旋钮。

> [!NOTE]
> 在该头文件之外的插件任何地方，都**没有**原始的 `#if ENGINE_MAJOR_VERSION`、`ENGINE_MINOR_VERSION` 或 `UE_VERSION_NEWER_THAN` 测试。唯一在该头文件之外的版本测试不是 C++：`DreamShaderPass.Build.cs` 读取 `Target.Version` 来设置 [`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass)。所有依赖版本的行为都通过这些宏或该定义实现，这正是下表能够穷尽列举的原因。

> [!NOTE]
> *(自 2.0.0 起)* 当某个**成员**比最早支持的引擎还要新、且无人能说清它出现在哪个版本时，代码会去询问类型而非版本：在一个小模板中使用 `if constexpr (requires { … })`，并在已知存在该成员的引擎上用 `static_assert` 就同一问题做检查，这样拼写错误就不会被解读为“该引擎没有此成员”。这些位置列在[询问类型](#asked-of-the-type)一节下。

<a id="dreamshader_with_custom_pass"></a>

## `DREAMSHADER_WITH_CUSTOM_PASS`

*(自 2.1.0 起)* Custom Pass 开关不在本头文件中。`DreamShaderPass.Build.cs` 根据所针对构建的引擎计算它，并将其添加到模块的 `PublicDefinitions` 中：

```csharp
bool bForcedOff = Target.ProjectDefinitions.Any(Definition =>
    Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS" || Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS=1");
bool bWithCustomPass = !bForcedOff
    && (Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8));
PublicDefinitions.Add("DREAMSHADER_WITH_CUSTOM_PASS=" + (bWithCustomPass ? "1" : "0"));
```

| | |
| :-- | :-- |
| 定义者 | `DreamShaderPass.Build.cs` —— **不是** `DreamShaderVersionCompat.h`，后者既不定义也不提及它 |
| 值 | UE ≥ 5.8 时为 `1`，低于 5.8 或被强制关闭时为 `0`。在可见处始终有定义，因此用 `#if` 测试它，而不是 `#ifdef` |
| 可见于 | `DreamShaderPass` 及依赖它的模块：在插件中为 `DreamShaderCompiler` 和 `DreamShaderEditor`，它们询问它而不是再次测试版本。`DreamShaderLang` 和 `DreamShader` 不依赖 `DreamShaderPass`，因此没有它 |
| 可覆盖 | 只能向下覆盖，从 UBT 命令行：`-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS` 可在任意引擎上构建 `0`，5.8 机器正是借此编译和测试另一侧（见[测试](../contributing/testing.md#both-sides-of-the-custom-pass-gate)）。在 5.8 以下没有任何东西能使它变为 `1`，且代码无法抢先设置它：规则文件设置它时没有 `#ifndef` 保护 |
| 还决定 | `DreamShaderPass` 的 `Renderer/Internal` 包含路径（用于 `FPostProcessingInputs`），仅在 `1` 处添加 |

该模块可在所有引擎上构建。其中每个反射类型都是纯数据，因此管线、设置、组件和体积可在任何受支持的引擎上加载和保存；在 5.8 以下它们不渲染任何内容。`0` 按模块编译掉的内容见 [UE ≥ 5.8 —— `DREAMSHADER_WITH_CUSTOM_PASS`](#ue--58--dreamshader_with_custom_pass)。

<a id="complete-version-gated-behaviour"></a>

## 完整的版本门控行为

插件中由这些宏之一守护的每个位置，按其所要求的版本分组。
诊断信息以代码形式引用；下面仍然引用的字符串是日志行、材质编译错误
和 UI 文本，它们不携带代码。

<a id="decided-by-the-node-catalog"></a>

### 由节点目录决定

*(自 2.0.0 起)* 1.x 生成器中由引擎版本控制的绝大部分内容已不再属于 DreamShader 的
门控。`UE.*` 和 `Substrate.*` 节点、它们的引脚和属性、
其枚举属性的值以及源可写入的材质属性都来自 **内置目录**，
该目录是从正在运行的引擎反射而来的（它已加载的每个非抽象 `UMaterialExpression`
类）。引擎没有的内容，目录不会列出，绑定器会这样说明：

| 1.x 门控的内容 | 门控 | 现在 |
| :-- | :-- | :-- |
| 一个 `Substrate.*` 调用 | 5.4 | 在 UE ≥ 5.4 上，`Substrate` 命名空间保存着引擎的 Substrate 表达式类；在 5.3 上它是空的。目录中缺少的名称是 [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)；`Substrate.Select`（5.6）和 `Substrate.Toon`（5.8），以及需要引擎缺少的节点的 Substrate 语法糖，都是 [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294)，它会指出所需版本 |
| `Base.FrontMaterial` | 5.4 | 与任何其他材质属性一样；若引擎的属性列表中没有 `FrontMaterial`，则返回 [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200) |
| `UE.TransformPosition(PeriodicWorldTileSize=…)` | 5.5 | 引擎节点的引脚或属性；若该类没有此成员，则 [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) |
| `UE.TransformPosition(FirstPersonInterpolationAlpha=…)` | 5.6 | 同上，`DSH5213` |
| `periodicworld`、`firstperson` 和 `firstpersontranslatedworld` 基类 | 5.5 / 5.6 | 与节点 `Source` / `Destination` 属性背后引擎枚举的值进行匹配（1.x 中较为宽松，使用 [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278)）；不匹配任何值的拼写会得到 [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) |
| `UE.UserSceneTexture` | — | 在目录中缺少它的引擎上为 [`DSH5300`](../diagnostics/DSH5xxx.md#dsh5300)（绑定器从 5.6 起认识它） |
| `UE.DreamPassBuffer` / `UE.DreamPassOutput` | — | 在低于 5.8 的版本中有意从目录中省略，因此 `DSH5300`（参见 [UE ≥ 5.8](#ue--58--dreamshader_with_custom_pass)） |

用 `dsc export-catalog` 从较旧引擎导出的目录会给出相同的答案。
C++ 中剩余的门控就是下面这些。

<a id="ue--54--dreamshader_ue_version_at_least5-4"></a>

### UE ≥ 5.4 — `DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)`

| 功能 | 在 UE ≥ 5.4 上 | 在 UE 5.3 上 |
| :-- | :-- | :-- |
| `TArray` / `FString` 收缩参数 | `EAllowShrinking::No` | `false` |
| 生成的 Custom 节点 | 在 IR 发射器创建的每个 Custom 节点上设置 `UMaterialExpressionCustom::ShowCode = false`，因此 HLSL 正文在材质编辑器中保持折叠 | 该属性不存在；不设置，生成的 Custom 节点会显示其代码 |
| 重建前重置材质，以及生成资产的摘要 | `bHasPixelAnimation` 与其他材质标志一起被清除，并记录在摘要中 | 跳过 |
| 反编译器材质标志导出 | `bHasPixelAnimation` 被包含在发出的 `Settings` 标志列表中 | 排除 |

<a id="ue--54--dreamshader_with_substrate_builtins"></a>

### UE ≥ 5.4 — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`

Substrate 门控是一个单独的宏，因此可以独立覆盖，但其默认
阈值同样是 5.4。

| 功能 | 在 UE ≥ 5.4 上 | 在 UE 5.3 上 |
| :-- | :-- | :-- |
| 内置目录的 `Substrate.*` 命名空间 | 引擎的 `UMaterialExpressionSubstrateBSDF` 和 `…SubstrateUtilityBase` 子类列在 `Substrate.` 下 | 每个类都列在 `UE.` 下，因此 `Substrate.` 不指名任何内容：调用会是 [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) 或 `DSH5294`（参见 [由节点目录决定](#decided-by-the-node-catalog)） |
| `MaterialExpressionSubstrate.h` | 由内置目录的反射和三个图形反编译器文件包含 | 不包含 |
| `ShadingModel = "Substrate"`（或 `"Strata"`） | 接受 | 错误 [`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128) |
| 着色模型别名目录 | 尽管有 `Hidden` 元数据，仍保留 `MSM_Strata`，并添加 `Substrate` 和 `Strata` 别名 | 跳过 `MSM_Strata`，且不添加任何别名 |
| Substrate 值的材质连接类型（目录引脚类型，`IsSubstrateMaterialValueType`） | `MCT_Substrate` | `MCT_Strata` |
| 反编译器类型命名 | `MCT_Substrate` 打印为 `"Substrate"` | `MCT_Strata` 打印为 `"Substrate"` |
| 反编译器着色模型导出 | 对 `MSM_Strata` 发出 `"Substrate"` | 不发出 |
| 反编译器绑定表 | 包含 `MP_FrontMaterial` | 排除 |
| `substrate-builtins.json` 清单 | `supported: true` 和已填充的 `builtins` 数组 | `supported: false` 带 `unsupportedReason: "Substrate builtins require Unreal Engine 5.4 or newer."`，以及空的 `builtins` 数组 |
| 桥接清单，着色模型 | 提供 `MSM_Strata` | 将 `MSM_Strata` 添加到排除的着色模型集合 |

<a id="ue--55"></a>

### UE ≥ 5.5

| 功能 | 在 UE ≥ 5.5 上 | 在 UE 5.3 – 5.4 上 |
| :-- | :-- | :-- |
| 统计表达式的输入 | `Expression->CountInputs()` | `Expression->GetInputsView().Num()` |
| 读取一个输入引脚 | `Expression->GetInput(Index)` | `Expression->GetInput(Index)` — 相同。`GetInputsView()` 也可以使用，但它已弃用，改用 `FExpressionInputIterator` / `GetInput()`，并在 UE 5.8 上发出 C4996，因此绝不会仅仅为了访问单个引脚而构建数组视图 |
| 解析 `UMaterialExpressionObjectPositionWS`（反编译器） | 直接使用 `StaticClass()` 和 `IsA<>` | `FindObject<UClass>(nullptr, "/Script/Engine.MaterialExpressionObjectPositionWS")` |

`periodicworld` 基础和 `PeriodicWorldTileSize` 参数在此处不再受门控
*(自 2.0.0 起)*：参见 [由节点目录决定](#decided-by-the-node-catalog)。

<a id="ue--56"></a>

### UE ≥ 5.6

| 功能 | 在 UE ≥ 5.6 上 | 在 UE 5.3 – 5.5 上 |
| :-- | :-- | :-- |
| 包元数据访问 | `UPackage::GetMetaData()` 返回 `FMetaData&` | 返回 `UMetaData*`，使用前进行空值检查 |
| 表达式引脚值类型 | `GetInputValueType` / `GetOutputValueType` | `GetInputType` / `GetOutputType`，强制转换为 `EMaterialValueType` |
| 重建表达式的输出 | `Expression->RebuildOutputs()` | 手动 `Outputs.Reset()` 后进行重建 |
| 解析 `UMaterialExpressionScreenPosition`（反编译器） | 直接使用 `StaticClass()` 和 `IsA<>` | `FindObject<UClass>(nullptr, "/Script/Engine.MaterialExpressionScreenPosition")` |
| 从类默认对象读取引脚类型——内置目录的反射、工作区服务、图形导入器 | `GetInputValueType` / `GetOutputValueType` | 已弃用的 `GetInputType` / `GetOutputType`，包装在 `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 中 |
| `Shader(Root="Plugin.X")` | 额外要求 `Plugin->IsMounted()`；否则 [`DSH8094`](../diagnostics/DSH8xxx.md#dsh8094) | 跳过挂载检查 |
| `Path(Plugin.X, "…")` 资产根 | 相同的挂载检查；否则 [`DSH8122`](../diagnostics/DSH8xxx.md#dsh8122) | 跳过挂载检查 |
| `.dsi` 实例上的纹理集合参数 | 该种类位于父级的架构中，并将覆盖写入该实例 | 不在架构中，因此覆盖会因未知而被拒绝；手工构建的实例会失败并显示 [`DSH8252`](../diagnostics/DSH8xxx.md#dsh8252) |
| 反编译器 `TextureSample` 导出 | `GatherMode` 被往返转换 | 省略 |
| `DREAMSHADER_ENGINE_EXPRESSION_CLASS(Name)`（在 `DreamShaderMaterialExpressionCompat.h` 中） | `U<Name>::StaticClass()` | `FindObject<UClass>(nullptr, "/Script/Engine.<Name>")`。仍被定义，且自 2.0.0 起未被任何地方调用：`UE.*` 目录是通过遍历已加载的类构建的，不需要 `StaticClass()` |

`firstperson` 基础和 `FirstPersonInterpolationAlpha` 参数在此处不再受门控
*(自 2.0.0 起)*：参见 [由节点目录决定](#decided-by-the-node-catalog)。

> [!NOTE]
> `DREAMSHADER_ENGINE_EXPRESSION_CLASS` 是**链接**期门控，不是编译期门控。UE 5.6
> 更改了 UHT，使其在导出 `Z_Construct_<Class>_NoRegister` 的情况下生成 `DECLARE_CLASS2`，因此
> 即使对于那些既不携带 `MinimalAPI` 也不携带
> `ENGINE_API` 的 `UCLASS()`，`StaticClass()` 也能从插件中解析出来。UE 5.5 及更早版本生成 `DECLARE_CLASS(..., NO_API)`：`GetPrivateStaticClass` 永远不会
> 离开 `Engine.dll`，而命名 `StaticClass()` 可在所有引擎上编译，随后会以
> `LNK2019` 失败。只有完整的 [`RunUAT BuildPlugin`](../contributing/index.md#synopsis) 才能看到它——针对单个引擎的
> 编辑器构建永远看不到。

<a id="asked-of-the-type"></a>

### 对类型的询问

*（自 2.0.0 起）* 此处没有版本号：每一行由引擎自身类型是否有该成员决定——
或者，*（自 2.0.1 起）*，由引擎是否有该节点的头文件决定（`__has_include`）。

| 功能 | 成员存在的位置 | 成员不存在的位置 |
| :-- | :-- | :-- |
| 将 **Convert** 节点（Make / Break FloatN）读回为其各输出组合而成的通道——`Materials/MaterialExpressionConvert.h`，即 5.6 材质编辑器的节点 *（自 2.0.1 起）* | 以 swizzle 和构造函数形式写入 | 没有图能容纳它 |
| 将 **Switch** 节点读回为其分支——`Materials/MaterialExpressionSwitch.h` *（自 2.0.1 起）* | 以 `==` 分支链的形式写入，或作为未接线 `SwitchValue` 所选取的单个输入 | 没有图能容纳它 |
| 将保留的 **double vector** 覆盖重新应用到 ThinCustom 实例——`UMaterialInstanceConstant::SetDoubleVectorParameterValueEditorOnly`（到 5.6 为止缺失） | 设置 | 按名称报告为此构建无法表达的种类，如同源丢弃的参数 |
| 重新应用保留的 **static component mask** 覆盖——实例上的 `SetStaticComponentMaskParameterValueEditorOnly`（到 5.6 为止缺失） | 设置 | 以相同方式报告 |
| 读取实例的 usage-flags 覆盖——`FMaterialInstanceBasePropertyOverrides::bOverride_UsageFlags`（到 5.6 为止缺失） | 设置时由实例反编译器命名为不支持 | 没有此类覆盖可报告 |
| 当图导入器判断样本是否为普通样本时的 `TextureSample.GatherMode`（5.5 中缺失） | 与默认值比较 | 无可比较 |

<a id="dreamshader_with_parameter_collection_parameters"></a>

### `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS`

*（自 2.0.0 起）*

| 功能 | `EMaterialParameterType::ParameterCollection` 存在的位置 | 不存在的位置（5.6 及更早） |
| :-- | :-- | :-- |
| 参数集合参数的 `.dsi` 覆盖 | 针对父级 schema 进行绑定并写入实例 | 该种类不在父级 schema 中，因此该覆盖因未知而被拒绝；手动构建一个会返回 `this engine has no parameter collection parameters.` |
| 实例反编译器 | 写入该覆盖 | 永远不会遇到 |

<a id="ue--57"></a>

### UE ≥ 5.7

| 功能 | 在 UE ≥ 5.7 上 | 在 UE 5.3 – 5.6 上 |
| :-- | :-- | :-- |
| 桥接中的材质资源诊断 | 迭代每一个 `EShaderPlatform` × `EMaterialQualityLevel` 组合 | 一条不同的、更窄的路径 |
| `dsc check -Shaders`，每个质量等级的材质资源 | 取自目标着色器平台 | 取自 `GMaxRHIFeatureLevel` |
| 层混合函数输入 | `UMaterialExpressionFunctionInput::BlendInputRelevance` 从 IR 设置，图导入器将其读回 | 未设置，未读取 |
| 布局阶段中的节点预览高度 | `Expression->ShouldShowPreview()` | `!bHidePreviewWindow && !bCollapsed`，即 5.7 所组合出的两个标志 |
| 反编译器标量参数元数据 | `ControlType`、`Enumeration` 和 `EnumerationIndex` 已导出 | 未导出——这些属性不存在。携带它们的源仍可解析；但没有可写入它们的地方 |
| `FMaterialParameterInfo` 包含 | `Materials/MaterialParameters.h` | `MaterialTypes.h`，5.7 仅将其保留为弃用存根 |

<a id="ue--58"></a>

### UE ≥ 5.8

| 功能 | 在 UE ≥ 5.8 上 | 在 UE 5.3 – 5.7 上 |
| :-- | :-- | :-- |
| 内置目录的材质属性 *（自 2.0.0 起）* | `FMaterialAttributeDefinitionMap::GetAttributeNameToIDList` | 该列表是私有的，因此通过公共的 `GetID` / `GetProperty` / `GetAttributeName` 遍历 `EMaterialProperty` 来访问这些属性。两种方式得到相同的属性；只是它们在导出目录中的顺序不同 |
| 应用设置时写入的默认体积云用法 | `UMaterial::SetUsageByFlag(MATUSAGE_VolumetricCloud, …)` | 直接使用 `bUsedWithVolumetricCloud` 字段。值相同；每种拼写都是在对应引擎上编译且不产生弃用警告的那一种 |

<a id="ue--58--dreamshader_with_custom_pass"></a>

### UE ≥ 5.8 — `DREAMSHADER_WITH_CUSTOM_PASS`

*（自 2.1.0 起）* Custom Pass 整体。语言本身没有变化：`.dsp` 可在
所有引擎上解析和绑定，`.dss` 也是如此——变化的是引擎侧能够构建和运行的内容。

| 模块 | 功能 | 在 UE ≥ 5.8 上 | 在 UE 5.3 – 5.7 上 |
| :-- | :-- | :-- | :-- |
| `DreamShaderPass` | 模块启动 | 将 `/DreamPassUser` 映射到项目源代码根目录的 `.dreampass/` 文件夹，在缺失时创建 `Slots/` 和两个注册表 `.ush` 文件，并删除快照缺失的注册表节；在烘焙构建中跳过该映射 | 记录 `DreamShaderPass: Custom Pass needs Unreal Engine 5.8 or later; this build has the asset types only.` 并且不执行其他操作 |
| `DreamShaderPass` | 渲染器部分——`Private/Render/` 下的所有内容 | 已编译进来：场景视图扩展（每个世界一个，由 `UDreamPassSubsystem` 创建）、槽全局着色器 `FDreamPassCS` / `FDreamPassPS`、网格通道着色器、缓冲区、导出、缓冲区可视化以及 `stat DreamPass` 组 | 已编译排除。子系统仍保留管线、激活、列表和层，但不存在视图扩展，因此没有通道运行 |
| `DreamShaderPass` | `DreamPass.Dump` | 以最后一帧的报告结束每个世界：带有通道的视图、已运行的通道、已跳过的通道 | 同样的列表，但没有帧报告 |
| `DreamShaderPass` | `r.DreamPass.Enable`、`r.DreamPass.DisablePipelines`、`r.DreamPass.Visualize` | 已注册 | 已注册；由于没有任何渲染，它们没有什么可以更改的 |
| `DreamShaderPass` | `UMaterialExpressionDreamPassBuffer` / `…Output`（`UE.DreamPassBuffer`、`UE.DreamPassOutput`） | 按文档编译 | 类存在——UHT 无法对 `UCLASS` 进行条件编译——但 `Compile` 返回 `Dream Pass Buffer needs Unreal Engine 5.8 or later.` / `Dream Pass Output needs Unreal Engine 5.8 or later.`，并且它们的大部分重写——引脚值类型、引用的纹理、着色器标签、`IsAllowedIn`——都被编译排除 |
| `DreamShaderCompiler` | 内置目录 | 列出这两个节点 | 将两者都排除，因此调用其中一个的源代码在绑定时会失败，并带有 [`DSH5300`](../diagnostics/DSH5xxx.md#dsh5300)，其中指明 5.8 和 Custom Pass 模块 |
| `DreamShaderCompiler` | 构建 `.dsp` | 管线及其导出渲染目标被写入 | [`DSH8300`](../diagnostics/DSH8xxx.md#dsh8300)；不构建任何内容 |
| `DreamShaderCompiler` | 通道的材质会针对什么进行检查 | 域、可混合位置、`UserSceneTexture` 输入、`UE.DreamPassOutput` 引脚、使用标志、预曝光和翻译器设置 | 仅域和可混合位置（`FPipelineReferences::bCustomPassAvailable` 为 false），并在信息 [`DSH7360`](../diagnostics/DSH7xxx.md#dsh7360) 中说明其余部分未被检查；[构建键](../generation/caching.md#custom-pass-pipelines) 携带 `CustomPass=0` |
| `DreamShaderCompiler` | HLSL 槽预检查 | 为活动功能级别和目标平台的着色器格式编译每个更改的槽 | [`DSH8323`](../diagnostics/DSH8xxx.md#dsh8323) |
| `DreamShaderCompiler` | 在 `.dsp` 上的 `dsc check -Shaders` | 预检查其槽 | [`DSH8338`](../diagnostics/DSH8xxx.md#dsh8338) |
| `DreamShaderCompiler` | 注册表提交后槽着色器的热重载 | 在编辑器中，从不在 commandlet 中 | 无 |
| `DreamShaderEditor` | [管线详情面板](../../tools/editor-integration.md#pass-pipeline-details-panel) | 按文档 | 在管线概览中多一行：`Custom Pass runs on Unreal Engine 5.8 and later. This engine loads and saves the pipeline, and runs none of it.` |
| `DreamShaderEditor` | [`pass-keys.json`](../../tools/workspace.md#pass-keysjson) | `supported: true` | `supported: false` 和 `unsupportedReason: "Custom Pass pipelines run on Unreal Engine 5.8 and later."`；键、注入点和格式在两种情况下都会被写入 |
| `DreamShaderEditor` | 渲染自动化测试（`DreamShaderPassRenderTests.cpp`） | 已编译进来 | 已编译排除 |

<a id="since-ue-5x-summary"></a>

## “自 UE 5.x 起”摘要

| 版本 | 需要它的功能 |
| :-- | :-- |
| **5.4** | Substrate——`Substrate.` 目录命名空间、`ShadingModel="Substrate"`、`MSM_Strata` 别名、反编译器中的 `Base.FrontMaterial` · 折叠的 Custom 节点代码（`ShowCode`） · `bHasPixelAnimation` 重置、摘要和导出 · `EAllowShrinking` |
| **5.5** | 直接解析 `ObjectPositionWS` · `CountInputs` |
| **5.6** | 针对 `Root=` 和 `Path(...)` 的插件挂载验证 · 纹理集合实例参数 · `TextureSample.GatherMode` 往返 · 直接解析 `FMetaData&`、`GetInputValueType`、`RebuildOutputs`、`ScreenPosition` |
| **5.7** | 层混合输入上的 `BlendInputRelevance` · 按平台 × 按质量的材料资源诊断 · 标量参数 `ControlType` / `Enumeration` / `EnumerationIndex` 往返 · 来自 `ShouldShowPreview()` 的节点预览高度 |
| **5.8** | Custom Pass *（自 2.1.0 起）*——构建和运行 `.dsp` 管线、HLSL 槽、`UE.DreamPassBuffer` / `UE.DreamPassOutput` 节点（[`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass)） · 参数集合实例参数（向引擎的枚举询问） |

只有较新引擎才有的节点、引脚、枚举器和材质属性不在此表中：
运行中引擎的目录决定它们（[见上文](#decided-by-the-node-catalog)）。其他所有内容
在从 5.3 到 5.8 的每个引擎上表现一致。`.dsp` 是唯一一个在所有引擎上都解析和绑定
且仅在 5.8 上构建的源。

<a id="notes"></a>

## 注意事项

- **该门控是编译时的，而非运行时的。** 针对 UE 5.3 构建的插件二进制文件
  完全不包含 Substrate 代码路径。将项目迁移到更新的引擎需要重新构建
  插件以获得更新的行为。
- **`GetInputsView()` 仅在一个地方保留。** Epic 已弃用它，转而使用
  `FExpressionInputIterator` 和 `UMaterialExpression::GetInput()`，在 UE 5.8 上命名它会产生 C4996
  ——今天是警告，在移除它的任何版本中都是编译错误。剩余的调用是
  `GetDreamShaderExpressionInputCount` 的 UE 5.3 – 5.4 分支，其中 `CountInputs()` 尚不存在，
  弃用也未生效；其他所有位置都按索引请求引脚。
- *（自 2.0.0 起）* 引擎缺少的节点、引脚、枚举器或属性在其目录中缺失，
  因此使用它们的源在绑定时是错误；1.x 会静默丢弃其中一些（未读取的参数）。
  只有少数这些错误会指明版本
  ——[`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294)（来自较新引擎的 Substrate 节点）、
  [`DSH5300`](../diagnostics/DSH5xxx.md#dsh5300)（`UE.UserSceneTexture`，Custom Pass
  节点）、[`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128)（5.3 上的 `ShadingModel="Substrate"`）以及
  [`DSH8300`](../diagnostics/DSH8xxx.md#dsh8300)（低于 5.8 的 `.dsp`）；其余都说明该节点、
  引脚、值或属性不存在（`DSH5210`、`DSH5213`、`DSH5215`、`DSH5200`）。5.3 上的
  `substrate-builtins.json` 清单仍携带 `unsupportedReason`。要在多个引擎上构建一个源，
  请用 `#if DS_ENGINE_MINOR >= …` 保护较新的代码。
- `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` 作为值被求值，因此必须写为
  `#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS`，而不是 `#ifdef`。它始终被定义。
- `DREAMSHADER_UE_VERSION_AT_LEAST` 是无条件定义的，因此在构建目标中预定义它
  会产生宏重定义。请改为覆盖三个版本号宏。
- 因为 `DreamShaderModule.h` 包含此头文件，任何使用插件路径辅助函数的
  翻译单元也可以使用这些宏。

<a id="example"></a>

## 示例

保护使用受版本门控的 DreamShader 行为的项目代码：

```cpp
#include "DreamShaderVersionCompat.h"

void ConfigureShadingModel(FString& OutSettingValue)
{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
    OutSettingValue = TEXT("Substrate");
#else
    OutSettingValue = TEXT("DefaultLit");
#endif
}

void TrimTrailing(TArray<int32>& InOut)
{
    if (InOut.Num() > 0)
    {
        InOut.RemoveAt(InOut.Num() - 1, 1, DREAMSHADER_ALLOW_SHRINKING_NO);
    }
}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
// Plugin-rooted asset paths are mount-validated here; a packaged-but-unmounted plugin is an error.
#endif
```

相应的 DreamShaderLang 源代码，编写为在 5.3 上也能编译：

```c
Shader(Name="Materials/M_Portable")
{
    Settings { ShadingModel = "DefaultLit"; }   // "Substrate" would require UE 5.4+
    Outputs  { vec3 Color; Base.BaseColor = Color; }
    Graph    { Color = UE.TransformVector(Input = UE.VertexNormalWS(), Source = "World", Destination = "Tangent"); }
}
```

<a id="see-also"></a>

## 另请参阅

- [C++ API](index.md) — 模块、头文件和链接
- [`DreamShaderModule.h`](dreamshader-module.md) — 在各处包含此头文件的头文件
- [`DreamShaderSettings.h`](settings.md) — 着色模型目录中的 `Strata` 别名门控
- [`Substrate.*`](../builtins/substrate.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` 背后的内置系列
- [Transform builtins](../builtins/transform.md) — 版本门控的基础名称
- [`UE.*` 目录](../builtins/ue.md) — 反射目录提供的节点
- [材质枚举](../settings/material-enums.md) — `Substrate` 作为着色模型出现的位置
- [`Path(...)`](../parameters/path.md) — 插件根目录与 5.6 挂载检查
- [资产路径](../generation/asset-paths.md) — `Root="Plugin.X"` 与 5.6 挂载检查
- [反编译器](../tools/decompiler.md) — 版本门控的往返属性
- [`DreamShaderPass`](pass-module.md) — 其规则文件定义 `DREAMSHADER_WITH_CUSTOM_PASS` 的模块
- [Custom Pass 运行时](../runtime/index.md) — 在 5.8 上运行的内容
- [诊断索引](../diagnostics/index.md) — 所有代码，包括版本要求相关的代码
