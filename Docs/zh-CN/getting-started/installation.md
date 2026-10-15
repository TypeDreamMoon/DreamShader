---
title: "安装"
description: "把 DreamShader 插件加入 Unreal 项目、找到它的设置面板，并确认编译链路正常。"
---

DreamShaderLang 由 DreamShader 这个 Unreal 插件编译。本页的事情每个项目只需要做一次。

| | |
| --- | --- |
| 仓库 | [TypeDreamMoon/DreamShader](https://github.com/TypeDreamMoon/DreamShader) |
| 插件版本 | `2.2.0` |
| 引擎 | Unreal Engine `5.3` – `5.8`，Win64；Custom Pass 需要 `5.8` |
| 放置路径 | `<Project>/Plugins/DreamShader` |
| 插件依赖 | `WebSocketNetworking`、`SQLiteCore` —— 都是引擎插件，都会被自动启用 |

<a id="install"></a>

## 安装

<a id="copy-the-plugin-into-the-project"></a>

### 把插件复制进项目

从 GitHub release 下载 `DreamShader-<version>.zip` —— 它唯一的顶层目录就是 `DreamShader` —— 或者直接 clone 仓库。放到：

```text
MyProject/
└─ Plugins/
   └─ DreamShader/
      └─ DreamShader.uplugin
```

目录名保持 `DreamShader`；模块名、日志类别以及本文档里的所有路径都基于这个前提。

release 压缩包里有什么：

| 内容 | 说明 |
| --- | --- |
| `Source/`、`Resources/`、`DreamShader.uplugin` | 插件本身；`Resources/Templates` 是 *New ▾* 用的源文件模板 |
| `Shaders/` | `DreamShaderBuiltins.ush`（让 `/Plugin/DreamShader/…` 在压缩包安装里能解析），以及 Custom Pass 的 shader（`Pass/`） |
| `Content/`（自 2.1.2 起） | `Localization/DreamShader/` —— `.locmeta` 和 `zh-Hans` `.locres`，编辑器界面的中文翻译 |
| `Docs/`、`.skill/` | 英文完整手册；Agent skills 和它的驱动脚本 `dsc.ps1` |
| `README.md`、`README.zh-CN.md`、`CHANGELOG.md`、`LICENSE` | |

**提示：**

1.6.0 到 2.1.1 的压缩包都没有带 `Content/`，所以按压缩包安装的编辑器界面只有英文。老版本想要中文界面，从对应 tag 把 `Content/Localization/` 复制进来即可。`Tests/`、`Tools/`
和 `Config/` 不随压缩包发布。

<a id="enable-it-and-restart"></a>

### 启用并重启

在 *Edit ▸ Plugins* 中启用 **DreamShader**，然后重启编辑器。插件描述文件会自动启用 `WebSocketNetworking`
和 `SQLiteCore` —— 前者用于实时预览的 WebSocket 服务，后者支撑编辑器桥接的
`bridge.db`。两者都只被 `DreamShaderEditor` 使用。

共五个模块：`DreamShaderLang`、`DreamShader` 和 `DreamShaderPass`（Runtime，`PostConfigInit` 阶段加载），
以及 `DreamShaderCompiler` 和 `DreamShaderEditor`（Editor，`Default` 阶段）。只有两个 Editor 模块会生成资产；
`DreamShaderPass` 是 Custom Pass 的运行时，在每个引擎上都能编译，pass 只在 5.8 上运行。见
[关于 DreamShader](about.md#the-five-modules)。

<a id="confirm-the-settings-page"></a>

### 确认设置页面

打开 *Project Settings*，找到：

```text
Project Settings ▸ DreamPlugin ▸ Dream Shader
```

**警告：**

这些设置位于 **DreamPlugin** 分类下，**不是** *Plugins*。如果找不到该页面，说明插件没有
加载 —— 检查 Output Log 中的 `LogDreamShader`。Custom Pass 的设置是同一分类下的独立页面，
*DreamShader Custom Pass*。

<a id="let-the-plugin-create-its-directories"></a>

### 让插件创建其目录

在编辑器首次启动时，运行时模块会创建工作目录，无论你是否已经编写了任何
源文件：

```text
<Project>/DShader/                                        source root
<Project>/DShader/Packages/                               installed shared libraries
<Project>/Intermediate/DreamShader/GeneratedShaders/      mapped to /DreamShaderGenerated
```

`DShader` 来自 *Source Directory* 设置；`Packages` 始终是它下面的字面量子文件夹 `Packages`，
无法单独配置。`GeneratedShaders` 仍会被创建并映射，但（自 2.0.0 起）没有
编译会写入那里 —— `Function` 的 HLSL 会直接进入其 Custom 节点。在 UE 5.8 上，`DreamShaderPass` 还会在源根目录下保留 `.dreampass/` ——
即 Custom Pass HLSL 的快照，这些快照应与源文件一起纳入版本控制。

<a id="write-a-source-file-and-save-it"></a>

### 编写源文件并保存

将新源文件编写为 `.dss`：

```hlsl
// <Project>/DShader/Materials/M_InstallCheck.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

export void M_InstallCheck(inout material m)
{
    m.EmissiveColor = float3(0.2, 0.6, 1.0);
}
```

或者使用 1.x 语法，它会构建相同的资产 —— 保留这两个文件中的一个：

```c
// <Project>/DShader/Materials/M_InstallCheck.dsm
Shader(Name="Materials/M_InstallCheck")
{
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(0.2, 0.6, 1.0);
    }
}
```

在 *Auto Compile On Save* 开启（默认）的情况下，文件监视器会捕获保存操作，并在
防抖后生成材质。Output Log 会显示：

```text
Generated Material /Game/Materials/M_InstallCheck from .../M_InstallCheck.dss.
```

或者跳过手动输入：*Tools ▸ DreamShader ▸ Material Content Browser ▸ New ▾ ▸ Material (.dss)* 会从模板写入相同的
结构。

<a id="verify-the-installation"></a>

## 验证安装

| 检查 | 预期 |
| --- | --- |
| 插件已加载 | *Project Settings ▸ DreamPlugin ▸ Dream Shader* 存在 |
| 目录已存在 | 项目根目录下的 `DShader/` 和 `DShader/Packages/` |
| 源文件可编译 | Output Log 输出 `Generated Material /Game/Materials/M_InstallCheck from …` |
| 材质确实存在 | 在 *Tools ▸ DreamShader ▸ Material Content Browser* 的 Sources 模式下，该行显示 **up to date** |
| 编辑器扩展已接通 | `Saved/DreamShader/Bridge/` 包含 `status.json`、`diagnostics.json`、`material-expressions.json`、`dreamshader-builtin-catalog.json` 等 |
| 中文界面（可选） | 在编辑器语言设置为简体中文时，DreamShader 的菜单和面板已翻译 —— 需要压缩包中的 `Content/` |

**警告：**

不要指望在 Content Browser 中看到新材质。在默认的 `ThinCustom` 后端下，它是 **Ephemeral** 的 ——
仅在内存中 —— 并将自身从 Content Browser、资产选择器和保存对话框中隐藏。请参阅
[你的第一个材质](first-material.md)。

<a id="the-settings-you-will-touch-first"></a>

## 你会最先接触到的设置

完整面板有 17 个属性；以下是你第一天就会遇到的。注意配置标识符保留了
`b` 前缀，而显示名称会去掉它。

| 界面名称 | 配置属性 | 默认值 | 作用 |
| --- | --- | --- | --- |
| Source Directory | `SourceDirectory` | `DShader` | 扫描源文件的根目录。相对路径相对项目目录解析。 |
| Generated Shader Directory | `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | 虚拟目录 `/DreamShaderGenerated` 映射到哪里。自 2.0 起编译不再写 `.ush`；这里的文件是 1.x 构建留下的。 |
| Default Compiler Backend | `DefaultBackend` | `ThinCustom` | 没写 backend 的源文件使用的 backend。 |
| Show Ephemeral Materials | `bShowEphemeralMaterials` | `false` | 让 Ephemeral 材质像未保存资产一样在 Content Browser 和资产选择器里可见。 |
| Auto Compile On Save | `bAutoCompileOnSave` | `true` | 关闭后，源目录监视器完全忽略文件变化。 |
| Save Debounce Seconds | `SaveDebounceSeconds` | `0.25` | 文件变化后等待多久再编译。取值被钳制到 `[0.05, 10.0]`。 |

这些值写入**项目的** `Config/DefaultEngine.ini`，section 为 `[/Script/DreamShader.DreamShaderSettings]`，因此会被所有拉取该项目的人共享。17 项设置全部记录在
[项目设置](../settings/project.md)。

**警告：**

只有在 `/DreamShaderGenerated` 虚拟 shader 挂载尚未注册时才会读取 `GeneratedShaderDirectory`。
会话中途改这个值不会移动输出目录 —— 需要重启编辑器。

<a id="where-the-asset-lands"></a>

## 资产生成在哪里

一个 `.dss` 资产落在 `/Game` 下、文件相对源码根所在的文件夹里，名字取自它的 `export`
函数：`DShader/Materials/M_Minimal.dss` 构建 `/Game/Materials/M_Minimal`。入口上方的 `/// @name` 可以另起名字。

1.x 写法由块头决定：`Shader(Name="Materials/M_Minimal")` 解析成 `/Game/Materials/M_Minimal`。
`Name` 是相对根路径的 package path，本身不要写 `/Game` 前缀。`Root` 可选，默认
是 `Game`：

```c
Shader(Name="Materials/M_Minimal", Root="Plugin.MyPlugin")
```

这会生成 `/MyPlugin/Materials/M_Minimal`，物理路径为
`<Project>/Plugins/MyPlugin/Content/Materials/M_Minimal.uasset`。只接受 `<Project>/Plugins` 下的**项目**插件；引擎插件和商城插件会被拒绝（`DSH8090`）。完整的分派规则见
[资产路径](../generation/asset-paths.md)。

<a id="engine-compatibility"></a>

## 引擎兼容性

DreamShader `2.2.0` 基于 Unreal Engine `5.8` 开发，并在 Win64 上用单插件
`RunUAT BuildPlugin` 验证：

| Unreal Engine | 状态 |
| --- | --- |
| `5.8` | 已验证（`2.2.0`）；Custom Pass 只在这里运行 |
| `5.7` | 验证到 `1.8.0`；之后的引擎开关都按引擎自己的类型判断，5.7 不需要单独的答案 |
| `5.6` | 已验证（`2.2.0`） |
| `5.5` | 已验证（`2.2.0`） |
| `5.4` | 源码兼容 —— 见下面的工具链警告 |
| `5.3` | 源码兼容 —— 见下面的工具链警告 |

如果只想验证插件能否在某个引擎版本下编译，不必构建完整项目目标：

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

`-Package` 必须指向源码树之外的目录 —— UAT 会在那里 stage 一份干净副本。

**警告：**

**UE `5.3` 和 `5.4` 在新版 MSVC 工具链下编不过，不论你编的是什么插件。** 引擎的
`ConcurrentLinearAllocator.h` 在一处写入 `#elif __has_feature(address_sanitizer)` 时没有 `defined()` 保护，而工具链带着 ASan
头文件时这会触发 `C4668`，UBT 又以 `/we4668` 编引擎代码。失败发生在
**引擎**代码里，其中根本没有出现 `Source/DreamShader*` —— Epic 自己的 `ExampleDeviceProfileSelector` 在同一台机器上同样失败，
可以拿来确认原因。只装 MSVC `14.38` 并不能解决（它同样带
ASan 头文件）；需要一个不带这些头文件的工具链，或者给引擎头文件打一行补丁。UE 5.5 修掉了这个
头文件。

功能可用性随引擎版本变化：Substrate 需要 （自 UE 5.4 起），少数 transform 基准和参数需要
`5.5` 或 `5.6`，Custom Pass 需要 （自 UE 5.8 起）。插件二进制按引擎编译，所以把项目
升级到新引擎意味着要重新编译插件。完整清单见
[关于 DreamShader ▸ 引擎支持](about.md#engine-support)。

<a id="editor-extensions"></a>

## 编辑器扩展

语言服务不在插件里。要获得高亮、补全、诊断和预览，需要安装下面两个扩展之一：

| 编辑器 | 仓库 |
| --- | --- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support) —— 当前 2.1.0，支持 `.dss`、`.dsi`、`.dsp` 和 1.x 源文件 |
| JetBrains Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) |

*Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* 会写出 `DShader/DreamShader.code-workspace`、刷新
桥接 manifest，并用 VSCode 打开它。见 [VSCode 与 Rider](../tools/vscode.md)。

<a id="next"></a>

## 下一步

- [你的第一个材质](first-material.md) —— 写一个，然后找到它
- [项目结构](project-layout.md) —— `DShader/` 怎么组织
- [日常工作流](workflow.md) —— 保存、防抖、生成这个循环
- [关于 DreamShader](about.md) —— 这个插件由什么组成
