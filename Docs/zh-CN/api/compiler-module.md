---
title: "DreamShaderCompiler"
description: "DreamShaderCompiler"
---

<a id="dreamshadercompiler"></a>

# DreamShaderCompiler

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderCompiler**

编译器后半部分：将源从文本驱动到资产的管线、IR 发射器、其下的资产层、由反射构建的内置目录以及产品索引。前半部分——从词法分析器到经过验证的 IR——是 [`DreamShaderLang`](lang-module.md)，它对引擎一无所知；本模块是首次接触到 `UObject` 的地方。

| | |
| :-- | :-- |
| 模块 | `DreamShaderCompiler`（**编辑器**，`Default` 加载阶段） |
| 公共头文件 | 20 个——调用方会遇到的列在[下方](#public-headers) |
| 命名空间 | `UE::DreamShader::Editor::Compiler` · `UE::DreamShader::Editor`（生成通知委托）· 全局作用域（模块类） |
| 导出宏 | `DREAMSHADERCOMPILER_API` |
| 公共依赖 | `Core`、`CoreUObject`、`DreamShader`、`DreamShaderLang`、`Engine` |
| 私有依赖 | `AssetRegistry`、`AssetTools`、`DreamShaderPass`、`Json`、`MaterialEditor`、`Projects`、`RenderCore`、`RHI`、`TargetPlatform`、`UnrealEd` |
| 反射 | 无 |

> [!IMPORTANT]
> **2.0 中的变更。** 直到 1.9.x，这是一个 *Runtime* 模块，除编译接口外不含任何内容，而编译器本身私有地位于 `DreamShaderEditor` 中。两者交换了位置：接口**下移**到运行时模块中，成为 [`DreamShaderCompilerInterface.h`](#the-interface-lives-in-dreamshader)，而编译器**移出**编辑器模块，进入本模块。`DreamShaderCompilerInterfaces.h`、`FDreamShaderCompileService`、编辑器的 `FEditorCompileAdapter` 与 1.x 的 `FMaterialGenerator` 均已移除。

<a id="the-interface-lives-in-dreamshader"></a>

## 接口位于 `DreamShader`

Editor 类型的模块在游戏目标中不存在，因此编辑器之外的任何内容都不得在构建时依赖它。请求、结果与接口因此位于**运行时**模块中，其头文件除了 `Core` 外不需要任何内容：

```cpp
#include "DreamShaderCompilerInterface.h"   // module: DreamShader

namespace UE::DreamShader
{
    enum class EThinCustomPersistence : uint8 { Ephemeral, Materialized };

    struct DREAMSHADER_API FDreamShaderCompileRequest
    {
        FString SourceFilePath;
        bool bForce = false;
        EThinCustomPersistence ThinCustomPersistence = EThinCustomPersistence::Materialized;
    };

    struct DREAMSHADER_API FDreamShaderCompileResult
    {
        bool bSucceeded = false;
        FText Message;
        FString Code;
    };

    class DREAMSHADER_API IDreamShaderCompiler
    {
    public:
        virtual ~IDreamShaderCompiler() = default;
        virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
        virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
    };

    DREAMSHADER_API IDreamShaderCompiler* GetDreamShaderCompiler();
    DREAMSHADER_API void RegisterDreamShaderCompiler(IDreamShaderCompiler* Compiler);
}
```

命名空间是 `UE::DreamShader`，而非 `UE::DreamShader::Compiler`：在 `UE::DreamShader::Editor` 内，裸写 `Compiler::` 会指向 `UE::DreamShader::Editor::Compiler`，因此此处的嵌套命名空间会被最常使用它的调用方所隐藏。

<a id="fdreamshadercompilerequest"></a>

### `FDreamShaderCompileRequest`

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `""` | 一个 `.dss`、`.dsi`、`.dsp`、`.dsm` 或 `.dsf`；编译器会对其进行规范化，因此接受相对路径。`.dsh` 头文件不是编译单元。 |
| `bForce` | `bool` | `false` | 即使产品的标记构建键表明其资产是最新的，也重新构建。使用 `false` 时，此类产品保持不变，调用以 `Skipped …` 行**成功**返回。 |
| `ThinCustomPersistence` | `EThinCustomPersistence` | `Materialized` | 本次编译所涉及的 ThinCustom 产品应处于哪种状态。**被 Graph、函数与实例产品忽略**，这些产品始终保存。 |

> [!NOTE]
> `Ephemeral` 是编辑器中 ThinCustom 产品的**正常**状态，而非特殊标志。桥接与预览渲染器都会请求它；命令小程序、cook 路径以及显式的*材质化*操作采用 `Materialized` 默认值，因此未指定状态的调用方——无头工具——会将资产写入磁盘。参见[瞬态材质](../generation/ephemeral.md)。

<a id="fdreamshadercompileresult"></a>

### `FDreamShaderCompileResult`

| 成员 | 类型 | 含义 |
| :-- | :-- | :-- |
| `bSucceeded` | `bool` | 编译是否完成。被跳过的编译视为成功。 |
| `Message` | `FText` | 采用桥接、扩展与 `dsc.ps1` 所解析的传输格式的报告——参见[结果消息](#result-messages)。将其写入传输的调用方会通过 `ToInvariantWireString` 传递它。 |
| `Code` | `FString` | 第一个错误的 `DSHnnnn` 代码；成功时为空。与 `Message` 并列携带，以便根据代码进行键控的调用方无需再从文本中将其解析出来。 |

一次编译的结构化诊断——每个错误、警告与信息及其代码、阶段、严重级别与范围——不在结果中；请向 [`GetDreamShaderLastCompileDiagnostics`](#dreamshadercompilerserviceh) 请求它们。

<a id="idreamshadercompiler"></a>

### `IDreamShaderCompiler`

| 方法 | 约定 |
| :-- | :-- |
| `CompileAssets` | 该源的所有产品：其材质以及每个导出的函数、层与混合——或者，对于 `.dsi`，为单个材质实例（先构建过期的父项）；对于 `.dsp` *（自 2.1.0 起）*，为管线及其导出缓冲区的渲染目标与 HLSL 槽位（先构建其各阶段所引用的任何过期材质）。 |
| `CompileMaterial` | 仅该源的材质产品：预览的路径。 |

<a id="getdreamshadercompiler"></a>

### `GetDreamShaderCompiler()`

已注册的编译器，**或 null**。在编辑器构建中，当尚无任何内容注册时，编译器模块会按需加载——两个模块都处于 `Default` 阶段，而同一阶段内的启动顺序并非契约。在游戏目标中为 null，此时该模块不存在；在引擎关闭期间也为 null。**每个调用方都必须处理 null。** 只要可能需要加载该模块，就仅限游戏线程。

`RegisterDreamShaderCompiler` 供编译器模块自身使用：其服务来自 `StartupModule`，`nullptr` 来自 `ShutdownModule`。注册表不拥有该对象。它被导出，因此另一个模块*可以*注册替代者——没有链式调用，也没有仲裁，最后一次调用生效。

<a id="public-headers"></a>

## 公共头文件

| 头文件 | 声明内容 |
| :-- | :-- |
| `DreamShaderCompilerService.h` | `FDreamShaderCompilerService` —— `IDreamShaderCompiler` 的唯一实现；`OnDreamShaderSourceGenerated`；`IsMemoryOnlyMaterial` / `MaterializeDreamShaderMaterial`；`GetDreamShaderLastCompileDiagnostics` |
| `DreamShaderCompilePipeline.h` | `IsDreamShaderLang2Source`、`CompileDreamShaderLang2File`、`RunDreamShaderLang2Pipeline` 及其选项与结果、`ResolveDreamShaderSourceProducts`、`ResolveDreamShaderProductDestination`、`ResolveDreamShaderLegacyTextureTypes` |
| `DreamShaderIREmitter.h` | `FIREmitContext`、`EmitDreamShaderIRProduct` —— 输入一个完成的 `IR::FIRProduct`，输出一个已保存的资产；源区间与反编译提示写入器 |
| `DreamShaderBuiltinCatalog.h` | `GetDreamShaderBuiltinCatalog`、`InvalidateDreamShaderBuiltinCatalog`、`BuildBuiltinCatalogFromReflection` —— 绑定器与发射器共享的 `IR::FBuiltinCatalog` |
| `DreamShaderProductIndex.h` | `FDreamShaderProductIndex` —— 哪个源构建哪个资产，覆盖每个根目录的 `.dss`、`.dsi` 与 `.dsp` 文件；`.dsi` 文件的 `ResolveInstanceParent`、`CollectInstanceDependents`、`FindInstanceParentSourceFile`；*（自 2.1.0 起）* `.dsp` 边：`CollectPipelineDependents`（材质源之后需重建的 `.dsp` 源）、`CollectPassBufferDependents`（读取管线缓冲区的 `.dss` 源）、`FindPipelineMaterialSourceFiles` 与 `FindPassBufferPipelineSourceFiles`（依赖排序的两条边），以及索引上的 `FindPipelines`、`FindPipelinesUsingSource`、`FindPassBufferReadersOfSource`。`.dsp` 记录携带其 `MaterialReferences` 与 `ShaderFiles`，`.dss` 记录携带其 `PassPipelineReferences` |
| `DreamShaderPassPipelines.h` *（自 2.1.0 起）* | 编辑器工具所接触到的 Custom Pass 部分：HLSL 槽注册表、槽预检查、`.dsp` 编译所依据的着色器文件 —— [见下文](#dreamshaderpasspipelinesh) |
| `DreamShaderInstanceSchema.h` · `DreamShaderInstanceSettings.h` | 父资产的参数模式（`BuildParameterSchemaFromAsset`），以及 `#pragma instance` 键（`ApplyInstanceSettings`、`ReadInstanceSettings`、`GetInstanceSettingKeys`） |
| `DreamShaderCompilerDiagnostics.h` · `DreamShaderDiagnosticRecord.h` | 连线行（`FormatLang2DiagnosticWireLine`）、诊断 JSON，以及桥接存储所记录的文件记录 |
| `DreamShaderCompilerIncludes.h` | `FDreamShaderIncludeResolver` —— 在源根目录与包之上进行 `#include` 解析 |
| `DreamShaderGeneratedAssets.h` · `DreamShaderGeneratedAssetDigest.h` | 目标规则、源元数据、所有权防护、[差异检测](../generation/divergence.md)背后的输出摘要 |
| `DreamShaderDependencyGraphService.h` · `DreamShaderSourceFileUtils.h` · `DreamShaderGraphDebugInfo.h` · `DreamShaderGenerationProgress.h` · `DreamShaderMaterialExpressionCompat.h` · `DreamShaderTextWireUtils.h` | 编辑器模块工具与编译器共享的内容：谁包含谁、源枚举、节点 ↔ 源调试信息、进度启发式（着色器编译停滞阈值、Cancel 接缝）、表达式 API 的引擎版本垫片、文化无关的连线字符串 |
| `DreamShaderCompilerModule.h` | `FDreamShaderCompilerModule`：在启动时注册该服务，在关闭时注销它 |

此处的一切都是仅编辑器 API。它被导出，以便 `DreamShaderEditor` —— 命令行工具（commandlet）、桥接、浏览器、反编译器、测试 —— 可以链接它；第三方编辑器模块也可以。

<a id="dreamshadercompilerserviceh"></a>

## `DreamShaderCompilerService.h`

```cpp
namespace UE::DreamShader::Editor
{
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDreamShaderSourceGenerated, const FString& /*SourceFilePath*/, bool /*bSucceeded*/);
    DREAMSHADERCOMPILER_API FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated();
}

namespace UE::DreamShader::Editor::Compiler
{
    class DREAMSHADERCOMPILER_API FDreamShaderCompilerService final : public ::UE::DreamShader::IDreamShaderCompiler
    {
    public:
        static FDreamShaderCompilerService& Get();
        // CompileAssets / CompileMaterial
    };

    DREAMSHADERCOMPILER_API bool GetDreamShaderLastCompileDiagnostics(const FString& SourceFilePath, TArray<FLang2DiagnosticRecord>& OutRecords);
}
```

| | |
| :-- | :-- |
| 服务 | 按扩展名选择前端，将请求的 ThinCustom 持久化转发给管线，并为结果措辞。每条编译路径 —— 桥接的监视器、命令行工具、Material Content Browser、来源操作、测试 —— 都终止于此。 |
| `OnDreamShaderSourceGenerated` | 每次对源进行**最外层**编译时触发一次，无论成功或失败，并带有规范化路径。嵌套在另一次编译中的编译（一个 `.dsi` 构建其过期的父级）不会触发。 |
| `MaterializeDreamShaderMaterial` | 通过使用 `bForce` 与 `Materialized` 再次编译仅内存的 ThinCustom 产物，然后按对象路径重新加载它，从而持久化该产物。 |
| `GetDreamShaderLastCompileDiagnostics` | 每个源最近的编译，以结构化记录表示 —— 代码、阶段、严重性、区间长度 —— 桥接将其归入其诊断存储，而不是重新解析结果的文本。 |

<a id="dreamshadercompilepipelineh"></a>

## `DreamShaderCompilePipeline.h`

管线是 **preprocess → parse → bind → lower → passes → validate → emit**，而此头文件是同一个驱动程序，只是其中间产物被交还而非被丢弃。

| 调用 | 用途 |
| :-- | :-- |
| `IsDreamShaderLang2Source(Path)` | 对管线自行编译的每个文件返回 true：经由 2.0 前端的 `.dss`、`.dsi` 与 `.dsp`，经由旧版前端的 `.dsm` 与 `.dsf`。`.dsh` 返回 false。（该名称比这更古老：曾只有 `.dss` 返回 true。） |
| `CompileDreamShaderLang2File(Path, bForce, OutError)` | 服务的 `CompileAssets`，带有一个 `Materialized` 请求，针对希望将代码与文本分开的调用方，用 `FDreamShaderError` 拼写。 |
| `RunDreamShaderLang2Pipeline(Path, Options, OutResult)` | 完整运行。`Options.bEmitAssets = false` 在 IR 验证后停止 —— 那就是 `dsc check`。结果拥有解析后的模块、绑定后的模块与 IR，**以承重的成员顺序**（绑定后的模块指向解析后的模块）；它保留运行所达到的任何内容，因此语言服务能从 false 返回值中获得 AST。*（自 2.1.0 起）*对于 `.dsp`，它还拥有宿主在绑定时解析的 `PipelineReferences` —— 声明在 `Bound` 之前，后者可能指向它 —— 并且对于 `.dss`，如其书写的那样记录其 `UE.DreamPassBuffer` 节点的 `PassPipelineReferences`。 |
| `ResolveDreamShaderSourceProducts(Path, OutResult)` | 源构建哪些资产，以及在哪个构建键下，**而不构建它们**：前端、绑定器与 IR 构建器，然后是发射器自己的目标规则。不构建、不保存任何内容，也不打开进度对话框 —— Material Content Browser 对其列出的每个源调用它。它确实会读取源的跨源引用所命名的资产，在它们未加载时加载它们：没有源构建的 `.dsi` 的父级，以及 *（自 2.1.0 起）* `.dsp` 的通道所命名的材质，以及 `.dss` 读取其导出缓冲区的管线。`PassPipeline` 产物在 `ExportTargetObjectPaths` 中列出，发射器为每个导出缓冲区在管线旁保留的渲染目标，按缓冲区顺序 —— 与管线一样是生成的资产，这正是 `dsc list-generated` 命名它们的方式。 |
| `ResolveDreamShaderProductDestination(Product, Path, …)` | 如果一个产物由 `Path` 声明，它会落在哪里。反编译器在写文件之前询问这一点，以查明文本是否需要 `/// @name` 来将资产保持在原位。 |

<a id="dreamshaderpasspipelinesh"></a>

## `DreamShaderPassPipelines.h`

*（自 2.1.0 起）* 编译器在编辑器工具所接触到的 Custom Pass 部分：HLSL 槽注册表，如 [`dsc pass-registry`](../../tools/commandlet.md#pass-registry) 所列、收集并重建它；槽预检查，如 `check -Shaders` 在 `.dsp` 上运行它；以及 `.dsp` 编译所依据的着色器文件，[桥接](../../tools/bridge.md#custom-pass-shader-files)会监视它们。此处不构建任何资产。仅游戏线程。一切都在 `UE::DreamShader::Editor::Compiler` 中。

| 声明 | |
| :-- | :-- |
| `EDreamPassSlotState` | `Live`、`Reserved`、`SnapshotMissing`、`PipelineGone`、`PassGone`、`Unknown`——一个槽位是什么（[清单](../../tools/commandlet.md#the-listing)） |
| `LexDreamPassSlotState(State)` | 该状态的名称。故意不是 `LexToString` 的重载：在该命名空间中声明的重载会从其中每个非限定调用中隐藏引擎的重载 |
| `FDreamPassSlotReport` | 一个槽位，既包含 `Registry.json` 记录它的样子，也包含它被判定时的样子：`bCompute`、`Slot`、`Pipeline`（对象路径）、`Pass`、`Source`（`.dsp`，项目相对路径）、`Shader`（项目相对路径）、`Entry`、`Hash`、`Formats`（其快照通过预检查的着色器格式）、`State`，以及对于资产仍然存在的 `PipelineGone` 槽位，`bPipelineAssetExists` |
| `FDreamPassRegistryReport` | `RegistryJsonPath`、`ComputeSlotCount`、`PixelSlotCount` 和 `Slots`——先是计算槽位，然后是像素槽位，各自按槽位顺序 |
| `DescribeDreamPassRegistry(bClassify, OutReport, OutError)` | 读取 `Registry.json` 并判定每个槽位。使用 `bClassify` 时，它会运行拥有槽位的每个 `.dsp` 的前端——不构建或写入任何内容——以区分 `Live` 与 `PipelineGone`、`PassGone` 和 `Unknown`；不使用它时，槽位是 `Live`、`Reserved` 或 `SnapshotMissing`。当 `Registry.json` 无法解析时，为 false，并带有 `OutError` |
| `CollectDreamPassRegistryGarbage(OutFreed, Diagnostics)` | `pass-registry -Gc`：释放每个 `PipelineGone` 和 `PassGone` 槽位——其条目、其注册表节、其快照——删除没有任何内容命名的槽位目录，并在编辑器中重新编译槽位着色器。当 `Registry.json` 无法解析或无法写入文件时，为 false 并带有诊断信息 |
| `ResetUnreadableDreamPassRegistry(OutMovedTo, Diagnostics)` | `-Rebuild` 的第一步：将无法解析的 `Registry.json` 移到 `Registry.json.unreadable` 一旁，并写入一个空注册表。当文件可解析时，不触碰任何内容直接返回 true；此时 `OutMovedTo` 为空 |
| `RewriteDreamPassRegistryFiles(OutReserved, Diagnostics)` | `-Rebuild` 的最后一步：从 `Registry.json` 写入 `RegistryCompute.ush` 和 `RegistryPixel.ush`，将快照文件缺失的槽位变为保留槽位，删除它未命名的槽位目录，并在编辑器中重新编译槽位着色器。`OutReserved` 统计它转换的槽位数量 |
| `CheckDreamShaderPipelineSlots(SourceFile, Formats, Diagnostics, OutPassesChecked)` | 对 `.dsp` 执行 `check -Shaders`：将其运行到 IR，暂存每条管线，按当前注册表规划其槽位，并为 `Formats` 预检查每个 HLSL pass，无论是否变更——为空：编译预检查的格式。不构建资产也不写入任何内容。`OutPassesChecked` 统计 HLSL pass 的数量。在 UE 5.8 以下，`DSH8338` |
| `IsDreamShaderPassShaderFile(Path)` | `.usf` 或 `.ush`，不区分大小写 |
| `CollectDreamShaderPipelineShaderFiles(PipelineSourceFile, OutShaderFiles)` | 一个 `.dsp` 的 HLSL pass 编译所依赖的每个文件：每个 `Shader = "..."` 文件、它通过相对路径包含的所有内容——此类 include 命名的一个尚不存在的文件也算——以及其快照保持活跃的用户虚拟 include 背后的文件。绝对路径、已规范化、每个仅一次 |
| `FindDreamShaderPipelinesUsingShaderFile(ShaderFile, OutPipelineSourceFiles)` | 其中一个 HLSL pass 从该文件编译的 `.dsp` 源 |
| `CollectDreamShaderPipelineShaderDirectories(OutDirectories)` | 包含某个 `.dsp` 编译所依赖文件的每个目录——即桥接监视的内容 |

注册表本身——`<DShader>/.dreampass/` 下的文件、槽位如何被规划、预检查与
提交——是该模块私有的；其路径和槽位数量在运行时模块中是公开的
（[`DreamShaderPassModule.h`](pass-module.md#the-hlsl-slot-registry)）。

<a id="thread-and-context-requirements">
</a>

## 线程和上下文要求

- **仅限游戏线程、编辑器构建。** 输出部分创建并修改 `UPackage`、`UMaterial` 和
  `UMaterialInstanceConstant`，并运行资产保存路径；绑定读取内置目录，该目录
  由反射构建。
- 在 commandlet 之外，编译会打开一个 `FScopedSlowTask`，并在短延迟后显示模态进度对话框；Cancel 会报告为
  `bCancelled`，与失败不同。
- 这些调用是**同步的**。没有异步变体、没有 future，也没有除 `OnDreamShaderSourceGenerated` 之外的完成回调。
- *（自 2.1.0 起）* `.dsp` 编译也会写入其包之外：HLSL 槽位注册表及其
  快照位于 `<DShader>/.dreampass/` 下。在渲染它的编辑器中，它随后会同步重新编译槽位着色器；
  commandlet 不会。

<a id="result-messages">
</a>

## 结果消息

`FDreamShaderCompileResult::Message` 故意保持 1.x 的传输格式：`.skill/dsc.ps1` 会对
`Generated` 行进行 grep，以列出一次运行写入的资产，而桥接会逐字记录该消息。

<a id="success">
</a>

### 成功

每个产物一行，按输出顺序；对于已构建的管线，在其行之后，为其导出的每个渲染目标各一行：

| 行 | 条件 |
| :-- | :-- |
| `Generated {Kind} {ObjectPath} from {File}.` | 该产物已构建。`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 *（自 2.1.0 起）* `PassPipeline` |
| `Generated RenderTarget {ObjectPath} from {File}.` | *（自 2.1.0 起）* 已构建管线的某个导出缓冲区的渲染目标，由该构建创建或复用并保存在其旁边，因此 `dsc.ps1` 会将其计入该次运行写入的资产中 |
| `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` | `bForce == false` 且资产的已标记构建键仍然匹配 |
| `Skipped {ObjectPath}; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk.` | 同一项目上的第二个编辑器 |
| `Compiled {File}; it declares no material and no exported function, so no asset was written.` | 没有产物的源 |
| `\nWarnings:\n` + 每个警告一行线路行 | 当运行引发警告时追加：其自身诊断，然后是 `RaiseGenerationWarning` 收集到的内容 |

<a id="failure">
</a>

### 失败

第一个错误作为 `<file>(<line>,<column>): DSHnnnn: <message>`，然后每个其他诊断各占一行。
`Code` 保存该第一个 `DSHnnnn`。代码收录在
[诊断](../diagnostics/index.md) 下；发射器自身的代码为 `DSH8200`–`DSH8289`，而 Custom Pass
发射的代码——管线、其渲染目标、HLSL 槽位及其预检查——为 `DSH8300`–`DSH8339`
*（自 2.1.0 起）*。

`Skipped` 行背后的构建键涵盖文件的**以及每个头文件的**预处理文本、
预处理器读取的定义，以及——对于 `.dsi`——父级的对象路径和由部分向量覆盖
具体化的任何继承通道（不是父级的整个 schema）；对于 `.dsp`，
涵盖其 pass 命名的材质及其构建时使用的构建键、其着色器文件的快照输入，
以及 pass 层表；对于读取导出缓冲区的 `.dss`，涵盖该缓冲区的导出。参见
[缓存](../generation/caching.md#custom-pass-pipelines)。

<a id="example">
</a>

## 示例

在不链接编译器模块的情况下请求编译——这就是运行时或第三方
编辑器模块所需的全部内容：

```csharp
// MyTooling.Build.cs
PrivateDependencyModuleNames.AddRange(new[] { "Core", "DreamShader" });
```

```cpp
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderModule.h"

using namespace UE::DreamShader;

void CompileOne(const FString& InPath)
{
    IDreamShaderCompiler* Compiler = GetDreamShaderCompiler();
    if (!Compiler)
    {
        return; // a game target, or the engine is shutting down
    }

    FDreamShaderCompileRequest Request;
    Request.SourceFilePath = InPath;
    Request.ThinCustomPersistence = EThinCustomPersistence::Ephemeral;

    const FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
    UE_LOG(LogDreamShader, Display, TEXT("%s"), *Result.Message.ToString());
}
```

一个导出函数和一个材质的 `.dss` 的典型成功消息：

```text
Generated MaterialFunction /Game/FX/MF_Tint.MF_Tint from I:/Project/DShader/FX/Glow.dss.
Generated Material /Game/FX/M_Glow.M_Glow from I:/Project/DShader/FX/Glow.dss.
```

只运行前半部分，就像 `dsc check` 那样：

```cpp
#include "DreamShaderCompilePipeline.h"      // module: DreamShaderCompiler (editor only)
#include "DreamShaderCompilerDiagnostics.h"  // FormatLang2DiagnosticWireLine

using namespace UE::DreamShader::Editor::Compiler;

bool Check(const FString& InPath)
{
    FDreamShaderLang2PipelineOptions Options;
    Options.bEmitAssets = false;

    FDreamShaderLang2PipelineResult Run;
    const bool bOk = RunDreamShaderLang2Pipeline(InPath, Options, Run);
    for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Run.Diagnostics.GetDiagnostics())
    {
        UE_LOG(LogTemp, Display, TEXT("%s"), *FormatLang2DiagnosticWireLine(Diagnostic, Run.SourceFilePath));
    }
    return bOk;
}
```

<a id="see-also">
</a>

## 另见

- [C++ API](index.md) — 模块、头文件、链接和构建依赖
- [`DreamShaderLang`](lang-module.md) — 此模块运行并输出的前端与 IR
- [`DreamShaderModule.h`](dreamshader-module.md) — 用于构建请求的 `NormalizeSourceFilePath`
- [生成](../generation/index.md) — 输出的资产是什么样的
- [临时材质](../generation/ephemeral.md) — 这两种状态在实践中意味着什么
- [缓存](../generation/caching.md) — `bForce` 绕过的构建键
- [命令行工具](../tools/commandlet.md) — `-run=DreamShader`，即持久化调用方
- [编辑器桥接](../../tools/bridge.md) — 仅内存调用方和 JSON 诊断
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每条消息
- [`DreamShaderPass`](pass-module.md) — `.dsp` 所编译目标的运行时模块
