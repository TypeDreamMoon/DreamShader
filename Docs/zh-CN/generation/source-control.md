---
title: 版本控制
description: 2.0 起哪些生成资产会落盘、给它们做版本管理的两套可行做法，以及 dsc list-generated —— ignore 规则和 P4 typemap 就是照它写的。
---

提交什么、忽略什么，以及怎么让一个团队只有一种答案。

| 项目 | 值 |
| :-- | :-- |
| 永远提交 | 每一个 `.dss`、`.dsi`、`.dsp`、`.dsh`、`.dsm`、`.dsf` —— 源码才是材质本身；`.dsp` 有 HLSL pass 时还有 `DShader/.dreampass/` （自 2.1.0 起） |
| 永远不提交 | `Intermediate/DreamShader/`、`Saved/DreamShader/` |
| 由你决定 | 生成出来的 `.uasset` —— 这一页讲的就是这个决定 |
| 工具 | [`dsc list-generated`](../tools/commandlet.md#list-generated) —— 不构建任何东西，列出源码会生成的全部资产 |
| 起始版本 | （自 2.0.0 起） |

<a id="为什么这里有一个选择"></a>

## 为什么这里有一个选择

整个 1.x 时期，交互式编辑器把生成资产放在内存里，所以大多数工程在 cook 之外从没见过生成的 `.uasset`。
从 2.0 起，只有一种产物还是这样。

| 产物 | 是否落盘 |
| :-- | :-- |
| `Graph` 后端的材质 | **总是** —— 每次成功生成都会保存 |
| 材质函数、层、层混合 | **总是** |
| 材质实例（`.dsi`） | **总是** |
| Custom Pass 管线（`UDreamPassPipeline`，来自 `.dsp`）（自 2.1.0 起） | **总是** |
| 导出 buffer 的 render target（`<Pipeline>_<Buffer>`）（自 2.1.0 起） | **总是** |
| `ThinCustom` 后端的材质 | 只有 [Materialized](ephemeral.md) 之后才有；默认是 Ephemeral 的，此时根本没有文件 |

`UMaterial` 和 `UMaterialFunction` 是引擎自带的类，没有办法让自己不被资产枚举到，所以一生成就保存 ——
就落在大家手工做的资产旁边，版本控制工具会提示你把它们加进去。请以工程为单位做一次决定。
「一半生成资产提交了、一半被忽略」是唯一行不通的安排。

**提示：**

生成资产的出身记在 **package metadata** 里（`DreamShader.SourceFile`、`DreamShader.SourceHash`、
`DreamShader.OutputDigest`），而不是 asset registry tag —— 插件没法给引擎自带的类加 tag。
在编辑器里，Material Content Browser 按它筛选；在编辑器外，去问**源码**，`list-generated` 做的正是这件事。

<a id="列出生成资产"></a>

## 列出生成资产

```powershell
./dsc.ps1 list-generated -All                                             # package 名
./dsc.ps1 list-generated -All -ListAs Files     -Out generated-files.txt  # 相对工程目录的路径
./dsc.ps1 list-generated -All -ListAs GitIgnore -Out generated.gitignore  # 一段带锚定的 .gitignore
./dsc.ps1 list-generated -All -ListAs Json      -Out generated.json       # 全部字段
```

清单是从源码算出来的 —— 前端、绑定器、资产去向规则 —— 不构建、不加载、不保存任何东西。
Ephemeral 材质默认不列（它没有文件可忽略），除非加 `-IncludeEphemeral`。编不过的源文件仍然会贡献失败之前已经确定的产物，
并且整次运行以 `RESULT=FAILED` 结束，所以清单不会悄悄变短。脚本请始终传 `-Out` 再读文件；日志是给人看的。
`-ListAs Files` 和 `GitIgnore` 写的是相对工程目录的路径；工程目录之外的生成资产（**引擎级**插件的内容）没有这样的路径，
运行会说明有几个被略过（`DSH9049`），`Packages` 或 `Json` 会列出它们。

<a id="做法-a--源码入库资产不入库"></a>

## 做法 A —— 源码入库，资产不入库

生成资产就是构建产物。没人提交它们；每台机器自己构建。

- **忽略它们。** 用 `-ListAs GitIgnore` 刷新 `.gitignore` 里一段带标记的块；或者更简单 ——
  当生成资产都放在自己的文件夹里时 —— 每个文件夹一行。`.p4ignore` 用同样的路径。
- **构建它们。** 全新检出的工作区里没有生成资产，任何引用它们的东西都要等它们存在之后才能解析。

| 时机 | 由什么来构建 |
| :-- | :-- |
| sync / checkout 之后 | `./dsc.ps1 compile -All` —— 做成 post-checkout hook 或同步步骤 |
| 打开编辑器 | 启动时与保存时都会编译源码 |
| cook | cook director 会先编译并保存全部工程源码 |
| CI | `check -All` 给源码把关；在任何要加载内容的步骤之前跑 `compile -All` |

代价：手改过的生成资产只存在于一台机器上 —— 而这正是这套做法的本意。用 **Adopt Into Source**
在它丢失之前把改动搬进源码。

<a id="做法-b--把生成资产也提交"></a>

## 做法 B —— 把生成资产也提交

生成资产是「碰巧被纳入版本管理的派生二进制」，就像工程给 lightmap 做版本管理那样。

- **Git** —— 和其他 `.uasset` 一样对待。
- **Perforce** —— 给它们 `binary+w` 类型，这样编译不用 checkout 就能覆盖；否则每次保存源码都会因为文件只读而失败。
  不要加 `+l`：给一个「工具会替所有人重写」的文件上排他锁，等于排队。
- **保持诚实。** 让 CI 证明提交进来的资产与源码一致：

```powershell
./dsc.ps1 compile -All              # 没变的源码按 build key 跳过：不重写、没有 diff
git status --porcelain -- Content   # 必须什么都不输出
```

build key 还覆盖插件版本、引擎版本，以及源码读过的宏，所以**升级 DreamShader 或引擎会让一切重新生成**。
请把它单独放进一个提交。生成资产的合并冲突靠重新编译解决，永远不要二选一。

**提示：**

build key 用**相对工程目录**的路径给每个源文件和头文件命名 （自 2.1.1 起），所以在别处检出的 CI agent 得到同样的键，
上面的检查在那里也成立。到 2.1.0 为止键用的是**绝对**路径，那样的 agent 会重建并重存每一个资产。见
[重新生成](regeneration.md#哈希覆盖什么)。

<a id="选哪个"></a>

## 选哪个

| | A —— 不入库 | B —— 入库 |
| :-- | :-- | :-- |
| 仓库体积 | 只有源码 | 每次重新生成都会长大 |
| 全新检出后能否直接打开 | 要先跑一次编译 | 立刻可以 |
| 没有插件工具链的人 | 构建不了 —— 不可行 | 没问题 |
| 过期资产 | 不可能出现 | 可能；上面的 CI 检查会抓到 |
| 升级插件或引擎 | 没有东西要提交 | 一次很大的重新生成提交 |
| 手改过的生成资产 | 在被 adopt 之前只在本地 | 被纳入版本管理，并被标记为已分歧 |

做法 A 适合人人都开着带插件的编辑器的团队；做法 B 适合有人只消费内容的团队。不管选哪个，ThinCustom 后端都让问题变小：
Ephemeral 材质没有文件，需要做决定的只剩它的函数、它的实例，以及被 Materialize 过的那些。

<a id="说明"></a>

## 说明

- **`DShader/.dreampass/` 在两种做法下都是源码** （自 2.1.0 起）。它装着 `.dsp` 管线的 HLSL pass 所用 `.usf` 的快照，以及
  全局 shader 据以构建的 slot 注册表；那些 shader 是从快照而不是从正在编辑的文件编译的，所以队友的编辑器和 cook 跑的就是快照。
  把这个文件夹和产生它的 `.dsp` / `.usf` 改动一起提交；它里面的冲突靠任取一边、再重编 `.dsp` 解决，永远不要手改。见
  [Custom Pass（.dsp）](../language-v2/passes.md)和 [`pass-registry`](../tools/commandlet.md#dsp-源文件)。
- **`Intermediate/DreamShader/GeneratedShaders/*.ush`** 是 1.x 为 `Function` 块写的东西，2.0 起没有编译再往那里写。永远不要提交它；
  Unreal 自带的忽略规则本来就覆盖 `Intermediate/`。
- **`Saved/DreamShader/`** 装着 dump、索引、迁移前源文件的备份和 bridge 的状态。清理之前值得看一眼的只有
  `Saved/DreamShader/Migrated/` —— [`dsc migrate`](../tools/migrate.md) 把 1.x 原件挪到那里。
- **自带 DreamShader 源文件的插件**对自己的 `Content/` 面临同样的选择。随插件发布生成资产（做法 B），工程才能不编译就用这个插件；
  `-All` 也覆盖插件源码根，一份清单两边都够用。
- **给源文件改名会挪动它的资产。** 在做法 B 下，把改名、新资产和旧资产的删除放进同一个提交；
  [资产改名同步](../tools/editor-integration.md#资产改名同步)会让引用指向新路径。
- **`list-generated` 列的是源码会构建的东西，不是磁盘上躺着的东西。** 源文件被删掉的生成资产是孤儿，不在清单里；
  Material Content Browser 会显示它们。
