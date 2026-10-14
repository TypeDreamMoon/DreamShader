---
title: "命令行工具"
description: "命令行工具"
---

<a id="commandlet"></a>

# 命令行工具

> [DreamShader](../index.md) » [工具](index.md) » **命令行工具**

`-run=DreamShader` — 一个无头入口点，将 DreamShader 源文件编译为持久化资产，并将现有材质资产反编译回源文件。

| | |
| :-- | :-- |
| 类型 | `UCommandlet` 子类 — `UDreamShaderCommandlet`，位于 `DreamShaderEditor` 模块 |
| 调用 | `-run=DreamShader`（等价于 `-run=DreamShaderCommandlet`） |
| 命令 | `compile`、`generate`、`decompile`、`export`、`dump-graph` *（自 1.9.0 起）*；`migrate`、`check`、`dump-ir`、`dump-layout`、`index`、`export-catalog`、`fmt`、`list-generated` *（2.0）*；`pass-registry` *（2.1.0）* |
| 退出码 | `0` 成功，`1` 失败 |
| 日志分类 | `LogDreamShader` |

<a id="synopsis"></a>

## 语法概要

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

`compile`、`dump-graph`、`check`、`dump-ir`、`dump-layout`、`index` 和 `list-generated` 接受**所有可编译源文件**：`.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`。只有一个编译器；1.x 源文件由旧版前端读取，而 `.dsh` 头文件通过包含它的源文件被编译。`fmt` 接受 2.0 文本 — `.dss`、`.dsi`、`.dsp`，以及不再有 1.x 声明的 `.dsh`。各命令对 [Custom Pass 管线](../language-v2/passes.md)具体做什么，见 [`.dsp` 源文件](#dsp-sources)。

该类声明的命令行工具标志：`IsClient = false`、`IsEditor = true`、`IsServer = false`、`LogToConsole = true`。

<a id="commands"></a>

## 命令

所有命令都接受可选的 `-AssetWritesManifest=<unused-json-path>` 以及 `-AssetWritesRunId=<run-id>`。它们将本次命令行工具执行的成功 `.uasset` 保存记录（包括绝对文件名和 MD5 内容指纹）记录在一个带版本号的 JSON 清单中。输出路径不能已存在；若无法写入请求的清单，则以 `1` 退出。[PowerShell 驱动程序](../../../.skill/README.md) 使用这一来源记录和运行前快照来实现 `-CleanNew`，保留无关资产以及在记录的保存之后被修改的文件。清单不授权删除预先存在的资产，也不授权删除驱动程序 Content 根目录之外的文件。

命令名是第一个**裸**（非 `-`）参数。如果没有裸参数，则改为查询 `Command=<name>` 参数。匹配不区分大小写；首尾空白会被修剪。

| 写法 | 等价于 | 效果 |
| :-- | :-- | :-- |
| `compile` | — | 将一个源文件或所有项目源文件编译为资产 |
| `generate` | `compile` | 完全相同；另一种写法 |
| `decompile` | — | 将材质、函数、层、混合或材质实例导出为源文件：默认是 2.0 文本（`.dss`，实例则是 `.dsi`），使用 `-Format=Legacy` 或以 `.dsm` / `.dsf` 结尾的 `-Out` 则为 1.x 文本 — 见[反编译导出](decompiler.md)。Custom Pass 管线写为 `.dsp`，且没有 1.x 文本 *（2.1.0）* |
| `export` | `decompile` | 完全相同；另一种写法 |
| `migrate` *（2.0）* | — | 将 1.x 源文件改写成 `.dss`，每次改写前先进行证明 — 见[迁移](migrate.md) |
| `check` *（2.0）* | — | 编译到 IR 校验为止，不写出任何资产；`-Shaders` 构建产物并将 HLSL 错误映射回源文件行 |
| `dump-ir` *（2.0）* | — | 将源文件降级后的图 IR 写成文本，使用 `-Json` 时也写成 JSON |
| `dump-layout` *（2.0）* | — | 将源文件每个产物的 2.0 图布局绘制为 SVG，不构建任何东西 — 见 [`dump-layout`](#dump-layout) |
| `index` *（2.0）* | — | 写出语言服务读取的符号索引。编辑器在每次编译后也会写同一个文件。 |
| `export-catalog` *（2.0）* | — | 将内建节点目录写成 JSON，使工具无需编辑器即可绑定 `UE.*`。编辑器在加载完成后也会写同一个文件。 |
| `fmt` *（2.0）* | `format` | 按打印器的排版重写 2.0 源文件；`-Check` 只报告而不写出任何内容 — 见 [`fmt`](#fmt) |
| `list-generated` *（2.0）* | — | 列出源文件构建的每个资产，但不构建任何东西 — 见 [`list-generated`](#list-generated) |
| `pass-registry` *（2.1.0）* | `passregistry` | 列出 Custom Pass 的 HLSL 槽位及其各自用途；`-Gc` 释放已失效的槽位，`-Rebuild` 重建注册表 — 见 [`pass-registry`](#pass-registry) |
| `dump-graph` *（自 1.9.0 起）* | — | 为每个源文件生成的图写出规范化的 JSON 指纹 |
| `dumpgraph` | `dump-graph` | 完全相同；连字符可选 |

其他任何内容都会失败，并输出 `Unknown DreamShader command '{Command}'.` 及用法横幅，然后以 `1` 退出。

> [!WARNING]
> 第一个裸 token 会**无条件**被当作命令名，在做任何校验之前。把一个不带前导连字符的选项写在*最前面* — `-run=DreamShader Source=X compile`
> — 会把 `Source=X` 当作命令名消耗掉，并产生 `Unknown DreamShader command 'Source=X'.`
> 请让命令保持为第一个裸参数。

<a id="compile--generate"></a>

## `compile` / `generate`

| 选项 | 别名 | 类型 | 必需 | 默认 | 含义 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Source=<path>`** | `-File=<path>` | 字符串 | `-Source` / `-File` / `-All` 之一 | — | 仅编译一个源文件 |
| **`-All`** | — | 标志 | `-Source` / `-File` / `-All` 之一 | 关 | 编译每个项目 DreamShader 源文件 |
| `-Force` | — | 标志 | 否 | 关 | 绕过源文件哈希跳过，无条件重新生成 |
| `-Define=<NAME>[=<value>]` *（自 1.9.0 起）* | `-D=<NAME>[=<value>]` | 字符串，可重复 | 否 | — | 为本次运行添加一个[预处理器定义](../language/preprocessor.md) |

优先级：先查找 `-Source`，然后查找 `-File`；仅当两者都未产生值时才查询 `-All`。同时给出 `-Source` 和 `-All` 时，会静默地只编译那一个文件。三者都没有时，用法横幅会以 `Error` 级别记录，运行以 `1` 退出。

*（2.1.2 之后）* `-Source` / `-File` 会指定文件名，因此其 [NoGeneration](../generation/no-generation.md) 资产也会被构建；`-All` 不指定任何文件，将它们按磁盘上的现状保留。`check -Shaders` 遵循同样的规则。

<a id="-source-path-resolution"></a>

### `-Source` 路径解析

按以下顺序尝试；第一个适用的胜出。

| 顺序 | 条件 | 结果 |
| :-- | :-- | :-- |
| 1 | 值为空，或者是**绝对**路径 | 按给定值规范化 |
| 2 | `<SourceDirectory>/<value>` **存在** | 使用该路径 |
| 3 | `<ProjectDir>/<value>` **存在** | 使用该路径 |
| 4 | 其他情况 | 按给定值规范化 — 随后会因扩展名守卫或编译而失败 |

`<SourceDirectory>` 是 *Source Directory* 项目设置，默认为 `<Project>/DShader`。因此相对值会先针对 DreamShader 源树解析，然后针对项目目录解析。

<a id="-all-discovery-and-ordering"></a>

### `-All` 发现与排序

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 递归收集每个[源根目录](../language/source-files.md#source-roots)下的 `*.dsm`、`*.dsh`、`*.dsf`、`*.dss`、`*.dsi` 和 `*.dsp` —— `<SourceDirectory>`，以及每个拥有 `DShader` 文件夹的插件的该文件夹 |
| 2 | 丢弃每个根目录的 `Packages` 文件夹下的所有内容，但 package 构建的内容除外：它的 `Assets` 源文件及其资产头文件 *(2.1.2 之后；之前则全部丢弃)* —— 见 [Packages](packages.md#source-file-enumeration) |
| 3 | 丢弃 `.dsh` 头文件 —— 它们不生成资产，而是通过包含它们的源文件被编译 —— 但 package 的资产头文件除外 |
| 4 | 排序：`.dsf` 函数文件和资产头文件在前（rank 0），其余每种在后（rank 1）；同 rank 按大小写不敏感的路径比较 |
| 5 | 分轮编译：某个源文件失败时，只要本轮还构建了别的东西就将其暂缓，在下一轮重试；一旦某轮没有构建出任何新内容，仍然失败的就会被报告 *(自 2.1.2 起)* |

调用另一个源文件所生成函数的材质会加载该函数的资产，因此该资产必须先存在。排序覆盖了常见情况——单轮中 `.dsm` 调用 `.dsf` ——但覆盖不了函数调用函数，也覆盖不了另一个源文件调用 `.dss` 所导出的函数；步骤 5 覆盖所有形态，在干净的工作区上也是如此。一直失败的源文件 —— 真实的错误、没有源文件生成的依赖，或者循环 —— 在每个构建出了东西的轮次中最多多尝试一次，并在所有成功之后以其最后一次尝试的消息报告。cook 运行相同的步骤（[Cook 行为](../generation/ephemeral.md#cook-behaviour)）。
`.dsp` 不需要自己的 rank *(2.1.0)*：它的某个 pass 引用的材质缺失或比其源文件旧时，会先由 `.dsp` 自身的编译来编译。

<a id="-define-since-190"></a>

### `-Define` *(自 1.9.0 起)*

每次出现都会为本次运行所编译所依据的[define 表](../language/preprocessor.md#where-defines-come-from)贡献一个条目。该选项可重复 —— 后续出现的是追加名称，而不是替换整组。

| 写法 | 定义 |
| :-- | :-- |
| `-Define=FOO=1` | `FOO` 为 `1` |
| `-Define=PROFILE=cinematic` | `PROFILE` 为字符串 `cinematic` |
| `-Define="NOTE=two words"` | `NOTE` 为 `two words` —— 引号包住整个参数，而不只是值 |
| `-Define=CI_BUILD` | `CI_BUILD` 为**空**值：一个裸标记，其 `defined(CI_BUILD)` 为 `1` 且 `#if CI_BUILD` 为真 |
| `-D=FOO=1` | 同 `-Define=FOO=1` —— `-D` 是短写法 |

名称是选项值中**第一个** `=` 之前的所有内容，因此值本身可以包含 `=`。名称区分大小写。

> [!WARNING]
> `-Define` 是少数必须从**原始**命令行读取而非通过命令行工具通常的参数映射读取的选项之一，原因在于引擎中的一个陷阱，而非偏好。`UCommandlet::ParseCommandLine` 的四参数重载不会把 `-Foo=Bar` 放入两个列表：它会将其从 `Switches` *移除*并移入 `Params`。因此扫描 `Switches` 根本找不到 `-Define`，而 `Params` 以选项名为键，会把 `-Define=A=1 -Define=B=2` 合并成单个条目。两条路径都会静默地丢弃除一个之外的所有 define。

命令行是优先级最高的 define 层级，因此它会覆盖项目设置中的 *Preprocessor Defines* 以及任何模块在 C++ 中注册的内容 —— 只有一个例外：以 `DS_` 开头的名称保留给内建内容，会被丢弃并给出警告，因为一次谎报引擎版本或 Substrate 的运行不会产生不同的材质，而是产生引擎根本无法编译的图。

> [!NOTE]
> define 是[构建键](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash)的一部分，因此使用不同 `-Define` 集合的运行会**不加** `-Force` 重建读取了变动名称的源文件，并且仍然跳过未读取它的那些。这正是按配置进行 CI 扫描负担得起的原因：每组 define 运行一次，各自只重建自己开关碰到的东西。

<a id="per-file-guard"></a>

### 逐文件守卫

编译列表中的每个文件在编译前都会被检查：不是 DreamShader 源文件的路径，或者*是* `.dsh` 头文件的路径，会记录 `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}`，将整次运行标记为失败，然后循环**继续**处理剩余文件。因此一个坏文件不会阻止其余文件编译，但进程仍然以 `1` 退出。

<a id="result-messages"></a>

### 结果消息

每个文件的编译结果都原样记录 —— 成功记 `Display`，失败记 `Error`。当一个文件产出多个资产时，消息用换行连接。

| 消息 | 结果 |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {SourceFile}.` | 构建出一个产物。`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline` *(2.1.0)* —— 适用于每种源语言和两种后端；ThinCustom 材质是 `Material`，路径为实例的路径 *(自 2.0.0 起)* |
| `Generated RenderTarget {ObjectPath} from {SourceFile}.` | 导出 buffer 的 render target，由其管线的构建创建或复用并随之保存 *(2.1.0)*；跟在管线那一行后面 |
| `Skipped {AssetPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` | 哈希一致 —— 传 `-Force` 重新生成 |
| `Skipped {AssetPath} from {SourceFile}; the source marks it NoGeneration, and only a compile that names the source builds it.` | *(2.1.2 之后)* 一个 [NoGeneration](../generation/no-generation.md) 资产，保持原样 —— 用 `-Source` 编译以构建它 |
| `Skipped {AssetPath} from {SourceFile}; the source marks it NoGeneration, and it has never been built.` | *(2.1.2 之后)* 同上，但磁盘上没有资产 |
| `Compiled {SourceFile}; it declares no material and no exported function, so no asset was written.` | 成功，没有东西要写 —— 一个只有辅助内容、`VirtualFunction` 或 `GraphFunction` 声明的文件 *(自 2.0.0 起)* |
| `Warnings:` 后跟每行一条诊断 | 当编译产生警告时追加在成功消息后面 |
| 每行一条诊断，`<file>(<line>,<col>): DSHnnnn: <message>` | **失败**：第一个错误在最前，然后是其他错误、警告和信息。见[诊断](../diagnostics/index.md) |

1.x 的结果行 —— 单独的 thin-custom 措辞、helper include 行、"no assets to generate" 和 `.dsh` 失败、` (virtual)` 后缀 —— 自 2.0.0 起不再写出。没有产物的文件算成功；`.dsh` 根本到不了编译器，被上面的逐文件守卫挡住。

<a id="decompile--export"></a>

## `decompile` / `export`

| 选项 | 别名 | 类型 | 必需 | 默认 | 含义 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Asset=<object-path>`** | — | 字符串 | 二选一 | — | 要反编译的资产 |
| **`-SourceFile=<path>`** | — | 字符串 | 二选一 | — | 把这个源文件构建的每个资产反编译进一个文件 |
| `-Out=<path>` | `-Output=<path>` | 字符串 | 否 | 计算得到 | 目标文件 |
| `-Format=<Dss\|Legacy\|Auto>` | — | 枚举 | 否 | `Auto` | `Auto` 让 `-Out` 的扩展名来决定：`.dsm` / `.dsf` 是 1.x 文本，其他都是 2.0。与扩展名矛盾的格式是 `DSH9085`。 |
| `-KeepAssetPath` | — | 标志 | 否 | 关 | 输出文件本来会指向别的路径时，写一行 `/// @name` 保留资产自己的路径 |
| `-Readable` | — | 标志 | 否 | 关 | 优先 HLSL 语法糖而不是精确到类的节点调用（重建的图节点类可能因此不同），并把 [Substrate 语法糖](../language-v2/substrate.md#decompiling) 读回来（`A * w`、`Transmittance = …`） |
| `-DiagnosticsOut=<file>` | — | 字符串 | 否 | — | 把反编译的诊断写成 JSON（schema `dreamshader-diagnostics`） |

<a id="asset-path-normalization"></a>

### 资产路径归一化

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 每个 `\` 变成 `/` |
| 2 | 若路径以 `/` 开头且**不含** `.`，追加短名：`/Game/Path/Asset` → `/Game/Path/Asset.Asset` |

先加载归一化后的路径。如果失败**且**归一化确实改变了字符串，
再原样重试去掉引号的原始输入。

<a id="supported-asset-classes"></a>

### 支持的资产类

| 类 | 产出 | 默认目标 |
| :-- | :-- | :-- |
| `UMaterial` | `.dss`；`.dsm` 时 `-Format=Legacy` | `<SourceDirectory>/Decompiled/Materials/<package path>.dss`（`.dsm`） |
| `UMaterialFunction` | `.dss`；`.dsf` 时 `-Format=Legacy` | `<SourceDirectory>/Decompiled/Functions/<package path>.dss`（`.dsf`） |
| `UMaterialFunctionMaterialLayer` | `.dss`；`.dsf` 时 `-Format=Legacy` | `<SourceDirectory>/Decompiled/Layers/<package path>.dss`（`.dsf`） |
| `UMaterialFunctionMaterialLayerBlend` | `.dss`；`.dsf` 时 `-Format=Legacy` | `<SourceDirectory>/Decompiled/LayerBlends/<package path>.dss`（`.dsf`） |
| `UMaterialInstanceConstant` *(2.0)* | `.dsi` —— 只有 2.0 文本。生成的 ThinCustom 实例就是它代表的材质，回来的是 `Materials/` 下的 `.dss` | `<SourceDirectory>/Decompiled/Instances/<package path>.dsi` |
| `UDreamPassPipeline` *(2.1.0)* | `.dsp` —— 只有 2.0 文本：`-Format=Legacy`，或者以 `.dsm` / `.dsf` 结尾的 `-Out`，会被拒绝；扩展名不是 `.dsp` 的 `-Out` 则被 [`DSH9210`](../diagnostics/DSH9xxx.md) | `<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp` |
| 其他任何类 | — | 报错 |

路径段会被净化：控制字符和 `< > : " / \ | ? *` 变成 `_`；空目录段
变成 `Folder<N>`，空资产段变成 `Asset<N>`。输出以 UTF-8 写出，
不带 BOM。`-Out` 完全绕过计算出来的目标 —— 必要时创建目录。
反编译器的完整行为见[反编译器](decompiler.md)。

<a id="dump-graph-since-190"></a>

## `dump-graph` *（自 1.9.0 起）*

一个**开发者工具**，不属于正常构建的一部分。它按 `compile` 的方式生成每个源文件，
然后不保存资产，而是写出一份规范化的 JSON 文件，描述生成所产生的图：
节点类、它们反射出来的属性、每一条连线，以及引脚顺序。

它存在的理由是[一致性](../contributing/testing.md#graph-baseline-since-190)。一次编译器改动之前做的 JSON 快照，
是重写的编译器唯一能对照的基准真相，一个源文件一个源文件地比 —— 这是机器可读版的“两个都反编译然后 diff 文本”，
而且中间没有反编译器自己的意见。两次快照用普通的文本 diff 比较。

| 选项 | 别名 | 类型 | 必需 | 默认 | 含义 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Source=<path>`** | `-File=<path>` | 字符串 | `-Source` / `-File` / `-All` 三选一 | — | 只导出恰好一个源文件 |
| **`-All`** | — | 标志 | `-Source` / `-File` / `-All` 三选一 | 关 | 导出每个工程 DreamShader 源文件 |
| `-Out=<dir>` | `-Output=<dir>` | 字符串 | 否 | `<Project>/Saved/DreamShader/GraphBaseline` | 导出树的根目录 |
| `-Force` | — | 标志 | 否 | 关 | 接受但被**忽略**；导出总是会重新生成 |
| `-Define=<NAME>[=<value>]` | `-D=<NAME>[=<value>]` | 字符串，可重复 | 否 | — | 与 `compile` 同一 define 层级；导出是按 define 集进行的 |

`-Source` 解析、`-All` 发现以及 `.dsf` 先于 `.dsm` 的顺序，与
[`compile`](#-all-discovery-and-ordering) 是完全相同的代码。这是刻意的：如果基线覆盖的源文件集
与编译器覆盖的不同，就会把缺失的文件报告成差异。

<a id="it-never-writes-an-asset"></a>

### 它从不写资产

对工程做基线快照，绝不能是为了读回它们而把每个生成的 `.uasset` 重建并保存一遍。
因此 `dump-graph` 在整个扫描过程中关闭编译器的写所有权，
这让生成在旧图被拆除*之前*就拒绝任何会落盘的资产。

> [!WARNING]
> 后果是**磁盘上已经存在的资产会按现状导出，而不是重新生成**。在编译过的工作区里，
> 它们是同一张图。而在过期的工作区里则不是，且是悄无声息的。
> 一旦源文件可能在资产写出之后变动过，就先跑 `compile -All -Force`。
> 运行会统计这些并说明：
> `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them…`

资产背后没有文件的源文件 —— 整个测试语料，以及任何尚未编译的东西 ——
都在内存中生成并从内存导出，所以磁盘上的东西无论如何都不会被碰。

<a id="output-layout"></a>

### 输出布局

```text
<Out>/<root>/<source path relative to that root>.<asset leaf>.graph.json
```

`<root>` 是拥有该文件的[源码根](../language/source-files.md) —— `Project`，
或拥有它的插件名；插件自己的测试语料是 `DreamShaderPlugin`，不属于任何源码根的文件是 `External`
（其相对路径会退化成文件名）。源文件的扩展名被刻意保留在名字中间：
否则 `M_Foo.dsm` 和 `M_Foo.dsf` 会冲突。相对路径内的路径分隔符仍保持为分隔符，
所以导出树镜像源文件树；文件名容纳不了的字符变成 `_`。

每个**资产**一个文件，而不是每个源文件一个：一个声明了三个 `ShaderFunction` 块的 `.dsf` 会写出三个。

<a id="what-the-json-contains"></a>

### JSON 包含什么

| 键 | 含义 |
| :-- | :-- |
| `schema` | 转储格式版本——当前为 `1`。两次捕获只有在版本号相同时才可比较 |
| `plugin` | 插件的 `VersionName` |
| `source` | `{ root, path }`——源根的名称以及相对于它的路径 |
| `asset` | 所生成资产的对象路径 |
| `kind` | `Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` / `ThinCustomInstance` / `MaterialInstance` / `PassPipeline` *(2.1.0)* |
| `backend` | `Graph`，实例材质则为 `ThinCustom`；材质实例和 pass 管线没有该项 |
| `nodes` | 图中的每个表达式，按规范顺序（见下文） |
| `properties` | *（材质）*已连接的材质属性 → `{ from, output, mask }`，键为 `EMaterialProperty` 枚举名 |
| `settings` | 精选出的 `Settings` 集合——对于材质是 `Domain`、`ShadingModel`、`BlendMode`、用法/渲染布尔值以及 `MaterialDecalResponse`；对于函数是 `Usage`、`Description`、`bExposeToLibrary`、`LibraryCategories` |
| `inputs` / `outputs` | *（函数）*名称、类型、排序优先级、描述、可选标志、预览值 |
| `instance` | *（ThinCustom）*`parent.kind` 和 `parameters`——即覆盖项、静态开关和静态组件掩码 |

对于 ThinCustom 实例，`nodes`、`properties` 和 `settings` 描述的是该实例所继承的**隐藏基础材质**，
图实际上就存在于那里。基础材质自身的对象路径被刻意省略：它在编辑器中是瞬态对象，在磁盘上是实例的子对象，
因此同一张图的两次捕获中该字符串会不同。

<a id="a-pipelines-dump"></a>

### 管线的转储

*(自 2.1.0 起)* `.dsp` 产物没有图，因此它的转储就是运行时读取到的 `UDreamPassPipeline`，
每个枚举都按 `.dsp` 的拼写方式书写。运行行中写的是 `nodes`，但统计的是 pass 数。
其导出缓冲区的渲染目标不会获得自己的文件。

| 键 | 含义 |
| :-- | :-- |
| `pipeline` | `order`、`defaultInjection`、`views`、`requires`、`enabled`（布尔参数，或 `null`） |
| `parameters` | 按声明顺序：`name`、`default`（`type` 和 `value`）、`group`、`description`、`slider`（`[min, max]` 或 `null`）、`sortPriority` |
| `buffers` | 按声明顺序：`name`、`format`、`resolution`、固定大小缓冲区为 `size` 否则为 `scale`、`clear`（四个浮点数，`Clear = None` 则为 `null`）、`mips`、`history`、`export`、`exportTarget`（渲染目标的对象路径，或 `null`）、`description` |
| `passes` | 按声明顺序——注入点内的执行顺序：`name`、`kind`、`injection`、`enabled`、`reads` / `writes`（`slot`、`buffer`、`previous`）、`params`（`target`、`source` 以及常量，或带 `multiplier` 和 `offset` 的参数）、`description` 以及 `settings`——只包含该 pass 自身类型的键，slot 编号也包含在内 |
| `slots` | 再次列出每个 HLSL pass：`pass`、`kind`（`compute` 或 `pixel`）、`slot`——该数字可能因重建另一条管线而变化 |

写入守卫同样覆盖 slot 注册表：背后没有文件的管线在内存中构建，其 slot 使用注册表已为其记录的值（无记录处为 `-1`），
注册表和快照都不会被写入。

每个节点是 `{ id, class, props, inputs }`，命名重路由用法上还会加上 `reroute`。`id` 是 `n0`、
`n1`……——按位置排列，绝不是引擎标识符。`class` 是表达式类的完整路径名。
`inputs` 仅列出**已连接**的引脚，按引脚顺序，格式为 `{ index, name, from, output, mask }`，其中
`mask` 是 `"rgba"` 风格的字符串或 `null`。命名重路由用法**按名称**链接到其声明，
因为名称是声明在源码中的身份，而其 GUID 在每次重建时都会重新发放。

输入的 `name` 是引擎自身的，只有一个例外 *(自 2.0.0 起)*：`BreakMaterialAttributes`、
`GetMaterialAttributes` 和 `SetMaterialAttributes` 用翻译后的文本命名其输入，因此它们
被写作 `MaterialAttributes` 以及属性自身的名称（`BaseColor`、`FrontMaterial`）——
捕获绝不能依赖进行捕获的编辑器的语言。同理，Get 和 Set 在 `props` 中以名称形式携带其属性列表，即 `AttributeGetTypes` 和 `AttributeSetTypes`，
尽管其他所有 `FGuid` 都被省略：对这两个节点而言，该列表就是行为本身。

<a id="the-exclusion-list"></a>

### 排除列表

确定性优先于完整性：同一编译器两次运行之间都会变化的指纹，无法说明两个*不同*编译器之间的任何事。
因此任何不属于图属性的东西都被省略，而不是被归一化。

| 排除项 | 原因 |
| :-- | :-- |
| 对象名称，以及引擎附加到其后的 `_12` 数字后缀 | 在节点被重新创建时分配；节点身份改为按位置确定 |
| `MaterialExpressionGuid`、`VariableGuid`、`DeclarationGuid`——每个 `FGuid` | 每次重新生成时重新发放 |
| `MaterialExpressionEditorX` / `EditorY` | 节点坐标；重新生成时无论如何都会从 `Layout` 部分重新分配 |
| `NodeColor` 以及所有其他颜色 | 命名重路由从其自身的**路径名**播种颜色，因此被重新创建的节点会自行重绘 |
| `Desc`、`bCollapsed`、`bCommentBubbleVisible`、`bHidePreviewWindow`、`bShowOutputNameOnPin`、`bRealtimePreview` | 表现，而非行为 |
| `GraphNode`、`Material`、`Function`、`SubgraphExpression` | 瞬态，或指向所属对象的反向指针 |
| `FExpressionInput` 属性 | 它们导出的文本是所连接对象的路径；连接改为按节点 id 以结构化方式记录 |

`props` 在其他方面就是插件已经视为*内容*的每个反射 `UPROPERTY`——与[差异摘要](../generation/divergence.md)相同的规则，
因此二者对何为手动编辑不会产生分歧。参数名称、分组和排序优先级、采样器类型、纹理对象路径、静态开关默认值、`Custom` `Code` / `OutputType` / `AdditionalOutputs` / `IncludeFilePaths`、
函数调用目标路径、`ComponentMask` 通道标志、常量以及 `Panner`/`Time` 设置
全部包含在内。无论值是否等于类默认值都会被写出——仅因恰好匹配 CDO 就省略某个值，会在引擎默认值变动时改变转储的形态。

浮点数格式为 `%.9g`；非有限值会降级为字符串，因为 JSON 无法表示它。

<a id="node-order"></a>

### 节点顺序

`nodes` 是**从汇点开始的后序深度优先遍历**，因此一个节点只有在所有为其提供输入的节点都被编号后才被编号，
并且 `from` 总是引用更早的条目。

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 汇聚点。对材质：每个已连接的材质属性输入，按 `EMaterialProperty` 枚举器顺序（对 ThinCustom 实例，用其隐藏基础材质的顺序）。对函数：其 `FunctionOutput` 表达式按 `SortPriority` 顺序，声明顺序用于打破平局 |
| 2 | 从每个汇聚点出发，按该节点的**引脚顺序遍历其输入**进行深度优先；具名重路由的使用处按其声明处理，如同它是一个输入 |
| 3 | 所有未被到达的内容——死节点、`CustomOutput` 汇聚点、未使用的函数输入——按类路径排序，再按节点自身的 `props`，再按数组位置 |

引擎自身的 `GetExpressions()` 顺序是创建顺序：这是构建图的编译器的属性，而不是图的属性，正是两个编译器无法指望达成一致的地方。第 3 步的最终平局规则是唯一一条两个编译器无需匹配的规则；它的存在是为了让单次采集可复现。

<a id="diffing-two-captures"></a>

### 对比两次采集

```powershell
git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

`--no-index` 对比两个不在仓库中的目录树，而布局正是为此设计的：相同的树形结构、有序的键、每行一个值、LF 行尾。只在一侧存在的文件，是一个资产集发生变化的源文件。其他每个差异块都是行为差异，需要修复或在一致性日志中记录一条。

输出为不带 BOM 的 UTF-8、两空格缩进、键在每一层按区分大小写排序、LF 行尾并以换行结尾。

<a id="dump-layout"></a>

## `dump-layout`

*(自 2.0.0 起)* 在不构建资产的情况下，将为源的每个产物在 IR 上计算的布局画出来，每个产物和样式一张 SVG。这就是在选择 [图布局样式](../generation/graph-layout.md#layout-styles) 之前查看 **Blocks**、**Source Bands** 和 **Layered** 的方式。对 `Blocks`，图中显示盒子以及它们之间的具名重路由：值旁边的声明、每个读取它的盒子中的使用处，且盒子之间没有连线。

| 选项 | 默认 | 含义 |
| :-- | :-- | :-- |
| `-Source=<path>` / `-All` | 必需其一 | 哪些源 |
| `-Style=` | `All` | `Blocks`、`SourceBands`、`Layered`，或 `All`（每种各一张图）。`Classic` 是 1.x 布局，它作用于成品图，无法从 IR 绘制（[`DSH9041`](../diagnostics/DSH9xxx.md)）。 |
| `-Out=<dir>` | `<Project>/Saved/DreamShader/Layout` | 输出树的根，布局方式与 `dump-ir` 的 `<root name>/<path relative to the root>` 相同 |
| `-Json` | 关 | 同时写出坐标——位置、尺寸、列、带、注释框、长边数量，对 `Blocks` 还有盒子、它们之间的重路由以及输出前方的重路由——放在每个 SVG 旁边 |

文件命名为 `<source file>.<Product>.<Style>.layout.svg`（以及 `.json`）。节点尺寸根据 IR 估算——节点的引脚和标题——所以图中显示的是排列，而不是活动节点的确切占位。无法写入的文件是 [`DSH9040`](../diagnostics/DSH9xxx.md)。

<a id="fmt"></a>

## `fmt`

*(自 2.0.0 起)* 格式化器就是语言自带的打印器，作用于保留了注释和空行的解析结果；任何地方都不存在关于布局的第二种意见。`fmt` 就地重写每个文件。

| 选项 | 含义 |
| :-- | :-- |
| `-Source=<path>` | 一个文件——任何源根下的任何 2.0 源文件 |
| `-All` | **可写**源根下的每个 `.dss`、`.dsi` 和 `.dsh`——即项目自己的；插件按其原样发布源码，点名它的某个文件就是要格式化它的方式 |
| `-Check` | 什么都不写；每个会变动的文件都是 [`DSH9046`](../diagnostics/DSH9xxx.md) 且运行失败——CI 用的形式 |
| `-Out=<dir>` | 把格式化后的副本写到该目录下，镜像源树，而不是覆盖源文件 |

它在写出一个字节之前对每个文件保证：格式化后的文本**可解析**，解析为**相同的声明**，且带有该文件原有的**每一条注释**，并且**再次**格式化不改变任何内容。任何一项失败的文件都会被保持原样并报告为 [`DSH9044`](../diagnostics/DSH9xxx.md)——这是格式化器的缺陷，绝不是文件的。文件自身的行终止符会被保留，所以格式化绝不会变成整文件差异。

有两类文件保持原样，且都不算错误：

| 文件 | 原因 | 代码 |
| :-- | :-- | :-- |
| 带 1.x 声明的——一个 `.dsm`、一个 `.dsf`、一个仍带有 `Function` 块的 `.dsh` | 打印器为这些写出来的是 2.0 文本，而把 1.x 变成 2.0 是 [`migrate`](migrate.md)，它在写出之前证明的东西要多得多 | `DSH9042`（info） |
| 在 `/// @custom` 体之外使用 `#if` 的 | 解析器读取的是预处理后的文本，而 `fmt` 按磁盘上的原样读取文件；重新打印 `#if` 的一侧会删掉另一侧 | `DSH9043`（info） |

运行结束时输出一行：`DreamShader fmt: 3 rewritten, 41 already formatted, 2 left alone, 0 failed
of 46 file(s). RESULT=OK`。

<a id="list-generated"></a>

## `list-generated`

*(自 2.0.0 起)* 命名源码所构建的每个资产——前端、绑定器和目标规则，之后什么都没有：不构建、不加载、不保存任何资产。ignore 规则和 P4 typemap 就是照它写的；[版本控制](../generation/source-control.md) 里有做法。

| 选项 | 默认 | 含义 |
| :-- | :-- | :-- |
| `-Source=<path>` / `-All` | 必需其一 | 哪些源；`-All` 也覆盖插件源根 |
| `-As=` | `Packages` | `Packages`——长 package 名；`Files`——相对项目目录的路径；`GitIgnore`——相同的路径以 `/` 锚定，带一行头注释；`Json`——全部字段（schema `dreamshader-generated-assets`，version 1） |
| `-Out=<file>` | 日志 | 把列表写到这里。脚本应该总是传它：日志是给人看的。 |
| `-IncludeEphemeral` | 关 | 也列出一个此刻仅驻留内存的 ThinCustom 材质；它没有文件，所以被排除在用于写 ignore 规则的列表之外 |

一个 `.dsp` 贡献两种行 *(2.1.0)*：它的管线（kind `PassPipeline`）以及 float 或 normalized 格式的每个导出缓冲区的渲染目标（kind `PassExportTarget`，位于管线文件夹中的 `<Pipeline>_<Buffer>`）。`<DShader>/.dreampass/` 下的 slot 注册表和快照是源，不是生成的资产，不会被列出——见 [版本控制](../generation/source-control.md#notes)。

编不过的源仍然贡献失败之前已经确立的产物，且运行以 `RESULT=FAILED` 结束——列表从不会悄悄变短。项目目录之外的资产（引擎级插件的内容）没有项目相对路径；`Files` 和 `GitIgnore` 会说明略过了多少个（[`DSH9049`](../diagnostics/DSH9xxx.md)）。未知的 `-As` 是 `DSH9048`；写不出的列表是 `DSH9047`。

自 `-All` 起*(2.1.2 之后)*，package 资产也会被列出——即某个 package 的 `Assets` 源文件与资产头文件所构建的内容（[Packages](packages.md#package-assets)）——并且会从 package 资产根目录（`/Game/DreamShaderPackages`、`/<Plugin>/DreamShaderPackages`）中搜索那些**不再有源文件构建**的资产：已移除的 package 的，或某个 package 不再生成的函数的。每一项都是警告 [`DSH9111`](../diagnostics/DSH9xxx.md#dsh9111)，而 `-As=Json` 用 `"orphaned": true` 列出它们（其他每一行都显示 `false`）。普通列表会略去它们——它们列的是源文件所构建的内容——并且不会删除任何东西。

<a id="dsp-sources"></a>

## `.dsp` 源文件

*（自 2.1.0 起）* [Custom Pass 管线](../language-v2/passes.md)和其他源文件一样是可编译源文件，只有一个产物——一个 `UDreamPassPipeline`——而且没有图。每个命令对它的处理如下：

| 命令 | 对 `.dsp` |
| :-- | :-- |
| `compile` | 构建管线以及每个导出 buffer 的 render target，并给每个 HLSL pass 分配 slot：规划、[预检](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)，把快照和注册表写到 `<DShader>/.dreampass/` 下（见 [`pass-registry`](#pass-registry)）。如果它的某个 pass 引用的材质缺失或比其源文件旧，会先编译该材质。命令行工具不重编任何 shader；下次编辑器启动时会编译改动的注册表。UE 5.8 以下会拒绝编译，`DSH8300` |
| `check` | 读取 pass 引用的每个材质和 `.usf`，并执行绑定器所做的每一项检查。不先编译任何东西，不构建任何东西，也不编译 HLSL |
| `check -Shaders` | 像 `compile` 那样构建并保存管线，然后在 slot 中预检每个 HLSL pass——见 [管线的 shader 检查](#shader-check-of-a-pipeline) |
| `dump-ir` | 以文本形式输出产物：管线的键，每个参数和 buffer 一行，以及每个 pass 的键、它所属 kind 的设置和它的绑定，均按绑定器确定的样子——`(inferred)` 标记源文件未写入的 buffer 分辨率，`(from the shader)` 标记从 `[numthreads]` 读到的组大小。`-Json` 会附加 JSON 形式 |
| `dump-layout` | 对它什么都不写：管线没有图可布局。不算错误 |
| `index` | 符号索引，含一个 `pipeline` 段：绑定后的 `#pragma pipeline` 值、uniform、buffer 以及各 pass 及其绑定和跨度，还有编辑器用来补全的注入点和格式拼写 |
| `fmt` | 打印器的排版，保证与 `.dss` 相同；`-All` 包括可写源文件根的 `.dsp` 文件 |
| `list-generated` | 管线及其导出 buffer 的 render target——见 [`list-generated`](#list-generated) |
| `dump-graph` | 管线的一份 JSON——见 [管线的 dump](#a-pipelines-dump) |
| `decompile` | 回头路：`-Asset` 指定一条管线，或 `-SourceFile` 指定一个 `.dsp`，写出一个 `.dsp`——见 [反编译器](decompiler.md#pipelines-dsp) |

<a id="shader-check-of-a-pipeline"></a>

### 管线的 shader 检查

对 `.dsp` 执行 `check -Shaders` 不会编译任何材质：管线没有材质，而它的 pass 引用的材质由构建它们的源文件来检查。可能编译失败的是 HLSL pass，而它改在全局 shader slot 中预检——即编译所执行的预检，只是有三处不同：

- **每个 HLSL pass，无论改动与否。** 编译只预检快照有变化的 pass。该检查按注册表当前的样子规划 slot——已有 slot 的 pass 保留它，新的 pass 在它将占用的 slot 中检查——并且不提交该规划的任何内容。
- **`-Platform` 格式**，当运行指定了任何格式时：`SM6` 是 `PCD3D_SM6`，`SM5` 为 `PCD3D_SM5`，`ES3_1`
  `PCD3D_ES3_1`、`VULKAN_SM6`、`VULKAN_SM5` 和 `METAL_SM5` 为 `SF_` 格式，其他任何 token 都被视为 shader 格式名。不指向本机可编译的任何 shader 格式的 token——包括诸如 `Windows` 这类目标平台名——不会被预检，并会由
  [`DSH8324`](../diagnostics/DSH8xxx.md) 说明。没有 `-Platform` 时，编译所预检的格式为：活动 feature level 的格式，以及活动目标平台所针对的每个格式，对于本机没有编译器的格式会给出 [`DSH8324`](../diagnostics/DSH8xxx.md)。
  `-Quality` 和 `-Timeout` 不适用于 slot。
- **其错误在摘要行中计为 shader 错误。** 编译错误是 `DSH8322`，位于引起它的 `.usf` 所在行；一个没有 HLSL pass 的 `.dsp` 会报告 `DSH8339`（info）并通过。

<a id="pass-registry"></a>

## `pass-registry`

*（自 2.1.0 起）* 从命令行管理 Custom Pass HLSL slot 注册表。每个 `.dsp` 的每个 HLSL pass
——一个 `compute` pass，或一个带 `Shader =` 的 `fullscreen` pass——都在全局 shader
`FDreamPassCS` 和 `FDreamPassPS` 的一个 **slot** 中运行，而把 slot 映射到 pass 的文件随源文件一起提交
（[HLSL pass](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)）：

```text
<DShader>/.dreampass/
├─ Registry.json          the record the other files are written from
├─ RegistryCompute.ush    one section per compute slot; what FDreamPassCS includes
├─ RegistryPixel.ush      the same for FDreamPassPS
└─ Slots/C00/, P03/, …    one snapshot per slot that has one
```

每个工程有一个注册表，位于工程的 *Source Directory* 下；插件的 `.dsp` 在其中像任何其他源文件一样占用 slot。`Registry.json`（schema `dreamshader-pass-registry`，version `1`）持有两个数组，`compute` 和 `pixel`，每个 slot 一个条目：`slot`、`pipeline`（对象路径）、`pass`、
`kind`（`compute` 或 `fullscreen`）、`source`（`.dsp`，工程相对）、`shader`（工程相对）、
`entry`、`hash`、`formats`、`files`（快照的，相对于 slot 目录）以及 `section`（slot 在其注册表文件中的文本）。

编译会让注册表自行保持最新。该命令用于编译看不到的情况：被删除或改名的 `.dsp`、
弄坏 `Registry.json` 的合并、丢失快照文件的检出。

| 选项 | 效果 |
| :-- | :-- |
| *（无）* | 列出每个 slot 及其内容。什么都不写 |
| `-Gc` | 释放每个 `PipelineGone` 和 `PassGone` slot |
| `-Rebuild` | 把无法读取的 `Registry.json` 移到一旁，重新编译每个 `.dsp`，释放垃圾并重写注册表文件。在旁边加上 `-Gc` 不会增加任何内容 |

这些选项遵循[参数规则](#argument-syntax)：`-Gc`、`--gc` 和 `-Gc=true` 是一个开关，
同时给出时 `-Rebuild` 胜出。`dsc.ps1` 开关是 `-Gc` 和 `-Rebuild`；在
PowerShell 提示符下，`./dsc.ps1 pass-registry --gc` 不是该开关——PowerShell 会把 `--gc` 绑定到脚本的位置参数目标，而该命令会忽略它，于是这次运行只会列出。

<a id="the-listing"></a>

### 列表

```text
LogDreamShader: Display: DreamShader pass-registry: C:/Projects/MyGame/DShader/.dreampass/Registry.json
LogDreamShader: Display:   C00  Live             /Game/Passes/CP_Highlight.CP_Highlight  pass 'Blur'  DShader/Passes/BoxBlur.usf : BlurCS  [PCD3D_SM5, PCD3D_SM6]
LogDreamShader: Display:   C01  PipelineGone     /Game/Passes/CP_Old.CP_Old  pass 'Simulate'  DShader/Passes/Old.usf : MainCS  [PCD3D_SM6]
LogDreamShader: Display: DreamShader pass-registry: 2 of 32 compute slot(s), 0 of 16 pixel slot(s) taken; 1 to collect with -Gc. RESULT=OK
```

每个 slot 一行，先列计算表，各自按 slot 顺序：

| 列 | |
| :-- | :-- |
| `C07` / `P03` | 表——计算表或像素表——以及槽编号：槽目录的名称 |
| state | 见下 |
| pipeline | `UDreamPassPipeline` 的对象路径 |
| `pass '<Name>'` | 该 pass |
| `<shader> : <entry>` | 着色器文件（工程相对路径）和入口；记录没有时为 `-` |
| `[<formats>]` | 快照通过预检的着色器格式。此后工程才定位到的格式，快照从未对它检查过 |

| 状态 | 含义 | `-Gc` |
| :-- | :-- | :-- |
| `Live` | 它管线的 `.dsp` 在该表中以 HLSL 运行那个 pass，且快照在磁盘上 | 保留它 |
| `Reserved` | 有记录但没有通过预检的快照：该槽编译为空桩，它的 pass 什么都不做，直到它的 `.dsp` 再次编译 | 保留它 |
| `SnapshotMissing` | 快照的文件不在磁盘上——一个未提交的 `Slots` 文件夹。它 `.dsp` 的下一次编译会写入它们；在此之前，加载 DreamShaderPass 模块的进程会在启动时把该槽的 section 从注册表文件中移除，记为错误，因此全局着色器仍然能编译。`DSH9208` | 保留它 |
| `PipelineGone` | 源根下不再有任何 `.dsp` 构建它的管线。当管线资产仍然存在时，`DSH9201` 警告说它的 pass 仍然指向该槽：一旦该槽被释放并给了另一个 pass，旧资产运行的就会是那个 pass 的着色器 | **释放它** |
| `PassGone` | 它管线的 `.dsp` 能编译，并且不再在该表中以 HLSL 运行那个 pass | **释放它** |
| `Unknown` | 它管线的 `.dsp` 编不到足以判断。`DSH9209` | 保留它 |

为把 `Live` 与 `PipelineGone`、`PassGone` 和 `Unknown` 区分开，该列表会运行每个拥有槽的
`.dsp` 的前端；不构建也不写入任何东西。只有在有确凿证据时槽才是垃圾——没有
源码构建它的管线，或者它管线的源码能编译但没有这样的 pass——所以编不过的源文件保留它所拥有的一切槽。

<a id="-gc"></a>

### `-Gc`

释放每个 `PipelineGone` 和 `PassGone` 槽——它在 `Registry.json` 中的条目、它在注册表
文件中的 section、它的快照——并删除每个无人命名的槽目录。每个被释放的槽是一条 info，
`DSH9202`，排在该列表的警告之后。commandlet 中不重编任何东西；下次编辑器启动
会编译改过的注册表。

```text
LogDreamShader: Display: DreamShader pass-registry -Gc: 1 slot(s) freed. RESULT=OK
```

在删除或重命名 `.dsp` 之后运行它：它的槽在被回收之前仍被占用，而一次
发现某张表的所有槽都被占用（`DSH8316`）的编译会点名这个动词。

<a id="-rebuild"></a>

### `-Rebuild`

| 步骤 | |
| :-- | :-- |
| 1 | 解析不了的 `Registry.json`——遗留其中的合并冲突是常见原因——被移到一旁成为 `Registry.json.unreadable`（`DSH9203`），并写入一个空注册表，以便下面的编译重新分配每个槽。能解析的原样保留 |
| 2 | 源根下的每个 `.dsp`，排除 `Packages` 文件夹，按路径顺序编译，强制，如同 `compile -Force` 那样：每个都针对注册表规划它的槽，并写入它缺失的快照。失败的为 `DSH9204` 并保留它原有的槽；完全没有 `.dsp` 时该步骤为 `DSH9206`（info） |
| 3 | 收集垃圾，如同 `-Gc` 所做，不带它那缺失快照的警告 |
| 4 | `RegistryCompute.ush` 和 `RegistryPixel.ush` 按 `Registry.json` 的现状写出。快照文件仍然缺失的槽变为预留（`DSH8337`），因为包含缺失文件的注册表会让全局着色器编译失败。每个无人命名的槽目录都被删除 |
| 5 | 对每个失败的 `.dsp`，它现有管线资产的每个 HLSL pass 都对照注册表检查：注册表现在给了另一个 pass 或留空的槽为 `DSH9205`——在它的源码能编译之前，那个 pass 运行的是该槽所持有的东西 |

```text
LogDreamShader: Display: DreamShader pass-registry -Rebuild: 4 .dsp compiled, 0 failed, 1 slot(s) freed, 0 slot(s) reserved for a missing snapshot. RESULT=OK
```

它像 `compile` 一样构建并保存管线。在 `Registry.json` 出现合并冲突之后运行它——
编译拒绝覆写解析不了的注册表（`DSH8315`），`-Gc` 也一样
（`DSH8335`）——或者在丢失了快照文件的检出之后，或每当该列表与源码
不一致时。把整个 `.dreampass/` 文件夹连同结果一起提交。

写不出的注册表文件为 `DSH8326`，移不到一旁的为 `DSH8336`；因
未检出而只读的文件是这两者的常见原因。

<a id="argument-syntax"></a>

## 参数语法

所有命令共用。由自动化测试 `DreamShader.Commandlet.Args.SplitAndGet` 和
`DreamShader.Commandlet.Args.FlagValues` 钉住。

| 规则 | 行为 |
| :-- | :-- |
| 键归一化 | trim，然后剥掉**所有**前导 `-`，再 trim。`-Source`、`--Source` 和 `Source` 是同一个键 |
| 名称匹配 | 大小写不敏感——`-source`、`-SOURCE`、`-Source` 等价 |
| 值归一化 | trim，然后剥掉一层包裹引号，再 trim |
| 赋值切分 | 在**第一个** `=` 处切；因此值里可以含 `=` |
| 无短横赋值 | 凡是能写 `-Key=Value` 的地方都接受裸 `Key=Value`（无前导短横） |
| 查找顺序 | 先解析出的参数表，再开关列表，再裸 token 列表；第一个命中胜出 |
| 空值 | `-Source=` 解析成空串，被当作**不存在** |
| 缺失键 | 不存在 |

<a id="boolean-flags"></a>

### 布尔标志

标志可以裸写，也可以带值。值在上面的值归一化之后大小写不敏感地匹配。

| 写法 | 结果 |
| :-- | :-- |
| `-Force` | 开 |
| `-Force=1`、`-Force=true`、`-Force=yes`、`-Force=on` | 开 |
| `-Force=0`、`-Force=false`、`-Force=no`、`-Force=off` | 关 |
| *（没写）* | 关 |
| `-Force=<anything else>`，包括空的 `-Force=` | **错误** [`DSH9110`](../diagnostics/DSH9xxx.md#dsh9110) |

错误不会去猜你的意思。命令记 `DSH9110`、**什么都不做**并以 `1` 退出：`migrate -Check=banana` 既不检查也不写，`-All=never` 什么都不选。
每个命令都在碰任何文件之前读完自己的所有标志。

引擎的解析器会把每个 `-Name=value` 从开关列表挪进参数表，然后命令才看到它，所以标志的查找顺序与值相同：参数表，然后
开关，然后裸 token（无短横的 `Force=true` 就是一个 token）。命令不读的标志和其他未知开关一样被忽略：`dump-graph -Force=banana` 照跑，因为 `dump-graph`
从不读 `-Force`。

> [!NOTE]
> 在 2.1.0 及更早版本中，带任何值写的标志会被读成**不存在**：查找只在
> 开关列表里进行，而引擎从不把 `-Name=value` 留在那里。`-Force=true` 不会强制，
> `migrate -Source=X -Check=true` 照样写出迁移结果。本页以前说过相反的话——
> 无法识别的值算开。

<a id="exit-codes"></a>

## 退出码

| 代码 | 条件 |
| :-- | :-- |
| `0` | 所选命令报告成功 |
| `0` | `compile -All` 解析出**空**的源文件列表 —— 记录 `Warning`，视为成功 |
| `1` | 既没有命令 token 也没有 `Command=` 值 |
| `1` | 未知命令名 |
| `1` | `compile` 但 `-Source` / `-File` / `-All` 一个都没给 |
| `1` | `compile` 期间任何逐文件守卫失败或编译失败 |
| `1` | `decompile` 没有 `-Asset`，或加载 / 反编译 / 写入失败 |
| `0` | `dump-graph -All` 解析出**空**的源文件列表 —— 记录 `Warning`，视为成功 |
| `1` | `dump-graph` 但 `-Source` / `-File` / `-All` 一个都没给 |
| `1` | `dump-graph` 期间任何逐文件守卫失败、生成失败或转储写入失败 |
| `0` | `pass-registry` 列出了注册表 —— 不管列出了什么；它的警告不会让运行失败 *(2.1.0)* |
| `1` | `pass-registry` 读不了 `Registry.json`（`DSH9200`） |
| `1` | `pass-registry -Gc` 读不了 `Registry.json`（`DSH8335`）或写不了注册表文件（`DSH8326`） |
| `1` | `pass-registry -Rebuild`：读不了的 `Registry.json` 无法移走（`DSH8336`），某个 `.dsp` 编不过（`DSH9204`），或收集或重写失败 |
| `1` | 任何命令：它读的某个标志的值既不是开也不是关（`DSH9110`）；什么都没跑 —— 见[布尔标志](#boolean-flags) |

<a id="notes"></a>

## 说明

- **commandlet 里 editor bridge 从不启动。** 编辑器模块一检测到 commandlet 进程就从启动流程返回，
  所以没有源目录 watcher、没有保存即编译、端口 17864 上没有 WebSocket 服务、没有 `diagnostics.json` 写入、没有
  `bridge.db`，也没有菜单注册。唯一的例外是 cook commandlet，它会装一个 post-engine-init 钩子。见[编辑器 bridge](bridge.md)。
- **commandlet 会写出真实 package。** `compile` 要求每个 ThinCustom 产物
  都 Materialized，所以 `/Game/...` `.uasset` 文件会被创建并保存到磁盘。交互式编辑器
  把 ThinCustom 产物构建为 Ephemeral（只在内存中），除非它已经保存过；`Graph`
  材质和材质函数两者都会保存到磁盘。这是在 CI 中物化整个工程源文件的预期做法。见[内存材质](../generation/ephemeral.md)。
- 因为资产会被持久化，一次 commandlet 运行可能在磁盘上留下资产，遮蔽编辑器的
  Ephemeral 材质。*Tools ▸ DreamShader ▸ Make Ephemeral* 会删除它们。
- Cook 是一个单独的 commandlet。只在 cook **director** 上（即 `-run=` 含
  `Cook` 且不带 `-cookworker` 的进程），DreamShader 会在正式 cook 之前把每个工程源文件物化为
  持久资产。那里的生成失败是 `Fatal`，会中止 cook：
  `DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook] Failed entries above.`
  属于本工程不往里写的插件 —— 引擎或商城插件 —— 的源文件在那里不计入：它和其他源文件一样编译，失败记成
  `[Cook] Not this project's to build (plugin '<Name>')` 而不中止，因为那些资产随该插件
  发布。见[Cook 行为](../generation/ephemeral.md#cook-behaviour)。
- `-Force` 只绕过 source hash 跳过；它不删除任何东西。见
  [缓存](../generation/caching.md)。
- **`.dsp` 编译会写到 `Content/` 之外** *(2.1.0)*：HLSL slot 注册表及其快照在
  `<DShader>/.dreampass/` 下，它们是要提交的源码 —— 见[版本控制](../generation/source-control.md#notes)。
  commandlet 写它们但不重编任何 shader；编辑器在它们变化时立刻重编 slot shader。
- 给*非* commandlet 的自动化运行（例如
  `-ExecCmds="Automation RunTests …"`）加上 `-NoDreamShaderEditorBridge`，同样能在那边压掉 bridge。

<a id="diagnostics"></a>

## 诊断

运行期替换写作 `{Placeholder}`。所有消息都进 `LogDreamShader`。

| 消息 | 严重度 | 原因 |
| :-- | :-- | :-- |
| *（用法横幅）* | Error | 既没有命令 token 也没有 `Command=` 值 |
| `Unknown DreamShader command '{Command}'.` + 用法横幅 | Error | 命令不是[命令](#commands)里的任何写法 |
| *（用法横幅）* | Error | `compile` 但 `-Source` / `-File` 和 `-All` 都没给 |
| `DreamShader commandlet found no source files to compile.` | **Warning** | 解析出的源文件列表为空；运行仍以 `0` 退出 |
| `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}` | Error | 该文件不是 DreamShader 源文件，或者是 `.dsh` 头文件 |
| *（编译结果消息）* | Display / Error | 逐文件结果；见[结果消息](#result-messages) |
| *（用法横幅）* | Error | `decompile` 但 `-Asset` 和 `-SourceFile` 都没给 |
| `DreamShader decompile: -Format takes Dss, Legacy or Auto; got '{Value}'.` | Error | 未知的 `-Format` |
| `DreamShader could not load asset '{AssetPath}'.` | Error | 资产在归一化路径和原始路径下都加载失败 |
| `DreamShader decompile: {Error}` | Error | `-SourceFile` 源文件解析不了，或它构建的东西还不存在 —— 先编译它 |
| `DreamShader failed to decompile '{LoadPath}': {Error}` | Error | 反编译器报告失败 |
| `DreamShader decompile supports Material and MaterialFunction assets only: {AssetPath}` | Error | `-Format=Legacy` 不支持该资产类（通过上面的消息呈现）；2.0 反编译器返回 [`DSH9086`](../diagnostics/DSH9xxx.md#dsh9086) |
| `Decompile did not produce source text.` | Error | 反编译器失败且没有错误文本 |
| `DreamShader failed to resolve an output file path.` | Error | 目标路径解析为空 |
| `DreamShader failed to create output directory '{Directory}'.` | Error | 目标目录无法创建 |
| `DreamShader failed to write decompiled source '{Path}'.` | Error | 文件无法写入 |
| `DreamShader decompiled '{LoadPath}' to '{OutputPath}' ({Format}).` | Display | 成功；`{Format}` 是 `dss` 或 `legacy` |
| `DreamShader commandlet found no source files to dump.` | **Warning** | `dump-graph` 解析出空的源文件列表；运行仍以 `0` 退出 |
| `DreamShader dump-graph requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}` | Error | 该文件不是 DreamShader 源文件，或者是 `.dsh` 头文件 |
| `DreamShader failed to dump '{Path}': {Error}` | Error | 生成或转储失败；见 [`DSH9030`–`DSH9034`](../diagnostics/DSH9xxx.md) |
| `Dumped {Kind} {AssetPath} ({N} nodes) to {File}.` | Display | 导出了一个资产；资产已有文件时追加 ` (read from disk; not regenerated)` |
| `DreamShader dump-graph wrote {N} graph dump(s) from {M} source file(s) to {Directory}.` | Display | 运行结束汇总 |
| `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them (it never writes an asset). Run 'compile -All -Force' first if those sources have changed since.` | **Warning** | 见[从不写资产](#it-never-writes-an-asset) |
| `DreamShader pass-registry: {Registry.json path}` | Display | 一次列出的开头 *(2.1.0)* |
| `DreamShader pass-registry: {N} of {M} compute slot(s), {N} of {M} pixel slot(s) taken; {N} to collect with -Gc. RESULT=OK` | Display | 一次列出的结尾 |
| `DreamShader pass-registry: the registry could not be read. RESULT=FAILED` | Error | 在 `DSH9200` 之后 |
| `DreamShader pass-registry -Gc: {N} slot(s) freed. RESULT={OK\|FAILED}` | Display / Error | `-Gc` 的结尾 |
| `DreamShader pass-registry -Rebuild: {N} .dsp compiled, {N} failed, {N} slot(s) freed, {N} slot(s) reserved for a missing snapshot. RESULT={OK\|FAILED}` | Display / Error | `-Rebuild` 的结尾 |
| `DreamShader pass-registry -Rebuild: the unreadable registry could not be reset. RESULT=FAILED` | Error | 在 `DSH8336` 之后 |
| `DSH9110: '-{Flag}={Value}' is neither on nor off. A flag takes true, 1, yes or on, or false, 0, no or off, and '-{Flag}' alone is on; the command did nothing.` | Error | 任何命令：它读的某个标志有另一个值，包括空值；见[布尔标志](#boolean-flags) |
| `DreamShader {verb}: a flag's value is neither on nor off, so nothing was run. RESULT=FAILED` | Error | 在 `DSH9110` 之后，来自 `check`、`dump-ir`、`dump-layout`、`index`、`fmt`、`list-generated` 和 `pass-registry` |

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
decompile writes 2.0 text by default -- a .dss for a Material or MaterialFunction, a .dsi for a
MaterialInstanceConstant, a .dsp for a DreamPassPipeline; -Format=Legacy, or an -Out ending in .dsm
or .dsf, writes the 1.x text (a DreamPassPipeline has none).
-SourceFile decompiles every asset that source builds into one file; -KeepAssetPath keeps each asset's own path.
migrate rewrites 1.x sources (.dsm, .dsf, .dsh) as .dss; -Check verifies the rewrite and writes nothing.
-All takes the writable source roots; -Root names one root, a plugin's included, by its name or its plugin's.
fmt rewrites 2.0 sources in the printer's layout (-All: the writable roots); -Check writes nothing and fails
when a file would change. list-generated names every asset the sources build, building nothing.
compile, dump-graph, check, dump-ir and index take any compilable source -- .dss, .dsi,
.dsp, .dsm or .dsf; a .dsh header is compiled through the sources that include it.
pass-registry lists the Custom Pass HLSL slots and what each is (Live, Reserved, SnapshotMissing,
PipelineGone, PassGone, Unknown), writing nothing; -Gc frees the PipelineGone and PassGone ones;
-Rebuild compiles every .dsp again, frees the garbage and rewrites the registry files from
Registry.json -- a Registry.json that does not parse is moved aside and every slot assigned afresh.
check writes no asset at all; -Shaders is the exception -- a shader
compile needs a real material, so it builds and saves the products the way compile
does, then reports HLSL errors as stage: shader.
dump-graph is a developer tool: it writes one canonical JSON per generated asset and
never writes an asset itself. Compile the tree first if its sources have changed.
-Define (short form -D) may be repeated; -Define=NAME with no value is a bare marker that
defined(NAME) sees. Names starting with DS_ are reserved for the built-in constants.
A flag is written bare (-Force) or with true/false, 1/0, yes/no or on/off (-Force=false);
any other value is an error (DSH9110), and the command does nothing.
```

<a id="example"></a>

## 示例

编译单个源文件，绕过哈希短路：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -Source="C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

作为 CI 关卡，编译全部工程源文件 —— `.dsf` 在前，然后 `.dsm`：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -All -Force `
  -unattended -nopause -nosplash -stdout -log
```

整棵树编译两遍，``#if`` 的每个分支各一遍，以此作为 CI 闸门，防止未激活分支慢慢腐坏——未被选中的分支从不解析，因此没有其他东西会检查它：

```powershell
foreach ($Legacy in 0, 1) {
    & $UnrealEditorCmd $Project `
      -run=DreamShader compile -All -Force `
      -Define=MOONTOON_LEGACY_TOON=$Legacy `
      -unattended -nopause -nosplash -stdout -log
    if ($LASTEXITCODE -ne 0) { throw "DreamShader failed with MOONTOON_LEGACY_TOON=$Legacy" }
}
```

将已有材质反编译到指定的路径：

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader decompile -Asset="/Game/Materials/M_Existing" `
  -Out="C:/Projects/MyGame/DShader/Decompiled/Materials/M_Existing.dsm" `
  -unattended -nopause -nosplash -stdout -log
```

相对的 ``-Source`` 先按 ``DShader/`` 解析：

```powershell
-run=DreamShader compile -Source="Materials/M_Sample.dsm"
```

先为整个工程捕获一份图基线，然后再与之后的一份进行比较——两步的形式很重要，因为 ``dump-graph`` 不会重建磁盘上已存在的资产：

```powershell
& $UnrealEditorCmd $Project -run=DreamShader compile -All -Force `
  -unattended -nopause -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader dump-graph -All -Out="I:/Baseline/before" `
  -unattended -nopause -nosplash -stdout -log

# …change the compiler, rebuild, capture again…

git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

在 ``DShader/.dreampass/Registry.json`` 中发生冲突的合并之后，从源文件重建 slot 注册表，然后列出它：

```powershell
& $UnrealEditorCmd $Project -run=DreamShader pass-registry -Rebuild `
  -unattended -nopause -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader pass-registry `
  -unattended -nopause -nosplash -stdout -log
```

一次成功的两文件 ``-All`` 运行的控制台输出：

```text
LogDreamShader: Display: Generated MaterialFunction /Game/Functions/MF_Noise.MF_Noise from C:/Projects/MyGame/DShader/Functions/MF_Noise.dsf.
LogDreamShader: Display: Generated Material /Game/Materials/M_Sample.M_Sample from C:/Projects/MyGame/DShader/Materials/M_Sample.dsm.
```

<a id="see-also"></a>

## 另见

- [编辑器桥接](bridge.md) — commandlet 刻意不启动的一切
- [测试](../contributing/testing.md#graph-baseline-since-190) — ``dump-graph`` 生成的图基线
- [分歧](../generation/divergence.md) — dump 与其共享“什么算内容”规则的那个输出摘要
- [临时材质](../generation/ephemeral.md) — 持久生成与临时生成
- [缓存](../generation/caching.md) — ``-Force`` 绕过的源哈希跳过
- [预处理器](../language/preprocessor.md) — ``-Define`` 馈送的对象，以及其他四个 define 层级
- [资产路径](../generation/asset-paths.md) — ``Name=`` 和 ``Root=`` 如何变成 package 路径
- [反编译器](decompiler.md) — ``decompile`` 命令驱动的那个导出器
- [Package](packages.md) — 为什么 ``DShader/Packages`` 被 ``-All`` 跳过
- [源文件](../language/source-files.md) — ``.dsm`` / ``.dsf`` / ``.dsh`` 的角色
- [Custom Pass 管线 — ``.dsp``](../language-v2/passes.md) — ``pass-registry`` 保存其 slot 的那个源文件
- [HLSL pass](../runtime/hlsl.md) — slot、快照和预检查
- [工程设置](../settings/project.md) — ``SourceDirectory``，路径解析依赖它
- [测试](../contributing/testing.md) — 无头运行自动化测试套件
- [诊断索引](../diagnostics/index.md) — 每条消息，按阶段排列
