---
title: "材质内容浏览器"
description: "材质内容浏览器"
---

<a id="material-content-browser"></a>

# 材质内容浏览器

> [DreamShader](../index.md) » [工具](index.md) » **材质内容浏览器**

一个可停靠的编辑器标签页，涵盖项目中的每个 DreamShader 源文件和每个材质：每个源文件编译成什么、该资产是否为最新、是否有人手动编辑过它、它导入了什么以及什么继承了它——以及由每个答案所引出的操作。

| | |
| :-- | :-- |
| 类型 | 可停靠标签页，由 `DreamShaderEditor` 模块注册 |
| 标签页 ID | `DreamShaderMaterialBrowser` |
| 显示名称 | **材质内容浏览器** |
| 模式 | **源文件**（源文件树：`.dss` / `.dsi` / `.dsp` 以及 1.x 的 `.dsm` / `.dsf` / `.dsh`）· **资产**（项目的材质） |
| 起始版本 | `1.5.0` |

<a id="opening-it"></a>

## 打开它

| 途径 | 路径 |
| :-- | :-- |
| 工具菜单 | *工具 ▸ DreamShader ▸ 材质内容浏览器* |
| 窗口菜单 | *窗口 ▸ 工具 ▸ 材质内容浏览器* |
| 内容浏览器 | 右键单击一个材质或实例 ▸ **在材质内容浏览器中显示**——在该资产上打开标签页 |

