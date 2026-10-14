---
title: "DreamShaderCompiler"
description: "DreamShaderCompiler"
---

<a id="dreamshadercompiler"></a>

# DreamShaderCompiler

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderCompiler**

编译器的后半部分：驱动源码从文本到资产的流水线、IR 发射器、其下的资产层、由反射构建的内置目录以及产品索引。前半部分——从词法分析器到经过验证的 IR——是 [`DreamShaderLang`](lang-module.md)，它对引擎一无所知；而本模块是首次触及 `UObject` 的地方。

| | |
| :-- | :-- |
| 模块 | `DreamShaderCompiler`（**Editor**，`Default` 加载阶段） |
| 公共头文件 | 20 个——调用方会遇到的列在[下方](#public-headers) |
| 命名空间 | `UE::DreamShader::Editor::Compiler` · `UE::DreamShader::Editor`（生成通知委托）· 全局作用域（模块类） |
| 导出宏 | `DREAMSHADERCOMPILER_API` |
| 公共依赖 | `Core`、`CoreUObject`、`DreamShader`、`DreamShaderLang`、`Engine` |
| 私有依赖 | `AssetRegistry`、`AssetTools`、`DreamShaderPass`、`Json`、`MaterialEditor`、`Projects`、`RenderCore`、`RHI`、`TargetPlatform`、`UnrealEd` |
| 反射 | 无 |

> [!IMPORTANT]
> **2.0 中的变更。** 在 1.9.x 及之前，这是一个 *Runtime* 模块，除了编译接口之外不含任何内容，编译器本身则私有地存在于 `DreamShaderEditor` 中。两者交换了位置：
> 接口**下移**到运行时模块中，成为
> [`DreamShaderCompilerInterface.h`](#the-interface-lives-in-dreamshader)，而编译器则
> **移出**编辑器模块，进入本模块。`DreamShaderCompilerInterfaces.h`、
> `FDreamShaderCompileService`、编辑器的 `FEditorCompileAdapter` 以及 1.x 的 `FMaterialGenerator`
> 都已移除。

<a id="the-interface-lives-in-dreamshader"></a>

## 接口位于 `DreamShader`

Editor 类型的模块不会出现在游戏目标中，因此在构建时编辑器之外的任何内容都不能依赖它。因此，请求、结果和接口都位于**运行时**模块中，放在一个除 `Core` 之外不需要任何东西的头文件里：

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

命名空间是 `UE::DreamShader`，而不是 `UE::DreamShader::Compiler`：在
`UE::DreamShader::Editor` 内部，裸的 `Compiler::` 指的是 `UE::DreamShader::Editor::Compiler`，因此此处的嵌套
命名空间会恰好对那些最常使用它的调用方隐藏。

<a id="fdreamshadercompilerequest"></a>

### `FDreamShaderCompileRequest`

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `""` | 一个 `.dss`、`.dsi`、`.dsp`、`.dsm` 或 `.dsf`；编译器会对其进行规范化，因此接受相对路径。`.dsh` 头文件不是编译单元。 |
| `bForce` | `bool` | `false` | 即使产品的标记构建键表明其资产是最新的，也进行重建。使用 `false` 时，此类产品将保持不变，调用会**成功**并输出一行 `Skipped …`。 |
| `ThinCustomPersistence` | `EThinCustomPersistence` | `Materialized` | 本次编译所触及的 ThinCustom 产品应最终处于哪种状态。**会被 Graph、函数和实例产品忽略**，它们始终保存。 |

> [!NOTE]
> `Ephemeral` 是 ThinCustom 产品在编辑器中的**正常**状态，而非特殊标志。
> 桥接器和预览渲染器都会请求它；命令行工具、cook 路径和显式的
> *Materialize* 操作采用 `Materialized` 默认值，因此未指定状态的调用方——一个
> 无头工具——会将资产写入磁盘。参见 [Ephemeral materials](../generation/ephemeral.md)。

<a id="fdreamshadercompileresult"></a>

### `FDreamShaderCompileResult`

| 成员 | 类型 | 含义 |
| :-- | :-- | :-- |
| `bSucceeded` | `bool` | 编译是否完成。被跳过的编译计为成功。 |
| `Message` | `FText` | 以桥接器、扩展和 `dsc.ps1` 所解析的线上格式呈现的报告——参见 [Result messages](#result-messages)。将其写入线路的调用方会通过 `ToInvariantWireString` 传递它。 |
| `Code` | `FString` | 首个错误的 `DSHnnnn` 代码；成功时为空。它与 `Message` 并列携带，因此以代码为键的调用方无需再从文本中解析它。 |

一次编译的结构化诊断——每个错误、警告和信息及其代码、阶段、
严重性和范围——不在结果中；请向
[`GetDreamShaderLastCompileDiagnostics`](#dreamshadercompilerserviceh) 索取。

<a id="idreamshadercompiler"></a>

### `IDreamShaderCompiler`

| 方法 | 契约 |
| :-- | :-- |
| `CompileAssets` | 源的所有产品：其材质以及每个导出的函数、层和混合——或者，对于 `.dsi`，是那个材质实例（先构建过期的父级）；对于 `.dsp` *（自 2.1.0 起）*，是带有其导出缓冲区的渲染目标和其 HLSL 槽位的流水线（先构建其通道所命名的任何过期材质）。 |
| `CompileMaterial` | 仅源的材质产品：预览的路径。 |

<a id="getdreamshadercompiler"></a>

### `GetDreamShaderCompiler()`

已注册的编译器，**或为 null**。在编辑器构建中，当尚未注册任何内容时，会按需加载编译器模块——两个模块都处于 `Default` 阶段，而同一阶段内的启动顺序并非契约。在游戏目标中（该模块不存在）以及引擎关闭期间为 null。**每个调用方都必须处理 null。** 每当它可能需要加载模块时，仅限游戏线程。

`RegisterDreamShaderCompiler` 供编译器模块自身使用：它的服务来自 `StartupModule`，
`nullptr` 来自 `ShutdownModule`。注册表不拥有该对象。它被导出，因此另一个
模块*可以*注册一个替代品——没有链式调用，也没有仲裁，最后一次调用胜出。

<a id="public-headers"></a>

## 公共头文件

| 头文件 | 声明内容 |
| :-- | :-- |
| `DreamShaderCompilerService.h` | `FDreamShaderCompilerService`——`IDreamShaderCompiler` 的唯一实现；`OnDreamShaderSourceGenerated`；`IsMemoryOnlyMaterial` / `MaterializeDreamShaderMaterial`；`GetDreamShaderLastCompileDiagnostics` |
| `DreamShaderCompilePipeline.h` | `IsDreamShaderLang2Source`、`CompileDreamShaderLang2File`、`RunDreamShaderLang2Pipeline`（含其选项与结果）、`ResolveDreamShaderSourceProducts`、`ResolveDreamShaderProductDestination`、`ResolveDreamShaderLegacyTextureTypes` |
| `DreamShaderIREmitter.h` | `FIREmitContext`、`EmitDreamShaderIRProduct`——一个已完成的 `IR::FIRProduct` 输入，一个已保存的资源输出；源跨度写入器与反编译提示写入器 |
| `DreamShaderBuiltinCatalog.h` | `GetDreamShaderBuiltinCatalog`、`InvalidateDreamShaderBuiltinCatalog`、`BuildBuiltinCatalogFromReflection`——绑定器与发射器共享的 `IR::FBuiltinCatalog` |
| `DreamShaderProductIndex.h` | `FDreamShaderProductIndex`——哪个源构建哪个资源，覆盖每个根目录的 `.dss`、`.dsi` 和 `.dsp` 文件；针对 `.dsi` 文件的 `ResolveInstanceParent`、`CollectInstanceDependents`、`FindInstanceParentSourceFile`；*（自 2.1.0 起）* `.dsp` 边：`CollectPipelineDependents`（在材质源之后需要重新构建的 `.dsp` 源）、`CollectPassBufferDependents`（读取某管线缓冲区的 `.dss` 源）、`FindPipelineMaterialSourceFiles` 与 `FindPassBufferPipelineSourceFiles`（依赖排序的两条边），以及索引上的 `FindPipelines`、`FindPipelinesUsingSource`、`FindPassBufferReadersOfSource`。`.dsp` 记录携带其 `MaterialReferences` 与 `ShaderFiles`，`.dss` 记录携带其 `PassPipelineReferences` |
| `DreamShaderPassPipelines.h` *（自 2.1.0 起）* | 编辑器工具所接触到的 Custom Pass 部分：HLSL 插槽注册表、插槽预检查、`.dsp` 编译所用的着色器文件——[见下文](#dreamshaderpasspipelinesh) |
| `DreamShaderInstanceSchema.h` · `DreamShaderInstanceSettings.h` | 父级资源的参数架构（`BuildParameterSchemaFromAsset`），以及 `#pragma instance` 键（`ApplyInstanceSettings`、`ReadInstanceSettings`、`GetInstanceSettingKeys`） |
| `DreamShaderCompilerDiagnostics.h` · `DreamShaderDiagnosticRecord.h` | 线路格式行（`FormatLang2DiagnosticWireLine`）、诊断 JSON，以及桥接器存储所记录的内容 |
| `DreamShaderCompilerIncludes.h` | `FDreamShaderIncludeResolver`——在源根目录与包之上的 `#include` 解析 |
| `DreamShaderGeneratedAssets.h` · `DreamShaderGeneratedAssetDigest.h` | 目标规则、源元数据、所有权保护、[分歧检测](../../generation/divergence.md) 背后的输出摘要 |
| `DreamShaderDependencyGraphService.h` · `DreamShaderSourceFileUtils.h` · `DreamShaderGraphDebugInfo.h` · `DreamShaderGenerationProgress.h` · `DreamShaderMaterialExpressionCompat.h` · `DreamShaderTextWireUtils.h` | 编辑器模块的工具与编译器共享的内容：谁包含谁、源枚举、节点 ↔ 源调试信息、进度启发式（着色器编译停滞阈值、Cancel 接缝）、表达式 API 的引擎版本兼容层、文化无关的线路字符串 |
| `DreamShaderCompilerModule.h` | `FDreamShaderCompilerModule`：在启动时注册服务，在关闭时注销服务 |

此处所有内容均为编辑器专用 API。它被导出，以便 `DreamShaderEditor`——命令小程序、桥接器、浏览器、反编译器、测试——可以链接它；第三方编辑器模块也可以。

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
| 服务 | 按扩展名选择前端，将请求的 ThinCustom 持久化转发到管线，并组织结果措辞。每条编译路径——桥接器的监视器、命令小程序、材质内容浏览器、溯源操作、测试——都终止于此。 |
| `OnDreamShaderSourceGenerated` | 每个源的**最外层**编译各触发一次，在其成功或失败之后，携带规范化路径。嵌套在另一编译中的编译（`.dsi` 构建其过期的父级）不会触发。 |
| `MaterializeDreamShaderMaterial` | 通过使用 `bForce` 与 `Materialized` 重新编译其源，然后按对象路径重新加载，来持久化仅内存的 ThinCustom 产品。 |
| `GetDreamShaderLastCompileDiagnostics` | 每个源最近一次的编译，以结构化记录形式呈现——代码、阶段、严重性、跨度长度——桥接器将其归档到诊断存储中，而不是重新解析结果的文本。 |

<a id="dreamshadercompilepipelineh"></a>

## `DreamShaderCompilePipeline.h`

管线流程为**预处理 → 解析 → 绑定 → 降低 → 遍 → 验证 → 发射**，而此头文件是
同一个驱动器，只是其中间产品被交还而非丢弃。

| 调用 | 用途 |
| :-- | :-- |
| `IsDreamShaderLang2Source(Path)` | 对于管线自行编译的每个文件返回 true：`.dss`、`.dsi` 和 `.dsp` 走 2.0 前端，`.dsm` 和 `.dsf` 走旧版前端。`.dsh` 返回 false。（此名称比这更古老：曾经只有 `.dss` 返回 true。） |
| `CompileDreamShaderLang2File(Path, bForce, OutError)` | 服务的 `CompileAssets`，携带 `Materialized` 请求，为希望将代码与文本分开的调用方提供 `FDreamShaderError` 形式。 |
| `RunDreamShaderLang2Pipeline(Path, Options, OutResult)` | 完整运行。`Options.bEmitAssets = false` 在 IR 验证后停止——即 `dsc check`。结果拥有解析模块、绑定模块和 IR，**以关键的成员顺序排列**（绑定模块指向解析模块内部）；它保留运行所到达的任何状态，因此语言服务即使从 false 返回也能获得 AST。*（自 2.1.0 起）*对于 `.dsp`，它还拥有宿主在绑定之前解析的 `PipelineReferences`——声明在 `Bound` 之前，后者可能指向它——并且对于 `.dss`，按原样记录其 `UE.DreamPassBuffer` 节点的 `PassPipelineReferences`。 |
| `ResolveDreamShaderSourceProducts(Path, OutResult)` | 一个源构建哪些资源，以及在哪个构建键之下，**而不构建它们**：前端、绑定器和 IR 构建器，然后是发射器自身的目录规则。不构建也不保存任何内容，也不打开进度对话框——材质内容浏览器为其列出的每个源调用它。它确实会读取源的跨源引用所指定的资源，在未加载时加载它们：`.dsi` 的父级（无源构建）以及 *（自 2.1.0 起）* `.dsp` 的遍所指定的材质，以及 `.dss` 读取其导出缓冲区的管线。`PassPipeline` 产品在 `ExportTargetObjectPaths` 中列出发射器为每个导出缓冲区在管线旁保留的渲染目标，按缓冲区顺序——生成的资源与管线一样多，`dsc list-generated` 正是如此命名它们。 |
| `ResolveDreamShaderProductDestination(Product, Path, …)` | 若 `Path` 声明了某产品，该产品将落在何处。反编译器在写入文件之前会询问此问题，以查明文本是否需要 `/// @name` 来将资源保留在原处。 |

<a id="dreamshaderpasspipelinesh"></a>

## `DreamShaderPassPipelines.h`

*（自 2.1.0 起）*编译器的 Custom Pass 部分，即编辑器工具所接触到的部分：HLSL 插槽注册表，
如 [`dsc pass-registry`](../../tools/commandlet.md#pass-registry) 所列、所收集并重建；插槽预检查，
如 `check -Shaders` 在 `.dsp` 上运行它；以及 `.dsp` 编译所用的着色器文件，
[桥接器](../../tools/bridge.md#custom-pass-shader-files) 会监视这些文件。此处不构建任何资源。仅限
游戏线程。一切都在 `UE::DreamShader::Editor::Compiler` 中。

| 声明 | |
| :-- | :-- |
| `EDreamPassSlotState` | `Live`、`Reserved`、`SnapshotMissing`、`PipelineGone`、`PassGone`、`Unknown` —— 一个槽位是什么（[清单](../../tools/commandlet.md#the-listing)） |
| `LexDreamPassSlotState(State)` | 该状态的名称。故意不使用 `LexToString` 重载：在此命名空间中声明的名称会遮蔽引擎的同名符号，使其在此命名空间中的任何非限定调用中都不可见 |
| `FDreamPassSlotReport` | 一个槽位，以 `Registry.json` 记录的形式以及被判定时的形式：`bCompute`、`Slot`、`Pipeline`（对象路径）、`Pass`、`Source`（`.dsp`，相对于项目）、`Shader`（相对于项目）、`Entry`、`Hash`、`Formats`（其快照通过预检查的着色器格式）、`State`，以及对于资源仍然存在的 `PipelineGone` 槽位的 `bPipelineAssetExists` |
| `FDreamPassRegistryReport` | `RegistryJsonPath`、`ComputeSlotCount`、`PixelSlotCount` 和 `Slots` —— 先计算槽位，再像素槽位，各自按槽位顺序 |
| `DescribeDreamPassRegistry(bClassify, OutReport, OutError)` | 读取 `Registry.json` 并判定每一个槽位。配合 `bClassify` 时，它会运行每个拥有槽位的 `.dsp` 的前端——不构建也不写入任何内容——以区分 `Live` 和 `PipelineGone`、`PassGone` 与 `Unknown`；不配合时，槽位为 `Live`、`Reserved` 或 `SnapshotMissing`。当 `Registry.json` 无法解析时返回 false，并带有 `OutError` |
| `CollectDreamPassRegistryGarbage(OutFreed, Diagnostics)` | `pass-registry -Gc`：释放每一个 `PipelineGone` 和 `PassGone` 槽位——其条目、其注册表节、其快照——删除没有任何东西引用的槽位目录，并在编辑器中重新编译槽位着色器。当 `Registry.json` 无法解析或文件无法写入时返回 false 并带有诊断信息 |
| `ResetUnreadableDreamPassRegistry(OutMovedTo, Diagnostics)` | `-Rebuild` 的第一步：将无法解析的 `Registry.json` 移到 `Registry.json.unreadable` 并写入一个空的注册表。当文件可解析时返回 true 且不触及任何内容；此时 `OutMovedTo` 为空 |
| `RewriteDreamPassRegistryFiles(OutReserved, Diagnostics)` | `-Rebuild` 的最后一步：从 `Registry.json` 写入 `RegistryCompute.ush` 和 `RegistryPixel.ush`，将快照文件缺失的槽位变为保留槽位，删除其未命名的槽位目录，并在编辑器中重新编译槽位着色器。`OutReserved` 统计它转变的槽位数量 |
| `CheckDreamShaderPipelineSlots(SourceFile, Formats, Diagnostics, OutPassesChecked)` | 对 `.dsp` 的 `check -Shaders`：将其运行到 IR，暂存每个管线，按注册表当前状态规划其槽位，并对每个 HLSL pass（无论是否更改）针对 `Formats` 进行预检查——为空：编译预检查的格式。不构建任何资源，也不写入任何内容。`OutPassesChecked` 统计 HLSL pass 的数量。在 UE 5.8 以下，`DSH8338` |
| `IsDreamShaderPassShaderFile(Path)` | `.usf` 或 `.ush`，不区分大小写 |
| `CollectDreamShaderPipelineShaderFiles(PipelineSourceFile, OutShaderFiles)` | 一个 `.dsp` 的 HLSL pass 编译所依赖的每一个文件：每个 `Shader = "..."` 文件、它通过相对路径包含的所有内容——包括此类包含所命名但尚不存在的文件——以及其快照保持活跃的用户虚拟包含背后的文件。绝对路径，已规范化，每个仅一次 |
| `FindDreamShaderPipelinesUsingShaderFile(ShaderFile, OutPipelineSourceFiles)` | 其某个 HLSL pass 从该文件编译的 `.dsp` 源 |
| `CollectDreamShaderPipelineShaderDirectories(OutDirectories)` | 包含某些 `.dsp` 编译所依赖文件的每个目录——桥接器监视的内容 |

注册表本身——`<DShader>/.dreampass/` 下的文件、槽位如何规划、预检查和
提交——是该模块私有的；其路径和槽位数量在运行时模块中公开
（[`DreamShaderPassModule.h`](../../api/pass-module.md#the-hlsl-slot-registry)）。

<a id="thread-and-context-requirements"></a>

## 线程与上下文要求

- **仅游戏线程，仅编辑器构建。** 输出部分会创建和修改 `UPackage`、`UMaterial` 和
  `UMaterialInstanceConstant`，并运行资源保存路径；绑定会读取内置目录，该目录
  由反射构建。
- 在命令行之外，编译会打开一个 `FScopedSlowTask`，并在短暂延迟后显示模态进度对话框；取消会报告为
  `bCancelled`，与失败不同。
- 这些调用是**同步的**。没有异步变体，没有 future，也没有除 `OnDreamShaderSourceGenerated` 之外
  的完成回调。
- *（自 2.1.0 起）* `.dsp` 编译也会写入其包之外：`<DShader>/.dreampass/` 下的 HLSL 槽位注册表及其
  快照。在编辑器渲染时，它随后会同步重新编译槽位着色器；命令行则不会。

<a id="result-messages"></a>

## 结果消息

`FDreamShaderCompileResult::Message` 有意保持 1.x 的传输格式：`.skill/dsc.ps1` 通过 grep 检索
`Generated` 行来列出一次运行写入的资源，桥接器会逐字记录该消息。

<a id="success"></a>

### 成功

每个产物一行，按输出顺序排列，并且在已构建管线的行之后，还会为每个导出缓冲区的渲染目标各输出一行：

| 行 | 条件 |
| :-- | :-- |
| `Generated {Kind} {ObjectPath} from {File}.` | 该产物已构建。`{Kind}` 为 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或*（自 2.1.0 起）* `PassPipeline` |
| `Generated RenderTarget {ObjectPath} from {File}.` | *（自 2.1.0 起）* 已构建管线的某个导出缓冲区的渲染目标，由该构建创建或复用并与它一起保存，因此 `dsc.ps1` 会将其计入本次运行写入的资源中 |
| `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` | `bForce == false` 且该资源标记的构建键仍然匹配 |
| `Skipped {ObjectPath}; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk.` | 同一项目上的第二个编辑器 |
| `Compiled {File}; it declares no material and no exported function, so no asset was written.` | 没有产物的源 |
| `\nWarnings:\n` + 每条警告一行传输格式行 | 当运行引发警告时追加：先是其自身的诊断，然后是 `RaiseGenerationWarning` 收集到的内容 |

<a id="failure"></a>

### 失败

第一个错误以 `<file>(<line>,<column>): DSHnnnn: <message>` 形式给出，然后每个其他诊断各占一行。
`Code` 保存该第一个 `DSHnnnn`。这些代码在
[诊断](../diagnostics/index.md) 下编目；发射器自身的代码为 `DSH8200`–`DSH8289`，而 Custom Pass
发射的代码——管线、其渲染目标、HLSL 槽位及其预检查——为 `DSH8300`–`DSH8339`
*（自 2.1.0 起）*。

`Skipped` 行背后的构建键涵盖该文件**以及每个
头文件**的预处理后文本、预处理器读取的宏定义，并且——对于 `.dsi`——还涵盖父级的对象路径以及由部分向量覆盖（而非父级的整个 schema）具体化的任何继承通道；对于 `.dsp`，
涵盖其 pass 所命名材质及其构建时使用的构建键、其着色器
文件的快照输入，以及 pass 层表；对于读取导出缓冲区的 `.dss`，涵盖该缓冲区的导出。参见
[缓存](../../generation/caching.md#custom-pass-pipelines)。

<a id="example"></a>

## 示例

请求一次编译而不链接编译器模块——这就是运行时或第三方
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

一个导出一个函数和一个材质的 `.dss` 的典型成功消息：

```text
Generated MaterialFunction /Game/FX/MF_Tint.MF_Tint from I:/Project/DShader/FX/Glow.dss.
Generated Material /Game/FX/M_Glow.M_Glow from I:/Project/DShader/FX/Glow.dss.
```

仅运行前半部分，如同 `dsc check` 所做的那样：

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

<a id="see-also"></a>

## 另见

- [C++ API](index.md) — 模块、头文件、链接和构建依赖
- [`DreamShaderLang`](lang-module.md) — 此模块运行并输出的前端和 IR
- [`DreamShaderModule.h`](dreamshader-module.md) — 用于构建请求的 `NormalizeSourceFilePath`
- [生成](../generation/index.md) — 输出的资产是什么样的
- [临时材质](../generation/ephemeral.md) — 两种状态在实践中的含义
- [缓存](../../generation/caching.md) — 构建键 `bForce` 所绕过的
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`，持久化调用方
- [编辑器桥接](../../tools/bridge.md) — 仅内存的调用方和 JSON 诊断
- [诊断索引](../diagnostics/index.md) — 按阶段划分的每条消息
- [`DreamShaderPass`](../../api/pass-module.md) — `.dsp` 所编译针对的运行时模块
