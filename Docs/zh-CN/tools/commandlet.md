---
title: 命令行
description: -run=DreamShader —— 每个命令和开关、路径解析、退出码，以及为什么它写真资产而编辑器不写。
---

`-run=DreamShader` 是无头入口。它把 DreamShader 源文件编译成**落盘**资产，也把已有的材质资产反编译回源文件 ——
没有编辑器 UI，也没有 bridge 在跑。

| 项目 | 值 |
| :-- | :-- |
| 类 | `UDreamShaderCommandlet`，在 `DreamShaderEditor` 模块里 |
| 调用 | `-run=DreamShader`，等价写法 `-run=DreamShaderCommandlet` |
| 命令 | `compile`、`generate`、`decompile`、`export`、`dump-graph` （自 1.9.0 起）；`migrate`、`check`、`dump-ir`、`dump-layout`、`index`、`export-catalog`、`fmt`、`list-generated` （自 2.0.0 起）；`pass-registry` （自 2.1.0 起） |
| 退出码 | `0` 成功，`1` 失败 |
| 日志分类 | `LogDreamShader` |
| Commandlet 标志 | `IsClient = false`、`IsEditor = true`、`IsServer = false`、`LogToConsole = true` |

**警告：**

**commandlet 写的是真 package。** `compile` 要求每个 ThinCustom 产物都 Materialized，所以 `/Game/…` 的 `.uasset` 会被创建并保存到磁盘。
交互式编辑器把 ThinCustom 产物保持为 Ephemeral（只在内存里），除非它已经存过盘；`Graph` 材质和材质函数则两边都会存盘。
这正是在 CI 里把整个工程的源文件落盘的正确方式，同时也意味着一次 commandlet 运行可能留下磁盘资产，取代编辑器里的 Ephemeral 材质 ——
*Tools ▸ DreamShader ▸ Make Ephemeral* 可以把它们清掉。见[内存材质](../generation/ephemeral.md)。

<a id="语法概要"></a>

## 语法概要

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```text
UnrealEditor-Cmd.exe <project>.uproject -run=DreamShader <command> [<option>…]

<command> ::= { compile | generate | decompile | export | migrate | dump-graph
              | check | dump-ir | dump-layout | index | export-catalog | fmt | list-generated
              | pass-registry }

-run=DreamShader { compile | generate } { -Source=<path> | -File=<path> | -All } [-Force]
                                        [-Define=<NAME>[=<value>]]…
-run=DreamShader { decompile | export } { -Asset=<object-path> | -SourceFile=<path> }
                                        [{ -Out | -Output }=<path>] [-Format={ Dss | Legacy | Auto }]
                                        [-KeepAssetPath] [-Readable] [-DiagnosticsOut=<file>]
-run=DreamShader migrate { -Source=<path> | -All | -Root=<source root> }
                                        [-Check] [-DryRun] [-Out=<dir>] [-NoBackup]
-run=DreamShader dump-graph { -Source=<path> | -File=<path> | -All } [{ -Out | -Output }=<dir>]
                                        [-Define=<NAME>[=<value>]]…
-run=DreamShader check { -Source=<path> | -All } [-Shaders] [-Platform=<list>] [-Quality=<list>]
                                        [-Timeout=<seconds>] [-DiagnosticsOut=<file>]
-run=DreamShader dump-ir { -Source=<path> | -All } [-Out=<dir>] [-Json]
-run=DreamShader dump-layout { -Source=<path> | -All } [-Style={ Blocks | SourceBands | Layered | All }]
                                        [-Out=<dir>] [-Json]
-run=DreamShader index { -Source=<path> | -All } [-Out=<dir>]
-run=DreamShader export-catalog [-Out=<file>]
-run=DreamShader fmt { -Source=<path> | -All } [-Check] [-Out=<dir>]
-run=DreamShader list-generated { -Source=<path> | -All } [-As={ Packages | Files | GitIgnore | Json }]
                                        [-Out=<file>] [-IncludeEphemeral]
-run=DreamShader pass-registry [-Gc | -Rebuild]
```

