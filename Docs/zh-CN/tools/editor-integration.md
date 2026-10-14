---
title: "编辑器集成"
description: "编辑器集成"
---

<a id="editor-integration"></a>

# 编辑器集成

> [DreamShader](../index.md) » [工具](index.md) » **编辑器集成**

DreamShader 挂接到 Unreal 编辑器 UI 上的每一处：Tools 菜单、关卡编辑器工具栏、Content Browser 资产右键菜单、材质编辑器工具栏、Window 菜单、Custom Pass 管线的详情面板，以及它自行弹出的通知。

| | |
| :-- | :-- |
| 注册方 | 模块启动时的 `DreamShaderEditor`，通过 `UToolMenus::RegisterStartupCallback` |
| ToolMenu 所有者 | `DreamShaderEditor`（bridge 条目）· `DreamShaderMaterialBrowser`（browser 条目） |
| 被禁用于 | `-NoDreamShaderEditorBridge`，以及任何 commandlet 运行 |

菜单注册是幂等的——第二次注册什么都不加——并且会在编辑器关闭期间跳过。

<a id="tools-menu"></a>

## Tools 菜单

*Tools ▸ DreamShader*，扩展 `LevelEditor.MainMenu.Tools`，section `DreamShader`。

| 条目名 | 标签 | Tooltip | 图标 | 效果 | 参考 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.RecompileAll` | **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first." | `Icons.Refresh` | 经过是/否确认后，重建依赖图并把每个根的所有源文件——`.dsm`、`.dsf`、`.dss`、`.dsi` 和 `.dsp`，尽管标签如此——排入队列，**强制**越过构建键跳过 | [见下](#recompile-dsm) |
| `DreamShader.CleanGeneratedShaders` | **Clean Generated Shaders** | "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile." | `Icons.Delete` | 删除生成着色器目录下的每个 `*.ush`，然后排入一次全量**强制**扫描，使每个文件都被重新生成 | [见下](#clean-generated-shaders) |
| `DreamShader.MakeEphemeral` | **Make Ephemeral** | "Delete the packages of Materialized ThinCustom products so they go back to being Ephemeral. Shows a confirmation with the full list; source files are untouched and the products are rebuilt in memory. Graph materials and material functions are not listed -- they have no Ephemeral state." | `Icons.Delete` | 通过标准编辑器删除流程，删除带 DreamShader 来源元数据的 ThinCustom 产物在磁盘上的 package | [见下](#make-ephemeral) |
| `DreamShader.ToggleShowEphemeralMaterials` | **Show Ephemeral Materials** | "Show Ephemeral ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. Graph-backend materials are plain UMaterials with no Ephemeral state, so this toggle does not affect them. While shown, an explicit Save on one would materialize it to disk (the shadow warning and Make Ephemeral cover recovery)." | *（无）* | 开关按钮。翻转 `bShowEphemeralMaterials` 并将其写入 `DefaultEngine.ini` | [见下](#show-ephemeral-materials) |
| `DreamShader.OpenWorkspace` | **Open Dream Shader Workspace (VSCode)** *（自 1.2.1 起）* | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | 重新导出三个 bridge manifest，重写 `DShader/DreamShader.code-workspace`，然后启动它 | [工作区](workspace.md) |
| `OpenMaterialContentBrowser` | **Material Content Browser** *（自 1.5.0 起）* | "Open the DreamShader Material Content Browser." | `ClassIcon.Material` | 唤起 `DreamShaderMaterialBrowser` nomad tab | [Material Content Browser](material-browser.md) |

> [!NOTE]
> 前五项和第六项由两个不同的启动回调注册进同一个 `DreamShader` section。它们在 section 内部的相对顺序取决于注册顺序，且不保证在两次编辑器运行之间保持一致。

<a id="level-editor-toolbar"></a>

## 关卡编辑器工具栏

一个 **Dream** 组合按钮，由 Dream 系列插件（DreamShader、DreamFX、DreamGUI）共用：每个插件在它还不存在时加上它，所以不管哪个先加载，它都只出现一次，并归属于所有者 `DreamToolsShared`，没有插件会注销它。

| 项目 | 值 |
| :-- | :-- |
| 扩展 | `LevelEditor.LevelEditorToolBar.AssetsToolBar`，section `DreamTools` |
| 条目 | `DreamTools.OpenWorkspaceCombo`，标签 **Dream**，图标 `Icons.OpenInExternalEditor`，tooltip "Dream-family language tools: open a source workspace in VSCode, or rebuild a whole source tree (DreamShader / DreamFX / DreamUI)." |
| 打开 | 共享菜单 `DreamTools.Actions`，每个插件在里面加自己的一节 |

DreamShader 在里面的一节，标题为 **DreamShader**：

| 条目名 | 标签 | Tooltip | 图标 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.OpenWorkspaceShared` | **DreamShader Workspace** | "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable." | `Icons.OpenInExternalEditor` | 等同 *Open Dream Shader Workspace (VSCode)* |
| `DreamShader.RecompileAllShared` | **Recompile DSM** | "Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first." | `Icons.Refresh` | 等同 *Recompile DSM*，包括确认框 |

