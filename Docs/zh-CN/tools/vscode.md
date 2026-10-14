---
title: "VSCode 与 Rider"
description: "语言扩展、生成的 DreamShader.code-workspace，以及两侧交换的 bridge 产物。"
---

DreamShaderLang 是**两个相互独立的产品**：Unreal 插件，和你编辑器里的语言扩展。它们从不互相直接调用 —— 交换发生在
`<Project>/Saved/DreamShader/Bridge/` 下的文件和一条回环 WebSocket 上。出问题时，先分清哪边负责什么能省很多时间。

| 一侧 | 负责 |
| :-- | :-- |
| Unreal 插件 | 编译、诊断、bridge 产物、预览渲染器、`DreamShader.code-workspace` |
| 编辑器扩展 | 语法高亮、补全、Hover、导航、预览面板及其相机、package 安装/更新、package manifest 和锁文件 |

<a id="which-side-implements-what"></a>

## 哪边实现了什么

| 面 | 实现方 | 说明 |
| :-- | :-- | :-- |
| 菜单、工具栏、右键菜单、浏览器面板 | 插件 | [编辑器工具](editor-integration.md) |
| 保存即编译、诊断存储 | 插件 | 写三个诊断 sink |
| 预览渲染（PNG 帧） | 插件 | 渲染器、网格集合和各种钳制 |
| 预览相机控制、pitch 钳制、帧确认 | **扩展** | 插件自己不做任何 pitch 钳制 |
| 反编译器 | 插件 | [反编译导出](decompiler.md) |
| `.dsm` / `.dsh` / `.dsf` 与 `.dss` / `.dsi` / `.dsp` 的高亮、补全、Hover | **扩展** | 数据来自插件导出的 manifest、符号索引和内建目录 |
| `DreamShader.code-workspace` | 插件 | 每次 *Open Dream Shader Workspace* 都重写 |
| `dreamshader.package.json`、`dreamshader.lock.json`、安装/更新命令 | **扩展** | **插件里没有任何 C++ 读这两个文件** —— [Package](packages.md) |
| `DShader/Packages` 创建、import 解析、自动编译排除 | 插件 | [Package](packages.md) |

<a id="the-extensions"></a>

## 两个扩展

两个扩展都不随插件一起发布，需要单独安装。

