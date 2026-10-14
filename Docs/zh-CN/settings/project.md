---
title: 项目设置
description: Project Settings ▸ DreamPlugin ▸ Dream Shader 下的全部 17 个属性、它们真实的配置标识符、默认值与作用，以及 Custom Pass 的设置对象。
---

DreamShader 的项目级配置是一个 `UDeveloperSettings` 对象，共 **17** 个属性。

**警告：**

面板位于 **Project Settings ▸ DreamPlugin ▸ Dream Shader** —— *不在* *Plugins* 下面。

| 方面 | 值 |
| :-- | :-- |
| 类 | `UDreamShaderSettings`，位于 `DreamShader` 运行时模块 |
| Container | `Project` |
| Category | `DreamPlugin` |
| Section | `DreamShader`，标题为 **Dream Shader** |
| 配置文件 | `<Project>/Config/DefaultEngine.ini`，节 `[/Script/DreamShader.DreamShaderSettings]` |

该类是 `Config=Engine, DefaultConfig`，因此值保存在**项目的** `DefaultEngine.ini` 里，而不是每用户文件里。签出这个项目的所有人
共享同一份配置。面板里的修改，以及编辑器在别处暴露的开关，都直接写回 `DefaultEngine.ini`。

<a id="17-个设置"></a>

## 17 个设置

