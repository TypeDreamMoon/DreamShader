<a id="dreammcp-endpoint"></a>

# DreamMCP 端点

> [DreamShader](../index.md) » [Tools](index.md) » **DreamMCP 端点**

DreamMCP 服务器借以访问 DreamShader 的 HTTP 端点：AI 代理的 `dream_shader` 工具会检查、编译并预览源，读取其诊断信息，查找节点并反编译资产——在用户的编辑器打开时于其中进行，在未打开时则在服务器启动的宿主进程中进行。

| | |
| :-- | :-- |
| 种类 | 编辑器服务——`UE::DreamShader::Editor::MCP::FEndpoint`，位于 `DreamShaderEditor` 模块中（`Source/DreamShaderEditor/Private/MCP/`） |
| 协议 | DreamMCP 协议 `1` |
| 清单 | `<Plugin>/Resources/DreamMCP.json`：动作、以 JSON Schema 表示的参数、路由、宿主 commandlet |
| 语言参考 | `.skill/reference/dss.md`、`dsp.md`、`legacy.md`，由清单的 `reference` 指定，并随插件打包（`Config/FilterPlugin.ini`）。每个页面都自成一体：其链接指向在线手册，无法打开这些链接的代理也不会遗漏任何所需内容 |
| 路由 | `http://127.0.0.1:<port>/dream/shader` |
| 端口 | `47800`，或 `-DreamMCPPort=<n>`；无法绑定的端口会跳过并尝试下一个，最多 16 个 |
| 绑定地址 | 仅 `127.0.0.1`——在启动时验证，参见[仅限环回](#loopback-only) |
| 身份验证 | 每个请求上的 `X-Dream-Token` 请求头；令牌取自环境中的 `DREAM_MCP_TOKEN`，否则取 `-DreamMCPToken=<t>`，再否则随机生成 |
| 发现 | `<Project>/Saved/DreamMCP/shader.json`，在端点应答了自身的 hello 之后写入，在端点停止时删除 |
| 宿主 | 带 `-DreamMCPHost -RenderOffscreen` 的编辑器（会渲染）；`-run=DreamShaderHost -nullrhi -DreamMCPHost`（不渲染） |
| 模块依赖 | `HTTPServer`, `Sockets` |
| 禁用方式 | `-NoDreamMCP`；在用户的编辑器中还包括 `-NoDreamShaderEditorBridge` 以及关闭 *Enable DreamMCP Endpoint* |

端点不会做源保存、[commandlet](commandlet.md) 或[预览](preview.md) 尚未做过的事：每个动作都转发到同一个编译器、渲染器、节点目录和反编译器。它新增的是所有这些操作统一的应答结构——一个结果、一个原因码、带代码和行号的诊断信息、写入的资产——因此服务器永远不必去读日志行。

<a id="which-process-serves-it"></a>

## 由哪个进程提供服务

| 进程 | 端点 | `mode` | 渲染 |
| :-- | :-- | :-- | :-- |
| 用户的编辑器，且 [桥接](bridge.md) 正在运行 | 是，在引擎初始化之后 | `editor` | 是 |
| **渲染宿主**：带 `-DreamMCPHost -RenderOffscreen` 的编辑器，无 `-run=` | 是 | `host` | 是 |
| **无头宿主**：`-run=DreamShaderHost -nullrhi -DreamMCPHost` | 是，在 commandlet 的 `Main` 之前 | `host` | 否 |
| 任何其他 commandlet——`-run=DreamShader compile`、一次 cook | 否 | — | — |
| 带 `-NoDreamShaderEditorBridge` 的编辑器（自动化运行） | 否 | — | — |

`-DreamMCPHost` 是让一个进程成为宿主的关键，每个 Dream 插件的端点都遵循它，因此一个宿主进程即可服务所有 Dream 插件，每个插件都在共享端口上以自己的路由提供服务。若 commandlet 的 `-run=` 为 `Dream…Host`，则即使没有该开关也算作宿主。commandlet 从不渲染预览场景，因此无头宿主无论开关如何都报告 `render: false`；需要画面的服务器应启动渲染宿主。

在用户的编辑器中，通过端点进行的编译就是编辑器自身的编译：它经由桥接，因此诊断存储、VSCode 扩展和[材质内容浏览器](material-browser.md) 会像看待一次保存那样看待它，ThinCustom 材质会保持为 [Ephemeral](../generation/ephemeral.md)。宿主没有桥接，也没有能让 Ephemeral 材质存活的会话，因此在那里 `compile` 会将其保存，如同 commandlet 所做的那样。

<a id="starting-a-host"></a>

## 启动宿主

DreamMCP 服务器会自行启动两者之一，并通过环境变量 `DREAM_MCP_TOKEN` 传入令牌：

```text
render:    UnrealEditor-Cmd.exe <Project>.uproject [<map>] -DreamMCPHost -RenderOffscreen -unattended -nosplash -nosound -DreamMCPPort=<p>
headless:  UnrealEditor-Cmd.exe <Project>.uproject -run=DreamShaderHost -nullrhi -DreamMCPHost -unattended -nosplash -nopause -DreamMCPPort=<p>
```

| 开关 | 作用 |
| :-- | :-- |
| `-DreamMCPHost` | 此进程为宿主：`mode: host`，接受 `/shutdown`，监视父进程 |
| `-DreamMCPPort=<n>` | 首先尝试的端口 |
| `-DreamMCPToken=<t>` | 当环境中未设置 `DREAM_MCP_TOKEN` 时使用的令牌；两者都未设置时随机生成。服务器使用环境变量，因为命令行对机器上的每个进程都可见 |
| `-DreamMCPParentPid=<pid>` | 其退出会结束宿主的进程；默认为启动它的进程。`0` 不监视任何进程 |
| `-DreamMCPStartupTimeout=<s>` | 若宿主的引擎循环在插件加载后的这么多秒内仍未运转，则以代码 `3` 退出，而不是被任何拖住它的东西挂起；默认 `600`，`0` 可将其关闭 |
| `-NoDreamMCP` | 没有端点——此时宿主会立即退出 |
| `<map>` *(渲染宿主)* | 编辑器打开的地图；默认地图很重的项目在 `/Engine/Maps/Entry` 上启动更快 |

宿主会在 `POST /dream/shader/shutdown` 时退出（应答后四分之一秒），在启动它的进程消失时退出（每秒检查一次），或在按下 Ctrl+C 时退出，并在退出过程中收回其发现文件。

渲染宿主是一个无人看见的编辑器，它的行为也与之相符：

| | |
| :-- | :-- |
| 窗口 | 无：没有可见的顶层窗口，没有任务栏条目，也永远不会成为前台窗口 |
| 对话框 | 由 `-unattended` 应答；仍卡住启动流程的对话框会触发 `-DreamMCPStartupTimeout` |
| 编辑器自身的部分 | 无桥接（无启动扫描、无文件监视器、无预览 WebSocket、不主张桥接的所有权）、无浏览器标签页、无菜单 |
| 设置 | 不写配置文件（配置系统的文件操作已关闭），不自动保存，并将它打开的项目记录在其发现文件旁，而不是记录在用户的 `ProjectEditorRecords.json` 中（UE 5.6+） |
| 空闲 | 关卡视口不是实时的，且在没有任务运行时帧率上限为 15（无人值守的编辑器不会被引擎限速） |

<a id="discovery-file"></a>

## 发现文件

`<Project>/Saved/DreamMCP/shader.json`，先写在自身旁边再移动到目标位置，因此永远不会读到写了一半的文件：

```json
{ "protocol": 1, "id": "shader", "pid": 23116, "port": 47811, "token": "…", "route": "/dream/shader",
  "ws": null, "mode": "host", "render": true, "engine": "5.8.2", "plugin": "2.2.0",
  "startedAt": "2026-11-12T10:00:00.000Z" }
```

| 规则 | |
| :-- | :-- |
| 何时 | 仅在端点应答之后。监听随模块一起启动，但直到引擎主循环驱动 HTTP 服务器时才会有应答——在编辑器中，这要等到数秒之后，其间它正在构建 UI。因此端点会先从工作线程通过套接字、带上令牌询问自己的 `GET /dream/shader/hello`，并在一次 `200` 之后的第一个 tick 写入文件；未被应答的 hello 会在半秒后再次询问。存在的文件所指向的端点必定已经应答过 |
| 启动时 | 直接覆盖写入文件：项目中最新的进程才是要通信的对象。在宿主运行期间打开的编辑器会替换宿主的文件，服务器会在宿主当前任务完成后转移到编辑器 |
| 停止时 | 仅当文件中仍指向本进程时才删除它，因此在编辑器接管之后退出的宿主不会动编辑器的文件 |

<a id="loopback-only"></a>

## 仅限环回

引擎的 `HTTPServer` 会绑定到项目 Engine 配置中 `[HTTPServer.Listeners]` 所指定的位置，而项目可能为了自己的 Web 界面而设置 `DefaultBindAddress=any`。因此端点在自己唯一一次绑定前，把 `(Port=<p>,BindAddress=127.0.0.1,ReuseAddressAndPort=false)` 放到 `ListenerOverrides` 的最前面，随后立刻把该节恢复原状，因此其中任何内容都不会进入已保存的配置。

然后它会验证结果：它通过机器上每一个非环回 IPv4 地址向该端口发起 TCP 连接，每一个都必须被拒绝。只要某个地址能应答，该端口就不会被使用：

```text
DreamMCP: port 47800 answers on 192.168.1.20:47800, not only on 127.0.0.1; it is not used. Check [HTTPServer.Listeners] in the project's Engine config.
```

当同一进程中有另一个插件更早地把同一端口绑定到所有地址时，就会出现这种情况；端点会继续尝试下一个端口。

<a id="requests"></a>

## 请求

| 方法与路径 | 应答 |
| :-- | :-- |
| `GET /dream/shader/hello` | `200` `{ protocol, id, plugin, engine, mode, render, busy, ws, pid, actions }` |
| `POST /dream/shader/call` | 短操作：`200` 以及 [结果](#the-result)；长操作：`202 { id, job, state }` |
| `GET /dream/shader/jobs/<job>` | `200 { id, job, action, state, progress?, result? }` |
| `POST /dream/shader/jobs/<job>/cancel` | `200 { job, state, cancelled, pending?, summary }` |
| `POST /dream/shader/shutdown` | 宿主：`200` 并退出；编辑器：`409` `not-a-host` |

每个 `POST` 都带有请求体，没有内容可说时用 `{}`：对于没有 `Content-Length` 的 `POST`，引擎的 HTTP 服务器会在端点看到它之前，用自己的 `411` 作出应答。

`/call` 接受 `{ "id": "<caller's id>", "action": "<name>", "args": { … } }`。在执行任何内容之前，参数会按照清单中该操作的 schema 进行检查：未声明的参数、类型错误、值超出其 `enum`、`minimum` 或 `maximum`，或者缺少必需的 `required`，都会被 `400` `bad-args`，并且摘要会指出该参数。在第一个任务仍然存在时，用相同 `id` 和操作发起的第二个 `/call` 会返回那个任务，而不是启动另一个。

协议本身的每次拒绝都采用结果的结构，并带有 `outcome: "refused"`：

| 状态 | `reason` | 何时 |
| :-- | :-- | :-- |
| `401` | `bad-token` | `X-Dream-Token` 缺失或错误 |
| `403` | `origin-not-allowed` | 请求带有 `Origin` 头——无论是否有令牌，都最先检查 |
| `400` | `bad-request` | 请求体不是 JSON 对象 |
| `400` | `bad-args` | 参数不符合该操作的 schema |
| `404` | `unknown-action` | 没有此操作；`data.actions` 会列出它们 |
| `404` | `unknown-route` / `unknown-job` | 没有此路径 / 此进程中无此任务 |
| `405` | `bad-method` | 对该路由使用了错误的动词 |
| `409` | `not-a-host` | 向编辑器发送了 `/shutdown` |
| `202`，然后是任务的结果 | `render-unavailable` | 在无法渲染的进程上执行 `needs: render` 操作：长操作仍然是一个任务，会立即以该拒绝结束 |

<a id="jobs"></a>

### 任务

长操作就是一个任务。请求和任务都在游戏线程上运行——编译器会创建对象——因此请求会立即应答 `202`，之后任务**每个 tick 执行一步**：任务运行期间 `hello` 仍可应答，而在有任何任务排队或运行时 `busy` 为 `true`。一次只运行一个任务，按它们到达的顺序执行。

| `state` | 含义 |
| :-- | :-- |
| `queued` | 等待它前面的任务 |
| `running` | `progress` 为 `{ current, total, message }`：`check` 统计文件数，`preview` 统计其阶段（编译、着色器、预热，然后每帧一个）；`message` 指出阶段名称 |
| `done` | 以某个结果结束：`ok`、`source-error` 或 `refused` |
| `failed` | 以 `engine-error` 结果结束 |
| `cancelled` | 按请求停止；其结果的 `reason` 为 `cancelled` |

取消排队中的任务是立即生效的。正在运行的任务会在下一步停止——在一次 `check` 的两个文件之间、预览渲染之前——并且应答中会注明 `pending: true`；已经开始的一步会先完成。最近 64 个已完成的任务仍可读取；任务不会在进程结束后保留。

<a id="the-result"></a>

## 结果

```json
{
  "ok": false,
  "outcome": "source-error",
  "reason": null,
  "summary": "Checked DShader/MCPProbe/M_McpBroken.dss: 1 error(s), 0 warning(s).",
  "diagnostics": [
    { "code": "DSH4200", "severity": "error", "file": "DShader/MCPProbe/M_McpBroken.dss", "line": 4, "column": 23,
      "length": 13, "message": "'UndefinedTint' is not declared in this scope.", "stage": "bind",
      "doc": "https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4200" }
  ],
  "writes": [],
  "artifacts": [],
  "data": { "checked": 1, "errors": 1, "warnings": 0, "files": [ … ] },
  "next": [ "Fix the errors at the lines given, then check again." ]
}
```

| 字段 | |
| :-- | :-- |
| `outcome` | `ok`；`source-error`——源本身有错，诊断信息会指出位置；`refused`——不是源的问题，`reason` 会说明原因；`engine-error`——内部某处失败了 |
| `diagnostics[]` | `code`、`severity`、`file`、`line`、`column`、`message`；`length`、`hint`（记录中的详细信息）、`stage` 和 `doc`（若已知）。先是错误，然后是警告，最后是信息，每类按发现的顺序排列 |
| `writes[]` | `{ asset, state }`：包，以及 `created`、`saved`（它已存在）或 `memory`（一个 Ephemeral 产品）——读取自引擎自身的保存事件 |
| `artifacts[]` | `{ kind, path, title }`：`Saved/DreamMCP/artifacts/` 下的一个 `image`，一个反编译的 `source` |

每个路径都相对于项目目录，并使用正斜杠；任何回答都不包含绝对路径。无论编辑器使用何种语言，每句话都是固定不变的英文——编译器消息通过 `ToInvariantWireString` 发出，与 [桥接](bridge.md) 上一样。

<a id="actions"></a>

## 操作

| 操作 | 长名称 | 写入 | 参数 | 作用 |
| :-- | :-- | :-- | :-- | :-- |
| `check` | 是 | 否 | `file`，或 `all: true` | 将管线运行到 IR 验证，作为 `dsc check`；不构建任何内容 |
| `compile` | 是 | 资源 | `file`; `force`, `shaders` | 构建源的资源；`shaders: true` 采用 `check -Shaders` 的路线，并将 HLSL 错误映射到源行（始终保存）。`data.products[]` 是 `{ asset, kind, onDisk, note }`，`note` 用一句话说明调用方应了解它的什么——它会一直保留在用户编辑器的内存中直到保存，或者在编辑器中对它所做的编辑会阻止下一次构建 |
| `diagnostics` | 否 | 否 | `file`（可选） | 此进程已经持有的内容：编辑器的诊断存储，否则就是通过该端点进行的最后一次检查或编译。未提供 `file` 时，摘要会列出每个文件及其计数，有错误的文件排在前面（最多十个，然后是“N more”）；`data.files[]` 的每一项都有 `errors`、`warnings`、`infos`。只有出现错误时，结果才为 `source-error` |
| `lookup` | 否 | 否 | `name`；`limit`（1–200，默认 20） | [内置节点目录](../builtins/ue-expression.md)：精确名称、别名或 `m.` 属性会给出 `export-catalog` 所写形式的条目；其他情况则返回包含该文本的名称 |
| `preview` | 是 | 可能 | `file`; `mesh`, `width`, `height`, `yaw`, `pitch`, `force`, `time`, `frames`, `duration`, `tone` | 与[预览](preview.md)一样编译材质，以每次一个 tick 的方式等待其着色器，将 PNG 渲染到 `Saved/DreamMCP/artifacts/<job>.png` 并[对其进行测量](#what-preview-measures)；着色器错误是带有默认材质图像的 `source-error` |
| `info` | 否 | 否 | — | 插件和引擎版本、源码根目录、`materials`（用一句话说明在项目默认后端下如何构建材质，以及这对调用方意味着什么）、`mode`、`render`、是否编译了 Custom Pass 和 Substrate |
| `decompile` | 是 | 一个源文件 | `asset`; `out`, `readable`, `overwrite` | 由 [反编译器](decompiler.md) 生成，格式与 commandlet 写出的一致；除非 `overwrite`，否则拒绝 `file-exists` |

操作使用的拒绝原因：`file-not-found`、`not-a-source`（`.dsh`，或不是 DreamShader 文件）、`not-previewable`、`asset-not-found`、`not-decompilable`、`file-exists`、`asset-hand-edited`（资产已被[手动编辑](../generation/divergence.md)，`DSH8115`——`force`无法通过它）、`strip-too-wide`（`frames × width`超过 8192 像素）、`cancelled`。引擎错误：`compiler-unavailable`、`compile-failed`、`preview-compile-failed`、`render-failed`、`artifact-not-written`、`source-not-written`。

<a id="what-preview-measures"></a>

### `preview` 度量的是什么

无法查看图像的模型仍需知道图像内容，因此 `preview` 会测量像素，并在 `summary` 的第一句说明最关键的事实，在 `next` 中说明如何处理。摘要以图像路径结尾。

测量使用三次渲染，三者都使用相同的网格、相机、时间和色调：材质；仅背景（网格上不绘制任何内容的绘制结果）；网格的轮廓（无光照洋红色、双面）。背景和轮廓每个作业只渲染一次，且使用第一帧的时间——不会跟随移动网格的 World Position Offset。三者都会先绘制一次然后丢弃：从未绘制过的材质在被绘制时会请求其着色器，在着色器就绪前按引擎默认材质绘制。

| 术语 | 含义 |
| :-- | :-- |
| 网格像素 | 轮廓渲染在某个通道上与背景的差值大于 **32**（满值 255） |
| 透明 | 每个通道与背景的差值都在 **4** 以内的网格像素：材质会让背景透出 |
| 前景 | 不属于透明的网格像素——即材质实际绘制的内容 |
| 接近白色 | 每个通道均为 **240** 或以上的前景像素 |
| 近黑色 | 每个通道都不高于 **12** 的前景像素 |
| 亮度 | 对 PNG 的 0–1 值应用 Rec. 709 权重（0.2126、0.7152、0.0722）——即伽马编码后图像所呈现的观感，而非场景光照 |
| flat | 亮度和每个通道的标准偏差均不超过 **0.012** |

`data.stats`（所有帧合在一起）和`data.frames[].stats`（每一帧）包含：

| 字段 | |
| :-- | :-- |
| `coverage` | 网格覆盖图像的比例，0–1 |
| `transparent` | 网格中透明部分所占的比例，0–1 |
| `foreground` | 图像中前景所占的比例，0–1 |
| `meanColor`, `meanColorName` | 前景的均值、`#RRGGBB`，以及它的通俗名称（`orange`、`pinkish grey`、`brown`；见下文） |
| `luminance` | 前景的`{ min, max, mean, stdDev }`，0–1 |
| `nearWhite`, `nearBlack` | 前景的占比，0–1 |
| `flat` | 前景为单一颜色 |

颜色名称先看 HSV 的明度和饱和度，再看色相：饱和度低于 0.12 的是灰色（`white`、`light grey`、`grey`、`dark grey`、`black`）；低于 0.4 的是带色调的灰色，用色相名称会夸大其词（`pinkish grey`、`light bluish grey`、`warm grey`）；高于该值则是色相（`red`、`orange`、`dark blue`、`pale green`），其中明度低于 0.5 的橙色和黄色称为 `brown`。

第一条成立的规则给出摘要中的事实：

| 规则 | 事实 | `next` |
| :-- | :-- | :-- |
| 覆盖率低于 0.5% | 网格不在画面中 | 检查 `yaw`、`pitch`、`mesh`，以及一个 World Position Offset |
| 透明 95% 或更高 | 材质不可见 | 检查 Opacity、OpacityMask 与混合模式 |
| 接近黑色 90% 或以上 | 材质渲染为黑色 | Unlit 材质只显示 EmissiveColor；受光照的材质则需要 BaseColor |
| 接近白色 30% 或更多 | 材质大部分为白色（`tone: "linear"`：在 1 处被裁剪） | 亮部数值被压向白色；`tone: "linear"` 会读取这些数值 |
| flat | 单一纯色，带有其 `#RRGGBB` | 检查本应使其变化的内容 |
| 否则 | 平均颜色和亮度范围，以及当透视占比达到 5% 或以上时的透视占比 | — |

`tone` 决定颜色如何进入图像：

| `tone` | 曝光 | 色调曲线 | 用途 |
| :-- | :-- | :-- | :-- |
| `editor`（默认） | 项目自身的曝光，按照引擎缩略图视图的使用方式：缩略图没有视图历史，因此它是引擎根据自动曝光范围确定的曝光 | 胶片式，带泛光 | 查看 [preview](preview.md) 向用户显示的内容 |
| `fixed` | 1 (EV100 0)，无论项目设置如何 | 胶片式，带泛光 | 用于在曝光设置不同的项目之间进行比较 |
| `linear` | 1 (EV100 0) | 无色调映射且无泛光——色调映射器的仅 gamma 路径，gamma 2.2 | 读取材质的输出：自发光 (1, 0, 0) 为 `#FF0000`，大于等于 1 的值会被钳制为白色 |

使用 Unlit 自发光灰度在插件的测试主机上测量，该主机的曝光设置为默认值（在那里 `editor` 和 `fixed` 的绘制结果相同）：

| 自发光 | 0.05 | 0.18 | 0.5 | 1 | 2 | 4 | (1, 0, 0) |
| :-- | --: | --: | --: | --: | --: | --: | :-- |
| `editor`, `fixed` | 54 | 117 | 178 | 208 | 227 | 240 | (227, 0, 6) |
| `linear` | 65 | 117 | 186 | 255 | 255 | 255 | (255, 0, 0) |

胶片曲线会进行压缩：没有任何内容被截断，明亮颜色会向白色偏移——这就是自发光内容较多的预览所呈现的样子，而 `data.toneNote` 会在每个结果中说明这一点。

`time` 以秒为单位设置材质的 Time（两个时钟，无论是否启用 *Ignore Pause*）；默认值为 `0`，因此同一来源的预览每次都是同一图像。`frames` 会渲染相应数量的时刻，这些时刻在从 `time` 开始的 `duration` 秒内均匀分布——第 *i* 帧位于 `time + duration × i / frames`——并排放入一个 PNG，每帧为 `width × height`（使用 `frames` 时默认为 256，不使用时为 512）。

条带绝不会用其所有帧的平均值来概括，因为那可能是任何一帧都不显示的颜色。会变化的条带会先说明其变化方式：按每帧的平均亮度给出最暗和最亮的帧及其颜色，并按顺序给出各帧颜色——`It animates over 8 frames in 1 s: darkest at 0.75 s, #5D4052 (dark purplish grey), brightest at 0.25 s, #BC99AE (pinkish grey); the frames in order are …`。上表中适用于所有帧整体的规则仍会优先说明。不变化的条带则按其第一帧作为一张图像来描述，并说明没有任何动画。

`data.animation` 为 `{ frames, duration, maxDifference, changes, straightness, undersampled, darkest, brightest }`：

| 字段 | |
| :-- | :-- |
| `maxDifference` | 一帧与第一帧之间最大的平均逐像素差异，0–255 |
| `changes` | `maxDifference` 高于 0.5 |
| `straightness` | 对每三个连续帧，将第一帧到第三帧的差异与经过中间帧的差异相比后求和——稳定漂移为 1，每帧来回跳变为 0。一个周期采样 8 次的正弦得分约为 0.8，采样 4 次为 0.5 |
| `undersampled` | 它发生变化、帧数为 4 帧或更多、相邻帧平均相差 1/255 或更多，并且 `straightness` 低于 0.45：它在帧之间来回跳变，因此其变化速度可能快于采样速度。摘要会说明这一点，而 `next` 会建议缩短 `duration` 或提高 `frames` |
| `darkest`, `brightest` | 这些帧的 `{ index, time, meanColor, meanColorName, luminance }` |

<a id="example"></a>

## 示例

```bash
T=$(jq -r .token Saved/DreamMCP/shader.json); P=$(jq -r .port Saved/DreamMCP/shader.json)
curl -s -H "X-Dream-Token: $T" http://127.0.0.1:$P/dream/shader/hello
curl -s -H "X-Dream-Token: $T" -d '{"id":"1","action":"check","args":{"file":"DShader/M_Rock.dss"}}' \
  http://127.0.0.1:$P/dream/shader/call
# {"id":"1","job":"3f9c0a1d2b-1","state":"queued"}
curl -s -H "X-Dream-Token: $T" http://127.0.0.1:$P/dream/shader/jobs/3f9c0a1d2b-1
```

<a id="see-also"></a>

## 另见

- [Commandlet](commandlet.md) — 从命令行使用相同的动词，每次运行一个进程
- [Bridge](bridge.md) — 端点所通过其进行编译的编辑器服务，以及它自身的两种传输方式
- [Preview](preview.md) — `preview` 背后的渲染器
- [Diagnostics](../diagnostics/index.md) — 结果可以携带的每一个代码
- [Project settings](../settings/project.md) — *Enable DreamMCP Endpoint*
