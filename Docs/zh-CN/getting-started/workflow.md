---
title: 日常工作流
description: 保存、防抖、生成这个循环 —— 什么会触发重建、什么会被跳过、改一个 .dsh 会让什么失效，以及出错时看哪里。
---

日常循环是：改源文件、保存、让插件编译、看结果。编辑器扩展负责改善"编辑"这一半；生成由 Unreal 插件
负责，而且只发生在编辑器进程里（或者 commandlet 和 cook 里）。

<a id="这个循环"></a>

## 这个循环

1. 修改 `DShader/` 下的源文件 —— `.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf` 或 `.dsh`。
2. 先处理编辑器扩展本地报出来的问题 —— 语法、未知标识符、错误的 import。
3. **保存。** 源目录监视器发现变化。
4. 防抖计时器等过 *Save Debounce Seconds* —— 默认 `0.25`，钳制到 `[0.05, 10.0]` —— 于是连续多次保存
   只触发一次编译。
5. DreamShader 编译该文件，并把结果输出到 Output Log。
6. 通过 *Tools ▸ DreamShader ▸ Material Content Browser* 或编辑器扩展的预览查看材质。

自动编译由两项项目设置控制：**Auto Compile On Save**（默认开启）和 **Save Debounce Seconds**。关闭
自动编译后，监视器完全忽略文件变化，需要手动触发编译。

<a id="一次编译做了什么"></a>

## 一次编译做了什么

（自 2.0.0 起） 每种源文件走同一条流程：`.dsm` / `.dsf` 由 legacy 前端读进与 `.dss` 相同的语法树，之后预处理、
绑定、降级成图 IR、校验、发射，和 `.dss` 完全一样。每个被 import 的 `.dsh` 被**单独**解析，它的声明被声明进当前文件 ——
不再把文本贴进来。

| 阶段 | 做什么 | 失败时 |
| --- | --- | --- |
| 文件类型闸门 | 构建 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`；`.dsh` 从不单独构建 | `DSH8296` |
| 预处理 | 用工程的 define 表求值 [`#if` / `#define`](../language/lexical.md#预处理器) （自 1.9.0 起） | `DSH1xxx` |
| 解析 | 两个前端之一 | `DSH2xxx`、`DSH3xxx` |
| 绑定 | 名字、类型、调用、设置、被 import 的 header | `DSH4xxx`–`DSH7xxx`、`DSH8292`–`DSH8295` |
| 构建键 | 文件和每个被 import 的 header 的哈希，加上决定输出的设置与版本 | —— |
| 降级、校验 | 把绑定后的树降成图 IR，折叠、去重、剪枝，再校验 | `DSH43xx`、`DSH6xxx` |
| 发射 | 每个产物一个资产，按依赖顺序：同一文件里被调用的函数先建 | `DSH8xxx` |

