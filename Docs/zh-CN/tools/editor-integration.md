---
title: 编辑器工具
description: DreamShader 注册的每一个菜单项、工具栏按钮和右键菜单操作，Material Content Browser 面板，以及预览、VirtualFunction 工具、资产改名同步和 bridge 产物。
---

DreamShader 加进 Unreal 编辑器的所有东西都在一个 editor-only 模块里。本页是那张地图：什么出现在哪、每条命令实际做了什么，
以及 Material Content Browser 面板怎么用。

| 项目 | 值 |
| :-- | :-- |
| 实现于 | `DreamShaderEditor` 模块，type `Editor` |
| 注册方式 | 模块启动时的 `UToolMenus::RegisterStartupCallback` |
| ToolMenu owner | `DreamShaderEditor`（bridge 项）· `DreamShaderMaterialBrowser`（browser 项）· `DreamShaderSourceNavigation`（节点右键菜单） |
| 被禁用于 | `-NoDreamShaderEditorBridge` 开关，以及任何 commandlet 运行 |
| 日志分类 | `LogDreamShader` |

打包出来的游戏里这些一个都没有 —— 模块是 editor-only 的，运行时 `DreamShader` 模块完全不带 UI。菜单注册是幂等的，
第二次注册什么都不加；编辑器正在关闭时则整个跳过。

<a id="启动了什么按什么顺序"></a>

## 启动了什么，按什么顺序

| # | 步骤 | 何时跳过 |
| :-- | :-- | :-- |
| 1 | 运行 commandlet 时：当 `-run=` 含 `Cook` 且没有 `-cookworker` 就装上 cook 钩子，然后停止 | — |
| 2 | 直接整体退出 | 命令行上有 `-NoDreamShaderEditorBridge` |
| 3 | 创建并启动 editor bridge | 同上 |
| 4 | 注册 Material Content Browser nomad tab 及其菜单项 | 同上 |