`compile`、`dump-graph`、`check`、`dump-ir`、`dump-layout`、`index` 和 `list-generated` 接受**任意可编译源文件**：`.dss`、`.dsi`、
`.dsp`、`.dsm` 和 `.dsf` （自 2.0.0 起）。编译器只有一个；1.x 源文件由 legacy 前端读，`.dsh` 头文件通过 include 它的源文件被编译。
`fmt` 只接受 2.0 文本。各命令对 [Custom Pass 管线](../language-v2/passes.md)做什么，见 [`.dsp` 源文件](#dsp-源文件)。

<a id="命令"></a>

## 命令

命令名是第一个**裸**（不以 `-` 开头）参数。没有裸参数时，改为查 `Command=<name>` 参数。
匹配大小写不敏感，首尾空白会被 trim 掉。

| 写法 | 等价于 | 效果 |
| :-- | :-- | :-- |
| `compile` | — | 把一个源文件、或全部工程源文件，编译成资产 |
| `generate` | `compile` | 完全相同；另一种写法 |
| `decompile` | — | 把材质、函数、layer、blend 或材质实例导出成源文件：默认 2.0 文本（`.dss`，实例是 `.dsi`），`-Format=Legacy` 或以 `.dsm` / `.dsf` 结尾的 `-Out` 写 1.x 文本 —— 见[反编译导出](decompiler.md)。Custom Pass 管线写成 `.dsp`，没有 1.x 文本 （自 2.1.0 起） |
| `export` | `decompile` | 完全相同；另一种写法 |
| `migrate` （自 2.0.0 起） | — | 把 1.x 源文件改写成 `.dss`，每次改写先证明 —— 见[从 1.x 迁移](migrate.md) |
| `check` （自 2.0.0 起） | — | 编到 IR 校验为止，**不写任何资产**；`-Shaders` 构建产物并把 HLSL 错误映射回源码行 |
| `dump-ir` （自 2.0.0 起） | — | 把降级后的图 IR 写成文本，`-Json` 同时写 JSON |
| `dump-layout` （自 2.0.0 起） | — | 把每个产物的 2.0 图布局画成 SVG，不构建任何东西 |
| `index` （自 2.0.0 起） | — | 写出语言服务读取的符号索引；编辑器每次编译之后也写同一个文件 |
| `export-catalog` （自 2.0.0 起） | — | 把内建节点目录写成 JSON，工具不开编辑器也能绑定 `UE.*`；编辑器加载完成后也写同一个文件 |
| `fmt` （自 2.0.0 起） | `format` | 按打印器的排版重写 2.0 源文件；`-Check` 只报告不写 |
| `list-generated` （自 2.0.0 起） | — | 不构建任何东西，列出源码会生成的全部资产 |
| `pass-registry` （自 2.1.0 起） | `passregistry` | 列出 Custom Pass 的 HLSL slot；`-Gc` 释放死掉的，`-Rebuild` 重建注册表 |
| `dump-graph` （自 1.9.0 起） | `dumpgraph` | 给每个源文件生成的图写一份规范化的 JSON 指纹 |

其他任何值都会失败于 `Unknown DreamShader command '{Command}'.` 加用法横幅，并以 `1` 退出。

**警告：**

第一个裸 token 会被**无条件**当成命令名，在做任何校验之前。把一个不带前导短横的选项写在*最前面* ——
`-run=DreamShader Source=X compile` —— 会把 `Source=X` 吃成命令名，得到
`Unknown DreamShader command 'Source=X'.` 请让命令保持为第一个裸参数。

<a id="compile--generate"></a>

## `compile` / `generate`

| 选项 | 别名 | 类型 | 必需 | 默认 | 含义 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `-Source=<path>` | `-File=<path>` | 字符串 | 三选一 | — | 只编译一个源文件 |
| `-All` | — | 标志 | 三选一 | 关 | 编译全部工程 DreamShader 源文件 |
| `-Force` | — | 标志 | 否 | 关 | 绕过 source hash 短路，无条件重新生成 |
| `-Define=<NAME>[=<value>]` （自 1.9.0 起） | `-D=<NAME>[=<value>]` | 字符串，可重复 | 否 | — | 给本次运行加一个[预处理器 define](../language/lexical.md#预处理器) |

优先级：先查 `-Source`，再查 `-File`；两者都没取到值时才看 `-All`。`-Source` 和 `-All` 同时给出时，
静默地只编译那一个文件。三者都没有时，用法横幅以 `Error` 记录，运行以 `1` 退出。

`-Force` 只绕过 [source hash 短路](../generation/regeneration.md#什么时候会跳过重新生成)，它不删除任何东西，也过不了
[手改检测](../generation/regeneration.md#手改检测)。

<a id="-source-路径解析"></a>

### `-Source` 路径解析

按下列顺序尝试，第一个适用的胜出。

| 顺序 | 条件 | 结果 |
| :-- | :-- | :-- |
| 1 | 值为空，或者是**绝对**路径 | 按给定值归一化 |
| 2 | `<SourceDirectory>/<value>` **存在** | 用这个路径 |
| 3 | `<ProjectDir>/<value>` **存在** | 用这个路径 |
| 4 | 其他 | 按给定值归一化 —— 随后会卡在扩展名守卫或编译上 |

`<SourceDirectory>` 是 *Source Directory* 项目设置，默认 `<Project>/DShader`。所以相对值先按
DreamShader 源树解析，再按工程目录解析。

<a id="-all-的发现与排序"></a>

### `-All` 的发现与排序

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 递归收集每个源码根下的 `*.dsm`、`*.dsh`、`*.dsf`、`*.dss`、`*.dsi` 和 `*.dsp` —— `<SourceDirectory>`，以及每个带 `DShader` 文件夹的插件的那个文件夹 |
| 2 | 丢掉每个源码根 `Packages` 文件夹下的**一切** |
| 3 | 丢掉 `.dsh` 头文件 —— 它们不生成资产，通过 include 它们的源文件被编译 |
| 4 | 排序：`.dsf` 函数文件在前（rank 0），其余每种在后（rank 1）；同 rank 按大小写不敏感的路径比较 |
| 5 | **分轮编译** （自 2.1.2 起）：失败的源文件在本轮还建出了别的东西时被留下、下一轮重试；一轮什么新东西都没建出来时，仍然失败的才报告 |

调用别的源文件所生成函数的材质会加载那个函数资产，所以资产必须先存在。第 4 步的排序在一轮里就覆盖了常见情况 —— `.dsm` 调 `.dsf` ——
但覆盖不了函数调函数、或者别的源文件调用一个 `.dss` 导出函数；第 5 步覆盖所有形状，干净的工作区也一样。一直失败的源文件 —— 真错误、
没人生成的依赖、或者成环 —— 每一轮最多多试一次，报告用的是最后一次的消息，排在所有成功之后。cook 跑的是同样的步骤
（见[内存材质](../generation/ephemeral.md#cook-行为)）。第 2 步对住在 package 里的 `.dsf` 有一处锋利边缘，见
[Package](packages.md#源文件枚举)。`.dsp` 不需要自己的 rank：它的 pass 引用的材质缺失或比源文件旧时，`.dsp` 自己的编译会先编那个材质。

<a id="-define"></a>

### `-Define`

（自 1.9.0 起）

每出现一次，就往本次运行编译所用的 [define 表](../language/lexical.md#预处理器)里加一项。可以重复 —— 后出现的是追加名字，不是替换整组。

| 写法 | 定义 |
| :-- | :-- |
| `-Define=FOO=1` | `FOO` 为 `1` |
| `-Define=PROFILE=cinematic` | `PROFILE` 为字符串 `cinematic` |
| `-Define="NOTE=two words"` | `NOTE` 为 `two words` —— 引号包住整个参数，而不只是值 |
| `-Define=CI_BUILD` | `CI_BUILD` 为**空**值：一个裸标记，`defined(CI_BUILD)` 为 `1`，`#if CI_BUILD` 为真 |
| `-D=FOO=1` | 同 `-Define=FOO=1` —— `-D` 是短写法 |

名字是值里**第一个** `=` 之前的部分，所以值本身可以含 `=`。名字区分大小写。命令行是优先级最高的一层 define，所以它覆盖项目设置里的
*Preprocessor Defines* 和任何模块在 C++ 里注册的东西 —— 唯一的例外：以 `DS_` 开头的名字留给内建常量，会被丢掉并给出警告。

**提示：**

define 是[构建键](../generation/regeneration.md#构建键)的一部分，所以换一组 `-Define` 跑，会**不加** `-Force` 就重建读了变动名字的源文件，
没读它的仍然跳过。这正是按配置跑 CI 负担得起的原因：每组 define 跑一次，各自只重建自己开关碰到的东西。

<a id="逐文件守卫"></a>

### 逐文件守卫

编译列表里的每个文件在编译前都会被检查。不是 DreamShader 源文件的路径，或者*是* `.dsh` 头文件的路径，
会记 `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}`，把整次运行标记为失败，
然后**继续**处理剩下的文件。所以一个坏文件不会阻止其余文件编译 —— 但进程仍然以 `1` 退出。

<a id="结果消息"></a>

### 结果消息

每个文件的编译结果原样记录：成功记 `Display`，失败记 `Error`。一个文件产出多个资产时，消息用换行连接。

| 消息 | 结果 |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {SourceFile}.` | 构建出一个产物。`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline` （自 2.1.0 起） —— 不分源语言、不分 backend；ThinCustom 材质也是 `Material`，路径是实例的 （自 2.0.0 起） |
| `Generated RenderTarget {ObjectPath} from {SourceFile}.` | 导出 buffer 的 render target，随它的管线一起建好并保存 （自 2.1.0 起）；跟在管线那一行后面 |
| `Skipped {AssetPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` | 哈希一致 —— 传 `-Force` 强制重新生成 |
| `Compiled {SourceFile}; it declares no material and no exported function, so no asset was written.` | 成功，没有东西要写 —— 只有 helper、`VirtualFunction` 或 `GraphFunction` 声明的文件 （自 2.0.0 起） |
| `Warnings:` 后跟每行一条诊断 | 编译有警告时追加在成功消息后面 |
| 每行一条诊断，`<file>(<line>,<col>): DSHnnnn: <message>` | **失败**：第一个错误在最前，然后是其他错误、警告和 info。见[错误速查](../diagnostics/index.md) |

1.x 的结果行 —— 单独的 thin-custom 措辞、helper include 那一行、"no assets to generate" 和 `.dsh` 失败、` (virtual)` 后缀 ——
2.0.0 起都不再写出。没有产物的文件算成功；`.dsh` 根本到不了编译器，被上面的逐文件守卫挡住。

<a id="decompile--export"></a>

## `decompile` / `export`

| 选项 | 别名 | 类型 | 必需 | 默认 | 含义 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `-Asset=<object-path>` | — | 字符串 | 二选一 | — | 要反编译的资产 |
| `-SourceFile=<path>` （自 2.0.0 起） | — | 字符串 | 二选一 | — | 把这个源文件构建的每个资产反编译进一个文件 |
| `-Out=<path>` | `-Output=<path>` | 字符串 | 否 | 计算得到 | 目标文件 |
| `-Format=<Dss\|Legacy\|Auto>` （自 2.0.0 起） | — | 枚举 | 否 | `Auto` | `Auto` 由 `-Out` 的扩展名决定：`.dsm` / `.dsf` 是 1.x 文本，其他都是 2.0。与扩展名矛盾的格式是 `DSH9085` |
| `-KeepAssetPath` （自 2.0.0 起） | — | 标志 | 否 | 关 | 输出文件本来会指向别的路径时，写一行 `/// @name` 保留资产自己的路径 |
| `-Readable` （自 2.0.0 起） | — | 标志 | 否 | 关 | 优先 HLSL 语法糖而不是精确到类的节点调用（重建的图节点类可能不同），并把 Substrate 语法糖读回来 |
| `-DiagnosticsOut=<file>` （自 2.0.0 起） | — | 字符串 | 否 | — | 把反编译的诊断写成 JSON（schema `dreamshader-diagnostics`） |

<a id="资产路径归一化"></a>

### 资产路径归一化

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 每个 `\` 变成 `/` |
| 2 | 若路径以 `/` 开头且**不含** `.`，追加短名：`/Game/Path/Asset` → `/Game/Path/Asset.Asset` |

先加载归一化后的路径。如果失败**且**归一化确实改变了字符串，再原样重试去掉引号的原始输入。

<a id="支持的资产类"></a>

### 支持的资产类

| 类 | 产出 | 默认目标 |
| :-- | :-- | :-- |
| `UMaterial` | `.dss`；`-Format=Legacy` 时 `.dsm` | `<SourceDirectory>/Decompiled/Materials/<package path>.dss`（`.dsm`） |
| `UMaterialFunction` | `.dss`；`-Format=Legacy` 时 `.dsf` | `<SourceDirectory>/Decompiled/Functions/<package path>.dss`（`.dsf`） |
| `UMaterialFunctionMaterialLayer` | `.dss`；`-Format=Legacy` 时 `.dsf` | `<SourceDirectory>/Decompiled/Layers/<package path>.dss`（`.dsf`） |
| `UMaterialFunctionMaterialLayerBlend` | `.dss`；`-Format=Legacy` 时 `.dsf` | `<SourceDirectory>/Decompiled/LayerBlends/<package path>.dss`（`.dsf`） |
| `UMaterialInstanceConstant` （自 2.0.0 起） | `.dsi` —— 只有 2.0 文本。生成的 ThinCustom 实例就是它代表的材质，回来的是 `Materials/` 下的 `.dss` | `<SourceDirectory>/Decompiled/Instances/<package path>.dsi` |
| `UDreamPassPipeline` （自 2.1.0 起） | `.dsp` —— 只有 2.0 文本 | `<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp` |
| 其他任何类 | — | 报错 |

路径段会被净化：控制字符和 `< > : " / \ \| ? *` 变成 `_`；空目录段变成 `Folder<N>`，空资产段变成 `Asset<N>`。
输出是 UTF-8 无 BOM。`-Out` 完全绕过计算出来的目标，必要时创建目录。导出器的完整行为见[反编译导出](decompiler.md)。

<a id="20-的命令"></a>

## 2.0 的命令

（自 2.0.0 起） 下面每个命令都以一行汇总结束，结尾是 `RESULT=OK` 或 `RESULT=FAILED` —— 因为工程自己的 commandlet 退出码
可能因为与 DreamShader 无关的原因而非零。

| 命令 | 作用 |
| :-- | :-- |
| `check` | 编到 IR 校验为止，**不写任何资产** —— CI 的闸门。`-Shaders` 会构建并保存产物、再编它们的 shader，于是 `/// @custom` 函数体里的 HLSL 错误会在这里失败，并映射回源码行；`-Platform`、`-Quality`、`-Timeout` 控制编哪些 shader |
| `migrate` | 把 1.x 源文件改写成 `.dss`，每次改写先证明；`-Check` 什么都不写 —— 见[从 1.x 迁移](migrate.md) |
| `fmt` | 按打印器的排版重写 2.0 源文件（`.dss`、`.dsi`、`.dsp`，以及没有 1.x 声明的 `.dsh`）。担保不了的文件一律不写 —— 格式化后的文本必须能解析、解析成同样的声明、注释一条不少、再格式化一次不变，否则是 `DSH9044`（格式化器的错，不是文件的）。还带 1.x 声明的文件（`DSH9042`，info）和在 `/// @custom` 函数体外用了 `#if` 的文件（`DSH9043`，info）保持原样。`-All` 只取工程自己的可写源码根；`-Check` 什么都不写、有文件会变就报 `DSH9046` 并失败；`-Out=<dir>` 把格式化后的副本写到别处 |
| `list-generated` | 不构建任何东西，列出源码会生成的全部资产 —— 见[下面](#list-generated) |
| `dump-layout` | 把每个产物的 IR 图布局 —— `Blocks`、`SourceBands`、`Layered` —— 画成 SVG，不构建任何东西；`-Style` 指定其中一种（`Classic` 是 `DSH9041`：它作用于成品图，没法从 IR 画），`-Json` 同时写出坐标；默认输出 `<Project>/Saved/DreamShader/Layout` |
| `dump-ir` · `index` · `export-catalog` | 降级后的 IR · 语言服务读取的符号索引 · JSON 形式的内建节点目录。后两个编辑器会自己写：索引在每次编译之后，目录在加载完成之后 |
| `dump-graph` （自 1.9.0 起） | 每个生成资产一份规范化 JSON —— 节点类、属性、连线、引脚顺序 —— 用来 diff 两次构建（`git diff --no-index`）。**从不写资产**：磁盘上已有的资产按现状导出而不是重建，所以源码可能变过时先跑 `compile -All -Force`。默认输出 `<Project>/Saved/DreamShader/GraphBaseline` |
| `pass-registry` （自 2.1.0 起） | Custom Pass 的 HLSL slot 注册表（`<DShader>/.dreampass/`）：不带参数只列出每个 slot 和它的状态；`-Gc` 释放管线或 pass 已经没了的 slot（删掉或改名 `.dsp` 之后跑）；`-Rebuild` 重新编译每个 `.dsp` 并从头重写注册表文件（`Registry.json` 合并冲突之后跑）。见 [Custom Pass（.dsp）](../language-v2/passes.md) |

<a id="list-generated"></a>

### `list-generated`

（自 2.0.0 起） 列出源码会构建的每个资产 —— 前端、绑定器和目标规则，到此为止：不构建、不加载、不保存任何资产。
ignore 规则和 P4 typemap 就是照它写的；做法见[版本控制](../generation/source-control.md)。

| 选项 | 默认 | 含义 |
| :-- | :-- | :-- |
| `-Source=<path>` / `-All` | 二选一必需 | 哪些源文件；`-All` 也覆盖插件源码根 |
| `-As=` | `Packages` | `Packages` —— 长 package 名；`Files` —— 相对工程目录的路径；`GitIgnore` —— 同样的路径加 `/` 锚定、带一行头注释；`Json` —— 全部字段（schema `dreamshader-generated-assets`，version 1） |
| `-Out=<file>` | 日志 | 把清单写到这里。脚本应该总是传它：日志是给人看的 |
| `-IncludeEphemeral` | 关 | 也列出此刻是 Ephemeral 的 ThinCustom 材质；它没有文件，所以写 ignore 规则用的清单里没有它 |

`.dsp` 贡献两种行 （自 2.1.0 起）：它的管线（kind `PassPipeline`），以及每个导出 buffer 的 render target（kind `PassExportTarget`）。
编不过的源文件仍然贡献失败之前已经确定的产物，整次运行以 `RESULT=FAILED` 结束 —— 清单永远不会悄悄变短。工程目录之外的资产
（引擎级插件的内容）没有工程相对路径；`Files` 和 `GitIgnore` 会说略过了几个（`DSH9049`）。未知的 `-As` 是 `DSH9048`；
清单写不出去是 `DSH9047`。

<a id="dsp-源文件"></a>

### `.dsp` 源文件

（自 2.1.0 起） [Custom Pass 管线](../language-v2/passes.md)和其他源文件一样是可编译源文件，只有一个产物 —— `UDreamPassPipeline` ——
而且没有图。

| 命令 | 对 `.dsp` 做什么 |
| :-- | :-- |
| `compile` | 构建管线和每个导出 buffer 的 render target，并给每个 HLSL pass 分配 slot：规划、预检，把快照和注册表写到 `<DShader>/.dreampass/`。它的 pass 引用的材质缺失或过旧时先编那个材质。commandlet 不重编任何 shader；下次编辑器启动会编改过的注册表。UE 5.8 以下拒绝编译（`DSH8300`） |
| `check` | 读 pass 引用的每个材质和 `.usf`，做绑定器的每一项检查。不先编译任何东西，不构建任何东西，不编 HLSL |
| `check -Shaders` | 像 `compile` 一样构建并保存管线，然后在 slot 里预检每个 HLSL pass —— 不管变没变；`-Platform` 指定要检查的 shader format |
| `dump-ir` · `index` · `fmt` · `list-generated` · `dump-graph` · `decompile` | 都接受 `.dsp`；`dump-layout` 对它什么都不写 —— 管线没有图可布局，不算错误 |

<a id="参数语法"></a>

## 参数语法

所有命令共用，并由自动化测试 `DreamShader.Commandlet.Args.SplitAndGet` 和 `DreamShader.Commandlet.Args.FlagValues` 钉住。

| 规则 | 行为 |
| :-- | :-- |
| 键归一化 | trim，然后剥掉**所有**前导 `-`，再 trim。`-Source`、`--Source` 和 `Source` 是同一个键 |
| 名称匹配 | 大小写不敏感 —— `-source`、`-SOURCE`、`-Source` 等价 |
| 值归一化 | trim，剥掉一层包裹引号，再 trim |
| 赋值切分 | 在**第一个** `=` 处切；因此值里可以含 `=` |
| 无短横赋值 | 凡是能写 `-Key=Value` 的地方都接受裸 `Key=Value` |
| 查找顺序 | 先解析出的参数表，再开关列表，再裸 token 列表；第一个命中胜出 |
| 空值 | `-Source=` 解析成空串，被当作**不存在** |
| 缺失键 | 不存在 |

<a id="布尔标志"></a>

### 布尔标志

标志可以裸写，也可以带值。值在上面的值归一化之后大小写不敏感地匹配。

| 写法 | 结果 |
| :-- | :-- |
| `-Force` | 开 |
| `-Force=1` / `-Force=true` / `-Force=yes` / `-Force=on` | 开 |
| `-Force=0` / `-Force=false` / `-Force=no` / `-Force=off` | 关 |
| *（没写）* | 关 |
| `-Force=<其他任何值>`，包括空的 `-Force=` | **错误** `DSH9110` （自 2.1.1 起） |

错误不会去猜你的意思。命令记 `DSH9110`、**什么都不做**并以 `1` 退出：`migrate -Check=banana` 既不检查也不写，`-All=never` 什么都不选。
每个命令都在碰任何文件之前读完自己的所有标志。命令不读的标志和任何未知开关一样被忽略：`dump-graph -Force=banana` 照跑，因为
`dump-graph` 从不读 `-Force`。

**警告：**

**2.1.0 及更早**，带值写的标志会被读成**不存在**：引擎的解析器把每个 `-Name=value` 从开关列表挪进了参数表，而命令只在开关列表里找。
`-Force=true` 不会强制，`migrate -Source=X -Check=true` 照样写出迁移结果。本页以前还说过相反的话 —— 无法识别的值算开。

<a id="退出码"></a>

## 退出码

| 码 | 条件 |
| :-- | :-- |
| `0` | 所选命令报告成功 |
| `0` | `compile -All` 或 `dump-graph -All` 解析出**空**的源文件列表 —— 记 `Warning`，视为成功 |
| `0` | `pass-registry` 列出了注册表 —— 不管列出了什么；它的警告不让运行失败 |
| `1` | 既没有命令 token 也没有 `Command=` 值 |
| `1` | 未知命令名 |
| `1` | `compile` / `dump-graph` 但 `-Source` / `-File` / `-All` 一个都没给 |
| `1` | `compile` / `dump-graph` 期间任何逐文件守卫失败、编译失败或写出失败 |
| `1` | `decompile` 没有 `-Asset` / `-SourceFile`，或加载 / 反编译 / 写入失败 |
| `1` | `pass-registry` 读不了 `Registry.json`、写不了注册表文件，或 `-Rebuild` 里有 `.dsp` 没编过 |
| `1` | 任何命令：它读的某个标志的值既不是开也不是关（`DSH9110`）；什么都没跑 |

<a id="powershell-驱动-dscps1"></a>

## PowerShell 驱动 `dsc.ps1`

插件的 `.skill/dsc.ps1` 包装了 `UnrealEditor-Cmd.exe <project> -run=DreamShader …`：从 `.uproject` 的 `EngineAssociation` 找到引擎，
从目标文件往上找工程，把编译报告完整地打印出来（一份报告是一条多行日志消息，引擎只给第一行加前缀），以命令自己的退出码退出。
[版本控制](../generation/source-control.md)和[从 1.x 迁移](migrate.md)里的 `./dsc.ps1 …` 写法就是它。

```powershell
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check   DShader/UI/M_Panel.dss
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew
```

**`-CleanNew`** 删掉这次运行新建的资产，用于反复试编时不在 `Content/` 里留东西。（自 2.1.2 起） 起它只删 commandlet
**自己保存的**东西：commandlet 把它保存的每个 package 及其 MD5 记进一份清单（`Saved/DreamShader/AssetWrites/<run-id>.json`），
驱动只删这份清单点了名、且内容没变过的新文件 —— 编辑器同时导入的资产、运行前就存在的文件、之后又被改过的文件都留着。
清单缺失、无效或过期，或者某个 Content 文件夹列不出来，就什么都不删，并让本来成功的运行以 `1` 退出。升级驱动时要重新构建插件：
旧的 commandlet 不写这份清单。（2.1.1 之前它问 git，在不是仓库的工程里从不删任何东西，在是仓库的工程里会删任何未跟踪的资产。）

**警告：**

`compile -All` 会构建每个源码根 —— 工程的和**每个启用插件的** `DShader/` —— 到对应的 `Content/` 里，`-CleanNew` 从不删运行前已存在的东西。
工作时编单个文件；`-All` 留给 `check`。

<a id="哪些东西不会跑"></a>

## 哪些东西不会跑

- **commandlet 里 editor bridge 从不启动。** 编辑器模块一检测到自己在 commandlet 进程里就从启动流程返回，
  所以没有源目录 watcher、没有保存即编译、端口 `17864` 上没有 WebSocket 服务、没有 `diagnostics.json` 写入、
  没有 `bridge.db`，也没有菜单注册。
- 唯一的例外是 **cook** commandlet，它会装一个 post-engine-init 钩子。只在 cook director 上 ——
  即 `-run=` 含 `Cook` 且不带 `-cookworker` 的进程 —— DreamShader 会在正式 cook 之前把每个工程源文件落成持久资产，
  顺序和 `compile -All` 相同（含分轮重试）。那里的生成失败是 `Fatal`，会中止整个 cook ——
  除非源文件属于一个本工程不往里写的引擎或商城插件 （自 2.1.2 起），那样的失败记成
  `[Cook] Not this project's to build (plugin '<Name>')`，cook 继续。见[内存材质](../generation/ephemeral.md#cook-行为)。
- **`.dsp` 的编译会写到 `Content/` 之外** （自 2.1.0 起）：`<DShader>/.dreampass/` 下的 HLSL slot 注册表和快照，它们是要提交的源码 ——
  见[版本控制](../generation/source-control.md#说明)。commandlet 写它们但不重编 shader；编辑器在它们变化时立刻重编 slot shader。
- 给*非* commandlet 的自动化运行（例如 `-ExecCmds="Automation RunTests …"`）加上
  `-NoDreamShaderEditorBridge`，同样能在那边压掉 bridge —— 见
  [编辑器工具](editor-integration.md#关闭整套集成)。

<a id="诊断"></a>

## 诊断

所有消息都进 `LogDreamShader`。运行期替换写作 `{Placeholder}`。

| 消息 | 严重度 | 原因 |
| :-- | :-- | :-- |
| *（用法横幅）* | Error | 既没有命令 token 也没有 `Command=` 值 |
| `Unknown DreamShader command '{Command}'.` + 用法横幅 | Error | 命令不是[命令](#命令)里的任何写法 |
| *（用法横幅）* | Error | `compile` 但 `-Source` / `-File` 和 `-All` 都没给 |
| `DreamShader commandlet found no source files to compile.` | **Warning** | 解析出的源文件列表为空；运行仍以 `0` 退出 |
| `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}` | Error | 该文件不是 DreamShader 源文件，或者是 `.dsh` 头文件 |
| *（编译结果消息）* | Display / Error | 逐文件结果 —— 见[结果消息](#结果消息) |
| *（用法横幅）* | Error | `decompile` 既没有 `-Asset` 也没有 `-SourceFile` |
| `DreamShader decompile: -Format takes Dss, Legacy or Auto; got '{Value}'.` | Error | 未知的 `-Format` |
| `DreamShader could not load asset '{AssetPath}'.` | Error | 归一化路径和原始路径都加载失败 |
| `DreamShader decompile: {Error}` | Error | `-SourceFile` 的源文件解析不了，或它构建的东西还不存在 —— 先编译它 |
| `DreamShader failed to decompile '{LoadPath}': {Error}` | Error | 反编译器报告失败 |
| `DreamShader failed to write decompiled source '{Path}'.` | Error | 文件写入失败 |
| `DreamShader decompiled '{LoadPath}' to '{OutputPath}' ({Format}).` | Display | 成功；`{Format}` 是 `dss` 或 `legacy` |
| `Dumped {Kind} {AssetPath} ({N} nodes) to {File}.` | Display | `dump-graph` 导出了一个资产；资产已有文件时追加 ` (read from disk; not regenerated)` |
| `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them …` | **Warning** | `dump-graph` 从不写资产 —— 源码变过就先 `compile -All -Force` |
| `DSH9110: '-{Flag}={Value}' is neither on nor off. A flag takes true, 1, yes or on, or false, 0, no or off, and '-{Flag}' alone is on; the command did nothing.` | Error | 任何命令：它读的某个标志值不对，包括空值；见[布尔标志](#布尔标志) |

不属于任何文件的命令错误 —— `dump-layout -Style=Bogus`（`DSH9041`）、`list-generated -As=Bogus`（`DSH9048`）、未知的
`check -Platform`、`pass-registry` 失败 —— 直接记成 `DSHnnnn: ...`，不再带一个编出来的位置 （自 2.1.1 起）。

用法横幅，原文：

```text
Usage:
  -run=DreamShader compile -Source="C:/Project/DShader/File.dsm" [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader compile -All [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader decompile { -Asset="/Game/Path/Asset.Asset" | -SourceFile="C:/Project/DShader/File.dss" } [-Out=<file>] [-Format=Dss|Legacy|Auto] [-KeepAssetPath] [-Readable] [-DiagnosticsOut=<file>]
  -run=DreamShader migrate { -Source="C:/Project/DShader/File.dsm" | -All | -Root=<source root> } [-Check] [-DryRun] [-Out=<dir>] [-NoBackup]
  -run=DreamShader dump-graph { -Source="C:/Project/DShader/File.dsm" | -All } [-Out="C:/Project/Saved/DreamShader/GraphBaseline"]
  -run=DreamShader check { -Source="C:/Project/DShader/File.dss" | -All } [-Shaders] [-Platform=SM6,SM5] [-Quality=High] [-Timeout=120] [-DiagnosticsOut=<file>]
  -run=DreamShader dump-ir { -Source="C:/Project/DShader/File.dss" | -All } [-Out=<dir>] [-Json]
  -run=DreamShader dump-layout { -Source="C:/Project/DShader/File.dss" | -All } [-Style=Blocks|SourceBands|Layered|All] [-Out=<dir>] [-Json]
  -run=DreamShader index { -Source="C:/Project/DShader/File.dss" | -All } [-Out=<dir>]
  -run=DreamShader export-catalog [-Out=<file>]
  -run=DreamShader fmt { -Source="C:/Project/DShader/File.dss" | -All } [-Check] [-Out=<dir>]
  -run=DreamShader list-generated { -Source="C:/Project/DShader/File.dss" | -All } [-As=Packages|Files|GitIgnore|Json] [-Out=<file>] [-IncludeEphemeral]
  -run=DreamShader pass-registry [-Gc | -Rebuild]
```

（横幅下面还有几行说明各命令的英文文字，此处略去。）

<a id="示例"></a>

## 示例

编译单个源文件，绕过哈希短路：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -Source="C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

作为 CI 关卡，编译全部工程源文件 —— `.dsf` 在前，然后分轮：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -All -Force `
  -unattended -nopause -nosplash -stdout -log
```

整棵树编两遍，`#if` 的每个分支各一遍 —— 没被选中的分支从不解析，不这样就没有东西检查它：

```powershell
foreach ($Legacy in 0, 1) {
    & $UnrealEditorCmd $Project `
      -run=DreamShader compile -All -Force `
      -Define=MOONTOON_LEGACY_TOON=$Legacy `
      -unattended -nopause -nosplash -stdout -log
    if ($LASTEXITCODE -ne 0) { throw "DreamShader failed with MOONTOON_LEGACY_TOON=$Legacy" }
}
```

把已有材质反编译成 1.x 文本、写到指定路径（不写 `-Format=Legacy` 时，`.dsm` 扩展名的 `-Out` 也会选 1.x）：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader decompile -Asset="/Game/Materials/M_Existing" `
  -Out="C:/Projects/MyGame/DShader/Decompiled/Materials/M_Existing.dsm" `
  -unattended -nopause -nosplash -stdout -log
```

相对的 `-Source` 先按 `DShader/` 解析：

```powershell
-run=DreamShader compile -Source="Materials/M_Sample.dsm"
```

一次成功的两文件 `-All` 运行的控制台输出：

```text
LogDreamShader: Display: Generated MaterialFunction /Game/Functions/MF_Noise.MF_Noise from C:/Projects/MyGame/DShader/Functions/MF_Noise.dsf.
LogDreamShader: Display: Generated Material /Game/Materials/M_Sample.M_Sample from C:/Projects/MyGame/DShader/Materials/M_Sample.dsm.
```

<a id="继续阅读"></a>

## 继续阅读

- [内存材质](../generation/ephemeral.md) — Ephemeral 与 Materialized，以及 cook 行为。
  - [重新生成](../generation/regeneration.md) — -Force 绕过的那个 source hash 短路。
  - [反编译导出](decompiler.md) — decompile 命令驱动的那个导出器。
  - [版本控制](../generation/source-control.md) — list-generated 和两套入库做法。
  - [Package](packages.md) — 为什么 -All 跳过 DShader/Packages。
  - [编辑器工具](editor-integration.md) — commandlet 刻意不启动的那一整套。