一个阶段会报出它的**全部**错误，有错误的阶段会在下一阶段开始前停下。完整的表见[生成流程](../generation/index.md#阶段)。

成功时每个产物一行，有警告时后面跟一个 `Warnings:` 块：

```text
Generated Material /Game/Materials/M_Emissive from .../M_Emissive.dss.
Generated MaterialFunction /Game/Functions/F_Tint from .../F_Tint.dss.
Skipped /Game/Materials/M_Other from .../M_Other.dsm; source hash is unchanged (build key 9f2c41ab).
Compiled .../Helpers.dsm; it declares no material and no exported function, so no asset was written.
```

`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline`。
只声明 helper 或 `VirtualFunction` 的文件编译成功、什么都不写；一个块都没有的 1.x 文件是**失败**（`DSH2254`）。

<a id="各种文件在实际使用中的区别"></a>

## 各种文件在实际使用中的区别

| 种类 | 你改它是为了 | 保存它会 |
| --- | --- | --- |
| `.dss` | 改一个材质，或一组材质函数 / layer / blend | 编译它的每个 `export` |
| `.dsi` | 改一个材质实例的父材质或覆盖项 | 编译这个实例；父材质缺失或比源文件旧时先编父材质 |
| `.dsp` | 改一条 Custom Pass 管线（UE 5.8） | 编译管线资产，以及导出 buffer 的 render target |
| `.dsm` | 改一个 1.x 材质 | 编译该材质，以及同文件里声明的函数资产 |
| `.dsf` | 改一个 1.x 函数资产 | 编译它声明的函数资产 |
| `.dsh` | 改共享 helper | **自己什么都不生成** —— 把每个 import / include 它的源文件排进编译队列 |

<a id="什么会触发编译"></a>

## 什么会触发编译

| 触发方式 | 强制 | ThinCustom 结果 | 范围 |
| --- | --- | --- | --- |
| 保存时自动编译 —— 文件监视器加防抖 | 否 | Ephemeral | 保存的文件；保存 `.dsh` 时是每个 import 它的源文件 |
| 编辑器启动扫描，以及改了构建键覆盖的设置之后的扫描 | 否 | Ephemeral | 全部工程源文件 |
| *Recompile DSM*、*Clean Generated Shaders* | 是 | Ephemeral | 全部工程源文件 |
| Material Content Browser 的 *Compile* / 缩略图刷新 | 是 | Ephemeral | 单个源文件 |
| 实时预览渲染 | 视请求而定 | Ephemeral | 单个源文件 |
| *Materialize*，以及为 Ephemeral 材质创建子实例 | 是 | **Materialized** | 单个源文件 |
| Commandlet `-run=DreamShader` | 带 `-Force` 时 | **Materialized** | 按指定 |
| Cook，仅在 cook director 进程上 | 是 | **Materialized** | 全部工程源文件 |

**提示：**

只有 `ThinCustom` 产物能活在内存里：交互式编辑器把它保持为 **Ephemeral**，直到 cook、commandlet 或显式 *Materialize*
  把它写到磁盘。`Graph` backend 的材质，以及每个材质函数、layer 和 blend，每次构建成功都存盘 （自 2.0.0 起）。
  见[内存材质](../generation/ephemeral.md)。

一批文件按依赖顺序编译：被 import 的 header、`.dsi` 的父材质、`.dsp` 引用的材质先编。`compile -All` 和 cook 先编所有 `.dsf`，
再分轮重试失败的文件，直到某一轮再也建不出新东西 （自 2.1.2 起） —— 所以依赖另一个源文件生成的函数的材质，不再因为
路径排在前面而失败。

<a id="什么时候会跳过重建"></a>

## 什么时候会跳过重建

每个生成的资产在 package 元数据里存两个键：

| 键 | 值 |
| --- | --- |
| `DreamShader.SourceFile` | 相对项目目录的源文件路径，使用正斜杠 |
| `DreamShader.SourceHash` | **构建键**的 CRC32，八位十六进制 |

构建键覆盖文件和它 import 的每个 header 的**预处理后**文本、它读到的 define、决定输出的设置，以及插件和引擎的版本。
非强制编译时，如果资产存在、存储的源路径匹配（忽略大小写）、存储的构建键匹配，这次工作就会被跳过，结果是
`Skipped {AssetPath} from {File}; source hash is unchanged (build key {BuildKey}).`

| 改动 | 效果 |
| --- | --- |
| 改 `M_Foo.dss` | 让 `M_Foo.dss` 失效 |
| 改被两个材质 import 的 `Common.dsh` | 让**两个**材质都失效 |
| 改一个 *Preprocessor Defines* 里的值 | 只让读到这个名字的源文件失效 |
| 调整空白、改注释 | 也会失效 —— 比较的是文本，不是语义 |
| 把项目挪到别的目录、另一个人检出同一个仓库 | 什么都不失效 —— 路径按项目相对记录 （自 2.1.1 起） |
| 升级插件 | 每个资产重建一次 —— 版本在构建键里；2.1.2 的键格式是 `DSK7` |
| 重命名源文件 | 存储的路径不再匹配，于是不会跳过任何东西 |

要通过非强制入口触发重建，只能改源文本，或者删掉生成的资产。*Recompile DSM* 和 commandlet 的 `-Force` 越过构建键。
见[重新生成 ▸ 构建键](../generation/regeneration.md#构建键)。

<a id="改-dsh-之后"></a>

## 改 `.dsh` 之后

保存 header 会把每个 import / include 它的源文件排进编译队列，它们各自按构建键判断要不要重建。直接编译一个 `.dsh`
（比如 commandlet `-Source` 指向它）是 `DSH8296` —— header 从不单独构建。

Material Content Browser 把 header 显示为 `function · used by N material(s)`，检查器里列出它的依赖方，改之前就能看到影响范围。

<a id="一次重建会毁掉什么"></a>

## 一次重建会毁掉什么

生成的资产是源文件的产物，不是文档。重新生成会清掉生成的注释框、删除所有表达式、把材质设置重置为引擎默认值，然后重新应用
设置并重建。资产内部的手动改动会消失，只有两个例外：文本**不以** `DreamShader: ` 开头的注释框，以及生成的 ThinCustom 实例上
的参数覆盖 —— 后者按名字放回 （自 1.9.0 起），在源码里改名或删掉的参数则丢掉它的覆盖值。

**警告：**

**手改过的资产不会被悄悄覆盖。** 资产生成后在编辑器里被改过，下一次编译拒绝重建它（`DSH8207`），并给出三个出口：
  **Revert to Source**（源码是对的）、**Adopt Into Source**（资产是对的，反编译回源码）、**Detach From DreamShader**（都不要，
  不再管理它）。`-Force` 也绕不过去。资产开着编辑器时同样拒绝重建（`DSH8206`）—— 关掉编辑器标签页再编译。
  见[重新生成](../generation/regeneration.md)。

DreamShader 也拒绝覆盖不是自己生成的资产：目标路径上已有一个没有 DreamShader 来源元数据的材质时，编译以 `DSH8201` 失败。

<a id="出错时看哪里"></a>

## 出错时看哪里

每条诊断都带 `DSHnnnn` 码，报在它所说那个构造的行列上：

```text
I:/MyProject/DShader/Materials/M_Minimal.dss(8,24): DSH4200: 'Tnt' is not declared; did you mean 'Tint'? Names are case-sensitive.
```

| 位置 | 显示什么 |
| --- | --- |
| Output Log | `LogDreamShader` 下的全部消息：先错误、再警告、再 note |
| Material Content Browser | 源文件那一行显示第一个错误，检查器列出全部记录 |
| 编辑器扩展 | 按文件的完整列表，定位到行列；成功编译的警告也在 （自 2.1.0 起） |

在 header 里报的记录指向 header 本身，不是导入它的文件。到[错误速查](../diagnostics/index.md)里查码。

<a id="迁移已有材质"></a>

## 迁移已有材质

在 Content Browser 里右键 `UMaterial`、`UMaterialFunction` 或材质实例，使用 *DreamShader ▸ Export .dss* / *Export .dsi*
（1.x 文本用 *Export Legacy .dsm / .dsf*）。输出落在 `DShader/Decompiled/` 下并会自动打开；反编译器表达不了的东西写在文件头部，
每条带码。把它当作初稿 —— 在正式作为源文件之前，先整理命名、helper 分层和 include。见[反编译导出](../tools/decompiler.md)。

已有的 `.dsm` / `.dsf` 用 [`dsc migrate`](../tools/migrate.md) 改写成 `.dss`，它在写盘之前先证明两者构建出同一张图。

<a id="下一步"></a>

## 下一步

- [生成流程](../generation/index.md) —— 每个阶段和它可能给出的码
- [重新生成](../generation/regeneration.md) —— 完整的"什么能保留"清单
- [编辑器工具](../tools/editor-integration.md) —— Tools 菜单、Material Content Browser、Make Ephemeral
- [VSCode 与 Rider](../tools/vscode.md) —— 诊断、预览和 Package 命令
- [命令行](../tools/commandlet.md) —— headless 编译
