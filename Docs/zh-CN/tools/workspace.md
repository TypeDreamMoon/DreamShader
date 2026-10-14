---
title: "工作区和编辑器扩展"
description: "工作区和编辑器扩展"
---

<a id="workspace-and-editor-extensions"></a>

# 工作区和编辑器扩展

> [DreamShader](../index.md) » [工具](index.md) » **工作区和编辑器扩展**

一个生成的 VSCode 工作区文件，它将六个 DreamShaderLang 文件扩展名映射到 [VS Code 扩展](https://github.com/TypeDreamMoon/dreamshader-language-support) 的语言 ID，以及写入该文件并在其上启动外部编辑器的编辑器命令。

| | |
| :-- | :-- |
| 类型 | 编辑器命令 + 生成的文件 |
| 写入 | `<SourceDirectory>/DreamShader.code-workspace` |
| 命令 | *工具 ▸ DreamShader ▸ 打开 Dream Shader 工作区 (VSCode)*，以及同名的工具栏按钮 |
| 平台 | 仅限 Windows — 发现过程使用 Windows 环境变量、以 `;` 分隔的 `PATH`、`cmd.exe` 和 `notepad.exe` |
| 自 | `1.2.1` |

<a id="synopsis"></a>

## 概要

```text
<SourceDirectory>/DreamShader.code-workspace
```

`<SourceDirectory>` 是 *Source Directory* 项目设置，默认为 `<Project>/DShader`。如果该目录不存在，则会创建它。文件以不带 BOM 的 UTF-8 写入，由 Unreal 的 JSON 写入器美化打印（制表符缩进）：

```json
{
	"folders": [
		{
			"name": "DreamShader Source",
			"path": "."
		},
		{
			"name": "Plugin: MoonToon",
			"path": "../Plugins/MoonToon/DShader"
		}
	],
	"settings": {
		"files.associations": {
			"*.dss": "dreamshader-dss",
			"*.dsi": "dreamshader-dss",
			"*.dsp": "dreamshader-dsp",
			"*.dsm": "dreamshaderlang",
			"*.dsh": "dreamshaderlang",
			"*.dsf": "dreamshaderlang"
		}
	}
}
```

每个源根对应一个 `folders` 条目：项目根始终位于第一位，并且始终为 `"."`（工作区文件就位于其中），然后每个附带了 `DShader` 文件夹的插件对应一个条目，命名为 `Plugin: <Name>`，并指向相对于工作区文件的路径。没有相对形式的根 — 位于不同驱动器上的引擎插件 — 则改为使用其绝对路径。请参阅[源文件](../language/source-files.md#source-roots)。

<a id="contents"></a>

## 内容

写入器输出的每一个键。没有其他键；只有 `folders` 条目的数量会变化。

| 键 | 值 | 用途 |
| :-- | :-- | :-- |
| `folders[0].name` | `DreamShader Source` | 项目根文件夹的显示名称。 |
| `folders[0].path` | `.` | 包含工作区文件的文件夹，即 `<SourceDirectory>` 本身。 |
| `folders[n].name`、`folders[n].path` | `Plugin: <Name>`，相对于工作区文件的路径（没有相对路径时为绝对路径） | 每个插件源根一个，按根顺序排列。 |
| `settings["files.associations"]["*.dss"]` | `dreamshader-dss` | 2.0 编译单元的语言 ID *（自 2.1.0 起）*。 |
| `settings["files.associations"]["*.dsi"]` | `dreamshader-dss` | 材质实例源的语言 ID *（自 2.1.0 起）*。 |
| `settings["files.associations"]["*.dsp"]` | `dreamshader-dsp` | Custom Pass 管线的语言 ID *（自 2.1.0 起）*。如果没有它，`.dsp` 会被其他知道该扩展名的东西占用——Faust 语言和旧的 Visual C++ 项目也使用它。 |
| `settings["files.associations"]["*.dsm"]` | `dreamshaderlang` | 材质源的语言 ID。 |
| `settings["files.associations"]["*.dsh"]` | `dreamshaderlang` | 头文件的语言 ID。 |
| `settings["files.associations"]["*.dsf"]` | `dreamshaderlang` | 函数源的语言 ID *（自 1.3.5 起）*。 |

三个语言 ID，即扩展注册的那些：`dreamshaderlang` 用于 1.x 源和头文件，`dreamshader-dss` 用于 2.0 源，`dreamshader-dsp` 用于管线（扩展 2.1.0 及更高版本）。工作区关联会覆盖扩展自身将扩展名映射到语言的映射，因此两者必须一致：如果对 `.dss` 使用 `dreamshaderlang`，VS Code 会为它使用 1.x 语法和语言服务器。作用域为某个语言的设置——`"[dreamshader-dss]": { ... }`——使用相同的 ID。

> [!WARNING]
> 每次调用 *打开 Dream Shader 工作区* 时，该文件都会**从头重写**。写入器序列化一个固定对象；它从不读取、合并或保留现有文件。任何手动添加的 `launch`、`tasks`、`extensions` 或额外的 `settings` 条目都会在该命令下次运行时被销毁。请将每用户工作区配置保存在不同的 `.code-workspace` 文件中或 `<SourceDirectory>/.vscode/settings.json` 中，此命令不会触及这两者。

<a id="open-behaviour"></a>

## 打开行为

该命令按以下顺序执行五个步骤：

| 步骤 | 操作 | 失败时 |
| :-- | :-- | :-- |
| 1 | 重新导出 `material-expressions.json` | 记录日志，命令继续 |
| 2 | 重新导出 `settings.json`，并在其后立即重新导出 [`pass-keys.json`](#pass-keysjson) *（自 2.1.0 起）* | 记录日志，命令继续 |
| 3 | 重新导出 `substrate-builtins.json` | 记录日志，命令继续 |
| 4 | 写入 `DreamShader.code-workspace` | 弹出提示 + 警告，命令**中止** |
| 5 | 在工作区文件上启动编辑器（回退链见下文） | 弹出提示 + 警告 |

步骤 1–3 与 [bridge](bridge.md#manifests) 在编辑器启动时写入的三个清单相同；打开工作区会刷新它们，因此刚刚安装的扩展无需重启编辑器即可看到当前数据。

<a id="launch-fallback-chain"></a>

### 启动回退链

第一个成功的机制胜出；不会尝试其余的。

| 顺序 | 机制 | 详细信息 |
| :-- | :-- | :-- |
| 1 | VSCode | 第一个发现的能产生有效进程句柄的可执行文件。会通过 `%ComSpec%`（回退到 `C:/Windows/System32/cmd.exe`）以 `/C`、隐藏方式运行 `.cmd` / `.bat` 候选；`.exe` 候选则直接启动。 |
| 2 | Shell 默认应用程序 | 使用 `Edit` 谓词调用 `FPlatformProcess::LaunchFileInDefaultExternalApplication`——即注册用于 `.code-workspace` 的任何程序。 |
| 3 | 记事本 | 如果存在 `%SystemRoot%\System32\notepad.exe`，则使用它，否则使用裸 `notepad.exe`。 |
| — | *（无）* | 失败提示以及日志中的警告。 |

<a id="vscode-executable-discovery"></a>

### VSCode 可执行文件发现

按此确切顺序探测。仅保留作为文件存在的路径，并丢弃重复项。

| 顺序 | 候选 |
| :-- | :-- |
| 1 | `%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe` |
| 2 | `%LOCALAPPDATA%\Programs\Microsoft VS Code\bin\code.cmd` |
| 3 | `%LOCALAPPDATA%\Programs\Microsoft VS Code Insiders\Code - Insiders.exe` |
| 4 | `%LOCALAPPDATA%\Programs\Microsoft VS Code Insiders\bin\code-insiders.cmd` |
| 5 | `%ProgramFiles%\Microsoft VS Code\Code.exe` |
| 6 | `%ProgramFiles%\Microsoft VS Code\bin\code.cmd` |
| 7 | `%ProgramFiles(x86)%\Microsoft VS Code\Code.exe` |
| 8 | `%ProgramFiles(x86)%\Microsoft VS Code\bin\code.cmd` |
| 9 | 对于每个以 `;` 分隔的 `PATH` 条目，按 `PATH` 顺序：`code.cmd`、`code.exe`、`Code.exe`、`code-insiders.cmd`、`Code - Insiders.exe` |

没有用于指定 VSCode 可执行文件的设置。非标准安装只能通过将其放入 `PATH` 来访问。

<a id="the-reuse-window-setting"></a>

## 重用窗口设置

| | |
| :-- | :-- |
| 设置 | *Open In New Window* — `bOpenInNewWindow`，类别 *Editor* |
| 默认 | `true` |
| 效果 | 当为 **false** 时，会将 ` --reuse-window` 追加到 VSCode 命令行。当为 true 时，不传递任何标志，VSCode 应用其自身的默认值。 |

> [!NOTE]
> `bOpenInNewWindow` 仅由工作区启动器查询。所有其他在 VSCode 中打开文件的 DreamShader 操作都使用单独的启动器，该启动器始终传递 `--reuse-window -g "<path>:<line>:<column>"`，无论该设置如何。开启 *Open In New Window* 不会让 *Open source*、*OpenVirtualFunction* 或导出后打开使用新窗口。

| 操作 | 启动器 | 窗口行为 |
| :-- | :-- | :-- |
| *打开 Dream Shader 工作区 (VSCode)*（菜单和工具栏） | 工作区启动器 | 遵循 `bOpenInNewWindow` |
| *Open source*（材质内容浏览器） | 文件启动器 | 始终 `--reuse-window` |
| *OpenVirtualFunction*（资源上下文菜单） | 文件启动器 | 始终 `--reuse-window`，定位到声明所在的行和列 |
| *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* 导出后打开 | 首选编辑器链 | 使用 VSCode 时始终 `--reuse-window` |

文件启动器会将行和列限制为 `1` 或更大。其自身的回退链为 VSCode → shell 默认应用程序（`Edit` 谓词）→ 记事本，与工作区链形态相同。

<a id="diagnostics"></a>

## 诊断

运行时替换项显示为 `{Placeholder}`。提示文本和日志文本不同；两者均已列出。

| 提示 | 日志 | 原因 |
| :-- | :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | `Warning` — `Failed to create DreamShader workspace: {Error}` | 工作区文件无法写入；`{Error}` 是下面三个写入器错误之一 |
| `Opened DreamShader workspace in VSCode: {Path}` | `Display` — 相同文本 | 启动了 VSCode 候选程序 |
| `Opened DreamShader workspace: {Path}` | `Display` — `Opened DreamShader workspace with the default editor: {Path}` | 启动了系统默认应用程序 |
| `Opened DreamShader workspace in Notepad: {Path}` | `Display` — 相同文本 | 启动了记事本 |
| `DreamShader could not open workspace: {Path}` | `Warning` — `Failed to open DreamShader workspace: {Path}` | 链中的每个机制都失败了；文件仍被写入 |

写入器错误，替换到上面的 `{Error}` 中：

| 消息 | 原因 |
| :-- | :-- |
| `DreamShader source directory is empty.` | 解析出的源目录规范化为空字符串 |
| `Failed to create DreamShader source directory '{Path}'.` | 源目录不存在且无法创建 |
| `Failed to write DreamShader workspace file '{Path}'.` | 文件无法保存（只读、被锁定、空间不足） |

所有日志都记录到 `LogDreamShader` 类别。

<a id="editor-extensions"></a>

## 编辑器扩展

语言工具链位于两个仓库中，**两者都不随本插件提供**。插件提供工作区文件、文件关联以及[桥接](bridge.md)构件；下面的一切——语法、补全、导航、包命令——都由扩展实现。

| 编辑器 | 仓库 | 提供 |
| :-- | :-- | :-- |
| VSCode | <https://github.com/TypeDreamMoon/dreamshader-language-support> | 语法高亮、代码片段、补全、转到定义、查找引用、悬停、签名帮助、本地诊断、Unreal 桥接诊断、包命令、快速模板。 |
| JetBrains Rider | <https://github.com/tsdaer/dreamshader-language-support> | `.dsm` / `.dsf` / `.dsh` 文件类型、语法和 PSI 解析、高亮、补全、导航、诊断、Unreal Bridge 集成、语义标记、内嵌提示、包工具。 |

扩展端的设置（例如项目根目录覆盖、预览帧率或包存储索引 URL）由扩展声明，而非由 `UDreamShaderSettings` 声明，并记录在扩展自己的仓库中。发布工作流会将最新的 VSCode 扩展资源附加到每个插件 GitHub 发布中；参见[发布](../contributing/release.md)。

<a id="what-an-extension-consumes"></a>

### 扩展消费的内容

扩展读取或写入的每个构件都位于 `<Project>/Saved/DreamShader/Bridge/` 下，外加回环 WebSocket 端点。完整架构参见[编辑器桥接](bridge.md)。

| 构件 | 方向 | 内容 |
| :-- | :-- | :-- |
| `Requests/*.json` | 扩展 → 编辑器 | 重新编译、清理和一次性预览命令；参见[请求文件](bridge.md#request-files)。 |
| `diagnostics.json` | 编辑器 → 扩展 | 所有当前诊断，按源文件分组。 |
| `diagnostics/index.json` + `diagnostics/<md5>.json` | 编辑器 → 扩展 | 相同数据按文件分片，用于增量读取。 |
| `bridge.db` | 编辑器 → 扩展 | 诊断和三个清单的 SQLite 镜像。 |
| `material-expressions.json` | 编辑器 → 扩展 | 反射的 `UMaterialExpression` 目录，用于 `UE.Expression` 补全 *（自 1.2.10 起）*。 |
| `settings.json` | 编辑器 → 扩展 | `ShadingModel` / `BlendMode` / `MaterialDomain` 别名表。 |
| `substrate-builtins.json` | 编辑器 → 扩展 | `Substrate.*` 内置目录及代码片段；在 UE 5.4 以下为 `supported: false`。 |
| `pass-keys.json` | 编辑器 → 扩展 | `.dsp` 可以说什么，用于补全；在 UE 5.8 以下为 `supported: false` — [见下文](#pass-keysjson) *（自 2.1.0 起）*。 |
| `preview.json` + `Preview/*.png` | 编辑器 → 扩展 | 一次性预览的结果清单和图像。 |
| `ws://127.0.0.1:17864` | 双向 | 带轨道控制的流式预览；参见[WebSocket 协议](bridge.md#websocket-server)。 |

<a id="pass-keysjson"></a>

### `pass-keys.json`

*（自 2.1.0 起）* [`.dsp`](../language-v2/passes.md) 的词汇表，用于编辑器补全：
`<Project>/Saved/DreamShader/Bridge/pass-keys.json`，架构 `DreamShader.PassKeys`，版本 `1`，带 `generatedAt` 时间戳。它使用 `settings.json` 写入——在桥接启动时以及 *Open Dream Shader Workspace* 时——因为它结尾的层名是项目设置。它没有 `bridge.db` 表。

| 键 | 内容 |
| :-- | :-- |
| `supported` | 在 UE 5.8 及更高版本上为 `true`。在此以下，`false` 带 `unsupportedReason`：`Custom Pass pipelines run on Unreal Engine 5.8 and later.` —— `.dsp` 仍会在那里解析和绑定，且没有任何东西运行它 |
| `injectionPoints` | 每个注入点，按帧到达它们的顺序：`name`、`order`、`postProcess` 和 `resolution`（`Render` 或 `Output`） |
| `pipelineKeys` | `#pragma pipeline` 键：`Order`、`Injection`、`Views`、`Requires`、`Enabled` |
| `bufferFormats` · `bufferKeys` · `bufferResolutions` | `R8` … `Depth32`；`Scale`、`Size`、`Resolution`、`Clear`、`Mips`、`History`、`Export`；`Render`、`Output` |
| `passKinds` | `fullscreen`、`compute`、`mesh`、`clear`、`copy`，每个都有自己的 `keys` 和一个 `detail` |
| `commonPassKeys` | `Injection` 和 `Enabled`，每种类型都接受它们 |
| `bindingStatements` | `read`、`write`、`param` |
| `filterKinds` | `Stencil`、`Layer`、`List`，每个都带编辑器插入的 `snippet`（`Stencil(${1:1})`） |
| `meshModes` · `depthModes` · `cullModes` · `blendModes` · `nanitePolicies` · `meshUsages` · `viewFlags` · `requirementFlags` · `parameterTypes` | 这些值各自采用的拼写 |
| `builtinBuffers` | `SceneColor`、`SceneDepth`、`CustomDepth`、`CustomStencil`、`GBufferA` … `GBufferF`、`Velocity`、`Translucency` |
| `weightParameter` | `DreamPassWeight` |
| `layerNames` | 项目的通道层，按位顺序——`Layer(...)` 采用的名称 |

每个键条目都是 `name`、`value`（值的类型，它命名补全来源的列表——`injection` 对应 `injectionPoints`，……）、`default`（省略该键意味着什么）、`required`（没有默认值时为 true）以及 `detail`。

<a id="example"></a>

## 示例

在默认项目上运行 *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* 会触及：

```text
<Project>/DShader/DreamShader.code-workspace                    rewritten
<Project>/Saved/DreamShader/Bridge/material-expressions.json    rewritten
<Project>/Saved/DreamShader/Bridge/settings.json                rewritten
<Project>/Saved/DreamShader/Bridge/pass-keys.json               rewritten (since 2.1.0)
<Project>/Saved/DreamShader/Bridge/substrate-builtins.json      rewritten
<Project>/Saved/DreamShader/Bridge/bridge.db                    tables replaced
```

然后，对于一个 `code.cmd` 候选程序，在 *Open In New Window* 保持默认值的情况下，启动：

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"  "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

在关闭 *Open In New Window* 的情况下，同一行会带上额外标志：

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd" --reuse-window "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

<a id="see-also"></a>

## 另见

- [编辑器桥接](bridge.md) — 扩展所驱动的产物、请求文件和 WebSocket 协议
- [包](packages.md) — `DShader/Packages`，以及扩展拥有包系统的哪一半
- [编辑器集成](editor-integration.md) — 调用此命令的“工具”菜单和工具栏条目
- [材质内容浏览器](material-browser.md) — 使用文件启动器的*打开源*操作
- [VirtualFunction 工具](virtual-function-tools.md) — *OpenVirtualFunction* 及其行/列跳转
- [反编译器](decompiler.md) — *导出*条目及导出后打开
- [项目设置](../settings/project.md) — `SourceDirectory` 和 `bOpenInNewWindow`
- [源文件](../language/source-files.md) — `.dsm`、`.dsh` 和 `.dsf` 各自可以包含的内容
- [自定义 Pass 管线 — `.dsp`](../language-v2/passes.md) — `pass-keys.json` 所描述的语言
- [发布](../contributing/release.md) — 扩展资源如何附加到插件发布版本
