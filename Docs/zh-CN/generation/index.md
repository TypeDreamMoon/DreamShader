---
title: 生成流程
description: 一个 DreamShaderLang 源文件如何变成 Unreal 资产 —— 每个阶段、触发条件，以及各阶段可能的失败。
---

DreamShaderLang 的编译目标不是字节码，也不是可执行文件，而是 **Unreal 资产**：一个 `UMaterial` 或
`UDreamShaderMaterialInstance`，以及若干材质函数资产。本页跟着一个源文件走完全程。

| 项目 | 说明 |
| :-- | :-- |
| 输入 | 一个 `.dsm` 或 `.dsf` 文件，加上它 `import` 的每一个 `.dsh`；同一条流程也编译 `.dss`、`.dsi` 和 `.dsp` （自 2.0.0 起） |
| 拒绝的输入 | `.dsh` —— 头文件只作为 import 它的那个文件的一部分被编译（`DSH8296`） |
| 产物 | `UMaterial` / `UDreamShaderMaterialInstance`、`UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend`。不再生成 `.ush` include （自 2.0.0 起） —— 见下面的 [Function 的 HLSL](#function-的-hlsl) |
| 运行环境 | 仅编辑器进程 —— 发射器依赖 editor-only 的材质 API |

没有运行时路径。打包后的游戏里不存在从 DreamShaderLang 构建材质的代码，那时资产早已存在。

<a id="阶段"></a>

## 阶段

（自 2.0.0 起） `.dsm` / `.dsf` 由 **legacy 前端** 读进与 `.dss` 解析器相同的语法树，之后走每种源文件共用的
同一条流程。一个阶段会报出它找到的**全部**错误；报了错的阶段会在下一阶段开始前停下整次编译。每条诊断都有稳定的
`DSHnnnn` 码，格式是 `<file>(<line>,<col>): DSHnnnn: <message>` —— 见[错误速查](../diagnostics/index.md)。

| # | 阶段 | 做什么 | 失败时 |
| :-- | :-- | :-- | :-- |
| 1 | 规范化路径 | 完整路径、规范化 | — |
| 2 | 文件类型闸门 | 构建 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`；`.dsh` 从不单独构建 | `DSH8296` |
| 3 | 读取 | 文件文本 | `DSH8290` |
| 4 | 预处理 | 用工程的 define 表求值 [`#if` / `#define`](../language/lexical.md#预处理器) （自 1.9.0 起），`.dsm` / `.dsf` 按 1.x 方言；指令行变成空行，所以位置不动 | `DSH8291`，内含预处理器自己的 `DSH1030`–`DSH1042` |
| 5 | 解析 | legacy 前端读 1.x 的块、section 和 `Graph` 语句 | `DSH2200`–`DSH2258`、`DSH3250`–`DSH3278`、`DSH5250`–`DSH5265`、`DSH6300`–`DSH6319`，以及词法器的 `DSH2101`–`DSH2106` |
| 6 | 绑定 | 名字、类型、调用、设置；每个被 import 的 `.dsh` 被**单独**解析、预处理和解析，它的声明被声明进当前文件 | `DSH4200`–`DSH4264`、`DSH5200`–`DSH5231`、legacy 规则 `DSH5275`–`DSH5292`、`DSH6200`–`DSH6211`、`DSH7200`–`DSH7233`；import 解析不到、读不了或解析失败是 `DSH8292`–`DSH8295` |
| 7 | 构建键 | 每个被 import 的头文件和本文件的哈希，加上决定输出的设置与版本 | — 见[重新生成](regeneration.md#构建键) |
| 8 | 降级 | 把绑定后的树降成图 IR，Custom 节点代码也在这一步 | `DSH4350`–`DSH4383`、`DSH6220`–`DSH6223`、`DSH6250`–`DSH6264`、`DSH6327`、`DSH6328` |
| 9 | Pass 与校验 | 常量折叠、去重、剪掉死节点；IR 校验器 | `DSH4390`；`DSH4300`–`DSH4335` |
| 10 | 发射 | 每个产物一个资产，按依赖顺序 —— 见[下面](#发射内部) | `DSH8200`–`DSH8237`；两个互相调用的导出函数是 `DSH8299` |
| 11 | 组装结果 | 每个产物一行，外加一个 `Warnings:` 块 —— 见[成功消息](#成功消息) | — |

`.dsi` 在解析之后解析它的父材质，`.dsp` 解析它的 pass 引用的材质和 shader 文件；见
[材质实例（.dsi）](../language-v2/instances.md)和 [Custom Pass（.dsp）](../language-v2/passes.md)。

失败的编译把**第一个错误**放在第一行，然后是本次运行的其余诊断：错误，再警告，再 info。结果的码就是第一个错误的码。

编辑器的 *compile material* 入口跑的是和 *compile assets* 同一次编译 （自 2.0.0 起）：一个源文件就是一个编译单元，
它的材质可以调用同一文件里声明的函数，那个函数会先被发射。

<a id="发射内部"></a>

### 发射内部

每个产物 —— 一个 `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 块 —— 按这个顺序走。
资产层的诊断经由发射器的诊断到达结果：发射器的消息引用资产层的码和文本。

| # | 子阶段 | 失败时 |
| :-- | :-- | :-- |
| 1 | 从 `Name=` / `Root=` 解析目标 | `DSH8200` —— 见[资产路径](asset-paths.md) |
| 2 | 创建或复用资产：类检查与[归属守卫](regeneration.md#归属守卫) | `DSH8201`（`Graph` 材质）、`DSH8203`（ThinCustom 实例）、`DSH8202`（函数、layer、blend） |
| 3 | 另一个编辑器持有本工程生成资产的写入权 | `DSH8209`，info：跳过，不算失败 |
| 4 | source hash 短路 | `DSH8237`，info：跳过 —— 见[重新生成](regeneration.md#什么时候会跳过重新生成) |
| 5 | 资产开在某个资产编辑器里 | `DSH8206` |
| 6 | 手改检测 | `DSH8207` —— 见[手改检测](regeneration.md#手改检测) |
| 7 | ThinCustom：先抓下实例上的参数覆盖，再找到或创建隐藏 base | `DSH8230` |
| 8 | 构建图、应用设置、布局、重新编译 —— 原子地 | 见下面和[重新生成](regeneration.md) |
| 9 | 打上来源元数据和输出指纹 | — |
| 10 | 保存，除非产物是 Ephemeral 的 ThinCustom 实例或写入被拒绝 | `DSH8229` |

1.x 生成器在这里做的检查 —— 存在顶层 `Shader`、`Outputs` 非空、`Backend` 认得 —— 现在归前端：没有 `Outputs`
的 `Shader` 是**警告** `DSH2256`，照样构建；既无 `Graph`、`Outputs` 也什么都没算的 `Shader` 是 `DSH2255`；
不是 backend 的 `Backend` 是 `DSH7202`（空值是 `DSH7201`；`Instance` 按 `ThinCustom` 读并给出 `DSH7204`）。

<a id="图构建内部"></a>

### 图构建内部

两种 backend 共用。在 `ThinCustom` 下，它作用于隐藏的 base 材质，而不是发出的那个实例。

1. 清除生成的 `DreamShader: ` 注释框，然后把现有的图**摘下来** —— 保留着，失败时放回去（无法打快照时警告 `DSH8208`）。
2. 把每一个材质属性重置为引擎默认值。
3. 应用 `Settings`（材质拒绝的设置是 `DSH8215`）。
4. 逐节点发射 IR 图 —— 参数、表达式、每次 `Function` 调用一个 Custom 节点、每次 `ShaderFunction` 调用一个
   `MaterialFunctionCall` —— 并通过一对 `DS_<output>` 命名 reroute 连接每条 `Outputs` 绑定。引擎建不出来的节点是
   `DSH8210`–`DSH8228`。
5. 按工程的 **Graph Layout Style** 布局 —— 每次构建都运行 （自 2.0.0 起），见 [Layout 与 #Region](../language/layout.md)。
6. 提交 —— 被摘下的旧图到这时才销毁 —— 并重新编译材质。

第 1 步就是"手改生成出来的资产没有意义"的原因 —— 见[重新生成](regeneration.md)。

<a id="什么会触发编译"></a>

## 什么会触发编译

| 触发方式 | 强制 | ThinCustom 结果 | 范围 |
| :-- | :-- | :-- | :-- |
| 保存时自动编译 —— 文件监视器加防抖 | 否 | Ephemeral | 保存的文件；保存一个 `.dsh` 会把每个 import 它的源文件排进队列 |
| 编辑器启动扫描，以及改了[构建键](regeneration.md#构建键)覆盖的设置之后的扫描 | 否 | Ephemeral | 全部工程源文件 |
| *Recompile DSM*、*Clean Generated Shaders* | 是 | Ephemeral | 全部工程源文件 |
| Material Content Browser 的 *Compile* / 缩略图刷新 | 是 | Ephemeral | 单个源文件 |
| 实时预览渲染器 | 经 bridge 是；经预览 WebSocket 只在请求带 `force` 时 | Ephemeral | 单个源文件 |
| *Materialize*，以及为 Ephemeral 材质创建子实例 | 是 | **Materialized** | 单个源文件 |
| 一个 `.dsi` 的父材质缺失或比它的源文件旧（`DSH8264`） | 否 | **Materialized** | 先编父材质的源文件 |
| Commandlet `-run=DreamShader` | 带 `-Force` 时 | **Materialized** | 按指定 |
| Cook，仅在 cook director 进程上 | 是 | **Materialized** | 全部工程源文件 |

保存时自动编译由两个项目设置控制：**Auto Compile On Save**（默认开）与 **Save Debounce Seconds**
（默认 `0.25`，钳制在 `[0.05, 10.0]`）。关掉前者，源目录监视器会完全忽略文件变化。见
[项目设置](../settings/project.md)。

**提示：**

**只有 ThinCustom 产物能活在内存里。** 交互式编辑器把它保持为 Ephemeral，直到 cook、commandlet 或一次显式的
*Materialize* 把它落盘。`Graph` backend 的材质，以及每一个材质函数、layer 和 blend，每次允许写入的构建都会保存
（自 2.0.0 起）。见[内存材质](ephemeral.md)。

**提示：**

**一批文件按依赖顺序编译** （自 1.8.0 起）。调用 `ShaderFunction` 的材质会把调用节点绑到**活着的**
`UMaterialFunction` 资产上 —— 发射器从对象而不是从源码上读 pin —— 所以先编调用方就绑到了那个函数上一版的接口。
一批里同时有文件和它依赖的东西（它 import 的头文件、`.dsi` 的父材质、`.dsp` 引用的材质）时，依赖先编；同一个文件内，
被其他产物调用的函数也先构建，互相调用的两个导出函数是 `DSH8299`。成环的文件按遍历到的顺序留着，由编译用 `DSH4211` 拒绝。
`compile -All` 和 cook 还会**分轮重试** （自 2.1.2 起），见[命令行](../tools/commandlet.md#-all-的发现与排序)。

<a id="不产出资产的结果"></a>

## 不产出资产的结果

一个源文件可以编译成功，却没有任何东西能放进 Content Browser。

| 源文件 | 结果 |
| :-- | :-- |
| 只声明 `Function`、`GraphFunction`、`Namespace` 或 `VirtualFunction` 块的 `.dsm` / `.dsf` | **成功**：`Compiled {File}; it declares no material and no exported function, so no asset was written.` |
| 一个块都没有的 `.dsm` / `.dsf` | **失败**，`DSH2254` |

<a id="成功消息"></a>

## 成功消息

每个产物一行，按发射顺序。这些行是一种线格式：脚本按 `Generated <Kind> <ObjectPath> from <Source>.` 读出一次运行写了什么。

| 消息 | 对应 |
| :-- | :-- |
| `Generated {Kind} {ObjectPath} from {File}.` | 每个构建出的产物。`{Kind}` 是 `Material`（两种 backend 都是）、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline`；`{ObjectPath}` 是 object 路径，`/Game/M.M` |
| `Generated RenderTarget {ObjectPath} from {File}.` | `.dsp` 每个导出 buffer 的 render target （自 2.1.0 起） |
| `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` | 被哈希短路跳过的产物，任何种类 |
| `Skipped {ObjectPath}; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk.` | 写入权在另一个编辑器手里的产物（`DSH8209`） |
| `Compiled {File}; it declares no material and no exported function, so no asset was written.` | 没有产物的源文件 |

`{File}` 是完整、规范化的源文件路径。这些行下面，一个 `Warnings:` 标题列出本次运行的每一条警告（线格式），
然后是生成期警告 `DSH8155`、`DSH9011` 和 `DSH9012`。警告永远不会让编译失败。

**提示：**

（自 2.0.0 起） 1.x 的 `Generated {AssetPath} from {File}.`、`Generated DreamShader thin-custom material …`、
` (virtual)` 后缀和 helper include 那一行都没有了：每个产物都说 `Generated` 加它的种类，被哈希跳过的函数也说
`Skipped`，而不是什么都不说。

本页中的 `{Placeholder}` 表示编译器在运行时替换的值。

<a id="进度反馈"></a>

## 进度反馈

生成过程通过 Unreal 的 slow task 系统汇报。对话框有延迟，所以一次快速编译不会闪出窗口；它带一个 **Cancel** 按钮
（自 1.9.0 起），在 `IsRunningCommandlet()` 下则完全不弹。

| 范围 | 标题 | 帧 | 对话框延迟 |
| :-- | :-- | :-- | :-- |
| 一次编译 | `Compiling DreamShader source '{File}'...` | 6：`Reading`、`Parsing`、`Resolving names in`、`Lowering … to IR`、`Validating the IR of`、`Building the graph for '{File}'...` | 0.35 s |
| `Classic` 布局（嵌套） | `Laying out DreamShader material graph...` | 每个节点一帧 | 继承 |

前五帧是前端，很快。最后一帧 `Building the graph for …` 包含发射，而发射以 shader 编译收尾，耗时没有上限。
进度条停在这一帧是正常的：shader 编译发生在插件之外，一个很重的排列就能占它好几分钟。

<a id="取消"></a>

### 取消

（自 1.9.0 起）

每个阶段之前、每个产物之前，以及发射内旧图被摘下之后，都会检查取消。取消走的是失败编译的同一条路：

- 资产恢复成编译开始前的样子 —— 靠的是原子重建的回滚，所以不会有半成品图或被清空的材质函数。
- 什么都不打标记，什么都不保存。
- 编译报 `DSH8298` 错误（1.x 是 `DSH9010`）。

唯一不会放回去的是生成的 `DreamShader: ` 注释框，原因和失败重建一样：它们在快照之前就被删了。下一次成功编译会重建它们。

**警告：**

取消不会停下 Unreal 已经排队的 shader。在 shader 编译阶段取消，`ShaderCompileWorker.exe` 仍会把已派发的活干完。

<a id="shader-编译太久时"></a>

### shader 编译太久时

（自 1.9.0 起）

一个资产的 shader 编译超过 **30 秒**，生成会发一次 `DSH9011` 警告（日志里和结果的 `Warnings:` 块里）。它从不阻塞或
让编译失败，只是说明这种等待通常意味着什么：一个循环上界是**输入 pin** 的 `Custom` 节点（`for` / `while` 的界不是字面量
也不是 `#define` 常量），加上发散控制流里的**隐式 mip** 采样（`Texture2DSample`、`Texture3DSample`、`.Sample`）——
编译器不知道迭代次数，只能完全展开。任务管理器里 `ShaderCompileWorker.exe` 在忙就说明还在编译而不是卡死。
修法：用字面量或 `#define` 给循环定界，或者改用 `SampleLevel` / `SampleGrad`。

发射 `Custom` 节点代码时也会找这一对组合，在编译开始之前就提前警告，码是 `DSH9012`。两者都只是提示。

<a id="function-的-hlsl"></a>

## Function 的 HLSL

（自 2.0.0 起） `Function` 的 HLSL 写进**每次调用生成的 Custom 节点**里：节点的 `Code` 带着被调函数的函数体（原样），
以及函数体调用到的每一个 `Function`（嵌在一个 wrapper 结构体里）；函数体开头的 `#include` 行被提到节点的
`IncludeFilePaths`。代码在资产里，由构建键覆盖的源码派生，所以没变的源文件和其他东西一起被跳过。
具体的代码形状和符号改名见 [函数](../language/functions.md#生成的-hlsl)。

1.x 为每个源文件写一个 `/DreamShaderGenerated/<basename>_<hash>.ush`。虚拟目录 `/DreamShaderGenerated` 仍然映射到
**Generated Shader Directory** 项目设置（默认 `Intermediate/DreamShader/GeneratedShaders`），*Clean Generated Shaders*
也仍然删除那里的每个 `*.ush` —— 但已经没有编译往那里写，那里的文件都是 1.x 构建留下的。1.x 保存的材质里可能还有
include 它们的 Custom 节点；重建那个材质就会换成自带代码的节点。

<a id="说明"></a>

## 说明

- **解析单元是文件。** （自 2.0.0 起） `import` 不再内联任何东西：被 import 的 `.dsh` 被单独预处理、解析，它的声明
  被声明进 import 它的文件。同一个名字声明两次是 `DSH4210`，import 成环是 `DSH4211`。头文件里不能有资产块（`DSH2249`），
  所以"一个文件一个 `Shader`"（`DSH2250`）说的就是这个文件本身。构建键仍然覆盖每一个被 import 的字节。见
  [import 与命名空间](../language/import.md)。
- 每条诊断都带着它所指构造的行列，落在那个构造所在的文件里：被 import 的头文件里报出的诊断写的是头文件。只有少数文件级的
  诊断（比如解析不到的 import）报在第 1 行。
- 产物按依赖顺序发射：同一个文件里被其他产物调用的函数先构建，所以 `Shader` 可以调用写在它旁边的 `ShaderFunction`。
  其他情况按声明顺序。
- 生成是 editor-only 的。要用 2.0 语言写同一个材质，见 [DreamShaderLang 2.0](../language-v2/index.md) 和
  [从 1.x 迁移](../tools/migrate.md)。

<a id="完整示例"></a>

## 完整示例

```c
// DShader/Materials/M_Emissive.dsm
import "Common.dsh";

ShaderFunction(Name="Functions/F_Tint")
{
    Inputs  { vec3 InColor; vec3 InTint; }
    Outputs { vec3 OutColor; }
    Graph   { OutColor = InColor * InTint; }
}

Shader(Name="Materials/M_Emissive")
{
    Properties { vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Settings   { ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = F_Tint(vec3(1.0, 1.0, 1.0), Tint); }
}
```

编译这个文件一次会产出：

```text
/Game/Functions/F_Tint            UMaterialFunction, saved
/Game/Materials/M_Emissive        UDreamShaderMaterialInstance + hidden UMaterial base, Ephemeral in the editor
```

并报告：

```text
Generated MaterialFunction /Game/Functions/F_Tint.F_Tint from I:/.../M_Emissive.dsm.
Generated Material /Game/Materials/M_Emissive.M_Emissive from I:/.../M_Emissive.dsm.
```

<a id="继续阅读"></a>

## 继续阅读

- [资产路径](asset-paths.md)
  - [内存材质](ephemeral.md)
  - [输出绑定](outputs.md)
  - [重新生成](regeneration.md)
  - [Backend](../settings/backend.md)
  - [命令行](../tools/commandlet.md)
