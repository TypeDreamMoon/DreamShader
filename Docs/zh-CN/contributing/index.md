---
title: "贡献"
description: "贡献"
---

<a id="contributing"></a>

# 贡献

> [DreamShader](../index.md) » **贡献**

构建插件本身、其五个 C++ 模块的布局，以及各子系统的代码所在位置。

| | |
| :-- | :-- |
| 仓库 | <https://github.com/TypeDreamMoon/DreamShader> |
| 类型 | 贡献者参考 |
| 插件版本 | `2.2.0` — 描述符 `Version` `220`，`IsBetaVersion` `false` |
| 引擎 | Unreal Engine `5.3` – `5.8`，已在 Win64 验证 |
| 许可证 | MIT |

<a id="synopsis"></a>

## 概要

独立验证插件，无需构建完整的项目目标：

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<PluginDir>\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

`<PluginDir>` 是存放 `DreamShader.uplugin` 的目录。`-Package` 必须指向一个不与源码树重叠的目录——UAT 会在那里暂存一份干净的副本。

仅一个引擎无法证明其他引擎的情况，因此请使用
[`.skill/build-plugin.ps1`](#the-engine-matrix) 运行整个矩阵。

<a id="repository-layout"></a>

## 仓库布局

| 路径 | 内容 | 在[发布归档](release.md#archive-contents)中 |
| :-- | :-- | :-- |
| `Source/` | 五个 C++ 模块 | 是 |
| `Docs/` | 本手册 | 是 |
| `.skill/` | 智能体技能集、其 `dsc.ps1` 驱动，以及 [`build-plugin.ps1`](#the-engine-matrix) | 是 *(自 1.5.1 起)* |
| `Resources/` | `Icon128.png`，插件图标 | 是 |
| `DreamShader.uplugin` | 插件描述符 | 是 |
| `README.md` | 英文自述文件 | 是 |
| `README.zh-CN.md` | 中文自述文件 | 是 *(自 1.5.1 起)* |
| `CHANGELOG.md` | 版本历史；发布工作流会读取其 `## <VersionName>` 节 | 是 |
| `LICENSE` | MIT | 是 |
| `Content/Localization/DreamShader/` | `.locmeta` 加上每种文化一个 `.locres`；通过描述符中的 `LocalizationTargets` 条目加载 | 是 *(自 2.1.2 起)* |
| `Shaders/` | `DreamShaderBuiltins.ush` 以及 `Pass/` 下的 Custom Pass 着色器，挂载于 `/Plugin/DreamShader` | 是 *(自 1.5.1 起)* |
| `Tests/Corpus/` | 数据驱动的测试夹具和 `.expected.json` 黄金基准 | **否** |
| `Tools/Localization/` | `localization_lint.ps1` 及其生成的收集基线 | **否** |
| `Config/` | `FilterPlugin.ini` —— 自带的注释模板；它未声明任何额外的打包文件 | **否** |
| `Images/` | 自述文件插图 | **否** |
| `.github/workflows/release.yml` | 仓库中唯一的工作流 | **否** |
| `Binaries/`、`Intermediate/` | 构建输出；已被 git 忽略 | **否** |

<a id="modules"></a>

## 模块

| 模块 | 类型 | 加载阶段 | 公共头文件 | 导出宏 |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | 运行时 | PostConfigInit | 31 | `DREAMSHADERLANG_API` |
| `DreamShader` | 运行时 | PostConfigInit | 8 | `DREAMSHADER_API` |
| `DreamShaderPass` *(自 2.1.0 起)* | 运行时 | PostConfigInit | 11 | `DREAMSHADERPASS_API` |
| `DreamShaderCompiler` | 编辑器 | Default | 20 | `DREAMSHADERCOMPILER_API` |
| `DreamShaderEditor` | 编辑器 | Default | **0** —— 不存在 `Public/` 文件夹 | `DREAMSHADEREDITOR_API` 由 UBT 定义，从未在源码中使用 |

该顺序即描述符中的顺序。`DreamShaderPass` 在 `PostConfigInit` 加载，因为它映射了一个着色器源目录，且其全局着色器类型会在其 DLL 加载时注册，这两者都必须在引擎确定着色器类型列表之前发生。

描述符声明了两个已启用的插件依赖：`WebSocketNetworking`（预览 WebSocket 服务器）和 `SQLiteCore`（桥接数据库）。两者都是引擎插件。

<a id="build-rules"></a>

### 构建规则

所有五个模块均设置 `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs`。其中三个模块添加了定义，每个都来自对所针对构建的引擎的探测：

| 模块 | 定义 |
| :-- | :-- |
| `DreamShader` | `PublicDefinitions`：`DREAMSHADER_WITH_MOON_ENGINE` 和 `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS`，`0` 或 `1` 来自头文件探测——见[版本兼容性](../api/version-compat.md#macros) |
| `DreamShaderPass` | `PublicDefinitions`：`DREAMSHADER_WITH_CUSTOM_PASS`，从 UE 5.8 起为 `1`——见 [`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass)。在 `1` 它还会将渲染器的 `Internal` 文件夹添加到其私有包含路径 |
| `DreamShaderEditor` | `PrivateDefinitions`：`MOON_ENGINE=1`，仅当引擎的 `SceneTypes.h` 声明了 `MP_MoonEncodedAttribute0` 时；没有源文件读取它 |

| 模块 | 依赖列表 |
| :-- | :-- |
| `DreamShaderLang` | **公共 (1)**：`Core`。无私有依赖。 |
| `DreamShader` | **公共 (7)**：`Core`、`CoreUObject`、`DeveloperSettings`、`DreamShaderLang`、`Engine`、`Projects`、`RenderCore`。无私有依赖。 |
| `DreamShaderPass` | **公共 (7)**：`Core`、`CoreUObject`、`DeveloperSettings`、`DreamShader`、`Engine`、`RenderCore`、`RHI`。**私有 (2)**：`Projects`、`Renderer`。 |
| `DreamShaderCompiler` | **公共 (5)**：`Core`、`CoreUObject`、`DreamShader`、`DreamShaderLang`、`Engine`。**私有 (10)**：`AssetRegistry`、`AssetTools`、`DreamShaderPass`、`Json`、`MaterialEditor`、`Projects`、`RenderCore`、`RHI`、`TargetPlatform`、`UnrealEd`。 |
| `DreamShaderEditor` | **私有 (31)**：`ApplicationCore`、`AssetRegistry`、`AssetTools`、`ContentBrowser`、`Core`、`CoreUObject`、`DesktopPlatform`、`DirectoryWatcher`、`DreamShader`、`DreamShaderCompiler`、`DreamShaderLang`、`DreamShaderPass`、`Engine`、`InputCore`、`Json`、`MaterialEditor`、`Projects`、`PropertyEditor`、`RHI`、`RenderCore`、`Renderer`、`Settings`、`Slate`、`SlateCore`、`SQLiteCore`、`TargetPlatform`、`ToolMenus`、`ToolWidgets`、`UnrealEd`、`WebSocketNetworking`、`WorkspaceMenuStructure`。无公共依赖。 |

`DreamShaderPass` 是 `DreamShaderCompiler` 和 `DreamShaderEditor` 的**私有**依赖。两者都读取其公共定义 `DREAMSHADER_WITH_CUSTOM_PASS`，而不是再次测试引擎版本。`DreamShaderPass` 本身依赖 `DreamShader`——因为项目源码根目录所在的 `.dreampass/` 文件夹——而不依赖任何编辑器模块。

`DreamShader` 是 `DreamShaderCompiler` 的**公共**依赖，因此任何链接 `DreamShaderCompiler` 的目标都会传递性地获得运行时头文件。

运行时模块为何需要其各项依赖：`DeveloperSettings` 用于 `UDeveloperSettings`；`Engine` 用于 `EngineTypes.h`、`MaterialDomain.h` 和 `UMaterialInstanceConstant`；`RenderCore` 用于 `AllShaderSourceDirectoryMappings` / `AddShaderSourceDirectoryMapping`；`Projects` 用于 `IPluginManager`。

<a id="source-tree"></a>

## 源码树

<a id="sourcedreamshader--runtime"></a>

### `Source/DreamShader` — 运行时

| 路径 | 职责 |
| :-- | :-- |
| `Public/` | 八个导出的头文件。文档位于 [C++ API](../api/index.md)。 |
| `Private/DreamShaderModule.cpp` | 模块引导：创建源码、包和生成着色器目录，注册 `/DreamShaderGenerated` 和 `/Plugin/DreamShader` 着色器映射，并实现源根、目录获取器、`SanitizeIdentifier`、`NormalizeSourceFilePath` 以及文件分类谓词。 |
| `Private/DreamShaderSettings.cpp` | `UDreamShaderSettings`：构造函数默认值、`NormalizeMappingKey`、三个 `TryResolve*` 方法、三个 `BuildDefault*Mappings` 别名目录，以及定义修订和 2.0 之前的配置钩子。 |
| `Private/DreamShaderTypes.cpp` | `NormalizeSettingKey`、`FTextShaderDefinition::TryGetSetting` / `GetSetting`。 |
| `Private/DreamShaderMaterialInstance.cpp` | `UDreamShaderMaterialInstance::HasOverridenBaseProperties` 和 `IsAsset`。 |
| `Private/DreamShaderDefineResolution.cpp` | 预处理器定义表中面向引擎的那一半：内置 `DS_` 定义、项目设置、C++ 提供程序以及命令行合并到一张表中。 |
| `Private/DreamShaderCompilerInterface.cpp` | `GetDreamShaderCompiler()` 背后的注册表，编译模块在启动时填充它。 |

*(自 2.0.0 起)* 位于 `Private/Parser/` 的 1.x 解析器已被移除。`.dsm` / `.dsf` / `.dsh` 由 `DreamShaderLang` 的旧版前端读取 — 参见 [`DreamShaderLang`](../api/lang-module.md)。

<a id="sourcedreamshaderpass--runtime"></a>

### `Source/DreamShaderPass` — 运行时

*(自 2.1.0 起)* Custom Pass 运行时：`.dsp` 编译成的资产、决定哪些管线应用于某个视图的子系统，以及运行其通道的场景视图扩展。该模块在所有引擎上构建；需要渲染器的部分仅在
[`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass) 为 `1` 的地方编译。参见
[`DreamShaderPass`](../api/pass-module.md) 和 [Custom Pass 运行时](../runtime/index.md)。

| 路径 | 职责 |
| :-- | :-- |
| `DreamShaderPass.Build.cs` | UE 5.8 门控：`DREAMSHADER_WITH_CUSTOM_PASS`，以及仅 5.8 构建所需的 Renderer `Internal` 包含路径。 |
| `Public/` | 十一个导出的头文件：模块（`LogDreamPass` 以及用户着色器目录的 `UE::DreamPass` 路径、槽注册表文件和槽文件夹）、`DreamPassTypes.h`、`DreamPassPipeline.h`（`UDreamPassPipeline`、`FOnDreamPassPipelineChanged`）、`DreamPassSettings.h`、`DreamPassSubsystem.h`、`DreamPassVolume.h`、`DreamPassComponent.h`、`DreamPassLayerComponent.h`、`DreamPassBlueprintLibrary.h`，以及在 `Materials/` 下的两个材质表达式。 |
| `Private/DreamShaderPassModule.cpp` | 5.8 下的模块启动：将 `/DreamPassUser` 着色器目录映射到项目源根的 `.dreampass/`、槽着色器包含的空注册表文件，以及移除快照缺失的注册表节。5.8 以下，一行日志。 |
| `Private/DreamPassPipeline.cpp`、`DreamPassTypes.cpp`、`DreamPassSettings.cpp` | 资产及其 `Validate`、共享枚举及其拼写、项目设置。 |
| `Private/DreamPassSubsystem.cpp`、`DreamPassVolume.cpp`、`DreamPassComponent.cpp`、`DreamPassLayerComponent.cpp`、`DreamPassBlueprintLibrary.cpp` | 激活：使管线应用于某个视图的因素、通道层与列表，以及蓝图接口。 |
| `Private/DreamPassConsole.{h,cpp}` | `r.DreamPass.Enable`、`r.DreamPass.DisablePipelines`、`r.DreamPass.Visualize` 以及 `DreamPass.Dump` 命令。 |
| `Private/Materials/` | `UMaterialExpressionDreamPassOutput` 和 `UMaterialExpressionDreamPassBuffer` — `UE.DreamPassOutput` 和 `UE.DreamPassBuffer`。 |
| `Private/Render/` | 渲染器的一半，仅在 5.8 编译：场景视图扩展及其调度器、逐帧快照、缓冲区、全屏、计算、网格、清除和复制通道、槽全局着色器 `FDreamPassCS` / `FDreamPassPS`、导出、缓冲区可视化，以及 `DreamPassRendererInternal.cpp` — 唯一从渲染器的 `Internal` 文件夹包含的文件。 |

这些通道编译的着色器位于插件的 `Shaders/Pass/` 中，挂载在
`/Plugin/DreamShader`。

<a id="sourcedreamshadercompiler--editor"></a>

### `Source/DreamShaderCompiler` — 编辑器

| 路径 | 职责 |
| :-- | :-- |
| `Public/` | 二十个导出的头文件——编译器服务（`DreamShaderCompilerService.h`，即 `IDreamShaderCompiler` 的唯一实现，其接口位于 `DreamShader`）、编译管线、IR 发射器、内置目录与资产层。参见[编译器模块](../api/compiler-module.md#public-headers)。 |
| `Private/DreamShaderCompilerService.cpp`、`DreamShaderCompilerModule.cpp` | 该服务——所有编译路径都终止于此——以及注册它的模块。 |
| `Private/Pipeline/` | 编译管线：预处理、按扩展名选择前端、绑定、降低、验证、发射；构建键；`#include` 解析；1.x 纹理默认类检查（`DSH1043` / `DSH1044`）；以其线格式表示诊断。 |
| `Private/Emitter/` | IR 发射器——IR 到 `UMaterial` / `UMaterialFunction` / 实例节点——以及从运行中的引擎反射得到的内置目录。 |
| `Private/Assets/` | 包与资产创建、带插件挂载检查的 `Root=` 与 `Path(...)` 解析、源元数据与输出摘要、失败重建的回滚、实例 schema 与 ThinCustom 参数覆盖。 |
| `Private/Reflection/` | 应用材质设置、解析设置值、写入反射的字面量属性，以及 `material-expressions.json` 背后的反射类接口。 |
| `Private/Layout/` | 图表布局：应用于已构建图表的 IR 样式，以及 `Classic` 1.x 布局。 |
| `Private/Sources/` | 源发现、源与头文件之间的依赖图，以及产物索引。 |
| `Private/Preview/` | 断点预览读取的（行、名称）探测表。 |
| `Public/DreamShaderPassPipelines.h` *(自 2.1.0 起)* | 从编译外部访问的 Custom Pass 槽注册表：列表及其槽状态、垃圾回收、不可读的 `Registry.json` 重置、注册表文件重写、`dsc check -Shaders` 的槽预检查，以及桥接用于监视的 `.usf` ↔ `.dsp` 查找。参见 [`DreamShaderPassPipelines.h`](../api/compiler-module.md#dreamshaderpasspipelinesh)。 |
| `Private/Pass/` *(自 2.1.0 起)* | HLSL 槽。`DreamShaderPassSlotRegistry` 规划槽、在进程内预检查已更改的槽、提交快照、`Registry.json` 和注册表 `.ush` 文件，并热重载槽着色器；`DreamShaderPassShaderText` 将 `.usf` 作为文本读取——`Shader = "..."` 引用的落点、文件定义了什么、它通过相对路径包含了哪些文件；`DreamShaderPassPipelines.cpp` 实现公共头文件。 |
| `Private/Pipeline/DreamShaderPipelineReferences.{h,cpp}` *(自 2.1.0 起)* | 在 `.dsp` 与其周围的 `.dss` 源之间跨越的两个阶段：在绑定之前，解析并读取 `.dsp` 的 `Material` 和 `Shader` 引用以获取绑定器检查的事实；在降低之后，将 `.dss` 的 `UE.DreamPassBuffer` 解析到其管线和缓冲区。 |
| `Private/Emitter/DreamShaderIREmitterPassPipeline.{h,cpp}` *(自 2.1.0 起)* | `PassPipeline` 产物：`UDreamPassPipeline`、其导出缓冲区的渲染目标及其 HLSL 槽 — 全部先暂存，以便失败时不写入任何内容。 |

编译器本身 — 编译管线、IR 发射器和资产层 — 在 2.0 中移入此模块，替换了 1.x 生成器（`DreamShaderEditor/Private/MaterialAssetGeneration/`）；参见[编译模块](../api/compiler-module.md)。

<a id="sourcedreamshaderlang--runtime-core-only"></a>

### `Source/DreamShaderLang` — 运行时，仅 `Core`

| 路径 | 职责 |
| :-- | :-- |
| `Private/Preprocessor/` | `#if` / `#define` 和 define 表 |
| `Private/Lang/` | 词法分析器、`.dss` / `.dsi` / `.dsp` 解析器、读取 `.dsm` / `.dsf` / `.dsh` 到同一棵树的**旧版前端**（`LangLegacy*.cpp`）、以及 `fmt` 背后的打印器 |
| `Private/Semantic/` | 绑定器（`LangBinder*.cpp`）、包含 1.x 旧版规则 |
| `Private/IR/` | IR 降低、验证器、自定义 HLSL 代码、IR 布局、转储和比较 |
| `Private/Decompile/`、`Private/Migrate/` | IR 转回文本，以及 `dsc migrate` |

参见 [`DreamShaderLang`](../api/lang-module.md) 和 [2.0 语言](../language-v2/index.md)。

<a id="sourcedreamshadereditor--editor"></a>

### `Source/DreamShaderEditor` — 编辑器

所有内容都在 `Private/` 下；没有导出任何内容。

| 路径 | 职责 |
| :-- | :-- |
| `DreamShaderEditorModule.cpp` | 模块入口。在 `IsRunningCommandlet()`、cook-director 检查和 `-NoDreamShaderEditorBridge` 上控制桥接；拥有 cook 时实现阶段。 |
| `DreamShaderEditorPersistenceUtils.h` | `BindAndExecute` — 两个 SQLite 写入器共享的预处理语句辅助工具。 |
| `Bridge/` | [编辑器桥接](../tools/bridge.md)：请求文件轮询、目录监视器、去抖和编译队列、材质编译诊断，以及预览 WebSocket 服务器。 |
| `Commandlet/` | `UDreamShaderCommandlet` 以及 `compile` / `decompile` / `dump-graph` 运行器和共享参数辅助工具；`DreamShaderMigrate.{h,cpp}`，即 `migrate` 动词 *(2.0)*。参见 [Commandlet](../tools/commandlet.md)。*(自 2.1.0 起)* `DreamShaderPassRegistryCommandlet.{h,cpp}`：[`pass-registry`](../tools/commandlet.md#pass-registry) 动词 — 列表、`-Gc` 和 `-Rebuild`。 |
| `Tools/` *(2.0)* | 其他 2.0 动词（`check`、`dump-ir`、`dump-layout`、`index`、`export-catalog`、`fmt`、`list-generated`）、`check -Shaders` 的 HLSL 着色器检查、反编译请求辅助工具和目录清单。 |
| `Decompiler/` | 2.0 反编译器（`DreamShaderIRDecompiler`、`DreamShaderGraphImport`、`DreamShaderInstanceDecompiler`）：图 → IR → `.dss` / `.dsi`；保留在 `-Format=Legacy` 之后的 1.x `.dsm` / `.dsf` 导出（`DreamShaderGraphDecompiler*`）；反编译服务。参见[反编译器](../tools/decompiler.md)。*(自 2.1.0 起)* `DreamShaderPipelineDecompiler.{h,cpp}`：将 `UDreamPassPipeline` 读回 `.dsp` 所绑定的负载中——对于 `dsc decompile`，它打印该负载并通过再次解析和绑定来检查文本；对于 Adopt，它将负载拼接到现有 `.dsp` 中。 |
| `Diagnostics/` | `FDreamShaderDiagnosticsStore`：错误位置解析、`diagnostics.json`、每文件的 `diagnostics/` 树和 SQLite `diagnostics` 表。 |
| `Navigation/` | 基于编译所标记的 `DreamShader.SourceSpans` 元数据进行节点 ↔ 源码行导航。 |
| `Provenance/` | *Revert to Source*、*Adopt Into Source* 和 *Detach*——参见[分歧](../generation/divergence.md)。 |
| `Pass/` *(自 2.1.0 起)* | `FDreamPassPipelineCustomization`——[`UDreamPassPipeline` 的细节面板](../tools/editor-integration.md#pass-pipeline-details-panel)，注册到资产类型所基于的每个引擎上——以及 `DreamPassSpellings`，即反编译器、图转储、细节面板和 `pass-keys.json` 所写入的每个 DreamShaderPass 枚举的 `.dsp` 拼写。 |
| `Preview/` | `FDreamShaderPreviewRenderer`：缩略图场景、渲染目标、轨道和网格处理、PNG 编码、阻塞和异步回读；流式会话和断点探测。参见[预览](../tools/preview.md)。 |
| `SourceFiles/` | [资产重命名同步](../tools/asset-rename-sync.md)服务。源发现本身在 2.0 中移至 `DreamShaderCompiler/Private/Sources/`。 |
| `Tests/` | 自动化测试套件和语料库运行器。参见[测试](testing.md)。 |
| `UI/` | [材质内容浏览器](../tools/material-browser.md)标签页——导航、Sources 和 Assets 视图、检查器及其实时预览、*新建*源、实例工厂和生成资产路径辅助工具。 |
| `VirtualFunction/` | `VirtualFunction` 声明编写和启动同步服务。参见[VirtualFunction 工具](../tools/virtual-function-tools.md)。 |
| `Workspace/` | `FDreamShaderWorkspaceService`：`.code-workspace` 写入器、三个桥接清单、`bridge.db` 以及外部编辑器启动辅助工具。参见[工作区](../tools/workspace.md)。 |

<a id="inside-materialassetgeneration"></a>

### `MaterialAssetGeneration/` 内部

*(在 2.0.0 中移除)* 1.x 生成器——带文本 `import` 内联的源加载、`Graph`
语句遍历器、各关注点降低单元以及生成的 `.ush` 写入器——已被删除。其各项任务
现在由谁完成：

| 1.x 任务 | 现在 |
| :-- | :-- |
| 加载源、内联 `import`、将字节索引映射回行 | `DreamShaderCompiler/Private/Pipeline/` 和 `DreamShaderLang/Private/Preprocessor/`：头文件单独解析，每个诊断都携带自己的行和列 |
| 解析和降低 `Graph` 语句、内置函数、调用和 swizzle | `DreamShaderLang`：旧版前端、绑定器和 IR 降低 |
| 创建 `UMaterialExpression` 节点、节点复用 | `DreamShaderCompiler/Private/Emitter/` |
| 资产、`Root=` / `Path(...)`、设置、反射属性、元数据和跳过检查 | `DreamShaderCompiler/Private/Assets/` 和 `Private/Reflection/` |
| 图布局和拆除 | `DreamShaderCompiler/Private/Layout/` 和 `Private/Assets/`（回滚） |
| HLSL 函数作为 Custom 节点以及生成的 `.ush` | `DreamShaderLang/Private/IR/IRCustomHlsl.cpp`；不写入 `.ush` |

<a id="building"></a>

## 构建

| 目标 | 方法 |
| :-- | :-- |
| 在项目内迭代插件 | 正常构建宿主项目的编辑器目标。该插件是 `EnabledByDefault`。 |
| 以消费者所见的方式独立验证插件 | `RunUAT BuildPlugin`，如[概要](#synopsis)中所述。 |
| 在每个受支持的引擎上验证引擎版本门控 | [`.skill/build-plugin.ps1`](#the-engine-matrix)。 |
| 重现发布存档 | 手动暂存十一个[已发布项](release.md#archive-contents)，或推送标签并让[发布工作流](release.md)完成。 |

`BuildPlugin` 针对目标引擎编译全部五个模块，并在第一个 UBT 或 UHT 错误时失败。在添加引擎版本门控时，这是至关重要的检查，因为项目构建只会针对一个引擎版本进行。它也是唯一能看到**链接**错误的检查：一个在所有地方都能编译、仅从 UE 5.6 起才能解析的引擎类，对每个编译时门控都是不可见的。参见 [UE ≥ 5.6](../api/version-compat.md#ue--56) 下有关版本兼容性的说明。

<a id="the-engine-matrix"></a>

### 引擎矩阵

`.skill/build-plugin.ps1` 为每个引擎根运行一次 `BuildPlugin`，各自进入自己的 `-Package` 目录，并为每个引擎打印一行 `PASS` / `FAIL`，加上失败引擎的编译器诊断。插件自身的 `Binaries/` 和 `Intermediate/` 从不被触碰。其退出代码是失败的引擎数量，因此 `0` 表示矩阵全绿。

```powershell
./.skill/build-plugin.ps1 -Engine C:\UE\UE_5.5, C:\UE\UE_5.6, C:\UE\UE_5.7
```

| 开关 | 作用 |
| :-- | :-- |
| `-Engine` | 引擎根目录。可以是一个数组，或一个以逗号分隔的字符串 |
| `-Plugin` | `.uplugin`。默认为脚本所在或其上方的那个 |
| `-Package` | 暂存根目录。默认为 `%TEMP%\DreamShaderBuildPlugin`；必须位于插件树和每个引擎根目录之外 |
| `-TargetPlatforms` | UAT 语法，以 `+` 分隔。默认为 `Win64` |
| `-StopOnFailure` | 在第一个失败的引擎处停止，而不是完成整个矩阵 |
| `-KeepOutput` | 保留暂存的插件。不保留时只有日志会留存——一次成功的构建每个引擎会产生几百 MB |
| `-Raw` | 输出完整的 UAT 日志，而不是仅输出诊断和步骤行 |
| `-NoRocket` | 丢弃 `-Rocket` |

每个引擎的完整 UAT 日志都保留在 `<Package>\Logs\<Engine>.log` 下，无论通过还是失败。

每个引擎都会构建到 `<Package>\<Engine>`（`UE_5.8`，来自 `Build.version`），该目录在构建前会被删除，由 `BuildPlugin` 自身清空，并在构建后再次删除，除非设置了
`-KeepOutput`。因此，在构建或写入任何内容之前，脚本会检查每个引擎的输出
目录，并在以下情况停止：该目录是驱动器根目录、是或包含或位于某个引擎根目录或
插件树内（`-Engine C:\UE\UE_5.8 -Package C:\UE` 否则会指向引擎本身），或者
已存在其中包含并非由脚本放入的文件。它自己的输出目录携带有
`.build-plugin-output` 标记，因此 `-KeepOutput` 之后的运行会回收它们。

<a id="engine-versions"></a>

## 引擎版本

| 引擎 | 状态 |
| :-- | :-- |
| `5.8` | 已在 `RunUAT BuildPlugin`（`2.2.0`）上验证 |
| `5.7` | 已在 `RunUAT BuildPlugin` 至 `1.8.0` 上通过 `RunUAT BuildPlugin` 验证；未针对 `2.0.0` 至 `2.2.0` 重新运行，其门控的编写方式使 5.7 不需要自己的答案——见 [向类型询问](../api/version-compat.md#asked-of-the-type)。`2.1.0` 的 Custom Pass 在 5.8 以下被编译排除（`DREAMSHADER_WITH_CUSTOM_PASS` 0）；代码的该侧在开发期间于 5.8 上在门控强制关闭（`Invoke-DreamShaderTests.ps1 -WithoutCustomPass`）的情况下构建；`2.1.2` 在 5.5、5.6 和 5.7 处增加了门控，其中最后一个在 5.7 上采用较新的一侧，未经测试 |
| `5.6` | 已在 `RunUAT BuildPlugin`（`2.2.0`）上验证 |
| `5.5` | 已在 `RunUAT BuildPlugin`（`2.2.0`）上验证 |
| `5.4` | 源码兼容；参见下面的工具链警告 |
| `5.3` | 源码兼容；参见下面的工具链警告 |

> [!WARNING]
> **Unreal Engine `5.3` 和 `5.4` 无论你将哪个插件指向它们，都无法在较新的 MSVC 工具链下构建。** `ConcurrentLinearAllocator.h` 写入 `#elif __has_feature(address_sanitizer)`
> 时没有 `defined()` 保护，位于 `#if PLATFORM_HAS_ASAN_INCLUDE` 内——而它是
> `__has_include(<sanitizer/asan_interface.h>)`，对于每个附带 ASan 头文件的工具链都为真。MSVC 没有 `__has_feature`，因此该行会引发 `C4668`，而 UBT 编译引擎代码
> 时使用 `/we4668`。UE 5.5 通过拆分该测试修复了该头文件。
>
> 该失败发生在**引擎**代码中，在任何插件源码被触及之前，且 `Source/DreamShader*`
> 在其中未被提及。Epic 自己的 `ExampleDeviceProfileSelector` 在同一
> 引擎上会以相同方式失败，这是在特定机器上确认原因的最省事方法。请注意，安装
> MSVC `14.38` 本身并**不能**解决问题——它也附带 `sanitizer/asan_interface.h`。需要
> 一个没有那些头文件的工具链，或对引擎头文件打一行补丁。

插件中的每个引擎版本测试都通过 `DreamShaderVersionCompat.h` 中的宏进行；
在 `Source/` 中任何地方都没有原始的 `ENGINE_MAJOR_VERSION` 或 `UE_VERSION_NEWER_THAN` 用法。当你
添加依赖版本的代码时，将它加到那里并使用 `DREAMSHADER_UE_VERSION_AT_LEAST(Major, Minor)`
或 `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`。唯一的例外是 Custom Pass：`DreamShaderPass.Build.cs`
测试 `Target.Version` 并定义 `DREAMSHADER_WITH_CUSTOM_PASS`，而依赖
`DreamShaderPass` 的代码使用 `#if` 测试该定义，而不是测试版本。两侧都能在 5.8 机器上编译和
测试：见 [Custom Pass 门控的两侧](testing.md#both-sides-of-the-custom-pass-gate)。
当前受门控行为的完整列表见 [版本兼容性](../api/version-compat.md)。

<a id="localization"></a>

## 本地化

面向编辑器的文本是 `LOCTEXT`/`NSLOCTEXT`。但线格式不是：`diagnostics.json`、
`diagnostics/*.json`、`bridge.db`、`preview.json` 以及预览 WebSocket 由 VSCode
和 Rider 扩展解析，并且在每种编辑器区域性中都必须读取相同内容。

使两者都成立的原则：

- **到达线格式的文本一路保持为 `FText`**，且写入通过
  `ToInvariantWireString()`（`Diagnostics/DreamShaderTextWireUtils.h`）进行。该函数在固定区域性中重放
  `FText` 的历史格式数据——模式、参数、嵌套格式。
  仅靠 `FTextInspector::GetSourceString` 是不够的：对于 `FText::Format` 结果，它会返回
  *已本地化*的替换后显示内容。
- **绝不要在此过程中用 `ToString()` 折叠这样的 `FText`。** `ToString()` 是本地化后的
  显示内容，而将结果重新包装为 `FText::FromString` 无法恢复英文。因此，错误通道为
  `FString` 类型的 API——`FDreamShaderPreviewRenderContext`、资产层的
  `FDreamShaderError`——保留字面量 `FString::Printf` 消息，并标记为 `I18N-EXEMPT`。
- 仅到达 Slate 的文本没有此类约束；按常规对其进行本地化。

```bash
pwsh -File Tools/Localization/localization_lint.ps1
```

检查 `LOCTEXT_NAMESPACE` 的定义/取消定义配对，拒绝在头文件中定义，并标记 gather 无法看到的字面量（`FText::FromString(TEXT(...))`、`FString::Printf(TEXT(...))`）。有意为之的
非显示字面量通过在**与调用同一行**上的 `I18N-EXEMPT` 注释被豁免——该
检查读取一行，而不是一段范围。`-IncludeDeferred` 扩大这些字面量规则所运行的文件范围。

`-EmitBaseline` 会重新生成 `Tools/Localization/BASELINE.md`，其 gather 计数就是回归
检查；它涵盖每个源文件，因为 `GatherText` 也如此。若计数发生变动却没有对应的
`.locres` 更新，则意味着翻译落后了。

目前还没有 `Config/Localization/DreamShader.ini`，因此 Dashboard 无法重新 gather 目标，
而检入的 `zh-Hans` `.locres` 是手工制作的快照。新的 `LOCTEXT` 会回退到英文，
直到它被重新生成。

<a id="notes"></a>

## 注意事项

- **`DreamShaderEditor` 不导出任何内容。** 它没有 `Public/` 文件夹，并且 `DREAMSHADEREDITOR_API`
  从不出现在源码中，因此第三方 C++ 无法链接到桥接或反编译器。受支持的 C++ 扩展点是 `IDreamShaderCompiler` —— 声明于 `DreamShader`，实现于
  `DreamShaderCompiler`，通过 `GetDreamShaderCompiler()` 访问；其他一切只能通过编辑器 UI、[commandlet](../tools/commandlet.md) 或 [桥接](../tools/bridge.md) 访问。
- **没有模块单例。** 没有模块类具有 `Get()` / `IsAvailable()` 访问器。请使用
  `FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"))`。公开委托包括
  `OnDreamShaderSourceGenerated`（编译完成，位于 `DreamShaderCompiler`）、`DreamShaderDefineResolution.h` 的 define-provider
  委托，以及 `DreamShaderPass` 中的 `FOnDreamPassPipelineChanged`。
- **编译器是游戏线程的、仅限编辑器的。** 编译管线会打开一个 `FScopedSlowTask`，
  创建并修改 `UPackage` 和 `UMaterial`，并调用 `PostEditChange()`。
- **存在三个不同的 "normalize" 辅助函数**，容易混淆：`NormalizeSettingKey`
  （去空白 + 转小写）、`UDreamShaderSettings::NormalizeMappingKey`（去空白 + 转小写 + 去除空格，
  `_` 和 `-`），以及 `NormalizeSourceFilePath`（使用 `/` 分隔符的绝对路径）。`SanitizeIdentifier`
  是标识符清理器，不是键规范化器。
- 生成的文档站点由 `Docs/` 构建；描述符中的 `DocsURL` 指向
  <https://shader.toolchain.64hz.cn/>。

<a id="example"></a>

## 示例

针对 5.7 引擎的完整独立插件构建：

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="D:\Work\MyProject\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="D:\Work\Out\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

暂存输出：

```text
D:\Work\Out\DreamShader\
  Binaries\Win64\UnrealEditor-DreamShaderLang.dll
  Binaries\Win64\UnrealEditor-DreamShader.dll
  Binaries\Win64\UnrealEditor-DreamShaderPass.dll
  Binaries\Win64\UnrealEditor-DreamShaderCompiler.dll
  Binaries\Win64\UnrealEditor-DreamShaderEditor.dll
  Binaries\Win64\UnrealEditor.modules
  Intermediate\              UBT and UHT working files
  Source\                    copied from the plugin tree
  Shaders\                   copied from the plugin tree
  Resources\                 copied from the plugin tree
  Docs\                      copied from the plugin tree
  DreamShader.uplugin
```

相同的五个模块名称会出现在 `<Plugin>/Binaries/Win64/` 下的普通项目构建中，
并伴随其 `.pdb` 文件。

<a id="see-also"></a>

## 另请参阅

- [测试](testing.md) —— 自动化测试套件、fixture 语料库，以及如何无头运行两者
- [发布](release.md) —— 标签约定、发布工作流，以及归档包含的内容
- [C++ API](../api/index.md) —— 导出的头文件，逐个类
- [编译器模块](../api/compiler-module.md) —— `IDreamShaderCompiler`，受支持的扩展点
- [版本兼容性](../api/version-compat.md) —— 兼容宏与所有受门控行为
- [Commandlet](../tools/commandlet.md) —— `-run=DreamShader`，无头入口点
- [编辑器桥接](../tools/bridge.md) —— 请求文件、`bridge.db`、预览 WebSocket
- [生成管线](../generation/index.md) —— 一次编译从端到端做了什么
- [项目设置](../settings/project.md) —— `UDreamShaderSettings`，包括源目录
</content>
</invoke>
