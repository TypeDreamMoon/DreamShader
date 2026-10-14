---
title: "C++ API"
description: "C++ API"
---

<a id="c-api"></a>

# C++ API

> [DreamShader](../index.md) » **C++ API**

该插件公开的 C++ 接口面：五个模块、69 个公共头文件，以及每个调用方都要经过的一个编译接口。

| | |
| :-- | :-- |
| 模块 | `DreamShaderLang`（Runtime）· `DreamShader`（Runtime）· `DreamShaderPass`（Runtime）· `DreamShaderCompiler`（Editor）· `DreamShaderEditor`（Editor） |
| 公共头文件 | 30 + 8 + 11 + 20 + 0 |
| 导出宏 | `DREAMSHADERLANG_API`、`DREAMSHADER_API`、`DREAMSHADERPASS_API`、`DREAMSHADERCOMPILER_API` |
| 公共头文件中的反射类型 | `DreamShader`：2 个 `UCLASS`、2 个 `UENUM` · `DreamShaderPass` *（自 2.1.0 起）*：9 个 `UCLASS`、18 个 `USTRUCT`、17 个 `UENUM` |
| 委托 | 三个 — `FDreamShaderDefineProviderDelegate`，`DreamShaderDefineResolution.h` 中的一个 `DECLARE_DELEGATE_OneParam` *（自 1.9.0 起；在 2.0 中移出 `DreamShaderDefineTable.h`）*；`FOnDreamShaderSourceGenerated`，`DreamShaderCompilerService.h` 中的一个多播委托 *（自 2.0 起公开）*；以及 `FOnDreamPassPipelineChanged`，`DreamPassPipeline.h` 中的一个 `DECLARE_MULTICAST_DELEGATE_OneParam` *（自 2.1.0 起）*。没有 `DECLARE_EVENT*`，也没有 `DECLARE_DYNAMIC*` |
| 插件版本 | `2.1.2`（`"Version": 212`） |

<a id="modules"></a>

## 模块

