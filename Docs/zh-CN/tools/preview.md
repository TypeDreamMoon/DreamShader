---
title: "预览"
description: "预览"
---

<a id="preview"></a>

# 预览

> [DreamShader](../index.md) » [工具](index.md) » **预览**

编辑器端的渲染器，用于编译材质源并将其材质渲染为 PNG，可执行一次，也可作为帧流连续渲染。

| | |
| :-- | :-- |
| 实现于 | `DreamShaderEditor` 模块 |
| 接受 | 用于构建材质或材质实例的源：`.dss`、`.dsi` 或 `.dsm` *(`.dss` 和 `.dsi` 自 2.0.0 起)* |
| 产生 | 通过 WebSocket 流式传输的帧；文件/`png` 路径下的 `<Project>/Saved/DreamShader/Bridge/Preview/` 中的 PNG 文件，外加一个 `preview.json` 清单 |
| 传输 | 桥接请求文件（一次性）· `127.0.0.1:17864` 上的 WebSocket（一次性或流式） |
| 流式传输自 | `1.5.0` 起；原始 RGBA8 帧和 Graph 断点自 `1.7.0` 起 |

渲染在游戏线程的 ticker 上运行。流式传输路径的存在正是为了不阻塞该线程。

> [!NOTE]
> 自 `1.7.0` 起，流式传输路径默认使用**原始 RGBA8 帧**（`encoding: "raw"`）：编辑器读回渲染目标并将像素按原样发送，不进行 PNG 编码，客户端将它们直接绘制到画布上。这正是能够流畅实现 30–60 FPS 流式传输的原因。旧的 JSON 元数据加 PNG 路径仍可作为 `encoding: "png"` 供旧版客户端使用。会话还会对相同帧进行**去重**，并在画面静止后**退避**到较低的闲置速率，因此静止的预览几乎不消耗资源。有关线路格式，请参阅[桥接 » 分帧](bridge.md#framing)。

<a id="breakpoints-probe-preview"></a>

## 断点（探针预览）

*自 `1.7.0` 起。* 在材质代码的某一行上设置的断点——`.dsm` 中的 `Graph` 行、`.dss` 中材质主体的语句 *(自 2.0.0 起)*——（在 DreamShaderLang VS Code 扩展中按 F9）会在网格体上预览**该行绑定的值**，而不是成品材质——这是材质编辑器右键**开始预览节点**的文本对应功能。

| 方面 | 行为 |
| :-- | :-- |
| 选择 | 一次一个探针——活动源中最上方启用的断点 |
| 行吸附 | 设置在空白/注释行上的断点会向前吸附到下一个绑定值的行 |
| 显示内容 | 绑定的值被路由到无光照的自发光预览中（当该值为 MaterialAttributes / Substrate 时则为其中之一）；纹理对象使用默认坐标采样 |
| 编译前设置 | 记为*待处理*，并在下一次生成时附加 |
| 重新编译后 | 自动重新解析；行可能会移动，客户端会被告知新的行 |

该机制复用了引擎自身的预览材质机制：一个临时的 `UPreviewMaterial`，它共享所生成材质的表达式集合，并将被探测的节点接入其中，通过 `FMaterialUpdateContext` 重新编译。`UPreviewMaterial` 受限的 `ShouldCache` 将该重新编译限制在少量着色器内。协议侧为 `setProbe` / `clearProbe` / `probeState` ——请参阅[桥接 » 消息类型](bridge.md#message-types)。

<a id="the-two-paths"></a>

## 两条路径

| | 一次性缩略图 | 流式预览 |
| :-- | :-- | :-- |
| 触发方式 | 一个 `previewMaterial` 请求文件，或一条 `stream` 为 false 的 `previewMaterial` WebSocket 消息 | 一条 `stream` 为 true（默认值）的 `previewMaterial` WebSocket 消息 |
| 读回 | 文件/PNG：同步；WebSocket 原始：异步 GPU 读回 | 异步——将 GPU 读回入队，并在之后的 tick 上轮询 |
| 等待着色器编译 | 是；WebSocket 原始会跨 tick 等待而不阻塞编辑器 | 否 |
| 摄像机控制 | 请求文件路径：**否**——不读取轨道字段。WebSocket 路径：是 | 是，通过 `previewControl` |
| 结果 | 文件/PNG：一个 PNG 加 `preview.json`；WebSocket 原始：恰好一帧 RGBA8 | 通过套接字每 tick 一帧原始 RGBA8（`encoding: "raw"`），或旧版 `preview.json` 加带标签的 PNG 对（`encoding: "png"`） |
| Tick 速率 | 0.1 秒的桥接 ticker | 每一帧 |

> [!NOTE]
> 流式传输路径不等待着色器编译。刚编辑过的材质的早期帧可能会显示先前或默认的着色器，直到编译完成；之后的帧会予以纠正。一次性路径会等待着色器就绪。文件/PNG 请求可能会短暂阻塞编辑器；原始请求会异步交付其单帧，包括在 `frameRate <= 0` 时。

帧会保留渲染开始时使用的摄像机、探针、编码和尺寸。在 GPU 读回期间更改的控件会应用于下一帧。启动另一个预览请求会丢弃前一个请求的任何待处理帧，因此它无法在新的请求 ID 下被发送。

[材质内容浏览器](material-browser.md#inspector)的检查器也通过此渲染器进行渲染：一个 224×224 的预览，异步执行，每次编译时重新渲染。1.x 的 *Dream Shader Gen* 页面及其静态资产缩略图已被移除。

<a id="request-fields"></a>

## 请求字段

| 字段 | 类型 | 默认值 | 备注 |
| :-- | :-- | :-- | :-- |
| **`sourceFile`** | string | — | 必须是已存在的 `.dss`、`.dsi` 或 `.dsm` |
| `width` | integer | `512` | 限制在 `[64, 2048]` |
| `height` | integer | `512` | 限制在 `[64, 2048]` |
| `mesh` | string | `sphere` | 见[网格体](#meshes) |
| `orbitYaw` | number | `-157.5` | 度数。仅限 WebSocket |
| `orbitPitch` | number | `-11.25` | 度数。仅限 WebSocket |
| `requestId` | string | *（缺省）* | 在每次回复和帧上回显 |
| `stream` | boolean | `true` | 仅限 WebSocket。`false` 渲染一帧后停止 |
| `frameRate` | number | `2.0` | 仅限 WebSocket。`<= 0` 禁用流式传输；否则限制在 `[0.25, 60.0]` |

尺寸限制会分别在三个点上独立应用——读取请求文件时、解析 WebSocket 消息时，以及渲染上下文内部——因此没有任何路径能够绕过它。

轨道默认值与 Unreal 自身的场景缩略图默认值相匹配，因此省略两个角度的请求会从与内容浏览器图块相同的视点进行渲染。

<a id="meshes"></a>

## 网格体

网格体名称的比较不区分大小写。任何无法识别的名称，包括空字符串，都会回退到球体。

| 值 | 图元 |
| :-- | :-- |
| `plane` | 平面 |
| `cube` | 立方体 |
| `cylinder` | 圆柱体 |
| `shaderball` | 着色器球 |
| *（任何其他值，或缺省）* | 球体 |

> [!WARNING]
> 未知的网格体名称会被**静默接受**并渲染一个球体。没有诊断信息。请检查 `preview.json` 或 `previewResult` 回复中回显的 `mesh` 字段，以查看实际使用的内容。

形状和两个轨道角度会在每次渲染前写入**材质资产自身**的场景缩略图信息——正是原生材质编辑器的预览形状按钮和拖拽轨道视口所写入的那些字段。如果材质没有该信息，则会延迟创建一个。

> [!NOTE]
> 由于设置位于资产上，预览渲染会更改该材质的内容浏览器图块和材质编辑器预览视口所使用的形状和摄像机。

<a id="camera"></a>

## 摄像机

| 方面 | 行为 |
| :-- | :-- |
| Yaw | 自由；两侧均无限制 |
| Pitch | **引擎侧无限制**。编辑器扩展在发送前会大致限制在 ±89°；如果客户端不做此限制，会毫不犹豫地把相机翻转过去 |
| `previewControl` 中缺失的角度 | 保持当前角度，因此单纯的一帧确认不会让相机发生跳变 |

<a id="render-settings"></a>

## 渲染设置

| 方面 | 值 |
| :-- | :-- |
| 渲染目标 | 瞬态，`PF_B8G8R8A8`，不强制使用线性伽马，清空为 `(0.025, 0.025, 0.03, 1.0)` |
| 复用 | 渲染目标和缩略图场景跨帧持续存在；更改尺寸会重新创建渲染目标和 GPU 回读缓冲区 |
| 显示标志 | 游戏显示标志，**禁用运动模糊**并**禁用抗锯齿** |
| 屏幕百分比 | 固定为 1.0 |
| 分离式半透明 | 遵循缩略图场景针对该材质自身的规则 |
| Alpha | 在编码为 PNG 之前强制设为 255 — 预览始终不透明 |
| UI 域材质 | 无论请求的网格体如何，都强制映射到平面上 |
| 投影宽高比 | **始终为 1:1**，绝不从请求的尺寸推导 |

GPU 就绪状态在该帧的复制命令之后于渲染线程上轮询，因此复用回读缓冲区不会把上一帧已完成的栅栏误认为当前帧的完成信号。游戏线程只检查异步结果。在捕获期间断开连接会安全地释放挂起的结果，而无效的渲染目标纹理会报告错误，而不是让捕获一直挂起。

> [!WARNING]
> **请使用正方形 `width`/`height`。** 预览通过 `FThumbnailPreviewScene` 进行渲染，其
> 投影矩阵构建为 `FReversedZPerspectiveMatrix(halfFov, 1, 1, near)` — 宽高比
> 是硬编码的，*不会*取自视图矩形。因此，非正方形的渲染目标并不会显示更多场景；
> 它会把球体拉伸成椭圆。这没有任何诊断提示。请请求正方形帧，如果其填充的表面
> 不是正方形，则在客户端一侧进行加黑边处理。

场景的材质接口在提交一帧后有意**不**清除；清除它会与仍在执行中的渲染命令产生竞态。

源文件的编译方式为交互式编译：即一个 `ThinCustom` 产物 **Ephemeral**，因此
预览永远不会将其写入磁盘；一个 `Graph` 后端材质和一个 `.dsi` 实例则是普通资产，
会像任何编译保存它们一样被保存 *(自 2.0.0 起)*。未更改的源文件会被构建键跳过，
除非请求强制进行。结果有意地类型化为材质*接口*，因为
默认的 `ThinCustom` 后端产生的是一层薄薄的材质实例而非 `UMaterial`，而 `.dsi`
则会生成一个材质实例。

<a id="output"></a>

## 输出

| 路径 | 内容 |
| :-- | :-- |
| `<Project>/Saved/DreamShader/Bridge/Preview/` | 渲染后的 PNG |
| `<Project>/Saved/DreamShader/Bridge/preview.json` | 结果清单 |

PNG 文件名为 `<sanitized source stem>-<crc32 of the project-relative path>.png`，其中
CRC 打印为八位小写十六进制数字。如果源文件的主干部分清洗后为空，则生成
`DreamShaderPreview-<crc32>.png`。

`preview.json` 字段：

| 字段 | 备注 |
| :-- | :-- |
| `version` | `1` |
| `requestId` | 仅当请求中携带了该字段时才出现 |
| `status` | `ready` 或 `error` |
| `sourceFile` | 所请求的 `.dsm` |
| `assetPath` | 所生成材质的对象路径 |
| `imagePath` | 为此结果写入的 PNG |
| `mesh` | 回退之后实际使用的网格体 |
| `message` | 成功或失败的文本 |
| `updatedAtUtc` | ISO 8601 UTC |

<a id="streaming-preview"></a>

## 流式预览

| 方面 | 值 |
| :-- | :-- |
| 端口 | `17864` |
| 绑定地址 | 仅 `127.0.0.1` |
| 连接过滤 | 客户端 IP 必须为 `127.0.0.1` 或 `localhost`；其他一律拒绝 |
| 帧格式 | 4 字节长度、1 字节类型标签，然后是负载。标签 `1` = JSON，标签 `2` = PNG，标签 `3` = 原始 RGBA8 帧 |

每条消息都以原始 WebSocket *二进制*帧发出，因此操作码无法区分 JSON 与图像字节 — 这就是需要显式类型标签的原因。完整帧格式、标签 `3` 的原始帧头部以及帧标志位均记录于 [Bridge » Framing](bridge.md#framing)。

| 消息 | 方向 | 用途 |
| :-- | :-- | :-- |
| `previewMaterial` | 入 | 编译并渲染一个源文件。也接受为 `action: "previewMaterial"`，比较时不区分大小写。`encoding` 选择 `raw` 或 `png`；`force` 控制重新生成 |
| `previewControl` | 入 | 为活动会话更改流式状态、帧率、相机角度、视口尺寸或网格体 |
| `setProbe` / `clearProbe` | 入 | 附加或分离一个断点式探针（参见 [Breakpoints](#breakpoints-probe-preview)） |
| `previewResult` | 出 | `status` `ready` 或 `error`，以及上面列出的结果字段 |
| `previewFrame` | 出 | *（仅 png 编码）* 帧元数据，随后立即以带标签的二进制消息发送 PNG |
| `probeState` | 出 | 探针是 `attached`、`pending` 还是 `cleared` |

`previewControl` 读取 `requestId`、`stream`、`frameRate`、`orbitYaw`、`orbitPitch`、`width`、
`height`、`mesh` 和 `ackFrameIndex`；除 `requestId`/`ackFrameIndex` 之外的每个字段在省略时都保持其
当前值。与活动会话不匹配的 `requestId` 会被忽略。

<a id="frame-pacing"></a>

### 帧节奏

流式传输既受确认门控，也受速率限制。只有在**两个**条件同时满足时才会开始新的一帧：客户端已确认上一帧，且帧间隔已过去。
间隔为 `1 / frameRate` 秒，`frameRate` 被限制在 `[0.25, 60.0]`。

流式传输期间的任何错误都会关闭流式传输并发送错误 `previewResult`。

> [!NOTE]
> 60 FPS 的上限之所以能够达到，仅仅是因为预览运行在它自己的每帧计时器上。
> 如果共用桥接的 0.1 秒请求计时器，则无论 `frameRate` 要求多少，
> 每个会话都会被限制在 10 FPS。

<a id="diagnostics"></a>

## 诊断

运行时替换显示为 `{Placeholder}`。

<a id="resolving-the-material"></a>

### 解析材质

按此顺序检查；第一个失败即为所报告的消息。

| 消息 | 原因 |
| :-- | :-- |
| `DreamShader source '{File}' does not exist.` | 路径为空，或文件缺失 |
| `DreamShader preview renders a material or a material instance, so it takes a .dss, .dsi or .dsm source: '{File}'.` | 路径是 `.dsf`、`.dsh`、`.dsp` 或其他任何内容 *(自 2.0.0 起)* |
| `<file>(<line>,<col>): DSHnnnn: <message>` | 解析该源文件所构建的内容失败；按编译所报告的第一个错误 |
| `{File}: the source could not be resolved to the assets it builds.` | 同上，但没有可显示的错误 |
| `{File}: this file builds no material or material instance.` | 例如仅包含导出函数的 `.dss` |
| `The DreamShader compiler module is not available, so '{File}' could not be compiled for preview.` | `DreamShaderCompiler` 模块未加载 |
| *（编译结果消息）* | 编译失败 — 其诊断，参见 [Commandlet » Result messages](commandlet.md#result-messages) |
| `Generated material '{ObjectPath}' could not be loaded.` | 编译成功但对象未能加载 |
| `Compiled preview material for {ObjectPath}.` | 成功；任何编译消息都会附加其后 |

源文件由编译自身的前半部分解析 — 预处理器、前端、绑定器、`import`
头文件单独解析 — 因此预览与编译对于某个文件会构建哪个资产不可能产生分歧。1.x 预览会剥离 `import` 行并运行 1.x 解析器 *(2.0.0 之前)*。

<a id="rendering"></a>

### 渲染

| 消息 | 原因 |
| :-- | :-- |
| `Failed to create preview render target.` | 无法分配渲染目标 |
| `Failed to access preview render target resource.` | 目标没有 RHI 资源 |
| `Failed to create preview view for '{ObjectPath}'.` | 无法构建场景视图 |
| `Preview material is not valid.` | 在没有材质的情况下进入了同步路径 |
| `Failed to read preview pixels for '{ObjectPath}'.` | 阻塞式回读失败 |
| `Failed to encode preview thumbnail for '{ObjectPath}'.` | PNG 编码失败（同步路径） |
| `A preview readback is already in flight.` | 在第一个异步帧完成之前启动了第二个 |
| `Failed to lock preview readback buffer.` | 无法映射回读缓冲区 |
| `Preview readback failed.` | 回读 promise 未被兑现 |
| `Failed to encode preview thumbnail.` | PNG 编码失败（异步路径） |
| `Failed to write preview image '{Path}'.` | 无法将 PNG 写入磁盘 |
| `Rendered preview for {ObjectPath}.` | 成功 |

<a id="server"></a>

### Server

| 消息 | 严重级别 | 原因 |
| :-- | :-- | :-- |
| `DreamShader preview WebSocket server could not load WebSocketNetworking.` | 警告 | 模块不可用 |
| `DreamShader preview WebSocket server could not be created.` | 警告 | 服务器创建失败 |
| `DreamShader preview WebSocket server failed to listen on 127.0.0.1:{Port}.` | 警告 | 端口被占用或被阻止 |
| `DreamShader preview WebSocket server listening on 127.0.0.1:{Port}.` | 显示 | 已启动 |
| `DreamShader preview WebSocket client connected: {Client}` | 显示 | 有客户端接入 |
| `DreamShader preview WebSocket: {Message}` | 显示 / 错误 | 每个请求的结果 |
| `DreamShader preview: {Message}` | 显示 / 错误 | 每个请求的结果，请求文件路径 |

无法解析的入站消息会得到 `previewResult` 响应，其状态为 `error`，
消息为 `Invalid DreamShader preview request JSON.`

<a id="limits"></a>

## 限制

- **仅支持材质来源** — `.dss`、`.dsi`、`.dsm`。`.dsf`、`.dsh` 或 `.dsp` 完全无法预览；
  也没有函数预览。
- 尺寸在两个维度上都被限制为 `[64, 2048]`。
- 运动模糊、抗锯齿和动态屏幕百分比均已关闭，因此预览不会与视口捕获
  逐像素匹配。
- 插件侧没有俯仰角限制。
- Alpha 通道会被丢弃 — 半透明材质会针对固定的清除颜色进行预览。
- 一次性路径会阻塞，直到所有着色器都编译完成；在冷着色器缓存上这可能会耗费
  很长时间。
- 只能有一个异步回读处于未完成状态；启动第二个会返回
  `A preview readback is already in flight.`
- 请求文件传输会完全忽略 `orbitYaw` 和 `orbitPitch`。

<a id="example"></a>

## 示例

由 bridge 请求文件驱动的一次性渲染。将其放入
`<Project>/Saved/DreamShader/Bridge/Requests/`，可使用任意 `.json` 名称；轮询器会处理它，
然后将其删除。

```json
{
  "action": "previewMaterial",
  "sourceFile": "I:/Project/DShader/Materials/M_Emissive.dsm",
  "mesh": "shaderball",
  "width": 512,
  "height": 512,
  "requestId": "preview-1"
}
```

生成的 `<Project>/Saved/DreamShader/Bridge/preview.json`：

```json
{
  "version": 1,
  "requestId": "preview-1",
  "status": "ready",
  "sourceFile": "I:/Project/DShader/Materials/M_Emissive.dsm",
  "assetPath": "/Game/Materials/M_Emissive.M_Emissive",
  "imagePath": "I:/Project/Saved/DreamShader/Bridge/Preview/M_Emissive-1a2b3c4d.png",
  "mesh": "shaderball",
  "message": "Rendered preview for /Game/Materials/M_Emissive.M_Emissive.",
  "updatedAtUtc": "2026-08-02T09:14:07Z"
}
```

<a id="see-also"></a>

## 另请参阅

- [Bridge](bridge.md) — 请求文件协议和完整的 WebSocket 消息架构
- [Material Content Browser](material-browser.md) — 检查器的实时预览，由此渲染器绘制
- [编辑器集成](editor-integration.md) — `-NoDreamShaderEditorBridge`，它会禁用服务器
- [临时材质](../generation/ephemeral.md) — 为什么预览编译不会写入 ThinCustom 材质
- [后端](../settings/backend.md) — 为什么预览的对象可能是实例而不是 `UMaterial`
- [源文件](../language/source-files.md) — 哪些源会构建材质
- [工作区](workspace.md) — 驱动流式客户端的编辑器扩展
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每条消息
