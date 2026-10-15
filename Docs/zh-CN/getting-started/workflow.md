---
title: "日常工作流"
description: "保存、防抖与生成这个循环——什么会触发重建、什么会被跳过、编辑一个 .dsh 会让什么失效，以及出错时该看哪里。"
---

日常循环是：编辑源文件、保存、让插件编译、查看结果。编辑器扩展改善的是
*编辑*这一半；生成属于 Unreal 插件，且只发生在编辑器进程里（或在 commandlet 或
cook 里）。

<a id="the-loop"></a>

## 这个循环

1. 编辑 `DShader/` 下的一个源文件——一个 `.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf` 或 `.dsh`。
2. 先修复编辑器扩展在本地报告的问题——语法、未知标识符、错误的导入。
3. **保存。** 源目录监视器发现变化。
4. 一个防抖计时器等待 *Save Debounce Seconds*——默认 `0.25`，钳制到 `[0.05, 10.0]`——因此连续多次保存
   只触发一次编译。
5. DreamShader 编译该文件，并把结果写入 Output Log。
6. 通过 *Tools ▸ DreamShader ▸ Material Content Browser* 或编辑器扩展的预览查看材质。

自动编译由两项项目设置控制：**Auto Compile On Save**（默认开启）和 **Save Debounce
Seconds**。关闭自动编译后，监视器会完全忽略文件变化，你需要手动触发编译。

<a id="what-one-compile-does"></a>

## 一次编译做了什么

（自 2.0.0 起）每种源文件走同一条路径：一个 `.dsm` / `.dsf` 由旧版前端读入，形成与
`.dss` 相同的语法树，然后像它一样经历预处理、绑定、降级为图 IR、校验和发射。
每个被导入的 `.dsh` 都**单独**解析，它的声明成为当前文件的名字——不再把文本粘贴
进来。

| 阶段 | 做什么 | 失败时 |
| --- | --- | --- |
| 文件类型闸门 | 构建 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`；`.dsh` 从不单独构建 | `DSH8296` |
| 预处理 | 用工程的 define 表求值 [`#if` / `#define`](../language/preprocessor.md)（自 1.9.0 起） | `DSH1xxx` |
| 解析 | 两个前端之一 | `DSH2xxx`、`DSH3xxx` |
| 绑定 | 名字、类型、调用、设置、被导入的 header | `DSH4xxx`–`DSH7xxx`、`DSH8292`–`DSH8295` |
| 构建键 | 文件和每个被导入 header 的哈希，加上决定输出的设置与版本 | —— |
| 降级、校验 | 把绑定后的树降级为图 IR，折叠、去重、剪枝，然后校验 | `DSH43xx`、`DSH6xxx` |
| 发射 | 每个产物一个资产，按依赖顺序：同一文件中被调用的函数先构建 | `DSH8xxx` |

