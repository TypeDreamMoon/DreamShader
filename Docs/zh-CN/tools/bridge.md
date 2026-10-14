---
title: "编辑器桥接"
description: "编辑器桥接"
---

<a id="editor-bridge"></a>

# 编辑器桥接

> [DreamShader](../index.md) » [工具](index.md) » **编辑器桥接**

编辑器侧的服务，它通过两种传输方式将 DreamShader 暴露给外部编辑器——轮询的请求文件目录和回环 WebSocket 服务器——并在 `Saved/DreamShader/Bridge/` 下生成一组产物。

| | |
| :-- | :-- |
| 类型 | 编辑器服务 — `FDreamShaderEditorBridge`，由 `DreamShaderEditor` 模块拥有 |
| 根目录 | `<Project>/Saved/DreamShader/Bridge/` |
| 传输 A | 请求文件 — `Bridge/Requests/*.json`，每 `0.1 s` 轮询一次，在分发前删除，应答写入 `Bridge/Responses/<requestId>.json` |
| 传输 B | WebSocket — `ws://127.0.0.1:17864`，仅限回环 |
| 存活状态 | `Bridge/status.json`，空闲时每 `2 s` 重写一次，**关闭时删除** |
| 协议 | `1` — 在请求中作为 `protocol` 发送，在每个响应和状态文件中回显 |
| 模块依赖 | `WebSocketNetworking`、`SQLiteCore` |
| 禁用方式 | `-NoDreamShaderEditorBridge`；在命令小工具中从不启动 |

<a id="synopsis"></a>

## 概要

```text
<Project>/Saved/DreamShader/Bridge/
├─ Requests/                   inbound  *.json, consumed and deleted
├─ Responses/                  outbound <requestId>.json, one per request that carried an id
├─ status.json                 outbound liveness heartbeat; absent means "not running"
├─ diagnostics.json            outbound aggregated diagnostics
├─ diagnostics/                outbound sharded diagnostics
│  ├─ index.json
│  └─ <md5-of-source-path>.json
├─ bridge.db                   outbound SQLite (+ bridge.db-wal, bridge.db-shm)
├─ material-expressions.json   outbound reflected UMaterialExpression catalogue
├─ settings.json               outbound enum alias tables
├─ substrate-builtins.json     outbound Substrate builtin catalogue
├─ pass-keys.json              outbound the .dsp vocabulary, for completion (since 2.1.0)
├─ dreamshader-builtin-catalog.json  outbound the 2.0 binder's node catalog (since 2.0.0)
├─ preview.json                outbound one-shot preview result
└─ Preview/                    outbound rendered PNGs
   └─ <stem>-<crc32>.png
```

<a id="lifecycle"></a>

## 生命周期

启动，按顺序进行。首先创建 `Bridge`、`Bridge/Requests` 和 `Bridge/Preview`。

| 步骤 | 操作 |
| :-- | :-- |
| 1 | 创建 `Bridge/`、`Bridge/Requests/`、`Bridge/Responses/`、`Bridge/Preview/` |
| 1a | 删除所有过期的 `Responses/*.json`，记录开始监听时间，发布 `status.json` |
| 2 | 删除 `bridge.db`、`bridge.db-wal`、`bridge.db-shm` 以及整个 `diagnostics/` 目录 |
| 3 | 导出 `material-expressions.json`、`settings.json`、`substrate-builtins.json`（及其 SQLite 表） |
| 4 | 扫描并刷新 `VirtualFunction` 声明 |
| 5 | 注册一个引擎初始化后处理器，在内存中生成每个源文件，然后导出 `dreamshader-builtin-catalog.json` *（自 2.0.0 起）* — 在扫描之后执行，因为目录是对每个已加载表达式类的反射，只有此时所有模块才已加载 |
| 6 | 注册项目设置属性监视器 |
| 7 | 将完整重新扫描排队并写入 `diagnostics.json` |
| 8 | 在端口 `17864` 上启动预览 WebSocket 服务器 |
| 9 | 在 `<SourceDirectory>` 上注册目录监视器（包括目录更改） |
| 10 | 挂钩 `UMaterial::OnMaterialCompilationFinished` |
| 11 | 注册工具菜单、工具栏和上下文菜单项 |
| 12 | 注册计时器 A — `0.1 s`：请求文件轮询和防抖队列 |
| 13 | 注册计时器 B — 每帧：预览流式传输 |

计时器 B 单独注册，间隔为零秒，因为共享计时器 A 会将每个预览流限制在 10 FPS，而无论客户端请求的帧率是多少。

关闭时**首先删除 `status.json`**，然后移除每个句柄、关闭 WebSocket 服务器、清除待处理文件映射和诊断存储、再次删除桥接数据库，并注销工具菜单，除非引擎已在退出。

<a id="one-editor-owns-the-bridge"></a>

## 一个编辑器拥有桥接

桥接目录是按**项目**而不是按进程划分的：一个 `Requests` 文件夹、一个 `status.json`、一个心跳。因此，同一项目上打开的两个编辑器都会轮询同一队列——谁先到达谁就消费了该文件，另一个则读取到半删除的文件，并且两者都用各自的 pid 覆盖 `status.json`，因此客户端无法分辨哪个编辑器将要应答它。

自 `1.8.0` 起，恰好有一个进程服务桥接，并在锁文件中说明这一点：

| | |
| :-- | :-- |
| 文件 | `<Project>/Saved/DreamShader/Bridge/owner.lock` |
| 内容 | `pid` 和 ISO-8601 格式的 `heartbeat`，在普通心跳时刷新 |
| 可信条件 | 拥有进程仍在运行**且**其心跳不超过 30 秒 |
| 释放 | 在编辑器关闭时释放，因此下一个编辑器会在其下一个心跳时接管，而不是在过期窗口之后 |

两个条件都需要。仅靠 pid 测试会在第一个编辑器处于编译中途时将桥接交给第二个编辑器，因为编译会阻塞游戏线程并停止心跳；仅靠心跳测试会在硬崩溃后让桥接在过期窗口内无人拥有。

