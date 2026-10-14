---
title: 内存材质（Ephemeral）
description: 为什么编译好的 DreamShader 材质不出现在 Content Browser —— ThinCustom 产物的 Ephemeral 与 Materialized 两种状态、隐藏 base 材质，以及怎么把它写到磁盘。
---

你保存了一个 `.dsm`，日志说材质已生成，然后 Content Browser 里什么都没有。这不是坏了。本页解释真正发生了什么，
建议在提 bug 之前先读一遍。

（自 2.0.0 起） 1.x 叫它"内存材质"（in-memory）。这个词同时指三件不同的事，所以 2.0 起只有一个概念、一个名字：
**ThinCustom** 产物有两种状态，而且只有两种。

| 状态 | 含义 | 怎么分辨 |
| :-- | :-- | :-- |
| **Ephemeral** | 磁盘上没有 package。隐藏 base 材质住在 transient package 里；实例报告 `IsAsset() == false`，把自己藏起来。默认状态 | package 带着 `PKG_NewlyCreated` |
| **Materialized** | 磁盘上有一个 package 装着实例，base 作为它的子对象 export | package 作为文件存在 |

在两者之间移动只有两种办法：

| 转换 | 触发 |
| :-- | :-- |
| **Materialize** | 显式的 *Materialize* 操作、cook、commandlet、为它创建子材质实例，或编译一个以它为父的 `.dsi` |
| **Make Ephemeral** | *Tools ▸ DreamShader ▸ Make Ephemeral*，它会删掉磁盘上的 package |

其余一切由一条规则决定：**磁盘上有 package 的产物，就留在磁盘上。** 由存储决定，而不是由发起编译的那一方决定。

**警告：**

**只有 ThinCustom 有这两种状态。** `Graph` backend 的 `UMaterial` 和 `.dsf` 的 `UMaterialFunction` 是普通引擎资产，
没有 `IsAsset()` 可以拿来"撒谎"、也没法退出资产枚举，所以没有 Ephemeral 形态，本页对它们都不适用。2.0 对这两种的规则很简单：
**每次成功生成都把它们存盘** （自 2.0.0 起）。

| 项目 | 说明 |
| :-- | :-- |
| 适用于 | `ThinCustom` backend（项目默认）下的 `Shader` 块 |
| 发出的类 | `UDreamShaderMaterialInstance` |
| 标记 | 所在 package 仍带着 `PKG_NewlyCreated` |
| 起始版本 | （自 1.5.0 起） —— ThinCustom backend 与 Content Browser 可见性开关；（自 2.0.0 起） 改名为 Ephemeral |

<a id="dreamshader-材质到底是什么"></a>

## DreamShader 材质到底是什么

在默认 backend 下，一个 `Shader` 块 **不会** 产出 `UMaterial` 资产。它产出两个对象：

```text
UDreamShaderMaterialInstance          "M_Emissive"            <- the asset you reference
  └─ UMaterial (subobject, hidden)    "MB_DreamThinBase_M_Emissive"
       └─ the generated node graph
```

节点图 —— 每个 `Properties` 节点、每条 `Graph` 语句、每条 `Outputs` 绑定 —— 都构建在这个 **隐藏 base** 上。实例只是个薄壳，
携带参数值、来源元数据和编译好的 shader map。`Settings` 里的东西全部落在 base 上，这也是为什么从生成出来的实例上读
`BlendMode` 看到的是 *继承来的* 值。

