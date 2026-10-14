---
title: "版本控制"
description: "版本控制"
---

<a id="source-control"></a>

# 版本控制

> [DreamShader](../index.md) » [生成](index.md) » **版本控制**

提交什么、忽略什么，以及怎么让一个团队只有一种答案。

| | |
| :-- | :-- |
| 永远提交 | 每一个 `.dss`、`.dsi`、`.dsp`、`.dsh`、`.dsm` 和 `.dsf` —— 源码才是材质本身 —— 以及当 `.dsp` 有 HLSL pass 时的 `DShader/.dreampass/` |
| 永远不提交 | `Intermediate/DreamShader/`、`Saved/DreamShader/` |
| 由你决定 | 生成出来的 `.uasset` 文件 —— 这一页讲的就是这个决定 |
| 工具 | [`dsc list-generated`](../tools/commandlet.md#list-generated) —— 不构建任何东西，列出源码会构建的全部资产 |
| 起始版本 | `2.0.0` |

<a id="why-there-is-a-choice-to-make"></a>

## 为什么这里有一个选择

整个 1.x 时期，交互式编辑器把生成资产放在内存里，所以大多数工程在 cook 之外从没见过生成出来的 `.uasset`。从 `2.0.0` 起，只有一种产物还是这样：

| 产物 | 来源 | 是否落盘 |
| :-- | :-- | :-- |
| `Graph` 后端的材质 | `.dss` / `.dsm` | **总是** —— 每次成功生成都会保存 |
| 材质函数、层、层混合 | `.dss` / `.dsf` | **总是** |
| 材质实例 | `.dsi` | **总是** |
| Custom Pass 管线（`UDreamPassPipeline`） | `.dsp` | **总是** |
| 导出 buffer 的 render target（`<Pipeline>_<Buffer>`） | `.dsp` | **总是** |
| `ThinCustom` 后端的材质 | `.dss` / `.dsm` | 只有 [Materialized](ephemeral.md) 之后才有；默认是 Ephemeral 的，此时根本没有文件 |

一个 `UMaterial` 和一个 `UMaterialFunction` 是引擎自带的类，没有办法让自己不被资产枚举到，所以一生成就保存。这些文件会出现在 `Content/` 里、大家手工做的资产旁边，版本控制工具会提示你把它们加进去。请以工程为单位做一次决定，选择下面两套做法中的哪一套 —— 「一半生成资产提交了、一半被忽略」的仓库是唯一行不通的安排。

<a id="how-a-generated-asset-is-recognised"></a>

## 怎么识别一个生成资产

生成资产把它的出身记在 **package metadata** 里，而不是 asset registry tag 里：

| 键 | 值 |
| :-- | :-- |
| `DreamShader.SourceFile` | 构建它的源码，相对于工程目录 |
| `DreamShader.SourceHash` | 它构建时所用的 [build key](caching.md) |
| `DreamShader.OutputDigest` | 生成器写下的内容，用于 [divergence](divergence.md) 检测 |

> [!NOTE]
> 引擎自带的类没法由插件添加 registry tag，所以在编辑器外没有任何东西能向 asset registry 询问「哪些资产是 DreamShader 的」。在编辑器里，[Material Content Browser](../../tools/material-browser.md) 按出身筛选；在编辑器外，去问**源码** —— ``list-generated`` 做的正是这件事。

<a id="listing-the-generated-assets"></a>

## 列出生成资产

```powershell
# long package names, one per line
./dsc.ps1 list-generated -All

# files, relative to the project directory
./dsc.ps1 list-generated -All -ListAs Files -Out Saved/DreamShader/generated-files.txt

# a ready .gitignore block (anchored paths, with a header comment)
./dsc.ps1 list-generated -All -ListAs GitIgnore -Out Saved/DreamShader/generated.gitignore

# everything: source, kind, backend, package, file, onDisk, persistent
./dsc.ps1 list-generated -All -ListAs Json -Out Saved/DreamShader/generated.json
```

清单是从源码算出来的 —— 前端、绑定器、资产去向规则 —— 不构建、不加载、不保存任何东西，所以在 CI 里是安全的；它的代价只是命令工具启动一次。不加 `-Out` 时清单写进日志，那是给人看的；脚本请始终传 `-Out` 再读文件。相对的 `-Out` 以启动 `dsc.ps1` 时所在的目录为基准。清单回答的是：

| 字段（`Json`） | 含义 |
| :-- | :-- |
| `source` | 声明该产物的源文件 |
| `kind` | `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance`；对于 `.dsp`，是 `PassPipeline`，并且按每个导出 buffer 是 `PassExportTarget` —— 它的 render target *（自 2.1.0 起）* |
| `backend` | 该后端上的材质是 `ThinCustom`；其他一切都是 `Graph` —— 一个函数、一个层、一个混合和一个实例都只有一种构建方式，不管它们的文件名写的是哪个后端 |
| `package` / `objectPath` | `/Game/FX/M_Glow` / `/Game/FX/M_Glow.M_Glow` |
| `file` / `projectRelativeFile` | 该 package 在磁盘上所在（或将在）的位置 |
| `onDisk` | 该文件此刻是否存在 |
| `persistent` | 只有当前是 Ephemeral 的 ThinCustom 材质才是 `false` |

Ephemeral 材质默认不出现在任何清单里，除非 `-IncludeEphemeral` 要求列出它：它没有文件可忽略。编译失败的源文件仍然会贡献失败之前已经确定的产物，并且整次运行以 `RESULT=FAILED` 结束，所以清单不会悄悄变短。

> [!NOTE]
> `-ListAs Files` 和 `GitIgnore` 写的是相对工程目录的路径。工程目录之外的生成资产 —— **引擎级**插件的内容 —— 没有这样的路径；运行会说明有几个被略过（[`DSH9049`](../diagnostics/DSH9xxx.md#dsh9049)），``Packages`` 或 ``Json`` 会列出它们。

<a id="recipe-a--sources-in-assets-out"></a>

## 做法 A —— 源码入库，资产不入库

生成资产就是构建产物。没人提交它们；每台机器自己构建。

**`.gitignore`。** 在两个标记之间保留一段生成块，并从清单刷新它：

```powershell
# Tools/Update-GeneratedIgnore.ps1
$begin = '# >>> DreamShader generated assets'
$end   = '# <<< DreamShader generated assets'
$list  = Join-Path ([IO.Path]::GetTempPath()) 'dreamshader-generated.gitignore'
& ./Plugins/DreamShader/.skill/dsc.ps1 list-generated -All -ListAs GitIgnore -Out $list
if ($LASTEXITCODE -ne 0) { throw 'list-generated failed; .gitignore was left as it is.' }
$block = @(Get-Content $list)

$lines = @(Get-Content .gitignore)
$from = [Array]::IndexOf($lines, $begin)
$to   = [Array]::IndexOf($lines, $end)
$head = if ($from -ge 0) { @($lines | Select-Object -First $from) } else { $lines }
$tail = if ($to -ge 0) { @($lines | Select-Object -Skip ($to + 1)) } else { @() }
@($head) + $begin + $block + $end + @($tail) | Set-Content .gitignore
```

更简单的做法 —— 当工程把生成资产放在它们自己的文件夹里时 —— 也是 [资产路径](asset-paths.md) 让这件事变简单的原因：一个 `.dss` 产物落在其源文件所在的文件夹下，而 1.x 的块落在它的 `Name=` / `Root=` 所说的位置 —— 就是每个文件夹一行：``/Content/DreamShader/``。用清单来检查这些文件夹里没有别的东西。

**Perforce。** 把同样的路径加到 `.p4ignore`；不需要别的，因为这些文件永远不会被添加。

**构建。** 全新检出的工作区里没有生成资产，任何引用它们的东西 —— 一个关卡、一个蓝图、一个手工做的材质实例 —— 都要等它们存在之后才能解析：

| 时机 | 由什么来构建 |
| :-- | :-- |
| sync 或 checkout 之后 | `./dsc.ps1 compile -All` —— 做成 post-checkout hook 或同步步骤 |
| 打开编辑器 | 启动时与保存时都会编译源码；第一次打开一个大工程要为它们全部付出代价 |
| cook | cook director 在 cook 之前会编译并保存全部工程源码 |
| CI | ``./dsc.ps1 check -All`` 给源码把关；在任何要加载内容的步骤之前跑 ``compile -All`` |

**代价。** sync 之后第一次启动编辑器，如果那次 sync 动过共享头文件，就会重建包含它们的那些内容。手改过的生成资产只存在于一台机器上 —— 而这正是这套做法的本意；用 [Adopt](divergence.md) 在它丢失之前把这样的改动搬进源码。

<a id="recipe-b--commit-the-generated-assets"></a>

## 做法 B —— 把生成资产也提交

生成资产是「碰巧被纳入版本管理的派生二进制」，就像工程给 lightmap 做版本管理那样。

**Git。** 像对待任何其他 `.uasset` 一样对待它们（LFS、`binary`）。不用忽略规则。

**Perforce。** 给它们一个可写类型，这样编译不用 checkout 就能覆盖它们 —— 否则每次保存源码都会因为文件只读而失败：

```powershell
# one typemap line per generated file; paste under `TypeMap:` in `p4 typemap`
$list = Join-Path ([IO.Path]::GetTempPath()) 'dreamshader-generated-files.txt'
./dsc.ps1 list-generated -All -ListAs Files -Out $list
Get-Content $list | ForEach-Object { "`tbinary+w //depot/MyProject/$_" }
```

`+w` 让文件在工作区里保持可写；**不要**加 `+l` —— 给一个「工具会替所有人重写」的文件上排他锁，等于排队。当生成资产都放在自己的文件夹里时，这就是一行：``binary+w //depot/MyProject/Content/DreamShader/...``。

**保持诚实。** 提交进来的资产可能落后于它的源码。让 CI 证明它没有：

```powershell
./dsc.ps1 compile -All          # unchanged sources are skipped by build key: no rewrite, no diff
git status --porcelain -- Content   # must print nothing
```

一次编译会跳过每一个 [build key](caching.md) 仍然匹配的源文件，因此一棵最新的树产出
的东西逐字节一致。该键还涵盖插件版本、引擎版本以及源文件读取过的宏
—— 所以**升级 DreamShader 或引擎会重新生成一切**。请把这个改动单独放在一个提交里，
不要夹带其他内容；这个 diff 很大、是二进制、而且是预期之中的。

> [!NOTE]
> 该键用源文件和头文件相对于工程目录的路径来命名它们，因此检出到别处的 CI agent
> 会得到相同的键，上面的检查在那里也成立。到 2.1.0 为止，该键用的是它们的**绝对**路径，
> 这样的 agent 会重建并重存每一个资产；升级后的第一次编译会重建一切一次
> （build key 格式 `DSK4`）。参见 [缓存](caching.md#what-is-hashed)。

**代价。** 历史中二进制文件的反复变动，以及靠重新编译——而不是靠二选一——解决的合并冲突：
任取一个版本，对源码运行 `compile`，提交结果。

<a id="which-one"></a>

## 选哪个

| | 做法 A —— 不入库 | 做法 B —— 入库 |
| :-- | :-- | :-- |
| 仓库体积 | 只有源码 | 每次重新生成都会长大 |
| 全新检出后打开 | 要先跑一次编译步骤 | 立刻可以 |
| 没有插件工具链的美术 | 构建不了 —— 不可行 | 没问题 |
| 过期资产 | 不可能出现：它总是由检出的内容构建 | 可能；上面的 CI 检查会抓到 |
| 升级插件或引擎 | 没有东西要提交 | 一次很大的重新生成提交 |
| 手改生成资产 | 在被采用之前只在本地 | 被纳入版本管理，并被[标记为已分歧](divergence.md) |

做法 A 适合人人都开着带插件的编辑器的团队；做法 B 适合有人只消费内容的团队。
不管选哪个，ThinCustom 后端都让问题变小：Ephemeral 材质没有文件，需要做决定的
只剩它的函数、它的实例，以及被 Materialize 过的那些。

<a id="notes"></a>

## 说明

- **`DShader/.dreampass/` 在两种做法下都是源码。** 它装着 `.usf` 文件的快照
  —— `.dsp` 管线的 HLSL pass 用的就是这些文件 —— 以及全局 shader 据以构建的 slot 注册表
  （[HLSL pass](../../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)）。那些 shader 是从快照而不是从正在编辑的文件编译的，
  所以队友的编辑器和 cook 跑的就是快照：一份缺少这些快照的检出会让每个 slot 都跑到空桩上，
  直到它的 `.dsp` 文件被重新编译。请把这个文件夹和产生它的 `.dsp` 和 `.usf` 改动一起提交；它里面的冲突靠任取一边、
  再重新编译 `.dsp` 文件解决，永远不要手改。
- **`Intermediate/DreamShader/GeneratedShaders/*.ush`** 是 1.x 为 `Function` 块写的东西；2.0.0 起没有
  编译再往那里写（[生成的 HLSL](generated-hlsl.md#location)）。永远不要提交它；
  Unreal 自带的忽略规则已经覆盖 `Intermediate/`。
- **`Saved/DreamShader/`** 装着 dump、索引、迁移源文件的备份和 bridge 的状态。
  自带的忽略规则也覆盖 `Saved/`；清理之前值得看一眼的只有 `Saved/DreamShader/Migrated/`，
  因为 [`dsc migrate`](../tools/migrate.md) 把 1.x 原件挪到那里。
- **自带 DreamShader 源文件的插件**对自己的 `Content/` 面临同样的选择。发布
  生成资产（做法 B）能让工程在不编译插件的情况下使用它；`-All` 也覆盖
  插件源码根，所以一份清单两边都够用。
- **给源文件改名会挪动它的资产。** 在做法 B 下，把改名、新资产和旧资产的删除
  放进同一个提交；[资产改名同步](../../tools/asset-rename-sync.md)会让引用
  指向新路径。
- **`list-generated` 列的是源码会构建的东西，不是磁盘上躺着的东西。** 源文件被删掉的生成资产
  是孤儿，不在清单里；Material Content Browser 会显示它们。

<a id="see-also"></a>

## 另见

- [资产路径](asset-paths.md) —— 产物落在哪里，这决定了你需要多少行忽略规则
- [Ephemeral 材质](ephemeral.md) —— 唯一可能没有文件的产物种类
- [缓存](caching.md) —— 让未变动的编译成为空操作的 build key
- [分歧](divergence.md) —— 对生成资产的手改，以及采用
- [Commandlet](../tools/commandlet.md) —— `list-generated`、`compile`、`check`
