---
title: "DreamShaderModule.h"
description: "DreamShaderModule.h"
---

<a id="dreamshadermoduleh"></a>

# DreamShaderModule.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderModule.h**

运行时模块入口点、插件的日志类别，以及其他所有模块所依赖的路径、标识符和文件分类辅助工具。

定义于头文件 `DreamShaderModule.h`。

| | |
| :-- | :-- |
| 模块 | `DreamShader`（Runtime） |
| 包含 | `#include "DreamShaderModule.h"` |
| 命名空间 | `UE::DreamShader`（自由函数）· 全局作用域（`LogDreamShader`、`FDreamShaderModule`） |
| 导出宏 | `DREAMSHADER_API` |
| 引入 | `CoreMinimal.h`、`DreamShaderVersionCompat.h`、`Modules/ModuleManager.h` |

<a id="synopsis"></a>

## 概要

```cpp
#include "CoreMinimal.h"
#include "DreamShaderVersionCompat.h"
#include "Modules/ModuleManager.h"

class IPlugin;

DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All);

namespace UE::DreamShader
{
    struct FDreamShaderSourceRoot
    {
        FString Directory;          // absolute, normalized, no trailing slash
        FString PackagesDirectory;  // <Directory>/Packages
        FString DisplayName;        // "Project", or the owning plugin's name
        FString PluginName;         // empty for the project root
        bool bIsProjectRoot = false;
        bool bIsProjectPlugin = false; // true when the plugin is one this project generates into
        bool bWritable = false;     // true for the project root only
    };

    DREAMSHADER_API const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots();
    DREAMSHADER_API const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath);
    DREAMSHADER_API bool IsWritableSourceFilePath(const FString& InPath);
    DREAMSHADER_API bool IsProjectContentPlugin(const IPlugin& InPlugin);
    DREAMSHADER_API void RefreshSourceShaderRoots();
    DREAMSHADER_API bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);

    DREAMSHADER_API FString GetSourceShaderDirectory();
    DREAMSHADER_API FString GetPackageShaderDirectory();
    DREAMSHADER_API FString GetGeneratedShaderDirectory();
    DREAMSHADER_API FString GetGeneratedShaderVirtualDirectory();
    DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
    DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderLang2File(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderInstanceFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderPipelineFile(const FString& InPath);
    DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
}

class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

<a id="logdreamshader"></a>

## `LogDreamShader`

| 方面 | 值 |
| :-- | :-- |
| 声明 | `DREAMSHADER_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamShader, Log, All)` |
| 默认运行时详细级别 | `Log` |
| 编译时最大详细级别 | `All` |
| 定义于 | `DreamShader`（Runtime） |
| 使用方 | `DreamShader`、`DreamShaderCompiler` 和 `DreamShaderEditor`。**不**包括 `DreamShaderLang`：它仅依赖 `Core` 且从不记录日志，而是通过 `FLangDiagnosticSink` 进行报告。也**不**包括 `DreamShaderPass`，它有自己的 `LogDreamPass` |

编译器和编辑器记录的每条消息——输出到日志的诊断信息、生成结果、cook 进度、桥接和 SQLite 警告——都经过这同一个类别。可通过配置文件或控制台提升其级别：

```text
[Core.Log]
LogDreamShader=Verbose
```

<a id="directory-functions"></a>

## 目录函数

```cpp
FString GetSourceShaderDirectory();
FString GetPackageShaderDirectory();
FString GetGeneratedShaderDirectory();
FString GetGeneratedShaderVirtualDirectory();
```

| 函数 | 返回值 | 设置未配置时的值 |
| :-- | :-- | :-- |
| `GetSourceShaderDirectory()` | 绝对、规范化的**项目**源根目录——即相对于项目目录解析的 `UDreamShaderSettings::SourceDirectory.Path`。所有编辑器端写入的目标；插件根目录位于[源根目录](#source-roots)下 | `<Project>/DShader` |
| `GetPackageShaderDirectory()` | 项目根目录的 `<source root>/Packages`，始终由 `GetSourceShaderDirectory()` 派生；没有单独的设置项 | `<Project>/DShader/Packages` |
| `GetGeneratedShaderDirectory()` | 当前为虚拟挂载点 `/DreamShaderGenerated` 注册的真实目录；**仅当不存在此类映射时**，才回退到配置的 `GeneratedShaderDirectory` | `<Project>/Intermediate/DreamShader/GeneratedShaders` |
| `GetGeneratedShaderVirtualDirectory()` | 编译时常量 `TEXT("/DreamShaderGenerated")` | 不适用 |

配置路径的解析顺序：

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 去除配置路径的首尾空白。如果为空，则替换为硬编码的默认值。 |
| 2 | 如果结果为相对路径，则与 `FPaths::ProjectDir()` 合并。 |
| 3 | `FPaths::NormalizeFilename`，然后 `FPaths::MakeStandardFilename`。 |

这三个配置路径保存在一个内部缓存中。当缓存尚未初始化**或**设置对象当前可读时——即
`UObjectInitialized() && !GExitPurge && !IsEngineExitRequested()`——会刷新缓存。一旦 UObject 系统启动，此条件始终为真，因此设置会在**每次**调用时重新读取，会话期间在项目设置中的更改会立即生效。在 UObject 存在之前以及退出清理期间，getter 不会访问 `GetDefault<UDreamShaderSettings>()`：它们直接从引擎 ini 中的 `[/Script/DreamShader.DreamShaderSettings]` 读取 `SourceDirectory` 和
`GeneratedShaderDirectory`——即设置对象将要加载的值——并在某个键缺失时回退到默认值。模块在 `PostConfigInit` 启动，早于设置对象可能存在的时机，这正是原因所在。

> [!WARNING]
> `GetGeneratedShaderDirectory()` 并不简单等同于 *Generated Shader Directory* 设置。
> `StartupModule` 仅在 `/DreamShaderGenerated` 映射不存在时注册它，并且从不
> 重新指向它。因此，在会话中途更改该设置会改变 `StartupModule` *本应*
> 挂载的位置，并改变内部缓存值，但 `GetGeneratedShaderDirectory()` 仍会
> 返回启动时挂载的目录。重启编辑器以移动挂载点。参见
> [生成的 HLSL](../generation/generated-hlsl.md)。

> [!NOTE]
> 该缓存是一个普通静态变量，没有锁。应将这四个函数视为游戏线程 API，尽管
> 它们本质上并不与任何线程绑定。

<a id="source-roots"></a>

## 源根目录

*（自 1.6.0 起）*

```cpp
const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots();
const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath);
bool IsWritableSourceFilePath(const FString& InPath);
bool IsProjectContentPlugin(const IPlugin& InPlugin);
void RefreshSourceShaderRoots();
bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory);
```

源根目录是一个目录树，源文件在其下被发现，也是导入解析的单位：文件的导入仅针对其自身的根目录及该根目录的 `Packages` 文件夹进行解析。

| 函数 | 行为 |
| :-- | :-- |
| `GetSourceShaderRoots()` | 首先是项目根目录，然后——当 *Scan Plugin Source Directories* 开启时——为每个启用了 `DShader` 文件夹的已启用插件提供一个根目录。与较早的根目录重叠（一个包含另一个）的插件根目录会被丢弃，并发出 `LogDreamShader` 警告。已缓存；在项目源目录或扫描开关更改时重建。该引用会在下次重建时失效，因此请在游戏线程上使用它，并且不要保留它 |
| `FindSourceRootForFile(InPath)` | 包含 `InPath` 的根目录，若其不属于任何根目录则为 `nullptr` |
| `IsWritableSourceFilePath(InPath)` | 对于插件根目录下的文件返回 `false`；对于项目根目录下的文件**以及**不属于任何根目录的路径（显式命名的临时源文件，例如命令件 `-Source`）返回 `true` |
| `IsProjectContentPlugin(InPlugin)` | 当插件是项目插件（`EPluginType::Project`）且其基础目录位于 `<Project>/Plugins` 下时返回 `true`——与第 3 关对 `Plugin.<Name>` 根目录所问的相同问题。对于引擎或市场插件返回 `false`，其挂载点不允许任何宿主项目生成到其中 |
| `RefreshSourceShaderRoots()` | 丢弃缓存。仅在插件于会话中途挂载或卸载时需要 |
| `IsPathUnderSourceDirectory(InPath, InDirectory)` | 当路径是目录本身或其下级时返回 `true`；两者都用 `NormalizeSourceFilePath` 规范化并进行不区分大小写的比较 |

只有项目根目录可写：那些就地重写源文件的编辑器功能（VirtualFunction 同步、[资产重命名同步](../../tools/asset-rename-sync.md)）绝不会触碰插件根目录。

`FDreamShaderSourceRoot::bIsProjectPlugin` 携带关于插件根目录的第二个独立答案——
即其产物是否属于项目应构建的内容，通过 `IsProjectContentPlugin` 计算。cook 会对生成失败的源文件读取此信息：在非项目插件的插件下，失败会被报告且 cook 继续进行，因为这些资产随该插件一同发布。参见
[Cook 行为](../generation/ephemeral.md#cook-behaviour)。

<a id="sanitizeidentifier"></a>

## `SanitizeIdentifier`

```cpp
DREAMSHADER_API FString SanitizeIdentifier(const FString& InText);
```

将任意文本重写为可用作 HLSL 或资产标识符的内容。纯函数；不触碰任何引擎状态；可从任何线程调用。

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 将 `[A-Za-z0-9_]` 之外的每个字符替换为 `_`。 |
| 2 | 如果结果为空，则返回 `DreamShaderSymbol`。 |
| 3 | 如果结果仅由下划线组成，则返回 `DreamShaderSymbol`。 |
| 4 | 如果第一个字符不在 `[A-Za-z_]` 中——即它是数字——则前置 `_`。 |
| 5 | 从右向左扫描，将每一段连续的下划线折叠为单个 `_`。 |

| 输入 | 结果 |
| :-- | :-- |
| `Noise::Perlin` | `Noise_Perlin` |
| `2Fast` | `_2Fast` |
| `a  b` | `a_b` |
| `___` | `DreamShaderSymbol` |
| `""` | `DreamShaderSymbol` |
| `Ünlit` | `_nlit` |

> [!WARNING]
> 字符分类是基于原始 ASCII 范围进行的。非 ASCII 字母会被**替换**，而不是
> 保留——`Ünlit` 会变成 `_nlit`，而不是 `Ünlit`。在命名空间和函数
> 名称中请使用 ASCII 标识符。

插件内部的调用方：发射器中的瞬态 ThinCustom 基础材质名称（`MB_DreamThinBase_<package>`）、输出命名重路由名称（`DS_<Name>_<Index>`），以及实例工厂和溯源操作中新建实例的 `.dsi` 文件对应的 `MI_<Name>` 词干。`DreamShaderLang` 无法链接该模块，因此旧版前端（将 `A::B` 展平为 `A_B`）和自定义 HLSL 构建器（`DreamShaderFn_<Name>`）各自保留同一规则的副本 *（自 2.0.0 起）*。

<a id="normalizesourcefilepath"></a>

## `NormalizeSourceFilePath`

```cpp
DREAMSHADER_API FString NormalizeSourceFilePath(const FString& InPath);
```

依次应用 `FPaths::ConvertRelativePathToFull`、`FPaths::NormalizeFilename`，然后 `FPaths::MakeStandardFilename`。结果为使用 `/` 分隔符的绝对路径。

这是源文件的**唯一**规范键。桥接模块、诊断存储、编译器和命令行工具在比较路径前都会先将其规范化，因此此函数生成的路径与插件内部存储的路径比较时相等。

<a id="the-four-normalizers-compared"></a>

## 四种规范化函数的比较

有四个名称相似但行为不同的函数。它们不可互换。

| 函数 | 头文件 | 操作 | 典型对象 |
| :-- | :-- | :-- | :-- |
| `UE::DreamShader::NormalizeSourceFilePath` | `DreamShaderModule.h` | 绝对路径，`/` 分隔符 | `.dsm` / `.dsf` / `.dsh` 文件路径 |
| `UE::DreamShader::SanitizeIdentifier` | `DreamShaderModule.h` | 非 `[A-Za-z0-9_]` → `_`，数字开头修正，连续下划线折叠 | HLSL 或资产标识符 |
| `UE::DreamShader::NormalizeSettingKey` | [`DreamShaderTypes.h`](types.md#normalizesettingkey) | 去除首尾空白，然后转为小写 — **仅此而已** | `Settings` / `Options` 键 |
| `UDreamShaderSettings::NormalizeMappingKey` | [`DreamShaderSettings.h`](settings.md#normalizemappingkey) | 去除首尾空白、转小写，然后去除所有空格、`_` 和 `-` | `ShadingModel` / `BlendMode` / `Domain` **值**别名 |

后两个是最常被混淆的一对。`NormalizeSettingKey` 保留空格和标点，因此设置键 `Two Sided` 和 `TwoSided` 是**不同**的键。`NormalizeMappingKey` 会移除它们，因此值 `Two Sided Foliage`、`TwoSidedFoliage` 和 `two-sided_foliage` 都解析为同一个着色模型。

<a id="file-classification-predicates"></a>

## 文件分类谓词

```cpp
DREAMSHADER_API bool IsDreamShaderMaterialFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderHeaderFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderFunctionFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderLang2File(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderInstanceFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderPipelineFile(const FString& InPath);
DREAMSHADER_API bool IsDreamShaderSourceFile(const FString& InPath);
```

| 函数 | 当扩展名为以下内容时返回 `true` |
| :-- | :-- |
| `IsDreamShaderMaterialFile` | `.dsm` |
| `IsDreamShaderHeaderFile` | `.dsh` |
| `IsDreamShaderFunctionFile` | `.dsf` |
| `IsDreamShaderLang2File` | `.dss` — 2.0 编译单元 |
| `IsDreamShaderInstanceFile` | `.dsi` — 2.0 材质实例 |
| `IsDreamShaderPipelineFile` *（自 2.1.0 起）* | `.dsp` — [Custom Pass 管线](../language-v2/passes.md) |
| `IsDreamShaderSourceFile` | 六者之一：`.dsm`、`.dsh`、`.dsf`、`.dss`、`.dsi` **或** `.dsp` *（`.dsp` 自 2.1.0 起）* |

`IsDreamShaderSourceFile` 是所有扫描、监视或列出源文件的代码都会询问的谓词 —— 启动扫描、源目录监视器、`compile -All`、材质内容浏览器 —— 因此 `.dsp` 对它们而言都是源文件；只有编译器会区分各种类型。所有七个谓词都将 `FPaths::GetExtension(InPath, /*bIncludeDot*/ true)` 的结果与 `ESearchCase::IgnoreCase` 进行比较，因此**扩展名匹配不区分大小写**：`Foo.DSM` 会被归类为材质文件。仅检查扩展名 —— 文件无需存在，也从不读取其内容。纯函数；可在任意线程调用。

<a id="fdreamshadermodule"></a>

## `FDreamShaderModule`

```cpp
class DREAMSHADER_API FDreamShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

通过 `IMPLEMENT_MODULE(FDreamShaderModule, DreamShader)` 注册。该类不暴露任何状态和访问器；如需获取，可使用 `FModuleManager::LoadModuleChecked<FDreamShaderModule>(TEXT("DreamShader"))`。

<a id="startupmodule"></a>

### `StartupModule()`

在模块加载时于游戏线程运行，处于 `PostConfigInit` 阶段 —— 早于任何材质加载，以便缓存包含路径中命名了 `/DreamShaderGenerated` 的材质能找到已注册的映射。它按顺序执行五个副作用。

| # | 操作 | 备注 |
| :-- | :-- | :-- |
| 1 | 将 `GetSourceShaderDirectory()` 创建为目录树 | 默认为 `<Project>/DShader` |
| 2 | 将 `GetPackageShaderDirectory()` 创建为目录树 | 默认为 `<Project>/DShader/Packages` |
| 3 | 将**配置的**生成着色器目录创建为目录树 | 使用配置的路径，而非 `GetGeneratedShaderDirectory()` |
| 4 | 如果 `/DreamShaderGenerated` 在 `AllShaderSourceDirectoryMappings()` 中不存在，则将其映射到配置的生成着色器目录 | 绝不重新指向已存在的映射 |
| 5 | 如果通过 `IPluginManager` 找到 `DreamShader` 插件，且 `/Plugin/DreamShader` 不在映射中，则将其映射到 `<PluginBaseDir>/Shaders` | 绝不重新指向已存在的映射 |

插件的 `Shaders/` 文件夹存放 `DreamShaderBuiltins.ush`，可通过 `/Plugin/DreamShader/DreamShaderBuiltins.ush` 访问（参见 [`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md)），并且 *（自 2.1.0 起）* 存放 `Pass/` 下的 Custom Pass 着色器（`/Plugin/DreamShader/Pass/…`）。

步骤 1–3 意味着无论用户是否创作了任何内容，源目录和包目录从首次编辑器启动起就已存在。

<a id="shutdownmodule"></a>

### `ShutdownModule()`

为空。

> [!NOTE]
> `StartupModule` 注册的两个虚拟着色器目录映射**永远不会被注销**。在卸载并重新加载该模块的编辑器会话中，第二次执行时步骤 4 和步骤 5 都是空操作，挂载点会一直指向它们最初被赋予的内容。

<a id="notes"></a>

## 说明

- 此头文件包含了 `DreamShaderVersionCompat.h`，因此任何包含 `DreamShaderModule.h` 的内容也会获得那十三个版本宏。参见 [`DreamShaderVersionCompat.h`](version-compat.md)。
- 所有自由函数都不记录日志、不抛出异常、不断言。失败以回退值表示 —— 默认路径、`DreamShaderSymbol` 或 `false`。
- 此头文件不声明任何委托。编译完成的通知是 `DreamShaderCompiler` 模块中的 `OnDreamShaderSourceGenerated` —— 参见 [编译器模块](compiler-module.md)；编辑器外的工具则改为读取桥接模块的诊断文件 —— 参见 [编辑器桥接](../../tools/bridge.md)。

<a id="example"></a>

## 示例

```cpp
#include "DreamShaderModule.h"
#include "HAL/FileManager.h"

using namespace UE::DreamShader;

/** Enumerate every DreamShaderLang source under the configured source root. */
void CollectProjectSources(TArray<FString>& OutFiles)
{
    const FString Root = GetSourceShaderDirectory();

    TArray<FString> Found;
    IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("*.*"), /*Files*/ true, /*Dirs*/ false);

    for (const FString& File : Found)
    {
        if (IsDreamShaderSourceFile(File))
        {
            OutFiles.Add(NormalizeSourceFilePath(File));
        }
    }

    UE_LOG(LogDreamShader, Display, TEXT("Found %d DreamShader source(s) under %s"), OutFiles.Num(), *Root);
}
```

使用默认设置的典型输出：

```text
LogDreamShader: Found 3 DreamShader source(s) under I:/Project/DShader
```

编译不写入辅助包含文件 *（自 2.0.0 起）*：HLSL 函数成为自定义节点的代码，其调用的 HLSL 函数嵌入该代码中。`/DreamShaderGenerated` 映射仍在启动时注册，*清理生成的着色器* 仍会清空 `GetGeneratedShaderDirectory()` 中的 `*.ush` 文件。

<a id="see-also"></a>

## 另请参阅

- [C++ API](index.md) — 模块、头文件与链接
- [`DreamShaderTypes.h`](types.md) — `NormalizeSettingKey` 与已解析源数据模型
- [`DreamShaderSettings.h`](settings.md) — `SourceDirectory`、`GeneratedShaderDirectory`、`NormalizeMappingKey`
- [`DreamShaderVersionCompat.h`](version-compat.md) — 此头文件引入的宏
- [`DreamShaderCompiler`](compiler-module.md) — 从 C++ 请求编译
- [`DreamShaderLang`](lang-module.md) — 仅限 Core 的前端。`DreamShaderPreprocessor.h`、
  `DreamShaderDefineTable.h` 和 `DreamShaderDiagnostic.h` 在 2.0 中移入其中；include
  的写法没有改变。只有 `DreamShaderDefineResolution.h` —— 读取
  项目设置、引擎版本、插件描述符和命令行的那次层级合并 —— 留在了
  此模块中
- [项目设置](../settings/project.md) — 用户侧的两个目录设置
- [生成的 HLSL](../generation/generated-hlsl.md) — `/DreamShaderGenerated` 接收到的内容
- [包](../tools/packages.md) — `DShader/Packages` 目录
- [源文件](../language/source-files.md) — 三种扩展名可能包含的内容
- [`DreamShaderBuiltins.ush`](../builtins/hlsl-library.md) — 挂载在 `/Plugin/DreamShader` 的头文件
