---
title: "缓存"
description: "缓存"
---

<a id="caching"></a>

# 缓存

> [DreamShader](../index.md) » [生成](index.md) » **缓存**

源哈希短路：存储编译后文本的指纹，让未更改的源完全跳过重新生成。

| | |
| :-- | :-- |
| 类型 | 生成优化 |
| 哈希文本 | 该文件以及它导入的每个 `.dsh` 的**预处理后**文本，外加编译它的上下文 |
| 算法 | `FCrc::StrCrc32`，格式为 `%08x` —— 八个小写十六进制数字 |
| 存储位置 | 生成资产的包元数据中，以资产对象为键 |
| 可绕过方式 | 生成入口点上的 `bForce` 标志 |

<a id="synopsis"></a>

## 概要

```text
digest text    := for each imported header, in the order the include resolver first read it:
                    "// Begin DreamShader source: <project-relative path>\n" <preprocessed header>
                    "\n// End DreamShader source: <project-relative path>\n\n"
                  then the same block for the file itself
                  (a `.dsi` adds its resolved parent and inherited vector channels,
                   a `.dsp` what it references)
build key      := "DSK7|Plugin=<version>|Engine=<major>.<minor>|" <settings> "Defines=<read defines>|"
                  "\n--\n" <digest text>
build key      ->  CRC32  ->  "%08x"  ->  DreamShader.SourceHash   e.g. "9f2c41ab"
source path    ->  project-relative, forward slashes  ->  DreamShader.SourceFile
```

<a id="what-is-hashed"></a>

## 哈希的内容

哈希覆盖编译实际读取的文本，是**预处理之后**的文本——不是磁盘上文件的字节。*(自 2.0.0 起)* 不再有任何内容被内联到解析中：每个导入的 `.dsh` 都会单独进行预处理和解析，构建键会在各自的 `// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` 块中对每个头文件的文本进行哈希，然后对该文件的文本进行哈希，包括其 `import` 行。两个导入都到达的同一个头文件只哈希一次。

| 更改 | 改变以下内容的哈希 |
| :-- | :-- |
| 编辑 `M_Foo.dsm` | `M_Foo.dsm` |
| 编辑被 `M_Foo.dsm` 和 `M_Bar.dsm` 导入的 `Common.dsh` | `M_Foo.dsm` 和 `M_Bar.dsm` 两者 |
| 将项目移动到另一个目录，或在别处检出 | 无——这些块通过文件相对于项目目录的路径来命名每个文件，就像 `DreamShader.SourceFile` 那样 *(2.0.0 – 2.1.0 通过其绝对路径命名，因此每个源都会重建)*。项目目录之外的源仍通过其绝对路径命名 |
| 重命名源文件 | 存储的路径不再匹配，因此不会跳过任何内容 |
| 重新格式化空白或编辑注释 | 哈希——文本按字节逐一比较，而非按语义比较 |
| 更改 **Default Compiler Backend** | **每个**源 *(自 1.8.0 起)* |
| 更改着色模型 / 混合模式 / 域**映射** | **每个**源 *(自 1.8.0 起)* |
| 升级插件或引擎 | **每个**源 *(自 1.8.0 起)* |
| 更改[预处理器定义](../language/preprocessor.md) | 每个**读取**了该定义的源 *(自 1.9.0 起)*——包括在它未定义时读取过它的源 |

<a id="it-is-a-build-key-not-just-a-source-hash"></a>

### 它是构建键，而不仅仅是源哈希

该戳记在**编译源的上下文中**对源进行指纹识别 *(自 1.8.0 起)*。任何会改变给定源产物的因素都会被纳入其中，因为遗漏它会让跳过检查对并非最新的资产回答“仍是最新的”：