bridge 自己的启动流程随后会重置 `bridge.db`、导出 [manifest](#bridge-产物)、执行
[VirtualFunction 同步](#启动同步服务)、在 post-engine-init 把每个源文件生成一遍（ThinCustom 产物是 Ephemeral 的）并导出内建节点目录、
排入一次全量扫描、在端口 `17864` 上打开预览 WebSocket 服务、注册源目录 watcher，并安装菜单。

<a id="tools-菜单"></a>

## Tools 菜单

*Tools ▸ DreamShader*，扩展 `LevelEditor.MainMenu.Tools`，section `DreamShader`。

| 标签 | Tooltip | 图标 | 效果 |
| :-- | :-- | :-- | :-- |
| **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first." | `Icons.Refresh` | 先弹是/否确认，然后重建依赖图，把每个源码根的每个源文件 —— 尽管标签这么写，其实是 `.dsm`、`.dsf`、`.dss`、`.dsi` 和 `.dsp` —— **强制**越过构建键排入队列 —— [见下](#recompile-dsm) |
| **Clean Generated Shaders** | "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile." | `Icons.Delete` | 删除生成目录下的每个 `*.ush`，然后排入一次**强制**的全量扫描 —— [见下](#clean-generated-shaders) |
| **Make Ephemeral** （自 2.0.0 起） | "Delete the packages of Materialized ThinCustom products so they go back to being Ephemeral. …" | `Icons.Delete` | 删除带 DreamShader 来源元数据的 ThinCustom 产物在磁盘上的 package —— [见下](#make-ephemeral)。取代 1.x 的 *Clean Persisted Generated Assets* |
| **Show Ephemeral Materials** （自 2.0.0 起） | "Show Ephemeral ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — …" | *（无）* | 作用于 `bShowEphemeralMaterials` 的开关按钮 —— [见下](#show-ephemeral-materials)。1.x 叫 *Show In-Memory Materials* |
| **Open Dream Shader Workspace (VSCode)** （自 1.2.1 起） | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | 重新导出 manifest、重写 workspace 文件、启动它 —— [VSCode 与 Rider](vscode.md) |
| **Material Content Browser** （自 1.5.0 起） | "Open the DreamShader Material Content Browser." | `ClassIcon.Material` | 唤起 `DreamShaderMaterialBrowser` nomad tab —— [见下](#material-content-browser) |

**提示：**

前五项和第六项由两个不同的启动回调注册进同一个 `DreamShader` section。它们在 section 内部的相对顺序取决于注册顺序，
不保证在两次编辑器运行之间保持一致。

<a id="关卡编辑器工具栏"></a>

## 关卡编辑器工具栏

一个 **Dream** 组合按钮，由 Dream 系列插件（DreamShader、DreamFX、DreamGUI）共用：每个插件在它还不存在时加上它，所以不管哪个先加载，
它都只出现一次。它扩展 `LevelEditor.LevelEditorToolBar.AssetsToolBar` 的 section `DreamTools`，打开共享菜单 `DreamTools.Actions`，
每个插件在里面加自己的一节。DreamShader 那一节：

| 标签 | 效果 |
| :-- | :-- |
| **DreamShader Workspace** | 等同 *Open Dream Shader Workspace (VSCode)* |
| **Recompile DSM** | 等同 *Recompile DSM*，包括确认框 |

<a id="window-菜单"></a>

## Window 菜单

Material Content Browser 注册的是一个 nomad tab，因此它同时出现在 *Window ▸ Tools* 下。

| 项目 | 值 |
| :-- | :-- |
| Tab id | `DreamShaderMaterialBrowser` |
| 显示名 | **Material Content Browser** |
| Tooltip | "Browse, manage, and create instances of project and DreamShader-generated materials." |
| 图标 | `ClassIcon.Material` |
| Workspace group | Tools 分类 |
| Tab role | `ETabRole::NomadTab` |

<a id="content-browser-右键菜单"></a>

## Content Browser 右键菜单

所有条目都加进按类划分的资产右键菜单里那个标准的 `GetAssetActions` section。

**警告：**

**每一个 DreamShader 右键菜单项都要求恰好选中一个资产。** 选中两个或更多时该 section 是空的，而且没有任何提示说明原因。
请右键单个资产。

| 被扩展的资产类 | 出现的内容 |
| :-- | :-- |
| `UMaterial` | 子菜单 **DreamShader** —— [Material 子菜单](#material-子菜单) |
| `UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend` | 子菜单 **DreamShader** —— [Material Function 子菜单](#material-function-子菜单) |
| `UDreamShaderMaterialInstance`、`UMaterialInstanceConstant` | 子菜单 **DreamShader** —— [材质实例子菜单](#材质实例子菜单) |
| `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | 平铺条目 **Create DreamShader instance** |
| `UMaterial` · `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | 平铺条目 **Show in Material Content Browser** |

Unreal 里的菜单名是按精确类名索引的，所以同时给两个实例类的条目注册了两遍 —— 一遍给标准类，一遍给 DreamShader 子类。

| 条目 | 效果 |
| :-- | :-- |
| **Create DreamShader instance** （自 1.5.0 起） | 打开[创建材质实例](#创建材质实例)对话框。tooltip："Create a material instance of this material: a .dsi source file compiled into the instance when DreamShader generated the material, an ordinary instance asset otherwise." |
| **Show in Material Content Browser** | 打开（或前置）[Material Content Browser](#material-content-browser)，进入 Assets 模式、限定在资产的挂载点、选中该资产 |

`UDreamPassPipeline` （自 2.1.0 起） 在这里没有 DreamShader 条目：它的 *Open Source*、*Revert to Source*、*Adopt Into Source*
在它的[详情面板](#pass-pipeline-详情面板)上，它的源文件在 Material Content Browser 里，反编译用 `dsc decompile`。

<a id="material-子菜单"></a>

### Material 子菜单

标签 **DreamShader**，tooltip "DreamShader actions for this Material."，图标 `Icons.Settings`。只有当唯一选中的资产是 `UMaterial` 时才会构建。

第一节是 [Generated Asset](#generated-asset-一节)；DreamShader 没生成过的资产这一节是空的。第二节 **Decompiler**：

| 条目 | 效果 |
| :-- | :-- |
| **Export .dss** （自 2.0.0 起） | 用 2.0 反编译器反编译到 `<SourceDirectory>/Decompiled/Materials/…​.dss` 并打开文件 —— [反编译导出](decompiler.md#20-反编译器) |
| **Export Legacy .dsm** （自 2.0.0 起）（1.3.5 – 1.9.x 叫 **Export DSM**） | 用保留的 1.x 导出器反编译到 `<SourceDirectory>/Decompiled/Materials/…​.dsm` 并打开文件 —— [反编译导出](decompiler.md) |

<a id="material-function-子菜单"></a>

### Material Function 子菜单

标签 **DreamShader**，tooltip "DreamShader actions for this Material Function."，图标 `Icons.Settings`。layer 和 layer blend 用同一个子菜单。

第一节是 [Generated Asset](#generated-asset-一节)。第二节 **Decompiler**：**Export .dss** （自 2.0.0 起） 和
**Export Legacy .dsf**（以前叫 **Export DSF**），写到 `<SourceDirectory>/Decompiled/{Functions,Layers,LayerBlends}/…`。

第三节 **VirtualFunction** （自 1.2.1 起） —— 内容取决于是否已经有一个声明指向这个资产。见 [VirtualFunction 工具](#virtualfunction-工具)。

<a id="材质实例子菜单"></a>

### 材质实例子菜单

`UDreamShaderMaterialInstance` —— ThinCustom `Shader` 的产物 —— 的 **DreamShader** 子菜单只有 [Generated Asset](#generated-asset-一节) 一节。
其他任何 `UMaterialInstanceConstant` —— [`.dsi`](../language-v2/instances.md) 构建的，或者手做的 —— 有 Generated Asset 一节（手做的为空），
外加 **Decompiler** 一节里的 **Export .dsi** （自 2.0.0 起）：把实例 —— 它的父材质、实例设置、与父材质不同的每个参数 ——
反编译到 `<SourceDirectory>/Decompiled/Instances/…​.dsi` 并打开文件。

<a id="generated-asset-一节"></a>

### Generated Asset 一节

上面每个子菜单的第一节。除非资产带着 DreamShader 的来源元数据，否则这一节是空的，所以手写资产一项都看不到。

| 条目 | 效果 |
| :-- | :-- |
| **Revert to Source** （自 1.8.0 起） | 按源码重建，丢掉每一处手改；源文件不动。资产是 `Diverged` 时标签写作 **Revert to Source (discards your edits)** |
| **Adopt Into Source** （自 1.8.0 起） | 用资产当前的内容重写源码；旧源码先备份在旁边 |
| **Detach From DreamShader** （自 1.8.0 起） | 资产原样保留，DreamShader 不再重建它 |
| **Adopt Tweaks as Source Defaults** （自 2.0.0 起） | 只在 `Tweaked` 的 ThinCustom 实例上出现。把它的参数覆盖写进 `.dss` 里对应 `uniform` 的默认值，再清掉实例上的覆盖；源码先备份。`bool` / `int` / `uint` 装不下的标量值是 `DSH9109`。源文件不是可写源码根下的 `.dss` 时禁用，原因写在 tooltip 里 |
| **Extract Tweaks to .dsi** （自 2.0.0 起） | 只在 `Tweaked` 的 ThinCustom 实例上出现。把它的参数覆盖写进一个以这个材质为父的新 `.dsi`、编译它，再清掉实例上的覆盖 |

前三个是[手改检测](../generation/regeneration.md#三个出口)的三个出口。它们是[「资产开着编辑器就拒绝重建」](../generation/regeneration.md#资产开着编辑器时拒绝重建)的例外：
自己关掉资产编辑器、动手、再打开。对 **Adopt** 来说，关闭过程中弹出的引擎保存提示是承重的 —— 取消它就取消整个动作。

<a id="材质编辑器工具栏"></a>

## 材质编辑器工具栏

`AssetEditor.MaterialEditor.ToolBar` 的 `DreamShader` section 中的一个动态条目 （自 1.2.1 起）。

| 步骤 | 行为 |
| :-- | :-- |
| 1 | 要求存在有效的 `UMaterialEditorMenuContext` 和活的 `IMaterialEditor` |
| 2 | 扫描当前正在编辑的对象，停在**第一个** `UMaterial`，找不到则停在第一个 `UMaterialFunction` |
| 3 | `UMaterial`：一个标签为 **DreamShader** 的组合按钮，内容是 [Material 子菜单](#material-子菜单) |
| 4 | `UMaterialFunction`：一个标签为 **DreamShader** 的组合按钮，内容是 [Material Function 子菜单](#material-function-子菜单) |
| 5 | 两者都没找到 ⇒ 工具栏上什么都不加 |

<a id="材质编辑器节点右键菜单"></a>

## 材质编辑器节点右键菜单

（自 2.0.0 起） 在材质编辑器的图里右键一个 DreamShader 生成的节点，节点自己的右键菜单里多一个 **DreamShader** 子菜单。

| 条目 | 效果 |
| :-- | :-- |
| **Open Source Line** | 在节点的行列位置打开源文件，走和 *Open Dream Shader Workspace* 一样的三级启动链 —— VSCode、系统默认编辑器、Notepad |
| **Open Call Site** | 只在节点来自一个**被内联的 helper** 时出现：打开**调用**这个 helper 的那一行，通常在另一个函数、往往在另一个文件里 |

没有记录源码位置的节点**根本没有这个菜单**（不是灰掉的菜单）—— 工程里每个手写材质的每个节点都是这样。2.0.0 之前由 1.x 生成器构建、
不带 span 表的资产也一样；现在每种源文件都由 2.0 流程构建，强制重建一次（`compile -Force`）就有了。失败时弹一条 4 秒的失败 toast，
并记一条带 `DSH9058` 的错误。

反方向是 bridge 的 `reveal-node` 请求：从一行源码到材质编辑器里的节点 —— VSCode 扩展的 *Reveal in Material Editor* 发的就是它。
`DSH9051` 表示这个源文件的资产没有在本编辑器里加载过（先编译它），`DSH9052` 表示资产是在节点导航出现之前构建的、没有 span 表（`-Force` 重建）。

<a id="pass-pipeline-详情面板"></a>

## Pass pipeline 详情面板

（自 2.1.0 起） `UDreamPassPipeline` —— [`.dsp`](../language-v2/passes.md) 编译成的资产 —— 的详情面板，在管线自己的属性上方多三个分类，
按一帧运行它的方式来读这个资产：

| 分类 | 显示 |
| :-- | :-- |
| **Pipeline Overview** | **Source**：构建它的 `.dsp` 和它的来源状态，带 **Open Source**、**Revert to Source**、**Adopt Into Source** 三个按钮（源文件随插件发布或找不到时 Adopt 禁用）；**Runs**：顺序、视图、需求、pass / buffer / 参数个数；**Problems**：只在管线校验失败时出现。UE 5.8 以下多一行说明这个引擎只加载和保存管线、不运行它 |
| **Passes in Frame Order** | 每个注入点一组，按一帧到达的顺序；每个 pass 显示设置、绑定、HLSL slot，以及打开它的 `.usf` 或 `.dsp` 对应行的链接 |
| **Buffers** | 每个 buffer 一行；导出的 buffer 带一个指向它的 render target 的链接 |

属性仍然在、仍然可编辑：面板就是调管线的地方，改动下一帧就生效，并让管线与它的 `.dsp` [分歧](../generation/regeneration.md#手改检测) ——
在 revert 或 adopt 之前，重建改过的 `.dsp` 会拒绝覆盖它。面板在每个编辑器进程里都在，带不带 `-NoDreamShaderEditorBridge` 都一样。

<a id="通知"></a>

## 通知

本页上唯一一个由 DreamShader 主动弹出、而不是注册好等人点击的东西。没有 Slate 应用或没人看的地方 —— commandlet、cook、专用服务器、
任何带 `-unattended` 的运行 —— 不弹通知。

| 通知 | 何时弹出 | 按钮 |
| :-- | :-- | :-- |
| **手改拒绝** （自 1.9.0 起） | 一次重建因资产被手改而被拒绝（`DSH8115`） | **Revert to Source** · **Adopt Into Source** · **Detach** · **Show Ephemeral Materials**（仅隐藏的 Ephemeral 实例）· **Dismiss** |
| **手改汇总** （自 1.9.0 起） | 一轮重建里超过五个资产分歧 | **Open Material Browser** · **Dismiss** |
| **Ephemeral 材质被遮蔽** | 编译 backend 改了，而已落盘的资产会遮蔽 Ephemeral 结果 | *（无）* |
| **Ephemeral 可见性切换** | *Show Ephemeral Materials* 被翻转 | *（无）* |
| **导出结果** | 右键菜单里的一次 *Export* 完成或失败 | *（无）* |
| **来历动作结果** | Revert、Adopt 或 Detach 完成或失败 | *（无）* |

两种手改通知**不会自动消失**，因为它们在提问；其他都是 4 秒的 toast，报告已经做完的事。规则和频率见
[手改检测](../generation/regeneration.md#通知)。

<a id="各命令的行为"></a>

## 各命令的行为

引用消息里的运行期替换写作 `{Placeholder}`。

<a id="recompile-dsm"></a>

### Recompile DSM

先问 —— 一个是/否对话框："Recompile every DreamShader .dsm and .dsf source file? This rebuilds all generated materials and can take a
while on a large tree." 选是之后，重建材质依赖图，然后把每个源码根的每个 `.dsm`、`.dsf`、`.dss`、`.dsi` 和 `.dsp` 以当前时间戳、
**强制**地打进待处理队列。源码根 `Packages` 文件夹下的 `.dsm` 被排除；`.dsh` 头文件从不排队，因为它们什么都不构建。
排队的文件随后由防抖 ticker 编译，和你手动保存它们完全一样。

日志：`DreamShader queued a full .dsm/.dsf recompile scan.`

<a id="clean-generated-shaders"></a>

### Clean Generated Shaders

删除生成的 `.ush` include，并排入一次强制的全量扫描。2.0 起已经没有编译往那里写 —— 那里的文件都是 1.x 留下的，见
[Function 的 HLSL](../generation/index.md#function-的-hlsl)。

| 行为 | 细节 |
| :-- | :-- |
| 安全护栏 | 当 *Generated Shader Directory* 不在工程 `Intermediate/` 目录内时拒绝执行 |
| 删除范围 | 只删 `*.ush`，递归，逐个删除。目录本身永远不删 |
| 标志 | 容忍文件缺失；只读文件照删 |

| 消息 | 严重度 |
| :-- | :-- |
| `DreamShader refused to clean generated shaders: '{Directory}' is not inside the project Intermediate directory. Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning.` | Warning |
| `DreamShader deleted {Count} generated shader file(s) from '{Directory}'.` | Display |
| `DreamShader cleaned generated shader includes and queued a full .dsm/.dsf recompile scan.` | Display |

<a id="make-ephemeral"></a>

### Make Ephemeral

（自 2.0.0 起）

找出并删除 Materialized 的 ThinCustom 产物 —— 磁盘上有 package 的那些 —— 因为已保存的产物会遮蔽同一源文件生成的 Ephemeral 产物。

| 项目 | 值 |
| :-- | :-- |
| 搜索范围 | 资产注册表，package path `/Game`，递归路径，递归类 |
| 类 | 只有 `UDreamShaderMaterialInstance`。`Graph` backend 的 `UMaterial` 和 `UMaterialFunction` 没有 Ephemeral 状态可回，删掉它们只是删掉产物 |
| 条件 1 | package 必须存在于磁盘 |
| 条件 2 | package 必须带非空的 `DreamShader.SourceFile` 元数据 |
| 删除方式 | 标准编辑器删除流程，带确认对话框和引用检查 |
| 删除之后 | 每个源文件立刻重新生成为 Ephemeral，因此引用无需重启编辑器即可解析 |

因为过滤依据是来源元数据，手写材质永远不会被碰 —— 而源文件已被删除或改名的孤儿资产仍然会被命中。

| Toast | 原因 |
| :-- | :-- |
| `No Materialized DreamShader ThinCustom products found.` | 没有匹配项 |
| `Made {Deleted} of {Total} Materialized product(s) Ephemeral.` | 删除流程结束后 |

<a id="show-ephemeral-materials"></a>

### Show Ephemeral Materials

翻转 `bShowEphemeralMaterials`（2.0 之前是 `bShowInMemoryMaterialsInContentBrowser`，旧键加载时迁移）并直接写进工程的
`DefaultEngine.ini`。随后它会遍历每一个 package 仍是 newly-created 的活 `UDreamShaderMaterialInstance` 并广播资产创建或移除，
所以图块立刻出现或消失，而不是等到下一次重新枚举。

| Toast | 条件 |
| :-- | :-- |
| `Showing {Count} Ephemeral material(s) in the Content Browser and asset pickers.` | 打开 |
| `Hidden {Count} Ephemeral material(s) from the Content Browser and asset pickers.` | 关闭 |

**警告：**

Ephemeral 材质被显示出来时，它们也会出现在**保存**选择器里，而一次显式 *Save* 就会把它 Materialize。这份保存副本随后会
遮蔽 Ephemeral 产物。用 *Make Ephemeral* 恢复；真的想要文件时请用 [*Materialize*](#materialize)。

Material Content Browser 的 *View ▾ ▸ Show Ephemeral materials* 跑的是同一个命令、同一个**全局**设置，toast 也一样。
菜单项的勾选状态实时读自设置对象，所以在 Project Settings 里改值会更新那个勾。

<a id="open-dream-shader-workspace-vscode"></a>

### Open Dream Shader Workspace (VSCode)

重新导出 `material-expressions.json`、`settings.json`、`pass-keys.json`、`substrate-builtins.json` 和内建节点目录，重写
`DShader/DreamShader.code-workspace`，然后走三级回退链启动它 —— VSCode、系统默认编辑器、Notepad。
探测顺序、文件的确切内容，以及"这个文件会被整体重写"的警告，都在 [VSCode 与 Rider](vscode.md)。

<a id="material-content-browser"></a>

## Material Content Browser

（自 1.9.0 起） 一个可停靠面板，覆盖工程里的每个 DreamShader 源文件和每个材质：每个源文件编译成什么、那个资产是不是最新、
有没有人手改过它、它 import 了什么、谁继承了它 —— 以及从这些答案出发的操作。（1.5.0 – 1.8.x 的 *Project* / *Dream Shader Gen*
两页面板被它取代。）

| 项目 | 值 |
| :-- | :-- |
| 类型 | nomad tab |
| 模式 | **Sources**（源码树：`.dss` / `.dsi` / `.dsp` 与 1.x 的 `.dsm` / `.dsf` / `.dsh`）· **Assets**（工程的材质） |
| 起始版本 | （自 1.5.0 起）；现在的布局 （自 1.9.0 起） |

<a id="怎么打开"></a>

### 怎么打开

| 入口 | 路径 |
| :-- | :-- |
| Tools 菜单 | *Tools ▸ DreamShader ▸ Material Content Browser* |
| Window 菜单 | *Window ▸ Tools ▸ Material Content Browser* |
| Content Browser | 右键材质或实例 ▸ **Show in Material Content Browser** —— 打开在那个资产上 |

这个 tab 只在 editor bridge 启动时才注册，所以 [`-NoDreamShaderEditorBridge`](#关闭整套集成) 会去掉所有入口。

<a id="布局"></a>

### 布局

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ [⟳ Refresh] [▶ Compile ▾] [+ New ▾] │ search…          │ [View ▾] [VSCode] [⚙] │
├──────────────┬──────────────────────────────────┬──────────────────────────┤
│ NAVIGATION   │ LIST                              │ INSPECTOR                │
│ ▾ Sources    │ Sources mode: columns or tiles    │ live preview · mesh      │
│   ▾ Project  │ Assets mode: the asset picker     │ name · status · storage  │
│ ▾ Content    │                                   │ actions · Diagnostics    │
│ Quick filters│                                   │ Provenance · Imports …   │
├──────────────┴──────────────────────────────────┴──────────────────────────┤
│ 14 sources · 9 ok · 3 stale · 1 errors · 1 edited by hand · 5 Ephemeral │ bridge: idle │
└────────────────────────────────────────────────────────────────────────────┘
```

三栏水平分割，上面工具栏，下面状态栏。栏宽比例、模式、导航范围、过滤、排序、列表 / 图块、预览网格和上次的选择都按用户、按工程保存，
下次打开时恢复。

| 工具栏控件 | 效果 |
| :-- | :-- |
| **Refresh**（`F5`） | 重新扫描源码根、重建依赖图、重算每个状态 |
| **Compile ▾** | **Compile**（`Ctrl+B`）选中项 · **Compile stale** —— 每个 stale、没编过或失败的源文件 · **Compile all**（`Ctrl+Shift+B`）—— 除 `.dsh` 外每个列出的源文件，带进度对话框 |
| **New ▾** | 从插件模板新建源文件 —— 见[新建源文件](#新建源文件) |
| 搜索框（`Ctrl+F`） | 大小写不敏感的子串匹配，匹配文件名、根名、源码路径、资产路径和状态细节 —— 所以错误消息也能搜；Assets 模式下匹配资产名 |
| **View ▾** | **Tiles** 图块视图 · **Sort by** Name / Status / Root / Asset path · **Show Ephemeral materials**（全局设置） |
| VSCode 图标 | 写出并打开 workspace，等同 *Open Dream Shader Workspace* |
| 齿轮 | 打开 *Project Settings ▸ DreamPlugin ▸ DreamShader* |

从这个 tab 发起的每次编译都经过 editor bridge，所以结果和保存即编译一样进诊断存储、`diagnostics.json` 和 VSCode 扩展，成功会清掉文件之前的错误。

<a id="导航与快速过滤"></a>

### 导航与快速过滤

左边是两组树。选中一个节点会**限定列表范围并切换模式**：**Sources**（全部源文件）、**Sources ▸ 根**（*Project* 或某个带 `DShader`
文件夹的插件）、**Sources ▸ 根 ▸ 文件夹**、**Sources ▸ Not managed by DreamShader**（没有任何源文件生成的材质 —— 手做的，或源文件已经没了的孤儿）；
**Content ▸ /Game** 和每个带 DreamShader 源文件的插件的内容根（Assets 模式）。

树下面的复选框，四个状态过滤是**或**的关系：**Errors**、**Stale**、**Edited by hand**（[已分歧](../generation/regeneration.md#手改检测)）、
**Ephemeral**；另有 **Hide functions**（去掉 `.dsf` 和 `.dsh`）、**Hide pipelines** （自 2.1.0 起）（去掉 `.dsp`）、**Hide unmanaged**。
状态栏的计数就是这些过滤：点一下单独打开它，再点一下关掉，点 *ok* 清掉全部四个。

<a id="sources-模式"></a>

### Sources 模式

每个通过范围、过滤和搜索的源文件，可排序的列表或 *View ▸ Tiles* 图块网格。多选；右键出[右键菜单](#右键菜单)；双击有材质时打开材质，否则打开源文件。

| 列 | 内容 |
| :-- | :-- |
| *（字形）* | 状态字形，tooltip 是状态细节 |
| **Name** | 文件名，tooltip 是绝对路径；`.dsp` 行前面有管线徽标 |
| **Root** | *Project* 或所属插件 |
| **Status** | 状态标签；`.dsf` / `.dsh` 是 `function · used by N material(s)`；`.dsp` 是 `pass pipeline · <状态>` |
| **Asset** | 源文件编译到的 package 路径 |

列出的是每个源码根下递归找到的 **`.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf` 和 `.dsh`**，排除每个根的 `Packages` 文件夹。
刷新后选择按路径保留。

**管线** （自 2.1.0 起）：`.dsp` 和材质源文件一样列出、按同样的规则判状态，但它的资产是 `UDreamPassPipeline` —— 这个 tab 里唯一不是材质的产物。
**Open** 在它的[详情面板](#pass-pipeline-详情面板)里打开管线；不提供 *Create instance*；没有预览。

**未受管的材质**：内容根下没有任何扫描到的源文件解析到的每个 `UMaterial` 和 `UMaterialInstanceConstant`，所以这个 tab 覆盖整个工程。
它们经资产注册表找到、**列出时从不加载**；选中时才加载，检查器读到真实的来源状态。对其中一个执行 *Export .dss*（普通材质实例是 `.dsi`），
就是把它纳入 DreamShader 管理的方式。

| 状态 | 字形 | 标签 | 含义 |
| :-- | :-- | :-- | :-- |
| `UpToDate` | `●` 绿 | **up to date** | 生成资产存在，且它记录的构建键与当前源一致 |
| `EphemeralUntracked` | `◐` 绿 | **compiled, Ephemeral** | 资产在内存里、没有 source hash。只有老版本插件（内存构建只打路径）生成的资产会这样；现在两种状态都打哈希，这是遗留状态 |
| `Stale` | `●` 琥珀 | **stale** | 资产存在，但记录的构建键与当前源不同 |
| `NotCompiled` | `○` 灰 | **not compiled** | 解析出的 object path 上没有对象。细节：`No generated asset at {ObjectPath}` |
| `Error` | `▲` 红 | **compile error** | 一次编译失败了 —— 从这个 tab 发起的，或 bridge 的诊断里有错误 |
| `Library` | `◆` 蓝 | **function / header** | `.dsf` 或 `.dsh` |
| `Unresolved` | `▲` 红 | **unresolved** | 算不出源文件构建什么：它有错误（细节是第一个，`<file>(<line>,<col>): DSHnnnn: <message>`），或者它什么材质、实例或导出函数都没声明 |

（自 2.0.0 起） 除 `.dsf` / `.dsh` 之外，每个源文件的状态这样算：

| # | 步骤 | 失败结果 |
| :-- | :-- | :-- |
| 1 | 用编译自己的前半段 —— define、按扩展名选的前端、绑定器、IR 构建器、目标规则 —— 解析出源文件构建的产物，取它的材质（没有则实例、再没有则管线、再没有则第一个产物）的 object path 和构建键 | ⇒ `unresolved` |
| 2 | **不加载**地查找该对象 | ⇒ `not compiled` |
| 3 | 比较资产记录的源文件和构建键与解析出的 | 一致 ⇒ `up to date` |
| 4 | 没有记录的键、资产也不在磁盘上 | ⇒ `compiled, Ephemeral` |
| 5 | 其他 | ⇒ `stale` |

构建键覆盖一次编译所打的一切 —— 预处理后的文件和它 include 的每个头文件、它们读过的 define、`.dsi` 的父材质 —— 所以最新的资产读作
**up to date**，一次重建总能让 stale 的变成最新。1.x 是在剥掉 import 的文本上解析 `Name=` / `Root=`、再对 import 内联后的源码求哈希；
那套做法回答不了 `.dss` 的任何问题，已经没有了。

然后叠加 bridge 的诊断，实时读自它的存储：文件有任何 `error` 记录就变成 `compile error`，细节是第一条错误（`L{line}:{col} {message}`），
全部记录留给检查器。

**提示：**

第 2 步不加载，所以磁盘上有、但本会话还没被加载过的材质会一直显示 **not compiled**，直到有东西加载它为止。

<a id="assets-模式"></a>

### Assets 模式

引擎自己的资产选择器，覆盖 `UMaterial` 和 `UMaterialInstanceConstant`（递归，所以包括 `UDreamShaderMaterialInstance`），限定在导航树的内容路径。
刻意保留引擎控件：缩略图、拖进视口、列视图和选择器自己的设置都随它而来。共用的过滤在这里同样生效，而且**不加载任何东西**就能回答。
双击打开资产，右键是同一个[右键菜单](#右键菜单)。

<a id="检查器"></a>

### 检查器

两种模式共用一个面板。它显示选中项存在的那两半 —— **源码**那一半（扫描到的文件）和**资产**那一半（工程里的材质）—— 通过资产的
`DreamShader.SourceFile` 标记连起来。

| 区块 | 内容 |
| :-- | :-- |
| **Preview** | 经插件的[预览渲染器](#预览)真正渲染的材质，224×224，每次编译后重渲。**左键拖动旋转**；**Mesh** 选球、平面、立方体、圆柱或 shaderball，并被记住。异步渲染，从不卡编辑器。只在有编译好的材质时显示 |
| **Header** | 名字 · 状态字形和标签 · `memory-only (not saved)` / `on disk` · 资产开在资产编辑器里时有一个徽标（这时重建会被拒绝） |
| **Actions** | **Create instance** · **Compile** · **Open** · **Materialize**（内存资产）· **Open source** |
| **Info rows** | **Base** · **Domain** · **Blend mode** · **Root** · **Source**（链接）· **Asset**（链接）· **Storage** · **Provenance** |
| **Diagnostics (N)** | bridge 为这个文件持有的每条记录：`[DSHnnnn] L{line}:{col} message`，每条都是链接，在编辑器里打开到那一行 —— 记录指向被 import 的头文件时就打开头文件 |
| **Provenance** | 指纹状态和一行解释，然后是答案：**Revert to Source** · **Adopt Into Source** · **Detach**；DreamShader 没生成过的材质则是 **Export .dss** / **Export .dsi**。源文件随插件发布时 *Adopt* 禁用并说明原因 |
| **Imports (N)** / **Used by (N)** | 材质传递地 import 的每个头文件和函数 / 头文件或函数被哪些材质 import —— 都是链接 |
| **Inheritance** | 父链，根在前，每行都是可点击链接 |
| **Child instances (N)** | 父级是它的已加载实例，以及 —— 从资产注册表的引用者里找到的 —— 本会话没人加载过的已保存实例，标为 *(not loaded)*；点击加载 |

`.dsp` （自 2.1.0 起） 的检查器没有预览和 *Create instance*，信息行以 **Pipeline** 开头（pass、buffer、参数个数和顺序），
Provenance 一节给出同样的三个答案。

Provenance 状态：`Generated` 是 *generated (matches the last build)*；`Tweaked` （自 1.9.0 起） 是
*generated, with parameter overrides on the instance* —— 值是你的，重建会[放回去](../generation/regeneration.md#生成实例上的参数覆盖)；
`Unstamped` 是 *generated (no comparable digest)*；`Diverged` 是 *edited by hand since the last build*，橙色显示，是唯一会拒绝重建的状态；
`Foreign` 是 *not generated by DreamShader*。状态栏的 **edited by hand** 计数只数 `Diverged`。

<a id="状态栏与右键菜单"></a>

### 状态栏与右键菜单

状态栏：`{N} sources` · **ok** / **stale** / **errors** / **edited by hand** / **Ephemeral** 计数（每个都是一键过滤）· **not managed**
（跳到那个节点）· bridge 状态：`bridge: idle`、编译时 `bridge: {action}`、同工程的第二个编辑器持有写入锁时
`bridge: idle (another editor owns writes)`，或 `bridge: off`。tooltip 是 bridge 的上一个结果。

<a id="右键菜单"></a>

#### 右键菜单

两种模式的列表里都能右键。

| 一节 | 条目 |
| :-- | :-- |
| **Open** | **Open material**（`Enter`；`.dsp` 是它的详情面板）· **Open source**（`Ctrl+Enter`，有错误时打开到第一个错误）· **Reveal in Content Browser** |
| **Build** | **Compile** · **Create instance**（`.dsp` 没有）· **Materialize**（内存资产；作用于选中的每个内存项） |
| **Generated asset** | **Revert to Source**（作用于选中的每个生成项，逐个确认）· **Adopt Into Source** · **Detach From DreamShader** —— 只在资产带 DreamShader 标记时 |
| **Decompiler** | **Export .dss** —— 只对手写的材质或函数（`.dss`）或普通材质实例（`.dsi`） |
| **Copy** | **Copy source path** · **Copy asset path** |

这个 tab 不轮询：任何途径（watcher、这个 tab、来历动作、VSCode 扩展）生成了一个源文件、bridge 提交了诊断、源文件在磁盘上增删改名或被修改、
资产注册表里资产增删改名，都会让对应的项重算，并合并成每 tick 一次刷新。

<a id="新建源文件"></a>

### 新建源文件

*New ▾* 从插件 `Resources/Templates` 下的模板写一个文件，bridge 的 watcher 像对待任何保存一样列出并编译它。

| 一节 | 条目 |
| :-- | :-- |
| DreamShaderLang 2.0 | **Material (.dss)** （自 2.0.0 起） · **Material function (.dss)** （自 2.0.0 起） · **Instance (.dsi)** |
| Custom Pass pipeline (.dsp) （自 2.1.0 起） | **Fullscreen post-process chain** · **Mesh mask chain** · **Compute chain** —— 管线模板还会在 `.dsp` 旁边写出它的 pass 用到的 `.dss` 材质；见 [Custom Pass（.dsp）](../language-v2/passes.md) |
| 1.x blocks | **Material (.dsm)** · **Material function (.dsf)** · **Header (.dsh)** |

对话框问 **Name**（必须是标识符）和 **Folder**（导航树指向的源文件夹；那是插件的文件夹时改用工程的 `DShader` 根 —— 插件源文件是只读的）。
资产落在邻居的旁边：在 `DShader/Materials/` 里新建的文件编译到 `/Game/Materials/…`。1.x 块自己说出来 —— 它的 `Name=` 是文件夹相对根的路径加名字；
`.dss` 什么都不说 —— 名字是它 `export` 的名字，文件夹就是文件所在的地方。名字不是标识符、文件夹不在可写根下、文件已存在时拒绝，并给出原因。

<a id="创建材质实例"></a>

### 创建材质实例

入口：检查器、右键菜单，以及 Content Browser 的 **Create DreamShader instance**。

| 项目 | 值 |
| :-- | :-- |
| 窗口标题 | **Create material instance** |
| 尺寸 | 480×240，模态，不能通过最小化/最大化调整 |

| 字段 | 类型 | 默认值 |
| :-- | :-- | :-- |
| **Parent** | 只读文本 | 父材质名 |
| **Name** | 文本框 | `MI_<ParentName>`，与已有资产去重 |
| **Folder** | 文本框 | 父级所在文件夹加上 *Material Instance Subfolder* 设置（默认 `Instances`）；子目录为空时实例建在父级旁边 |
| **Browse...** | 按钮 | 打开标题为 "Choose a destination folder" 的文件夹选择器 |
| **Open the instance after creating** | 复选框 | 勾选 |
| **Cancel** / **Create** | 按钮 | — |

| 守卫 | 错误 |
| :-- | :-- |
| 没有父材质 | `No parent material was provided.` |
| 名字或目标文件夹为空 | `Provide a name and a destination folder.` |
| 父级是内存材质 | 转发 [Materialize](#materialize) 的错误 |
| 目标路径上已有资产 | `An asset already exists at {PackageName}.` |
| package 创建失败 | `Failed to create package {PackageName}.` |
| 对象创建失败 | `Failed to create the material instance object.` |
| package 保存失败 | `Generated DreamShader asset '{Path}' could not be saved.` |
| 从打开窗口到确认之间父级失效 | `The parent material is no longer available.` —— 窗口关闭 |

成功时 toast 是 `Created {Name}` —— **Name** 框里的资产名，不是 object path —— 窗口关闭。
失败时错误被 toast 出来，而**窗口保持打开**，方便修正名字或文件夹。

| 细节 | 行为 |
| :-- | :-- |
| 创建的类 | 普通 `UMaterialInstanceConstant`，不是 `UDreamShaderMaterialInstance` |
| 对象标记 | `RF_Public \| RF_Standalone` |
| Parent 赋值 | 以 editor-only 方式设置，随后触发 post-edit change 和资产创建广播 |
| 保存失败回滚 | 半成品对象会被撤销广播、剥掉标记、无重定向器地改名进 transient package、标记为垃圾，并清掉 package 的脏标记 —— 因此它无法在 GC 后存活、无法出现在 Content Browser、无法被 *Save All* 落盘，也不会挡住同名重试 |

给内存中的父级创建实例时会先把父级落盘，因此子实例永远不会引用一个 transient 对象。从没编译过的源文件会先编译；
仍然没有材质时 toast 是 `Compile {File} first.`

<a id="materialize"></a>

### Materialize

*Materialize* 把内存材质，以及它包着的隐藏 base，一起写到磁盘。

| 规则 | 细节 |
| :-- | :-- |
| "内存"的判定 | 材质的 package 带着 newly-created package 标记 |
| 已在磁盘上 | 原样返回 —— 该操作是空操作 |
| 前提 | 材质必须是带有记录源文件路径的 `UDreamShaderMaterialInstance` |
| 实现 | 以强制、Materialized 重跑该源文件的生成，然后在同一 object path 上重新加载对象 |

| 消息 | 原因 |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | 材质不是 DreamShader 实例，或者没有记录源路径 |
| `Failed to materialize the material to disk: {Error}` | 生成失败 |
| `Materialized the material but could not reload it at {ObjectPath}.` | 生成成功但对象没能重新加载 |
| `Materialized {Name} to disk` | 成功 —— 检查器切到那个已落盘的资产 |

按钮在点击时按 object path 重新解析材质，而不是握着构建面板时捕获的指针，这样中间发生的删除或 GC 就不会导致崩溃。
整套模型见[内存材质](../generation/ephemeral.md)。

<a id="键盘"></a>

#### 键盘

| 键 | 命令 |
| :-- | :-- |
| `F5` | Refresh |
| `Ctrl+B` | 编译选中项 |
| `Ctrl+Shift+B` | Compile all |
| `Enter` | 打开材质 |
| `Ctrl+Enter` | 打开源文件 |
| `Ctrl+F` | 聚焦搜索框 |

<a id="预览"></a>

## 预览

插件这一侧的渲染器编译一个材质源文件、把它的材质渲染出来 —— 一次，或者一串帧。

| | 单次预览 | 流式预览 |
| :-- | :-- | :-- |
| 是什么 | 渲染出的 PNG 加 `preview.json` | WebSocket 上的实时帧流 |
| 触发方式 | `previewMaterial` 请求文件，或 `stream` 为 false 的 WebSocket 消息 | `stream` 为 true（默认）的 `previewMaterial` WebSocket 消息 |
| 等待 shader 编译 | 是 —— 文件 / PNG 路径会短暂卡住编辑器；WebSocket raw 路径跨 tick 等待、不阻塞 | 否 |
| 相机控制 | 请求文件通道：无。WebSocket 通道：有 | 有，通过 `previewControl` |
| 起始版本 | — | （自 1.5.0 起）；原始 RGBA8 帧与 Graph 断点 （自 1.7.0 起） |

Material Content Browser 检查器里的预览也走这个渲染器 （自 1.9.0 起）：224×224、异步、每次编译后重渲。1.x 的 *Dream Shader Gen*
页和它的静态资产缩略图已经没有了。

能预览的是构建材质或材质实例的源文件：`.dss`、`.dsi` 或 `.dsm` （自 2.0.0 起）。`.dsf`、`.dsh` 或 `.dsp` 会被拒绝，消息是
`DreamShader preview renders a material or a material instance, so it takes a .dss, .dsi or .dsm source: '{File}'.`
源文件按交互式编译的方式编译：ThinCustom 产物是 Ephemeral 的，所以预览从不把它写盘；`Graph` 材质和 `.dsi` 实例是普通资产，像任何编译一样被保存。
源文件由编译自己的前半段解析，所以预览和编译对"一个文件构建哪个资产"不会有分歧。

| 项目 | 值 |
| :-- | :-- |
| 端点 | `ws://127.0.0.1:17864`，仅回环；非回环客户端会被拒绝 |
| 尺寸 | `width` / `height` 默认 `512`，钳制到 `[64, 2048]` |
| 帧率 | `frameRate` 默认 `2.0`，钳制到 `[0.25, 60.0]`；`<= 0` 关闭流式 |
| 网格 | `plane`、`cube`、`cylinder`、`shaderball`，其他任何值静默回退到 `sphere` |
| 轨道默认值 | yaw `-157.5`，pitch `-11.25` —— Unreal 自己的场景缩略图默认值 |
| 输出 | `Saved/DreamShader/Bridge/Preview/` 下的 PNG，加 `preview.json` |

**警告：**

网格和两个轨道角度会被写到**材质资产自己的**场景缩略图信息上 —— 也就是原生材质编辑器的预览形状按钮和
拖拽旋转视口写的那几个字段。所以一次预览渲染会改变该材质在 Content Browser 图块和材质编辑器视口里用的形状与相机。
未知的网格名会被静默接受并渲染成球；看 `preview.json` 里回显的 `mesh` 才知道实际用了什么。投影的宽高比恒为 1:1 ——
请求非正方形尺寸只会把球拉成椭圆，请在客户端留黑边。

<a id="帧编码与空闲退避"></a>

### 帧编码与空闲退避

（自 1.7.0 起）

流式路径默认发**原始 RGBA8 帧**（`encoding: "raw"`）：编辑器把 render target 读回来、把像素原样发出，
不做 PNG 编码，客户端直接画到 canvas 上。这就是流畅 30–60 FPS 之所以负担得起的原因。旧的 JSON 元数据加 PNG 那条路径仍然以
`encoding: "png"` 提供给老客户端。

画面不动时，相同的帧会被丢弃；连续几帧相同之后渲染时钟退到一个很低的空闲速率，直到一次编辑、一次相机 /
网格变化或一次探针变化把它唤醒。省略了某个字段的 `previewControl` —— 包括 `frameRate` —— 保持当前值而不是重置回 2 FPS，
并且可以携带 `width` / `height` / `mesh`。帧保留开始渲染时的相机、探针、编码和尺寸；GPU 读回期间改的控制作用于下一帧；
开始另一个预览请求会丢掉上一个请求挂起的帧，所以它不会顶着新请求的 ID 发出去 （自 2.1.2 起）。

<a id="graph-断点探针预览"></a>

### Graph 断点（探针预览）

（自 1.7.0 起）

在材质代码的某一行上打断点 —— `.dsm` 里的一行 `Graph`、`.dss` 里材质函数体的一条语句 （自 2.0.0 起）（DreamShaderLang VS Code
扩展里是 F9）—— 预览网格就显示**那一行绑定的值**，而不是成品材质 —— 材质编辑器右键 *Start Previewing Node* 的文本版。

| 方面 | 行为 |
| :-- | :-- |
| 选择 | 同一时刻只有一个探针 —— 活动源文件中最靠上的那个启用断点 |
| 行吸附 | 空行或注释行上的断点向前吸附到下一个绑定了值的行 |
| 显示什么 | 该绑定值接进一个 unlit 的 Emissive 预览（值是 `MaterialAttributes` / Substrate 时接对应输入；纹理对象用默认坐标采样一次） |
| 编译前设置 | 记为 *pending*，在下一次生成时挂上 |
| 重新编译后 | 自动重解析；行号可能移动，并会把新行号告诉客户端 |

机制上复用引擎自己的预览材质：一个临时 `UPreviewMaterial` 共享生成材质的表达式集合、把被探测的节点接进去，
经 `FMaterialUpdateContext` 重新编译。`UPreviewMaterial` 受限的 `ShouldCache` 把这次重编译压到少数几个 shader。
协议是 `setProbe` / `clearProbe` / `probeState`。

插件侧没有 pitch 钳制 —— 编辑器扩展在发送前会钳到约 ±89°，不这么做的客户端会把相机翻过去。
运动模糊、抗锯齿和动态屏幕百分比都是关的，alpha 被强制成不透明，UI domain 材质总是画在平面上。
这套协议的客户端一侧见 [VSCode 与 Rider](vscode.md)。

<a id="virtualfunction-工具"></a>

## VirtualFunction 工具

为已有 `UMaterialFunction` 资产生成、打开和刷新 [`VirtualFunction`](../language/blocks.md) 声明的那组操作。

| 项目 | 值 |
| :-- | :-- |
| 挂在 | `UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend` 的右键菜单 ▸ *DreamShader* ▸ **VirtualFunction**，以及材质编辑器的 **DreamShader** 工具栏组合按钮 |
| 写入 | `<SourceDirectory>/VirtualFunctions/<Name>.dsh`，UTF-8 无 BOM |
| 起始版本 | （自 1.2.0 起） 声明本身 · （自 1.2.1 起） 菜单 · （自 1.2.2 起） 复用、*OpenVirtualFunction* 和启动同步 |

这个 section 每次右键都会重新构建。构建之前，编辑器会在所有源文件里搜索一个解析出的资产与当前选中项一致的声明 ——
**你拿到的条目取决于这个答案**。两种写法都算声明：任何源文件里的 1.x `VirtualFunction` 块，以及 `.dsh` / `.dss` 里带 `/// @asset`
行的 `extern` 原型 （自 2.0.0 起）。

| 已有声明？ | 条目 |
| :-- | :-- |
| 有 | **OpenVirtualFunction** —— 在声明的行列位置打开文件 · **Copy Virtual Function Reference** —— 复制一个用**声明名**（而非资产名）构建的调用 |
| 没有 | **CopyVirtualFunction** —— 复制整段声明 · **CreateVirtualFunction** —— 写入一个新 `.dsh` 并打开 · **Copy extern Prototype (2.0)** / **Create extern Prototype (2.0)** （自 2.0.0 起） —— 复制或写出 `.dss` 用的 `extern` 原型（带 `/// @asset` 行），给 `.dss` 源文件 `#include` · **CopyVirtualFunctionCall** —— 复制一个 1.x 调用示例 |

**警告：**

这个查找会在每次右键 Material Function 资产时，**从磁盘重新枚举并重新读取每个源码根下 `Packages` 之外的每一个源文件**（`.dsm`、`.dsh`、
`.dsf`、`.dss`、`.dsi`、`.dsp`）；同时提到 `extern` 和 `@asset` 的 `.dsh` / `.dss` 还会被解析。开销随源文件数量增长，而且是在菜单出现之前付掉的。
没有缓存，也没有别的关掉办法 —— 除了 `-NoDreamShaderEditorBridge`，而它会把菜单整个去掉。

**提示：**

**对已经有声明的资产执行 CreateVirtualFunction 会静默改道去执行 OpenVirtualFunction。** 它绝不会写出重复文件，
拿到的 toast 是 `Opened VirtualFunction definition: {File}`，而不是"已创建"。*Create extern Prototype* 在任何一种写法已存在时也一样。

文件名是 `<净化后的 MaterialFunction 名>.dsh`，冲突时是 `<净化后的名字>_<object path 的 crc32>.dsh`，
CRC 用八位小写十六进制。写在这里的 `.dsh` 会像其他头文件一样被源目录 watcher 捡到，
所以 import 它的材质会自动重编。生成的声明带
`Options.Asset = Path(Game|Engine|Plugins.<Name>, "…")` —— 见 [Path 资产引用](../parameters/path.md) ——
每个函数输入一条 `Inputs`，每个输出一条 `Outputs`。复制出来的调用示例总是以 `OutputIndex=0` 结尾，
哪怕函数只有一个输出；示例里可选输入写成 `default`，这是只属于 1.x 文本的 legacy 规则，`dsc migrate` 会改写成具名实参。

<a id="启动同步服务"></a>

### 启动同步服务

每个编辑器会话一次，在 bridge 启动时，DreamShader 会重读工程里每一段 `VirtualFunction` 声明，
从活资产重建它，并在文本发生变化时把文件写回。

**警告：**

**这件事没有 watcher。** 改动 `UMaterialFunction` 的输入或输出不会刷新声明，保存 `.dsh` 也不会。
声明只在下一次编辑器启动时被重新同步。

| 项目 | 行为 |
| :-- | :-- |
| 扫描的文件 | **可写**源码根（工程自己的；插件根被跳过）下的每个源文件，排除每个根的 `Packages` 文件夹 |
| 拒绝的文件 | 含[预处理指令](../language/lexical.md#预处理器)的源文件 —— `DSH9001` （自 1.9.0 起）：同步按字节偏移拼接，而预处理只保证行数守恒，第一处被删的行之后每个偏移都是错的。它报一次、文件原样不动 |
| 关键字匹配 | 裸词 `VirtualFunction`，**大小写敏感**，带标识符边界检查 |
| 跳过的区域 | 带 `\` 转义的字符串字面量、`//` 行注释和 `/* */` 块注释 |
| 校验 | 每个抽出的块由 legacy 前端单独重新解析 （自 2.0.0 起），必须恰好产出**一个** `VirtualFunction`，且其 `Options.Asset` 能解析 |
| 不同步 | 带 `/// @asset` 的 2.0 `extern` 原型：查找认它，同步从不改写它 （自 2.0.0 起） |
| 比较 | 在归一化文本上做 —— CRLF 和 CR 变 LF，然后整体 trim —— 所以行尾差异永远不会触发重写 |
| 写回 | 整个文件，UTF-8 无 BOM |

同步问题会以 stage `virtualFunctionSync`、code `virtual-function-sync`、severity `error` 发布成诊断（`DSH9001` 写在消息文本里）。
汇报日志：`DreamShader refreshed {Count} VirtualFunction definition(s) in '{File}'.` 和
`DreamShader scanned {Scanned} VirtualFunction definition(s), refreshed {Refreshed}, reported {Issues} issue(s).`

<a id="资产改名同步"></a>

## 资产改名同步

（自 1.9.0 起）

在编辑器里重命名或移动一个资产，会改写引用它的 `.dsm` / `.dsf` / `.dsh` 文件，让源码继续指向这个资产，而不是它原来的路径。
在此之前，重命名一张纹理会**静默**弄坏每个引用它的源文件 —— 改名那一刻什么都没发生，失败在下一次重建时才出现。

| 项目 | 值 |
| :-- | :-- |
| 触发 | `IAssetRegistry::OnAssetRenamed` —— Content Browser 改名、拖动移动、*Fix Up Redirectors*，或任何 asset-tools 改名 |
| 写入 | 每个文本变了的可写工程源文件，UTF-8 无 BOM；每个文件先备份成 `<file>.bak` |
| 设置 | *Sync Source References On Asset Rename* —— `bSyncSourceReferencesOnAssetRename`，分类 *Editor*，默认**开**，实时读取 |

改名被收集成批，**最后一次改名之后 0.5 秒**才处理，所以 *Fix Up Redirectors* 一帧里冒出的几百个事件只扫一遍源码树。
只有持有 [bridge](#一个工程只有一个编辑器持有-bridge) 的编辑器会写；commandlet 里和 `-NoDreamShaderEditorBridge` 下服务根本不启动。

| 写法 | `/Game/Textures/T_X` 改名为 `T_Y` 之后 |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `Path(Game, "Textures/T_Y")` |
| `Path(Game, "Textures/T_X.T_X")` | `Path(Game, "Textures/T_Y.T_Y")` |
| `Path("/Game/Textures/T_X")` | `Path("/Game/Textures/T_Y")` |
| `"/Game/Textures/T_X"` | `"/Game/Textures/T_Y"` |
| `"Texture2D'/Game/Textures/T_X.T_X'"` | `"Texture2D'/Game/Textures/T_Y.T_Y'"` |
| `/Script/Engine.Texture2D'/Game/Textures/T_X.T_X'` | `/Script/Engine.Texture2D'/Game/Textures/T_Y.T_Y'` |

两条是承重的：**写下的形式被保留**（package 路径还是 package 路径，带 `.Name` 的保留 `.Name`，相对的仍然相对 —— 除非资产离开了它所写的根，
那时塌缩成带引号的绝对路径；带类前缀的写法类名不动）；**引用按整条路径精确相等匹配，从不按前缀** —— 改名 `/Game/Foo` 不会碰 `/Game/FooBar` 或
`/Game/Foo/T_X`。

从不改写的地方：`Function` 和 `GraphFunction` 函数体（那是原样交给 shader 编译器的 HLSL）、注释、`Path(…)` 和带类写法之外的裸绝对路径、
跨行的引用、插件源码根下的文件（只读）、`DShader/Packages`。含 `#if` 的源文件**会**被改写 —— 它只按行读写原始文件，
被本次构建删掉的分支里的引用也一起更新。

先写文本、再重建：一批里每个文件都落盘之后才编译任何东西，重建请求和源目录 watcher 一样派发，但不受 *Auto Compile On Save* 约束。
备份失败是 `DSH9020`，文件不写；备份之后写入失败是 `DSH9021`，原文件在 `.bak` 里。`<file>.bak` 只保存**一代**。每批一条 `Display`
日志，列出每个被改写的文件、它的备份，以及背后的旧 → 新路径对 —— 编辑器提示"文件在磁盘上变了"时去找的就是这条。删除不在处理范围内：
被删的资产没有新路径可写，编译错误就是正确的结果。

<a id="bridge-产物"></a>

## Bridge 产物

编辑器会在 `<Project>/Saved/DreamShader/Bridge/` 下写一组文件供外部工具读取。manifest 在 bridge 启动时导出，
并在每次 *Open Dream Shader Workspace* 时重新导出；其余的随工作产生。

| 产物 | 内容 |
| :-- | :-- |
| `material-expressions.json` | 反射得到的 `UMaterialExpression` 目录 —— schema `DreamShader.MaterialExpressions`，version 1。这是 [`UE.Expression`](../builtins/ue-expression.md) 补全的数据源 （自 1.2.10 起） |
| `substrate-builtins.json` | [`Substrate.*`](../builtins/substrate.md) 目录 —— schema `DreamShader.SubstrateBuiltins`，version 1。UE 5.4 以下写出的是空数组、`"supported": false` 和 `"unsupportedReason"` |
| `settings.json` | `ShadingModel` / `BlendMode` / `MaterialDomain` 别名表 —— schema `DreamShader.Settings`，version 1。见[枚举取值](../settings/material-enums.md) |
| `pass-keys.json` （自 2.1.0 起） | `.dsp` 能写什么，供补全 —— schema `DreamShader.PassKeys`，version 1，和 `settings.json` 一起写；UE 5.8 以下 `supported: false` |
| `dreamshader-builtin-catalog.json` （自 2.0.0 起） | 绑定器自己的节点目录 —— 类、pin、属性、别名、位置顺序；在启动生成扫描之后导出（那时每个模块都已加载） |
| `../Index/<root>/<source>.index.json` （自 2.0.0 起） | 每个源文件的符号索引（schema `dreamshader-symbol-index`），每次编译到绑定为止 —— 失败的也算 —— 之后刷新；`dsc index` 按同样规则写同一个文件 |
| `diagnostics.json`、`diagnostics/` | 当前全部诊断，聚合版和按文件分片版。字段见 [VSCode 与 Rider](vscode.md#扩展读到的诊断) |
| `bridge.db` | 诊断和 manifest 的 SQLite 镜像，启动和关闭时都会被删掉 |
| `preview.json`、`Preview/*.png` | 单次预览结果 |
| `Requests/*.json` | 入站请求目录，每 0.1 秒轮询一次，**派发前**即删（这样一个能弄崩编辑器的请求不会在每次启动时重放）；读不出来的文件留给下一轮，因为那几乎总是一个正在被写入的文件 |
| `status.json` （自 1.6.0 起） | 心跳，空闲时每 2 秒重写一次，**退出时删除** —— 所以文件缺失确定地意味着「没在运行」。带 `protocol`、`pid`、`project`、`projectDir`、`engineDir`、`pluginVersion`、`busy` / `busyAction`、`lastResult`、`heartbeatUtc` |
| `Responses/<requestId>.json` （自 1.6.0 起） | 任何带 `requestId` 的请求的回复：`ok`、`durationMs`、`message`（永远是 invariant 英文）和本次编译的诊断 —— 每条带自己的文件，被 import 的头文件上的也在内。原子写入（写在旁边再改名） |
| `owner.lock` （自 1.8.0 起） | 持有本工程 bridge 的编辑器的 pid 与心跳 —— 见下 |

每份 manifest 都带一个 `generatedAt` ISO-8601 UTC 时间戳。这些 schema 和请求协议从消费端的角度记在
[VSCode 与 Rider](vscode.md)。

**警告：**

判断心跳是否过期**之前**先读 `busy`。编译会阻塞游戏线程，所以编译期间心跳是停的 —— 只看时间戳的客户端
会把每一次真实编译都读成崩溃。剩下的歧义是硬崩溃，它会把文件留在原地；记录下来的 `pid` 用来收尾。

**Custom Pass shader 文件** （自 2.1.0 起）：`.dsp` 的 HLSL pass 从可能放在任何地方的 `.usf` 编译，所以 bridge 为它们另设了 watch ——
每个装着某个 `.dsp` 编译所用文件的目录一个。改了其中一个文件会把用它的每个 `.dsp` **不强制**地排进队列（`.dsp` 的构建键覆盖了那些文本，
改了无关文件的保存会被跳过）。编译通过 pre-check 之后，新的 shader 下一帧就生效；pre-check 失败报在 `.usf` 的那一行，什么都不写，旧版本继续运行。
`.dsp` 和它周围的 `.dss` 互相引用，一边编译成功会把另一边不强制地排进队列。见 [Custom Pass（.dsp）](../language-v2/passes.md)。

<a id="一个工程只有一个编辑器持有-bridge"></a>

### 一个工程只有一个编辑器持有 bridge

（自 1.8.0 起）

bridge 目录是按**工程**而不是按进程的：一个 `Requests` 目录、一个 `status.json`、一个心跳。两个开在同一
工程上的编辑器于是都在轮询同一个队列 —— 谁先到谁消费掉文件，另一个读到的是一个删了一半的文件 ——
而且都在用自己的 pid 覆写 `status.json`。

| 项目 | 说明 |
| :-- | :-- |
| 文件 | `<Project>/Saved/DreamShader/Bridge/owner.lock` |
| 内容 | `pid` 和一个 ISO-8601 `heartbeat`，随常规心跳刷新 |
| 认定持有的条件 | 持有进程仍在运行**且**心跳不超过 30 秒 |
| 释放 | 编辑器关闭时释放，所以下一个编辑器在它的下一次心跳就接手，不用等过期窗口 |

两个条件缺一不可：只看 pid，会在持有者正在编译（编译阻塞游戏线程、心跳因此停摆）时把 bridge 交出去；
只看心跳，会在硬崩溃之后留下一段无主窗口。

非持有者仍然编译自己的 Ephemeral 材质 —— 那是每进程各自的事 —— 但它不消费请求、不写 `status.json`，
也**不把生成资产写到磁盘**，否则两个编辑器会 `SavePackage` 同一个文件，而这场竞争的输家不是合并，是一个损坏的 package 或一个死掉的编辑器。
这样的编译报 info `DSH8209`，结果行是：

```text
Skipped {AssetPath}; another editor owns this project's DreamShader bridge, and only that one
writes generated assets to disk.
```

只有持有者发布或清理共享的回复、状态、诊断、`bridge.db` 和 manifest （自 2.1.2 起）；非持有者保留自己本地的诊断和浏览器通知，
关掉它也不会动持有者的文件（在此之前，第二个编辑器会删掉或覆盖持有者的回复、状态、数据库和诊断）。
[命令行](commandlet.md)不受影响：它没有 bridge、没有锁文件、也没有第二个进程要谈判，而写这些资产正是它的全部工作。

<a id="关闭整套集成"></a>

## 关闭整套集成

```powershell
UnrealEditor.exe "<Project>.uproject" -NoDreamShaderEditorBridge
```

这个开关按裸命令行参数解析，所以 `-NoDreamShaderEditorBridge` 是唯一被接受的写法。存在时，
`StartupModule` 在创建任何东西之前就返回。

| 被关掉 | 仍然有效 |
| :-- | :-- |
| editor bridge —— 文件 watcher 和保存即编译、防抖队列、诊断存储及三个 sink、`bridge.db`、请求文件轮询、VirtualFunction 启动同步、post-engine-init 的全量 Ephemeral 生成、设置 watcher | 运行时模块 —— `DreamShaderLang`（两个前端、绑定器、IR）、`DreamShader`（设置对象、`UDreamShaderMaterialInstance`、编译接口）和 `DreamShaderPass` —— 以及编译接口用到的 `DreamShaderCompiler` 编辑器模块 |
| 端口 `17864` 上的预览 WebSocket 服务，以及整个预览渲染器 | [命令行](commandlet.md)，它从不使用 bridge |
| 本页所有菜单、工具栏和右键菜单项，以及节点右键菜单 | 已经生成并保存在磁盘上的资产 |
| Custom Pass shader 文件的 `.usf` / `.ush` watch | [Pass pipeline 详情面板](#pass-pipeline-详情面板) （自 2.1.0 起） |
| Material Content Browser 的 tab 注册 —— 该面板完全打不开 | |
| 导出的 manifest 和内建目录，以及 `DreamShader.code-workspace` 的重新生成 | |
| 资产改名同步服务 | |

commandlet 运行会以另一条路径到达同样的状态：模块一旦检测到自己在 commandlet 进程里就提前返回，
只安装 cook 期的落盘钩子，而且仅当 `-run=` 含 `Cook` 且没有 `-cookworker` 时。

<a id="说明"></a>

## 说明

- 四个 VirtualFunction 条目标签是没有空格的驼峰 —— `OpenVirtualFunction`、`CopyVirtualFunction`、
  `CreateVirtualFunction`、`CopyVirtualFunctionCall` —— 而第五个写作 **Copy Virtual Function Reference**。
  这个不一致存在于出厂的标签里。
- bridge 弹出的 toast 4 秒后消失；两种手改通知不会自动消失。
- 编辑器从不主动写出 ThinCustom 产物的 `.uasset`。落盘发生在 cook、通过[命令行](commandlet.md)，
  或者显式的 *Materialize*；`Graph` 材质和材质函数每次允许写入的构建都会保存。
- 在 Project Settings 里改 *Default Compiler Backend*、三张枚举映射表之一或 *Preprocessor Defines*，会**不强制**地重新生成每个源文件 ——
  它们都是[构建键](../generation/regeneration.md#构建键)的一部分，所以恰好是受影响的源文件过不了跳过检查 —— 并在已落盘的 ThinCustom 产物会遮蔽结果时弹一条
  失败 toast。**没有其他设置属性会触发反应。**

<a id="示例"></a>

## 示例

先关掉集成启动编辑器，再无头地做同样的事：

```powershell
# 没有菜单，没有 watcher，没有 bridge。
& "$Engine\Binaries\Win64\UnrealEditor.exe" "I:\Project\Project.uproject" -NoDreamShaderEditorBridge

# commandlet 不受这个开关影响。
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "I:\Project\Project.uproject" `
    -run=DreamShader compile -All -Force -unattended -nopause -nosplash -stdout -log
```

<a id="继续阅读"></a>

## 继续阅读

- [反编译导出](decompiler.md) — Export .dss / .dsi 与 Export Legacy 产出什么，又做不到什么。
  - [VSCode 与 Rider](vscode.md) — workspace 文件、两个扩展，以及它们消费的产物。
  - [命令行](commandlet.md) — 无头入口，它写的是真资产。
  - [内存材质](../generation/ephemeral.md) — 为什么 Content Browser 是空的，以及 Materialize 做了什么。
  - [重新生成](../generation/regeneration.md) — 手改检测、通知和三个出口。
  - [项目设置](../settings/project.md) — 这些命令读写的设置。
  - [错误速查](../diagnostics/index.md) — 每条消息、原因和处理办法。
