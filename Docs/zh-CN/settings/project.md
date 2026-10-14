---
title: "项目设置"
description: "项目设置"
---

<a id="project-settings"></a>

# 项目设置

> [DreamShader](../index.md) » [设置](index.md) » **项目设置**

项目级配置对象：*Project Settings ▸ DreamPlugin ▸ Dream Shader* 下的十七个属性，持久化到项目的 `DefaultEngine.ini`。

| | |
| :-- | :-- |
| 声明于 | C++ —— `UDreamShaderSettings`，`DreamShader` 运行时模块中的一个 `UDeveloperSettings` |
| 种类 | 项目设置对象 |
| 配置文件 | `<Project>/Config/DefaultEngine.ini`，节 `[/Script/DreamShader.DreamShaderSettings]` |

<a id="location"></a>

## 位置

| 方面 | 值 |
| :-- | :-- |
| Container | `Project` |
| Category | `DreamPlugin` |
| Section | `DreamShader` |
| 节标题 | **Dream Shader** |
| 节描述 | **Dream Shader Settings** |

面板位于 *Project Settings ▸ DreamPlugin ▸ Dream Shader* —— **不在** *Plugins* 下面。在面板中做的修改，以及编辑器在别处暴露的开关，都会直接写回 `DefaultEngine.ini`。

<a id="settings"></a>

## 设置

每一个可配置属性，按它在面板中所属的分类分组。

