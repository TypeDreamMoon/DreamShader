---
title: "DSH9xxx --- 工具、同步与内部不变量"
description: "DSH9xxx --- 工具、同步与内部不变量"
---

<a id="dsh9xxx-----tools-sync-and-internal-invariants"></a>

# DSH9xxx --- 工具、同步与内部不变量

> 生成标记之间的内容由 `.skill/gen-diagnostics.ps1` 写入。
> 标记下方的一切均由手工撰写，并在重新生成后保留。

<a id="dsh9001"></a>

## DSH9001

<!-- generated:begin DSH9001 -->
**严重性** 错误

**消息**

```
DSH9001: '{0}' uses conditional compilation, and VirtualFunction sync rewrites a source in place at byte offsets taken from the file as written -- it cannot tell which of your branches a definition belongs to, and refuses rather than risk writing one branch over the others. Refresh these definitions by moving them into a source without directives, or edit them by hand.
```

**引发者** `Source/DreamShaderEditor/Private/VirtualFunction/DreamShaderVirtualFunctionSyncService.cpp:458`
<!-- generated:end DSH9001 -->

**原因。** VirtualFunction 启动同步发现某个源文件使用了条件编译，因而拒绝改动它。同步会按照文件写入时的**字节偏移**就地重写声明；预处理器会保留行数但不会保留字节数（被截断的行会变成更短的空行），因此第一个截断点之后的每个偏移都是错的，而一旦让经预处理的字符串到达文件写入器，就会永久性地抹平所有死分支。与 Adopt 不同，没有人会为此发出请求——它在桥接启动时无人值守地对每个可写源文件运行——因此这是一次拒绝，而非一次询问

**修复。** 将 `VirtualFunction` 定义保存在没有指令的源文件中，或手动刷新它们：材质函数上的 **Copy VirtualFunction Definition** 会给出当前声明以供粘贴。该拒绝是按文件生效的，不会阻止其他文件的同步

**另见** [VirtualFunction 工具](../../tools/virtual-function-tools.md)、[预处理器](../language/preprocessor.md)

<a id="dsh9011"></a>

## DSH9011

<!-- generated:begin DSH9011 -->
**严重性** 警告

**消息**

```
Compiling shaders for '%s' took %.0f seconds. A stall of this length is almost always a Custom node whose loop bound is an input (a 'for' or 'while' whose limit is not a literal or a #define) combined with implicit-mip texture sampling -- Texture2DSample / Texture3DSample / .Sample inside divergent flow -- which forces the compiler to fully unroll an iteration count it cannot know. To confirm it is still working rather than hung, check whether ShaderCompileWorker.exe is busy in Task Manager. To fix it, bound the loop with a literal or a #define, or switch the samples to SampleLevel.
```

**引发者** `Source/DreamShaderCompiler/Private/DreamShaderCompilerService.cpp:262`
<!-- generated:end DSH9011 -->

**原因。** 某个资产生成中涉及着色器编译的那一半——`UpdateStaticPermutation`、`PostEditChange`、材质重新编译以及保存——运行了超过三十秒。三十秒并非性能预算；许多合法材质在冷着色器缓存上都会超过它。它只是一个临界点，超过之后静止的进度条就不再读作「正在工作」，而开始读作「已挂起」，此警告的存在是为了把这种差异讲清楚，而不是让人去猜