| 成员 | 可见性 | 含义 |
| :-- | :-- | :-- |
| `SourceFilePath` | 详情面板中只读，分类 `DreamShader` | 生成这个实例的 `.dsm` |
| `SourceHash` | 详情面板中只读，分类 `DreamShader` | source hash —— 见[重新生成](regeneration.md#构建键) |
| `Parent` | 标准 | 隐藏的 base 材质 |

两个 override 决定了这个类的行为，而且两个都很关键。

| Override | 结果 | 后果 |
| :-- | :-- | :-- |
| `HasOverridenBaseProperties()` | 当且仅当父级是 `UMaterial` 时为 `true` | **根** 实例拥有自己的静态排列和 shader map；挂在它下面的子 `UMaterialInstanceConstant` 走回引擎默认行为，**共享** 这份 shader map，于是几十个颜色或参数变体只花一次编译 |
| `IsAsset()` | 当 package 仍是 `PKG_NewlyCreated` **且** *Show Ephemeral Materials* 关闭时为 `false` | Ephemeral 材质对 Content Browser、资产选择器和保存选择器隐身 |

第二个 override 就是"我的材质去哪了"的全部答案。

<a id="隐藏-base"></a>

### 隐藏 base

| 状态 | base 对象名 | Outer | 对象标记 |
| :-- | :-- | :-- | :-- |
| Ephemeral | `MB_DreamThinBase_<净化后的 package 名>`，必要时加唯一后缀 | transient package | `RF_Public`、`RF_Standalone`、`RF_Transient` |
| Materialized | `MB_DreamThinBase_<instance leaf name>` | **实例对象自身** | `RF_Public`、`RF_Standalone` |

`<净化后的 package 名>` 是实例的 package 名，`[A-Za-z0-9_]` 之外的每个字符都换成 `_`、连续下划线折叠，所以实例
`/Game/Mat/Test` 的 base 在加唯一后缀之前是 `MB_DreamThinBase__Game_Mat_Test`。重新生成时实例复用它挂着的 base；新实例分配
一个唯一的对象名 —— 净化只是一个可读的标签，`/Game/A_B/M` 和 `/Game/A/B_M` 会压成同一个标签，但必须各有各的图
（自 2.1.2 起）。

Materialized 状态下 base 是实例的子对象，因此它作为普通 export 序列化 **进实例自己的 package**。一个资产、一个 `.uasset`、
Content Browser 里没有 `MB_DreamThinBase_*` 兄弟资产，cook 时也不会丢跨 package 的父级 import。由于非 package 的 outer
本身就让 `IsAsset()` 为 false，base 在两种状态下都不可见。

**提示：**

由 1.5.0 之前的版本保存、父级位于独立 `MB_*` package 中的实例 **不会** 被复用。重新生成会创建一个新的子对象 base，
把旧的兄弟 package 留成孤儿。这个孤儿无害，可以删掉。

<a id="为什么-content-browser-里什么都没有"></a>

## 为什么 Content Browser 里什么都没有

ThinCustom 产物刻意做成 Ephemeral 的。`.dsm` 文件才是创作面，磁盘上躺着的生成 `.uasset` 只是源码已经说过的东西的第二份拷贝。
除 cook、commandlet 和显式 *Materialize* 之外的所有触发方式都让产物保持 Ephemeral —— 除非这个资产已经在磁盘上，
那样它就在磁盘上维护。见[什么会触发编译](index.md#什么会触发编译)和
[磁盘上已经存在的资产](#磁盘上已经存在的资产)。

当产物处于 Ephemeral 状态时：

- `IsAsset()` 返回 `false`，所以它不出现在任何 Content Browser 视图和资产选择器里。
- 生成结束时它的 package 被标记为非脏，因此 *Save All* 和退出提示都无法悄悄把它落盘。
- 但它是完全可用的 —— 通过 [Material Content Browser](../tools/editor-integration.md#material-content-browser)、实时预览，
  以及任何持有它指针的东西。

<a id="可见性开关"></a>

### 可见性开关

| 项目 | 值 |
| :-- | :-- |
| 设置项 | **Show Ephemeral Materials**（`bShowEphemeralMaterials`），分类 `Compiler` |
| 默认 | 关 |
| 菜单 | *Tools ▸ DreamShader ▸ Show Ephemeral Materials* |
| 另一处 | Material Content Browser 的 *View ▾ ▸ Show Ephemeral materials* |

切换会写入项目配置，并立刻为每个 package 仍是 `PKG_NewlyCreated` 的活 `UDreamShaderMaterialInstance` 广播
asset-created / asset-deleted 事件，所以图块会即时出现和消失，不需要重新扫描。确认消息是：

```text
Showing {Count} Ephemeral material(s) in the Content Browser and asset pickers.
Hidden {Count} Ephemeral material(s) from the Content Browser and asset pickers.
```

**提示：**

2.0 之前的配置键是 `bShowInMemoryMaterialsInContentBrowser`。设过它的工程会在加载时迁移 —— 旧键读一次、并入
`bShowEphemeralMaterials` —— 所以已有的 `DefaultEngine.ini` 照样有效，不会悄悄退回默认值。

**警告：**

开关打开后，Ephemeral 材质看上去就是一个普通图块 —— 对它执行显式 **Save** 会把它 Materialize。从那以后这个路径就在磁盘上维护，
见[磁盘上已经存在的资产](#磁盘上已经存在的资产)。真的想要文件时，请用 *Materialize*，不要用 *Save*。

<a id="落盘materialize"></a>

## 落盘（Materialize）

*Materialize* 会以"Materialized + 强制"重新执行该材质自己源文件的生成，然后在解析出的路径上重新加载对象。

| 入口 | 操作 |
| :-- | :-- |
| Material Content Browser 的检查器 | **Materialize** 按钮 —— *"Write this memory-only material (and its base) to disk."* |
| Content Browser 右键菜单 | DreamShader 的 materialize 操作 |
| 隐式 | 为 Ephemeral 父材质创建子材质实例时，会先把父级落盘 |

创建子实例 *必须* 先落盘，因为 transient 的 base 不能作为父级 import。子实例的默认目标是
`<parent directory>/<Instance Subfolder>` —— 即 **Material Instance Subfolder** 项目设置，默认 `Instances`；
留空则创建在父级旁边。子实例命名为 `MI_<parent leaf>`，并做唯一化处理。

已经 Materialized 的产物会原样返回。

| 消息 | 原因 |
| :-- | :-- |
| `This material is memory-only and has no DreamShader source file to materialize from.` | 对象不是 `UDreamShaderMaterialInstance`，或它的 `SourceFilePath` 为空 |
| `Failed to materialize the material to disk: {Message}` | 用于落盘的那次重新生成失败了 |
| `Materialized the material but could not reload it at {ObjectPath}.` | 生成成功，但对象无法被重新加载回来 |

**为什么 PKG_NewlyCreated 要最后才清**

Ephemeral 的 ThinCustom 产物落盘时，`PKG_NewlyCreated` 标记是在保存前的最后一步才清掉的。Unreal 的
`IsEmptyPackage()` 只统计 `IsAsset()` 为 `true` 的对象，而标记还在时实例报告的是 `false` —— 所以带着标记去保存的 package
会被当成空包跳过。保存失败时标记会被还原。首次落盘时会直接通知资产注册表，因此 Content Browser 不需要重新扫描就会更新。

<a id="磁盘上已经存在的资产"></a>

## 磁盘上已经存在的资产

**存储决定一次重建怎么落地，而不是发起编译的那一方决定。** （自 1.8.0 起） 一次编译落在磁盘上已经存在的 package 上时，
资产会被重建**并保存**，而不是在自己的文件之上于内存里重建。生成成功，并记一条：

```text
'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run
Tools > DreamShader > Make Ephemeral to make it Ephemeral again.
```

| 项目 | 说明 |
| :-- | :-- |
| 适用于 | 每一种触发方式 —— 因为每一次交互式编译都让 ThinCustom 产物保持 Ephemeral |
| 自 | `1.8.0` —— 在此之前，这样一次编译在内存里重建资产，然后清掉脏标记 |

**警告：**

旧行为产出的是一个**既不等于磁盘上的文件、也不等于将来会被写出的任何东西、还自称干净**的对象 ——
于是一次 Save All 就能落下一个谁也没选过的状态，而你在编辑器里看到的版本会在重启后消失。
现在「这个资产到底是什么」只有一个答案：上一次编译产出的东西，无论这个资产住在哪。

一个值得知道的推论：**启动扫描不强制重建**。在每个 Ephemeral 产物无论如何都会重建时，强制是免费的；
一旦磁盘资产开始被*保存*，它就不免费了 —— 每次启动都会重写每一个已落盘的生成资产。现在启动遵守
[source hash 跳过](regeneration.md#什么时候会跳过重新生成)，已经最新的已保存资产不会被动。
改 **Default Compiler Backend** 同样不强制：[构建键](regeneration.md#构建键)覆盖了这个设置，
受它影响的资产各自过不了跳过检查。

有两个命令处理磁盘上的这些资产：

| 命令 | 效果 |
| :-- | :-- |
| *Tools ▸ DreamShader ▸ Make Ephemeral* （自 2.0.0 起） | 删除带 DreamShader 来源元数据的 **Materialized ThinCustom 产物**的 package，确认框逐个列出，然后把它们重建为 Ephemeral。手写资产绝不会被碰；`Graph` 材质和材质函数不会被列出 —— 它们没有 Ephemeral 状态可回。空的情况：`No Materialized DreamShader ThinCustom products found.`（1.x 的 *Clean Persisted Generated Assets* 还会删已保存的 `UMaterial` 和 `UMaterialFunction`） |
| *Tools ▸ DreamShader ▸ Clean Generated Shaders* | 删除生成着色器目录下的每个 `*.ush`，并排入一次强制的全量重编。2.0 起已经没有编译往那里写 —— 见 [Function 的 HLSL](index.md#function-的-hlsl) |

*Make Ephemeral* 会递归扫描 `/Game`，寻找 package 存在于磁盘、且元数据中 `DreamShader.SourceFile` 非空的
`UDreamShaderMaterialInstance`。

修改 **Default Compiler Backend** —— 或者某张映射表、或者预处理器 define —— 会不强制地重新生成每个源文件，
随后如果还有已保存的生成资产，会警告：

```text
{Count} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials.
Run Tools > DreamShader > Make Ephemeral to remove them.
```

（这些资产其实不再*遮蔽*任何东西 —— 它们只是仍然在磁盘上，并且就在磁盘上被重建。这条通知是指给想要 Ephemeral 终态的人看的。）

<a id="cook-行为"></a>

## Cook 行为

cook 才是这些资产变成实体的时候。

| 方面 | 行为 |
| :-- | :-- |
| 判定 | `-run=` 的值里含 `Cook` 就认为该进程是 cook |
| 谁来生成 | **只有 cook director** —— 以 `-cookworker` 启动的进程跳过生成，直接加载 director 保存的结果 |
| 何时 | post-engine-init，即引擎子系统就绪之后、commandlet 的 `Main` 之前 |
| 生成什么 | 除 `.dsh` 外的所有工程 DreamShader 源文件，强制生成并 Materialized |
| 顺序 | 与 [`compile -All`](../tools/commandlet.md#-all-的发现与排序) 相同：`.dsf` 在前，然后**分轮**，调用别的源文件所生成函数的源文件会在那个函数建好之后重试 （自 2.1.2 起） |
| 资产注册表 | 生成实际写出的每个 package 都会在 commandlet 的 `Main` 之前交给 `IAssetRegistry::ScanModifiedAssetFiles` （自 1.8.0 起） |
| 失败时 | **中止整个 cook** —— 除非源文件属于一个本工程不往里写的插件，见下 |

cook 日志：

```text
DreamShader cook: generating {Count} source file(s) as persistent assets...
  [Cook] {Message}
DreamShader: retrying {Count} source file(s) that failed before {Count} other(s) in the same pass were built; they may depend on them.
  [Cook] Failed: {Message}
  [Cook] Not this project's to build (plugin '{PluginName}'): {Message}
DreamShader cook: registered {Count} generated package(s) with the AssetRegistry.
DreamShader cook asset generation complete.
```

**警告：**

只要有一个文件生成失败，cook 就会以一条 fatal 日志结束：
`DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook]
Failed entries above.` 在 cook 之前，先在编辑器里把每个源文件干净地编译一遍，或者跑一次
[commandlet](../tools/commandlet.md)。（自 2.1.2 起） 起只有当一轮什么新东西都没建出来时，失败才算最终失败 ——
只是在等另一个源文件的函数的源文件不算；所有 `[Cook] Failed` 行都排在成功之后。

**一个例外**，也是那条 fatal 消息只数"本工程"的原因 （自 2.1.2 起）：住在引擎插件或商城插件 `DShader` 文件夹下的源文件。
那个插件的产物是它自己的 —— 第 3 道插件闸门拒绝往它的挂载点生成（`DSH8090`），工程也改不了它的文件 —— 所以那里的失败记成
`[Cook] Not this project's to build (plugin '{Name}')`，cook 继续；插件自己带着它的源文件描述的资产，cook 打包的就是那些。
工程自己的每个源文件，以及 `<Project>/Plugins` 下插件的每个源文件，生成不了仍然会中止 cook。

生成放在 post-engine-init 而不是模块启动时，是因为它依赖的材质编辑库需要编辑器子系统，而那些在模块加载期还不存在。
限定只在 director 上跑，则避免了所有 worker 争抢保存同一批 package。

**提示：**

**注册进资产注册表这一步是必要的，不是保险。** （自 1.8.0 起） 一个 cook 请求是经
`IAssetRegistry::DoesPackageExistOnDisk` 解析的，而这个函数只查注册表的缓存状态、没有文件系统兜底 ——
而生成恰好跑在注册表枚举完内容目录**之后**。所以刚写出的 package 对那次查询是不可见的，即使显式的
`DirectoriesToAlwaysCook` 覆盖了它的文件夹也会被无声地丢掉。生成因此记下它保存了什么，并把这些文件重新扫进注册表。

<a id="说明"></a>

## 说明

- Ephemeral 与 Materialized 只由一件事决定：package 是否仍带着 `PKG_NewlyCreated`。除此之外没有任何东西区分两种状态。
- **布局总是会运行** （自 2.0.0 起）。内存材质以前会跳过布局，除非开了 *Lay Out In-Memory Graphs*；这个设置在 2.0.0
  里删掉了，每张生成的图都会布局，只剩 `Classic` 风格的大图保护。见 [Layout 与 #Region](../language/layout.md)。
- 来源元数据 —— 源码路径**和**哈希 —— 在**两种**状态下都会打在每个生成资产上；隐藏的 ThinCustom base 在 Materialized 时额外打。
  见[重新生成](regeneration.md#元数据存在哪)。
- 生成出来的实例刻意 **不是** `RF_Transactional`：材质实例不支持撤销/重做，否则 shader map 会失步。
- 这些都不影响 source hash 短路；哈希没变的编译不论哪种 backend 都会跳过工作。*Recompile DSM* 和
  *Clean Generated Shaders* 会强制越过它。

<a id="完整示例"></a>

## 完整示例

```c
Shader(Name="Materials/M_Emissive")
{
    Properties { ScalarParameter Intensity = 2.0 [Slider(0, 10)]; }
    Settings   { Backend = "ThinCustom"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = vec3(1.0, 0.4, 0.1) * Intensity; }
}
```

在编辑器里保存这个文件，产出一个 Ephemeral 产物：

```text
package        /Game/Materials/M_Emissive          (PKG_NewlyCreated, not dirty, not on disk)
  object       M_Emissive                          UDreamShaderMaterialInstance
  transient    MB_DreamThinBase__Game_Materials_M_Emissive   UMaterial, holds the node graph
```

执行 *Materialize* 之后：

```text
on disk        <Project>/Content/Materials/M_Emissive.uasset
  export       M_Emissive                          UDreamShaderMaterialInstance
  export       MB_DreamThinBase_M_Emissive         UMaterial (hidden, same package)
```

注意 base 的名字在两种状态下不同：Ephemeral 时是净化后的 package 名，磁盘上是实例的 leaf 名。

<a id="继续阅读"></a>

## 继续阅读

- [Backend](../settings/backend.md)
  - [项目设置](../settings/project.md)
  - [重新生成](regeneration.md)
  - [编辑器工具](../tools/editor-integration.md)
  - [命令行](../tools/commandlet.md)