| 分类 | 面板名 | 配置属性 | 类型 | 默认值 | 作用 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| Mappings | Shading Model Mappings | `ShadingModelMappings` | `TMap<FString, EMaterialShadingModel>` | *空* | 为 `ShadingModel` 取值增加或覆盖拼写。先于内置表扫描。 |
| Mappings | Blend Mode Mappings | `BlendModeMappings` | `TMap<FString, EBlendMode>` | *空* | 为 `BlendMode` / `RenderType` 增加或覆盖拼写。 |
| Mappings | Material Domain Mappings | `MaterialDomainMappings` | `TMap<FString, EMaterialDomain>` | *空* | 为 `MaterialDomain` / `Domain` 增加或覆盖拼写。 |
| Paths | Source Directory | `SourceDirectory` | `FDirectoryPath` | `DShader` | 扫描源文件的根目录。为空时回退到 `DShader`；相对路径相对项目目录解析。`<Source>/Packages` 由它推导。 |
| Paths | Generated Shader Directory | `GeneratedShaderDirectory` | `FDirectoryPath` | `Intermediate/DreamShader/GeneratedShaders` | 虚拟着色器目录映射到哪里。为空时回退到默认值。 |
| Paths | **Scan Plugin Source Directories** （自 1.6.0 起） | `bScanPluginSourceDirectories` | `bool` | `true` | 开启时，每个带 `DShader` 目录的已启用插件都贡献一个自己的源码根。关闭后，项目的 *Source Directory* 是唯一的根。见[工程结构](../getting-started/project-layout.md)。 |
| Compiler | **Preprocessor Defines** （自 1.9.0 起） | `PreprocessorDefines` | `TMap<FString, FString>` | *空* | 项目编译的每个源文件里 [`#if`](../language/lexical.md#预处理器) 能看到的名字。键**区分大小写**；空值是一个裸标记，对 `#if` 和 `defined()` 都为真。以 `DS_` 开头的键会被丢弃并记一条警告 —— 这个前缀留给内置名。修改这张表只重建读到了改动名字的源文件。 |
| Compiler | **Default Compiler Backend** | `DefaultBackend` | `EDreamShaderDefaultBackend` | `ThinCustom` | 源文件未写 `Settings = { Backend = … }` 时使用的后端。修改它会重建构建键因此改变的源文件；不受影响的被跳过。 |
| Compiler | **Graph Layout Style** （自 2.0.0 起） | `GraphLayoutStyle` | `EDreamShaderGraphLayoutStyle` | `Blocks` | 生成的图怎么排布：三种读取编译器 IR 的布局之一 —— `Blocks`（按源码顺序排成一页方框，之间用具名 reroute 相连；默认）、`SourceBands`（每条源码语句一条带）、`Layered`（整张图分层）—— 或 `Classic`，即 1.x 的布局。`dsc dump-layout` 不构建任何东西就能画出三种 IR 布局。不属于构建键：布局只挪节点，不改变计算。所以源文件要等下次**重建**才会重新排布 —— 未改动的照常被跳过，用 *Recompile*（或 `compile -Force`）才能看到已有的图换成新样式。 |
| Compiler | **Show Ephemeral Materials** | `bShowEphemeralMaterials` | `bool` | `false` | 关闭时，纯内存的 DreamShader 实例把自己报告为非资产，从内容浏览器、资产注册表枚举和保存选择器中消失。只作用于 `ThinCustom` 后端：`Graph` 材质是普通的 `UMaterial`，没有 Ephemeral 状态，总是可见。每次查询都实时读取。 |
| Compiler | Auto Compile On Save | `bAutoCompileOnSave` | `bool` | `true` | 关闭时，源目录监视器完全忽略文件变化。 |
| Compiler | Save Debounce Seconds | `SaveDebounceSeconds` | `float` | `0.25` | 文件变化后等待多久再编译。钳制在 `[0.05, 10.0]`，滑条上限是 `2.0`。设置对象不可用时回退到 `0.25`。 |
| Compiler | Verbose Logs | `bVerboseLogs` | `bool` | `false` | 为依赖文件编译队列增加 `Display` 级日志。 |
| Decompiler | Export Decompiled Layout | `bExportDecompiledLayout` | `bool` | `true` | 开启时，反编译出的源码保留节点位置：旧式 `.dsm` 带 `Layout = { … }` 段，2.0 反编译器写出的 `.dss` 带 `#pragma layout` 行 （自 2.0.0 起）。 |
| Editor | Open In New Window | `bOpenInNewWindow` | `bool` | `true` | 关闭时，VSCode 启动命令会加上 `--reuse-window`。 |
| Editor | **Material Instance Subfolder** | `InstanceSubfolder` | `FString` | `Instances` | 相对父材质所在文件夹的子目录，材质内容浏览器在这里创建新实例。留空则与父材质同级创建。资产名为 `MI_<ParentName>`，并做唯一化处理。 |
| Editor | **Sync Source References On Asset Rename** （自 1.9.0 起） | `bSyncSourceReferencesOnAssetRename` | `bool` | `true` | 开启时，重命名或移动资产会改写引用它的源文件，写之前先把每个文件备份成 `<file>.bak`。只改项目源码 —— 插件根从不触碰。实时读取，关掉后下一次重命名就生效。 |

**加粗**的行带有显式 `DisplayName`；其余显示的是 Unreal 从属性标识符推导出的名字，布尔属性会去掉开头的 `b`。而 **ini 里的键
是原始 C++ 标识符**，`b` 前缀照写 —— 是 `bAutoCompileOnSave`，不是 `AutoCompileOnSave`。

<a id="edreamshaderdefaultbackend"></a>

### `EDreamShaderDefaultBackend`

| 枚举项 | 含义 |
| :-- | :-- |
| `Graph` | 每个材质构建一份可见的 `UMaterial` 节点图。 |
| `Instance` | `ThinCustom` 的**弃用**别名 （自 1.5.0 起） |
| `ThinCustom` | 把图建在每材质一份的隐藏基础材质上，并产出它的轻量材质实例。**默认值。** |

按文件写的 `Backend` 如何覆盖它，见 [Backend](backend.md#优先级)。

<a id="三张映射表"></a>

## 三张映射表

`Mappings` 下的三个条目用来扩展三个枚举设置可接受的取值拼写。它们**默认为空**：内置别名表是在解析时按需重建的，从不写进这
三个 map，所以面板是空的并不代表“没有别名”。

| 行为 | 细节 |
| :-- | :-- |
| 优先级 | 先扫描项目 map；第一个规范化键命中即立刻返回 |
| 覆盖 | 键规范化后与某个内置别名相同的条目会替换它 |
| 禁用 | 映射到 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 的条目会让该拼写解析失败，且不会回落到内置表 |
| 键匹配 | trim、小写，并删除空格、`_` 和 `-` |
| 歧义 | 两个规范化后相同的条目，解析顺序未定义 |

完整的内置表见[枚举取值](material-enums.md)。

<a id="preprocessor-defines"></a>

## Preprocessor Defines

（自 1.9.0 起）

这张表给项目的每个源文件提供 `#if` / `#elif` 读取的名字：名字回答 `defined()`；能解析成整数的值按数字比较，其他按字符串
比较；空值仍算已定义；表里没有的名字求值为 0，和 C 一样。条件在**生成时**求值，落败的分支在解析器看到之前就被剪掉，所以
条件能选择 `Domain`、`ShadingModel` 乃至整个 `Outputs` 块 —— 这些 StaticSwitch 都够不着，因为它们描述的是材质*是*什么，而
不是它*算*什么。名字区分大小写，必须匹配 `[A-Za-z_][A-Za-z0-9_]*`。

`DS_` 前缀留给只读的内置名 `DS_ENGINE_MAJOR`、`DS_ENGINE_MINOR`、`DS_ENGINE_PATCH`、`DS_PLATFORM`、`DS_PLUGIN_VERSION` 和
`DS_SUBSTRATE`；使用它的条目会被丢弃并记一条日志警告，而不是让编译失败 —— 设置里的错误没有源码行可以报。这张表是 define
来源的五层之一，也是人能编辑的那层里优先级最低的：C++ 注册和 provider 压过它，命令行的 `-Define=` 又压过那些，只有内置
`DS_` 名压过一切。见[预处理器](../language/lexical.md#预处理器)。

<a id="面板提示文本"></a>

## 面板提示文本

编辑器面板自身的原文。

*Scan Plugin Source Directories*：

> When enabled, every enabled plugin that ships a DShader folder contributes its own source root, so
> a plugin can carry the .dsm/.dsf/.dsh files that build its materials. Plugin roots are discovered
> and compiled but never rewritten by the editor -- only the project's own source directory is
> writable. Imports never cross roots: a file resolves its imports against its own root and that
> root's Packages folder.

*Default Compiler Backend*：

> How DreamShader materializes a source file that does not specify `Settings = { Backend = "..." }`.
> ThinCustom (the default) builds the material graph on a hidden per-material base and emits a
> lightweight, Ephemeral material instance of it -- full feature surface, no visible per-material
> asset. Graph builds a visible UMaterial node graph. Instance is a deprecated alias for ThinCustom.

*Show Ephemeral Materials*：

> Applies to the ThinCustom/Instance backend only -- it is the only one with an Ephemeral state,
> which it hides via UDreamShaderMaterialInstance::IsAsset. When enabled, Ephemeral materials appear
> in the Content Browser like unsaved assets. Disabled by default: the source files are the intended
> authoring surface, and hiding the materials also prevents accidental Save actions from
> materializing them to disk. Graph-backend materials are plain UMaterials with no way to opt out of
> asset enumeration, so they are always visible and this setting does not affect them.

*Material Instance Subfolder*：

> Subfolder, relative to the parent material's folder, where the Material Content Browser creates
> new material instances. Leave empty to create them alongside the parent.

*Sync Source References On Asset Rename*：重命名或移动资产时改写引用它的源文件，让源码继续指向资产而不是它原来的路径；
只改项目源码目录下的文件，插件根从不触碰；写之前把文件复制成旁边的 `.bak`；*Fix Up Redirectors* 这类批量重命名合并成一次；
删除资产不在其列 —— 删除没有新路径可写，它造成的编译错误就是正确结果。如果不希望插件自己改你的源文件，就关掉它。

<a id="还能在哪里改到这些设置"></a>

## 还能在哪里改到这些设置

| 设置 | 其他入口 |
| :-- | :-- |
| `bShowEphemeralMaterials` | *Tools ▸ DreamShader ▸ Show Ephemeral Materials*，以及[材质内容浏览器](../tools/editor-integration.md)的 Project 页。两者都会写入 ini，并为每个纯内存实例重新广播资产的创建/移除。 |
| `DefaultBackend`、三张映射表、`PreprocessorDefines` | 在面板里修改会立刻重建工程的源文件，并记录 `DreamShader setting '{Setting}' changed; regenerating all source files.` 它们都属于构建键，所以不受这次修改影响的源文件被跳过。如果持久化的生成资产遮蔽了结果，还会弹出通知。 |
| `SourceDirectory`、`GeneratedShaderDirectory` | 由模块的目录辅助函数消费 —— 见[工程结构](../getting-started/project-layout.md)和 [Package](../tools/packages.md)。 |
| `bScanPluginSourceDirectories` | 根列表是缓存的，只在 *Source Directory* 或这个开关变化、或按下 *DreamShader Gen ▸ Refresh* 时重建。 |
| `bSyncSourceReferencesOnAssetRename` | 每次重命名资产时被读取，合并的批次刷新时再读一次。没有菜单项。 |

<a id="dreamshader-custom-pass"></a>

## DreamShader Custom Pass

（自 2.1.0 起）

旁边还有第二个设置对象：`DreamShaderPass` 运行时模块里的 `UDreamPassSettings`，位于
*Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass*。它保存在同一个 `DefaultEngine.ini` 的
`[/Script/DreamShaderPass.DreamPassSettings]` 节，配置 [Custom Pass 运行时](../runtime/index.md)。它在每个引擎上都存在；UE 5.8
以下没人读它。

| 分类 | 配置属性 | 类型 | 默认值 | 作用 |
| :-- | :-- | :-- | :-- | :-- |
| Runtime | `bEnabled` | `bool` | `true` | 关闭后任何地方都不运行 pipeline，不论是谁激活的。`r.DreamPass.Enable 0` 对一次会话做同样的事。 |
| Pipelines | `GlobalPipelines` | `FDreamPassGlobalPipeline` 数组 —— `Pipeline`、`bEnabled`、`Priority`、`Overrides` | 空 | 以权重 1 作用于每个世界每个视图的 pipeline。世界启动时加载，列表被编辑时再加载一次。 |
| Layers | `LayerNames` | `FName` 数组，最多 32 个 | 空 | pass layer：`UDreamPassLayerComponent` 给 primitive 其中一些 layer，mesh pass 的 `Filter = Layer(Name)` 选出带它的 primitive。顺序就是位序 —— 给 layer 改名是安全的；挪动位置会让每个引用它的已编译 pipeline 指向别的 layer，直到它们重新编译。 |

```ini
[/Script/DreamShaderPass.DreamPassSettings]
bEnabled=True
+GlobalPipelines=(Pipeline="/Game/Effects/CP_Wind.CP_Wind",bEnabled=True,Priority=0.000000)
+LayerNames=Highlight
+LayerNames=XRay
```

<a id="备注"></a>

## 备注

- **没有持久化的开关。** `ThinCustom` 产物是 Ephemeral 的 —— 源文件才是创作面 —— 落盘发生在 cook 时、通过
  [命令行](../tools/commandlet.md)，或通过显式操作。*Default Compiler Backend* 在 （自 1.5.0 起） 取代了旧的 In-Memory 开关。
- **2.0.0 移除了** *Lay Out In-Memory Graphs*（`bLayoutInMemoryGraphs`）：图布局现在总是运行，只有大图的性能保护会跳过它。
  *Show Ephemeral Materials* 的键在 2.0.0 之前叫 `bShowInMemoryMaterialsInContentBrowser`，加载时自动迁移。
- `GeneratedShaderDirectory` 只在虚拟着色器目录尚未映射时才被读取。如果本项目已经存在映射，**已有映射优先于该设置**，直到编辑器
  重启。（自 2.0.0 起） 起 DreamShader 不再往那里写逐源文件的 `.ush`：`Function` 的 HLSL 直接嵌在 Custom 节点的代码里。
- 内置着色器库路径设置已在 **1.3.8 移除**，没有替代项。

<a id="示例"></a>

## 示例

`<Project>/Config/DefaultEngine.ini`，列出所有标量属性的默认值：

```ini
[/Script/DreamShader.DreamShaderSettings]
SourceDirectory=(Path="DShader")
GeneratedShaderDirectory=(Path="Intermediate/DreamShader/GeneratedShaders")
DefaultBackend=ThinCustom
GraphLayoutStyle=Blocks
bScanPluginSourceDirectories=True
bShowEphemeralMaterials=False
bAutoCompileOnSave=True
SaveDebounceSeconds=0.250000
bVerboseLogs=False
bExportDecompiledLayout=True
bOpenInNewWindow=True
InstanceSubfolder=Instances
bSyncSourceReferencesOnAssetRename=True
```

三张映射表和 *Preprocessor Defines* 是容器属性；请在 Project Settings 面板里编辑，让编辑器写回 ini。手写条目用 `+Key=((…))` 形式：

```ini
+PreprocessorDefines=(("MOONTOON_LEGACY_TOON", "1"))
```

<a id="下一步"></a>

## 下一步

- [Backend](backend.md) —— 按文件如何覆盖 *Default Compiler Backend*
- [枚举取值](material-enums.md) —— 映射表所扩展的内置表
- [预处理器](../language/lexical.md#预处理器) —— *Preprocessor Defines* 喂给谁，以及另外四层 define 来源
- [内存材质](../generation/ephemeral.md) —— Ephemeral 在实践中意味着什么
- [编辑器工具](../tools/editor-integration.md) —— Tools 菜单项与材质内容浏览器