<a id="window-menu"></a>

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

<a id="content-browser-context-menus"></a>

## Content Browser 右键菜单

所有条目都加进按类划分的资产右键菜单里那个标准的 `GetAssetActions` section。

> [!WARNING]
> **每一个 DreamShader 右键菜单项都要求恰好选中一个资产。** 选中两个或更多资产时该 section 是空的，而且没有任何提示说明原因。请右键单个资产。

| 被扩展的资产类 | 动态条目名 | 出现的内容 |
| :-- | :-- | :-- |
| `UMaterial` | `DreamShader.MaterialAssetActions` | 子菜单 **DreamShader** —— [Material 子菜单](#material-submenu) |
| `UMaterialFunction` | `DreamShader.VirtualFunctionAssetActions` | 子菜单 **DreamShader** —— [Material Function 子菜单](#material-function-submenu) |
| `UMaterialFunctionMaterialLayer` | `DreamShader.MaterialLayerAssetActions` | 同一个 Material Function 子菜单 |
| `UMaterialFunctionMaterialLayerBlend` | `DreamShader.MaterialLayerBlendAssetActions` | 同一个 Material Function 子菜单 |
| `UDreamShaderMaterialInstance` | `DreamShader.MaterialInstanceAssetActions` | 子菜单 **DreamShader** —— [材质实例子菜单](#material-instance-submenus) |
| `UMaterialInstanceConstant` | `DreamShader.MaterialInstanceConstantAssetActions` | 子菜单 **DreamShader** —— [材质实例子菜单](#material-instance-submenus)；不属于 `UDreamShaderMaterialInstance` |
| `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | `DreamShader.InstanceCreateActions` | 平铺条目 **Create DreamShader instance** |
| `UMaterial` · `UMaterialInstanceConstant` · `UDreamShaderMaterialInstance` | `DreamShader.ShowInBrowserActions` | 平铺条目 **Show in Material Content Browser** |

Unreal 里的菜单名是按精确类名索引的，所以同时给两个实例类的条目注册了两遍——一遍给标准类，一遍给 DreamShader 子类。

（自 2.1.0 起）`UDreamPassPipeline` 在这里没有 DreamShader 条目。它的 *Open Source*、*Revert to Source* 和 *Adopt Into Source* 在它的[详情面板](#pass-pipeline-details-panel)上，它的源文件在 [Material Content Browser](material-browser.md#pipelines) 里，反编译用 [`dsc decompile`](decompiler.md#pipelines-dsp)。

| 条目 | 标签 | 工具提示 | 图标 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.CreateInstance` | **创建 DreamShader 实例** *（自 1.5.0 起）* | "创建此材质的材质实例：若该材质由 DreamShader 生成，则为一个编译进实例的 .dsi 源文件；否则为一个普通实例资产。" | `ClassIcon.MaterialInstanceConstant` | 打开[创建材质实例](material-browser.md#create-material-instance)对话框。要求所选对象可转换为 `UMaterialInterface` |
| `DreamShader.ShowInMaterialBrowser` | **在材质内容浏览器中显示** | "在此资产上打开 DreamShader 材质内容浏览器：显示其源文件、编译状态、来源与继承关系。" | `ClassIcon.Material` | 以资产模式打开（或前置）[材质内容浏览器](material-browser.md)，范围限定为该资产的挂载点，并选中该资产 |

<a id="material-submenu"></a>

### 材质子菜单

`DreamShader.MaterialActions` —— 标签为 **DreamShader**，工具提示为 "DreamShader actions for this
Material."，图标为 `Icons.Settings`。仅当唯一选中的资产为 `UMaterial` 时构建。

第 1 节为 `DreamShader.ProvenanceActions`，标题为 **Generated Asset** —— 即下文的
[生成资产一节](#generated-asset-section)；对于并非由 DreamShader 生成的资产，该节为空。

第 2 节为 `DreamShader.DecompileActions`，标题为 **Decompiler**：

| 条目 | 标签 | 工具提示 | 图标 | 效果 | 参考 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportMaterialDSS` | **导出 .dss** *（自 2.0.0 起）* | "将此材质图表反编译为 2.0 的 DreamShader .dss 源文件。" | `Icons.Save` | 反编译到 `<SourceDirectory>/Decompiled/Materials/…​.dss` 并打开该文件 | [反编译器](decompiler.md) |
| `DreamShader.ExportMaterialLegacyDSM` | **导出旧版 .dsm** *（自 2.0.0 起；**导出 DSM** 自 1.3.5 起）* | "使用保留至 2.0.x 的 1.x 反编译器，将此材质图表反编译为 1.x 的 .dsm 源文件。" | `Icons.Save` | 反编译到 `<SourceDirectory>/Decompiled/Materials/…​.dsm` 并打开该文件 | [反编译器](decompiler.md) |

<a id="material-function-submenu"></a>

### 材质函数子菜单

`DreamShader.MaterialFunctionActions` —— 标签为 **DreamShader**，工具提示为 "DreamShader actions for this
Material Function."，图标为 `Icons.Settings`。同一个子菜单也服务于材质层和层混合。

第 1 节为 `DreamShader.ProvenanceActions`，标题为 **Generated Asset** —— 即下文的
[生成资产一节](#generated-asset-section)。

第 2 节为 `DreamShader.DecompileActions`，标题为 **Decompiler**：

| 条目 | 标签 | 工具提示 | 图标 | 效果 | 参考 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `DreamShader.ExportFunctionDSS` | **导出 .dss** *（自 2.0.0 起）* | "将此材质函数图表反编译为 2.0 的 DreamShader .dss 源文件。" | `Icons.Save` | 反编译到 `<SourceDirectory>/Decompiled/{Functions,Layers,LayerBlends}/…​.dss` 并打开该文件 | [反编译器](decompiler.md) |
| `DreamShader.ExportFunctionLegacyDSF` | **导出旧版 .dsf** *（自 2.0.0 起；**导出 DSF** 自 1.3.5 起）* | "使用保留至 2.0.x 的 1.x 反编译器，将此材质函数图表反编译为 1.x 的 .dsf 源文件。" | `Icons.Save` | 反编译到 `<SourceDirectory>/Decompiled/{Functions,Layers,LayerBlends}/…​.dsf` 并打开该文件 | [反编译器](decompiler.md) |

第 3 节为 `DreamShader.VirtualFunctionActions`，标题为 **VirtualFunction** *（自 1.2.1 起）*。其
内容取决于是否已有 `VirtualFunction` 声明引用此资产 ——
完整表格见 [VirtualFunction 工具](virtual-function-tools.md#context-menu)。

| 声明是否存在？ | 条目 |
| :-- | :-- |
| 是 | **OpenVirtualFunction**、**Copy Virtual Function Reference** |
| 否 | **CopyVirtualFunction**、**CreateVirtualFunction**、**Copy extern Prototype (2.0)**、**Create extern Prototype (2.0)**、**CopyVirtualFunctionCall** |

两个 *extern Prototype* 条目 *（自 2.0.0 起）* 是 `VirtualFunction`
块的 `.dss` 对应物：函数的 `extern` 原型及其 `/// @asset` 行，复制到剪贴板，或写入
一个新的 `.dsh`，以供 `.dss` 源文件 `#include`。

<a id="material-instance-submenus"></a>

### 材质实例子菜单

一个 `UDreamShaderMaterialInstance` —— 即 ThinCustom `Shader` 的产物 —— 会得到子菜单
`DreamShader.MaterialInstanceActions`，标签为 **DreamShader**，工具提示为 "DreamShader actions for this
generated material."，仅含[生成资产一节](#generated-asset-section)。

任何其他 `UMaterialInstanceConstant` —— 即 [`.dsi`](../language-v2/instances.md) 构建的，或
手工制作的实例 —— 会得到子菜单 `DreamShader.MaterialInstanceConstantActions`，标签为
**DreamShader**，工具提示为 "DreamShader actions for this material instance."，图标为 `Icons.Settings`：

| 节 | 条目 | 标签 | 效果 |
| :-- | :-- | :-- | :-- |
| **Generated Asset** | | | [生成资产一节](#generated-asset-section)；对于手工制作的实例为空 |
| **Decompiler** | `DreamShader.ExportMaterialInstanceDSI` | **导出 .dsi** *（自 2.0.0 起）* | 反编译该实例 —— 其父材质、其实例设置，以及与父材质不同的每个参数 —— 到 `<SourceDirectory>/Decompiled/Instances/…​.dsi` 并打开该文件 |

<a id="generated-asset-section"></a>

### 生成资产一节

上述每个子菜单的第 `DreamShader.ProvenanceActions` 节。除非该资产带有
DreamShader 的源元数据，否则该节为空，因此手工编写的资产不会显示其中任何内容。

| 条目 | 标签 | 效果 | 参考 |
| :-- | :-- | :-- | :-- |
| `DreamShader.RevertToSource` | **Revert to Source**，或者当资产为 `Diverged` 时显示 **Revert to Source (discards your edits)** | 从其源文件重建资产，丢弃每一处手工编辑；源文件不会被修改 | [分歧](../generation/divergence.md#revert-to-source) |
| `DreamShader.AdoptIntoSource` | **Adopt Into Source** | 用资产当前内容重写源文件；先前的源文件会备份在它旁边 | [分歧](../generation/divergence.md#adopt-into-source) |
| `DreamShader.DetachFromDreamShader` | **Detach From DreamShader** | 保持资产原样，并让 DreamShader 不再重建它 | [分歧](../generation/divergence.md#detach-from-dreamshader) |
| `DreamShader.AdoptTweaksIntoSourceDefaults` | **Adopt Tweaks as Source Defaults** *（自 2.0.0 起）* | 仅适用于 `Tweaked` 的 ThinCustom 实例。将其参数覆盖写入 `.dss`，作为其 `uniform` 的默认值，然后从实例中清除它们；源文件会先备份。拒绝声明的 `bool`/`int`/`uint` 类型无法表示的标量值（`DSH9109`）；请显式更改源类型，或提取一个 `.dsi`。除非源文件是可写根目录下的 `.dss`，否则禁用，并在其工具提示中说明原因 | [分歧](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance) |
| `DreamShader.ExtractTweaksToInstanceSource` | **Extract Tweaks to .dsi** *（自 2.0.0 起）* | 仅适用于 `Tweaked` 的 ThinCustom 实例。将其参数覆盖写入一个新的 `.dsi`，其父级为此材质，编译它，并从实例中清除这些覆盖 | [材质实例](../language-v2/instances.md) |

<a id="material-editor-toolbar"></a>

## 材质编辑器工具栏

`DreamShader.MaterialEditorToolbarActions`，是
`AssetEditor.MaterialEditor.ToolBar` 第 `DreamShader` 节中的一个动态条目 *（自 1.2.1 起）*。

| 步骤 | 行为 |
| :-- | :-- |
| 1 | 要求存在有效的 `UMaterialEditorMenuContext` 和一个活跃的 `IMaterialEditor` |
| 2 | 扫描当前正在编辑的对象，停在**第一个** `UMaterial`，若没有则停在第一个 `UMaterialFunction` |
| 3 | 对于 `UMaterial`：组合按钮 `DreamShader.MaterialToolbarMenu`，标签为 **DreamShader**，工具提示为 "DreamShader actions for this Material."，图标为 `Icons.Settings`，内容 = [材质子菜单](#material-submenu) |
| 4 | 对于 `UMaterialFunction`：组合按钮 `DreamShader.MaterialFunctionToolbarMenu`，标签为 **DreamShader**，工具提示为 "DreamShader actions for this Material Function."，图标为 `Icons.Settings`，内容 = [材质函数子菜单](#material-function-submenu) |
| 5 | 两者都未找到 ⇒ 不向工具栏添加任何内容 |

<a id="pass-pipeline-details-panel"></a>

## Pass 管线详情面板

*(自 2.1.0 起)* `UDreamPassPipeline` —— [`.dsp`](../language-v2/passes.md) 编译成的资产 —— 的详情面板，在管线自身属性之上有三个分类，按一帧运行它的方式读取该资产。属性保留，也保持可编辑：这个面板就是调校管线的地方，而 *Adopt Into Source* 会把调校结果写回它的 `.dsp`。

| | |
| :-- | :-- |
| 注册者 | 模块启动时的 `DreamShaderEditor`：通过 PropertyEditor 模块对类 `DreamPassPipeline` 的详情自定义 |
| 存在 | 在每个编辑器进程中，**无论是否**有 `-NoDreamShaderEditorBridge`，因为它只读取资产；从不在 commandlet 中 |
| 显示 | 对选中的一个管线：打开资产 —— 双击，或在 [Material Content Browser](material-browser.md#pipelines) 中 *Open* —— 或它的任何详情视图。选中多个时保留默认布局，其各行会编辑它们全部 |
| 引擎 | 资产类型所构建的每个引擎。UE 5.8 以下时概览会多一行：*Custom Pass 在 Unreal Engine 5.8 及更高版本上运行。此引擎会加载并保存管线，但不运行它的任何部分。* |

<a id="pipeline-overview"></a>

### 管线概览

| 行 | 显示 |
| :-- | :-- |
| **Source** | 管线构建自的 `.dsp`，绝对路径，由它的 `DreamShader.SourceFile` 戳记解析而来（没有戳记时为 *not built from a .dsp*），其下是管线的[来源](../generation/divergence.md#states)，每次更改后重新计算。三个按钮：**Open Source** 在你偏好的编辑器中打开 `.dsp`；**Revert to Source** 从它重建管线，丢弃在此所做的每项编辑，且不修改该文件；**Adopt Into Source** 把编辑逐值写回 `.dsp` —— 见[分歧](../generation/divergence.md#a-custom-pass-pipeline)。当源随插件发布或未找到时，Adopt 会被禁用，原因在其 tooltip 中 |
| **Runs** | `Order {N}, in views {Views}, requiring {Requires}: {N} pass(es), {N} buffer(s), {N} parameter(s).` |
| **Problems** | 仅当 `UDreamPassPipeline::Validate` 失败时 —— 在一次破坏运行时所依赖规则的手工编辑之后 —— 每个问题一行。运行时会跳过每个失败的 pass，并给出警告 |

<a id="passes-in-frame-order"></a>

### 帧顺序中的 Pass

管线使用的每个注入点一个分组，按一帧到达它们的顺序排列，标题为 `<injection point> (<number of passes>)`，其中的 pass 按声明顺序 —— 即它们运行的顺序。同一注入点上另一管线的 pass 按 `Order` 在这些之前或之后运行。每个 pass：

| 行 | |
| :-- | :-- |
| 名称 | 加粗；其 tooltip 是该 pass 的 `/// @desc` |
| 设置 | 种类及其键，按 `.dsp` 的写法，例如 `compute    Shader = "BoxBlur.usf"    Entry = BlurCS    Threads = uint3(8, 8, 1)    Dispatch = Blurred` |
| 绑定 | `read …    write …    param …`，按该 pass 自身的顺序 |
| 槽位 | 仅 HLSL pass：`compute shader slot C03` 或 `pixel shader slot P01`。橙色：*尚无 compute shader 槽位：编译源文件以给它分配一个*，或 *……，其快照缺失：编译源文件*。红色：此构建没有的槽位编号，并附上它拥有的数量 |
| **Open `<file>.usf`** | 着色器路径可解析到文件的 HLSL pass：在你偏好的编辑器中打开它 |
| **Open `<file>.dsp` at line `<n>`** | 代码在其 `.dsp` 中的 HLSL pass：在 `.dsp` 中打开该 pass 的 `hlsl` 块，或打开其在文件块中的条目 |
| *运行时会跳过此 pass：见上方 Problems。* | 验证失败的 pass |

<a id="buffers"></a>

### Buffer

每个声明的 buffer 一行，其 tooltip 是该 buffer 的 `/// @desc`：格式及键，按 `.dsp` 的写法（`R8    Resolution = Render    Scale = 0.5    Clear = 0    Export = true`）。导出的 buffer 会附加一个链接 **exported to `<render target>`**，在 Content Browser 中显示该渲染目标 —— 或者，在首次编译之前，*已导出，但其渲染目标尚不存在：编译源文件*。

<a id="edits"></a>

### 编辑

这些行实时读取资产，所以在下方属性中编辑的值会立刻显示在上方。添加、删除或重命名 pass 或 buffer，把 pass 移到另一个注入点，以及更改槽位、着色器路径或导出，都会重建面板。编辑会在下一帧生效，并使管线与其 `.dsp` 构建出的内容[分歧](../generation/divergence.md#a-custom-pass-pipeline)：在它被还原或采纳之前，对已更改的 `.dsp` 的重建会拒绝覆盖它。

<a id="notifications"></a>

## 通知

本页上唯一由 DreamShader 自行发起、而非注册后等待点击的表面。在没有 Slate 应用或无人阅读通知的地方，通知会被抑制 —— commandlet、cook、专用服务器，以及任何带 `-unattended` 的运行。

| 通知 | 触发时机 | 按钮 | 参考 |
| :-- | :-- | :-- | :-- |
| **分歧拒绝** *(自 1.9.0 起)* | 因为资产被手工编辑而拒绝重建（`DSH8115`） | **Revert to Source** · **Adopt Into Source** · **Detach** · **Show Ephemeral Materials**（仅隐藏的 Ephemeral 实例） · **Dismiss** | [分歧](../generation/divergence.md#what-you-see-since-190) |
| **分歧摘要** *(自 1.9.0 起)* | 一轮重建中有超过五个资产分歧 | **Open Material Browser** · **Dismiss** | [分歧](../generation/divergence.md#how-often-it-appears) |
| **被遮蔽的 Ephemeral 材质** | 编译器后端更改，而持久化资产遮蔽了 Ephemeral 结果 | *（无）* | [见下](#notes) |
| **Ephemeral 可见性已切换** | *Show Ephemeral Materials* 被翻转 | *（无）* | [见下](#show-ephemeral-materials) |
| **导出结果** | 从上下文菜单发起的 *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* 完成或失败：`Exported '{File}'.`、`Exported '{File}' but could not open it.` 或 `DreamShader failed to export '{Asset}': {Reason}` | *（无）* | [反编译器](decompiler.md) |
| **来源操作结果** | Revert、Adopt 或 Detach 完成或失败 | *（无）* | [分歧](../generation/divergence.md#the-three-ways-out) |

两个分歧通知**不会**超时，因为它们要提问；本页其他每个通知都是四秒的 toast，报告已经完成的事。关闭编辑器会撤销任何仍在等待答复的分歧通知。

<a id="command-semantics"></a>

## 命令语义

本页每条引用的消息中的运行时替换都以 `{Placeholder}` 表示。

<a id="recompile-dsm"></a>

### Recompile DSM

先询问 —— 一个 Yes/No 对话框，"Recompile every DreamShader .dsm and .dsf source file? This rebuilds all generated materials and can take a while on a large tree."。选择 Yes 时，重建材质依赖图，然后把每个源根的 `.dsm`、`.dsf`、`.dss`、`.dsi` 和 `.dsp` 以当前时间强制戳入待处理队列。根目录 `Packages` 文件夹下的 `.dsm` 会被排除；`.dsh` 头文件从不入队，因为它们不构建任何东西。这些文件随后由去抖 ticker 编译，正如它们被保存过一样。

日志：`DreamShader queued a full .dsm/.dsf recompile scan.`

<a id="clean-generated-shaders"></a>

### Clean Generated Shaders

删除生成的 `.ush` include 并入队一次完整扫描。

| 行为 | 细节 |
| :-- | :-- |
| 安全防护 | 当 `GeneratedShaderDirectory` 不在工程的 `Intermediate/` 目录内时拒绝运行 |
| 删除范围 | 仅 `*.ush` 文件，递归，逐个删除。目录本身从不会被移除 |
| 标志 | 容忍缺失的文件；只读文件照样删除 |

| 消息 | 严重级别 | 原因 |
| :-- | :-- | :-- |
| `DreamShader refused to clean generated shaders: '{Directory}' is not inside the project Intermediate directory. Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning.` | Warning | 防护被触发 |
| `DreamShader deleted {Count} generated shader file(s) from '{Directory}'.` | Display | 成功 |
| `DreamShader cleaned generated shader includes and queued a full .dsm/.dsf recompile scan.` | Display | 队列被戳记之后 |

<a id="make-ephemeral"></a>

### Make Ephemeral

查找并删除已物化的 ThinCustom 产物 —— 即在磁盘上有 package 的那些 —— 因为已保存的产物会遮蔽由同一源生成的自定义（Ephemeral）产物。

| 方面 | 值 |
| :-- | :-- |
| 搜索范围 | 资产注册表、package 路径 `/Game`、递归路径、递归类 |
| 类 | 仅 `UDreamShaderMaterialInstance` *（自 2.0.0 起）*。Graph 后端的 `UMaterial` 或 `UMaterialFunction` 没有可回退的自定义状态，所以删除它只会删除产物 |
| 门槛 1 | package 必须存在于磁盘上 |
| 门槛 2 | package 必须带有非空的 `DreamShader.SourceFile` 元数据 |
| 删除 | 标准编辑器删除流程，带确认对话框和引用检查 |
| 删除之后 | 每个源文件立即在内存中重新生成，因此引用无需重启编辑器即可解析 |

因为筛选基于来源，手工编写的材质永远不会被触及，而源文件已被删除或重命名的孤儿产物仍然符合条件。

| Toast | 成功状态 | 原因 |
| :-- | :-- | :-- |
| `No Materialized DreamShader ThinCustom products found.` | 成功 | 无匹配项 |
| `Made {Deleted} of {Total} Materialized product(s) Ephemeral.` | `{Deleted}` > 0 时成功 | 删除流程之后 |

日志：`DreamShader made {Deleted} of {Total} Materialized product(s) Ephemeral.`

<a id="show-ephemeral-materials"></a>

### 显示自定义（Ephemeral）材质

翻转 `bShowEphemeralMaterials` 并将其直接写入工程的 `DefaultEngine.ini`。然后它遍历每个 package 新创建的活跃 `UDreamShaderMaterialInstance`，广播资产创建或资产移除，因此图块立即出现或消失，而不是等到下一次重新枚举。

| Toast | 条件 |
| :-- | :-- |
| `Showing {Count} Ephemeral material(s) in the Content Browser and asset pickers.` | 已开启 |
| `Hidden {Count} Ephemeral material(s) from the Content Browser and asset pickers.` | 已关闭 |

> [!WARNING]
> 当自定义（Ephemeral）材质显示时，它们也会出现在保存选择器中，而显式执行 *Save* 会物化其中一个。保存的副本随后会遮蔽自定义产物。用 *Make Ephemeral* 恢复。

[材质内容浏览器](material-browser.md#toolbar)中的 **View ▾ ▸ Show Ephemeral materials** 在同一全局设置上运行同一命令，包括 toast。

<a id="open-dream-shader-workspace-vscode"></a>

### 打开 Dream Shader 工作区（VSCode）

按此顺序运行：重新导出 `material-expressions.json`、`settings.json`、`substrate-builtins.json`、`preprocessor-defines.json` 和内建节点目录（`dreamshader-builtin-catalog.json`）；重写 `DShader/DreamShader.code-workspace`；然后通过三步回退链启动它 —— VSCode、操作系统默认编辑器、记事本。发现顺序和确切文件内容见[工作区](workspace.md)。

| Toast | 条件 |
| :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | 工作区文件无法写入 |
| `Opened DreamShader workspace in VSCode: {Path}` | VSCode 已启动 |
| `Opened DreamShader workspace: {Path}` | 操作系统默认编辑器已启动 |
| `Opened DreamShader workspace in Notepad: {Path}` | 记事本已启动 |
| `DreamShader could not open workspace: {Path}` | 所有启动器都失败 |

<a id="disabling-the-integration"></a>

## 禁用集成

```powershell
UnrealEditor.exe "<Project>.uproject" -NoDreamShaderEditorBridge
```

该开关按裸命令行参数解析，因此 `-NoDreamShaderEditorBridge` 是唯一被接受的拼写。存在时，`StartupModule` 在创建任何内容之前返回。

| 已禁用 | 仍然活跃 |
| :-- | :-- |
| 编辑器桥 —— 文件监视器和保存时自动编译、去抖队列、诊断存储及其全部三个接收器、`bridge.db`、请求文件轮询器、VirtualFunction 启动同步、引擎初始化后所有源的 Ephemeral 生成、设置监视器 | 运行时模块 —— `DreamShaderLang`（两个前端、绑定器、IR）、`DreamShader`（设置对象、`UDreamShaderMaterialInstance`、编译接口）和 `DreamShaderPass` —— 以及编译接口所触及的 `DreamShaderCompiler` 编辑器模块 |
| 端口 `17864` 上的预览 WebSocket 服务器，以及整个预览渲染器 | `-run=DreamShader` [commandlet](commandlet.md)，它从不使用桥 |
| 本页上的每个菜单、工具栏和上下文菜单条目，以及[节点上下文菜单](#material-editor-node-context-menu) | 已生成并保存到磁盘的资产 |
| Custom Pass shader 文件的 `.usf` / `.ush` 监视 | [pass 管线详情面板](#pass-pipeline-details-panel) *（自 2.1.0 起）* |
| 材质内容浏览器标签注册 —— 该标签完全无法打开 | |
| 导出的 manifest 和内建目录，以及 `DreamShader.code-workspace` 重新生成 | |
| 资产重命名同步服务 | |

commandlet 运行通过不同途径达到相同状态：每当 `IsRunningCommandlet()` 为 true 时模块提前返回，仅安装 cook 时资产物化钩子，且仅当 `-run=` 包含 `Cook` 且 `-cookworker` 不存在时。

<a id="example"></a>

## 示例

在集成关闭的情况下启动编辑器，然后以无头方式做同样的工作：

```powershell
# No menus, no watcher, no bridge.
& "$Engine\Binaries\Win64\UnrealEditor.exe" "I:\Project\Project.uproject" -NoDreamShaderEditorBridge

# The commandlet is unaffected by the switch.
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "I:\Project\Project.uproject" `
    -run=DreamShader compile -All -Force -unattended -nopause -nosplash -stdout -log
```

<a id="notes"></a>

## 说明

- VirtualFunction 条目标签中有四个是 CamelCase 且没有空格 —— `OpenVirtualFunction`、`CopyVirtualFunction`、`CreateVirtualFunction`、`CopyVirtualFunctionCall` —— 而第五个是 **Copy Virtual Function Reference**。这种不一致存在于已发布的标签中。
- 在两套 VirtualFunction 条目之间做决定会从磁盘重新读取并重新词法分析每个工程源文件，且发生在每次右键点击 Material Function 资产时。在有很多源文件的工程上，菜单打开会明显更慢。见 [VirtualFunction 工具](virtual-function-tools.md#context-menu)。
- *Show Ephemeral Materials* 开关的勾选状态实时从设置对象读取，因此在项目设置中更改该值会更新菜单勾选标记。
- 在项目设置中更改 *Default Compiler Backend*、三个枚举映射表之一或 *Preprocessor Defines* 会重新生成每个源文件 —— 不是强制的：它们中每一个都是[构建键](../generation/caching.md)的一部分，所以恰好是它影响的源文件无法通过跳过检查 —— 并在已物化的 ThinCustom 产物会遮蔽结果时引发失败状态 toast：`{Count} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials. Run Tools > DreamShader > Make Ephemeral to remove them.` 没有其他设置属性会触发反应。

<a id="material-editor-node-context-menu"></a>

## 材质编辑器节点上下文菜单

*（自 2.0.0 起）* 在材质编辑器图表中右键点击 DreamShader 生成的节点会在节点自身上下文菜单的 `DreamShader` 区段中添加一个 **DreamShader** 子菜单。

| | |
| :-- | :-- |
| 注册者 | `DreamShaderEditor` 在模块启动时，通过 `UToolMenus::RegisterStartupCallback` |
| ToolMenu 所有者 | `DreamShaderSourceNavigation` |
| 扩展的菜单 | `GraphEditor.GraphNodeContextMenu.MaterialGraphNode` 以及 `_Custom`、`_Composite`、`_Operator`、`_PinBase`、`_Knot` 变体 |
| 条目条件 | 节点的表达式 GUID 出现在某个已加载资产的 `DreamShader.SourceSpans` 元数据中 |

> [!NOTE]
> 没有记录的源位置的节点得到**完全没有菜单** —— 不是禁用的菜单。工程中每个手工编写材质的每个节点都处于该状态，给它们全都显示一个变灰的条目只会是噪音。最后一次由 1.x 生成器构建的、早于 2.0.0 的资产也是如此，它没有写入 span 表；现在每个源（包括 `.dsm` 和 `.dsf`）都由 2.0 管线构建，所以强制重建（`compile -Force`）会为其节点提供条目。

| 条目 | 标签 | 效果 |
| :-- | :-- | :-- |
| `DreamShader.OpenSourceLine` | **Open Source Line** | 在节点所在行和列打开源文件，通过与 *Open Dream Shader Workspace* 相同的三步启动器链 —— VSCode、操作系统默认编辑器、记事本 |
| `DreamShader.OpenCallSite` | **Open Call Site** | 仅当节点来自**内联辅助函数**时存在：打开调用该辅助函数的行，这通常位于不同的函数中，且常常在不同的文件中 |

失败时是一个四秒的失败 toast 加上一条带 `DSH9058` 的 `LogDreamShader` 错误 —— 要么源文件已不存在，要么没有启动器可以启动。

<a id="reveal-node--the-reverse-direction"></a>

## `reveal-node` — 反方向

*(自 2.0.0 起)* [bridge](bridge.md) 请求中方向相反的那一个：从一行源码到材质编辑器中的
节点。它就是 VSCode 扩展的 *Reveal in Material Editor* 命令所发送的内容。

| 字段 | 类型 | 备注 |
| :-- | :-- | :-- |
| `action` | string | `"reveal-node"` |
| `file` | string | 必填，非空；绝对路径或相对于工程的路径均可，两种都会归一化 |
| `line` | number | 必填，从 1 开始 |

同步处理：它会找到带有该源文件标记的资产，挑选跨度落在该行上的表达式，为拥有它们的资产打开材质编辑器，
并选中第一个。**起始**于该行的节点优先于仅仅是从该处某个调用内联而来的节点。

响应是普通的 `Responses/<requestId>.json` 信封，外加四个字段：

| 字段 | 类型 | 备注 |
| :-- | :-- | :-- |
| `version` | number | 恒为 `1` —— 新增字段的载荷版本，与 `protocol` 相互独立 |
| `assetPath` | string | 其图表被打开的资产；对于 ThinCustom 产物，这是隐藏的 **base material**，也就是图表所在之处 |
| `instanceAssetPath` | string | 仅对 ThinCustom 产物出现：base 所归属的那个实例 |
| `expressions` | array | 每个匹配表达式的 GUID，带连字符（`FGuid` `DigitsWithHyphens`，即资产元数据使用的写法），选中的那个排在最前 |

失败会以 `ok: false` 返回，原因写在 `diagnostics`（`stage: "navigate"`）中，编码为
`DSH9050`–`DSH9057`。其中两个值得了解：`DSH9051` 表示来自该源文件的资产没有加载到
此编辑器中 —— 请先编译该文件 —— 而 `DSH9052` 表示资产存在，但构建于
节点导航之前，因此它们不携带跨度表，需要一次 `-Force` 重建。

<a id="see-also"></a>

## 另见

- [工具](index.md) — 编辑器工具入口
- [材质内容浏览器](material-browser.md) — 工具菜单打开的标签页
- [反编译器](decompiler.md) — *Export .dss*、*Export .dsi* 和两个 *Export Legacy* 条目产出什么
- [VirtualFunction 工具](virtual-function-tools.md) — 条件性的 VirtualFunction 区块
- [工作区](workspace.md) — 工作区文件、VSCode 发现和启动回退链
- [Bridge](bridge.md) — 该开关禁用的文件与 WebSocket 接口
- [命令行](commandlet.md) — 无头入口点
- [项目设置](../settings/project.md) — 这些命令所读取或写入的每一项设置
- [临时材质](../generation/ephemeral.md) — 为什么已持久化的资产会遮蔽生成的资产
- [生成的 HLSL](../generation/generated-hlsl.md) — *Clean Generated Shaders* 会删除什么
- [分歧](../generation/divergence.md#a-custom-pass-pipeline) — 对于在细节面板中编辑过的管线，使用 Revert 和 Adopt