| 编辑器 | 仓库 | 提供 |
| :-- | :-- | :-- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support) | 语法高亮、snippets、补全、Go to Definition、Find References、Hover、Signature Help、本地诊断、Unreal 桥接诊断、Package 命令、快速模板、材质预览面板；`.dsp` 支持自 2.1.0 起、2.0 头文件、`#include` 和 `BL::` 补全自 2.2.0 起 |
| JetBrains Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` 文件类型、语法与 PSI 解析、高亮、补全、导航、诊断、Unreal Bridge 集成、语义 token、inlay hints、Package 工具 |

| 能力 | VSCode | Rider |
| :-- | :-- | :-- |
| 文件类型识别 | 有 | 有 |
| 语法高亮 | 有 | 有 |
| 解析模型 | parser-based 语言服务 | JetBrains PSI 解析器 |
| 补全、Hover、Signature Help | 有 | 有 |
| 跳转定义、查找引用 | 有 | 有 |
| 本地诊断 | 有 | 有 |
| Unreal 桥接诊断 | 有 | 有 |
| Inlay hints / 语义 token | 有 | 有 |
| 材质预览面板 | 有 | 取决于插件 |
| Package 命令 | 有 | 取决于插件 |
| 创作模板 | 有 | 取决于插件 |

发布流程会把最新的 VSCode 扩展产物附到每个插件 GitHub release 上，所以两边版本大致同步。
扩展侧的设置 —— 项目根目录覆盖、预览帧率、package store index URL —— 由扩展声明，不属于 `UDreamShaderSettings`，
以扩展自己的仓库为准。

<a id="the-generated-workspace"></a>

## 生成的 workspace

*Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* —— 以及同名的工具栏按钮 ——
会写一个文件并用编辑器打开它。（自 1.2.1 起）

| 项目 | 值 |
| :-- | :-- |
| 路径 | `<SourceDirectory>/DreamShader.code-workspace` —— 默认 `<Project>/DShader/` |
| 编码 | UTF-8 无 BOM，由 Unreal 的 JSON writer 美化输出（tab 缩进） |
| 平台 | **仅 Windows** —— 探测用的是 Windows 环境变量、`;` 分隔的 `;`、`PATH`、`cmd.exe` 和 `notepad.exe` |

源目录不存在时会被创建。文件的样子：

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

每个源码根一个 `folders` 条目：工程根永远在第一个、永远是 `"."`（workspace
文件就在里面），然后每个带 `DShader` 文件夹的插件一个，名为 `Plugin: <Name>`，
路径相对 workspace 文件（没有相对形式时 —— 比如另一个盘上的引擎插件 —— 写绝对路径）。
writer 会写出的所有键如下，没有别的键；变化的只有 `folders` 条目的个数。

| 键 | 值 | 用途 |
| :-- | :-- | :-- |
| `folders[0].name` | `DreamShader Source` | 工程根文件夹的显示名 |
| `folders[0].path` | `.` | 存放 workspace 文件的目录 —— 也就是 `<SourceDirectory>` 本身 |
| `folders[n].name`、`folders[n].path` | `Plugin: <Name>`，相对 workspace 文件的路径 | 每个插件源码根一个，按根的顺序 |
| `settings["files.associations"]["*.dss"]` | `dreamshader-dss` | 2.0 编译单元的 language id（自 2.1.0 起） |
| `settings["files.associations"]["*.dsi"]` | `dreamshader-dss` | 材质实例源文件的 language id（自 2.1.0 起） |
| `settings["files.associations"]["*.dsp"]` | `dreamshader-dsp` | Custom Pass 管线的 language id（自 2.1.0 起）。没有它，`.dsp` 会被别的认这个扩展名的东西抢走 —— Faust 语言和老的 Visual C++ 工程都用它 |
| `settings["files.associations"]["*.dsm"]` | `dreamshaderlang` | 材质源文件的 language id |
| `settings["files.associations"]["*.dsh"]` | `dreamshaderlang` | 头文件的 language id |
| `settings["files.associations"]["*.dsf"]` | `dreamshaderlang` | 函数源文件的 language id（自 1.3.5 起） |

三个 language id 就是扩展注册的那三个：1.x 源文件和头文件用 `dreamshaderlang`，2.0 源文件用 `dreamshader-dss`，
管线用 `dreamshader-dsp`（扩展 2.1.0 及以上）。workspace 里的关联会覆盖扩展自己的扩展名映射，所以两边必须一致：
给 `dreamshaderlang` 写 `.dss`，VS Code 就会用 1.x 的语法和语言服务打开它。按语言限定的设置 ——
`"[dreamshader-dss]": { ... }` —— 用同样的 id。

**警告：**

**这个文件每次调用都被整体重写。** writer 序列化的是一个固定对象；它从不读取、合并或保留原有内容。
你手工加的任何 `launch`、`tasks`、`extensions` 或额外 `settings` 条目，
都会在下次执行该命令时被摧毁。把个人配置放在**另一个** `.code-workspace` 文件里，或者放进 `<SourceDirectory>/.vscode/settings.json`
—— 这两处这条命令都不碰。

<a id="what-the-command-does-in-order"></a>

### 这条命令的执行顺序

| 步骤 | 动作 | 失败时 |
| :-- | :-- | :-- |
| 1 | 重新导出 `material-expressions.json` | 记日志，命令继续 |
| 2 | 重新导出 `settings.json`，紧接着导出 `pass-keys.json`（自 2.1.0 起） | 记日志，命令继续 |
| 3 | 重新导出 `substrate-builtins.json` | 记日志，命令继续 |
| 4 | 写 `DreamShader.code-workspace` | toast + warning，命令**中止** |
| 5 | 用编辑器打开该 workspace 文件 | toast + warning |

第 1–3 步重写的就是 bridge 在编辑器启动时写的那三份 manifest，所以刚装好的扩展不用重启编辑器就能拿到当前数据。
见 [Bridge 产物](bridge.md)。

<a id="launch-fallback-chain"></a>

### 启动回退链

第一个成功的机制胜出，其余不再尝试。

| 顺序 | 机制 | 细节 |
| :-- | :-- | :-- |
| 1 | VSCode | 第一个能拿到有效进程句柄的已发现可执行文件。`.cmd` / `.bat` 候选经由 `%ComSpec%`（回退到 `C:/Windows/System32/cmd.exe`）带 `/C` 隐藏运行；`.exe` 候选直接启动 |
| 2 | Shell 默认应用 | `LaunchFileInDefaultExternalApplication` 加 `Edit` 动词 —— 系统为 `.code-workspace` 注册的那个程序 |
| 3 | Notepad | 存在时用 `%SystemRoot%\System32\notepad.exe`，否则用裸 `notepad.exe` |
| — | *（都没有）* | 失败 toast，以及日志里的一条 warning |

<a id="vscode-executable-discovery"></a>

### VSCode 可执行文件探测

按下列精确顺序探测。只保留确实作为文件存在的路径，并去重。

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
| 9 | 对每个 `;` 分隔的 `PATH` 条目，按 `PATH` 顺序：`code.cmd`、`code.exe`、`Code.exe`、`code-insiders.cmd`、`Code - Insiders.exe` |

**提示：**

**没有任何设置可以指定 VSCode 可执行文件。** 非标准安装位置只能通过把它放进 `PATH` 来触达。

<a id="which-launcher-an-action-uses"></a>

### 各操作用的是哪个 launcher

| 项目 | 值 |
| :-- | :-- |
| 设置项 | *Open In New Window* —— `bOpenInNewWindow`，分类 *Editor* |
| 默认 | `true` |
| 效果 | 为 **false** 时，VSCode 命令行后面追加 ` --reuse-window`。为 true 时不传任何标志，由 VSCode 自己决定 |

**警告：**

`bOpenInNewWindow` **只被 workspace launcher 读取**。其他每一个用 VSCode 打开文件的 DreamShader 操作
都走另一个 launcher，那个 launcher 无视该设置，总是传
`--reuse-window -g "<path>:<line>:<column>"`。

| 操作 | Launcher | 窗口行为 |
| :-- | :-- | :-- |
| *Open Dream Shader Workspace (VSCode)*，菜单和工具栏 | workspace launcher | 遵循 `bOpenInNewWindow` |
| *Open source*（Material Content Browser） | file launcher | 总是 `--reuse-window` |
| *OpenVirtualFunction*（资产右键菜单） | file launcher | 总是 `--reuse-window`，并定位到声明的行列 |
| *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* 导出后的打开 | 首选编辑器链 | 用 VSCode 时总是 `--reuse-window` |

file launcher 会把行列钳到 `1` 或更大，它自己的回退链是
VSCode → Shell 默认应用（`Edit` 动词）→ Notepad，和 workspace 链形状一致。

<a id="diagnostics"></a>

### 诊断

toast 文本和日志文本不同，两者都列出。运行期替换写作 `{Placeholder}`。

| Toast | 日志 | 原因 |
| :-- | :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | Warning —— `Failed to create DreamShader workspace: {Error}` | workspace 文件写不了；`{Error}` 是下面三个 writer 错误之一 |
| `Opened DreamShader workspace in VSCode: {Path}` | Display —— 同文本 | 某个 VSCode 候选启动了 |
| `Opened DreamShader workspace: {Path}` | Display —— `Opened DreamShader workspace with the default editor: {Path}` | Shell 默认应用启动了 |
| `Opened DreamShader workspace in Notepad: {Path}` | Display —— 同文本 | Notepad 启动了 |
| `DreamShader could not open workspace: {Path}` | Warning —— `Failed to open DreamShader workspace: {Path}` | 所有机制都失败；文件仍然写出来了 |

| Writer 错误 | 原因 |
| :-- | :-- |
| `DreamShader source directory is empty.` | 解析出的源目录归一化后是空串 |
| `Failed to create DreamShader source directory '{Path}'.` | 源目录不存在且创建失败 |
| `Failed to write DreamShader workspace file '{Path}'.` | 文件保存失败 —— 只读、被占用、磁盘满 |

<a id="what-an-extension-consumes"></a>

## 扩展消费什么

扩展读写的每个产物都在 `<Project>/Saved/DreamShader/Bridge/` 下，加上那个回环 WebSocket 端点。

| 产物 | 方向 | 内容 |
| :-- | :-- | :-- |
| `Requests/*.json` | 扩展 → 编辑器 | 重编译、清理、单次预览和 `reveal-node` 命令 |
| `diagnostics.json` | 编辑器 → 扩展 | 当前全部诊断，按源文件分组 |
| `diagnostics/index.json` + `diagnostics/<md5>.json` | 编辑器 → 扩展 | 同一份数据按文件分片，便于增量读取 |
| `bridge.db` | 编辑器 → 扩展 | 诊断和三份 manifest 的 SQLite 镜像 |
| `material-expressions.json` | 编辑器 → 扩展 | 反射得到的 `UMaterialExpression` 目录，用于 [`UE.Expression`](../builtins/ue-expression.md) 补全（自 1.2.10 起） |
| `settings.json` | 编辑器 → 扩展 | `ShadingModel` / `BlendMode` / `MaterialDomain` 别名表 |
| `substrate-builtins.json` | 编辑器 → 扩展 | 带 snippet 的 [`Substrate.*`](../builtins/substrate.md) 目录；UE 5.4 以下 `supported: false` |
| `pass-keys.json`（自 2.1.0 起） | 编辑器 → 扩展 | `.dsp` 能写什么，供补全：注入点、管线 / buffer / pass 的键、各种取值、工程的 pass 层名；UE 5.8 以下 `supported: false`。没有 `bridge.db` 表 |
| `dreamshader-builtin-catalog.json`（自 2.0.0 起） | 编辑器 → 扩展 | 绑定器自己的节点目录 —— `UE.` / `Substrate.` 补全的来源；`dsc export-catalog` 也写它 |
| `../Index/<root>/<source>.index.json`（自 2.0.0 起） | 编辑器 → 扩展 | 每个源文件的符号索引，每次编译（失败的也算）之后刷新 |
| `preview.json` + `Preview/*.png` | 编辑器 → 扩展 | 单次预览的结果清单和图片 |
| `status.json` | 编辑器 → 扩展 | 心跳与身份 —— **文件缺失就是「编辑器没在运行」**（自 1.6.0 起） |
| `Responses/<requestId>.json` | 编辑器 → 扩展 | 带 `requestId` 的请求的回复（自 1.6.0 起） |
| `owner.lock` | 编辑器内部 | 哪个编辑器持有本工程的 bridge（自 1.8.0 起） |
| `ws://127.0.0.1:17864` | 双向 | 带轨道控制和断点的流式预览 |

**警告：**

`bridge.db` 从插件这一侧看是 **只写** 的，而且不是持久状态：bridge 启动时 *和* 关闭时都会删掉它，
每个写入者都在事务里整表替换。插件里没有任何东西读回过一行。把它当作 JSON 的一个便于查询的镜像，
只在编辑器运行期间有效 —— 绝不要拿它存客户端状态。

<a id="is-the-editor-alive"></a>

### 编辑器还活着吗

（自 1.6.0 起）

在此之前，客户端想知道有没有编辑器在跑，唯一的办法是找 `bridge.db` 然后祈祷。
现在 bridge 每 2 秒重写一次 `status.json`，并在 **退出时删除** 它 —— 所以文件缺失确定地意味着「没在运行」，
客户端可以立刻回退，而不必等一个存活窗口过期。

| 字段 | 说明 |
| :-- | :-- |
| `protocol`、`pid`、`project`、`projectDir`、`engineDir`、`pluginVersion` | 身份 |
| `busy` | 正在服务某个请求时为 true |
| `busyAction` | 只在 busy 时出现 |
| `lastResult` | 有东西完成过之后出现 |
| `heartbeatUtc` | ISO-8601 UTC |

**警告：**

判断心跳是否过期 **之前** 先读 `busy`。编译会阻塞游戏线程，所以编译期间心跳是停的 ——
只看时间戳的客户端会把每一次真实编译都读成崩溃。
剩下的歧义是硬崩溃，它会把文件留在原地；记录下来的 `pid` 用来收尾。

**提示：**

没人在听的时候写下的请求会被 **丢弃** 而不是排队。分界线是 bridge 自己的启动时间，
所以一个在编辑器启动时就躺在磁盘上的请求会被删掉并记一条日志，而不是被迟到地服务 —— 到那时发它的人早就超时了。
写请求之前先看 `status.json`。

<a id="request-files"></a>

### 请求文件

| 项目 | 值 |
| :-- | :-- |
| 目录 | `<Project>/Saved/DreamShader/Bridge/Requests/` |
| 发现方式 | `*.json`，只看文件，**不递归** |
| 轮询间隔 | `0.1 s` |
| 消费方式 | 每个发现的文件都在该轮循环结束时**被删除** |

文件名无关紧要，只有 JSON 内容有意义。`action` 和 `scope` 都做大小写不敏感匹配。

| `action` | 必需字段 | 效果 | 何时回复 |
| :-- | :-- | :-- | :-- |
| `ping` （自 1.6.0 起） | —— | 什么都不做，只确认 bridge 在服务 | 立即 |
| `recompile` | `scope: "all"` | 重建依赖图，把每个源码根的每个源文件 —— `.dsm`、`.dsf`、`.dss`、`.dsi`、`.dsp`，某源码根 `Packages` 下的 `.dsm` 除外 —— 强制排入队列 | 立即，作为 *queued* |
| `recompile` | `scope: "file"`、`sourceFile` | 把一个文件强制排进防抖队列 | **那次编译完成时** |
| `cleanGeneratedShaders` | —— | 删掉生成的 `*.ush` include，然后把一次全量重扫排入队列 | 立即 |
| `previewMaterial` | `sourceFile` | 同步渲染一次预览并写 `preview.json`；`sourceFile` 是 `.dss`、`.dsi` 或 `.dsm` | 立即，带渲染结果 |
| `reveal-node` （自 2.0.0 起） | `file`、`line` | 找到那一行生成的节点，打开它所属资产的材质编辑器并选中它 | 立即，带资产路径和表达式 GUID |

带 `requestId` 的请求会在 `Responses/<requestId>.json` 里被回答 （自 1.6.0 起），字段是
`protocol`、`requestId`、`ok`、`durationMs`、`message` 和 `diagnostics`。写入是原子的 ——
先写在目标旁边，再改名 —— 所以「出现」和「完整」是同一个事件；上个会话遗留的回复
在启动时被删掉。

**警告：**

`recompile` 配 `scope: "file"` 是唯一回复被**延迟**的动作。文件进防抖队列，
若干 tick 之后才编译，所以在派发时回答就等于在什么都还没试之前报告成功。它的 id 被寄存下来，
由那次编译本身回答，并带上那次编译的诊断。

`scope: "all"` 正相反，并且明说：它立即以 *queued* 回复，因为批次跨多个 tick 才排干，
结果此刻不可知。**别把它当成结果。**

**提示：**

错误的 `scope`、缺失的 `sourceFile` 和无法识别的 `action` 现在是**错误响应**，
而不再是静默无操作 —— 前提是请求带了 `requestId`。不带 id 时行为不变：文件被删掉，什么都不说。
这正是当前 VSCode 扩展发出的每一个请求的样子，也是它无需改动仍然能工作的原因。

请求在**派发之前**就被删除，所以一个能弄崩编辑器的请求不会在之后每次启动时被重放。唯一的例外是
完全读不出来的文件：那几乎总是一个还在被写入的文件，所以留给下一轮，而不是丢掉。仍然建议把 JSON
写到别处的临时名字上、再**重命名**进 `Requests/`，让它原子地出现。

请求可以带 `protocol`。**缺失**这个字段会被读成当前版本而不是版本不匹配
—— 当前 VSCode 扩展发出的每个请求都不带它，拒绝这些请求会让每台装了它的机器都用不了。
字段存在**且**不同才会被拒：

```text
Protocol 3 is not understood; this editor speaks 1. Update the DreamShaderLang extension or the
plugin so the two match.
```

走这条路径的编译对 ThinCustom 产物永远是 Ephemeral 的；排队的文件在防抖窗口
（*Save Debounce Seconds*，钳制到 `[0.05, 10.0]`，默认 `0.25`）加最多 `0.1 s` 轮询延迟之后编译，
且仅当它还存在于磁盘上。

<a id="diagnostics-the-extension-reads"></a>

### 扩展读到的诊断

`diagnostics.json` 的形状是
`{ "version": 1, "updatedAtUtc": "…", "files": [ { "path": "…", "diagnostics": [ … ] } ] }`。可选字段为空时整条省略。

| 字段 | 出现时机 | 值 |
| :-- | :-- | :-- |
| `message` | 始终 | 诊断文本 |
| `detail` | 非空时 | 底层原始行 |
| `stage` | 非空时 | 编译的记录：按码所在区间是 `preprocess`、`parse`、`bind`、`ir`、`generate`、`tools` 或 `compile`；其余是 `generate`、`materialCompile` 或 `virtualFunctionSync` |
| `assetPath` | 非空时 | 相关资产的 object path |
| `shaderPlatform`、`qualityLevel` | 非空时 | 仅材质编译诊断 |
| `code` | 非空时 | 编译的记录：它的 `DSHnnnn` 码 （自 2.0.0 起）；其余是 `generate-error`、`material-compile` 或 `virtual-function-sync` |
| `line`、`column` | 始终 | 从 1 开始，默认 `1` |
| `severity` | 始终 | `error`、`warning` 或 `info` （自 2.1.0 起） |
| `source` | 始终 | `DreamShader Lang2`、`DreamShader`、`DreamShader Generate`、`DreamShader Material Compile` 或 `DreamShader VirtualFunction` |

**提示：**

一次编译提交它自己的记录 —— 每条带着 `DSHnnnn` 码、严重度和位置：失败的编译提交全部记录，
**成功的编译提交它的警告和 note** （自 2.1.0 起）（在此之前成功编译会清掉文件的诊断，警告只出现在日志里）。
按严重度过滤的客户端应该把缺失或未知的值当作 error。

**提示：**

**诊断归属于产生它的源文件和那一次运行** （自 2.1.1 起）。运行有三种：
一个源文件的编译、它生成的某个材质的 shader 编译、启动时对一个文件的 VirtualFunction 扫描。
编译 `A.dsm` 只替换 A 上一次编译提交的东西 —— 包括提交到 A 和 A import 的每个头文件上的 ——
``B.dsm`` 提交到同一个头文件上的记录保持不动；成功编译的警告在它的材质 shader 编译完成后也还在。
在此之前，编译 A 会把 B 在共享 `.dsh` 上的错误一起清掉，直到 B 再编一次。删掉一个文件会清掉它拥有的一切和别人提交到它上面的一切。

编译的记录带着自己的行列。`generate-error` 的位置是从形如 `<path>(<line>,<column>): <message>` 的消息里还原的；
没有可解析位置的行报在 `1,1`。材质编译诊断带一条形如 `[{ShaderPlatform} / {QualityLevel}] {Message}` 的展示消息。
UE 5.7 起 `shaderPlatform` 带的是 shader format 名，例如 `PCD3D_SM6`；5.7 以下带的是 feature level 名，例如 `SM6`。

<a id="streaming-preview"></a>

### 流式预览

预览面板连接 `ws://127.0.0.1:17864`，驱动插件的渲染器。

| 消息 | 方向 | 用途 |
| :-- | :-- | :-- |
| `previewMaterial` | 客户端 → 编辑器 | 为一个 `.dss`、`.dsi` 或 `.dsm` 开启预览会话 （自 2.0.0 起）。`encoding` 选 `raw` 或 `png`；`force` 控制是否重新生成 （自 1.7.0 起） |
| `previewControl` | 客户端 → 编辑器 | 调整当前会话 —— 流开关、帧率、相机、`width` / `height` / `mesh` （自 1.7.0 起） |
| `setProbe` / `clearProbe` | 客户端 → 编辑器 | 挂上 / 摘掉一个断点式探针 （自 1.7.0 起） |
| `previewResult` | 编辑器 → 客户端 | 会话启动结果，或流中途的错误 |
| `previewFrame` | 编辑器 → 客户端 | *（仅 png 编码）* 紧随其后那张 PNG 的元数据 |
| `probeState` | 编辑器 → 客户端 | 探针处于 `attached`、`pending` 还是 `cleared` （自 1.7.0 起） |

每条**出站**消息都是 WebSocket *binary* 帧，载荷是 4 字节小端长度、1 字节类型标签
（`1` = UTF-8 JSON，`2` = 原始 PNG，`3` = 原始 RGBA8 帧
（自 1.7.0 起）），然后是载荷本体。入站消息是纯 UTF-8 JSON，没有长度前缀也没有标签字节。

流式既限速又受确认门控：只有客户端已确认上一帧**且**帧间隔已过，新帧才会开始。

**提示：**

**默认编码自 （自 1.7.0 起） 起是 `raw`。** 标签 `3` 的帧自带描述：
24 字节头（尺寸、标志位、相机、解析后的探针行号）之后直接是像素，编辑器侧完全没有 PNG 编码，
客户端把它直接画到画布上。这就是流畅 30–60 FPS 之所以负担得起的原因。旧的 `previewFrame` 加带 tag 的 PNG 那一对
仍然以 `encoding: "png"` 提供给老客户端，行为不变。

画面不动时相同的帧会被丢弃，连续几帧相同之后渲染时钟退到一个很低的空闲速率，直到一次编辑、一次相机 / 网格
变化或一次探针变化把它唤醒。

**警告：**

**旧行为（`1.7.0` 之前）：** 在 `previewControl` 里省略 `frameRate` 会把会话静默掉到 2 FPS ——
读取器在找这个字段之前就把它初始化成了 `2.0`，所以一条纯粹用来确认帧或微调相机的控制消息就足以踩中。

`1.7.0` 起，**省略的字段保持当前值**（`frameRate` 也一样），只有 `requestId`
和 `ackFrameIndex` 例外。仍然需要兼容旧插件的客户端，可以继续在每条 `previewControl` 上带
`frameRate` —— 两种版本下都正确。

<a id="graph-breakpoints"></a>

### Graph 断点

（自 1.7.0 起）

`setProbe` 把一个行号 —— `.dsm` 里的一行 `Graph`、`.dss` 里材质函数体的一条语句 （自 2.0.0 起） —— 交给编辑器，预览网格随即显示**那一行绑定的值**而不是成品材质 ——
材质编辑器右键 *Start Previewing Node* 的文本版。空行或注释行上的断点会向前吸附到下一个绑定了值的行；
源码还没生成过时设的断点记为 *pending*，下一次编译时挂上；重新编译之后自动重解析，并通过 `probeState`
把落在哪一行告诉客户端。行号也会随每一帧回显在标签 `3` 的帧头里。
语义细节见 [编辑器工具](editor-integration.md)。

一次会话，客户端视角：

```text
→ {"type":"previewMaterial","sourceFile":"…/M_Sample.dsm","mesh":"shaderball",
   "width":512,"height":512,"requestId":"8f3c1b","stream":true,"frameRate":12,
   "encoding":"raw"}
← [len][1] {"type":"previewResult","requestId":"8f3c1b","status":"ready", … }
← [len][3] <24-byte header><RGBA8 pixels>
→ {"type":"previewControl","requestId":"8f3c1b","ackFrameIndex":0,"orbitYaw":-140.0}
← [len][3] <24-byte header><RGBA8 pixels>
→ {"type":"setProbe","requestId":"8f3c1b","line":31}
← [len][1] {"type":"probeState","requestId":"8f3c1b","state":"attached","line":31}
← [len][3] <24-byte header><RGBA8 pixels>      // now showing the value bound on line 31
```

渲染器自身的限制 —— 只支持构建材质或材质实例的源文件（`.dss`、`.dsi`、`.dsm`）、尺寸钳制、网格回退、没有 pitch 钳制 ——
见 [编辑器工具](editor-integration.md)。

<a id="dreamshader-20-sources"></a>

## DreamShader 2.0 源文件

（自 2.0.0 起） `.dss` 与 `.dsi` 文件以语言 **DreamShader 2.0**（language id
`dreamshader-dss`，扩展 2.0.0）打开，有自己的语法高亮、代码片段、`#pragma region` 与
`#if` 的折叠，以及回车后自动续写的 `///`。`.dsp` 文件以 `dreamshader-dsp` 打开（扩展 2.1.0）
（自 2.1.0 起），补全来自 `pass-keys.json`，它的符号索引多一个 `pipeline` 段 ——
语言本身见 [Custom Pass（.dsp）](../language-v2/passes.md)。

扩展不自己去理解 2.0 的文本。2.0 有重载、隐式转换，还有一个认识引擎节点类的绑定器；用 JavaScript 再实现一遍，
恰恰会在要紧的地方出错。所以编辑器去问编译器：

| 功能 | 答案来自 |
| :-- | :-- |
| 转到定义 · 查找引用 · 悬停 · 大纲 · 签名帮助 · 重命名 | 插件每次编译后写出的**符号索引**：`Saved/DreamShader/Index/<root>/<file>.index.json` |
| `UE.` 与 `Substrate.` 的补全和悬停 —— 节点名、引脚、属性、Substrate 语法糖实参 | 编辑器加载完成后导出的**内建目录**：`Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json` |
| `#include "` 路径；`BL::` 和其他头文件命名空间；被包含的头文件声明的函数，在一个尚未编译的文件里也能悬停、转到定义和签名帮助（扩展 2.2.0） | **头文件文本**，1.x 和 2.0 一视同仁；索引有答案时由索引先回答 |
| 诊断 | 与 1.x 一样，走 bridge |

答案的新鲜度等于上一次编译，而插件在保存时就会编译。文本已经对不上的位置会被丢掉而不是照样显示；
重命名则直接拒绝并提示先编译。索引只列文件作用域的声明、不列函数体里的局部变量，并且按名字记录提及 ——
所以只有在文件作用域里唯一的名字才能重命名。

一个 `.dsh` 头文件由两种语言共享。用 2.0 写的 —— `#include`、`/// @custom`、`T Name(...)`
且没有 1.x 块 —— 会以 DreamShader 2.0 打开（扩展 2.2.0）；带任何 1.x 块的头文件仍然是
`dreamshaderlang`，因为插件把头文件按混合内容读取。插件在 2.1.2 之后的文件级形式 ——
`#pragma generate` / `/// @generate`、`#pragma nogeneration` / `/// @nogeneration`、`#pragma disable_warning(...)`
/ `disable_error(...)` 和 `// disable_warning DSHnnnn` —— 在两种语言里都能补全、也都有着色，
扩展自己发现的诊断同样遵守禁用列表。

**Reveal in Material Editor** 为当前行发出 bridge 的 `reveal-node` 请求：正在运行的编辑器会打开所属资产的材质编辑器，
并选中这一行生成的节点。反方向在插件里 —— 生成节点的右键菜单有 *DreamShader ▸ Open Source Line*。

<a id="vscode-commands"></a>

## VSCode 命令

这些名字属于扩展，不属于插件，都在 `DreamShaderLang` 命令组下。

```text
DreamShaderLang: Recompile Current Source
DreamShaderLang: Recompile All Sources
DreamShaderLang: Clean Generated Shaders
DreamShaderLang: Show Bridge Panel
DreamShaderLang: Refresh Bridge Diagnostics
DreamShaderLang: Show Material Preview
DreamShaderLang: Install Package from GitHub
DreamShaderLang: Browse Package Store
DreamShaderLang: Update Installed Packages
DreamShaderLang: Remove Installed Package
DreamShaderLang: Open Packages Folder
DreamShaderLang: Add Package Store Index Source
DreamShaderLang: Remove Package Store Index Source
DreamShaderLang: Create Package Step by Step
DreamShaderLang: Create DreamShader Material
DreamShaderLang: Create DreamShader Function File
DreamShaderLang: Create DreamShader Header
DreamShaderLang: Create DreamShader Texture Sample
DreamShaderLang: Create DreamShader Noise Material
```

<a id="vscode-settings"></a>

## VSCode 设置

这些由扩展声明，以扩展仓库为准。把 workspace 指向 Unreal 项目根目录 —— 或者设置 `dreamshader.projectRoot` ——
这样扩展才能解析 `DShader`、`DShader/Packages` 和 `Saved/DreamShader/Bridge`。

| 设置 | 默认值 | 用途 |
| :-- | :-- | :-- |
| `dreamshader.projectRoot` | *（自动）* | workspace 不是在项目根目录打开时，指定 Unreal 项目根 |
| `dreamshader.previewWebSocketPort` | `17864` | 插件的预览 WebSocket 端口 —— 插件侧是固定的 |
| `dreamshader.previewAutoRefreshDelayMs` | `1200` | 编辑后保存并刷新前的延迟 |
| `dreamshader.previewTransport` | `websocket` | 用 WebSocket，或强制走 `file` bridge 请求 |
| `dreamshader.previewLiveFrameRate` | `2` | 流式预览帧率上限；`0` 关闭连续帧 |
| `dreamshader.packageStoreIndexUrls` | *（默认 index）* | 一个或多个 package store index JSON 地址 |
| `dreamshader.enableGitHubPackageSearch` | `true` | 同时按 GitHub topic `dreamshader-package` 搜索 |

```json
{
  "dreamshader.packageStoreIndexUrls": [
    "https://raw.githubusercontent.com/TypeDreamMoon/dreamshader-package-index/main/packages.json"
  ],
  "dreamshader.enableGitHubPackageSearch": true
}
```

<a id="notes"></a>

## 说明

- 插件这一侧对应的 `previewWebSocketPort` **不可配置**：服务器总是绑定
  `127.0.0.1:17864`，来自其他地址的连接会被拒绝。同一台机器上的两个编辑器
  无法同时提供预览 —— 第二个会记一条 listen 警告，然后在没有流式预览的情况下继续跑。
- 每条诊断都带着它所指构造所在文件的行列：报在被 import 的头文件里的诊断写的就是那个头文件 （自 2.0.0 起）（头文件被单独解析，
  不再内联进 import 它的文件）。
- Material Content Browser 的源文件行上只显示每个文件的**第一条**错误，检查器的 *Diagnostics* 一节和扩展显示全部。
  见[编辑器工具](editor-integration.md#material-submenu)。

<a id="example"></a>

## 示例

在一个默认项目上执行 *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* 会动到：

```text
<Project>/DShader/DreamShader.code-workspace                    rewritten
<Project>/Saved/DreamShader/Bridge/material-expressions.json    rewritten
<Project>/Saved/DreamShader/Bridge/settings.json                rewritten
<Project>/Saved/DreamShader/Bridge/pass-keys.json               rewritten (since 2.1.0)
<Project>/Saved/DreamShader/Bridge/substrate-builtins.json      rewritten
<Project>/Saved/DreamShader/Bridge/bridge.db                    tables replaced
```

然后，对一个 `code.cmd` 候选、*Open In New Window* 保持默认时，启动的是：

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"  "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

不通过 VSCode，直接让运行中的编辑器重编一个文件：

```powershell
$req = @{ action = "recompile"; scope = "file"; sourceFile = "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" }
$dir = "C:\Projects\MyGame\Saved\DreamShader\Bridge\Requests"
$tmp = Join-Path $env:TEMP ("ds-" + [guid]::NewGuid() + ".json")
$req | ConvertTo-Json | Set-Content -Path $tmp -Encoding utf8
Move-Item $tmp (Join-Path $dir ([IO.Path]::GetFileName($tmp)))
```

<a id="where-next"></a>

## 继续阅读

  - [编辑器工具](editor-integration.md) — 写出 workspace 的那个菜单项，以及各种 bridge 产物。
  - [Package](packages.md) — package 系统中归扩展负责的那一半。
  - [命令行](commandlet.md) — 没有编辑器、也完全没有 bridge 的编译方式。
  - [安装](../getting-started/installation.md) — 安装插件和其中一个扩展。
  - [诊断](../diagnostics/index.md) — bridge 能发布的每一条消息。