| 输入 | 为什么它在键中 |
| :-- | :-- |
| 文件及其导入的每个头文件的预处理后文本 | 编译的实际输入——这就是为什么更改的 `.dsh` 在此不需要其他任何内容 |
| [Default Compiler Backend](../settings/project.md) | 决定一个 `Shader` 块是变成 `UMaterial` 还是薄实例 |
| 映射表 | 决定一个 `Settings` 键解析为什么 |
| 插件版本，外加一个手动递增的格式标签——`DSK7` *(自 2.1.2 起；2.1.1 中为 `DSK4`；1.9.0 – 2.1.0 中为 `DSK3`)* | 升级生成器会使旧生成器写入的内容失效。当前键会为修正后的链式写入、类型转换、数学、嵌套返回、参数默认值和条件表达式写入重建图表 |
| 引擎版本 | 可生成的内容随之变化（例如 Substrate） |
| [此源读取的预处理器定义](../language/preprocessor.md#rebuilds) *(自 1.9.0 起)* | 它们决定源的哪些分支被编译 |

定义的合并是**精确的**：只包括文件实际读取的名称，每个都带有其当时的取值，并为在未定义任何内容时被读取的定义设置显式的 `<undef>` 哨兵值。改为合并整个表会导致任何项目一碰任何开关就重建整个世界；省略哨兵值则比两者都更糟，因为这样*添加*一个定义就不会改变哈希，源会悄悄开始走另一条分支，而它的资产永远不会重建。

该合并的两个性质至关重要。它按名称**排序**，区分大小写，因为 `TMap` 的迭代顺序不稳定，未排序的合并每次运行都会产生不同的哈希——这会被解读为“每次每个资产都已过期”。而且它是**单射的**：朴素的 `Name=Value;` 拼接不是，因为值可能合法地包含 `=` 和 `;`（`-Define=A=1;B=2` 是有效的定义），所以 `{A: "1;B=2"}` 和 `{A: "1", B: "2"}` 会合并为同一个字符串，其中一个会悄悄复用另一个的缓存资产。

> [!NOTE]
> 在此之前，键仅对源文本进行哈希，因此更改后端会让每个已生成的资产看起来都是最新的——这就是为什么那一个设置要额外附加自己的强制重建扫描。该扫描不再强制：每个受影响的资产现在会自行通不过跳过检查，而且——同样重要的是——该设置*不*影响的资产仍会被跳过，而不会被无谓地重建。
>
> 更改键的组成会使每个现有戳记失效。这为每个资产一次性付出一次重建的代价，正是预期的效果。

> [!NOTE]
> 编辑 `.dsh` 会使导入它的每个源失效，但头文件自身从不生成任何内容：直接交给编译器时它会是 [`DSH8296`](../diagnostics/DSH8xxx.md#dsh8296)。
> 开启 **Auto Compile On Save** 后，保存头文件会将直接或通过另一个头文件导入它的每个源排入队列，这些编译会发现它们的键已改变。否则，依赖项会在它们自身被编译时重建：在它们自己保存时，由启动扫描或更改构建键覆盖的设置后的扫描，材质内容浏览器的编译按钮，[命令行工具](../tools/commandlet.md)，或烘焙。

当一个批次同时包含一个文件及其依赖项——它导入的头文件、`.dsi` 的父级、`.dsp` 命名的材质——该批次会**按依赖项先编译的顺序排列** *(自 1.8.0 起)*。这不是小事：一个调用 `ShaderFunction` 的材质会将其调用节点绑定到实时的 `UMaterialFunction` 资产上——发射器从对象读取引脚，而不是从源读取——因此先编译调用方会将其绑定到该函数接口的*上一个*版本。在单个文件内，发射器保持相同顺序，在调用方之前构建文件另一个产物所调用的函数；互相调用的两个函数是 [`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299)。处于导入循环中的文件按遍历到达它们的顺序保留；编译本身会以 [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211) 拒绝该循环。

<a id="custom-pass-pipelines"></a>

## 自定义 Pass 管线

*(自 2.1.0 起)* 一个 [`.dsp`](../language-v2/passes.md) 会像任何 2.0 源一样生成键——它的预处理后文本、它读取的定义，以及上述每个设置和版本——外加其编译在文件之外读取的内容，因为文本未更改的管线在其中一个内容变动时仍需重建：

| 归入 `.dsp` 的键中 | 何时移动 |
| :-- | :-- |
| 项目的通道层名称，按位顺序 | 在 *Project Settings ▸ DreamShader Custom Pass* 中添加、移除、重命名或移动层时：`Layer(...)` 过滤器被编译为位 |
| 引擎是否具有 Custom Pass 运行时 | 项目在 5.8 另一侧的引擎上打开 |
| 每个 `Material = "..."`：写入的引用、它解析到的对象路径、该材质上次构建时所用的构建键（其 `DreamShader.SourceHash`），以及检查各通道所依据的事实——其域与可混合位置、`bDisablePreExposureScale`、它读取的 `UE.UserSceneTexture` 名称、其 `SceneTexture` 节点接收的后处理输入、其 `UE.DreamPassOutput` 以及哪些引脚已连接、其使用标志、是否请求新翻译器 | 材质从更改的源重新构建、出现，或更改其中一项事实 |
| 每个 `Shader = "..."`：引用、其虚拟路径，以及快照输入的哈希——文件及其按相对路径包含的所有内容的文本和相对路径，以及每个按虚拟路径包含的内容背后的文本，该虚拟路径既不是 `/Engine/` 也不是 `/Plugin/` | 编辑了 `.usf` 或它包含的任何内容 |
| 每个被 `hlsl` 块包含的文件：其相对于 `.dsp` 文件夹的路径，以及其输入的相同哈希——写在 `.dsp` 中的 HLSL 已在键中，作为 `.dsp` 的文本 | 编辑了被包含的文件，或它包含的任何内容 |

这正是让 [bridge](../../tools/bridge.md#custom-pass-shader-files) 能在每次
`.usf` 保存时以及每次重建其命名的材质后，排队一个非强制的 `.dsp` 的原因：不会移动上述任何一项的更改会被跳过。
预检查插槽所用的着色器格式不在键中。

匹配的键不足以跳过管线：插槽注册表还必须仍然持有它的每个 HLSL 通道，位于资产记录的插槽中，并带有通过预检查的快照及其在磁盘上的文件，且每个导出的缓冲区都必须有其渲染目标。被删除的注册表，或丢失了插槽的合并，会在下次编译时重建，而不是被隐藏在未更改的键后面。

通过 [`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer) 读取导出缓冲区的 `.dss`
会按节点并入管线的对象路径、缓冲区、它是否被导出、其格式以及渲染目标的路径，因此更改导出的 `.dsp` 会重建材质。

<a id="where-the-metadata-lives"></a>

## 元数据所在的位置

两个键被写入生成资产的 **包元数据** 中，以资产对象为键。

| 键 | 值 |
| :-- | :-- |
| `DreamShader.SourceFile` | 源路径相对于项目目录的路径，使用正斜杠。项目外的源保留其绝对路径。 |
| `DreamShader.SourceHash` | 八位十六进制 CRC32。仅在非空时写入。 |

存储 *项目相对* 路径是故意的：在另一台机器上的检出，或移动后的项目
目录，仍会将其生成的资产识别为自己的——[所有权
守卫](regeneration.md#ownership-guard) 和 *Make Ephemeral* 会读取此键。哈希随它一起传递：
构建键按相同的项目相对路径命名每个文件，因此在另一位置的检出
会跳过已是最新的内容 *（2.0.0 – 2.1.0 在那里使用绝对路径，这样的检出会重建每个
资产一次）*。

哪些资产会被标记，以及何时：

| 资产 | 标记 |
| :-- | :-- |
| `UDreamShaderMaterialInstance`（ThinCustom） | **始终**——Ephemeral 和 Materialized 都一样 |
| 隐藏的 `MB_DreamThinBase_*` 基础 | 仅 Materialized |
| `UMaterial`（Graph 后端） | **始终** *（自 1.8.0 之后的版本起；在此之前，仅持久模式）* |
| `UMaterialFunction` / 层 / 层混合 | **始终** *（同上）* |
| `.dsi` 的 `UMaterialInstanceConstant` | **始终** |
| `UDreamPassPipeline` *(2.1.0)* | **始终**——哈希仅在一次可能写入磁盘的构建上。因拒绝写入（另一个编辑器拥有写入权，或 `dump-graph` 的守卫）而在内存中构建的管线仅标记路径，因此后续编译不会据此跳过 |
| 导出缓冲区的渲染目标 *(2.1.0)* | 仅路径，从不标记哈希——它会随 `.dsp` 的每次编辑而移动——以及 `DreamShader.PassPipeline`，即它所属管线的对象路径 |

> [!NOTE]
> 直到 1.8.0，仅内存的 Graph 材质或函数仅标记源 **路径**，
> 因此跳过永远不会对它触发。代价是没有任何东西能说明这样的资产
> 是否是最新的——材质内容浏览器会永远将每一个都报告为过期。现在每个生成的
> 资产在两种模式下都携带其哈希，而 *意为* "无论如何都重建" 的入口点
> 会强制越过跳过（见下文）。

ThinCustom 实例还会将源路径和哈希作为只读 `UPROPERTY` 携带——
`SourceFilePath` 和 `SourceHash`，类别 `DreamShader`——因此它们在细节面板中可见，
无需检查包元数据。`SourceFilePath` 保存 **完整规范化** 的源路径，而不是
元数据存储的项目相对形式；`SourceHash` 是相同的八位十六进制值。参见
[`UDreamShaderMaterialInstance`](../api/material-instance.md)。

> [!NOTE]
> `DreamShader.SourceFile` 兼作 **所有权标记**。它的存在告诉
> DreamShader 某个资产是它自己可以覆盖的，也是 *Make Ephemeral*
> 所过滤的依据。参见 [Regeneration](regeneration.md#ownership-guard)。

<a id="when-regeneration-is-skipped"></a>

## 何时跳过重新生成

仅当以下 **全部** 条件成立时短路才会触发：

| # | 条件 |
| :-- | :-- |
| 1 | 生成调用未设置 `bForce` |
| 2 | 资产存在且新计算的哈希非空 |
| 3 | 存储的 `DreamShader.SourceFile` 存在且非空 |
| 4 | 存储的源路径等于正在编译的源的项目相对路径，**忽略大小写** |
| 5 | 存储的 `DreamShader.SourceHash` 等于新哈希，**区分大小写** |

跳过被报告为信息 [`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237)，并且产品结果
行读取 `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` 在比较哈希之前，如果另一个编辑器拥有写入此项目生成资产的权限，则一次可能写入磁盘的构建会被跳过
（[`DSH8209`](../diagnostics/DSH8xxx.md#dsh8209)）。

失败的包保存会清除内存中的源哈希，因此再次编译同一源会在没有 `-Force` 的情况下重试生成和保存。源所有者和输出摘要保持不变：失败保存后所做的手动编辑仍会停止重新生成。失败的批处理会使每个参与的资产失效，包括可能已经到达磁盘的包。管线在跳过之前还会验证其导出渲染目标的持久化配置，因此部分保存的批处理可以在重启编辑器后修复。

按资产类型：

| 资产 | 跳过点 | 额外条件 | 结果行 |
| :-- | :-- | :-- | :-- |
| ThinCustom 材质 | 在实例被创建或复用之后，在隐藏基础被创建 **之前** | — | `Skipped …` |
| `Graph` 后端材质 | 在材质被创建或复用之后 | — | `Skipped …` |
| 材质函数 | 在函数资产被创建或复用之后 | 资产的材质函数使用必须已与块所要求的一致 | `Skipped …` *（自 2.0.0 起；直至 1.x 函数的跳过是静默的）* |
| `UDreamPassPipeline` *(2.1.0)* | 在管线被复用之后，在其 HLSL 插槽被规划之前 | 插槽注册表持有管线的每个 HLSL 通道，位于资产记录的插槽中，并带有通过预检查的快照及其在磁盘上的文件，且每个导出的缓冲区都有与其配置匹配的已保存、干净的渲染目标——参见 [Custom Pass pipelines](#custom-pass-pipelines) | `Skipped …` |

将 ThinCustom 检查放在创建基础材质之前，正是让跳过变得廉价的原因：没有基础材质、没有所有权检查、没有图拆除。

> [!NOTE]
> 使用方式不匹配的材质函数——例如一个 `ShaderLayer` 块，其资产仍被标记为 `Default`——即使哈希匹配也会重新生成，并且使用方式会被纠正。

<a id="forcing-regeneration"></a>

## 强制重新生成

| 路径 | 强制 |
| :-- | :-- |
| 保存时自动编译 | **否**——哈希短路处于活动状态 |
| 启动扫描（`GenerateAllSources`） | **否**——参见[临时材质](ephemeral.md#when-the-asset-already-exists-on-disk) |
| 后端、映射表或预处理器定义发生更改后的同一次扫描 | **否**——键覆盖了它们中的每一项，因此只会重建受它们影响的内容 |
| *工具 ▸ DreamShader ▸ 重新编译 DSM*、*清理生成的着色器* | 是——通过桥接排队为强制 |
| 桥接 `recompile` 请求（`scope: "file"` / `"all"`） | 是——显式请求意味着“重建” |
| 材质内容浏览器编译 / 缩略图刷新 | 是 |
| 实时预览渲染 | 通过桥接的 `previewMaterial` 时，是；通过预览 WebSocket 时，仅当请求设置 `force` |
| *物化*，以及子实例创建 | 是 |
| 烘焙 | 是 |
| Commandlet `-run=DreamShader` | 仅在有 [`-Force`](../tools/commandlet.md#compile--generate) 时；否则报告 `Skipped {ObjectPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` |
| 通过桥接的 `.usf` / `.ush` 保存 *(2.1.0)* | 否——从文件编译的 `.dsp` 会被排队，它们的键覆盖其文本 |
| 通过桥接的依赖项队列编译的 `.dsp` 或其指定的材质 *(2.1.0)* | 否 |
| `dsc pass-registry -Rebuild` *(2.1.0)* | 是，每个 `.dsp` |

无法从源语言清除存储的哈希。若要在没有支持强制的入口点的情况下强制重建，可以更改源文本（任何更改，包括空白字符），或者删除生成的资产。

<a id="notes"></a>

## 说明

- 哈希是 CRC32，不是密码学摘要。它检测编辑；它不是安全或完整性机制。
- 没有需要同步的生成 `.ush` 包含文件 *(自 2.0.0 起)*：`Function` 的 HLSL 被写入资产内每次调用的 Custom 节点中，而未更改的源会像其他所有内容一样跳过它。参见[生成的 HLSL](generated-hlsl.md)。
- 比较是按资产进行的。一个声明了一个材质和三个函数的源文件会在四个资产上存储相同的哈希，且每个都会被独立跳过。
- 在 UE 5.6 及更新版本上，包元数据通过引擎的值类型元数据 API 访问；更早的引擎使用对象类型 API。存储的键和值是相同的。
- 没有任何东西写入生成时间戳。除了这两个键之外，构建会在资产上标记 `DreamShader.OutputDigest` 和 `DreamShader.OutputDigestClasses`（[发散](divergence.md)），在带有图的资产上标记 `DreamShader.SourceSpans` 和 `DreamShader.DecompileHints`，并在管线的渲染目标上标记 `DreamShader.PassPipeline`。它们都不参与跳过。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH8237` | *（信息）* 短路跳过了一个产品；结果行显示 `Skipped …` |
| `DSH8209` | *（信息）* 另一个编辑器拥有写入本项目生成资产的权限，因此会写入磁盘的构建被跳过 |
| `DSH8296` | 一个 `.dsh` 被直接交给编译器 |

<a id="example"></a>

## 示例

```c
// DShader/Common.dsh
Function float Remap01(in float value) { return saturate(value * 0.5 + 0.5); }
```

```c
// DShader/M_Ramp.dsm
import "Common.dsh";

Shader(Name="Materials/M_Ramp")
{
    Properties { ScalarParameter Input = 0.25; }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { float R = Remap01(Input); Color = vec3(R, R, R); }
}
```

观察到的序列：

```text
save M_Ramp.dsm      Generated Material /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm.
save M_Ramp.dsm      Skipped /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm; source hash is unchanged (build key …).
save Common.dsh      (the header builds nothing; it queues M_Ramp.dsm, whose key moved)
                     Generated Material /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm.
```

生成实例上的元数据：

```text
DreamShader.SourceFile   DShader/M_Ramp.dsm
DreamShader.SourceHash   9f2c41ab
```

<a id="see-also"></a>

## 另见

- [生成](index.md)——在管线中计算哈希的位置
- [导入](../language/import.md)——如何读取导入的头文件
- [预处理器](../language/preprocessor.md)——键纳入的定义，以及为什么只纳入已读取的那些
- [重新生成](regeneration.md)——建立在 `DreamShader.SourceFile` 上的所有权保护
- [发散](divergence.md)——另一个指纹：资产所持有的内容，而不是源所声明的内容
- [临时材质](ephemeral.md)——哪些资产在哪种模式下被标记
- [`UDreamShaderMaterialInstance`](../api/material-instance.md)——`SourceFilePath` 和 `SourceHash`
- [生成的 HLSL](generated-hlsl.md)——Custom 节点代码，以及它所取代的 1.x 包含文件
- [Commandlet](../tools/commandlet.md)——无头编译和强制
- [HLSL 通道](../../runtime/hlsl.md)——其输入由 `.dsp` 的键覆盖的快照
- [项目设置](../settings/project.md)——自动编译和防抖