一个阶段会报告它的**全部**错误，有错误的阶段会在下一个阶段开始前停下。完整的表见
[Pipeline](../generation/index.md#pipeline)。

成功时每个产物一行，有警告时后面跟一个 `Warnings:` 块：

```text
Generated Material /Game/Materials/M_Emissive from .../M_Emissive.dss.
Generated MaterialFunction /Game/Functions/F_Tint from .../F_Tint.dss.
Skipped /Game/Materials/M_Other from .../M_Other.dsm; source hash is unchanged (build key 9f2c41ab).
Compiled .../Helpers.dsm; it declares no material and no exported function, so no asset was written.
```

`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或
`PassPipeline`。一个只声明 helper 或 `VirtualFunction` 的文件会编译成功、什么都不写入；一个完全没有
块的 1.x 文件是**失败**（`DSH2254`）。

<a id="the-file-kinds-in-practice"></a>

## 各种文件在实际使用中的区别

| 种类 | 你编辑它是为了 | 保存它会 |
| --- | --- | --- |
| `.dss` | 改一个材质，或一组材质函数 / 层 / 混合 | 编译它的每个 `export` |
| `.dsi` | 改一个材质实例的父材质或覆盖项 | 编译该实例；父材质缺失或比其源文件旧时先编译父材质 |
| `.dsp` | 改一条 Custom Pass 管线（UE 5.8） | 编译管线资产，以及其导出缓冲区的渲染目标 |
| `.dsm` | 改一个 1.x 材质 | 编译该材质以及同一文件中声明的函数资产 |
| `.dsf` | 改一个 1.x 函数资产 | 编译它声明的函数资产 |
| `.dsh` | 改共享 helper | **自己什么都不生成** —— 把每个导入或包含它的源文件排进队列 |

<a id="what-triggers-a-compile"></a>

## 什么会触发编译

| 触发方式 | 强制 | ThinCustom 结果 | 范围 |
| --- | --- | --- | --- |
| 保存时自动编译 —— 文件监视器加防抖 | 否 | Ephemeral | 保存的文件；对于 `.dsh`，是每个导入它的源文件 |
| 编辑器启动扫描，以及构建键覆盖的设置变化后的扫描 | 否 | Ephemeral | 全部工程源文件 |
| *Recompile DSM*、*Clean Generated Shaders* | 是 | Ephemeral | 全部工程源文件 |
| Material Content Browser 的 *Compile* / 缩略图刷新 | 是 | Ephemeral | 单个源文件 |
| 实时预览渲染 | 视请求而定 | Ephemeral | 单个源文件 |
| *Materialize*，以及为 Ephemeral 材质创建子实例 | 是 | **Materialized** | 单个源文件 |
| Commandlet `-run=DreamShader` | 带 `-Force` 时 | **Materialized** | 按指定 |
| Cook，仅在 cook director 进程上 | 是 | **Materialized** | 全部工程源文件 |

**注意：**

只有 `ThinCustom` 产物能活在内存里：交互式编辑器把它保持为 **Ephemeral**，直到 cook、
commandlet 或显式 *Materialize* 把它写到磁盘。`Graph` 后端的材质，以及每个材质函数、
层和混合，每次构建成功都会存盘（自 2.0.0 起）。见
[In-memory Materials](../generation/ephemeral.md)。

一批文件按依赖顺序编译：被导入的 header、`.dsi` 的父材质，以及 `.dsp` 引用的材质会
先编译。`compile -All` 和 cook 会先编译所有 `.dsf`，然后分轮重试失败的文件，直到某一轮再也建不出
新东西（自 2.1.2 起）—— 因此调用另一个源文件所生成函数的材质，不再仅仅因为其
路径排在前面而失败。

<a id="when-a-rebuild-is-skipped"></a>

## 什么时候会跳过重建

每个生成的资产都会存储两个 package 元数据键：

| 键 | 值 |
| --- | --- |
| `DreamShader.SourceFile` | 相对项目目录的源文件路径，使用正斜杠 |
| `DreamShader.SourceHash` | **构建键**的 CRC32，八位十六进制 |

构建键覆盖该文件及其导入的每个 header 的**预处理后**文本、它读取到的 define、决定输出的
设置，以及插件和引擎版本。在非强制编译时，如果资产存在、存储的源路径匹配（忽略大小写）
且存储的构建键匹配，这次工作就会被跳过，结果为
`Skipped {AssetPath} from {File}; source hash is unchanged (build key {BuildKey}).`

| 改动 | 效果 |
| --- | --- |
| 编辑 `M_Foo.dss` | 使 `M_Foo.dss` 失效 |
| 编辑被两个材质导入的 `Common.dsh` | 使**两个**材质都失效 |
| 修改 *Preprocessor Defines* 中的一个值 | 只使读取该名字的源文件失效 |
| 调整空白、编辑注释 | 也会失效 —— 比较的是文本，不是语义 |
| 移动项目，或在别处检出同一仓库 | 什么都不失效 —— 路径按项目相对存储（自 2.1.1 起） |
| 升级插件 | 每个资产重建一次 —— 版本在构建键里；2.2.0 的键格式是 `DSK7`，与 2.1.2 相同 |
| 重命名源文件 | 存储的路径不再匹配，因此不会跳过任何东西 |

要通过非强制触发方式强制重建，请修改源文本或删除生成的资产。*Recompile DSM*
和 commandlet 的 `-Force` 会越过构建键。见 [重新生成 ▸ 构建键](../generation/caching.md)。

<a id="editing-a-dsh"></a>

## 编辑一个 `.dsh`

保存 header 会把每个 import 或 include 它的源文件都排进队列，每个源文件再按自己的构建键决定是否
重建。直接编译一个 `.dsh`（比如某个 commandlet 的 `-Source` 指向它）会产生 `DSH8296` —— header
从不单独构建。

Material Content Browser 把 header 显示为 `function · used by N material(s)`，检查器里列出它的依赖方，改之前就能看到影响范围。

<a id="what-a-rebuild-destroys"></a>

## 一次重建会毁掉什么

生成的资产是源文件的产物，不是文档。重新生成会清掉生成的注释框、删除所有表达式、把材质设置重置为引擎默认值，然后重新应用
设置并重建。资产内部的手动改动会消失，只有两个例外：文本**不以** `DreamShader: ` 开头的注释框，以及生成的 ThinCustom 实例上
的参数覆盖 —— 后者按名字放回（自 1.9.0 起），在源码里改名或删掉的参数则丢掉它的覆盖值。

**警告：**

**手动编辑过的资产不会被静默覆盖。** 生成后在编辑器中编辑过的资产会被下一次编译拒绝（`DSH8207`），并给出三条出路：**Revert to Source**（源文件是对的）、**Adopt Into Source**（资产是对的；将其反编译回去）、**Detach From DreamShader**（两者都不对；不再管理它）。`-Force` 也无法绕过这一点。在资产编辑器中打开的资产同样会被拒绝（`DSH8206`）—— 关闭该编辑器选项卡后重新编译。
参见[重新生成](../generation/regeneration.md)。

DreamShader 还会拒绝覆盖不是它生成的资产：目标路径上存在没有 DreamShader 来源元数据的材质时，编译会以 `DSH8201` 失败。

<a id="when-something-fails"></a>

## 出错时

每条诊断都带有 `DSHnnnn` 代码，并报告在其所指构造的行和列处：

```text
I:/MyProject/DShader/Materials/M_Minimal.dss(8,24): DSH4200: 'Tnt' is not declared; did you mean 'Tint'? Names are case-sensitive.
```

| 位置 | 显示内容 |
| --- | --- |
| Output Log | `LogDreamShader` 下的所有消息：先是错误，再是警告，然后是注释 |
| Material Content Browser | 源文件所在行显示第一个错误，检查器中显示每条记录 |
| Editor 扩展 | 按文件列的列表，定位到行和列；成功编译的警告也会显示（自 2.1.0 起） |

在头文件中引发的记录指向该头文件，而不是导入它的文件。请在[诊断](../diagnostics/index.md)中查找该代码。

<a id="migrating-an-existing-material"></a>

## 迁移现有材质

在 Content Browser 中右键单击 `UMaterial`、`UMaterialFunction` 或材质实例，然后使用 *DreamShader ▸ Export
.dss* / *Export .dsi*（1.x 文本使用 *Export Legacy .dsm / .dsf*）。输出会落在 `DShader/Decompiled/` 下并自动打开；反编译器无法表述的内容会列在文件顶部，每项都带有其代码。把它当作初稿 —— 在采用为源文件之前，先整理好命名、helper 分层与 include。参见
[反编译器](../tools/decompiler.md)。

现有的 `.dsm` / `.dsf` 文件会由 [`dsc migrate`](../tools/migrate.md) 重写为 `.dss`，它在写入之前会证明两者构建出相同的图。

<a id="next"></a>

## 下一步

- [管线](../generation/index.md) —— 每个阶段及它可能引发的代码
- [重新生成](../generation/regeneration.md) —— 可保留内容的完整列表
- [编辑器工具](../tools/editor-integration.md) —— Tools 菜单、Material Content Browser、Make Ephemeral
- [VSCode 与 Rider](../tools/vscode.md) —— 诊断、预览与 Package 命令
- [Commandlet](../tools/commandlet.md) —— 无头编译