| 分类 | 界面名称 | 配置属性 | 类型 | 默认值 | 作用 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| Mappings | Shading Model Mappings | `ShadingModelMappings` | `TMap<FString, EMaterialShadingModel>` | *空* | 为 `ShadingModel` 设置值增加或覆盖拼写。先于内置表扫描。 |
| Mappings | Blend Mode Mappings | `BlendModeMappings` | `TMap<FString, EBlendMode>` | *空* | 为 `BlendMode` / `RenderType` 增加或覆盖拼写。 |
| Mappings | Material Domain Mappings | `MaterialDomainMappings` | `TMap<FString, EMaterialDomain>` | *空* | 为 `MaterialDomain` / `Domain` 增加或覆盖拼写。 |
| Paths | Source Directory | `SourceDirectory` | `FDirectoryPath` | `DShader` | 扫描 `.dsm` / `.dsf` / `.dsh` 源文件的根目录。为空时回退到 `DShader`；相对路径相对项目目录解析。`<Source>/Packages` 由它推导。 |
| Paths | Generated Shader Directory | `GeneratedShaderDirectory` | `FDirectoryPath` | `Intermediate/DreamShader/GeneratedShaders` | 生成的 `.ush` include 写到哪里，以及虚拟着色器目录映射到哪里。为空时回退到默认值。 |
| Paths | **Scan Plugin Source Directories** | `bScanPluginSourceDirectories` | `bool` | `true` | 开启时，每个带 `DShader` 文件夹的已启用插件都贡献一个自己的源码根。关闭后，项目的 *Source Directory* 是唯一的根。 |
| Compiler | **Preprocessor Defines** *（自 1.9.0 起）* | `PreprocessorDefines` | `TMap<FString, FString>` | *空* | 项目编译的每个源文件里 [`#if`](../language/preprocessor.md) 能看到的名字。键**区分大小写**；空值是一个裸标记，对 `#if` 和 `defined()` 都为真。以 `DS_` 开头的键会被丢弃并记一条警告 —— 这个前缀留给内置名。修改这张表会重建读到了改动名字的源文件。 |
| Compiler | **Default Compiler Backend** | `DefaultBackend` | `EDreamShaderDefaultBackend` | `ThinCustom` | 源文件未设置 `Settings = { Backend = … }` 时使用的后端。修改它会重建构建键因此改变的每个源文件；不受影响的被跳过。 |
| Compiler | **Graph Layout Style** *（自 2.0.0 起）* | `GraphLayoutStyle` | `EDreamShaderGraphLayoutStyle` | `Blocks` | 生成的图怎么排布：三种读取编译器 IR 的布局之一 —— `Blocks`（按源码顺序排成一页方框，之间用具名 reroute 相连；默认）、`SourceBands`（每条源码语句一条带）和 `Layered`（整张图分层）—— 或 `Classic`，即 1.x 的布局。见[图布局](../generation/graph-layout.md#layout-styles)；`dsc dump-layout` 不构建任何东西就能画出三种 IR 布局。不属于构建键：布局只挪节点，不改变它们计算的东西。所以源文件要等下次**重建**才会重新排布 —— 未改动的照常被跳过，用 *Recompile*（或 `compile -Force`）才能看到已有的图换成新样式。 |
| Compiler | **Show Ephemeral Materials** | `bShowEphemeralMaterials` | `bool` | `false` | 关闭时，纯内存的 DreamShader 实例把自己报告为非资产，从内容浏览器、资产注册表枚举和保存选择器中消失。只作用于 `ThinCustom` 后端：`Graph` 材质是普通的 `UMaterial`，没有 Ephemeral 状态，总是可见。每次查询都实时读取。 |
| Compiler | Auto Compile On Save | `bAutoCompileOnSave` | `bool` | `true` | 关闭时，源目录监视器完全忽略文件变化。 |
| Compiler | Save Debounce Seconds | `SaveDebounceSeconds` | `float` | `0.25` | 文件变化后编译前的静默期。钳制在 `[0.05, 10.0]`；滑条停在 `2.0`。设置对象不可用时回退到 `0.25`。 |
| Compiler | Verbose Logs | `bVerboseLogs` | `bool` | `false` | 为依赖文件编译队列增加 `Display` 级日志。 |
| Decompiler | Export Decompiled Layout | `bExportDecompiledLayout` | `bool` | `true` | 开启时，反编译出的源码保留节点位置：旧式 `.dsm` 带 `Layout = { … }` 段，2.0 反编译器写出的 `.dss` 带 `#pragma layout` 行 *（自 2.0.0 起）*。 |
| Editor | Open In New Window | `bOpenInNewWindow` | `bool` | `true` | 关闭时，VSCode 启动命令会加上 `--reuse-window`。 |
| Editor | **Material Instance Subfolder** | `InstanceSubfolder` | `FString` | `Instances` | 相对父材质所在文件夹的子目录，材质内容浏览器在这里创建新实例。留空则与父材质同级创建。资产名为 `MI_<ParentName>`，并做唯一化处理。 |
| Editor | **Sync Source References On Asset Rename** *（自 1.9.0 起）* | `bSyncSourceReferencesOnAssetRename` | `bool` | `true` | 开启时，重命名或移动资产会改写引用它的 `.dsm` / `.dsf` / `.dsh` 文件，写之前先把每个文件备份成 `<file>.bak`。只改项目源码 —— 插件根从不触碰。见[资产重命名同步](../tools/asset-rename-sync.md)。实时读取，所以关掉后下一次重命名就生效。 |

**加粗**的行带有显式 `DisplayName`；其余显示的是 Unreal 从属性标识符推导出的名字，布尔属性会去掉开头的 `b`。

<a id="enumerators-of-edreamshaderdefaultbackend"></a>

### `EDreamShaderDefaultBackend` 的枚举项

| 枚举项 | 含义 |
| :-- | :-- |
| `Graph` | 每个材质构建一份可见的 `UMaterial` 节点图。 |
| `Instance` | `ThinCustom` 的**弃用**别名 *（自 1.5.0 起）*。 |
| `ThinCustom` | 把图建在每材质一份的隐藏基础材质上，并产出它的轻量材质实例。**默认值。** |

完整行为，包括按文件写的 `Backend` 设置如何覆盖它，见[后端](backend.md#precedence)。

<a id="verbatim-tooltips"></a>

## 原文提示文本

*Scan Plugin Source Directories*：

> When enabled, every enabled plugin that ships a DShader folder contributes its own source root, so
> a plugin can carry the .dsm/.dsf/.dsh files that build its materials. Plugin roots are discovered
> and compiled but never rewritten by the editor -- only the project's own source directory is
> writable. Imports never cross roots: a file resolves its imports against its own root and that
> root's Packages folder.

*Default Compiler Backend*：

> DreamShader 如何物化一个未指定 `Settings = { Backend = "..." }` 的源文件。
> ThinCustom（默认值）在每材质一份的隐藏基础上构建材质图，并产出它的
> 轻量、Ephemeral 材质实例 —— 完整的功能面，没有可见的逐材质
> 资产。Graph 构建一份可见的 UMaterial 节点图。Instance 是 ThinCustom 的弃用别名。

*Show Ephemeral Materials*：

> 只作用于 ThinCustom/Instance 后端 —— 它是唯一一个有 Ephemeral 状态的后端，
> 通过 UDreamShaderMaterialInstance::IsAsset 把它藏起来。开启时，Ephemeral 材质会像未保存的资产一样出现
> 在内容浏览器里。默认关闭：源文件才是预期的
> 创作面，而隐藏材质也防止意外的 Save 操作把它们
> 物化到磁盘。Graph 后端的材质是普通 UMaterial，无法退出
> 资产枚举，所以它们总是可见，此设置不影响它们。

*Material Instance Subfolder*：

> 相对父材质所在文件夹的子目录，材质内容浏览器在这里创建
> 新的材质实例。留空则与父材质同级创建。

*Sync Source References On Asset Rename*：

> 开启时，在编辑器中重命名或移动资产会改写引用它的 .dsm/.dsf/.dsh 文件，
> 因此源文件会继续按名字而不是它曾有的路径来指称该资产。只有项目源目录下的文件会被改写 —— 插件源根从不触碰 —— 并且
> 每个文件在被写入前都会复制成旁边的 .bak。像 Fix Up
> Redirectors 这样的批量重命名会被合并为单次处理。删除资产不在其列：删除没有
> 新路径可写，它产生的编译错误就是正确的结果。如果你
> 更希望插件从不自行编辑你的源文件，就关掉它。

*Preprocessor Defines*：

> 每个 .dsm/.dsf/.dsh 源文件编译时都带的预处理器 define —— 它的 #if / #elif 条件
> 读到的内容。名字回答 defined()；能解析为整数的值按数字比较，其他
> 按字符串比较；空值仍算作已定义；此表中不存在的名字求值为
> 0，与 C 中一样。条件在生成时求值，落败的分支在
> 解析器看到它之前就被剪掉，所以一个条件可以选中一个 Domain、一个 ShadingModel 乃至整个 Outputs 块 —— 这些
> 都是 StaticSwitch 够不着的，因为它们描述的是材质*是*什么，
> 而不是它*计算*什么。编辑此表只会重建那些源文件确实读取了
> 发生变化的名字的生成资产。名字区分大小写，必须匹配 [A-Za-z_][A-Za-z0-9_]*。DS_ 前缀
> 保留给只读的内置名；使用它的条目会被丢弃并记一条日志警告，而不是
> 让编译失败，因为设置里的错误没有源码行可以报告。

这段提示文本所暗指的内置名是 `DS_ENGINE_MAJOR`、`DS_ENGINE_MINOR`、
`DS_ENGINE_PATCH`、`DS_PLATFORM`、`DS_PLUGIN_VERSION` 和 `DS_SUBSTRATE`；它们被列在
[预处理器页面](../language/preprocessor.md)中。提示文本只按前缀提到它们，
因为 UHT 将元数据字符串限制在 1024 个字符，把六个都拼出来会超出。

<a id="the-mapping-maps"></a>

## 映射表

三个 `Mappings` 条目扩展了三个枚举设置所接受的取值拼写。它们
**默认为空**：内置别名表在解析时按需重建，从不
物化到这些映射中，所以空面板并不意味着“没有别名”。

| 行为 | 细节 |
| :-- | :-- |
| 优先级 | 先扫描项目映射；第一个规范化键命中即立刻返回 |
| 遮蔽 | 键规范化后与某个内置别名相同的条目会替换它 |
| 禁用 | 映射到 `MSM_MAX` / `BLEND_MAX` / `MD_MAX` 的条目会让该拼写解析失败，且不会回落到内置表 |
| 键匹配 | trim、小写，并删除空格、`_` 和 `-` |
| 歧义 | 两个规范化后完全相同的条目，其解析顺序未指定 |

完整的内置表见[材质枚举](material-enums.md)。

<a id="other-places-these-settings-are-surfaced"></a>

## 还能在哪里改到这些设置

| 设置 | 其他入口 |
| :-- | :-- |
| `bShowEphemeralMaterials` | *Tools ▸ DreamShader ▸ Show Ephemeral Materials*，以及[材质内容浏览器](../tools/material-browser.md)的 Project 页。两者都会写入 ini，并为每个纯内存实例重新广播资产的创建/移除。 |
| `DefaultBackend`、三张映射表、`PreprocessorDefines` | 在面板里修改一个会立刻重建工程的源文件，并记录 `DreamShader setting '{Setting}' changed; regenerating all source files.`。它们每一个都属于构建键，所以不受这次修改影响的源文件被跳过。如果持久化的生成资产遮蔽了结果，随后会弹出通知。 |
| `SourceDirectory`、`GeneratedShaderDirectory` | 由模块的目录辅助函数消费；见[生成的 HLSL](../generation/generated-hlsl.md) 和 [Package](../tools/packages.md)。 |
| `bSyncSourceReferencesOnAssetRename` | 每次重命名资产时被[资产重命名同步服务](../tools/asset-rename-sync.md)读取，合并的批次刷新时再读一次。没有菜单项。 |
| `PreprocessorDefines` | 编译所见 define 表由五层组成，它是其中一层，也是人能编辑的那层里优先级最低的。C++ 注册和 provider 压过它，[命令行](../tools/commandlet.md)上的 `-Define=` 又压过那些；只有内置的 `DS_` 名压过一切。见[预处理器 ▸ define 从何而来](../language/preprocessor.md#where-defines-come-from)。 |

<a id="notes"></a>

## 备注

- 该类是 `Config=Engine, DefaultConfig`，所以值保存在**项目的**
  `Config/DefaultEngine.ini` 里，而不是每用户文件里。签出这个
  项目的所有人共享同一份配置。
- **没有持久化的开关。** ThinCustom 产物是 Ephemeral 的 —— 源文件才是
  创作面 —— 落盘发生在 cook 时、通过
  [命令行](../tools/commandlet.md)，或通过显式操作。*Default Compiler Backend*
  在 1.5.0 取代了旧的 In-Memory 开关。**在 `2.0.0` 中移除：** *Lay Out In-Memory Graphs*
  （`bLayoutInMemoryGraphs`）。图布局现在总是运行；只有大图的性能保护
  会跳过它。*Show Ephemeral Materials* 的键在 `2.0.0` 之前叫 `bShowInMemoryMaterialsInContentBrowser`
  ，加载时自动迁移。
- `GeneratedShaderDirectory` 只在虚拟着色器目录尚未映射时才被读取。如果本项目已经存在
  映射，**已有映射优先于该设置**，直到
  编辑器重启。
- 内置着色器库路径设置已在 **1.3.8 移除**；没有替代项。
- 此对象的 C++ 界面 —— 三个 `TryResolve*` 方法、`NormalizeMappingKey` 和
  三个 `BuildDefault*Mappings` 辅助函数 —— 记录于[设置 API](../api/settings.md)。

<a id="example"></a>

## 示例

`<Project>/Config/DefaultEngine.ini`，列出所有标量属性在默认值下的情况：

```ini
[/Script/DreamShader.DreamShaderSettings]
SourceDirectory=(Path="DShader")
GeneratedShaderDirectory=(Path="Intermediate/DreamShader/GeneratedShaders")
DefaultBackend=ThinCustom
bShowEphemeralMaterials=False
bAutoCompileOnSave=True
SaveDebounceSeconds=0.250000
bVerboseLogs=False
bExportDecompiledLayout=True
bOpenInNewWindow=True
InstanceSubfolder=Instances
bSyncSourceReferencesOnAssetRename=True
```

ini 键就是原始 C++ 标识符，包括布尔属性上的 `b` 前缀 —— `bAutoCompileOnSave`，
而不是 `AutoCompileOnSave`。三张映射表和 *Preprocessor Defines* 是容器属性；
请在 Project Settings 面板里编辑，让编辑器写回它们。手写条目使用
`+Key=((…))` 形式：

```ini
+PreprocessorDefines=(("MOONTOON_LEGACY_TOON", "1"))
```

<a id="dreamshader-custom-pass"></a>

## DreamShader Custom Pass

旁边还有第二个设置对象：`UDreamPassSettings`，位于 `DreamShaderPass` 运行时
模块，在 *Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass*。它保存在同一个
`DefaultEngine.ini` 的 `[/Script/DreamShaderPass.DreamPassSettings]` 节，并配置
[Custom Pass 运行时](../runtime/index.md)。它在每个引擎上都存在；UE 5.8 以下没人读它。

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

<a id="see-also"></a>

## 另见

- [Custom Pass 运行时](../runtime/index.md) —— `GlobalPipelines` 和 layer 喂给谁
- [设置](index.md) —— 本页默认值与之交互的逐文件 `Settings` 节
- [预处理器](../language/preprocessor.md) —— *Preprocessor Defines* 喂给谁，以及另外四层
- [Backend](backend.md) —— *Default Compiler Backend* 如何被逐文件覆盖
- [材质枚举](material-enums.md) —— 映射表所扩展的内置表
- [着色器设置](material.md) —— 反射出的 `UMaterial` 属性面
- [设置 API](../api/settings.md) —— C++ 中的 `UDreamShaderSettings`
- [材质实例 API](../api/material-instance.md) —— 由可见性开关驱动的 `IsAsset()` 行为
- [内存材质](../generation/ephemeral.md) —— “仅内存”在实践中意味着什么
- [生成的 HLSL](../generation/generated-hlsl.md) —— `GeneratedShaderDirectory` 收到什么
- [材质内容浏览器](../tools/material-browser.md) —— Project 页与实例工厂
- [反编译器](../tools/decompiler.md) —— 布局导出开关
- [工作区](../tools/workspace.md) —— VSCode 启动与导出的设置清单
- [资产重命名同步](../tools/asset-rename-sync.md) —— *Sync Source References On Asset Rename* 开启什么
- [编辑器集成](../tools/editor-integration.md) —— Tools 菜单项
