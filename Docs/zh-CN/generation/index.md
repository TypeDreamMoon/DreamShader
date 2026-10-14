---
title: "生成流程"
description: "生成流程"
---

<a id="generation"></a>

# 生成流程

> [DreamShader](../index.md) » **生成流程**

把一个 DreamShaderLang 源文件变成 Unreal 资产的阶段。

| | |
| :-- | :-- |
| 输入 | 一个 `.dsm` 或 `.dsf` 文件，加上它 import 的每一个 `.dsh` 头文件 —— 同一条流程也编译 `.dss`、`.dsi` 和 `.dsp` |
| 拒绝的输入 | `.dsh` —— 头文件只作为 import 它的那个文件的一部分被编译（[`DSH8296`](../diagnostics/DSH8xxx.md#dsh8296)） |
| 产物 | `UMaterial` / `UDreamShaderMaterialInstance`、`UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend`。不再生成 `.ush` include *（自 2.0.0 起）* —— 见 [生成的 HLSL](generated-hlsl.md) |
| 运行环境 | 仅编辑器进程（发射器使用 editor-only 的材质 API） |

<a id="pipeline"></a>

## 流程

*（自 2.0.0 起）* 一个 `.dsm` / `.dsf` 由 **legacy 前端** 读进 `.dss` 解析器
构建的同一棵树，之后走每种源文件共用的那一条流程
（`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp`）。一个阶段会报出它找到的**全部**
错误；报出过错误的阶段会在下一阶段开始前停下整次运行。每条诊断都有
稳定的码，格式是 `<file>(<line>,<col>): DSHnnnn: <message>` —— 见
[诊断索引](../diagnostics/index.md)和[码范围](../language-v2/index.md#diagnostics)。
下面的码是各阶段针对 1.x 源文件会抛出的码。

| # | 阶段 | 做什么 | 失败时 |
| :-- | :-- | :-- | :-- |
| 1 | 规范化路径 | 完整路径、规范化 | — |
| 2 | 文件类型闸门 | 构建 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`；`.dsh` 从不单独构建 | `DSH8296` |
| 3 | 读取 | 文件的文本 | [`DSH8290`](../diagnostics/DSH8xxx.md#dsh8290) |
| 4 | 预处理 | 用工程的 define 表求值 `#if` / `#define`，`.dsm` / `.dsf` 按 1.x 方言；指令行变成空行，所以位置不动 | [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291)，内含预处理器自己的 `DSH1030`–`DSH1042` |
| 5 | 解析 | legacy 前端读 1.x 的块、section 和 `Graph` 语句 | `DSH2200`–`DSH2258`、`DSH3250`–`DSH3278`、`DSH5250`–`DSH5265`、`DSH6300`–`DSH6319`，以及词法器的 `DSH2101`–`DSH2106`；随后是一个纹理默认值，其类壳写错了类，[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043) / [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044) |
| 6 | 绑定 | 名字、类型、调用、设置；每个被 import 的 `.dsh` 被**单独**解析、预处理和解析，它的声明被声明进当前文件 | `DSH4200`–`DSH4264`、`DSH5200`–`DSH5231`、legacy 规则 `DSH5275`–`DSH5292`、`DSH6200`–`DSH6211`、`DSH7200`–`DSH7233`；import 解析不到、读不了或解析失败是 [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292)–`DSH8295` |
| 7 | 构建键 | 每个被 import 的头文件和本文件的哈希，加上决定输出的设置与版本 | — 见[缓存](caching.md) |
| 8 | 降级 | 把绑定后的树降成图 IR，Custom 节点代码也在内 | `DSH4350`–`DSH4383`、`DSH6220`–`DSH6223`、`DSH6250`–`DSH6264`、`DSH6327`、`DSH6328` |
| 9 | Pass 与校验 | 常量折叠、去重、剪掉死节点；IR 校验器 | `DSH4390`；`DSH4300`–`DSH4335` |
| 10 | 发射 | 每个产物一个资产，按依赖顺序 —— 见[下面](#inside-the-emit) | `DSH8200`–`DSH8237`；两个互相调用的导出函数是 [`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299) |
| 11 | 组装结果 | 每个产物一行，外加一个 `Warnings:` 块 —— 见[成功消息](#success-messages) | — |

`.dsi` 在解析之后解析它的父对象（`DSH8260`–`DSH8265`），而 `.dsp` 解析它的 pass 引用的材质和 shader
文件；见 [`.dsi`](../language-v2/instances.md)和 [`.dsp`](../language-v2/passes.md)。

失败的编译把第一个错误放在第一行，然后是本次运行的其余诊断：
错误，再警告，再 info。结果的码就是第一个错误的码。

*compile material* 入口跑的是和 *compile assets* 同一次编译 *（自 2.0.0 起）*：一个源文件就是
一个编译单元，它的材质可以调用同一文件里声明的函数，那个函数会先被发射。

<a id="inside-the-emit"></a>

### 发射内部

一个产物 —— 一个 `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 块 —— 按这个顺序走。
资产层的诊断经由发射器的诊断到达结果：发射器的消息引用
资产层的码和文本。

| # | 子阶段 | 失败时 |
| :-- | :-- | :-- |
| 1 | 从 `Name=` / `Root=` 解析目标 | [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200) —— 见[资产路径](asset-paths.md#diagnostics) |
| 2 | 创建或复用资产：类检查与[归属守卫](regeneration.md#ownership-guard) | `DSH8201`（`Graph` 材质）、`DSH8203`（ThinCustom 实例）、`DSH8202`（函数、layer、blend） |
| 3 | 另一个编辑器持有本工程生成资产的写入权 | [`DSH8209`](../diagnostics/DSH8xxx.md#dsh8209)，info：跳过，不算失败 |
| 4 | source hash 短路 | [`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237)，info：跳过 —— 见[缓存](caching.md) |
| 5 | 在资产编辑器中打开 | [`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206) —— 见[重新生成](regeneration.md#open-in-an-asset-editor) |
| 6 | 手改检测 | [`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207) —— 见[手改检测](divergence.md) |
| 7 | ThinCustom：先抓下实例上的参数覆盖，再找到或创建隐藏 base | `DSH8230` |
| 8 | 构建图、应用设置、布局、重新编译 —— 原子地 | 见下面和[重新生成](regeneration.md) |
| 9 | 打上来源元数据和输出指纹 | — |
| 10 | 保存，除非产物是 Ephemeral 的 ThinCustom 实例或写入被拒绝 | [`DSH8229`](../diagnostics/DSH8xxx.md#dsh8229) |

1.x 生成器在这里做的检查 —— 存在顶层 `Shader`、`Outputs` 非空、
`Backend` 认得 —— 现在归前端：没有 `Outputs` 的 `Shader` 是**警告**
[`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256)，照样构建；既无 `Graph`、`Outputs`
也什么都没算的 `Shader` 是 [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)；
不是 backend 的 `Backend` 是 [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202)（空值是
[`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201)；`Instance` 按 `ThinCustom` 读，并给出
[`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204)）。

<a id="inside-the-graph-build"></a>

### 图构建内部

两种 backend 共用。在 ThinCustom backend 下，它作用于隐藏的 base 材质，
而不是发出的那个实例。

1. 清除生成的 `DreamShader: ` 注释框，然后分离现有图 —— 保留它，以便
   失败时能恢复它（无法生成快照时为 [`DSH8208`](../diagnostics/DSH8xxx.md#dsh8208)）。
2. 将每个材质属性重置为其引擎默认值。
3. 应用 `Settings`（被拒绝时为 [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)）。
4. 逐节点发出 IR 图 —— 参数、表达式、每个 `Function` 调用一个 Custom 节点、每个 `ShaderFunction` 调用一个
   `MaterialFunctionCall` —— 并通过一对 `DS_<output>` 命名重路由连接每个 `Outputs` 绑定。引擎无法构建的节点为 `DSH8210`–`DSH8228`。
5. 按项目的 [图布局样式](graph-layout.md#layout-styles) 布局该图。
6. 提交 —— 被分离的图此时才被销毁 —— 并重新编译材质。

<a id="what-triggers-a-compile"></a>

## 什么会触发编译

| 触发方式 | 强制 | ThinCustom 结果 | 范围 |
| :-- | :-- | :-- | :-- |
| 保存时自动编译（文件监视器 + 防抖） | 否 | 临时 | 被保存的文件；保存的 `.dsh` 会将所有导入它的源文件排入队列 |
| 启动扫描，以及修改了 [构建键](caching.md#it-is-a-build-key-not-just-a-source-hash) 所覆盖的设置之后的扫描 | 否 | 临时 | 每个项目源文件 |
| *Recompile DSM*、*Clean Generated Shaders* | 是 | 临时 | 每个项目源文件 |
| 材质内容浏览器 *Compile* / 缩略图刷新按钮 | 是 | 临时 | 单个源文件 |
| 实时预览渲染器 | 通过 bridge 时是；通过预览 WebSocket 时，仅当请求设置了 `force` | 临时 | 单个源文件 |
| *Materialize*，以及为临时材质创建子实例 | 是 | **已实体化** | 单个源文件 |
| 一个 `.dsi`，其父材质缺失或比其源文件更旧（[`DSH8264`](../diagnostics/DSH8xxx.md#dsh8264)） | 否 | **已实体化** | 先处理父材质的源文件 |
| Commandlet `-run=DreamShader` | 带 `-Force` 时 | **已实体化** | 按指定 |
| Cook，仅在 cook director 进程上 | 是 | **已实体化** | 每个项目源文件 |

自动编译由两个项目设置控制：**Auto Compile On Save**（默认开启）和 **Save
Debounce Seconds**（默认 `0.25`，钳制到 `[0.05, 10.0]`）。见
[项目设置](../settings/project.md)。

> [!NOTE]
> 只有 **ThinCustom** 产品能存在于内存中。交互式编辑器将其保留在内存中，为临时状态，
> 直到 cook、commandlet 或显式 *Materialize* 将其写入磁盘。一个 `Graph` 后端的材质
> 以及每个材质函数、layer 和 blend 都会被每次允许写入的构建保存
> *（自 2.0.0 起）*。见 [临时材质](ephemeral.md)。

<a id="outcomes-that-are-not-assets"></a>

## 不产出资产的结果

一个源文件可以编译成功，却不产出任何可放入内容浏览器的东西。

| 源文件 | 结果 |
| :-- | :-- |
| 一个 `.dsm` / `.dsf`，仅声明了 `Function`、`GraphFunction`、`Namespace` 或 `VirtualFunction` 块 | 成功：`Compiled {File}; it declares no material and no exported function, so no asset was written.` |
| 一个 `.dsm` / `.dsf`，完全没有块 | **失败**，[`DSH2254`](../diagnostics/DSH2xxx.md#dsh2254) |

<a id="success-messages"></a>

## 成功消息

每个产品一行，按产品发出的顺序排列。这些行是一种线格式：
`.skill/dsc.ps1` 读取 `Generated <Kind> <ObjectPath> from <Source>.` 来列出一次运行写入了什么。

| 消息 | 针对 |
| :-- | :-- |
| `Generated {Kind} {ObjectPath} from {File}.` | 每个构建的产品。`{Kind}` 是 `Material`（任一后端）、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline`；`{ObjectPath}` 是对象路径，`/Game/M.M` |
| `Generated RenderTarget {ObjectPath} from {File}.` | 一个 `.dsp` 的每个导出 buffer 的渲染目标 *（自 2.1.0 起）* |
| `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` | 被哈希短路跳过的产品，任意种类 —— 见 [缓存](caching.md) |
| `Skipped {ObjectPath}; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk.` | 由另一个编辑器拥有写入权的产品（`DSH8209`） |
| `Skipped {ObjectPath} from {File}; the source marks it NoGeneration, and only a compile that names the source builds it.` | *（2.1.2 之后）* 一个 [NoGeneration](no-generation.md) 产品，未被未指定该文件的编译所触碰（`DSH8277`） |
| `Skipped {ObjectPath} from {File}; the source marks it NoGeneration, and it has never been built.` | *（2.1.2 之后）* 同上，但磁盘上没有资产（`DSH8278`） |
| `Compiled {File}; it declares no material and no exported function, so no asset was written.` | 没有产品的源文件 |

`{File}` 是完整、规范化的源文件路径。在这些行之下，一个 `Warnings:` 标题以线格式列出
本次运行的每条警告，然后是生成警告 `DSH8155`、`DSH9011` 和 `DSH9012`。

> [!NOTE]
> *（自 2.0.0 起）* 1.x 的行 `Generated {AssetPath} from {File}.`、`Generated DreamShader thin-custom
> material …`, the ` (virtual)` 后缀和 helper include 行已消失：每个产品都以其种类说
> `Generated`，被哈希跳过的函数说 `Skipped` 而不是什么都不说。

本页上的运行时替换显示为 `{Placeholder}`；编译器发出
替换后的文本。

<a id="progress-reporting"></a>

## 进度报告

生成通过 Unreal 的慢任务系统报告。对话框有延迟，因此快速编译绝不会
闪现一个，它带有一个 **Cancel** 按钮，并且在 `IsRunningCommandlet()` 下不显示。为材质内容浏览器解析源文件的产品时，
前半部分运行且完全无对话框。

| 范围 | 标题 | 帧 | 对话框延迟 |
| :-- | :-- | :-- | :-- |
| 一次编译 | `Compiling DreamShader source '{File}'...` | 6：`Reading '{File}'...`、`Parsing '{File}'...`、`Resolving names in '{File}'...`、`Lowering '{File}' to IR...`、`Validating the IR of '{File}'...`、`Building the graph for '{File}'...` | 0.35 s |
| `Classic` 布局（嵌套） | `Laying out DreamShader material graph...` | 每个节点一个 | 继承 |

此处 `{File}` 是不带文件夹的文件名。

<a id="where-the-time-goes"></a>

### 时间都花在哪里

对于 `.dsm` / `.dsf`，前五帧是前端，且很快。最后一帧
`Building the graph for '{File}'...` 包含发射 —— 而发射以 shader 编译结束
（`RecompileMaterial`、ThinCustom 实例的 `UpdateStaticPermutation`、函数的
`UpdateMaterialFunction`），该过程耗时无上限。*（自 2.0.0 起）* 1.9 的 `Step 1 of 2` / `Step 2 of 2`
前缀随 1.x 生成器一起消失。

> [!NOTE]
> 进度条停在 `Building the graph for …` 上是正常的。Shader 编译发生在
> 插件之外，单个重型排列可以使其停留数分钟。

<a id="cancelling-since-190"></a>

### 取消 *（自 1.9.0 起）*

在每个阶段之前、每个产品之前，以及在旧图被分离后的发射内部，都会检查取消。取消走的是与失败编译相同的路径：

- 资产被恢复为编译开始前的精确状态 —— 原子重建
  回滚正是做这件事的，因此绝不会出现半构建的图或被清空的材质函数。
- 不盖上标记，也不保存任何内容。
- 编译将 [`DSH8298`](../diagnostics/DSH8xxx.md#dsh8298) 作为错误报告 *（自 2.0.0 起；1.x 期间为
  `DSH9010`）*。

取消唯一不会恢复的是材质或函数上生成的 `DreamShader: ` 注释框，
原因与失败重建不恢复它们相同：它们在拍摄快照之前就被销毁了。
下一次成功编译会重新创建它们，而你自己写的注释绝不会被触碰。

> [!WARNING]
> 取消不会停止 Unreal 已经排队了的 shader。如果你在 shader
> 编译期间取消，`ShaderCompileWorker.exe` 会继续处理已派发的任何内容。

<a id="when-a-shader-compile-takes-too-long-since-190"></a>

### 当 shader 编译花费太长时间 *（自 1.9.0 起）*

如果某个资产的 shader 编译超过 **30 秒**，生成过程会发出一次 `DSH9011`，作为日志中的警告，并出现在结果的 `Warnings:` 块中。它从不阻塞或使编译失败 —— 它的作用是说明这种等待通常意味着什么：

- 一个 `Custom` 节点，其循环上界是它的某个**输入** —— 一个 `for` 或 `while`，其界限不是字面量，也不是被 `#define` 的常量。
- 结合**隐式 mip** 纹理采样 —— `Texture2DSample`、`Texture3DSample` 或
  `.Sample` —— 出现在发散控制流中。

编译器从屏幕空间导数推导 mip 级别，而这些导数在发散流中未定义，因此它会完全展开一个迭代次数未知的循环。要确认
编译仍在工作而不是卡住，请检查任务管理器中的 `ShaderCompileWorker.exe` 是否繁忙。要修复它，用字面量或 `#define` 给循环定界，或者调用 `SampleLevel` /
`SampleGrad`，它们将 mip 级别作为参数。

生成过程在发射 `Custom` 节点的代码时也会寻找这一组合，并在编译甚至尚未开始之前就提前警告，码为
`DSH9012`。两者都只是提示；都不会使构建失败。

<a id="pages"></a>

## 页面

| 页面 | 涵盖内容 |
| :-- | :-- |
| [资产路径](asset-paths.md) | `Name=` + `Root=` → 包路径 → 磁盘上的 `.uasset` |
| [内存材质](ephemeral.md) | ThinCustom 结果、隐藏 base、可见性、物化、cook |
| [缓存](caching.md) | 构建键、元数据键、何时跳过重新生成 |
| [图布局](graph-layout.md) | 生成的节点如何定位 |
| [重新生成](regeneration.md) | 重建会销毁什么、什么会保留 |
| [发散](divergence.md) | 输出摘要，以及资产被手工编辑时会发生什么 |
| [生成的 HLSL](generated-hlsl.md) | 一次 `Function` 调用放入其 Custom 节点的 HLSL |
| [源代码管理](source-control.md) | 哪些生成的资产有文件、对它们进行版本控制的两种方案、`dsc list-generated` |

<a id="notes"></a>

## 说明

- **解析单元是文件。** *（自 2.0.0 起）* `import` 不内联任何东西：被导入的 `.dsh` 会
  被单独预处理和解析，它的声明会被声明进导入它的文件。同一个名字声明两次是
  [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)，导入成环是
  [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211)。头文件不能包含资产块
  （[`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)），所以“一个文件一个 `Shader`”
  （[`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)）说的就是这个文件。构建键仍然覆盖每一个
  被导入的字节。见 [import](../language/import.md)。
- 每条诊断都带有它所指构造的行和列，位于该构造所在的文件中：在导入的头文件内部引发的诊断会指明该头文件。只有少数文件级诊断，
  比如无法解析的导入，会报第 1 行。
- 产物按依赖顺序发射：同一文件的另一个产物所调用的函数会先构建，所以一个 `Shader` 可以调用写在它旁边的 `ShaderFunction`。
  其他情况按声明顺序。
- 警告永远不会使编译失败。它们跟在结果行下面，位于一个 `Warnings:` 标题下。
- 生成是 editor-only 的。不存在从 DreamShaderLang 构建材质的运行时代码路径。
- 要用 2.0 语言编写同一个材质，见 [DreamShaderLang 2.0](../language-v2/index.md) 和
  [`dsc migrate`](../tools/migrate.md)。

<a id="example"></a>

## 示例

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

对该文件的一次编译会产出：

```text
/Game/Functions/F_Tint            UMaterialFunction, saved
/Game/Materials/M_Emissive        UDreamShaderMaterialInstance + hidden UMaterial base, Ephemeral in the editor
```

结果消息：

```text
Generated MaterialFunction /Game/Functions/F_Tint.F_Tint from I:/.../M_Emissive.dsm.
Generated Material /Game/Materials/M_Emissive.M_Emissive from I:/.../M_Emissive.dsm.
```

<a id="see-also"></a>

## 另见

- [Shader](../language/shader.md) —— 声明材质的块
- [ShaderFunction](../language/shader-function.md) —— 声明材质函数的块
- [源文件](../language/source-files.md) —— `.dsm`、`.dsf` 和 `.dsh` 可以包含什么
- [import](../language/import.md) —— 头文件如何被读入导入它的文件
- [Backend](../settings/backend.md) —— `Graph` 与 `ThinCustom`，以及已弃用的 `Instance` 别名
- [项目设置](../settings/project.md) —— 自动编译、防抖、默认 backend、路径
- [Commandlet](../tools/commandlet.md) —— `-run=DreamShader`，无界面生成入口点
- [Material Content Browser](../../tools/material-browser.md) —— Compile / Materialize 界面
- [DreamShaderLang 2.0](../language-v2/index.md) —— 该流水线的各阶段和代码范围
- [诊断索引](../diagnostics/index.md) —— 每一个码