| 模块 | 类型 | 加载阶段 | 公共头文件 | 导出宏 | 用途 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | `Runtime` | `PostConfigInit` | 30 | `DREAMSHADERLANG_API` | 前端与中端，也是唯一一个只依赖 `Core` 的模块：源文本与跨度、诊断、词法分析器、AST、**两个**解析器（2.0 与 1.x 旧版前端）、打印器、带定义表的预处理器、绑定器（`Semantic/`）、不依赖引擎且带构建器、遍、验证器与比较器的图 IR（`IR/`）、从 IR 回到源码的路径（`Decompile/`），以及 1.x → 2.0 迁移器（`Migrate/`）。它不感知引擎 —— 引擎相关信息以参数形式传入，或作为 `FBuiltinCatalog` 中的数据存在。 |
| `DreamShader` | `Runtime` | `PostConfigInit` | 8 | `DREAMSHADER_API` | 日志类别、规范路径辅助函数、1.x 源数据模型、为预处理器提供引擎侧值的定义解析层级、项目设置对象、生成的实例类、引擎版本宏，以及带注册表的**编译接口**（`GetDreamShaderCompiler`）。曾位于此处的 1.x 解析器已在 2.0 中退役。 |
| `DreamShaderPass` *（自 2.1.0 起）* | `Runtime` | `PostConfigInit` | 11 | `DREAMSHADERPASS_API` | Custom Pass 运行时：`.dsp` 编译生成的 `UDreamPassPipeline` 资产、设置、决定哪些管线应用于某个视图的子系统、体积与组件、两个材质表达式，以及运行这些遍的场景视图扩展 —— 最后一个仅在 UE 5.8 及更高版本上支持（[`DREAMSHADER_WITH_CUSTOM_PASS`](version-compat.md#dreamshader_with_custom_pass)）。参见 [DreamShaderPass](pass-module.md)。 |
| `DreamShaderCompiler` | **`Editor`** | `Default` | 20 | `DREAMSHADERCOMPILER_API` | 编译器的后半部分 *（自 2.0 起；直至 1.9.x 为仅含接口的 Runtime 模块）*：管线、IR 发射器、资产层、由反射构建的内置目录、产物索引、`.dsi` 父模式与设置，以及 *（自 2.1.0 起）* 带 Custom Pass HLSL 槽注册表及其预检查的 `.dsp` 发射。注册唯一的 `IDreamShaderCompiler`。 |
| `DreamShaderEditor` | `Editor` | `Default` | **0** | *（未使用）* | 用户能接触到的一切：反编译器、`dsc migrate`、命令小程序、桥接、预览、Material 内容浏览器、来源操作、工作区导出器、遍管线详情面板，以及自动化测试。 |

这五个模块都在 `DreamShader.uplugin` 中声明。`DreamShaderLang`、`DreamShader` 和 `DreamShaderPass`
在 `PostConfigInit` 加载 —— 足够早，使设置对象与着色器目录映射在任何事物请求它们之前就已存在；
对于 `DreamShaderPass`，还早于引擎确定其着色器类型列表的时间（其全局着色器类型会在模块加载时加入该列表）——
而两个 Editor 模块则在 `Default` 加载。`DreamShaderLang` 在描述符中列于首位：其他所有模块都直接或通过
`DreamShader` 依赖它，单靠加载阶段无法确定它们的顺序。基于同样的原因，`DreamShaderPass` 跟在
`DreamShader` 之后，`DreamShaderCompiler` 列在 `DreamShaderEditor` 之前；在编译器启动之前运行的调用方仍然能获得一个：`GetDreamShaderCompiler()` 会按需加载
该模块。
插件为 `EnabledByDefault` 和 `CanContainContent`；`IsBetaVersion` 为 `false`。

<a id="public-headers"></a>

## 公共头文件

| 头文件 | 模块 | 包含 | 用途 |
| :-- | :-- | :-- | :-- |
| `DreamShaderModule.h` | `DreamShader` | `#include "DreamShaderModule.h"` | `LogDreamShader`、`FDreamShaderModule`、`FDreamShaderSourceRoot`，十八个导出的自由函数(源根目录、路径、标识符清理、文件分类——其中包含 `IsDreamShaderPipelineFile` *(自 2.1.0 起)*)。 |
| `DreamShaderTypes.h` | `DreamShader` | `#include "DreamShaderTypes.h"` | 解析后的 AST 数据模型：13 个结构体、5 个枚举、`LexToString`、`NormalizeSettingKey`。 |
| `DreamShaderAssetReferenceText.h` *(自 1.9.0 起)* | `DreamShader` | `#include "DreamShaderAssetReferenceText.h"` | 针对 Unreal 导出形式中资产引用的纯头文件辅助工具——`Class'/Game/Path/Asset.Asset'`，即 *复制引用* 放入剪贴板的内容：`FDreamShaderReferenceShell` 以及用于剥离外壳的函数。 |
| `DreamShaderCompilerInterface.h` *(自 2.0 起)* | `DreamShader` | `#include "DreamShaderCompilerInterface.h"` | `EThinCustomPersistence`、`FDreamShaderCompileRequest`、`FDreamShaderCompileResult`(`Message` 是 `FText`，`Code` 是第一个 `DSHnnnn`)、`IDreamShaderCompiler`、`GetDreamShaderCompiler`、`RegisterDreamShaderCompiler`。参见 [DreamShaderCompiler](compiler-module.md#the-interface-lives-in-dreamshader)。 |
| `DreamShaderDefineTable.h` *(自 1.9.0 起；`DreamShaderLang` 自 2.0 起)* | `DreamShaderLang` | `#include "DreamShaderDefineTable.h"` | [预处理器](../language/preprocessor.md)定义表，纯文本且不依赖引擎：`EDreamShaderDefineSource`、`FDreamShaderDefineEntry`，区分大小写的 `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap` 别名、`FDreamShaderDefineTable`、`IsReservedDreamShaderDefineName`、`IsValidDreamShaderDefineName`。 |
| `DreamShaderDefineResolution.h` *(自 2.0 起)* | `DreamShader` | `#include "DreamShaderDefineResolution.h"` | 构建上述某个表的引擎部分：`GetBuiltinDreamShaderDefines`、`RegisterDreamShaderDefine`、`UnregisterDreamShaderDefinesFrom`、`FDreamShaderDefineProviderDelegate`、`RegisterDreamShaderDefineProvider`、`UnregisterDreamShaderDefineProvider`、`SetDreamShaderCommandLineDefines`、`ResolveDreamShaderDefines`、`GetDreamShaderDefineRevision`、`NotifyDreamShaderDefineSettingsChanged`。 |
| `DreamShaderPreprocessor.h` *(自 1.9.0 起；`DreamShaderLang` 自 2.0 起)* | `DreamShaderLang` | `#include "DreamShaderPreprocessor.h"` | `PreprocessDreamShaderSource`、`FDreamShaderPreprocessResult`、`DreamShaderSourceHasPreprocessorDirectives`、`BuildDreamShaderDefineKeyFragment`。 |
| `DreamShaderSettings.h` | `DreamShader` | `#include "DreamShaderSettings.h"` | `EDreamShaderDefaultBackend`、`UDreamShaderSettings`，枚举别名解析器。 |
| `DreamShaderMaterialInstance.h` | `DreamShader` | `#include "DreamShaderMaterialInstance.h"` | `UDreamShaderMaterialInstance`——ThinCustom 后端生成的资产。 |
| `DreamShaderVersionCompat.h` | `DreamShader` | `#include "DreamShaderVersionCompat.h"` | 六个引擎版本宏。无类型，无函数。 |
| `DreamPassPipeline.h` · `DreamPassTypes.h` · `DreamPassSubsystem.h` · `DreamPassSettings.h` · `DreamShaderPassModule.h` · 另有 6 个 *(自 2.1.0 起)* | `DreamShaderPass` | `#include "DreamPassPipeline.h"` | Custom Pass 资产及其描述符类型、子系统、设置、体积与组件、蓝图库、两个材质表达式、槽位注册表路径。列于 [DreamShaderPass 页面](pass-module.md#public-headers)。 |
| `DreamShaderCompilerService.h` · `DreamShaderCompilePipeline.h` · `DreamShaderIREmitter.h` · `DreamShaderBuiltinCatalog.h` · `DreamShaderProductIndex.h` · `DreamShaderPassPipelines.h` *(自 2.1.0 起)* · 另有 14 个 | `DreamShaderCompiler` | 仅编辑器 | 编译器服务、带其中间产物的管线、发射器、目录、产物索引、Custom Pass 槽位注册表工具。列于 [DreamShaderCompiler 页面](compiler-module.md#public-headers)。 |
| `Lang/*.h` · `Semantic/LangBound.h` · `IR/*.h` · `Decompile/IRToAst.h` · `Migrate/LangMigrate.h` | `DreamShaderLang` | 保留文件夹前缀：`#include "Lang/LangParser.h"` | 语言本身。列于 [DreamShaderLang 页面](lang-module.md#public-headers)。 |

命名空间：`DreamShaderModule.h`、`DreamShaderTypes.h`、
`DreamShaderCompilerInterface.h`、`DreamShaderDefineResolution.h`、`DreamShaderDefineTable.h` 和
`DreamShaderPreprocessor.h` 中的所有内容都位于 `UE::DreamShader` 中——在
2.0 中迁移到 `DreamShaderLang` 的那两个保留了该命名空间，因此它们的调用方无需更改源码。语言位于
`UE::DreamShader::Lang` 和 `UE::DreamShader::IR`；编译器模块是
`UE::DreamShader::Editor::Compiler`。`FDreamShaderModule`、`FDreamShaderCompilerModule`、
`UDreamShaderSettings`、`UDreamShaderMaterialInstance` 和 `EDreamShaderDefaultBackend` 位于全局
作用域。`DreamShaderPass` *(自 2.1.0 起)* 将其反射类型——`UDreamPassPipeline`、
`UDreamPassSubsystem`、`ADreamPassVolume`、`EDreamPass*` 枚举和 `FDreamPass*` 结构体——置于全局
作用域(反射类型必须如此)，并将其自由函数置于 `UE::DreamPass` 中。

<a id="linkage"></a>

## 链接

> [!WARNING]
> **`DreamShaderEditor` 不导出任何内容，无法被链接。** 它根本没有 `Public/` 文件夹，
> 其 `Build.cs` 未声明任何 `PublicDependencyModuleNames`，而 `DREAMSHADEREDITOR_API`——
> UnrealBuildTool 为每个模块都会定义——在插件源码中零次出现。
> 其模块类 `FDreamShaderEditorModule` 声明于 `.cpp` 内部，而非头文件中，因此
> 也无法从外部命名。编辑器模块所做的一切只能通过
> UE 的模块系统、通过 UI 操作或通过 [commandlet](../tools/commandlet.md) 访问。
> 编译已不再属于这些操作：自 2.0 起编译器是独立模块，通过
> [`IDreamShaderCompiler`](compiler-module.md#idreamshadercompiler) 访问。

| 模块 | `DREAMSHADERLANG_API` | `DREAMSHADER_API` | `DREAMSHADERPASS_API` | `DREAMSHADERCOMPILER_API` | `DREAMSHADEREDITOR_API` |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShaderLang` | `DLLEXPORT` | — | — | — | — |
| `DreamShader` | `DLLIMPORT` | `DLLEXPORT` | — | — | — |
| `DreamShaderPass` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` | — | — |
| `DreamShaderCompiler` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` | — |
| `DreamShaderEditor` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLIMPORT` | `DLLEXPORT` *(已定义，从未使用)* |

`UE_PLUGIN_NAME` 对全部五个模块均为 `"DreamShader"`。

这些头文件均未用 `#ifndef` 保护其导出宏；它们都依赖 UBT 生成的定义。

<a id="there-are-no-module-singletons"></a>

### 不存在模块单例

该插件未在任何模块类上声明 `Get()`、`IsAvailable()` 或 `GetChecked()` 访问器。请使用
标准的模块管理器——或者对于编译器，使用 `GetDreamShaderCompiler()`，这是该插件唯一的
注册表：

```cpp
FDreamShaderModule& Module = FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"));
```

实际上，模块对象本身并不需要任何东西——只要模块加载完成，
`UE::DreamShader` 中导出的自由函数即可使用。

<a id="build-dependencies"></a>

## 构建依赖

<a id="dreamshaderbuildcs"></a>

### `DreamShader.Build.cs`

| 设置 | 值 |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`、`CoreUObject`、`DeveloperSettings`、`DreamShaderLang`、`Engine`、`Projects`、`RenderCore` |
| `PrivateDependencyModuleNames` | *(未声明任何项)* |
| `PublicDefinitions` | `DREAMSHADER_WITH_MOON_ENGINE=0\|1`，从引擎的 `SceneTypes.h` 探测；`DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS=0\|1`，从声明 `EMaterialParameterType` 的头文件探测 |

| 依赖 | 用途 |
| :-- | :-- |
| `Core` | `FString`、`FPaths`、`IFileManager`，日志类别 |
| `CoreUObject` | `UObject`、`UObjectInitialized()`、两个 `UCLASS` |
| `DeveloperSettings` | `UDeveloperSettings`，`UDreamShaderSettings` 的基类 |
| `DreamShaderLang` | `DreamShaderDefineResolution.h` 填充的定义表，`FDreamShaderError` |
| `Engine` | `Engine/EngineTypes.h`、`MaterialDomain.h`、`UMaterialInstanceConstant` |
| `Projects` | `IPluginManager`——着色器挂载查找和 `Path(Plugin.X, …)` 解析 |
| `RenderCore` | `ShaderCore.h` 的 `AllShaderSourceDirectoryMappings` / `AddShaderSourceDirectoryMapping` |

<a id="dreamshaderpassbuildcs"></a>

### `DreamShaderPass.Build.cs`

*(自 2.1.0 起)*

| 设置 | 值 |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`、`CoreUObject`、`DeveloperSettings`、`DreamShader`、`Engine`、`RenderCore`、`RHI` |
| `PrivateDependencyModuleNames` | `Projects`、`Renderer` |
| `PublicDefinitions` | 当目标引擎为 5.8 或更高版本时为 `DREAMSHADER_WITH_CUSTOM_PASS=1`（`Target.Version`），低于该版本时为 `=0` —— 见 [`DREAMSHADER_WITH_CUSTOM_PASS`](version-compat.md#dreamshader_with_custom_pass) |
| `PrivateIncludePaths` | 仅在 5.8 及更高版本上，Renderer 模块的 `Internal` 文件夹，用于 `FPostProcessingInputs` —— UBT 只会将 `Internal` 文件夹交给处于同一规则作用域的模块，引擎插件属于该作用域，而项目插件则不属于 |

引擎门控位于构建规则中，而不是描述符中，描述符没有按模块的引擎字段，并且该定义是公开的，以便 `DreamShaderCompiler` 和 `DreamShaderEditor` 询问同一问题，而不是再次推导它。

<a id="dreamshadercompilerbuildcs"></a>

### `DreamShaderCompiler.Build.cs`

| 设置 | 值 |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | `Core`、`CoreUObject`、`DreamShader`、`DreamShaderLang`、`Engine` |
| `PrivateDependencyModuleNames` | `AssetRegistry`、`AssetTools`、`DreamShaderPass`、`Json`、`MaterialEditor`、`Projects`、`RenderCore`、`RHI`、`TargetPlatform`、`UnrealEd` |

这些公开依赖之所以是公开的，是因为公开头文件暴露了它们：`UMaterial*` 和
`EMaterialProperty`（`Engine`）、`FTextShaderDefinition` 以及编译接口（`DreamShader`）、
`FLangDiagnosticSink` / `FModule` / `FIRModule`（`DreamShaderLang`）。Editor 类型的模块不能
作为 Runtime 类型模块的依赖，这就是接口本身位于 `DreamShader` 的原因。

`DreamShaderPass`、`RenderCore`、`RHI` 和 `TargetPlatform` 是 `.dsp` 的另一半*（自 2.1.0 起）*：
`DreamShaderPass` 用于 `.dsp` 编译成的资材、其设置及其槽路径；另外三个用于
HLSL 槽 —— 着色器源映射、预检查的进程内编译、它按名称找到的全局着色器类型、着色器平台以及目标平台烘焙的格式。该模块没有任何公开头文件
提及它们，因此它们是私有的。

<a id="dreamshadereditorbuildcs"></a>

### `DreamShaderEditor.Build.cs`

| 设置 | 值 |
| :-- | :-- |
| `PCHUsage` | `PCHUsageMode.UseExplicitOrSharedPCHs` |
| `PublicDependencyModuleNames` | **无** |
| `PrivateDependencyModuleNames` | 31 个，列于下方 |
| `PrivateDefinitions` | 当引擎的 `SceneTypes.h` 声明 `MP_MoonEncodedAttribute0` 时为 `MOON_ENGINE=1` |

| # | 模块 | 用途 |
| :-: | :-- | :-- |
| 1 | `ApplicationCore` | 从工作区服务启动外部编辑器和使用剪贴板 |
| 2 | `AssetRegistry` | `FAssetRegistryModule` —— 来自桥接的资材创建/删除广播 |
| 3 | `AssetTools` | `FAssetToolsModule` —— 材质实例工厂 |
| 4 | `ContentBrowser` | `FContentBrowserModule` —— Project 页面和文件夹选择器 |
| 5 | `Core` | — |
| 6 | `CoreUObject` | — |
| 7 | `DesktopPlatform` | 文件和文件夹对话框 |
| 8 | `DirectoryWatcher` | `FDirectoryWatcherModule` —— 保存时自动编译背后的源目录监视器，以及对 Custom Pass 着色器文件的监视 |
| 9 | `DreamShader` | 类型、设置、日志类别、编译接口 |
| 10 | `DreamShaderCompiler` | 管线、发射器的资材层、目录、产品索引 |
| 11 | `DreamShaderLang` | 两个前端、IR、IR → 源代码、迁移器 |
| 12 | `DreamShaderPass` *（自 2.1.0 起）* | `UDreamPassPipeline` 及其类型：`.dsp` 反编译器、Adopt、浏览器、细节面板和 `dump-graph` 读取资材 |
| 13 | `Engine` | 材质、包 |
| 14 | `InputCore` | Slate 输入 |
| 15 | `Json` | 桥接清单和诊断文件 |
| 16 | `MaterialEditor` | `UMaterialEditingLibrary`、节点导航 |
| 17 | `Projects` | `IPluginManager` |
| 18 | `PropertyEditor` *（自 2.1.0 起）* | 管线资材的细节面板 |
| 19 | `RHI` | 预览渲染目标、`EShaderPlatform` 枚举 |
| 20 | `RenderCore` | 着色器目录映射 |
| 21 | `Renderer` | 预览渲染 |
| 22 | `Settings` | 打开项目设置页面 |
| 23 | `Slate` | UI |
| 24 | `SlateCore` | UI |
| 25 | `SQLiteCore` | 桥接数据库 —— 见下文 |
| 26 | `TargetPlatform` | `check -Shaders`：着色器编译所针对的平台 |
| 27 | `ToolMenus` | `UToolMenus` 菜单和工具栏扩展 |
| 28 | `ToolWidgets` | 搜索框和筛选控件 |
| 29 | `UnrealEd` | 命令基类、编辑器子系统、`FScopedSlowTask` |
| 30 | `WebSocketNetworking` | 实时预览服务器 —— 见下文 |
| 31 | `WorkspaceMenuStructure` | Material Content Browser 的标签页生成器类别 |

<a id="plugin-dependencies"></a>

## 插件依赖

`DreamShader.uplugin` 恰好启用了两个其他插件。二者仅由 `DreamShaderEditor` 使用。

| 插件 | 已启用 | 实际使用位置 |
| :-- | :-- | :-- |
| `WebSocketNetworking` | 是 | 仅一处：实时预览 WebSocket 服务器。编辑器桥接按名称加载 `IWebSocketNetworkingModule`，在端口 **17864** 上启动服务器，并将预览帧作为原始二进制 WebSocket 帧流式传输，这些帧携带 `[4-byte length][1-byte type tag][payload]` 信封。见 [编辑器桥接](../../tools/bridge.md)。 |
| `SQLiteCore` | 是 | 两个翻译单元，一个文件：`<Project>/Saved/DreamShader/Bridge/bridge.db`。工作区服务创建架构并写入设置别名、材质表达式和 Substrate 内置表；诊断存储写入 `diagnostics` 表。该数据库是 **VSCode 和 Rider 语言扩展的缓存** —— 插件中没有任何内容会读回它。见 [工作区](../../tools/workspace.md)。 |

<a id="pages"></a>

## 页面

| 页面 | 涵盖内容 |
| :-- | :-- |
| [`DreamShaderModule.h`](dreamshader-module.md) | 模块类、`LogDreamShader`、启动着色器挂载以及每个导出的自由函数 |
| [`DreamShaderTypes.h`](types.md) | 1.x 源代码数据模型中的每个结构和枚举，资材层仍然使用它们 |
| [`DreamShaderParser.h`](parser.md) | **在 2.0 中已弃用。** 取代 `FTextShaderParser::Parse` 的内容 |
| [`DreamShaderSettings.h`](settings.md) | `UDreamShaderSettings`、`EDreamShaderDefaultBackend` 以及别名目录 |
| [`DreamShaderMaterialInstance.h`](material-instance.md) | `UDreamShaderMaterialInstance` 及其两个重载 |
| [`DreamShaderVersionCompat.h`](version-compat.md) | 兼容宏以及它们选择的每个版本门控行为 |
| [`DreamShaderCompiler`](compiler-module.md) | 编译接口及其注册表、编译器服务、管线以及该模块的其他公开头文件 |
| [`DreamShaderLang`](lang-module.md) | 前端和中端：`Lang/`、`Semantic/`、`IR/`、`Decompile/` 和 `Migrate/` 头文件、移入其中的两个预处理器头文件、从预处理到 IR 验证再回到源代码的每个入口点，以及 `Lang` 夹具语料库 |
| [`DreamShaderPass`](pass-module.md) *（自 2.1.0 起）* | Custom Pass 运行时的 C++ 和蓝图接口：管线资材、子系统和激活源、体积和组件、槽注册表路径 |
| `DreamShaderDefineTable.h` · `DreamShaderPreprocessor.h` · `DreamShaderDefineResolution.h` | 尚无独立页面；整个接口从语言侧记录在 [预处理器](../language/preprocessor.md) 上。前两个位于 `DreamShaderLang` —— 见 [DreamShaderLang 页面](lang-module.md) |

<a id="notes"></a>

## 说明

- **请求编译只需要运行时模块。** `GetDreamShaderCompiler()` 会返回编辑器端注册的编译器，若不存在则返回 null（游戏目标）。生成本身仅限编辑器，位于 [`DreamShaderCompiler`](compiler-module.md)。
- **该语言与引擎无关。** `DreamShaderLang` 中的所有内容——解析任一语法、绑定、降级为 IR、打印、迁移——都是对 `Core` 的字符串和数据处理；它不会创建 `UObject`，也不会加载任何资源。绑定需要一个 `FBuiltinCatalog`，`dsc export-catalog` 将其写为 JSON，供在编辑器外部运行的工具使用。
- `DreamShader` 导出两个 `UCLASS`，即 `UDreamShaderSettings` 和 `UDreamShaderMaterialInstance`。`DreamShaderPass` *（自 2.1.0 起）* 通过 `DREAMSHADERPASS_API` 导出七个——管道、设置、子系统、体积、两个组件和蓝图库——并声明其两个材质表达式 `MinimalAPI`。`UDreamShaderCommandlet` 存在于编辑器模块的 `Private/` 文件夹中，**没有** API 宏，因此只能通过 UE 的反射系统和 `-run=DreamShader` 访问。
- `Config/FilterPlugin.ini` 仅包含自带的带注释模板——该插件未声明任何额外的打包文件。

<a id="example"></a>

## 示例

一个希望解析 DreamShaderLang 并请求编译的使用方模块：

```csharp
// MyTooling.Build.cs
PrivateDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "DreamShaderLang",      // the language: parse, print, bind, IR
    "DreamShader"           // paths, settings, the compile interface
});
```

```cpp
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderModule.h"
#include "Lang/LangParser.h"

using namespace UE::DreamShader;

void InspectSource(const FString& InPath)
{
    if (!IsDreamShaderSourceFile(InPath))
    {
        return;
    }

    FString SourceText;
    const FString Path = NormalizeSourceFilePath(InPath);
    if (!FFileHelper::LoadFileToString(SourceText, *Path))
    {
        return;
    }

    // Auto picks the front end from the extension: .dss / .dsi / .dsp / .dsh are 2.0, .dsm / .dsf are 1.x.
    const Lang::FLangSourceText Source(Path, SourceText);
    const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Source);
    UE_LOG(LogDreamShader, Display, TEXT("%s: %d declaration(s), %d diagnostic(s)."),
        *Path,
        Parsed.Module.IsValid() ? Parsed.Module->Declarations.Num() : 0,
        Parsed.Diagnostics.GetDiagnostics().Num());

    if (Parsed.Succeeded())
    {
        if (IDreamShaderCompiler* Compiler = GetDreamShaderCompiler())
        {
            FDreamShaderCompileRequest Request;
            Request.SourceFilePath = Path;
            Compiler->CompileAssets(Request);
        }
    }
}
```

`ParseDreamShaderLang` 不读取文件，也不遵循 `#include` / `import`：包含在绑定时解析，由宿主交给绑定器的解析器完成。

<a id="see-also"></a>

## 另请参阅

- [`DreamShaderModule.h`](dreamshader-module.md) —— 模块入口点及其导出的辅助函数
- [`DreamShaderCompiler`](compiler-module.md) —— 编译接口、服务和管道
- [`DreamShaderPass`](pass-module.md) —— Custom Pass 运行时
- [项目设置](../settings/project.md) —— 从用户角度看的 `UDreamShaderSettings`
- [生成](../generation/index.md) —— 生成的资源是什么样子
- [Commandlet](../tools/commandlet.md) —— `-run=DreamShader`，无头入口点
- [编辑器桥接](../../tools/bridge.md) —— WebSocket 服务器和请求文件协议
- [工作区](../../tools/workspace.md) —— `bridge.db` 缓存和导出的清单
- [贡献](../contributing/index.md) —— 从源码构建插件