非拥有编辑器仍会编译其自身的 Ephemeral 材质——那些是每进程的，与其他人无关——但它不会消费请求、不会写入 `status.json`，并且**不会将生成的资源写入磁盘**。最后一点很重要，因为[存储决定重建如何持久化](../generation/ephemeral.md#when-the-asset-already-exists-on-disk)：没有它，两个编辑器都会 `SavePackage` 同一个文件，而输掉该竞争的保存不是合并——而是损坏的包或死掉的编辑器。这样的编译会报告：

```text
Skipped {AssetPath}; another editor owns this project's DreamShader bridge, and only that one
writes generated assets to disk.
```

命令小工具不受影响：它没有桥接、没有锁文件，也没有第二个进程需要协商，而写入这些资源正是它的全部工作。

只有拥有者会发布或清理共享响应、状态、诊断、`bridge.db` 和清单。非拥有者会保留其本地诊断和浏览器通知。当它后来获得所有权时，它会启动新的监听会话并发布自己的清单和当前诊断；关闭非拥有者会保持正在运行的拥有者的文件不变。

保存、添加或移除所包含的 `.dss` 会将其直接和传递消费者排队，就像 `.dsh` include 一样。监视器会为删除批次保留先前的依赖边，因此移除一个 include 仍会导致其消费者报告缺失文件。

<a id="request-files"></a>

## 请求文件

| | |
| :-- | :-- |
| 目录 | `<Project>/Saved/DreamShader/Bridge/Requests/` |
| 发现 | `*.json`，仅文件，**非递归**——从不扫描子目录 |
| 轮询间隔 | `0.1 s` |
| 消费 | 每个发现的文件都在其循环迭代结束时**被删除** |

文件名无关紧要；只有 JSON 内容重要。为每个请求使用唯一名称。

请求在分发**之前**被删除，而不是之后，因此导致编辑器崩溃的请求不会在每次后续启动时被重放。唯一的例外是完全无法读取的文件：那几乎总是仍在写入的文件，因此它会被留到下一次轮询，而不是被丢弃。

由于轮询器仍可能在文件写入中途打开它，因此请将 JSON 写入其他位置的临时名称，并将其**重命名**到 `Requests/` 中，以便它以原子方式出现。

> [!NOTE]
> 携带非空 `requestId` 的请求会在 `Responses/<requestId>.json` 中得到应答——包括失败：无法解析的 JSON、协议不匹配、无法识别的 `action`，以及源文件在防抖窗口结束前消失的 `recompile`。**不带** id 的请求不会得到响应，这正是随附 VSCode 扩展发送的每个请求的样子，也是该扩展能保持不变继续工作的原因。

> [!WARNING]
> 在无编辑器监听时写入的请求会被**丢弃**，而不是排队。截止时间是桥接自身的启动时间，因此当编辑器启动时正在磁盘上等待的请求会连同日志行一起被删除，而不是被服务——到那时发送它的任何人早已超时，服务它意味着进行一次没人要求的编译。写入请求前请检查 `status.json`。

<a id="actions"></a>

### 操作

`action` 和 `scope` 均以不区分大小写的方式匹配。

| `action` | 必填字段 | 效果 | 应答 |
| :-- | :-- | :-- | :-- |
| `ping` | — | 无。确认桥接正在服务 | 立即 |
| `recompile` | `scope: "all"` | 重建依赖图并将每个根节点的每个源文件——`.dsm`、`.dsf`、`.dss`、`.dsi`、`.dsp`，根节点 `Packages` 下的 `.dsm` 除外——加入编译队列，**强制**跳过构建键检查 | 立即，状态为 **已排队**——见下文 |
| `recompile` | `scope: "file"`、`sourceFile` | 将一个文件加入防抖队列，**强制**跳过源哈希检查 | **该次编译完成时** |
| `cleanGeneratedShaders` | — | 删除生成的 `*.ush` 包含文件，然后排队执行一次完整重新扫描 | 立即 |
| `previewMaterial` | `sourceFile` | 同步渲染一个预览并写入 `preview.json` | 立即，附带渲染结果 |
| `reveal-node` | `file`、`line` | 查找由该行生成的节点，打开其材质编辑器并选中它 *（自 2.0.0 起）* | 立即，附带资产路径和表达式 GUID |

> [!IMPORTANT]
> 带 `scope: "file"` 的 `recompile` 是唯一其响应为**延迟**的操作。文件会
> 进入防抖队列并在若干 tick 之后编译，因此在派发时应答会在任何事情尝试之前
> 就报告成功。该 id 会被暂存，并由编译本身应答，携带该次编译的诊断信息。
> 两个客户端请求同一文件都会得到应答。
>
> `scope: "all"` 相反，并且明确说明了这一点：它立即以 *已排队* 应答，因为该批次
> 会跨 tick 排空，结果尚不可知。不要将其视为结果。

> [!NOTE]
> 错误的 `scope`、缺失的 `sourceFile` 或无法识别的 `action` 现在会返回**错误响应**，
> 而不再是静默无操作——前提是请求携带了 `requestId`。若没有，则行为
> 不变：文件被删除，不作任何说明。

<a id="protocol"></a>

### 协议

请求可以携带 `protocol`。**缺失**该字段会被视为当前版本，而非不匹配——
已发布的 VSCode 扩展发送的每个请求都省略该字段，拒绝这些请求会破坏每一台
安装了该扩展的机器上的功能。若字段存在*且*不同，则会被拒绝：

```text
Protocol 3 is not understood; this editor speaks 1. Update the DreamShaderLang extension or the
plugin so the two match.
```

<a id="responsesrequestidjson"></a>

### `Responses/<requestId>.json`

以原子方式写入——先写入目标旁的临时文件，然后重命名——因此出现与完整是同一
事件。上一会话的过期响应会在启动时删除：一个已无人等待的应答比没有更糟，
因为重新连接的客户端会对它采取行动。

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `protocol` | number | 始终为 `1` |
| `requestId` | string | 回显请求 |
| `ok` | bool | |
| `durationMs` | number | |
| `message` | string | 不变形式，绝不使用本地化的显示文本 |
| `diagnostics` | array | `file`、`line`、`column`、`severity`，以及非空时的 `code` / `stage` / `assetPath` |

每条诊断保留各自的文件：一个引入了损坏 `.dsh` 的 `.dsm` 会在 `.dsh` 的
位置失败，将其归因于 `.dsm` 会把作者引向错误的文件。

<a id="statusjson"></a>

### `status.json`

心跳，空闲时每 `2 s` 重写一次，并在**关闭时删除**——因此文件缺失
明确意味着“未运行”，客户端可以立即回退，而不必等待
存活窗口超时。

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `protocol`、`pid`、`project`、`projectDir`、`engineDir`、`pluginVersion` | | 身份标识 |
| `busy` | bool | 正在服务请求时为 true |
| `busyAction` | string | 仅在忙碌时存在 |
| `lastResult` | string | 某项操作完成后存在 |
| `heartbeatUtc` | string | ISO-8601 UTC |

> [!WARNING]
> 在判断心跳的新旧之前**先**读取 `busy`。编译会阻塞游戏线程，因此运行编译时
> 心跳会停止——只看时间戳的客户端会把每次真实编译都读作崩溃。剩余的
> 歧义是硬崩溃，它会留下该文件；记录的 `pid` 正是用来消除它的。

<a id="recompile"></a>

### `recompile`

| 字段 | 类型 | 默认值 | 说明 |
| :-- | :-- | :-- | :-- |
| `action` | string | — | `"recompile"` |
| `scope` | string | — | `"all"` 或 `"file"` |
| `sourceFile` | string | — | 当 `scope` 为 `"file"` 时必填且非空；建议使用绝对路径 |

`scope: "all"` 记录 `DreamShader queued a full .dsm/.dsf recompile scan.` 排队的文件会在
防抖窗口之后编译——*Save Debounce Seconds*，被限制在 `[0.05, 10.0]` 范围内，默认为 `0.25`——且
仅当它仍然存在于磁盘上时。有效延迟为防抖时间加上最多 `0.1 s` 的轮询延迟。
通过此路径进行的编译始终是**瞬态的**：编辑器在内存中生成。

<a id="cleangeneratedshaders"></a>

### `cleanGeneratedShaders`

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `action` | string | `"cleanGeneratedShaders"` |

仅递归删除 `*.ush` 文件，一次一个——目录本身永不被删除——然后
排队执行一次完整重新扫描。当 *Generated Shader Directory* 被指向项目的 `Intermediate`
目录之外时，它会拒绝运行：

```text
DreamShader refused to clean generated shaders: '{Directory}' is not inside the project Intermediate
directory. Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning.
```

<a id="previewmaterial"></a>

### `previewMaterial`

| 字段 | 类型 | 默认值 | 约束 |
| :-- | :-- | :-- | :-- |
| `action` | string | — | `"previewMaterial"` |
| `sourceFile` | string | `""` | 必须存在且为 `.dss`、`.dsi` 或 `.dsm`——即构建材质或材质实例的源 |
| `mesh` | string | `""` → `sphere` | 参见 [网格体](#meshes) |
| `width` | number | `512` | 取整，限制在 `[64, 2048]` 范围内 |
| `height` | number | `512` | 取整，限制在 `[64, 2048]` 范围内 |
| `requestId` | string | *（不存在）* | 非空时回显到 `preview.json` |

> [!NOTE]
> 请求文件变体**不会**读取 `orbitYaw`、`orbitPitch`、`stream` 或 `frameRate`。这些
> 字段仅存在于 WebSocket 传输上。文件请求始终以
> 默认相机角度渲染一帧。

结果会写入 `preview.json`，状态为 `ready` 或 `error`，并在成功时以
`Display` 记录 `DreamShader preview: {Message}`，失败时记录 `Error`。

<a id="reveal-node"></a>

### `reveal-node`

*（自 2.0.0 起）* 节点到源导航的逆向操作：从 `.dss` 源的一行到材质编辑器
中的节点。它是 VSCode 扩展的 *Reveal in Material Editor* 命令所发送的内容；
编辑器侧的部分在 [编辑器集成](editor-integration.md#reveal-node--the-reverse-direction) 中描述。

| 字段 | 类型 | 默认值 | 约束 |
| :-- | :-- | :-- | :-- |
| `action` | string | — | `"reveal-node"` |
| `file` | string | — | 必填；绝对路径或项目相对路径，两者都会被规范化 |
| `line` | number | — | 必填，从 1 开始 |
| `requestId` | string | *（不存在）* | 指定响应文件 |

同步提供服务：桥接会找到带有该源文件标记的资产，挑选记录跨度落在此行上的表达式（**起点**在此行上的节点优先于仅从该处调用内联而来的节点），为拥有它们的资产打开材质编辑器，并选中第一个。

响应是普通的 `Responses/<requestId>.json` 信封，额外增加了四个字段：

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `version` | number | 始终为 `1` —— 所添加字段的载荷版本，与 `protocol` 相互独立 |
| `assetPath` | string | 打开了其图表的资产；对于 ThinCustom 产品，则是隐藏的**基础材质**，即图表所在之处 |
| `instanceAssetPath` | string | 仅对 ThinCustom 产品存在：基础材质被寻址到的实例 |
| `expressions` | array | 每个匹配表达式的 GUID，带连字符，选中的排在最前 |

失败会以 `ok: false` 返回，原因位于 `diagnostics`（`stage: "navigate"`）中，错误码为
`DSH9050`–`DSH9057`。`DSH9051` 表示此编辑器中未加载来自该源的任何资产——请先编译该文件；`DSH9052` 表示这些资产是在节点导航功能存在之前构建的，不携带跨度表——请使用 `-Force` 重新构建。

<a id="symbol-index"></a>

## 符号索引

*(自 2.0.0 起)* 每次编译只要进行到绑定了模块——**失败的**编译也算，因为导航在一个无法构建的文件中最为重要——都会刷新

```text
<Project>/Saved/DreamShader/Index/<root>/<source path relative to that root>.index.json
```

（schema `dreamshader-symbol-index`，版本 `1`）：源文件及其包含的每个头文件的声明——种类、名称、签名、类型、文档、位置——每个名称的提及作为一个跨度、解析后的包含路径，以及参数 schema。`<root>` 是 `Project` 或插件的名称，以此将各自持有 `Materials/M_Foo.dss` 的两个源根区分开来。`dsc index` 按相同规则写入同一文件，因此语言服务无论由谁最后编译都只读取一处；文本不会变化的文件不会被重写，因此未发生变化的编译不会唤醒任何监视器。

VS Code 扩展从这些文件中为 `.dss` 源提供转到定义、查找引用、悬停、大纲、签名帮助和重命名，并从
`dreamshader-builtin-catalog.json` 补全 `UE.` / `Substrate.`——即绑定器自己的节点类、引脚、属性、别名
和位置顺序表。

`.dsp` *(自 2.1.0 起)* 的索引也有一个 `pipeline` 部分：绑定时的 `#pragma pipeline` 值、uniform、缓冲区和通道及其绑定与跨度，以及注入点和格式拼写。`.dsp` 词汇表的其余部分见 [`pass-keys.json`](workspace.md#pass-keysjson)。

<a id="custom-pass-shader-files"></a>

## 自定义 Pass 着色器文件

*(自 2.1.0 起)* [`.dsp`](../language-v2/passes.md) 的 HLSL pass 从一个 `.usf` 编译而来，该文件可以位于任何位置：紧邻 `.dsp` 是常见位置，但既不需要源根，也不需要映射的着色器目录，因为引擎只会编译该文件的快照
（[HLSL pass](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)）。源目录监视器仅作用于 DreamShader 源，因此着色器文件有自己的监视器。

| | |
| :-- | :-- |
| 监视范围 | 每个包含某个 `.dsp` 编译所依据文件的目录：每个 `Shader = "..."` 文件，以及 `.dsp` 中某个 `hlsl` 块包含的每个文件，以及它们通过相对路径包含的一切——包括此类包含所指定但尚不存在的文件，因为创建它会改变该 pass——还有通过虚拟路径包含的每个文件，且该虚拟路径既不是 `/Engine/` 也不是 `/Plugin/` |
| 方式 | 每个目录一个监视器，不监视其子树。不存在的目录不会被监视 |
| 触发的文件 | `.usf` 和 `.ush` |
| 刷新时机 | 每次编译 `.dsp` 之后（无论成功与否），以及移除 `.dsp` 时。启动扫描所排队的编译正是注册首批监视器的来源 |
| 门控 | *保存时自动编译*，与保存源文件时相同 |

对上述文件之一的更改会将每个 HLSL pass 从它编译而来的 `.dsp` 排队，**非强制**：
`.dsp` 的[构建键](../generation/caching.md#custom-pass-pipelines)保存其 pass 编译所依据的每个文件的文本，因此一次未改变其中任何内容的保存——比如同一文件夹中无关的头文件——会因其键而被跳过。失去跟踪的监视器（重新扫描）会将每个 `.dsp` 排队。

随后，对 `.dsp` 的一次编译会将每个快照发生变化的 HLSL pass 依次经过以下步骤；快照未变化的 pass 保持原样。

| 步骤 | |
| :-- | :-- |
| 1 | **预检查。** 该 pass 的槽位使用新快照在内存中编译，针对活动功能级别的着色器格式以及项目活动目标平台所面向的每种格式。错误会在导致该错误的 `.usf` 行处报告为 `DSH8322`，像任何其他编译错误一样归档；**不写入任何内容**，该 pass 的先前版本继续运行 |
| 2 | **快照。** 将该文件及其通过相对路径包含的一切复制到 `<DShader>/.dreampass/Slots/<C\|P><NN>/` 中，并重写 `Registry.json` 和该表的注册文件 |
| 3 | **热重载。** 更新管线资产，针对每个活动功能级别同步重新编译槽位着色器类型 `FDreamPassCS` / `FDreamPassPS`——下一帧即运行新代码——并保存该资产 |

被删除的 `.dsp` 会像任何源文件一样记录 `DreamShader source removed, existing generated assets were left
untouched: {Path}`。其槽位会留在注册表中，直到
[`dsc pass-registry -Gc`](commandlet.md#pass-registry) 将它们收集起来。

<a id="dependents"></a>

### 依赖项

一个 `.dsp` 和它周围的 `.dss` 源互相引用，因此一次成功的编译会将另一方排队，非强制：绑定在构建键的跳过之前运行，因此被更改破坏的依赖项会报告其错误，而未被触及的依赖项仅付出一次绑定的代价。

| 成功编译以下内容后 | 桥接将以下内容排队 | 时机 |
| :-- | :-- | :-- |
| 一个 `.dss` 或一个 `.dsi` | 每个 `.dsp`，其某个 pass 将该产物命名为其 `Material` | 编译写入了资产——其报告中有 `Generated …` 行 |
| 一个 `.dsp` | 每个 `.dss`，其 `UE.DreamPassBuffer` 读取该管线——其导出可能已改变 | 编译写入了资产 |
| 一个 `.dss` 或一个 `.dsi` | 每个 `.dsi`，其 `Parent` 是其产物之一，进而再排其产物 | 始终 |

排空的一批会经过排序，使 `.dsp` 在其材料所依赖的源之后编译，而读取某管线缓冲区的 `.dss` 在该 `.dsp` 之后编译。开启 *Verbose Logs* 时，每次排队都会记录
`DreamShader queued {N} instance source(s), {N} pipeline source(s) and {N} pass-buffer reader(s) after '{Source}' compiled.`

<a id="websocket-server"></a>

## WebSocket 服务器

| | |
| :-- | :-- |
| 端点 | `ws://127.0.0.1:17864` |
| 绑定地址 | `127.0.0.1` —— 绝不使用 `0.0.0.0` |
| 端口 | `17864`，固定；没有对应的项目设置 |
| 连接筛选 | 仅当客户端 IP 恰好为 `127.0.0.1` 或 `localhost` 时才接受连接；其他一律拒绝 |
| 更新频率 | 每个编辑器帧 |
| 用途 | 以实时轨道控制流式传输材质预览 |

启动和连接日志：

| 消息 | 严重性 |
| :-- | :-- |
| `DreamShader preview WebSocket server could not load WebSocketNetworking.` | Warning |
| `DreamShader preview WebSocket server could not be created.` | Warning |
| `DreamShader preview WebSocket server failed to listen on 127.0.0.1:{Port}.` | Warning |
| `DreamShader preview WebSocket server listening on 127.0.0.1:{Port}.` | Display |
| `DreamShader preview WebSocket client connected: {Endpoint}` | Display |
| `DreamShader preview WebSocket: {Message}` | Display on success, Error on failure |

监听失败——最常见的情况是端口已被另一个编辑器实例占用——只是一个警告，而非错误：桥接的其余部分会继续运行，只是不进行流式预览。

<a id="framing"></a>

### 帧结构

每条**出站**消息，无论是 JSON 元数据还是原始 PNG，都作为 WebSocket *二进制*帧发送，其载荷为：

```text
+--------------------+----------+------------------+
| length (uint32 LE) | tag (u8) | payload bytes …  |
+--------------------+----------+------------------+
```

| 字段 | 宽度 | 值 |
| :-- | :-- | :-- |
| length | 4 字节，无符号，小端序 | `1 + payload byte count` —— 包含标记字节 |
| tag | 1 字节 | `1` = UTF-8 JSON，`2` = 原始 PNG 字节，`3` = 原始帧（头部 + RGBA8） |
| payload | *length − 1* 字节 | JSON 文本、图像，或原始帧的头部+像素 |

之所以需要标记，是因为传输层始终写入二进制帧，因此 WebSocket 操作码无法区分 JSON 消息与图像。图像以原始字节发送——此协议中没有任何地方使用 Base64。

会话通过 `previewMaterial` 的 `encoding` 字段选择其帧编码：

| `encoding` | 帧传输方式 | 起始版本 |
| :-- | :-- | :-- |
| `"raw"` | 每帧一条自描述的 tag-`3` 消息（头部 + RGBA8），无 JSON 元数据 | `1.7.0` |
| `"png"` *（缺省时的默认值）* | 一条 tag-`1` `previewFrame` JSON 消息，紧接着一条 tag-`2` PNG | `1.5.0` |

`raw` 是浏览器/webview 客户端想要的高效路径——像素通过 `putImageData` 直接绘制到画布上，编辑器端无需 PNG 编码，客户端也无需解码。保留 `png` 是为了兼容早于 `raw` 的客户端。

**关联规则（仅 png）：** JSON `previewFrame` 总是先发送，匹配的 PNG 二进制消息紧接着在同一连接上发送。客户端按到达顺序将它们配对。`raw` 帧是单条消息，无需配对。

<a id="raw-frame-header-tag-3"></a>

#### 原始帧头部（标记 `3`）

tag-`3` 载荷是一个 24 字节的小端序头部，后跟 `width × height × 4` 字节的 RGBA8 像素（alpha 始终为 `255`）。

| 偏移 | 字段 | 类型 | 备注 |
| :-- | :-- | :-- | :-- |
| 0 | `version` | u8 | `1` |
| 1 | `format` | u8 | `1` = RGBA8 |
| 2 | `width` | u16 | 像素 |
| 4 | `height` | u16 | 像素 |
| 6 | `flags` | u16 | 帧标志位，见下文 |
| 8 | `frameIndex` | u32 | 单调递增，从 `0` 开始；在 `ackFrameIndex` 中回传该值 |
| 12 | `orbitYaw` | f32 | 渲染该帧时的 yaw 值 |
| 16 | `orbitPitch` | f32 | 渲染该帧时的 pitch 值 |
| 20 | `probeLine` | u32 | 活动探针解析到的源行，或 `0` |

帧标志位：

| 位 | 名称 | 含义 |
| :-- | :-- | :-- |
| `1 << 0` | Compiling | 着色器映射仍在构建；像素为回退材质 |
| `1 << 1` | ProbeActive | 已附加断点，该帧显示的是该值，而非材质 |
| `1 << 2` | ProbePending | 已请求断点但尚未能附加 |
| `1 << 3` | Keyframe | 发送该帧是因为有内容发生变化，而非时钟跳动 |

**入站**消息是纯 UTF-8 JSON，没有长度前缀，也没有标记字节。

<a id="message-types"></a>

### 消息类型

| 方向 | `type` | 用途 |
| :-- | :-- | :-- |
| 客户端 → 编辑器 | `previewMaterial` | 为 `.dss`、`.dsi` 或 `.dsm` 启动预览会话 |
| 客户端 → 编辑器 | `previewControl` | 调整现有会话 |
| 客户端 → 编辑器 | `setProbe` | 将断点式探针附加到 Graph 行 |
| 客户端 → 编辑器 | `clearProbe` | 分离活动探针 |
| 编辑器 → 客户端 | `previewResult` | 会话启动结果，或流中途错误 |
| 编辑器 → 客户端 | `previewFrame` | 后续 PNG 的元数据 *（仅 png 编码）* |
| 编辑器 → 客户端 | `probeState` | 探针是已附加、待处理还是已清除 |

分发会以不区分大小写的方式同时读取 `type` 和 `action`：当**任一**字段等于 `previewMaterial` 时，该消息会被视为 `previewMaterial`。`previewControl`、`setProbe` 和 `clearProbe` 仅根据 `type` 匹配。其他任何内容都会被静默忽略。

无法解析的 JSON 会立即得到回复：

```json
{"type":"previewResult","status":"error","message":"Invalid DreamShader preview request JSON.","updatedAtUtc":"<ISO-8601>"}
```

<a id="previewmaterial--client--editor"></a>

### `previewMaterial` —— 客户端 → 编辑器

| 字段 | 类型 | 默认值 | 约束 / 效果 |
| :-- | :-- | :-- | :-- |
| `type` 或 `action` | string | — | 必须等于 `previewMaterial` |
| `sourceFile` | string | `""` | 必须存在且为 `.dss`、`.dsi` 或 `.dsm` —— 一种可构建材质或材质实例的源 |
| `mesh` | string | `""` → `sphere` | 参见 [Meshes](#meshes) |
| `width` | number | `512` | 取整，限制在 `[64, 2048]` |
| `height` | number | `512` | 取整，限制在 `[64, 2048]` |
| `orbitYaw` | number，度 | `-157.5` | 相机 yaw；与 `USceneThumbnailInfo` 自身的默认值匹配 |
| `orbitPitch` | number，度 | `-11.25` | 相机 pitch；引擎端不进行限制 |
| `requestId` | string | *（缺省）* | 在本会话的每条消息上回传 |
| `frameRate` | number，FPS | `2.0` | `<= 0` 禁用流式传输；否则限制在 `[0.25, 60.0]` 并转换为帧间隔 |
| `stream` | bool | `true` | 流式传输还要求帧间隔为正 |
| `encoding` | string | `"png"` | `"raw"` 表示 tag-`3` RGBA8 帧；`"png"` 表示旧式 JSON+PNG 对 |
| `force` | bool | `false` | `true` 即使源哈希未变也会重新生成材质（显式的 **Refresh** 操作）；`false` 复用上次生成，因此因相机/网格变化或桥接已捕获的保存而重新请求不会产生重新生成开销 |

成功后，编辑器会瞬态解析并编译该材质，发送 `previewResult`，发送 `probeState`，并安装一个每连接会话，从下一 tick 开始流式发送帧。`png` 编码还会在 `Bridge/Preview/` 下保存第一帧 PNG，写入 `preview.json`，并立即发送该 PNG；`raw` 编码会像其他所有帧一样从 tick 循环流式发送其第一帧。使用 `stream: false` 或 `frameRate <= 0` 时，raw 会在着色器编译完成后恰好发送一帧，而不启用流式传输。失败时，该连接的会话会被移除。

帧元数据描述捕获时的控件状态。在回读期间收到的更改会请求新的关键帧；它们不会重新标记待处理像素。新的 `previewMaterial` 请求会取代前一次请求的任何待处理帧，即使渲染上下文被复用也是如此。

<a id="previewcontrol--client--editor"></a>

### `previewControl` —— 客户端 → 编辑器

当连接没有会话时会被完全忽略。当消息的 `requestId` 和会话的 `requestId` 都非空时，它们必须匹配，否则该消息会被忽略。

| 字段 | 类型 | 缺省时 | 效果 |
| :-- | :-- | :-- | :-- |
| `requestId` | string | 不应用任何守卫 | 会话守卫 |
| `stream` | bool | 保持当前值 | 启用或禁用流式传输 |
| `frameRate` | number | 保持当前速率 | 新的帧间隔；`<= 0` 停止流式传输，否则钳制到 `[0.25, 60.0]` |
| `orbitYaw` | number | 保持当前角度 | 拖拽旋转 |
| `orbitPitch` | number | 保持当前角度 | 拖拽旋转 |
| `width` / `height` | number | 保持当前尺寸 | 调整渲染目标尺寸；钳制到 `[64, 2048]` |
| `mesh` | string | 保持当前网格体 | 更改预览图元 |
| `ackFrameIndex` | number | 无确认 | 仅当它与当前未确认帧匹配时才释放流控门；过时或未来的索引会被忽略 |

> [!NOTE]
> 除 `requestId`/`ackFrameIndex` 之外，每个字段在省略时都会保持其当前值 *(自
> `1.7.0` 起)*，因此仅发送帧确认或摄像机微调不再重置速率。流式传输会话中
> 缺少 `ackFrameIndex` 仍会被视为"客户端仍存活"，从而释放流控门。

<a id="setprobe--client--editor"></a>

### `setProbe` — 客户端 → 编辑器

附加一个探针（对应材质编辑器中 **Start Previewing Node** 的文本源版本）：在 Graph 行上绑定
的值会替换预览网格体上的材质。当连接没有会话时忽略。

| 字段 | 类型 | 默认值 | 效果 |
| :-- | :-- | :-- | :-- |
| `type` | string | — | 必须等于 `setProbe` |
| `line` | number | `0` | 会话源文件中从 1 开始的行号；将选择该行或其之后最近的绑定 |
| `name` | string | `""` | 当多个绑定共享同一行时用于消歧 |

回复为一个 `probeState`。在源尚未生成之前设置的探针会被记住，并在下次生成时附加，
而不是失败。

<a id="clearprobe--client--editor"></a>

### `clearProbe` — 客户端 → 编辑器

分离活动探针；网格体恢复为整个材质。回复一个 `probeState`，其中
`status` 为 `cleared`。

<a id="probestate--editor--client"></a>

### `probeState` — 编辑器 → 客户端

在任何 `setProbe`/`clearProbe` 之后、在 `previewMaterial` 重新解析探针之后，
以及每当重新生成移动或丢弃它时发送。

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `type` | string | 始终为 `probeState` |
| `requestId` | string | 为空时省略 |
| `sourceFile` | string | 规范化绝对源路径 |
| `status` | string | `attached`、`pending` 或 `cleared` |
| `line` / `column` | number | 当 `attached` 时存在；探针解析到的行 |
| `name` | string | 当 `attached` 时存在；绑定的变量 |
| `componentCount` | number | 当 `attached` 时存在；该值的通道数 |
| `message` | string | 当 `pending` 时存在；尚未附加的原因 |

<a id="previewresult--editor--client"></a>

### `previewResult` — 编辑器 → 客户端

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `type` | string | 始终为 `previewResult` |
| `requestId` | string | **为空时省略** |
| `status` | string | `ready` 或 `error` |
| `sourceFile` | string | 规范化绝对源路径 |
| `assetPath` | string | 解析后的对象路径 |
| `imagePath` | string | 绝对 PNG 路径 — **仅在初始结果中**；流中途的错误变体省略它 |
| `mesh` | string | 解析后的网格体名称 |
| `message` | string | 人类可读文本 |
| `updatedAtUtc` | string | ISO-8601 UTC |

流中途发送的带有 `status: "error"` 的 `previewResult` 也会关闭流式传输；会话将停止投递帧，
直到新的 `previewMaterial` 或 `previewControl` 重新启用它。

<a id="previewframe--editor--client"></a>

### `previewFrame` — 编辑器 → 客户端

仅为 `png` 编码发送，且总是紧跟一条携带 PNG 的标签 `2` 二进制消息。
使用 `raw` 编码的会话会改为发送标签 `3` 帧，并且从不发送 `previewFrame`。

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `type` | string | 始终为 `previewFrame` |
| `requestId` | string | 为空时省略 |
| `sourceFile` | string | 规范化绝对源路径 |
| `assetPath` | string | 解析后的对象路径 |
| `mesh` | string | 解析后的网格体名称 |
| `frameIndex` | number | 单调递增，从 `0` 开始 |
| `flags` | number | 帧标志位（与原始帧头相同的集合） |
| `updatedAtUtc` | string | ISO-8601 UTC |

<a id="streaming-state-machine"></a>

### 流式传输状态机

按连接计算，每个编辑器帧评估一次。

| 状态字段 | 默认值 | 用途 |
| :-- | :-- | :-- |
| `RequestId`、`SourceFilePath`、`AssetPath`、`Mesh` | `""` | 会话标识 |
| `OrbitYaw` / `OrbitPitch` | `-157.5` / `-11.25` | 摄像机角度，度 |
| `Width` / `Height` | `512` / `512` | 渲染尺寸 |
| `FrameIntervalSeconds` | `0.5`（2 FPS） | 速率限制 |
| `FrameIndex` | `0` | 出站计数器 |
| `LastAckFrameIndex` | `-1` | 客户端最后确认的索引 |
| `bFrameInFlight` | `false` | 流控门 |
| `bStreaming` | `false` | 总开关 |

每 tick 执行：

1. 当既没有流式传输也没有待处理的原始一次性渲染，或没有渲染上下文时，不执行任何操作。
   无效材质会以错误停止会话。
2. 如果有 GPU 回读正在进行中，在不阻塞的情况下轮询它。就绪 → 投递该帧（一个标签 `3`
   原始帧，或一对 `previewFrame`+PNG）并提升进行中门。错误 → 发送错误
   `previewResult` 并停止流式传输。两者皆非 → 返回并在下一个 tick 重试。
3. 否则，当上一帧已被确认（或其确认超时）且帧间隔已过时，开始新的一帧。原始一次性渲染
   还会等待其着色器映射。
4. 开始帧失败会发送错误 `previewResult` 并停止流式传输。

循环之上有两种效率行为 *(自 `1.7.0` 起)*：

- **去重。** 像素与上一发送帧完全相同的帧会被丢弃，因此静态材质在稳定后不消耗带宽。
- **空闲退避。** 在若干相同帧之后，渲染时钟放宽到约 2 FPS，直到发生变化（编辑的重新编译、
  摄像机/网格体更改、探针更改）。当着色器映射仍在编译时渲染的帧从不退避 — 编译落地正是
  所等待的变化。任何变化都会恢复完整速率，并强制下一帧作为关键帧通过去重。

确认始终未到达的帧（隐藏的标签页、丢失的消息）不会被无限等待；在两秒超时后，下一帧
无论如何都会开始。有效帧率受 60 FPS 钳制、客户端的确认、空闲退避以及编辑器的实际 tick 速率限制。

<a id="meshes"></a>

### 网格体

不区分大小写比较。任何无法识别的值（包括空字符串）都会回退到 `sphere`，并且结果消息
报告的就是该回退名称。

| 值 | 预览图元 |
| :-- | :-- |
| `plane` | 平面 |
| `cube` | 立方体 |
| `cylinder` | 圆柱体 |
| `shaderball` | 着色器球 |
| `sphere` | 球体 |
| *(其他任何值)* | 球体 |

网格与环绕视图是通过写入**材质资产自身**的缩略图信息来应用的，这与原生材质编辑器的预览形状按钮
和拖动环绕视口所写入的字段相同。UI 域的材质无论请求的形状如何，
始终绘制在平面上。完整的渲染器行为及其限制见 [预览](preview.md)。

<a id="artifacts"></a>

## 产物

桥接写入的所有内容，以及由谁读取。

| 路径 | 方向 | 插件是否回读？ |
| :-- | :-- | :-- |
| `Requests/*.json` | 入站 | 被消费并删除 |
| `Responses/<requestId>.json` | 出站 | 否 — 客户端会删除自己的 |
| `status.json` | 出站 | 否 |
| `owner.lock` | 内部 | 是 — 每个编辑器都会读取它，以判断自己是否是为该项目提供服务的那一个 |
| `diagnostics.json` | 出站 | 否 — [材质内容浏览器](material-browser.md) 从桥接内存中的诊断存储读取相同的记录，因此它永远不会等待文件写入 |
| `diagnostics/index.json`、`diagnostics/<md5>.json` | 出站 | 否 |
| `bridge.db` | 出站 | **否** |
| `material-expressions.json` | 出站 | 否 |
| `settings.json` | 出站 | 否 |
| `substrate-builtins.json` | 出站 | 否 |
| `pass-keys.json` *(自 2.1.0 起)* | 出站 | 否 — 见 [工作区](workspace.md#pass-keysjson) |
| `dreamshader-builtin-catalog.json` | 出站 | 否 — 也由 `dsc export-catalog` 写入，并在 *打开 DreamShader 工作区* 时写入 |
| `preview.json` | 出站 | 否 |
| `../Index/<root>/<source>.index.json` | 出站 | 否 — 某个源文件的[符号索引](#symbol-index)，位于桥接目录旁边而非其中 |
| `Preview/<stem>-<crc32>.png` | 出站 | 否 |
| `<SourceDirectory>/DreamShader.code-workspace` | 出站 | 否 — 见 [工作区](workspace.md) |

<a id="diagnosticsjson"></a>

### `diagnostics.json`

```json
{ "version": 1, "updatedAtUtc": "<ISO-8601>", "files": [ { "path": "…", "diagnostics": [ … ] } ] }
```

诊断对象字段。可选字段为空时会完全省略。

| 字段 | 类型 | 出现条件 | 值 |
| :-- | :-- | :-- | :-- |
| `message` | 字符串 | 始终 | 诊断文本 |
| `detail` | 字符串 | 非空时 | 原始底层行 |
| `stage` | 字符串 | 非空时 | 某次编译的记录：来自其代码范围内，为 `preprocess`、`parse`、`bind`、`ir`、`generate`、`tools` 或 `compile`；否则为 `generate`、`materialCompile` 或 `virtualFunctionSync` |
| `assetPath` | 字符串 | 非空时 | 所涉及资产的对象路径 |
| `shaderPlatform` | 字符串 | 非空时 | 仅材质编译诊断 |
| `qualityLevel` | 字符串 | 非空时 | 仅材质编译诊断 |
| `code` | 字符串 | 非空时 | 某次编译的记录：其 `DSHnnnn` 代码；否则为 `generate-error`、`material-compile` 或 `virtual-function-sync` |
| `line` | 数字 | 始终 | 从 1 开始，默认为 `1` |
| `column` | 数字 | 始终 | 从 1 开始，默认为 `1` |
| `severity` | 字符串 | 始终 | `error`、`warning` 或 `info` |
| `source` | 字符串 | 始终 | `DreamShader Lang2`、`DreamShader`、`DreamShader Generate`、`DreamShader Material Compile` 或 `DreamShader VirtualFunction` |

> [!NOTE]
> 一次编译会归档其自身的记录——每条都带有其 `DSHnnnn` 代码、严重级别和位置：失败的编译
> 会归档全部，成功的编译只会归档其警告和注释 *(自 2.1.0 起；在此之前，成功的编译会清空
> 该文件的诊断，其警告仅出现在日志中)*。按严重级别过滤的客户端应将
> 缺失或未知的值视为错误。

阶段、代码和来源一同传递：

| `stage` | `code` | `source` | 由谁产生 |
| :-- | :-- | :-- | :-- |
| 来自代码 | `DSHnnnn` | `DreamShader Lang2` | 某源文件的一次编译：失败编译的每条记录，成功编译的警告 |
| `generate` | `generate-error` | `DreamShader Generate` | 一次失败的编译，其记录无法解释它——没有编译器模块，或拒绝原因不属于诊断信息 |
| `materialCompile` | `material-compile` | `DreamShader Material Compile` | 生成的材质上的着色器编译错误 |
| `virtualFunctionSync` | `virtual-function-sync` | `DreamShader VirtualFunction` | 启动时的 `VirtualFunction` 声明扫描 |

一次编译的记录会携带其自身的行和列。对于 `generate-error`，位置会从
形如 `<path>(<line>,<column>): <message>` 的消息中恢复。行和列都必须为数字，
并被钳制为不小于 `1`；无法解析位置的行会报告为 `1,1`，并去掉开头的
`"<source path>: "` 前缀。

材质编译诊断会携带形如
`[{ShaderPlatform} / {QualityLevel}] {Message}` 的显示消息，并按来源、平台、质量、
消息、行和列进行去重。

> [!NOTE]
> 自 UE 5.7 起，`shaderPlatform` 字段携带一个着色器格式名称，例如 `PCD3D_SM6`，
> 因为该枚举会遍历每个着色器平台。在 UE 5.7 以下，它携带的是功能级名称，
> 例如 `SM6`。

<a id="diagnostics"></a>

### `diagnostics/`

同一份数据按每个源文件分片，以便客户端仅重新读取发生变化的内容。

`index.json`：

```json
{ "version": 1, "updatedAtUtc": "<ISO-8601>", "files": [ { "path": "…", "file": "<md5>.json", "count": 2 } ] }
```

| 字段 | 类型 | 备注 |
| :-- | :-- | :-- |
| `path` | 字符串 | 规范化后的源文件路径 |
| `file` | 字符串 | 同级文件名——规范化路径的 MD5 加上 `.json` |
| `count` | 数字 | 该分片中的诊断数量 |

每个 `<md5>.json` 都保存与 `diagnostics.json` 相同的 `files[]` 条目结构。过期的分片——任何
名称不再是活动键的 `*.json`（`index.json` 除外）——会在每次写入时被删除。
所有三个接收端（`diagnostics.json`、`diagnostics/`、数据库）会一同重写。

<a id="bridgedb"></a>

### `bridge.db`

SQLite，以 `PRAGMA journal_mode=WAL` 和 `PRAGMA synchronous=NORMAL` 以读写创建模式打开，因此
`bridge.db-wal` 和 `bridge.db-shm` 会出现在它旁边。

```sql
CREATE TABLE IF NOT EXISTS meta(
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS settings_mappings(
    kind TEXT NOT NULL, alias TEXT NOT NULL, normalized_alias TEXT NOT NULL,
    value INTEGER NOT NULL, name TEXT, display_name TEXT, source TEXT NOT NULL,
    PRIMARY KEY(kind, normalized_alias));

CREATE TABLE IF NOT EXISTS material_expressions(
    name TEXT PRIMARY KEY, class_name TEXT NOT NULL, path_name TEXT NOT NULL,
    default_output_type TEXT NOT NULL, json TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS substrate_builtins(
    name TEXT PRIMARY KEY, qualified_name TEXT NOT NULL, class_name TEXT NOT NULL,
    output_type TEXT NOT NULL, is_substrate_output INTEGER NOT NULL,
    detail TEXT, example TEXT, snippet TEXT, json TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS diagnostics(
    path TEXT PRIMARY KEY, json TEXT NOT NULL, updated_at_utc TEXT NOT NULL);

CREATE INDEX IF NOT EXISTS idx_settings_mappings_kind_alias
    ON settings_mappings(kind, alias);
```

| 表 | 内容 |
| :-- | :-- |
| `meta` | 生成时间戳，每个生产者一行 |
| `settings_mappings` | 每个 `ShadingModel` / `BlendMode` / `MaterialDomain` 别名，及其枚举值以及它来自项目设置还是内置表 |
| `material_expressions` | 每个反射的 `UMaterialExpression` 一行，完整清单条目在 `json` 中 |
| `substrate_builtins` | 每个 `Substrate.*` 内置项一行，完整清单条目在 `json` 中 |
| `diagnostics` | 每个源文件一行，该文件的诊断数组在 `json` 中 |

`meta` 键：`settings.generatedAt`、`materialExpressions.generatedAt`、
`substrateBuiltins.generatedAt`、`diagnostics.updatedAt`。

> [!WARNING]
> 该数据库**从插件一侧看是只写的**，且不是持久状态。它会在
> 桥接启动*和*关闭时被删除，并且每个写入者都会在一个事务内替换其整张表（先 `DELETE FROM` 再插入）
> 而不是更新行。插件中没有任何东西会读回行。不要在其中存储客户端状态；
> 把它视为 JSON 产物的一种便于查询的镜像，仅在编辑器运行时有效。

打开数据库失败只是一个警告——`Failed to open DreamShader bridge database: {Path}`，或
`Failed to open DreamShader bridge database for diagnostics: {Path}`——并且 JSON 接收端仍会被
写入。替换的是数据库路径，而不是原因。

<a id="manifests"></a>

### 清单

这三个文件都会在桥接启动时写入，并在每次打开工作区时再次写入。每个文件都带有一个 `generatedAt` ISO-8601 UTC 时间戳。

| 文件 | `schema` | `version` | 载荷 |
| :-- | :-- | :-- | :-- |
| `settings.json` | `DreamShader.Settings` | `1` | `mappings: { ShadingModel[], BlendMode[], MaterialDomain[] }` |
| `material-expressions.json` | `DreamShader.MaterialExpressions` | `1` | `expressions[]` |
| `substrate-builtins.json` | `DreamShader.SubstrateBuiltins` | `1` | 不支持时为 `builtins[]`、`supported` 和 `unsupportedReason` |

| 清单 | 条目字段 |
| :-- | :-- |
| `settings.json` | `alias`、`value`（数字）、`name`、`displayName`、`source`（`user` 或 `builtin`） |
| `material-expressions.json` | `name`、`className`、`pathName`、`defaultOutputType`、`properties[]`（`name`、`type`、`isInput`）、`inputs[]`、`outputs[]`（`index`、`name`、`componentCount`、`outputType`） |
| `substrate-builtins.json` | `name`、`qualifiedName`、`className`、`outputType`、`isSubstrateOutput`、`detail`、`example`、`snippet`、`parameters[]`（`qualifier` —— 始终为 `in` ——、`type`、`name`、可选的 `placeholder`） |

用户定义的别名条目会先于内置别名条目输出，归一化形式已被用户别名占用的内置别名会被丢弃。别名在每种类型内部分别排序。材质表达式按 `name` 排序。

`pass-keys.json` *（自 2.1.0 起）* 紧跟在 `settings.json` 之后写入，写入时机相同，因为它列出的 pass 层也是项目设置。Schema `DreamShader.PassKeys`，版本 `1`；它没有 `bridge.db` 表。其内容位于 [工作区](workspace.md#pass-keysjson)。

> [!NOTE]
> 在 UE 5.4 以下，`substrate-builtins.json` 带有一个空的 `builtins` 数组、`"supported": false` 和
> `"unsupportedReason": "Substrate builtins require Unreal Engine 5.4 or newer."`，并且
> `MSM_Strata` 着色模型被排除在 `settings.json` 之外。

日志记录：`Wrote DreamShader MaterialExpression manifest: {Path}` /
`Failed to write DreamShader MaterialExpression manifest: {Path}`、
`Wrote DreamShader settings manifest: {Path}` /
`Failed to write DreamShader settings manifest: {Path}`，以及
`Wrote DreamShader Substrate builtin manifest: {Path}` /
`Failed to write DreamShader Substrate builtin manifest: {Path}`。无法读取的设置会产生
`Failed to read DreamShader settings for Bridge manifest.` 并且不生成文件。

<a id="previewjson"></a>

### `preview.json`

由每次一次性预览重写，无论请求来自哪种传输方式。

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `version` | 数字 | 始终为 `1` |
| `requestId` | 字符串 | 仅在非空时出现 |
| `status` | 字符串 | `ready` 或 `error` |
| `sourceFile` | 字符串 | 归一化的绝对源路径 |
| `assetPath` | 字符串 | 解析后的对象路径 |
| `imagePath` | 字符串 | 绝对 PNG 路径 |
| `mesh` | 字符串 | 解析后的网格体名称 |
| `message` | 字符串 | 成功或错误文本 |
| `updatedAtUtc` | 字符串 | ISO-8601 UTC |

PNG 名称是 `<sanitized source stem>-<crc32 of the project-relative path, 8 hex digits>.png`，位于
`Bridge/Preview/` 下；如果某个源的主干名净化后为空，则变为 `DreamShaderPreview`。因此同一个
源始终映射到同一个文件名，并且每次渲染都会覆盖它。

<a id="kill-switch"></a>

## 终止开关

| | |
| :-- | :-- |
| 开关 | `-NoDreamShaderEditorBridge` |
| 形式 | 一个裸命令行参数；无值 |

| 效果 | 详情 |
| :-- | :-- |
| 桥接 | 不创建——不轮询请求、没有 WebSocket 服务器、没有目录监视器、没有诊断写入器、没有清单、没有 `bridge.db` |
| 启动时临时生成 | 不运行 |
| 材质内容浏览器 | 不注册标签页和菜单项 |
| 工具菜单、工具栏、上下文菜单 | 不注册 |
| Cook 钩子 | 不受影响——它安装在另一条代码路径上 |

在 commandlet 进程中，无论是否带该开关，桥接也永远不会创建；请参阅
[Commandlet](commandlet.md)。对于在普通编辑器进程中运行的自动化任务（例如 `-ExecCmds="Automation RunTests …"`），如果必须没有文件监视器、生成 pass 或
监听套接字，请使用该开关。

<a id="example"></a>

## 示例

从 PowerShell 请求正在运行的编辑器重新编译一个文件，并以原子方式写入请求：

```powershell
$req = @{ action = "recompile"; scope = "file"; sourceFile = "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" }
$dir = "C:\Projects\MyGame\Saved\DreamShader\Bridge\Requests"
$tmp = Join-Path $env:TEMP ("ds-" + [guid]::NewGuid() + ".json")
$req | ConvertTo-Json | Set-Content -Path $tmp -Encoding utf8
Move-Item $tmp (Join-Path $dir ([IO.Path]::GetFileName($tmp)))
```

在立方体上请求一次性 512×512 预览并读取结果：

```json
{
  "action": "previewMaterial",
  "sourceFile": "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm",
  "mesh": "cube",
  "width": 512,
  "height": 512,
  "requestId": "8f3c1b"
}
```

之后 `Saved/DreamShader/Bridge/preview.json`：

```json
{
  "version": 1,
  "requestId": "8f3c1b",
  "status": "ready",
  "sourceFile": "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm",
  "assetPath": "/Game/Materials/M_Sample.M_Sample",
  "imagePath": "C:/Projects/MyGame/Saved/DreamShader/Bridge/Preview/M_Sample-3fa91c07.png",
  "mesh": "cube",
  "message": "Rendered preview for /Game/Materials/M_Sample.M_Sample.",
  "updatedAtUtc": "2026-08-02T09:14:22Z"
}
```

通过 WebSocket 进行的流式会话，客户端侧，按顺序：

```text
→ {"type":"previewMaterial","sourceFile":"…/M_Sample.dsm","mesh":"shaderball",
   "width":512,"height":512,"requestId":"8f3c1b","stream":true,"frameRate":12}
← [len][1] {"type":"previewResult","requestId":"8f3c1b","status":"ready", … "imagePath":"…"}
← [len][2] <PNG bytes>
← [len][1] {"type":"previewFrame","requestId":"8f3c1b","frameIndex":0, …}
← [len][2] <PNG bytes>
→ {"type":"previewControl","requestId":"8f3c1b","ackFrameIndex":0,"frameRate":12,"orbitYaw":-140.0}
← [len][1] {"type":"previewFrame","requestId":"8f3c1b","frameIndex":1, …}
← [len][2] <PNG bytes>
```

注意控制消息上重复的 `frameRate`：省略它会使会话降至 2 FPS。

<a id="see-also"></a>

## 另请参阅

- [工作区与编辑器扩展](workspace.md) —— 工作区文件以及驱动此协议的扩展
- [Commandlet](commandlet.md) —— 无头路径，桥接在此处从不启动
- [预览](preview.md) —— `previewMaterial` 背后的渲染器及其限制
- [材质内容浏览器](material-browser.md) —— 显示每个源诊断信息的标签页，从存储中读取
- [编辑器集成](editor-integration.md) —— 请求操作所镜像的菜单命令
- [VirtualFunction 工具](virtual-function-tools.md) —— 生成 `virtualFunctionSync` 诊断信息的同步 pass
- [包](packages.md) —— 完整重新扫描实际排队的文件
- [HLSL pass](../runtime/hlsl.md) —— `.usf` 保存所经过的预检查、快照和槽位
- [Commandlet](commandlet.md#pass-registry) —— `pass-registry`，用于已删除 `.dsp` 的槽位
- [项目设置](../settings/project.md) —— `SaveDebounceSeconds`、`bAutoCompileOnSave`、`GeneratedShaderDirectory`
- [材质枚举](../settings/material-enums.md) —— 导出到 `settings.json` 的别名表
- [Substrate 内置项](../builtins/substrate.md) —— 导出到 `substrate-builtins.json` 的目录
- [UE.Expression](../builtins/ue-expression.md) —— `material-expressions.json` 所描述的反射接口
- [诊断](../diagnostics/index.md) —— 诊断信息出现的位置以及如何阅读它们；[每个代码](../diagnostics/README.md)