该标签页仅在编辑器桥接启动时注册。使用 `-NoDreamShaderEditorBridge` 启动编辑器会移除所有途径——请参阅[编辑器集成](editor-integration.md#disabling-the-integration)。

<a id="layout"></a>

## 布局

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ [⟳ Refresh] [▶ Compile ▾] [+ New ▾] │ search…          │ [View ▾] [VSCode] [⚙] │
├──────────────┬──────────────────────────────────┬──────────────────────────┤
│ NAVIGATION   │ LIST                              │ INSPECTOR                │
│ ▾ Sources    │ Sources mode: columns or tiles    │ live preview · mesh      │
│   ▾ Project  │ Assets mode: the asset picker     │ name · status · storage  │
│     Materials│                                   │ actions                  │
│   ▸ PluginA  │                                   │ Root / Source / Asset /  │
│ ▾ Content    │                                   │   Domain / Blend / …     │
│   ▸ /Game    │                                   │ Diagnostics (N)          │
│ ──────────── │                                   │ Provenance               │
│ Quick filters│                                   │ Imports / Used by        │
│ ☐ Errors …   │                                   │ Inheritance · Children   │
├──────────────┴──────────────────────────────────┴──────────────────────────┤
│ 14 sources · 9 ok · 3 stale · 1 errors · 1 edited by hand · 5 Ephemeral │ bridge: idle │
└────────────────────────────────────────────────────────────────────────────┘
```

水平分隔条中的三个窗格，上方是工具栏，下方是状态栏。窗格比例、模式、导航范围、过滤器、排序、列表/平铺选择、预览网格和上次选择均按用户和按项目保存（`UDreamShaderBrowserUserSettings`、`EditorPerProjectUserSettings`），并在下次打开标签页时恢复。

<a id="toolbar"></a>

## 工具栏

| 控件 | 标签 | 效果 |
| :-- | :-- | :-- |
| 按钮 | **刷新**（`F5`） | 重新扫描源文件根目录，重建依赖关系图，重新计算每个状态 |
| 菜单 | **编译 ▾** | **编译**（`Ctrl+B`）选择项 · **编译过期项**——每个过期、从未编译或失败的源文件 · **全部编译**（`Ctrl+Shift+B`）——每个列出的源文件，但 `.dsh` 除外（`.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf`），在进度对话框后执行。只有**编译**会指名其源文件：它还会构建它们的 [NoGeneration](../generation/no-generation.md) 资产，而另外两个则保持这些资产不变 *（2.1.2 之后）* |
| 菜单 | **新建 ▾** | *DreamShaderLang 2.0：* **材质 (.dss)** · **材质函数 (.dss)** · **实例 (.dsi)**——*Custom Pass 管线 (.dsp)：* **全屏后处理链** · **网格遮罩链** · **计算链**——*1.x 块：* **材质 (.dsm)** · **材质函数 (.dsf)** · **头文件 (.dsh)**——请参阅[新建源文件](#new-source) |
| 搜索框 | （`Ctrl+F` 将其聚焦） | 对文件名、根名称、源文件路径、资产路径和状态详情进行不区分大小写的子字符串匹配——因此错误消息可搜索，插件的名称可过滤到它随附的所有内容。在资产模式下，它匹配资产名称 |
| 菜单 | **视图 ▾** | **平铺**——源文件列表显示为缩略图平铺 · **排序依据** 名称 / 状态 / 根 / 资产路径，**升序** · **显示临时材质**——全局项目设置，请参阅[编辑器集成](editor-integration.md#show-ephemeral-materials) |
| 按钮 | *（VSCode 图标）* | 写入并打开 DreamShader 工作区——与 *工具 ▸ DreamShader ▸ 打开 Dream Shader 工作区* 完全相同 |
| 按钮 | *（齿轮）* | 打开 *项目设置 ▸ DreamPlugin ▸ DreamShader* |

从此标签页启动的每次编译都通过编辑器桥接进行，因此其结果会像保存时编译一样到达诊断存储、`diagnostics.json` 和 VSCode 扩展，并且成功会清除该文件之前的错误。

<a id="navigation"></a>

## 导航

一棵包含两个组的树。选择节点会**限定列表范围并切换模式**。

| 节点 | 范围 |
| :-- | :-- |
| **源文件** | 每个根目录下的每个源文件（源文件模式） |
| **源文件 ▸ *根*** | 一个根目录——*项目*，或随附 `DShader` 文件夹的插件——以及其下的源文件数量 |
| **源文件 ▸ *根* ▸ *文件夹*** | 该文件夹及其下所有内容 |
| **源文件 ▸ 不由 DreamShader 管理** | 内容根目录下没有由任何已扫描源文件生成的每个材质和实例——手动编写的，以及其源文件已消失的孤立项。请参阅[未管理的材质](#unmanaged-materials) |
| **内容** | `/Game`（资产模式） |
| **内容 ▸ /Game**、**内容 ▸ /*插件*** | 一个挂载点——`/Game`，以及每个随附 DreamShader 源文件的插件的内容根目录，其生成的资产就落在那里 |
| **内容 ▸ … ▸ *文件夹*** | 该内容路径及其下所有内容；文件夹在展开时从资产注册表读取 |

<a id="quick-filters"></a>

### 快速过滤器

树下的复选框。四个状态过滤器之间是**或**关系：勾选*错误*和*过期*会显示二者之一存在的条目。

| 过滤器 | 保留 |
| :-- | :-- |
| **错误** | 上次编译失败，或无法读取或解析的源文件 |
| **过期** | 自其资产生成以来发生更改的源文件 |
| **手动编辑** | 内容不再与上次生成匹配的已生成资产（[偏差](../generation/divergence.md)） |
| **临时** | 尚未写入磁盘的材质 |
| **隐藏函数** | 丢弃每个 `.dsf` 和 `.dsh` |
| **隐藏管线** *（自 2.1.0 起）* | 丢弃每个 `.dsp`——Custom Pass 管线——及其管线 |
| **隐藏未管理项** | 丢弃 DreamShader 不管理的材质 |

状态栏计数与这些过滤器相同：单击某个计数会单独启用它，再次单击会将其关闭，而单击 *ok* 计数会清除全部四个。

<a id="sources-mode"></a>

## 源文件模式

每个通过范围、过滤器和搜索的已扫描源文件，以带可排序列的**列表**形式或——*视图 ▸ 平铺*——缩略图网格形式显示。支持多选；右键单击打开[上下文菜单](#context-menu)；双击打开材质（若有），否则打开源文件。

| 列 | 内容 |
| :-- | :-- |
| *（字形）* | 状态字形，工具提示 = 状态详情 |
| **名称** | 文件名，工具提示 = 绝对路径；`.dsp` 行在其前面带有[管线徽章](#pipelines) |
| **根** | *项目*，或所属插件 |
| **状态** | 状态标签；对于 `.dsf` / `.dsh`：`function · used by N material(s)`；对于 `.dsp`：`pass pipeline · <status label>` |
| **资产** | 源文件编译成的包路径 |

<a id="listed-files"></a>

### 列出的文件

**`.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf` 和 `.dsh`** 在每个源文件根目录下递归找到，排除每个根目录的 `Packages` 文件夹——此外，在 *源文件* 标题和 *不由 DreamShader 管理* 节点下，还有[未管理的材质](#unmanaged-materials)。选择在刷新后仍然保留：条目按路径重新选择。

<a id="pipelines"></a>

### 管线

*（自 2.1.0 起）* `.dsp` 像材质源文件一样列出——其状态是其管线的状态，由相同的规则解析和判断——但其资产是 `UDreamPassPipeline`，这是此标签页中唯一一种不是材质的产品，而各行也如此说明：

| | |
| :-- | :-- |
| 徽章 | 名称前的后处理体积类图标，工具提示为 "A Custom Pass pipeline source (.dsp)."。在平铺视图中，尚未编译的管线会显示后处理体积缩略图，而材质则显示其状态图标 |
| 状态 | `pass pipeline · up to date`、`pass pipeline · stale`、`pass pipeline · not compiled`、… |
| 双击、**打开** | 在其资产编辑器中打开该管线，其[细节面板](editor-integration.md#pass-pipeline-details-panel)按注入点对其各通道进行分组，并显示其槽、缓冲区和渲染目标；首次编译前，源 |
| **创建实例** | 不提供：管线没有可供实例化的内容 |
| 预览 | 无；检查器的平铺显示 `pass pipeline` |
| 过滤 | 树下的 **隐藏管线** |

管线仅通过其 `.dsp` 列出：没有被任何已扫描源构建的 `UDreamPassPipeline` 不是
[非托管](#unmanaged-materials)行，其导出缓冲区的渲染目标也不是。

<a id="unmanaged-materials"></a>

### 非托管材质

内容根目录下（`/Game` 以及每个附带 DreamShader 源的插件的挂载点）的每个 `UMaterial` 和 `UMaterialInstanceConstant`，如果没有任何已扫描源解析到它，那么该标签页会覆盖整个项目，而不仅仅是 DreamShader 的角落。它们通过资产注册表找到，并且**绝不会为了列出而加载**——一个有一千个手工制作材质的项目只耗费一次注册表查询，而不是一千次加载；只有被选中时才会加载某一行，此时检查器会读取其真实来源（一个孤立的生成资产会显示为 `Unstamped` 或
`Generated`，手工制作的则保持 `Foreign`）并将其合并回该行。

| 列 | 对于非托管行 |
| :-- | :-- |
| *（图标）* | `◇` 灰色 |
| **名称** | 资产名称，工具提示 = 对象路径 |
| **根** | 挂载点 —— `/Game`、`/PluginName` |
| **状态** | `not managed by DreamShader`，或 `material instance · not managed by DreamShader` |
| **资产** | 包路径 |

状态栏计数 **未管理** 会跳转到该节点，而不是切换过滤器。精简后端的隐藏 `MB_DreamThinBase_*` 基类
从不会被列出。对其中的某一个执行 *Export .dss*（对于普通材质实例而言是 `.dsi`）
就是它进入 DreamShader 管理的方式。

<a id="status-values"></a>

### 状态值

| 状态 | 图标 | 标签 | 含义 |
| :-- | :-- | :-- | :-- |
| `UpToDate` | `●` 绿色 | **最新** | 生成的资产存在，且其标记的源哈希与当前源匹配 |
| `EphemeralUntracked` | `◐` 绿色 | **已编译，临时** | 资产存在于内存中，且不携带源哈希。只有由某个插件版本生成的资产——该版本对仅内存构建仅用路径进行标记——才会显示为此状态；当前构建在两种状态下都会标记哈希，因此这是一个旧版状态 |
| `Stale` | `●` 琥珀色 | **过期** | 资产存在，且其标记的哈希与当前源不同 |
| `NotCompiled` | `○` 灰色 | **未编译** | 解析后的对象路径上没有对象。详情：`No generated asset at {ObjectPath}` |
| `Error` | `▲` 红色 | **编译错误** | 编译失败，来自此标签页或依据桥接的诊断信息 |
| `Library` | `◆` 蓝色 | **函数 / 头文件** | 一个 `.dsf` 或 `.dsh` |
| `Unresolved` | `▲` 红色 | **未解析** | 无法确定源构建的内容：它有错误（详情是第一个，`<file>(<line>,<col>): DSHnnnn: <message>`），或者它没有声明任何材质、实例或导出函数 |

对于除 `.dsf` / `.dsh` 之外的每个源的状态计算 *（自 2.0.0 起）*：

| # | 步骤 | 失败 |
| :-- | :-- | :-- |
| 1 | 使用编译自身的前半部分——定义、按扩展名的前端、绑定器、IR 构建器、目标规则——解析源构建的产品，并取其材质的对象路径（否则取其实例，否则取其管线，否则取其第一个产品）及其构建键 | ⇒ `unresolved` |
| 2 | **不加载**地查找该对象 | ⇒ `not compiled` |
| 3 | 将资产标记的源文件和构建键与解析出的结果进行比较 | 匹配 ⇒ `up to date` |
| 4 | 没有标记键且资产不在磁盘上 | ⇒ `compiled, Ephemeral` |
| 5 | 否则 | ⇒ `stale` |

构建键涵盖编译所标记的内容——预处理后的文件及其包含的每个头文件、它们所读取的定义、一个 `.dsi` 的父级——因此当前资产会显示为 **最新**，而重新构建总会使过期的资产变为最新。1.x 从文本中解析 `Name=` / `Root=` 并剥离导入，并在内联它们的情况下对源进行哈希；该方案对 `.dss` 无法给出任何答案，已被移除。

然后叠加桥接的诊断信息，从其存储中实时读取：该文件的任何 `error` 记录都会使其变为 `compile error`，并以第一条错误记录作为详情
（`L{line}:{col} {message}`）。所有记录都保留在条目上供检查器使用。

> [!NOTE]
> 步骤 2 不会加载，因此磁盘上存在但本会话尚未加载的材质会
> 报告为 **未编译**，直到有东西加载它。

<a id="assets-mode"></a>

## 资产模式

引擎的资产选择器基于 `UMaterial` 和 `UMaterialInstanceConstant`（递归地，其中
包括 `UDreamShaderMaterialInstance`），范围限定为导航树的内容路径。有意保留为引擎控件：缩略图、拖拽到视口、列视图以及选择器自身的
设置都随之而来。

共享过滤器在这里同样适用，并且**不加载任何内容**即可得到答案：已扫描源的资产由其条目判定，而 *Ephemeral* 则从注册表的包标志中读取。DreamShader 从未生成过的
手工制作材质，除
*Ephemeral* 外不通过任何状态过滤器。

双击打开该资产。右键单击会给出相同的[上下文菜单](#context-menu)。

<a id="inspector"></a>

## 检查器

两种模式共用一个面板。它显示选择中存在的任何一半——**源**一半
（一个已扫描文件）和**资产**一半（项目中的一个材质）——通过资产的
`DreamShader.SourceFile` 标记连接起来。因此，在资产模式中选中的材质会显示其源的
编译状态和诊断信息；在源模式中选中的源会显示其资产的来源和
继承关系。

| 部分 | 内容 |
| :-- | :-- |
| **预览** | 通过插件的[预览渲染器](preview.md)对材质进行的实时渲染，224×224，每次编译时重新渲染。**左键拖拽可环绕旋转**；**网格**选择器可选择球体、平面、立方体、圆柱体或着色球，且会被记住。采用异步渲染，因此绝不会阻塞编辑器。仅在存在已编译的材质时显示 |
| **头部** | 名称 · 状态图标与标签 · `memory-only (not saved)` / `on disk` · 当资产在资产编辑器中打开时显示徽章（此时重建会被拒绝） |
| **操作** | **创建实例** · **编译** · **打开** · **实体化**（仅内存资产） · **打开源文件** |
| **信息行** | **基础** · **域** · **混合模式** · **根** · **源文件**（一个链接：在源文件模式下显示该文件） · **资产**（一个链接：在资产模式下显示该资产） · **存储** · **来源** |
| **诊断 (N)** | 桥接为该文件保存的每一条记录：`[DSHnnnn] L{line}:{col} message`，每条都是链接，可在编辑器中打开该文件并跳转到对应行——当该记录指向导入的头文件时，则跳转到导入的头文件。此选项卡自身固定的某个失败会单独显示其消息 |
| **来源** | 摘要状态及一行说明，然后是各选项：**还原到源文件** · **纳入源文件** · **分离**；或者，对于 DreamShader 从未生成的材质，**导出 .dss**（材质或材质函数）/ **导出 .dsi**（普通材质实例）*（自 2.0.0 起；之前为 **导出 DSM** / **导出 DSF**）*。参见[差异](../generation/divergence.md)。当源文件随插件一起提供时，*纳入*会被禁用并显示原因 |
| **导入 (N)** | 对于材质：它导入的每个头文件和函数，递归地——链接到源文件列表 |
| **被用于 (N)** | 对于头文件或函数：导入它的每个材质——链接 |
| **继承** | 父级链，根节点在前，每一行都是链接，可重新定位面板 |
| **子实例 (N)** | 父级为此的已加载实例，以及——来自资产注册表的引用者——本会话中无人加载的已保存实例，标记为*（未加载）*；点击其中一个即可加载它 |

对于 `.dsp` *（自 2.1.0 起）*，检查器没有预览，也没有**创建实例**。**打开**会在其[详细信息面板](editor-integration.md#pass-pipeline-details-panel)中打开管线，信息行以**管线**——`{N} pass(es), {N} buffer(s), {N} parameter(s); order {N}`——开头，取代基础、域和混合模式。**来源**部分提供与材质相同的三个选项：在详细信息面板中编辑的管线会被逐值[纳入](../generation/divergence.md#a-custom-pass-pipeline)其 `.dsp` 中。

**来源**行和部分所说明的摘要状态：

| 状态 | 显示 |
| :-- | :-- |
| `Generated` | *已生成（与上次构建匹配）* |
| `Tweaked` *（自 1.9.0 起）* | *已生成，实例上带有参数覆盖*——这些值是你的，重建会继续进行并[将它们放回](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance) |
| `Unstamped` | *已生成（无可比较的摘要）* |
| `Diverged` | *自上次构建以来被手动编辑*——以橙色显示，是唯一拒绝重建的状态 |
| `Foreign` | *非 DreamShader 生成* |

状态栏的**手动编辑**计数仅指 `Diverged`：被调整的实例没有问题，也不需要查找。

<a id="status-bar"></a>

## 状态栏

`{N} sources` · **正常**（最新，或已编译，Ephemeral）、**陈旧**、**错误**、**手动编辑**和 **Ephemeral** 的计数，每个都是一键筛选器 · **未管理**，可跳转到该节点 · 桥接的状态：`bridge: idle`、编译时的 `bridge: {action}`、同一项目上的第二个编辑器持有写锁时的 `bridge: idle (another editor owns writes)`，或 `bridge: off`。工具提示是桥接的最后一次结果。

<a id="context-menu"></a>

## 上下文菜单

在任一列表中右键单击。命令作用于所选内容；大多数取第一个选中的条目。

| 部分 | 条目 |
| :-- | :-- |
| **打开** | **打开材质**（`Enter`；对于 `.dsp`，则在其详细信息面板中打开管线） · **打开源文件**（`Ctrl+Enter`，若有错误则定位到第一个错误处） · **在内容浏览器中显示** |
| **构建** | **编译** · **创建实例**（不适用于 `.dsp`） · **实体化**（对仅内存资产显示；作用于所选内容中的每个仅内存条目） |
| **生成资产** | **还原到源文件**（作用于所选内容中的每个生成条目，逐个确认） · **纳入源文件** · **从 DreamShader 分离**——仅当资产带有 DreamShader 的标记时 |
| **反编译器** | **导出 .dss**——仅适用于手工编写的材质或函数（一个 `.dss`）或普通材质实例（一个 `.dsi`） |
| **复制** | **复制源路径** · **复制资产路径** |

<a id="following-the-editor"></a>

## 跟随编辑器

此处不做任何轮询。选项卡监听：

| 信号 | 反应 |
| :-- | :-- |
| 通过任何途径生成了源文件（监视器、此选项卡、来源操作、VSCode 扩展） | 重新计算该条目的状态 |
| 桥接的诊断提交 | 刷新每个条目的错误覆盖层 |
| 磁盘上添加、删除或重命名了源文件（即使关闭了保存时编译） | 重新扫描树 |
| 磁盘上修改了源文件 | 重新计算该条目 |
| 注册表中添加、删除或重命名了资产 | 重新计算其解析到的条目 |

反应会合并为每个时钟周期一次刷新，因此一个头文件重新编译其十二个依赖项时，选项卡只重绘一次。**刷新**仍适用于这些情况之外的任何场景——例如在会话中途挂载的插件。

<a id="new-source"></a>

## 新建源文件

*新建 ▾* 会从插件模板在 `Resources/Templates` 下写入文件，桥接的监视器会像对待任何保存一样列出并编译它。菜单有三个部分：

| 部分 | 条目 | 模板 | 写入内容 |
| :-- | :-- | :-- | :-- |
| DreamShaderLang 2.0 | **材质 (.dss)** *（自 2.0.0 起）* | `NewMaterial.dss` | 两个 `uniform` 和 `export void <Name>(inout material m)` |
| | **材质函数 (.dss)** *（自 2.0.0 起）* | `NewFunction.dss` | `export float3 <Name>(float3 InColor, float Strength = 1.0)` |
| | **实例 (.dsi)** | `NewInstance.dsi` | `#pragma instance(Parent = …)`——参见[创建材质实例](#create-material-instance) |
| 自定义 Pass 管线 (.dsp) *（自 2.1.0 起）* | **全屏后处理链** | `NewPipelinePostProcess.dsp` | 一个 `copy` 将光照场景抓取到半尺寸缓冲区，一个 `fullscreen` 通道将其以着色形式混合回场景之上，两者均采用 `BeforePostProcess` |
| | **网格遮罩链** | `NewPipelineMeshMask.dsp` | 一个 `mesh` 通道将列表 `<Base>` 中的对象在不进行深度测试的情况下绘制到遮罩中，一个 `fullscreen` 通道点亮遮罩周围的像素：穿墙轮廓 |
| | **计算链** | `NewPipelineCompute.dsp` | 一个 `compute` 通道在 `BeginView` 下推进一个逐帧保留的 256 × 256 风场（`History = true`），并导出为渲染目标 `<Name>_Wind` |
| 1.x 块 | **材质 (.dsm)** | `NewMaterial.dsm` | 一个 `Shader` 块 |
| | **材质函数 (.dsf)** | `NewFunction.dsf` | 一个 `ShaderFunction` 块 |
| | **头文件 (.dsh)** | `NewHeader.dsh` | 一个带有一个 `Function` 的头文件 |

管线会指定其通道所绘制的材质，因此每个管线模板也会写入这些材质，紧挨着 `.dsp`，并且 `.dsp` 一经落盘即构建；着色器则直接写在 `.dsp` 本身中。`<Base>` 是去掉其 `CP_` 前缀后的名称（`CP_Outline` 得到 `Outline`）；没有前缀的名称则按原样使用。

| 模板 | 同时还会写入，除 `.dsp` 之外 |
| :-- | :-- |
| 全屏后处理链 | `PP_<Base>.dss` — `fullscreen` 通道的后处理材质，通过 `UE.UserSceneTexture` 读取副本 |
| 网格遮罩链 | `M_<Base>Mask.dss` — `mesh` 通道的覆盖材质，带有 `UE.DreamPassOutput` — 以及 `PP_<Base>Composite.dss`，轮廓的后处理材质 |
| 计算链 | 无：计算着色器就是该通道的 `hlsl` 块（[HLSL 通道](../runtime/hlsl.md)） |

伴生文件会先写入，因此监视器在编译管线时能找到 `.dsp` 所指定的材质；所有文件要么全部写入，要么全部不写——一旦某次写入失败，会收回在此之前写入的文件。管线只有在被某物激活时才会运行：*Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass* 中的全局管线、Dream Pass Volume、某个组件，或 API——参见
[Custom Pass 运行时](../runtime/index.md#what-makes-a-pipeline-apply)。

对话框：

| 字段 | 默认值 |
| :-- | :-- |
| **Name** | `M_NewMaterial` / `MF_NewFunction`（`.dss`）· `F_NewFunction`（`.dsf`）/ `Common` / `MI_NewInstance` / `CP_SoftGlow`、`CP_Outline`、`CP_WindField`（三种管线模板）——必须是标识符 |
| **Folder** | 导航树指向的源文件夹，或者当该文件夹属于某个插件时，项目的 `DShader` 根目录（插件源为只读） |

对于管线，对话框还会说明该模板除 `.dsp` 之外会写入哪些文件。

无论哪种情况，资产都会落在其邻居的旁边——在 `DShader/Materials/` 中创建的文件会编译到
`/Game/Materials/…`。1.x 块自己会说明这一点：其 `Name=` 是文件夹相对于其根目录的路径
加上名称。`.dss` 什么也不说明：名称来自其 `export`，而文件夹就是文件所在的位置。
`.dss` 是一种产品——一个材质，或任意数量的导出函数，每个函数都是自己的
资产——旁边还有未导出的辅助项；这两个条目启动这两种类型。

| 拒绝 | 消息 |
| :-- | :-- |
| 不是标识符 | `The name must be an identifier: letters, digits and underscores, not starting with a digit.` |
| 在任何可写根目录之外 | `Choose a folder under the project's DShader root. A plugin's sources are read-only.` |
| 文件已存在 | `'{Path}' already exists.` |
| 某个管线模板除 `.dsp` 之外要写入的文件已存在 | `'{Path}' already exists, and the template writes it for '{Pipeline}'; choose another name.` |

这些模板由自动化测试
`DreamShader.Browser.NewSource.TemplatesRenderAndCompile` 编译，因此不会腐坏。管线模板
不在其所覆盖的模板之列。

<a id="create-material-instance"></a>

## 创建材质实例

可从检查器、上下文菜单以及内容浏览器条目 **Create DreamShader
instance** 打开。

| 方面 | 值 |
| :-- | :-- |
| 窗口标题 | **Create material instance** |
| 大小 | 480×240，模态，无法通过最小化或最大化调整大小 |

| 字段 | 类型 | 默认值 |
| :-- | :-- | :-- |
| **Parent** | 只读文本 | 父材质的名称 |
| **Name** | 文本框 | `MI_<ParentName>`，针对现有资产进行唯一化 |
| **Folder** | 文本框 | 父材质的文件夹加上 *Material Instance Subfolder* 设置（默认为 `Instances`）；空的子文件夹会将实例放在父材质旁边 |
| **Browse...** | 按钮 — “Pick the destination folder.” | 打开标题为 “Choose a destination folder” 的文件夹选择器 |
| **Open the instance after creating** | 复选框 | 已勾选 |
| **Cancel** / **Create** | 按钮 | — |

创建步骤及其错误：

| 防护 | 错误 |
| :-- | :-- |
| 没有父材质 | `No parent material was provided.` |
| 名称为空或文件夹为空 | `Provide a name and a destination folder.` |
| 父材质仅存在于内存中 | 转发 [Materialize](#materialize) 错误 |
| 目标路径已存在资产 | `An asset already exists at {PackageName}.` |
| 无法创建包 | `Failed to create package {PackageName}.` |
| 无法创建对象 | `Failed to create the material instance object.` |
| 无法保存包 | `Generated DreamShader asset '{Path}' could not be saved.` |
| 父材质在打开与确认之间失效 | `The parent material is no longer available.` — 窗口关闭 |

成功时提示为 `Created {Name}`——资产名称来自 **Name** 框，而非对象
路径——并且窗口关闭。失败时错误会以提示显示，并且 **窗口保持打开**，以便更正
名称或文件夹。

| 细节 | 行为 |
| :-- | :-- |
| 创建的类 | 一个普通的 `UMaterialInstanceConstant`，而不是 `UDreamShaderMaterialInstance` |
| 对象标志 | `RF_Public \| RF_Standalone` |
| 父材质分配 | 设置为仅编辑器，随后进行一次编辑后变更并广播资产已创建 |
| 保存失败回滚 | 取消广播半创建的对象，剥离其标志，将其重命名到瞬态包中且不带重定向器，标记为垃圾，并清除包的脏标志——因此它无法在 GC 中存活、不会出现在内容浏览器中、不会被 *Save All* 持久化，也不会阻止同名重试 |

对仅存在于内存中的父材质进行实例化会先实体化父材质，因此子实例永远不会引用
瞬态对象。从未编译过的源会先被编译；如果它仍未产生
材质，提示为 `Compile {File} first.`

<a id="materialize"></a>

## 实体化

将仅存在于内存中的材质及其包装的隐藏基础写入磁盘。

| 规则 | 细节 |
| :-- | :-- |
| “仅存在于内存中”测试 | 材质的包带有新建包标志 |
| 已在磁盘上 | 原样返回——该操作无效果 |
| 要求 | 材质必须是 `UDreamShaderMaterialInstance`，并记录有源文件路径 |
| 实现 | 对该源文件重新运行生成，强制开启且瞬态 **关闭**，然后在同一对象路径重新加载对象 |

| 消息 | 原因 |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | 材质不是 DreamShader 实例，或未记录源路径 |
| `Failed to materialize the material to disk: {Error}` | 生成失败 |
| `Materialized the material but could not reload it at {ObjectPath}.` | 生成成功但对象未重新加载 |
| `Materialized {Name} to disk` | 成功——检查器重新指向已持久化的资产 |

单击按钮时会按对象路径重新解析材质，而不是保留自面板构建时的引用，因此期间发生的删除或垃圾回收不会使其崩溃。

<a id="keyboard"></a>

## 键盘

| 按键 | 命令 |
| :-- | :-- |
| `F5` | 刷新 |
| `Ctrl+B` | 编译所选 |
| `Ctrl+Shift+B` | 编译全部 |
| `Enter` | 打开材质 |
| `Ctrl+Enter` | 打开源 |
| `Ctrl+F` | 聚焦搜索框 |

<a id="example"></a>

## 示例

一个源树以及 Sources 模式为其显示的内容：

```text
<Project>/DShader/
    Materials/M_Emissive.dsm        ● up to date          /Game/Materials
    Materials/M_Toon.dss            ● up to date          /Game/Materials
    Materials/M_Broken.dsm          ▲ compile error       L12:9 'Tin' is not declared in this scope.
    Materials/M_New.dsm             ○ not compiled        No generated asset at /Game/Materials/M_New.M_New
    Lib/Noise.dsf                   ◆ function · used by 2 material(s)
    Lib/Common.dsh                  ◆ function · used by 3 material(s)
    Packages/Sample/Demo.dsm        (not listed — under DShader/Packages)
```

<a id="see-also"></a>

## 另请参阅

- [编辑器集成](editor-integration.md) — 打开此选项卡的菜单条目，以及管线打开所在的[细节面板](editor-integration.md#pass-pipeline-details-panel)
- [Custom Pass 管线 — `.dsp`](../language-v2/passes.md) — 管线模板使用的编写语言
- [预览](preview.md) — 检查器预览背后的渲染器
- [分歧](../generation/divergence.md) — Revert、Adopt、Detach
- [反编译器](decompiler.md) — 导出 .dss / .dsi
- [瞬态材质](../generation/ephemeral.md) — 仅内存生成、隐藏基础、实体化
- [缓存](../generation/caching.md) — **up to date** 和 **stale** 背后的源哈希
- [资产路径](../generation/asset-paths.md) — `Name=` 和 `Root=` 如何解析为对象路径
- [项目设置](../settings/project.md) — *Material Instance Subfolder* 和 Ephemeral 可见性开关
- [桥接](bridge.md) — 此选项卡读取的诊断存储及其跟随的事件
- [材质实例 API](../api/material-instance.md) — C++ 中的 `UDreamShaderMaterialInstance`
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每条消息