**修复。** 首先判断是否真有问题。如果在任务管理器中 `ShaderCompileWorker.exe` 正忙，说明编译正在进行，唯一的问题是需要多久；冷 DDC、Substrate 材质或大量排列组合都可能合法地花费数分钟。如果它空闲且毫无进展，那是另一个问题。当等待确实存在而你想缩短它时，要寻找的形态正是 [DSH9012](#dsh9012) 所描述的那种：一个 `Custom` 节点，其循环边界是一个输入引脚，并且紧邻一个隐式 mip 纹理采样。用字面量或 `#define` 的常量来限定循环，或调用 `SampleLevel` / `SampleGrad`，它们把 mip 级别作为参数，不会强制展开。仅供参考；每个资产仅发出一次；绝不会导致构建失败

**另见** [生成](../generation/index.md#progress-reporting)

<a id="dsh9012"></a>

## DSH9012

<!-- generated:begin DSH9012 -->
**严重性** 警告

**消息**

```
'%s' loops on the input '%s' and samples with '%s', which takes its mip level from screen-space derivatives. The shader compiler cannot know how many iterations to expect, so it fully unrolls the loop to keep the derivatives defined, and compilation can take minutes. Bound the loop with a literal or a #define, or call SampleLevel / SampleGrad instead.
```

**引发者** `Source/DreamShaderCompiler/Private/DreamShaderCompilerService.cpp:290`
<!-- generated:end DSH9012 -->

**原因。** 进入 `Custom` 节点的 HLSL 包含两样东西，单独看无害，合在一起代价高昂：一个是 `for` 或 `while`，其边界是该节点的某个**输入引脚**（着色器编译器无法将其解析为常量的标识符，不同于字面量或 `#define` 的名称），另一个是 mip 级别为**隐式**的采样调用——`Texture2DSample`、`Texture3DSample`、其他 `Texture*Sample` 辅助函数，或成员 `.Sample()`。隐式 mip 采样从屏幕空间导数推导级别，而这些导数在发散控制流内部是未定义的，因此编译器为了保持其有定义，会完全展开外层循环——却又没有可展开的迭代次数。正是这一点把一段光线步进主体变成数分钟的编译

**修复。** 拆散这一对。要么让边界变为静态——使用字面量或 `#define` 的常量，把该输入用于提前 `break` 退出，而不是用于限定循环——要么用 `SampleLevel`、`SampleGrad` 或 `SampleBias` 让采样变为显式，它们把级别（或梯度）作为参数。在光线步进中，第二种几乎总是你真正想要的：循环内部的导数是毫无意义的，而 `SampleLevel(..., 0)` 也正说明了这一点。此检查是一种启发式：采样调用在主体中的任何位置都会被查找，不限于循环内部，且引脚名匹配不区分大小写。它只是警告，绝不会导致构建失败；如果你已实测过着色器且它编译正常，请忽略它

**另见** [生成](../generation/index.md#progress-reporting)

<a id="dsh9020"></a>

## DSH9020

<!-- generated:begin DSH9020 -->
**严重性** 错误

**消息**

```
Could not back up '%s' to '%s'; its asset references were left pointing at the old path.
```

**引发者** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1147`
<!-- generated:end DSH9020 -->

**原因。** 某个源文件所引用的资产被重命名或移动，因此该文件必须被重写——但复制到 `<file>.bak` 失败，所以没有可回退的内容。通常是因为该源文件（或其旁边的 `.bak`）是只读的、被版本控制签出、被另一进程占用，或位于已满或已断开的驱动器上

**修复。** 什么都没写入：源文件仍然指向旧路径，并将在下次编译时无法加载该资产。清除任何阻碍复制的东西——签出该文件、去掉只读标志、关闭占用 `<file>.bak` 的程序——然后来回重命名该资产，或手动编辑路径。在项目设置中关闭 *Sync Source References On Asset Rename* 可完全阻止插件尝试此操作

**另见** [资产重命名同步](../../tools/asset-rename-sync.md#backups)

<a id="dsh9021"></a>

## DSH9021

<!-- generated:begin DSH9021 -->
**严重性** 错误

**消息**

```
Could not write '%s' after renaming an asset it references; the file as it was is in '%s'.
```

**引发者** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1163`
<!-- generated:end DSH9021 -->

**原因。** 备份已取好，但重写后的源文件无法覆盖保存到原始文件之上。两者之间的窗口很小，因此这几乎总是因为文件在此期间变得不可写——版本控制锁，或某个编辑器以独占方式将其打开

**修复。** 文件的原样保存在 `<file>.bak` 中；磁盘上的源文件未被改动，因为失败的步骤正是写入本身。恢复或忽略 `.bak`，使文件可写，然后手动编辑路径或来回重命名该资产以再次触发同步

**另见** [资产重命名同步](../../tools/asset-rename-sync.md#backups)

<a id="dsh9022"></a>

## DSH9022

<!-- generated:begin DSH9022 -->
**严重性** 错误

**消息**

```
The IR dump could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:570`
<!-- generated:end DSH9022 -->

**原因。** `dump-ir` 无法创建其输出目录或写入转储文件。消息说明了
是哪一种。默认目录为 `<Project>/Saved/DreamShader/IR`；`-Out=` 可覆盖它。

**修复。** 检查该目录是否可写，以及是否有任何东西占用着该文件。转储路径是
`<out>/<root>/<path relative to that root>.ir.txt`，因此若某个根的显示名称包含路径
分隔符，就会产生意料之外的目录——如果消息中的路径
看起来不对，这一点值得检查。

<a id="dsh9023"></a>

## DSH9023

<!-- generated:begin DSH9023 -->
**严重性** 错误

**消息**

```
The symbol index could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:816`
<!-- generated:end DSH9023 -->

**原因。** `index` 无法创建其输出目录或写入索引文件。默认
目录为 `<Project>/Saved/DreamShader/Index`。

**修复。** 同 DSH9022。

<a id="dsh9024"></a>

## DSH9024

<!-- generated:begin DSH9024 -->
**严重性** 错误

**消息**

```
The builtin catalog manifest could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1353`
<!-- generated:end DSH9024 -->

**原因。** `export-catalog` 无法创建其目录或写入清单。默认路径为
`<Project>/Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json`，与其他桥接
清单并列。

**修复。** 同 DSH9022。如果桥接正在运行，该目录一定存在，因此这指向一个
锁或一个只读的 `Saved/`。

<a id="dsh9025"></a>

## DSH9025

<!-- generated:begin DSH9025 -->
**严重性** 错误

**消息**

```
The builtin catalog came back empty, so '{0}' describes no expression at all. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded.
```

**由以下触发** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1367`
<!-- generated:end DSH9025 -->

**原因。** 清单已写入，但完全不描述任何表达式。一个针对空目录进行绑定的语言服务会把项目中的每一个 `UE.*` 名称都报告为未知，这读起来像是语言错误而不是缺少导出——所以尽管文件存在，这仍是一个错误。

原因与 DSH8297 相同：反射没有找到 `UMaterialExpression` 子类。

**修复。** 见 DSH8297。

<a id="dsh9026"></a>

## DSH9026

<!-- generated:begin DSH9026 -->
**严重性** 错误

**消息**

```
'{0}' is not a shader platform this engine knows. Write SM6, SM5, ES3_1, or a shader format name such as PCD3D_SM6.
```

**由以下触发** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:438`
<!-- generated:end DSH9026 -->

**原因。** `-Platform=` 指定了此引擎没有对应着色器平台的内容。可接受的写法是短配置文件（`SM6`、`SM5`、`ES3_1`），它们分别映射到 `PCD3D_SM6`、`PCD3D_SM5` 和
`PCD3D_ES3_1`，以及引擎已知的任何着色器格式名称（`PCD3D_SM6`、`SF_VULKAN_SM6`、
`SF_METAL_SM5`）。注意，格式名称与 `EShaderPlatform` 枚举器名称并不相同：是 `PCD3D_SM6`，而不是 `SP_PCD3D_SM6`。

**修复。** 使用其中一个短配置文件，或精确的着色器格式名称。多个值之间以逗号
分隔：`-Platform=SM6,SM5`。

<a id="dsh9027"></a>

## DSH9027

<!-- generated:begin DSH9027 -->
**严重性** 错误

**消息**

```
'{0}' is not a material quality level. Write Low, Medium, High or Epic.
```

**由以下触发** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:455`
<!-- generated:end DSH9027 -->

**原因。** `-Quality=` 指定了不是材质质量级别的内容。共有四个：`Low`、
`Medium`、`High`、`Epic`。

**修复。** 使用这四个之一。多个值之间以逗号分隔。省略该开关会检查项目当前的
可扩展性级别，这正是编辑器所显示的。

<a id="dsh9028"></a>

## DSH9028

<!-- generated:begin DSH9028 -->
**严重性** 错误

**消息**

```
Shader compilation for '{0}' did not finish within {1} seconds per material. A compile that never finishes is usually a dynamic loop or a texture read whose mip cannot be resolved in a divergent branch; move it into a '@custom' body with an explicit SampleLevel.
```

**由以下触发** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:791`
<!-- generated:end DSH9028 -->

**原因。** 着色器编译未在超时时间内完成——默认每个材质 120 秒，可通过 `-Timeout=` 更改。运行会停止等待并报告；它不会一直等下去，因为永远无法完成的编译正是此关卡存在所要捕获的失败。

导致此问题的两种形态都是真实存在的，且都在消息中有所提及：一种是编译器无法确定循环次数的循环，另一种是因其读取位于发散分支中而无法确定 mip 级别的纹理读取。两者都会让着色器编译器执行无界的工作，而不是直接失败。

**修复。** 找到那个循环或纹理读取。如果循环次数不是常量，循环应放在 `@custom` 主体中（前端在图中会以 DSH4360 拒绝它，所以这里几乎总是已经是 HLSL 循环）。分支中的纹理读取需要显式级别——`Tex.SampleLevel(UV, 0)`
而不是 `Tex.Sample(UV)`。

<a id="dsh9029"></a>

## DSH9029

<!-- generated:begin DSH9029 -->
**严重性** 错误

**消息**

```
[{0} / {1}] {2}
```

**由以下触发** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:692`
<!-- generated:end DSH9029 -->

**原因。** 着色器编译器拒绝了生成的 HLSL。消息是编译器自身的文本，前面加上了其来源的着色器平台和质量级别，并且诊断信息会尽可能精确地定位到位置：

1. 当错误指名某个表达式时，通过资产的 `DreamShader.SourceSpans` 表——精确的
   文件、行、列和长度；
2. 当错误指名生成 HLSL 中的某一行时，通过 `@custom` 节点代码内的 `// Begin/End DreamShader source:` 标记，
   在该块内按行计数；
3. 当两种方式都不奏效时，定位到源文件的第 1 行，并在 `detail` 中给出编译器的原始文本。

因此，位置为第 1 行意味着“此错误属于此文件，但无法精确定位”，
而绝不是“错误在第 1 行”。

**修复。** 阅读编译器自身的消息。在 DreamShader 源码中，绝大多数常见原因是
在 `@custom` 主体中使用了图未定义的符号——`@custom` 主体内的 `UE.*` 调用不会被提升为输入引脚，因此在那里写一个会产生此处未声明的标识符，而不是一个节点。

<a id="dsh9030"></a>

## DSH9030

<!-- generated:begin DSH9030 -->
**严重性** 错误

**消息**

```
DreamShader failed to create graph dump directory '%s'.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1888`
<!-- generated:end DSH9030 -->

**原因。** `dump-graph` 无法创建转储所属的文件夹。转储树镜像源树——`<Out>/<root>/<source path>.<asset>.graph.json`——因此每个源子目录会创建一个文件夹，而这里失败的正是该 `MakeDirectory`：驱动器不存在下的 `-Out`、进程无法写入的文件夹，或者转储需要目录的位置上坐落着一个*文件*

**修复。** 检查 `-Out` 路径是否存在且可写，并确认没有任何文件已经占用转储所需的某个目录名。没有 `-Out` 时，目标是 `<Project>/Saved/DreamShader/GraphBaseline`，只有 `Saved/` 本身为只读时才会失败

**另见** [Commandlet](../../tools/commandlet.md#dump-graph-since-190)

<a id="dsh9031"></a>

## DSH9031

<!-- generated:begin DSH9031 -->
**严重性** 错误

**消息**

```
DreamShader failed to write graph dump '%s'.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1899`
<!-- generated:end DSH9031 -->

**原因。** 文件夹存在，但 JSON 无法写入其中。几乎总是因为该文件在另一个程序中被打开，或者因为上一次捕获被提交到版本控制并检出为只读而变为只读

**修复。** 关闭占用该文件的内容，或清除只读标志，然后重新运行。捕获到一个全新的空目录可以同时绕开这两者——基线是整体写入的，因此旧目录中没有需要保留的内容

**另见** [Commandlet](../../tools/commandlet.md#dump-graph-since-190)

<a id="dsh9032"></a>

## DSH9032

<!-- generated:begin DSH9032 -->
**严重性** 错误

**消息**

```
DreamShader could not work out which assets '%s' builds, so there is no graph to dump.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1792`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1801`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1840`
<!-- generated:end DSH9032 -->

**原因。** 文件已被读取并预处理，但它没有解析出可转储的资产：要么解析失败，要么源文件完全没有声明 `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 块。`.dsh` 头永远不会走到这一步，但一个只声明 `Function` 或 `VirtualFunction` 主体的 `.dsm` 会这样——那些只会生成一个 `.ush` 包含或什么都不生成，而不是生成图，因此没有可供指纹描述的内容

**修复。** 如果消息带有解析错误，请修复源码；对同一文件运行 `compile` 会以自己的代码报告相同的失败。如果文件是一个合理地不产生任何资产的辅助文件，它就没有可转储的内容，可以排除在捕获之外——`-All` 会访问它并报告它，这就是为什么对一棵辅助源码树运行 `-All` 会以 `1` 退出，而这棵树本身并没有任何问题

**另见** [Commandlet](../../tools/commandlet.md#dump-graph-since-190)

<a id="dsh9033"></a>

## DSH9033

<!-- generated:begin DSH9033 -->
**严重性** 错误

**消息**

```
DreamShader could not resolve generated asset '%s' from '%s' after generation.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1864`
<!-- generated:end DSH9033 -->

**原因。** 生成报告成功，但它本应产生的资产无法从源文件命名的对象路径加载回来。通常的原因是资产已存在于磁盘上，`dump-graph` 的写入防护拒绝重建它，*并且*它加载失败——源文件仍声称的某个路径背后是一个损坏或缺失的包

**修复。** 在源上运行 `compile -Force`，查看编译器对同一路径的反馈；无法加载的包在那里也会失败，并给出关于该包而非关于转储的消息。如果资产是被手动删除的，编译一次即可重新创建它

**另见** [Commandlet](../../tools/commandlet.md#dump-graph-since-190)

<a id="dsh9034"></a>

## DSH9034

<!-- generated:begin DSH9034 -->
**严重性** 错误

**消息**

```
DreamShader cannot dump '%s': %s is not a Material, MaterialFunction, material instance or pass pipeline.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1876`
<!-- generated:end DSH9034 -->

**原因。** 位于源资产路径上的对象不是转储所覆盖的类 —— 不是 `UMaterial`，不是 `UMaterialFunction`（或层 / 层混合），也不是 `UDreamShaderMaterialInstance`。有其他东西占用了源解析到的路径；生成本身拒绝覆盖外来资产（`DSH8102` / `DSH8103` 之类）

**修复。** 查看消息所指的资产，要么将其移开，要么更改源的 `Name=` / `Root=`，使两者不再冲突。转储报告的冲突与生成会报告的是同一个；只是它通过不同途径到达那里，因为它不必写入

**另见** [Commandlet](../../tools/commandlet.md#dump-graph-since-190)

<a id="dsh9035"></a>

## DSH9035

<!-- generated:begin DSH9035 -->
**严重性** 错误

**消息**

```
'{0}' is not a compilable DreamShader source (.dss, .dsi, .dsp, .dsm or .dsf), so '{1}' has nothing to do with it; a .dsh header is checked through a source that includes it.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:215`
<!-- generated:end DSH9035 -->

**原因。** `check`、`dump-ir` 或 `index` 被给予了一个不是 `.dss` 的文件。这三个动词驱动 2.0 管线；`.dsm` 和 `.dsf` 属于 `compile`，后者会将它们路由到 1.x 生成器。

注意 `-All` 从不会产生此情况：它只枚举 `.dss` 文件。这总是来自显式的 `-Source=` 或位置路径。

**修复。** 将动词指向 `.dss`，或对 1.x 源使用 `compile`。

<a id="dsh9036"></a>

## DSH9036

<!-- generated:begin DSH9036 -->
**严重性** 错误

**消息**

```
The diagnostics JSON could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderCommandletRunner.cpp:800`、`Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:484`
<!-- generated:end DSH9036 -->

**原因。** 给出了 `-DiagnosticsOut=`，但 JSON 无法写入。只有一个源时，路径按给定值使用；使用 `-All` 时，它会变成一个目录，每个源写入一个文件，这样每个源就不会覆盖上一个。

**修复。** 检查该路径是否可写。使用 `-All` 时，请传入目录而不是文件名。

<a id="dsh9037"></a>

## DSH9037

<!-- generated:begin DSH9037 -->
**严重性** 信息

**消息**

```
'{0}' produced no material, so there are no shaders to compile; a function library is checked by the material that calls it.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:571`
<!-- generated:end DSH9037 -->

**原因。** 信息性。`check -Shaders` 在产生不出材质的源上运行了 —— 一个仅含导出函数的文件。没有什么着色器可编译：材质函数本身没有着色器，它作为每个调用它的材质的一部分被编译。

**修复。** 无需处理。要覆盖一个函数库，请检查一个调用它的材质。

<a id="dsh9038"></a>

## DSH9038

<!-- generated:begin DSH9038 -->
**严重性** 警告

**消息**

```
Shader errors cannot be read in this configuration: '-nullrhi' switches the rendering shader maps off, and no cook target platform matched the requested platforms. Re-run without '-nullrhi', or pass a '-Platform=' an active target platform supports.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:800`
<!-- generated:end DSH9038 -->

**原因。** `check -Shaders` 在一种无法读取任何编译错误的配置下运行。`-nullrhi` 关闭了 `FApp::CanEverRender()`，而 `UMaterial::CacheResourceShadersForRendering` 正是以其为门控条件的，因此渲染着色器映射 —— 插件为其提供公共访问器的唯一 `FMaterialResource` —— 从未被构建；并且没有 cook 目标平台匹配所请求的平台，所以另一半也无从驱动。

该次运行仍会报告超时，也仍会报告引擎记录的任何内容，因此并非无用 —— 但它无法保证一次静默的运行就意味着一次干净的运行。

**修复。** 不带 `-nullrhi` 重新运行（`.skill/dsc.ps1` 会自动为 `check -Shaders` 去掉它；传入 `-NullRhi` 可强制重新启用），或传入项目某个活动目标平台支持的 `-Platform=`。

<a id="dsh9039"></a>

## DSH9039

<!-- generated:begin DSH9039 -->
**严重性** 错误

**消息**

```
%s: DSH9039: the DreamShader 2.0 pipeline failed without raising a diagnostic.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1398`
<!-- generated:end DSH9039 -->

**原因。** 内部不变量。2.0 管线返回失败，却没有在其诊断接收器中放入任何一条错误，因此没有关于该源可报告的内容。管线的每个阶段在拒绝之前都会先引发，所以到达这里意味着某个阶段在一条不这样做的路径上返回了 false。

**修复。** 这不是源的问题 —— 报告它。消息会指出文件名，这足以复现。对同一文件运行 `dsc dump-ir` 通常能显示该次运行推进到了哪里，因为该动词会保留管线产生的任何内容，即使它失败了。

<a id="dsh9040"></a>

## DSH9040

<!-- generated:begin DSH9040 -->
**严重性** 错误

**消息**

```
The layout dump could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:719`
<!-- generated:end DSH9040 -->

**原因。** `dsc dump-layout` 无法写入其某个文件 —— SVG，或 `-Json` 下与其并排的 JSON。消息中携带了文件系统的原因。

**修复。** 将 `-Out` 指向可写入的目录，并关闭任何占用该文件的对象。

<a id="dsh9041"></a>

## DSH9041

<!-- generated:begin DSH9041 -->
**严重性** 错误

**消息**

```
'{0}' is not a layout style; -Style takes Blocks, SourceBands, Layered or All.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:642`
<!-- generated:end DSH9041 -->

**原因。** `dsc dump-layout -Style` 被给予的不是 `Blocks`、`SourceBands`、`Layered` 或 `All`。`Classic`，即 1.x 布局，不在其中：它作用于已完成的图，无法从 IR 绘制。

**修复。** 对每一种各出一张图时不要包含 `-Style`，或指定三者之一。

<a id="dsh9042"></a>

## DSH9042

<!-- generated:begin DSH9042 -->
**严重性** 信息

**消息**

```
'{0}' has 1.x declarations, and what the printer writes for those is 2.0 text; rewriting 1.x as 2.0 is 'dsc migrate', so 'fmt' leaves the file as it is.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:108`
<!-- generated:end DSH9042 -->

**原因。** `dsc fmt` 遇到了一个仍带有 1.x 声明的头文件。打印器写入的是 2.0 文本，因此格式化这样的文件会走后门将其迁移 —— 且不经过 `dsc migrate` 运行的检查。这是信息，不是错误：文件保持原样，运行继续。

**修复。** 无需处理。当该头文件要变为 2.0 时运行 `dsc migrate`；此后 `fmt` 会格式化它。

<a id="dsh9043"></a>

## DSH9043

<!-- generated:begin DSH9043 -->
**严重性** 信息

**消息**

```
'{0}' uses the preprocessor outside a custom body; 'fmt' reads the file as it is on disk and would have to drop one side of every '#if', so it leaves the file as it is.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:94`
<!-- generated:end DSH9043 -->

**原因。** `dsc fmt` 遇到了一个在 `/// @custom` 体之外使用 `#if`、`#ifdef` 或类似内容的文件。格式化器按磁盘上的原样读取文件，不进行预处理，而解析器只保留指令之间的文本 —— 它必须丢弃每个条件的一侧。这是信息，不是错误：文件保持原样。

**修复。** 无需处理。手动格式化此类文件，或将条件部分移入其自己的头文件。

<a id="dsh9044"></a>

## DSH9044

<!-- generated:begin DSH9044 -->
**严重性** 错误

**消息**

```
The formatted text of '{0}' failed its own check -- {1} -- so nothing was written. This is a fault of the formatter, not of the file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:130`
<!-- generated:end DSH9044 -->

**原因。** `dsc fmt` 产生的文本未通过格式化器自身的检查：它必须能解析，解析出的声明与原始一致，保留每个 `//` 和 `/* */` 注释，并在再次格式化时保持不变。没有写入任何内容。这是格式化器的故障，绝不是文件的故障。

**修复。** 连同该文件一起报告。消息会指出四项检查中哪一项失败。

<a id="dsh9045"></a>

## DSH9045

<!-- generated:begin DSH9045 -->
**严重性** 错误

**消息**

```
'{0}' could not be read, so it was not formatted.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:926`、`Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:970`
<!-- generated:end DSH9045 -->

**原因。** `dsc fmt` 无法读取某个源文件，或者无法将格式化后的文本写回。对于第二种情况，只读文件——已签入 Perforce 但未签出——是常见原因。

**修复。** 签出该文件，或使其可写，然后再次运行 `fmt`。`fmt -Check` 只读取。

<a id="dsh9046"></a>

## DSH9046

<!-- generated:begin DSH9046 -->
**严重性** 错误

**消息**

```
'{0}' is not in the formatter's layout; 'dsc fmt' would rewrite it.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:960`
<!-- generated:end DSH9046 -->

**原因。** `dsc fmt -Check` 发现了一个 `fmt` 将会重写的文件。`-Check` 不写入任何内容并使运行失败，这正是 CI 所需的形式。

**修复。** 对该文件运行 `dsc fmt`（或 `-All`）并提交结果。

<a id="dsh9047"></a>

## DSH9047

<!-- generated:begin DSH9047 -->
**严重性** 错误

**消息**

```
The list of generated assets could not be written: {0}.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1313`
<!-- generated:end DSH9047 -->

**原因。** `dsc list-generated -Out` 无法写入该列表。消息中带有文件系统给出的原因。

**修复。** 将 `-Out` 指向一个可写入的文件；相对路径是相对于调用 `dsc.ps1` 时所在的目录。

<a id="dsh9048"></a>

## DSH9048

<!-- generated:begin DSH9048 -->
**严重性** 错误

**消息**

```
'-As={0}' is no list format; the four are Packages, Files, GitIgnore and Json.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1103`
<!-- generated:end DSH9048 -->

**原因。** `dsc list-generated -As`（`dsc.ps1` 中的 `-ListAs`）收到的不是那四种格式之一。

**修复。** `Packages` 用于包名，`Files` 用于项目相对文件，`GitIgnore` 用于现成的忽略块，`Json` 用于所有情况。

<a id="dsh9049"></a>

## DSH9049

<!-- generated:begin DSH9049 -->
**严重性** 警告

**消息**

```
{0} generated asset(s) lie outside the project directory -- an engine plugin's content -- and have no project-relative path; '-As=Packages' or '-As=Json' lists them.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1301`
<!-- generated:end DSH9049 -->

**原因。** 某些生成的资源位于项目目录之外——例如安装在引擎中的插件的资源——因此它们没有项目相对路径，被排除在 `Files` 或 `GitIgnore` 列表之外。这是警告：列表的其余部分是完整的。

**修复。** 使用 `-As=Packages` 或 `-As=Json` 来查看它们。引擎插件资源的忽略规则应放在该插件的仓库中。

<a id="dsh9050"></a>

## DSH9050

<!-- generated:begin DSH9050 -->
**严重性** 错误

**消息**

```
reveal-node needs a non-empty 'file' and a 'line' of 1 or more; got file '{File}' and line {Line}.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:885`
<!-- generated:end DSH9050 -->

**原因。** 一个 `reveal-node` 请求到达桥接时缺少构成它的两个字段：`file` 必须是非空的源路径，`line` 必须为 1 或更大。完全省略 `line` 的请求会被读取为第 0 行，也会落到这里，这是刻意为之的——“在这个文件中的某处”并不是一个节点。

**修复。** 同时发送这两个字段。`file` 可以是绝对路径或项目相对路径；它会以与编译器规范化源路径相同的方式被规范化，因此两种写法都匹配同一个文件。`line` 是从 1 开始的，与 DreamShader 线路中的每个行号一样，因此按 0 开始计数的编辑器必须在发送前加一。

<a id="dsh9051"></a>

## DSH9051

<!-- generated:begin DSH9051 -->
**严重性** 错误

**消息**

```
No asset loaded in this editor was generated from '{File}', so there is no graph to reveal a node in; compile the file first, or open one of its assets once so the editor knows about it.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:908`
<!-- generated:end DSH9051 -->

**原因。** 编辑器中当前加载的任何内容都不带有指定该源的 `DreamShader.SourceFile` 元数据。要么该文件在此会话中从未被编译过，要么它编译成的资源已在磁盘上但尚未被加载——资源注册表知道的包仍然不是活动的 `UObject`，而源文件戳记存在于包元数据中，不加载包就无法查询。

**修复。** 先编译该文件（使用 `scope: "file"` 的 `recompile`、*重新编译 DSM*，或开启自动编译后保存），或者打开它的某个资源一次，使该包被加载。如果该文件确实不产生任何资源——一个 `.dsh` 头，或者一个所有产物都存在于另一个文件中的 `.dss`——那就没有什么可揭示的，请求指向了错误的文件。

<a id="dsh9052"></a>

## DSH9052

<!-- generated:begin DSH9052 -->
**严重性** 错误

**消息**

```
The {Count} asset(s) generated from '{File}' carry no DreamShader.SourceSpans metadata, so no node on them can be located; they predate node navigation -- rebuild them with -Force.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:944`
<!-- generated:end DSH9052 -->

**原因。** 找到了由该源生成的资源，但它们都没有携带 `DreamShader.SourceSpans` 表。该表仅由 2.0 发射器写入：由 1.x 生成器构建的资源，或由节点导航功能落地之前的 2.0 构建所构建的资源，具有源文件戳记但没有逐节点的跨度。

**修复。** 使用 `-Force` 重新构建该源（源哈希表明该资源是最新的，因此普通重建会跳过它，表永远不会被写入）。`.dsm` / `.dsf` 源由同一个发射器构建，也会得到该表；只有 1.x 生成器留下的资源才会触发此错误。

<a id="dsh9053"></a>

## DSH9053

<!-- generated:begin DSH9053 -->
**严重性** 错误

**消息**

```
The DreamShader.SourceSpans metadata of '{Asset}' is not a JSON object, so no node on it can be mapped back to a source line; rebuild the asset from its source.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:387`
<!-- generated:end DSH9053 -->

**原因。** 该资源的 `DreamShader.SourceSpans` 元数据存在，但不是 JSON 对象——它完全无法反序列化。包元数据是一个纯字符串映射，因此任何内容都可能被写入该键；在实践中，这意味着该值被截断或被手动编辑过。

**修复。** 使用 `-Force` 从其源重新构建该资源，这会重写整个表。如果问题再次出现，说明该值正被发射器以外的其他东西写入——检查是否有工具或脚本在生成的资源上写入包元数据。

<a id="dsh9054"></a>

## DSH9054

<!-- generated:begin DSH9054 -->
**严重性** 错误

**消息**

```
No node generated from '{File}' has a source span on line {Line}; the line produced no graph node (a declaration, a comment, or a statement that folded away).
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:953`
<!-- generated:end DSH9054 -->

**原因。** 表被读取且文件匹配，但没有任何节点的跨度从该行开始、从该行的辅助调用到达，或在该行之前开始。真实源的多数行都是如此：一个变成参数节点的 `uniform` 声明位于自己的行上，但空行、注释、右花括号、`struct` 字段、`#pragma`，或被常量折叠移除的语句都不会产生任何节点。

**修复。** 瞄准携带表达式的行。请注意，去重阶段会将两个相同的表达式合并为一个节点，其跨度是它们中的第一个，因此重复表达式的第二种写法没有自己的节点，会得到此结果。

<a id="dsh9055"></a>

## DSH9055

<!-- generated:begin DSH9055 -->
**严重性** 错误

**消息**

```
The material editor would not open for '{Asset}', so the node could not be revealed.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1003`
<!-- generated:end DSH9055 -->

**原因。** 为该资源调用了 `UAssetEditorSubsystem::OpenEditorForAsset`，但完全没有返回任何编辑器。常见原因是一个其编辑器无法在此会话中打开的资源：一个 Ephemeral 且在磁盘上没有包的 ThinCustom 基础材质，或者一个编辑器正处于关闭过程中的资源。能打开但是*错误*编辑器的属于 DSH9056，而不是此错误。

**修复。** 手动打开该资源一次，以查看真正的拒绝原因，资源编辑器会自行报告该原因。对于 ThinCustom 产物，先将其实体化——隐藏的基础才是拥有图表的东西，而 Ephemeral 基础在被实体化为资源之前没有编辑器。

<a id="dsh9056"></a>

## DSH9056

<!-- generated:begin DSH9056 -->
**严重性** 错误

**消息**

```
'{Asset}' opened in an editor that is not the material editor, which has no node graph to select in; only a node of a Material or a Material Function can be revealed.
```

**引发者** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1009`
<!-- generated:end DSH9056 -->

**原因。** 为资产打开了编辑器，但不是材质编辑器——因此没有可选择的节点图。`UMaterialInstance` 会这样做：它打开材质*实例*编辑器，其中只有参数列表而没有图，并且它不是 `IMaterialEditor`。揭示路径有意指向承载图的资产，因此出现此情况意味着承载跨度表的资产并非它所声称的那种资产。

**修复。** 在源生成的材质或材质函数中揭示，而不是在其实例中。对于 ThinCustom 产品，可寻址的资产是实例，但图位于其隐藏的基础材质上，而 `reveal-node` 的答案会在 `assetPath` 中指明该基础材质——实例会单独在 `instanceAssetPath` 中报告。如果 `assetPath` 已经指明了一个材质而此问题仍然出现，则该资产的类与其元数据不一致；请使用 `-Force` 从源重新构建它。

<a id="dsh9057"></a>

## DSH9057

<!-- generated:begin DSH9057 -->
**严重性** 错误

**消息**

```
The expression {Guid} recorded for line {Line} is not in '{Asset}' any more, so the node could not be selected; the asset changed since it was generated -- rebuild it from its source.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1017`
<!-- generated:end DSH9057 -->

**原因。** 跨度表中指明的表达式 GUID 已不在该资产的图中。表和图是一起写入的，因此这意味着其中之一在此之后发生了变化：某个节点被手动删除，或者资产被还原为较旧版本而其元数据没有。

**修复。** 使用 `-Force` 从源重新构建该资产。如果该资产是被有意手动编辑的，请先解决这种分歧（*Revert to Source*、*Adopt Into Source* 或 *Detach From DreamShader*）——分离后的资产既不保留标记也不保留表，并完全停止响应导航，对于不再生成的资产来说这是正确的结果。

<a id="dsh9058"></a>

## DSH9058

<!-- generated:begin DSH9058 -->
**严重性** 错误

**消息**

```
The source file '{File}' this node was generated from is not on disk any more, so it could not be opened; regenerate the asset, or restore the file.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:588`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:594`
<!-- generated:end DSH9058 -->

**原因。** *Open Source Line*（或 *Open Call Site*）无法为该文件启用文本编辑器。两种不同的原因共用此代码，因为二者的用户下一步操作相同：跨度中指明的文件已不在磁盘上，或者链中的每个启动器都拒绝了——VSCode、该扩展名的操作系统默认编辑器以及记事本。

**修复。** 如果文件缺失，说明资产的寿命长于其源：重新生成它，或从版本控制恢复该文件。如果文件存在，则问题出在启动器链上——检查该扩展名是否有默认应用程序，并参阅 [Workspace](../../tools/workspace.md) 了解 VSCode 是如何被发现的。

<a id="dsh9059"></a>

## DSH9059

<!-- generated:begin DSH9059 -->
**严重性** 警告

**消息**

```
The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an expression GUID and was skipped; that node cannot be navigated to.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:402`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:416`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:440`
<!-- generated:end DSH9059 -->

**原因。** `DreamShader.SourceSpans` 表中的某一行无法读取：其键不是 GUID、其值不是对象，或者该行未指明文件（或行号低于 1）。该行会被跳过，表的其余部分仍会被使用，因此导航对其他每个节点仍然有效——这之所以是警告，正是因为一行坏数据不足以成为拒绝整个资产的理由。

**修复。** 使用 `-Force` 从源重新构建该资产。如果某一行反复以格式错误的形式出现，那是发射器缺陷，而不是创作错误：请附上消息中携带的资产路径进行报告。

<a id="dsh9060"></a>

## DSH9060

<!-- generated:begin DSH9060 -->
**严重性** 错误

**消息**

```
'{0}' is neither a material nor a material function, so it has no graph to read. A material instance decompiles to a '.dsi'.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:3101`
<!-- generated:end DSH9060 -->

**原因。** 交给 2.0 反编译器的资产既不是材质也不是材质函数（包括层和混合），因此没有可读取的表达式图。普通材质实例由实例反编译器处理，并输出为一个 `.dsi`。

**修复。** 反编译材质或函数本身；对于实例，使用同一命令，它会自行将其路由到 `.dsi` 输出。

<a id="dsh9061"></a>

## DSH9061

<!-- generated:begin DSH9061 -->
**严重性** 信息

**消息**

```
{0} expression(s) of '{1}' feed no output and are not part of the source; a build would prune them all the same.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2894`
<!-- generated:end DSH9061 -->

**原因。** 图中包含不向任何输出馈送的表达式：编辑留下的残余。源无法表达“没有任何东西读取的节点”——编译器会再次将其剪除——因此它们被排除在外。

**修复。** 无需修复。如果希望它们保留下来，请先在资产中将它们连接到某个对象。

<a id="dsh9062"></a>

## DSH9062

<!-- generated:begin DSH9062 -->
**严重性** 警告

**消息**

```
The reroute {0} feeds itself; the pin that reads it is treated as unconnected.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:511`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:605`
<!-- generated:end DSH9062 -->

**原因。** 图中存在循环：Reroute（或命名 reroute）链回到自身，或者某个节点读取了反过来读取它的节点。引擎会将这样的连线编译为未连接；反编译器将 reroute 引脚视为未连接，并为闭合节点循环的读取写入零值。

**修复。** 在材质编辑器中删除形成循环的连线并再次反编译。

<a id="dsh9063"></a>

## DSH9063

<!-- generated:begin DSH9063 -->
**严重性** 警告

**消息**

```
The named reroute {0} has no declaration; the pin that reads it is treated as unconnected.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1666`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1939`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1975`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:487`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:525`
<!-- generated:end DSH9063 -->

**原因。** 某个节点读取了不存在的东西：其声明已不在图中的 NamedRerouteUsage（被删除，或从另一个材质复制时丢失）、未接入任何内容的 Reroute，或未接入任何材质的 Break/GetMaterialAttributes 节点。Convert 节点也会如此，当它要映射到该节点没有的引脚或通道上或从其中映射出时，以及读取超出接入其输入的值宽度的通道时——这两种情况引擎都拒绝编译。

**修复。** 在资产中重新连接或删除该节点。读取 reroute 的引脚会反编译为未连接；读取无材质属性的操作会写入为零。Convert 映射到该节点没有相应引脚的通道会被排除，因此该通道采用输出的默认值；超出已接入值的通道为零，这也是新材质转换器在此处读取到的值。

<a id="dsh9064"></a>

## DSH9064

<!-- generated:begin DSH9064 -->
**严重性** 警告

**消息**

```
{0} calls a material function that is missing; a zero stands in for every value read from it.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1419`
<!-- generated:end DSH9064 -->

**原因。** 某个 MaterialFunctionCall 节点的函数资产缺失（被删除，或其插件未挂载）。没有可用于读取引脚的接口，因此从该调用读取的每个值都会写入零，以保持其余文本可编译。

**修复。** 恢复该函数资产，或修复材质中的该调用，然后再次反编译。

<a id="dsh9065"></a>

## DSH9065

<!-- generated:begin DSH9065 -->
**严重性** 警告

**消息**

```
{0} is of a class the builtin catalog does not list (abstract, deprecated, or from a module loaded after the catalog was built); a zero stands in for every value read from it.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2351`
<!-- generated:end DSH9065 -->

**原因。** 某个表达式属于内置目录未列出的类——抽象类、已弃用类，或来自在目录构建之后才加载的模块。该语言只能命名目录中已有的类，因此用零代替该节点的值。

**修复。** 如果该类来自某个插件，请确保在 DreamShader 构建其目录之前加载该插件（启用后重新启动），然后再次反编译。

<a id="dsh9066"></a>

## DSH9066

<!-- generated:begin DSH9066 -->
**严重性** 警告

**消息**

```
The input '{0}' of '{1}' is a dynamic bool pin, which the language has no parameter for; it is written as 'bool' and rebuilds as a scalar pin.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1390`
<!-- generated:end DSH9066 -->

**原因。** 某个函数输入属于引擎的动态 `Bool` 引脚类型。该语言中，`bool` 参数是标量引脚，`/// @static` 参数是 StaticBool 引脚；它没有针对动态 bool 引脚的拼写方式。

**修复方法。** 接受标量引脚（调用方可能需要重新连接一次），或在反编译前在资源中
将输入的 类型 改为 StaticBool 或 Scalar。

<a id="dsh9067"></a>

## DSH9067

<!-- generated:begin DSH9067 -->
**严重性** 警告

**消息**

```
{0} carries {1} additional define(s), which a '/// @custom' function cannot declare; the rebuilt node has none. Move them into the body as '#define' lines.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1556`
<!-- generated:end DSH9067 -->

**原因。** 一个 Custom 节点带有 Additional Defines。一个 `/// @custom` 函数声明了输入、
输出和代码，但没有用于 defines 的指令。

**修复方法。** 将每个 define 作为 `#define` 行移动到函数体中。

<a id="dsh9068"></a>

## DSH9068

<!-- generated:begin DSH9068 -->
**严重性** 警告

**消息**

```
{0} changes '{1}', which a '///' directive cannot carry; the rebuilt parameter has the default.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1069`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2455`
<!-- generated:end DSH9068 -->

**原因。** 一个节点更改了文本无法承载的属性：在参数上，某个没有任何 `///`
指令代表的属性（通道名、曲线图集、自定义原始数据索引……）；在任何
其他节点上，某个调用参数无法容纳的类型的属性（引脚数组、映射、委托）。
结构体数组会按 ImportText 读取的文本写出，其中不含引脚
（`Layers = "((LayerName=\"Grass\"))"`）。重建后的节点会为其使用引擎默认值。

**修复方法。** 在重建后的资源上手动设置该属性，或将该节点排除在源之外
（手工制作的函数 + `extern`）。

<a id="dsh9069"></a>

## DSH9069

<!-- generated:begin DSH9069 -->
**严重性** 信息

**消息**

```
{0} has nothing wired to Value, so it is its {1} branch and nothing else; the rebuilt graph has no switch there.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1793`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2170`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2181`
<!-- generated:end DSH9069 -->

**原因。** 一个 StaticSwitch 的 `Value` 引脚没有连接任何内容，因此它始终采用其
`DefaultValue` 所命名的分支。源会直接说明该分支；重建后的图表中那里没有开关。
对于 `SwitchValue` 未连接任何内容、其 `ConstSwitchValue` 永久选择一个
输入（或默认值）的 Switch，以及除了默认值之外没有其他输入的 Switch，也是如此。

**修复方法。** 无需修复；如果该开关应为可切换的，请在资源中将静态布尔值连接到 `Value`（将值连接到 `SwitchValue`）。

<a id="dsh9070"></a>

## DSH9070

<!-- generated:begin DSH9070 -->
**严重性** 警告

**消息**

```
{0} has {1} wired input(s) the class does not declare as named pins, so a call cannot connect them; the rebuilt node leaves them unconnected.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2414`
<!-- generated:end DSH9070 -->

**原因。** 一个节点连接了其类未声明为命名引脚的输入（某个类别的动态输入，
而目录通过反射读取该类）。源中的调用按名称连接引脚，因此无法
写出这些连接。目录知道按节点为其引脚命名的类（Landscape 图层混合、
草地或物理材质输出）会改用该节点显示的名称写出；这仅适用于名称为空、
是关键字，或与另一个引脚或参数同名的引脚。

**修复方法。** 在反编译后手动重建该部分；消息中会指明该节点。

<a id="dsh9071"></a>

## DSH9071

<!-- generated:begin DSH9071 -->
**严重性** 警告

**消息**

```
The input '{0}' of '{1}' has its Preview pin wired. Source says that as a default that is an expression ('float {0} = UE.TexCoord(Index = 1).r'), which the decompiler does not write yet; the rebuilt input previews its number.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1397`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:3024`
<!-- generated:end DSH9071 -->

**原因。** 一个材质函数说明了其接口的某些内容，而源尚未写出这些内容。
一个 FunctionInput 连接了其 Preview 引脚：该输入的默认值是一个图表表达式，源
会将其表示为作为表达式的参数默认值——编译器会构建它，反编译器尚不
写出它，重建后的输入会预览其数字。或者该函数有一个用户公开的标题，
而没有任何指令承载它。

**修复方法。** 在反编译文本中手动写出默认值：`float Name = <expression>`。标题
必须在重建后的资源上设置。

<a id="dsh9072"></a>

## DSH9072

<!-- generated:begin DSH9072 -->
**严重性** 警告

**消息**

```
{0} is read through its output {1}, which is no material attribute the catalog knows; a zero stands in for that read.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1597`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1628`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:695`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:728`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:735`
<!-- generated:end DSH9072 -->

**原因。** 一个节点通过目录不了解的某种内容使用：
一个 Break/GetMaterialAttributes 输出不是材质属性（由插件添加，或是
表中不存在的索引）会被读取为零；一个节点没有对应槽位的输出索引会被读取为该节点的
第一个输出，而对一个其类未列出任何输出的节点的读取会被读取为零；一个
Set/MakeMaterialAttributes 输入不是已知属性时会被丢弃。

**修复方法。** 检查该属性是否属于一个未加载的插件；否则在资源中重新连接
该节点。

<a id="dsh9073"></a>

## DSH9073

<!-- generated:begin DSH9073 -->
**严重性** 信息

**消息**

```
{0} is a StaticSwitchParameter, which the language writes as a '/// @static' uniform and a static branch; the rebuilt graph has those two nodes in its place.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1191`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1329`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1716`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2064`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2227`
<!-- generated:end DSH9073 -->

**原因。** 一个引擎节点被该语言写为两个节点，或写为另一个类下的节点。一个
StaticSwitchParameter 既是参数又是开关：源会表示一个 `/// @static uniform bool`
和一个静态分支，构建时会生成一个 StaticBoolParameter 加一个 StaticSwitch。一个纹理采样
参数会变成一个纹理 uniform 及其采样（TextureObjectParameter + TextureSample）。一个
GetMaterialAttributes 节点会重建为 BreakMaterialAttributes。一个 Convert 节点（Make / Break
FloatN）以及一个带有未命名分支的 Switch 会将其引脚保留在任何调用都无法命名的数组中，因此它们会
按其计算内容写出（所有分支都已命名的 Switch 会按 Switch 本身写出）：每个 Convert 输出按其组合来源的通道写出（ComponentMask 和 AppendVector
节点），一个 Switch 按引擎由它生成的各分支写出（Floor 和 If 节点）。实例会继续
正常工作：参数名称和静态排列相同。

**修复方法。** 无需修复。

<a id="dsh9074"></a>

## DSH9074

<!-- generated:begin DSH9074 -->
**严重性** 信息

**消息**

```
'{0}' reads its attributes as one set, so the {1} individual pin(s) that are also wired are ignored, by the engine and here.
```

**由以下内容引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2985`
<!-- generated:end DSH9074 -->

**原因。** 一个启用了 *Use Material Attributes* 的材质会从唯一的
MaterialAttributes 引脚读取其属性；同时已连接的各单独属性引脚会被引擎忽略，
因此也会被反编译器忽略。

**修复方法。** 无需修复；在资源中断开无效连线以停止该提示。

<a id="dsh9075"></a>

## DSH9075

<!-- generated:begin DSH9075 -->
**严重性** 信息

**消息**

```
The uniform '{0}' is written as '{1}': the name is already taken in this file.
```

**由以下内容引发** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1277`、`Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:664`、`Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:69`
<!-- generated:end DSH9075 -->

**原因。** 反编译文件中的某项内容无法保留其名称：一个参数，其名称已被
另一个声明占用；一个局部变量，其名称与参数相同或不是该语言允许的标识符；一个
自定义函数，其名称已被占用。它会以另一个名称写出；uniform 会通过 `/// @name` 保留其真实
参数名称。

**修复方法。** 对于 uniform 和局部变量，无需修复。被重命名的自定义函数仍可能
在另一个自定义函数体内部以其旧名称被调用，而该函数体是逐字 HLSL：请手动更改该调用。

<a id="dsh9076"></a>

## DSH9076

<!-- generated:begin DSH9076 -->
**严重性** 警告

**消息**

```
A region of '{0}' is titled '{1}', which does not fit on a '#pragma region' line; it is written as '{2}'.
```

**由以下内容引发** `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:575`
<!-- generated:end DSH9076 -->

**原因。** 注释框的标题无法原样放在 `#pragma region` 行上（它含有换行，
或为空）。它会以可以使用的形式写出。

**修复方法。** 无需修复；如果愿意，可在文本中编辑该区域的标题。

<a id="dsh9077"></a>

## DSH9077

<!-- generated:begin DSH9077 -->
**严重性** 信息

**消息**

```
{0} node position(s) of '{1}' belong to values the source writes inline, and a position is kept by variable name; those nodes are placed by the layout pass when the file is built.
```

**由以下内容引发** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1698`、`Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1927`
<!-- generated:end DSH9077 -->

**原因。** 某些节点位置未能保留。位置通过变量名（`#pragma
layout(Node, Var = ...)`）按整个文件存储：源中内联写入的值没有可依附的名称，而文件中两个函数将其放置在不同位置的名称则存在歧义。这些节点将在下次构建时由布局处理阶段放置。

**修复。** 无需修复。为值赋予其自己的变量以保留其位置。

<a id="dsh9078"></a>

## DSH9078

<!-- generated:begin DSH9078 -->
**严重性** error

**消息**

```
The output '{0}' of '{1}' did not become a parameter; nothing is written to it.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1837`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1889`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1894`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1098`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:801`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:121`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:319`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:360`
<!-- generated:end DSH9078 -->

**原因。** 图中某物在正在写入的文本中没有位置：一个未成为参数的函数输出、一个名称不是该语言所具有的属性或引脚、一个既不是其类的引脚也不是参数可携带的名称的输入、一个根本无法写入为源的节点（`0.0` 代替其位置），或传递给 `.dss` 写入器的材质实例（它被写为 `.dsi`）。

**修复。** 在资产中将引脚、输出或属性重命名为标识符，然后再次反编译。对于无法写入的节点，消息会说明原因。

<a id="dsh9079"></a>

## DSH9079

<!-- generated:begin DSH9079 -->
**严重性** info

**消息**

```
{0} is an If whose branches no comparison selects between; it is written as 'UE.If(...)'.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1406`
<!-- generated:end DSH9079 -->

**原因。** If 节点的 A/B 输入不是该语言可写为 `a > b ? x : y` 的比较（其三个分支的差异方式没有任何单一运算符可选择）。它被写为节点调用 `UE.If(...)`，该调用构建相同的节点。

**修复。** 无需修复。

<a id="dsh9080"></a>

## DSH9080

<!-- generated:begin DSH9080 -->
**严重性** warning

**消息**

```
The parameter '{0}' appears more than once and its nodes do not agree on its default, group or kind; the uniform is written from the first one, in '{1}'.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:549`
<!-- generated:end DSH9080 -->

**原因。** 一个参数名被多个节点使用，而这些节点在默认值、组或类型上不一致。引擎采用其中之一；uniform 从遇到的第一个写出。

**修复。** 在资产中使节点一致，或检查写出的默认值是否是你想要的。

<a id="dsh9081"></a>

## DSH9081

<!-- generated:begin DSH9081 -->
**严重性** warning

**消息**

```
'{0}' is called and its interface was not available, so its 'extern' prototype is written from the calls alone: pins no call connects are missing from it, and their order is the order the calls wire them in.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1136`
<!-- generated:end DSH9081 -->

**原因。** 文本调用了一个材质函数，但无法读取其资产以获取接口（仅 Core 反编译，或资产缺失），因此 `extern` 原型是从调用推断的：没有调用连接的引脚缺失，其顺序为首次使用的顺序，默认值未知。

**修复。** 在编辑器中函数资产存在的情况下反编译，或根据资产的引脚手动更正原型。

<a id="dsh9082"></a>

## DSH9082

<!-- generated:begin DSH9082 -->
**严重性** warning

**消息**

```
The code of the custom node '{0}' carries DreamShader's markers and does not read as what the compiler writes; it is kept verbatim as the body of one function, the functions it embeds included.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1220`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1431`
<!-- generated:end DSH9082 -->

**原因。** Custom 节点的代码无法被干净地拆解。要么它带有 DreamShader 的开始/结束标记，且无法解析为代码构建器所写的内容——它在生成后被手动编辑过——并作为单个函数的主体原样保留，包括嵌入的辅助函数；要么某个引脚的名称是该语言无法声明的，因此参数以另一个名称写出，而主体仍然使用旧名称。

**修复。** 对于第一种，如果代码正确则无需修复；如果你希望辅助函数共享，手动将它们整理为各自的函数。对于第二种，将主体中的标识符重命名为与参数匹配。

<a id="dsh9083"></a>

## DSH9083

<!-- generated:begin DSH9083 -->
**严重性** warning

**消息**

```
'{0}' is a {1}, and its pins are not the ones the language writes that kind with; it is written as a plain exported function, which builds a material function.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:950`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:959`
<!-- generated:end DSH9083 -->

**原因。** 该资产是材质层或混合层，但其引脚不是该语言写入该类型所用的形状（一个带有额外输入的层，一个没有恰好两个材质输入的混合）。它被写为普通的导出函数，这会构建 MaterialFunction 而非层资产。

**修复。** 在资产中为层/混合提供标准引脚，或保持该资产手工制作并声明为 `extern`。

<a id="dsh9084"></a>

## DSH9084

<!-- generated:begin DSH9084 -->
**严重性** warning

**消息**

```
The output '{0}' of '{1}' is not connected to anything; nothing is written to it.
```

**抛出者** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1814`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1126`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1149`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1506`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1545`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:204`
<!-- generated:end DSH9084 -->

**原因。** 某物因图或语言没有可写的内容而保留在默认值：一个未连接任何东西的函数输出、自定义输出引脚或材质属性；一个为列表的节点属性，或其名称是参数无法携带的（保留在类默认值）；一个必需输入未连接（传递 `0.0`）；一个 `inout` 输入未连接（变量从零开始）；一个已连接输入其类未声明（丢弃）。

**修复。** 无需修复，除非该输出或输入本应携带值；那么在资产中连接它并再次反编译。

<a id="dsh9085"></a>

## DSH9085

<!-- generated:begin DSH9085 -->
**严重性** error

**消息**

```
'{0}' ends in '.{1}', which is read as {2} source, and the decompile was asked for {3} text; name the file after the text, or leave the format to the extension.
```

**抛出者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderDecompileService.cpp:295`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:212`
<!-- generated:end DSH9085 -->

**原因。** 输出文件的扩展名与所请求的文本不一致：`.dsm` / `.dsf` 被读取为 1.x 文本，其他任何内容被读取为 2.0，而反编译请求的是另一种；或者该资产是材质实例，它反编译为 `.dsi` 而不为其他。

**修复。** 根据你想要的文本命名文件，或将 `-Format` 保留为 `Auto`（并关闭 `-Out`），让资产决定。

<a id="dsh9086"></a>

## DSH9086

<!-- generated:begin DSH9086 -->
**严重性** error

**消息**

```
The decompile was asked for 2.0 text and was handed the 1.x decompiler, which writes '.dsm' and '.dsf' only; build the service with GetIRDecompiler() for Format = Dss.
```

**抛出者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderDecompileService.cpp:312`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:356`
<!-- generated:end DSH9086 -->

**原因。** 反编译无法接受交给它的内容。从命令行：该资产属于没有 DreamShader 源描述的类——反编译接受材质、材质函数、层或混合，或材质实例。从代码：对 2.0 文本的请求被路由到 1.x 反编译器，后者只写出 `.dsm` / `.dsf`。

**修复。** 传入一种受支持的资产类。第二种情况是调用方中的接线错误：为 `Format = Dss` 使用 `GetIRDecompiler()` 构建服务，如果它来自编辑器则报告它。

<a id="dsh9087"></a>

## DSH9087

<!-- generated:begin DSH9087 -->
**严重性** error

**消息**

```
'{0}' does not resolve to the assets it builds, so there is nothing to decompile for it.
```

**抛出者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:297`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:328`
<!-- generated:end DSH9087 -->

**原因。** `-SourceFile` 命名了一个源，而它构建的资产无法解析：该源编译得不够远，无法列出其产物，或者它们中还没有任何一个存在。

**修复。** 先编译该源，或直接命名该资产。

<a id="dsh9088"></a>

## DSH9088

<!-- generated:begin DSH9088 -->
**严重性** error

**消息**

```
The graph of '{0}' did not read into a valid module; the errors above say where. This is a defect of the decompiler, not of the asset.
```

**抛出者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:428`
<!-- generated:end DSH9088 -->

**原因。** 导入的图未通过 IR 验证。导入器产生了编译器自身规则拒绝的内容，这是反编译器的缺陷；其上面的错误说明了原因。

**修复。** 连同资产一起报告它。`-Format Legacy` 仍为同一资产提供 1.x 文本。

<a id="dsh9089"></a>

## DSH9089

<!-- generated:begin DSH9089 -->
**严重性** warning

**消息**

```
The decompiled text does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler.
```

**抛出者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:483`
<!-- generated:end DSH9089 -->

**原因。** 反编译器打印的文本无法重新解析。尽管如此仍会写出，以免丢失任何内容；消息会指出第一个解析错误。

**修复。** 随资产一起报告；手动修复所指出的行通常就能得到可用的文件。

<a id="dsh9090"></a>

## DSH9090

<!-- generated:begin DSH9090 -->
**严重性** 错误

**消息**

```
'{0}' fails conditional compilation ({1}: {2}), and a source with '#if' lines is not migrated in any case.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:106`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:117`
<!-- generated:end DSH9090 -->

**原因。** 源文件使用了 `#if` 条件编译（或未通过它）。迁移只会看到依据当前定义所走的分支，并会静默丢弃其他分支。

**修复。** 先移除或解决这些条件编译，或手动迁移文件。

<a id="dsh9091"></a>

## DSH9091

<!-- generated:begin DSH9091 -->
**严重性** 错误

**消息**

```
'{0}' names a source root in front of the path, which '#include' cannot; write the path from the root's own folder (a leading '/') or relative to this file, then migrate again.
```

**引发者** `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:502`
<!-- generated:end DSH9091 -->

**原因。** 一个 `import` 在路径前方指定了源根（`import
"MoonToon:Shared/Common.dsh"`). `#include` 没有这种形式。

**修复。** 从该根自身的文件夹开始书写路径，并带上前导 `/`，或相对于文件书写，然后再次迁移。

<a id="dsh9092"></a>

## DSH9092

<!-- generated:begin DSH9092 -->
**严重性** 错误

**消息**

```
{0} comment(s) of '{1}' would not be in the migrated file ({2}), so nothing was written; this is a fault of the migration, not of the source.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:609`
<!-- generated:end DSH9092 -->

**原因。** 1.x 文件中的注释会在迁移后的文本中缺失。每次重写都会与其源文件的注释进行核对，一旦丢失注释就不会写出任何内容。

**修复。** 这是迁移器的缺陷，而不是源文件的缺陷。随文件一起报告；消息会引用这些注释。

<a id="dsh9093"></a>

## DSH9093

<!-- generated:begin DSH9093 -->
**严重性** 错误

**消息**

```
'{0}' has no 1.x declaration left; there is nothing to migrate.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:548`
<!-- generated:end DSH9093 -->

**原因。** 该文件是一个仅包含 2.0 声明的 `.dsh`。已没有可迁移的内容。

**修复。** 无需处理。

<a id="dsh9094"></a>

## DSH9094

<!-- generated:begin DSH9094 -->
**严重性** 警告

**消息**

```
'{0}' keeps its '/// @root', the 1.x spelling of where its asset goes, because the place the new file would put it could not be worked out; check the asset path the first build reports.
```

**引发者** `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:2133`、`Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:437`、`Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:483`、`Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:609`、`Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:916`、`Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:926`
<!-- generated:end DSH9094 -->

**原因。** 迁移后的文本与 1.x 源文件存在差异，这是迁移器无法避免且不会阻止构建的：保留 `/// @root` 是因为无法确定新文件的位置；某个类型的可选输入没有可写入的默认值（它变为必需项）；多行文本被展平为一行 `///`；某个被选中的输出没有可读取的变量。此外还有对另一个文件所声明的 1.x 函数的调用，是按其当前形态为该函数书写的：一旦那个文件被迁移，一个原本不返回任何值的函数会返回它的第一个 `out`（规则 L9）——此时图中的调用会多传一个实参——而一个 `Namespace` 函数会保留其 `out`，因此原本读取其值的函数体就无处可读。

**修复。** 阅读消息：每种情况都会说明变更内容。检查资产路径或首次构建所报告的输入。

<a id="dsh9095"></a>

## DSH9095

<!-- generated:begin DSH9095 -->
**严重性** 错误

**消息**

```
'{0}' is not a 1.x source; migrate takes '.dsm', '.dsf' and '.dsh' files.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:514`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:522`
<!-- generated:end DSH9095 -->

**原因。** 传给 `dsc migrate` 的文件不是 1.x 源文件，或无法读取。

**修复。** 传入 `.dsm`、`.dsf` 或 `.dsh`。

<a id="dsh9097"></a>

## DSH9097

<!-- generated:begin DSH9097 -->
**严重性** 错误

**消息**

```
The migrated text of '{0}' does not build as 2.0 source ({1}), so nothing was written; the text is in '{2}'.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:627`
<!-- generated:end DSH9097 -->

**原因。** 重写后的文本在写出之前，已作为 2.0 源文件进行解析、绑定和降级，其 `@custom` 函数体中的每个调用都与被调用方所生成的 HLSL 进行了匹配（`DSH6262`、`DSH6265`、`DSH6266`）——头文件会降级为空，因此没有其他东西会读取其函数体——而它无法构建。没有写出任何内容；被拒绝的文本保存在 `Saved/DreamShader/Migrated/Rejected/` 下，随后的诊断会指向其中。

**修复。** 阅读被拒绝文件中的第一个错误。它要么是迁移器尚未处理的构造（请报告它），要么是源文件依赖的某种 1.x 宽松规则，而 2.0 没有对应写法——请在 1.x 源文件中修复该处并再次迁移。

<a id="dsh9098"></a>

## DSH9098

<!-- generated:begin DSH9098 -->
**严重性** 警告

**消息**

```
The migrated text of '{0}' does not build the graph the 1.x file builds: {1}
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:661`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:676`
<!-- generated:end DSH9098 -->

**原因。** 迁移后的文本能够构建，但其 IR 与 1.x 文件的不等价：要么图不同（消息会向下走到第一个不同的节点），要么资产会落在另一个路径上。那些属于同一资产两种写法的差异——引脚上的常量与其 `Const*` 对应项、恒等 swizzle、Custom 代码中的间距——不会被报告。

**修复。** 对于图差异，比较所指出的两个节点；典型原因是某个 1.x `OutputType` 错误描述了节点宽度，以及在 1.x 原本按原样接线更宽值的地方使用了显式 swizzle。对于路径差异，添加消息所给出的 `/// @name`。

<a id="dsh9099"></a>

## DSH9099

<!-- generated:begin DSH9099 -->
**严重性** 错误

**消息**

```
'{0}' already exists and is not written over; move it away, or migrate into another folder with -Out.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:148`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:555`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:704`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:720`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:732`、`Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:750`
<!-- generated:end DSH9099 -->

**原因。** 迁移无法读取、写入或替换文件：内置目录为空（没有任何东西指定 `UE.*` 节点），`.dss` 已存在，输出位置或备份位置不可写，或 1.x 文件无法移走——在后一种情况下，新文件也不会被写出，因为二者会声明相同的资产。

**修复。** 释放消息所指出的路径，或使用 `-Out` 迁移到另一个文件夹。

<a id="dsh9100"></a>

## DSH9100

<!-- generated:begin DSH9100 -->
**严重性** 错误

**消息**

```
'{0}' has no parent material, and a '.dsi' is nothing but overrides of one.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:175`
<!-- generated:end DSH9100 -->

**原因。** 材质实例没有父级。`.dsi` 只是对父级的覆盖，因此没有可写的内容。

**修复。** 在资产中设置该实例的父级。

<a id="dsh9101"></a>

## DSH9101

<!-- generated:begin DSH9101 -->
**严重性** 警告

**消息**

```
'{0}' overrides {1} parameter(s) of its material layers or blends, which a '.dsi' cannot address; they are left out.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:300`
<!-- generated:end DSH9101 -->

**原因。** 该实例覆盖了其材质层或混合的参数。`.dsi` 只寻址全局参数，因此这些覆盖会从文本中略去，并在重建时丢失。

**修复。** 将此类实例保持为手工制作，或将这些值移到父级的全局参数中。

<a id="dsh9102"></a>

## DSH9102

<!-- generated:begin DSH9102 -->
**严重性** 信息

**消息**

```
The parameter '{0}' is not a name a variable can have; it is written under another with '/// @name {0}'.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:289`
<!-- generated:end DSH9102 -->

**原因。** 参数名称不是标识符（`Base Color`、`UV-Scale`）。该覆盖会以标识符声明，并携带 `/// @name <real name>`。

**修复。** 无需修复。

<a id="dsh9103"></a>

## DSH9103

<!-- generated:begin DSH9103 -->
**严重性** 错误

**消息**

```
DSH9103: The Parent of '{0}' no longer resolves, so the overrides of '{1}' cannot be written back into it: {2}
```

**引发者** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:747`
<!-- generated:end DSH9103 -->

**原因。** Adopt（或某个调整操作）必须重写一个 `.dsi`，而该文件的 `Parent` 不再能够解析，因此无法依据参数列表检查该实例的覆盖。

**修复。** 修复 `.dsi` 中的 `Parent` 键，然后再次 adopt。

<a id="dsh9104"></a>

## DSH9104

<!-- generated:begin DSH9104 -->
**严重程度** 警告

**消息**

```
'{0}' overrides '{1}', which '#pragma instance' has no key for; the rebuilt instance has the parent's.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:320`
<!-- generated:end DSH9104 -->

**原因。** 该实例覆盖了一个基础属性 `#pragma instance`，但没有对应的键。重建后的
实例会采用父级的值。

**修复。** 在重建后的资产上手动设置它，或在父级上设置。

<a id="dsh9105"></a>

## DSH9105

<!-- generated:begin DSH9105 -->
**严重程度** 警告

**消息**

```
'{0}' is a curve atlas row, and '{1}' picks its curve; a '.dsi' can state the row's number and nothing else, so the rebuilt instance loses the curve.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:281`
<!-- generated:end DSH9105 -->

**原因。** 标量覆盖是一条曲线图集行：引擎会把曲线和图集与数值一起存储。而 `.dsi` 只能声明数值。

**修复。** 如果曲线很重要，就让该实例保持手工制作。

<a id="dsh9106"></a>

## DSH9106

<!-- generated:begin DSH9106 -->
**严重程度** 警告

**消息**

```
'{0}' is declared 'bool' by the parent's source and '{1}' sets it to {2}; the override is written as a 'float'.
```

**引发者** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:270`
<!-- generated:end DSH9106 -->

**原因。** 父级的源文件声明了参数 `bool`（一个标量 0/1 参数），而实例持有的值既不是 0 也不是 1。它被写为 `float` 覆盖，以便该值得以保留。

**修复。** 无需修复；如果本意是 0 或 1，请在资产中修正该值。

<a id="dsh9107"></a>

## DSH9107

<!-- generated:begin DSH9107 -->
**严重程度** 错误

**消息**

```
Expected '{0}' to be declared alone to rewrite its value, found it in a declaration shared with other names; split the declaration first.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:656`、`Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:1227`
<!-- generated:end DSH9107 -->

**原因。** 某个编辑器操作必须在 `.dsi` 中重写一个覆盖的值，而该名称与其他名称共享其声明（`uniform float a = 1, b = 2;`）。重写器一次只编辑一个声明。

**修复。** 将声明拆分为每个名称一行。

<a id="dsh9108"></a>

## DSH9108

<!-- generated:begin DSH9108 -->
**严重程度** 错误

**消息**

```
Expected every change to this file to touch its own stretch of text, found an edit at line {0} that overlaps another or runs past the end; the file was left unchanged.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:803`、`Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:1150`
<!-- generated:end DSH9108 -->

**原因。** 为一个 `.dsi` 计算出的两处文本编辑相互重叠，或者其中一处超出文件末尾。文件保持原样。这是实例源重写器的内部错误。

**修复。** 连同文件和所执行的操作一起报告。

<a id="dsh9109"></a>

## DSH9109

<!-- generated:begin DSH9109 -->
**严重程度** 错误

**消息**

```
Expected a '#pragma instance(...)' line in this .dsi file, found none; the file was left unchanged.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:1109`、`Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:1122`、`Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:954`、`Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:2355`
<!-- generated:end DSH9109 -->

**原因。** 某个就地重写源文件的编辑器操作没有找到可供其编辑锚定的内容：一个没有 `#pragma instance(...)` 行的 `.dsi`，或者——当实例的值作为其父级 uniform 的默认值写回时——文件本身并未声明该参数名的 `uniform`（包含在头文件中的声明无法被拼接），或者该参数被声明为另一种类型的参数。该操作还会拒绝 `.dss` uniform 的 `bool`、`int` 或 `uint` 类型无法表示的标量值：隐式更改着色器输入的类型可能会改变其调用方。

**修复。** 添加该 pragma（`#pragma instance(Parent = "...")`），或者在该操作所针对的文件中声明该 uniform，然后重复该操作。对于无法表示的标量，请显式更改源类型，或将该调整提取到 `.dsi` 实例中。

<a id="dsh9110"></a>

## DSH9110

<!-- generated:begin DSH9110 -->
**严重程度** 错误

**消息**

```
'-{0}={1}' is neither on nor off. A flag takes true, 1, yes or on, or false, 0, no or off, and '-{0}' alone is on; the command did nothing.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderCommandletRunner.cpp:228`
<!-- generated:end DSH9110 -->

**原因。** commandlet 动词的某个布尔标志——`-Force`、`-All`、`-Check`、`-DryRun`、`-NoBackup`、`-Shaders`、`-Json`、`-IncludeEphemeral`、`-KeepAssetPath`、`-Readable`、`-Gc` 或 `-Rebuild`——被写入了一个既非开启（`true`、`1`、`yes`、`on`）也非关闭（`false`、`0`、`no`、`off`）的值（不区分大小写）。空值两者都不算：`-Force=` 通常是未被设置过的脚本变量。不会猜测其本意。动词在触及任何文件之前会读取其所有标志，遇到此错误时不执行任何操作并以 `1` 退出。在 2.1.0 及更早版本中，写入任何值的标志都被视为不存在，因此 `migrate -Check=true` 被迁移，`fmt -Check=true` 被重写。

**修复。** 单独写入该标志以将其开启（`-Force`），或使用八个词之一写入；将其省略，或写入 `=false`，以使其保持关闭。仅检查动词所读取的标志：它不接受的标志会被忽略，无论其值为何。

**另见** [Commandlet——布尔标志](../../tools/commandlet.md#boolean-flags)

<a id="dsh9111"></a>

## DSH9111

<!-- generated:begin DSH9111 -->
**严重程度** 警告

**消息**

```
'{0}' is in a package's asset folder and no source builds it any more: its package was removed or stopped generating it. Delete it if nothing uses it.
```

**引发者** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1216`
<!-- generated:end DSH9111 -->

**原因。** `list-generated -All` 在某个包资产根目录（`/Game/DreamShaderPackages`、`/<Plugin>/DreamShaderPackages`）下发现了一个不再有任何源文件构建的资产：其包已被移除，或者该包不再生成它（函数被重命名、注解被移除）。包只会构建到这些文件夹中，因此该资产是残留物。不会删除任何内容：材质可能仍在使用它。

**修复。** 检查有哪些内容引用了该资产（引用查看器），替换这些使用处，然后删除它。重新安装该包，或恢复该注解，会让源文件再次构建它，警告便会消失。

<a id="dsh9200"></a>

## DSH9200

<!-- generated:begin DSH9200 -->
**严重程度** 错误

**消息**

```
The Custom Pass slot registry cannot be read: {0}. 'dsc pass-registry -Rebuild' moves it aside and gives every HLSL pass a slot again.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:164`
<!-- generated:end DSH9200 -->

**原因。** `pass-registry` 无法读取 `Registry.json` 以列出它：该文件无法打开，它不是有效的 JSON——其中残留的合并冲突是常见原因——或者某个条目没有槽位编号、管线或通道。不会列出任何内容，运行失败（退出码 `1`）。任何 `.dsp` 的编译都会拒绝同一个文件（DSH8315）。

**修复。** 运行 `./dsc.ps1 pass-registry -Rebuild`：它会将该文件移至一旁，保存为 `Registry.json.unreadable`，并编译每一个 `.dsp`，从而再次为每个 HLSL 通道分配槽位。选择冲突的任意一方并再次编译 `.dsp` 文件同样可行；切勿手动合并该 JSON。参见
[`pass-registry`](../../tools/commandlet.md#pass-registry)。

<a id="dsh9201"></a>

## DSH9201

<!-- generated:begin DSH9201 -->
**严重程度** 警告

**消息**

```
No .dsp builds '{0}' any more, but the asset is still there and its pass '{1}' points at {2} slot {3}. Delete the asset or restore its source: once the slot is collected and given to another pass, that pass's shader is what this one would run.
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:98`
<!-- generated:end DSH9201 -->

**原因。** 源根目录下已没有任何 `.dsp` 构建此槽位的管线——`.dsp` 已被删除或重命名——而该管线资产仍然存在，无论是已加载还是在磁盘上，其通道仍指向该槽位。`-Gc` 会释放这样的槽位，而一旦新的 HLSL 通道占用它，旧管线便会用它自己的绑定在该管线仍然运行的地方（体积、组件、项目的全局管线）派发该通道的着色器。由列表、`-Gc` 和 `-Rebuild` 引发。

**修复。** 删除旧的管线资产以及任何激活它的内容，或者恢复其 `.dsp` 并编译它，这会再次为该通道分配一个它自己的槽位。重命名后的 `.dsp` 会在其新名称下构建一条新管线；旧名称下的资产便是孤儿。

<a id="dsh9202"></a>

## DSH9202

<!-- generated:begin DSH9202 -->
**严重程度** 信息

**消息**

```
{0} slot {1} was freed: {2}
```

**引发者** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:213`
<!-- generated:end DSH9202 -->

**原因。** 信息性。`pass-registry -Gc`，或 `-Rebuild` 的收集步骤释放了一个槽位：不再有任何
`.dsp` 构建其管线，或其管线的 `.dsp` 编译后不再在该表中以
HLSL 运行该通道。其在 `Registry.json` 中的条目、在注册表文件中的区段及其快照均被
删除。命令程序不会重新编译任何着色器；下一次编辑器启动时编译已更改的注册表。

**修复。** 无需处理。提交已更改的 `.dreampass/` 文件夹。

<a id="dsh9203"></a>

## DSH9203

<!-- generated:begin DSH9203 -->
**严重程度** 警告

**消息**

```
Registry.json did not parse and was moved aside to '{0}'; the compiles that follow give every HLSL pass a slot afresh.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:343`
<!-- generated:end DSH9203 -->

**原因。** `pass-registry -Rebuild` 发现一个无法解析的 `Registry.json` —— 其中残留的合并冲突
是常见原因 —— 将其移到 `Registry.json.unreadable` 并从一个空
注册表开始。随后的编译会按路径顺序为每个 HLSL 通道重新分配槽位，因此槽位编号
可能变化，每个带 HLSL 通道的管线都会以新槽位重新构建并保存。

**修复。** 无需修复。提交重新构建的 `.dreampass/` 文件夹，如果你
提交生成资产，也提交重新构建的管线。没有任何东西读取 `Registry.json.unreadable`：查看它以了解冲突
是什么，然后删除它，而不是提交它。

<a id="dsh9204"></a>

## DSH9204

<!-- generated:begin DSH9204 -->
**严重程度** 错误

**消息**

```
'{0}' did not compile, so its pipeline keeps the slots it had: {1}
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:395`
<!-- generated:end DSH9204 -->

**原因。** `pass-registry -Rebuild` 强制编译源根目录下的每个 `.dsp`，而此
失败；消息以编译报告结尾。其管线保留注册表中为其保留的槽位
—— 如果此次重建重置了不可读的注册表（DSH9203），则为无槽位 —— 其资产
不会更新。本次运行失败（退出代码 `1`），但仍完成其余步骤。

**修复。** 根据报告修复 `.dsp` 并编译它（`./dsc.ps1 compile <file>.dsp`）：这会在重建的注册表上
规划其槽位并更新其资产。DSH9205 会列出其每个指向
不再拥有的槽位的通道，而 DSH8337 会列出其每个没有快照的槽位。

<a id="dsh9205"></a>

## DSH9205

<!-- generated:begin DSH9205 -->
**严重程度** 警告

**消息**

```
Pass '{1}' of '{0}' points at {2} slot {3}, which the registry {4}. Its source '{5}' did not compile, so the asset was not updated: fix the source and compile it, or that pass runs whatever the slot holds.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:314`
<!-- generated:end DSH9205 -->

**原因。** 在 `pass-registry -Rebuild` 之后，重建中 `.dsp` 失败的管线（DSH9204）有一个
通道指向注册表现在分配给另一通道的槽位，或该槽位被留空 —— 通常是因为
重建重置了不可读的注册表并重新分配了每个槽位。资产未更新，因此
在其源编译之前，该通道会分派槽位中当前的任何内容：另一通道的着色器搭配本
通道的绑定，或空存根。

**修复。** 修复消息中指出的 `.dsp` 并编译它；编译会为该通道分配自己的槽位
并写入资产。在此之前，`r.DreamPass.DisablePipelines` 会阻止该管线运行。

<a id="dsh9206"></a>

## DSH9206

<!-- generated:begin DSH9206 -->
**严重程度** 信息

**消息**

```
No .dsp under the source roots; the registry is rewritten from Registry.json as it stands.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:363`
<!-- generated:end DSH9206 -->

**原因。** 信息性。`pass-registry -Rebuild` 在源根目录下未发现 `.dsp`（已排除 `Packages`
文件夹），因此没有要编译的内容。重建的其余部分仍会运行：不再由任何 `.dsp` 构建的
管线的槽位会被释放 —— 记录的 `.dsp` 仍在磁盘上的槽位会保留 —— 并且
注册表文件会根据 `Registry.json` 重写。

**修复。** 当项目没有管线时无需处理。如果有，则源根目录未到达其
`.dsp` 文件：检查 *Source Directory* 项目设置和插件的 `DShader` 文件夹。

<a id="dsh9208"></a>

## DSH9208

<!-- generated:begin DSH9208 -->
**严重程度** 警告

**消息**

```
The snapshot of {0} slot {1} (pass '{2}' of '{3}') is not on disk, and the registry file includes it: the next start takes the slot's section out of the registry file, and the pass does nothing until its source is compiled again -- unless the registry file is read-only, and then the global shaders fail to compile, which is fatal. Compile '{4}', or run 'dsc pass-registry -Rebuild'; commit the Slots folder with the registry.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:114`
<!-- generated:end DSH9208 -->

**原因。** `Registry.json` 为此槽位记录了快照，而其文件不在
`.dreampass/Slots/` 下 —— 一个未提交或未同步的 `Slots` 文件夹 —— 且注册表文件
仍包含它们。在其下次启动时，DreamShaderPass 模块会将此类区段从
注册表文件中移除（日志中会出现一个错误），该通道在 `.dsp` 再次编译之前不会做任何事。只有
只读的注册表文件 —— 未检出 —— 无法以这种方式重写：它会继续包含
缺失的文件，随后全局着色器编译失败，这是致命的。由列表和 `-Gc` 触发；
`-Rebuild` 会自行保留此类槽位（DSH8337）。

**修复。** 编译消息中指出的 `.dsp`：这会写回快照及
相应区段。`./dsc.ps1 pass-registry -Rebuild` 一次处理所有槽位。将 `Slots` 文件夹与注册表一起
提交。如果注册表文件为只读，请在编辑器下次启动前检出它们。

<a id="dsh9209"></a>

## DSH9209

<!-- generated:begin DSH9209 -->
**严重程度** 警告

**消息**

```
'{0}' does not compile far enough to tell whether it still runs pass '{1}' in {2} slot {3}, so the slot is kept. Fix the source and compile it.
```

**由以下触发** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:124`
<!-- generated:end DSH9209 -->

**原因。** 拥有此槽位的 `.dsp` 编译不够深入，无法判断它是否仍在该表中以
HLSL 运行该通道：其前端失败、其管线的资产路径无法解析，或文件
在磁盘上但未能进入产品索引（预处理错误会停止索引的解析）。
槽位仅在证据确凿时才会被释放，因此此槽位会保留。由列表、`-Gc` 和 `-Rebuild` 触发。

**修复。** 修复 `.dsp` —— `./dsc.ps1 check <file>.dsp` 会说明问题所在 —— 并编译它；列表
随后就能判断该槽位。如果该 `.dsp` 本应移除，删除它，`-Gc` 会释放其槽位。

<a id="dsh9210"></a>

## DSH9210

<!-- generated:begin DSH9210 -->
**严重程度** 错误

**消息**

```
'{0}' is a pass pipeline, which decompiles to a '.dsp', and '{1}' is not one.
```

**由以下触发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:247`
<!-- generated:end DSH9210 -->

**原因。** 管线反编译为 `.dsp` 文本，而请求的输出文件 —— `-Out`，或
请求的输出路径 —— 具有其他扩展名。（`-Format=Legacy`，或以 `.dsm` /
`.dsf` 结尾的 `-Out`，会将管线交给 1.x 反编译器，而它拒绝该管线：管线没有 1.x 文本。）

**修复。** 让 `-Out` 以 `.dsp` 结尾，或省略它：默认为
`<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp`。

<a id="dsh9211"></a>

## DSH9211

<!-- generated:begin DSH9211 -->
**严重程度** 警告

**消息**

```
The pass '{0}' has neither a material nor a shader, so the text names neither, and it does not build until one is given.
```

**由以下触发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:435`
<!-- generated:end DSH9211 -->

**原因。** 管线的某个全屏通道既没有材质也没有着色器 —— 它指定的材质
不再加载，或二者都在管线的详细信息面板中被清除。`.dsp` 全屏通道会指定
二者之一，因此文本二者都未指定，在提供其一之前不会构建。`dsc decompile` 会将
该警告写在文件开头；Adopt 会拒绝该管线且不写入任何内容。

**修复。** 在详细信息面板中为该通道指定其材质，或指定其着色器和入口，然后再次反编译或
采用 —— 或者在反编译后，将 `Material`（或 `Shader` 和 `Entry`）写入文本。

<a id="dsh9212"></a>

## DSH9212

<!-- generated:begin DSH9212 -->
**严重程度** 警告

**消息**

```
The pass '{0}' has both a material, '{1}', and a shader, '{2}'; a fullscreen pass names one of the two, and the text keeps the material, which is what the pass draws.
```

**由以下触发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:413`
<!-- generated:end DSH9212 -->

**原因。** 管线的全屏通道同时拥有一个材质和一个着色器,而只有在其详情面板中编辑才会造成这种情况:一次编译会设置两者之一。该通道绘制该材质,因此文本保留材质并舍弃着色器。Adopt 会拒绝该管线,而不是从 `.dsp` 中丢弃着色器。

**修复。** 在详情面板中清除该通道不应拥有的那一个,然后再次反编译或采用。

<a id="dsh9214"></a>

## DSH9214

<!-- generated:begin DSH9214 -->
**严重性** 信息

**消息**

```
A layer filter of the pass '{0}' kept no spelling of its layers, so they are written as the project's layer table names its bits today: {1}.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:534`
<!-- generated:end DSH9214 -->

**原因。** 信息性提示。网格通道的 `Layer(...)` 过滤词项持有层位,却不持有写入时所用的任何名称——这是手工编辑,或是在名称与层位并存之前编译的资产所致。文本按项目当前的层表为这些层位命名。

**修复。** 检查这些名称是否就是所要的层:如果层表自这些层位设置以来已被重新排序,那么同样的层位现在指向其它层。

<a id="dsh9215"></a>

## DSH9215

<!-- generated:begin DSH9215 -->
**严重性** 警告

**消息**

```
A layer filter of the pass '{0}' selects layer bit(s) {1}, which the project's layer table has no name for; the text cannot say them and leaves them out.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:541`
<!-- generated:end DSH9215 -->

**原因。** 网格通道的 `Layer(...)` 过滤词项选择的层位在项目的层表中没有任何名称(表更短,或条目为空),且没有存储的名称说明它们曾是什么。`.dsp` 为层命名,因此文本省略了这些层位。Adopt 会拒绝该管线,而不是丢弃它们。

**修复。** 在 Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass ▸ Layer Names(位 *n* 是第 *n* 个名称,从 0 开始计数)中为那些位置命名,或在详情面板中从过滤器中移除这些层位;然后再次反编译或采用。

<a id="dsh9216"></a>

## DSH9216

<!-- generated:begin DSH9216 -->
**严重性** 警告

**消息**

```
'{0}' runs in no kind of view, which a '.dsp' cannot say; the text leaves 'Views' out, and a rebuild runs in {1}.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:834`
<!-- generated:end DSH9216 -->

**原因。** 该管线不在任何类型的视图中运行:其 `Views` 为空,这只有在其详情面板中编辑才会造成。`.dsp` 对此没有拼写——省略 `Views` 意味着 `Game | Editor`——因此文本将其省略,而从该文本重建的管线会在 `Game | Editor` 中运行。Adopt 会拒绝该管线。

**修复。** 用其它方式关闭该管线——将 `Enabled` 绑定到 `bool` uniform(常量 `Enabled = false` 是 [`DSH7322`](DSH7xxx.md#dsh7322)),将其从激活它的内容中移除,或为会话使用 `r.DreamPass.DisablePipelines`——然后恢复它应运行的视图;之后再次反编译或采用。

<a id="dsh9217"></a>

## DSH9217

<!-- generated:begin DSH9217 -->
**严重性** 警告

**消息**

```
The pass '{0}' checks its override material for no usage flag at all, which a '.dsp' cannot say; the text leaves 'Usage' out, and a rebuild checks the default set ({1}).
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:584`
<!-- generated:end DSH9217 -->

**原因。** 网格通道检查其覆盖材质时发现没有任何用法标志:其 `Usage` 为空,这只有在其详情面板中编辑才会造成。`.dsp` 对此没有拼写——省略 `Usage` 意味着默认集合——因此文本将其省略,而重建会检查 `StaticMesh`、`InstancedStaticMeshes` 和 `SkeletalMesh`。Adopt 会拒绝该管线。

**修复。** 在详情面板中设置覆盖材质所需的标志(若没有别的,即为默认集合),然后再次反编译或采用。

<a id="dsh9218"></a>

## DSH9218

<!-- generated:begin DSH9218 -->
**严重性** 警告

**消息**

```
The parameter '{0}' defaults to '{1}', which is not a 2D texture. A '.dsp' declares every texture parameter 'Texture2D', and the text does so here too, keeping this default; it builds as it is.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:230`
<!-- generated:end DSH9218 -->

**原因。** 管线的某个纹理参数默认使用的纹理不是 2D——立方体贴图、体积纹理或数组纹理。`.dsp` 将所有纹理参数声明为 `Texture2D`,因此文本声明的类型与其默认值不符,并保留该默认值。文本按原样构建:构建可接受任意纹理作为默认值,因此从它重建的管线会保留这一个。

**修复。** 若该参数本应是 2D,就给其一个 2D 默认值,在详情面板或文本的 `/// @default` 中设置。

<a id="dsh9219"></a>

## DSH9219

<!-- generated:begin DSH9219 -->
**严重性** 警告

**消息**

```
The pass '{0}' binds '{1}' to a texture constant, which a 'param' cannot state; the binding is left out of the text.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:333`
<!-- generated:end DSH9219 -->

**原因。** 某个通道将 `param` 绑定到纹理常量,这只有在其详情面板中编辑才会造成。`param` 接受 uniform、`static const`、字面量或 `DreamPassWeight`,且没有对纹理常量的拼写,因此文本省略了该绑定。Adopt 会拒绝该管线。

**修复。** 声明一个以该纹理为 `/// @default` 的 `Texture2D` uniform,并将该参数绑定到它,在详情面板或文本中均可;然后再次反编译或采用。

<a id="dsh9220"></a>

## DSH9220

<!-- generated:begin DSH9220 -->
**严重性** 警告

**消息**

```
The pass '{0}' tests against its own depth but names no Depth32 buffer for it; the text writes 'Own()' empty, and it does not build until one is named.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:573`
<!-- generated:end DSH9220 -->

**原因。** 某个网格通道对其自身深度(`Depth = Own`)进行测试,却没有为其指定 `Depth32` 缓冲区,这只有在其详情面板中编辑才会遗漏。文本将 `Own()` 写为空,在指定缓冲区之前无法构建。Adopt 会拒绝该管线。

**修复。** 在详情面板中为其指定一个 `Depth32` 缓冲区——若该管线没有,则声明一个——或选择 `TestScene` 或 `None`;然后再次反编译或采用。

<a id="dsh9221"></a>

## DSH9221

<!-- generated:begin DSH9221 -->
**严重性** 警告

**消息**

```
The decompiled pipeline does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:951`
<!-- generated:end DSH9221 -->

**原因。** 反编译器输出的 `.dsp` 文本无法解析回去;消息给出了第一个解析错误。文件仍会被写出,因此不会丢失任何内容。这是反编译器的缺陷,而非资产的缺陷。

**修复。** 连同资产一起报告;手工修复所指定的行通常能得到可用的文件。

<a id="dsh9222"></a>

## DSH9222

<!-- generated:begin DSH9222 -->
**严重性** 警告

**消息**

```
The decompiled pipeline parses but does not bind back into a pipeline: {0}: {1}. It is written as it is; either the asset breaks a rule a '.dsp' is checked against (an edit by hand can), or this is a defect of the decompiler.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:967`
<!-- generated:end DSH9222 -->

**原因。** 输出的文本可以解析,但将其绑定——不借助引擎:引用按原样获取,需要项目的检查被跳过——失败或得不到管线;消息引用了第一个错误。要么是该资产违反了每个 `.dsp` 都要检查的某条规则,而这可由在其详情面板中编辑造成(两个通道或缓冲区同名,某个通道同时读写同一缓冲区,在 `Clear = None` 缓冲区的首次写入之前读取,某种类型处于其无法运行的注入点),要么是反编译器出了问题。文件按原样写出。

**修复。** 查阅所引用的代码:它会给出规则名称及其在文本中的行号。在详情面板或文本中修复该资产;若该管线从未被手工编辑过,则连同资产一起报告。

<a id="dsh9223"></a>

## DSH9223

<!-- generated:begin DSH9223 -->
**严重性** 警告

**消息**

```
The decompiled pipeline reads back as a different pipeline ({0} difference(s); the first: {1}). It is written as it is; this is a defect of the decompiler or the printer.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:981`
<!-- generated:end DSH9223 -->

**原因。** 输出的文本可以解析并绑定,而它所绑定到的管线与资产不同——消息统计差异数量并给出第一个——即使只有引擎才知道的事实(引用解析为什么、`.usf` 的 `[numthreads]`)取自该资产。这是反编译器或输出器的缺陷;文件按原样写出。

**修复。** 连同资产一起报告。在从文本构建之前,修正文本中所指的值。

<a id="dsh9224"></a>

## DSH9224

<!-- generated:begin DSH9224 -->
**严重性** 警告

**消息**

```
A '.dsp' names its pipeline after its file and has no '/// @name', so '{0}' builds '{1}' where it is written, not '{2}'; move the file to where the pipeline's source belongs to keep its path.
```

**引发自** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:910`
<!-- generated:end DSH9224 -->

**原因。** `-KeepAssetPath` 要求保持管线在原位，而 `.dsp` 无法说明其管线应去哪里：它按文件名命名管线，且没有 `/// @name`，因此其源根目录下该文件所在的文件夹决定了包路径。在文本所写入的位置，它会在消息所指定的路径构建管线，而不是资产自身的路径。

**修复。** 将该文件（连同 `-Out`）写入或移动至能够给出资产路径的文件夹和名称 —— 通常是管线自身 `.dsp` 所在的位置。在原位编译时，它会构建出第二条管线。

<a id="dsh9225"></a>

## DSH9225

<!-- generated:begin DSH9225 -->
**严重性** 错误

**消息**

```
There is no pass pipeline to decompile.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:821`、`Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:921`
<!-- generated:end DSH9225 -->

**原因。** 内部问题。管线反编译器被传入时完全没有管线，或者它读取的有效载荷无法为打印器布局为 `.dsp`。二者都不是资产自身的属性；不会写入任何内容。

**修复。** 随资产及所运行的命令一同报告。

<a id="dsh9226"></a>

## DSH9226

<!-- generated:begin DSH9226 -->
**严重性** 错误

**消息**

```
DSH9226: '{0}' is built from the pipeline file '{1}' but is not a pass pipeline, so nothing was adopted.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:489`
<!-- generated:end DSH9226 -->

**原因。** Adopt 被要求处理的资产的源戳记（`DreamShader.SourceFile`）指向 `.dsp`，而该资产并非 `UDreamPassPipeline`。`.dsp` 所生成的另一个资产是导出缓冲区的渲染目标，它带有相同的戳记，且不包含 `.dsp` 所声明的任何内容；任何其他带有 `.dsp` 戳记的内容，其元数据已被复制或编辑。没有采用任何内容。

**修复。** 采用管线本身 —— 通过其详细信息面板、差异通知或材质内容浏览器。渲染目标遵循其缓冲区的声明：在 `.dsp` 中更改缓冲区。

<a id="dsh9227"></a>

## DSH9227

<!-- generated:begin DSH9227 -->
**严重性** 错误

**消息**

```
DSH9227: '{0}' is not a .dsp pipeline file, so the settings of '{1}' cannot be spliced into it.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:880`
<!-- generated:end DSH9227 -->

**原因。** `.dsp` Adopt 被传入的文件并非 `.dsp`。Adopt 根据资产源戳记的扩展名选择管线路径，因此编辑器自身的条目总是传给它 `.dsp`；此处防范的是使用另一文件进行直接调用的情况。没有写入任何内容。

**修复。** 从编辑器中采用 —— 管线的详细信息面板、差异通知、材质内容浏览器 —— 它们会使用管线所戳记的 `.dsp`。

<a id="dsh9228"></a>

## DSH9228

<!-- generated:begin DSH9228 -->
**严重性** 错误

**消息**

```
DSH9228: '{0}' builds no pass pipeline any more, so '{1}' has nothing to be spliced into.
```

**由以下项引发** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:935`
<!-- generated:end DSH9228 -->

**原因。** 管线所戳记的 `.dsp` 检查通过 —— 它能够解析、绑定并降级 —— 且它所构建的内容都不是传递管线，因此没有可将资产的值拼接进去的位置。检查通过的 `.dsp` 始终只构建一条管线，以文件命名，因此此处防范的是文件通常不会触及的情况。没有写入任何内容。

**修复。** 运行 `./dsc.ps1 dump-ir <file>.dsp` 以查看该文件构建的内容，并随该文件一同报告。
