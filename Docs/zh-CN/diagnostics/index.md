---
title: "诊断"
description: "诊断"
---

<a id="diagnostics"></a>

# 诊断

> [DreamShader](../index.md) » **诊断**

DreamShader 的诊断出现在哪里、如何读懂一条诊断，以及 1.x 生成器的消息后来怎么样了。一次编译报出的每条诊断都有一个稳定的 `DSHnnnn` 码*（自 2.0.0 起）*；码是契约，措辞不是。目录在 [README.md](README.md)——每个码及其严重级别和消息——每个码在其页面上都有一节，`DSH1xxx.md` … `DSH9xxx.md`，列有它名下报出的每条消息、报出它的位置、原因和修复方法。

| | |
| :-- | :-- |
| 产生者 | `DreamShaderLang`——预处理器、两个前端、绑定器、IR 及其校验器 · `DreamShaderCompiler`——流水线、include 解析器、发射器、资产层 · `DreamShaderEditor`——bridge、commandlet 命令和工具 |
| 日志分类 | `LogDreamShader` |
| 线上格式 | `<file>(<line>,<column>): DSHnnnn: <message>`，无论编辑器是什么语言都是英文 |
| 严重级别 | `error`、`warning` 或 `info`——一次编译的记录保留各自的级别*（自 2.1.0 起）*；编译不解释的少数记录是 `error` |
| Bridge 产物 | `<Project>/Saved/DreamShader/Bridge/diagnostics.json`、`.../Bridge/diagnostics/`、`.../Bridge/bridge.db` |

> [!NOTE]
> **到 1.9.x 为止**大多数消息没有码：1.x parser 给自己的消息编了码，1.x 生成器约 560 个
> 报错点只报裸文本，本页当时也逐字收录那些文本。生成器和 parser 已经删掉，那些字符串
> 一条都不会再报出。如果你在旧日志或搜索结果里见到一条，[1.x 消息及其码](#1x-messages-and-their-codes) 列出了常见的那些。

<a id="where-diagnostics-appear"></a>

## 诊断出现在哪里

| 界面 | 显示什么 | 实现方 |
| :-- | :-- | :-- |
| **Output Log** | 分类 `LogDreamShader` 下的编译报告：失败时记 `Error`——每条记录一行线上格式，先错误、再警告、再 note——成功时记 `Display`——`Generated …` / `Skipped …` 行，再跟一个 `Warnings:` 块 | 插件 |
| **[Material Content Browser](../../tools/material-browser.md)** | 每个源文件的记录，从 bridge 的内存诊断存储读出；源文件那一行显示第一个错误 | 插件 |
| **`Bridge/diagnostics.json`** | `{ version, updatedAtUtc, files[] }`，每个文件一项 | 插件 |
| **`Bridge/diagnostics/`** | 每个文件一个 `<md5-of-normalized-path>.json` 分片，外加 `index.json`；每次写入都会删掉过期分片 | 插件 |
| **`Bridge/bridge.db`** | SQLite 表 `diagnostics(path, json, updated_at_utc)`，在一个事务里整体替换 | 插件 |
| **`-DiagnosticsOut=<file>`** | 一次 commandlet 运行的诊断，JSON（schema `dreamshader-diagnostics`）——见 [Commandlet](../tools/commandlet.md) | 插件 |
| **VSCode / Rider 扩展波浪线** | 由上面三份 bridge 产物渲染 | **扩展侧**，不是插件 |

三个 bridge sink 每次诊断更新都一起写，所以它们永远不会互相矛盾。Output Log 是另一条独立路径，也是唯一会显示*成功*行的界面。

诊断归属于产生它的源文件和产生它的那次**运行**。运行有三种：一个源文件的编译、它生成的某个材质的 shader 编译，以及对一个文件的启动时 VirtualFunction 扫描。

| 运行 | 它替换什么 |
| :-- | :-- |
| `A.dsm` 的编译 | 恰好是 A 上次编译登记的那些——登记在 A 上的，和登记在 A 导入的任何头文件上的。`B.dsm` 登记在同一个头文件上的记录保留。它也会撤掉启动时 VirtualFunction 扫描对 A 及其头文件说过的话 |
| 一个材质的 shader 编译 | 该材质的 shader 编译记录，在其 shader 编译结束时——比生成它的那次编译晚几帧；绝不会动编译自己的警告 |
| 启动时的 VirtualFunction 扫描 | 它对所扫文件的记录 |
| 删除一个文件 | 这个文件拥有的一切记录，来自每种运行，以及登记在它上面的每条记录，无论谁登记 |

同一个文件里，记录按「编译、shader 编译、VirtualFunction 扫描」排序，各自内部按源文件排，所以一个文件的第一个错误是它编译时的错误。见 [Bridge](../../tools/bridge.md)。

> [!NOTE]
> bridge 从不在 commandlet 里运行。`-run=DreamShader`、`-run=Cook` 以及任何其他 commandlet
> 进程都不写 `diagnostics.json`、不写分片、不写 `bridge.db` 行；消息只存在于
> 日志中（以及一个 `-DiagnosticsOut` 文件中）。bridge 也会被 `-NoDreamShaderEditorBridge` 禁用。
> 见 [Commandlet](../tools/commandlet.md)。

<a id="severity"></a>

## 严重级别

一次编译按每条记录被报出时的级别登记它自己的记录——失败的编译登记全部，成功的编译登记它的警告和 note*（自 2.1.0 起；在此之前，每条入库的诊断都是 `error`，成功编译的警告只到达 Output Log）*。编译不解释的记录——`generate-error`、`material-compile`、`virtual-function-sync`——是 `error`。该字段及其取值见 [Bridge](../../tools/bridge.md)。

不属于任何编译记录、只写日志的警告（`UE_LOG(LogDreamShader, Warning, …)`）从不进入存储。客户端遇到缺失或未知的严重级别时，按 error 处理。

<a id="message-locations"></a>

## 消息位置

诊断按 MSVC 风格打印，带着它的码：

```text
I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.
```

| 诊断 | 它的位置从哪来 |
| :-- | :-- |
| 一次编译的记录 | 构造本身的行和列，取自报错时所在的 token——在 section 体里、在 `Graph` 里、在被 import 的头文件里都一样。在头文件里报的记录指向**头文件**，不是导入它的文件 |
| 关于整个文件的——解析不了的 include、编译器不构建的源文件 | 文件的第 1 行第 1 列 |
| 材质编译 | 引擎自己的 `<path>(<line>,<col>): ` 前缀，被重新解析并归回它来自的源码行 |
| `generate-error`——记录解释不了的失败 | 从一行 `<path>(<line>,<column>): <message>` 里取；取不到就是 `1,1` |

*（自 2.0.0 起）* 每个文件单独做词法和语法分析，所以位置从不从拼起来的文本映射回去。到 1.9.x 为止，解析错误带的是一个指向「import 内联之后的文本」的 `near index {Index}` 偏移，section 体里的位置是错的，大多数语句级消息根本没有位置。

一个阶段会报出它的**全部**错误——parser 和绑定器遇到一个错误后会继续往下走——而有错误的阶段会在下一阶段开始前停下流水线：有语法错误的文件不会进入绑定，绑定不过的文件不会被发射。

<a id="reading-a-code"></a>

## 读懂一个码

首位数字是报出它的阶段；完整分配见 [DreamShaderLang 2.0](../language-v2/index.md#diagnostics)。

| 范围 | 阶段 |
| :-- | :-- |
| `DSH1xxx` | 预处理器（`#if`、`#define`），1.x 纹理默认值中的资产引用 |
| `DSH2xxx` | 词法分析器和解析器 —— `DSH2200`–`DSH2258` 是旧版前端的：1.x `Graph` 语句和顶层块 |
| `DSH3xxx` | 声明、`#pragma`、`///` 块；`DSH3250`–`DSH3278` 是旧版前端的 section |
| `DSH4xxx` | 绑定器（名称、类型、语句）、IR 校验器、降低拒绝 |
| `DSH5xxx` | `UE.*` / `Substrate.*` 调用和节点目录；`DSH5250`–`DSH5292` 是 1.x 调用写法和编了号的 **旧版规则** |
| `DSH6xxx` | 函数种类、内联、`/// @custom` HLSL；`DSH6300`–`DSH6330` 是 1.x 的 `Function` / `GraphFunction` / `Namespace` / `VirtualFunction` |
| `DSH7xxx` | uniform、指令、`#pragma material`、材质设置写入器、`.dsi`、`.dsp` 检查 |
| `DSH8xxx` | 发射器、资产层、include 解析器（`DSH8291`–`DSH8295`）、Custom Pass 发射 |
| `DSH9xxx` | 工具 —— `check`、`dump-ir`、反编译器、`migrate`、导航、`pass-registry` |

**旧版规则**（`DSH5275`–`DSH5292`）是 1.x 做过、而 1.x 源文件今天仍然会得到的行为，只是现在会说出来：只靠大小写才对得上的名字（`DSH5275`、`DSH5276`）、GLSL 拼写（`DSH5277`）、宽向量被截到窄位置（`DSH5289`）、一条赋值顺带声明了变量（`DSH5292`）。它是 `.dsm` / `.dsf` 上的 warning 或 info，从不出现在 `.dss` 里，而 [`dsc migrate`](../tools/migrate.md) 会把每一条都写成显式的 2.0 文本。

<a id="1x-messages-and-their-codes"></a>

## 1.x 消息及其代码

1.x 解析器和生成器的常见消息，以及今天由什么报告同样的状况。`{…}` 代表消息里填入的内容。

| 1.x 消息 | 今天 |
| :-- | :-- |
| `Unknown Graph identifier '{Name}'.` | [`DSH4200`](DSH4xxx.md#dsh4200) —— 当已声明的名字只差大小写时附带 *did you mean* |
| `Unknown Graph function '{Name}'.` | [`DSH4208`](DSH4xxx.md#dsh4208) |
| `Math function '{Name}' expects exactly {N} argument(s).` | [`DSH4224`](DSH4xxx.md#dsh4224)；命名参数现在选择 pin，未知的名字是 [`DSH4216`](DSH4xxx.md#dsh4216) —— 见 [数学内置](../builtins/math.md#named-arguments) |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` · `Operator '{Op}' requires matching vector sizes …` · `Arithmetic operators cannot be applied to …` | [`DSH4226`](DSH4xxx.md#dsh4226) |
| `Integer division is not supported by the material graph …` | [`DSH4243`](DSH4xxx.md#dsh4243) |
| `Expected {N} component(s) but got {M}.` | [`DSH4228`](DSH4xxx.md#dsh4228)；宽值写进窄位置在 1.x 源文件里是 info [`DSH5289`](DSH5xxx.md#dsh5289) |
| `Swizzle '{Swizzle}' is invalid …` · `Unsupported swizzle '{Swizzle}'.` | [`DSH4230`](DSH4xxx.md#dsh4230) |
| `Constructor '{Name}' expects {N} total components but got {M}.` | [`DSH4222`](DSH4xxx.md#dsh4222)；`float3()` 后面什么都不写是 [`DSH4221`](DSH4xxx.md#dsh4221) |
| `Graph variable '{Name}' is declared more than once.` | 在一个块里是 [`DSH4220`](DSH4xxx.md#dsh4220)；在文件作用域是 [`DSH4210`](DSH4xxx.md#dsh4210) |
| `Graph variable type '{Type}' requires an explicit initializer.` | [`DSH2215`](DSH2xxx.md#dsh2215) |
| `Unsupported Graph variable type '{Type}'.` | 未知类型是 [`DSH4201`](DSH4xxx.md#dsh4201)；`return`、`for`、`while`、`switch` 是 [`DSH2208`](DSH2xxx.md#dsh2208) |
| `Graph if statement is missing a '{ ... }' body.` | [`DSH2209`](DSH2xxx.md#dsh2209) |
| `Graph if statement could not resolve both branch values for '{Name}'.` | [`DSH4372`](DSH4xxx.md#dsh4372) |
| `Graph if statement cannot select texture value '{Name}'.` · `… Substrate value …` | [`DSH4379`](DSH4xxx.md#dsh4379) · [`DSH4378`](DSH4xxx.md#dsh4378) |
| `Unsupported material output '{Name}'.` | [`DSH5200`](DSH5xxx.md#dsh5200) |
| `Unsupported property type '{Type}'.` | [`DSH3252`](DSH3xxx.md#dsh3252)；七个没有 2.0 拼写的节点 token 之一是 [`DSH3253`](DSH3xxx.md#dsh3253) |
| `Metadata 'Slider(min, max)' requires exactly two numeric bounds …` · `Metadata key '{Key}' is declared more than once.` · `Metadata SortPriority value '{Value}' is not an integer.` | [`DSH3257`](DSH3xxx.md#dsh3257) · [`DSH3256`](DSH3xxx.md#dsh3256) · [`DSH3258`](DSH3xxx.md#dsh3258) |
| `Unsupported UE builtin call '{Name}' in Graph. …` | [`DSH5210`](DSH5xxx.md#dsh5210) —— 目录认得的节点不需要 `OutputType` |
| `UE.{Name} could not resolve MaterialExpression class '{Class}'.` | [`DSH5212`](DSH5xxx.md#dsh5212) |
| `UE.Expression requires Class="MaterialExpressionName".` | [`DSH5218`](DSH5xxx.md#dsh5218) |
| `UE.{Name}: '{Argument}' is not a property on '{Class}'.` | [`DSH5213`](DSH5xxx.md#dsh5213) |
| `Generic {Namespace}.{Name} calls require named arguments.` | [`DSH5220`](DSH5xxx.md#dsh5220) / [`DSH5221`](DSH5xxx.md#dsh5221) |
| `UE.SceneTexture expects exactly Id="..." …` · `SampleTexture2D expects exactly two positional arguments …` | [`DSH5255`](DSH5xxx.md#dsh5255) · [`DSH5256`](DSH5xxx.md#dsh5256) |
| `UE.StaticSwitchParameter requires Name="ParameterName".` · `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | [`DSH5257`](DSH5xxx.md#dsh5257) · [`DSH5258`](DSH5xxx.md#dsh5258) |
| `{Kind} '{Name}' is missing required input '{Input}'.` · `… does not have an input named '{Input}'.` | [`DSH4217`](DSH4xxx.md#dsh4217) · [`DSH4216`](DSH4xxx.md#dsh4216) |
| `GraphFunction cycle detected: {Chain}.` · `SelfContained Function cycle detected: …` | 互相调用的 `Function` / `GraphFunction` 体是 [`DSH6260`](DSH6xxx.md#dsh6260)；环经由从体中提升出来的调用运行时是 [`DSH6330`](DSH6xxx.md#dsh6330)；被内联的 `.dss` helper 是 [`DSH6220`](DSH6xxx.md#dsh6220) |
| `Function '{Name}' has a return type and cannot also declare out parameters. …` · `Function '{Name}' must declare at least one out parameter.` | [`DSH6304`](DSH6xxx.md#dsh6304) · [`DSH6305`](DSH6xxx.md#dsh6305) |
| `Function '{Name}' parameter '{Parameter}' uses unsupported qualifier '{Qualifier}'. …` · `… '__return' is reserved …` | [`DSH6302`](DSH6xxx.md#dsh6302) · [`DSH6303`](DSH6xxx.md#dsh6303) |
| `Namespace(Name="...") is required.` · `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | [`DSH6309`](DSH6xxx.md#dsh6309) · [`DSH6310`](DSH6xxx.md#dsh6310) |
| `VirtualFunction(Name="...") is required.` · `… must provide Options = { Asset = Path(...); }.` · `… must declare at least one output.` | [`DSH6311`](DSH6xxx.md#dsh6311) · [`DSH6312`](DSH6xxx.md#dsh6312) · [`DSH6313`](DSH6xxx.md#dsh6313) |
| `Shader(Name="...") is required.` · `{Block}(Name="...") is required.` | [`DSH2242`](DSH2xxx.md#dsh2242) |
| `Shader must provide a Graph block.` | [`DSH2255`](DSH2xxx.md#dsh2255) |
| `{File}: Outputs block is required.` · `No Outputs block was provided. …` | warning [`DSH2256`](DSH2xxx.md#dsh2256)；材质照样构建，只是没有任何东西接到它上面 |
| `Unknown shader section '{Section}'.` · `Unknown material function section '{Section}'.` · `Unknown VirtualFunction section '{Section}'.` | [`DSH2245`](DSH2xxx.md#dsh2245) |
| `Shader graph sections now use Graph = { ... }. …` | [`DSH2246`](DSH2xxx.md#dsh2246) |
| `MaterialLayer is deprecated; use ShaderLayer instead.` · `MaterialLayerBlend is deprecated; …` | warning [`DSH2251`](DSH2xxx.md#dsh2251) |
| `Only one top-level Shader block is currently supported.` | [`DSH2250`](DSH2xxx.md#dsh2250) —— 现在按 **文件** 判定；它过去跨越整个 import 闭包 |
| `DreamShader header '{File}' may only declare …` | [`DSH2249`](DSH2xxx.md#dsh2249)，按声明判定：注释里提到 `Shader(` 没问题 |
| `DreamShader function file '{File}' may only declare …` · `{File}: .dsf files cannot define top-level Shader blocks.` | [`DSH2259`](DSH2xxx.md#dsh2259) —— 见 [源文件](../language/source-files.md#how-the-restriction-is-enforced) |
| `A top-level Shader, Function, … block was not found.` | [`DSH2254`](DSH2xxx.md#dsh2254) |
| `Unexpected token near index {Index}.` | [`DSH2240`](DSH2xxx.md#dsh2240)；`.dsm` / `.dsf` 里的 2.0 语法是 [`DSH2248`](DSH2xxx.md#dsh2248) |
| `Expected '{' near index {Index}.` · `Expected ',' or ')' near index {Index}.` · `Expected identifier near index {Index}.` · `Expected value near index {Index}.` | [`DSH2257`](DSH2xxx.md#dsh2257) · [`DSH2243`](DSH2xxx.md#dsh2243) |
| `Unterminated block.` · `Unterminated string literal.` | [`DSH2150`](DSH2xxx.md#dsh2150) · [`DSH2103`](DSH2xxx.md#dsh2103) |
| `DreamShader import '{Specifier}' referenced from '{File}' could not be resolved.` | [`DSH8292`](DSH8xxx.md#dsh8292)；import 一个 `.dsf` / `.dsm` 或根限定写法是 [`DSH2252`](DSH2xxx.md#dsh2252) |
| `DreamShader could not read '{File}'.` · `DreamShader import cycle detected at '{File}'.` | [`DSH8293`](DSH8xxx.md#dsh8293) · [`DSH4211`](DSH4xxx.md#dsh4211) |
| `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` | [`DSH7202`](DSH7xxx.md#dsh7202)；空值是 [`DSH7201`](DSH7xxx.md#dsh7201)，`Instance` 是 warning [`DSH7204`](DSH7xxx.md#dsh7204) |
| `Unsupported BlendMode/RenderType '{Value}'.` · `Unsupported ShadingModel '{Value}'.` · `Unsupported MaterialDomain '{Value}'.` · `Unsupported material setting '{Key}'.` | [`DSH7127`](DSH7xxx.md#dsh7127) · [`DSH7129`](DSH7xxx.md#dsh7129) · [`DSH7130`](DSH7xxx.md#dsh7130) · [`DSH7118`](DSH7xxx.md#dsh7118) —— 材质设置写入器保留了它的 1.x 消息 |
| `Asset '{ObjectPath}' already exists and is not a Material.` · `… was not generated by DreamShader. …` | [`DSH8201`](DSH8xxx.md#dsh8201)，原因附带 [`DSH8102`](DSH8xxx.md#dsh8102) / [`DSH8103`](DSH8xxx.md#dsh8103) |
| `Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. …` | [`DSH8206`](DSH8xxx.md#dsh8206)，附带 [`DSH8101`](DSH8xxx.md#dsh8101) |
| `Asset '{ObjectPath}' was edited by hand since DreamShader generated it …` | [`DSH8207`](DSH8xxx.md#dsh8207)，附带 [`DSH8115`](DSH8xxx.md#dsh8115) —— 见 [Divergence](../generation/divergence.md) |

不在表里的消息：到 [README.md](README.md) 里搜它所描述情况的一个词，
或者把文件编译一遍、读它现在报的码。

<a id="what-1x-accepted-in-silence"></a>

## 1.x 静默接受的写法

1.x 生成器一声不吭地接受、然后构建出与文本不符的东西（或者把文本说的东西丢掉）的情况。（自 2.0.0 起）每一种都有诊断。

| 1.x 文本 | 1.x 的做法 | 今天 |
| :-- | :-- | :-- |
| `a % b`、`a & b`、`a \| b`、`a ^ b`、`a << b` 在 `Graph` 表达式里 | 读到运算符就停：表达式就是 `a` | [`DSH2201`](DSH2xxx.md#dsh2201) |
| `a ? b : c`，或 `if` 条件之外的 `a < b` | 一样：表达式就是 `a` | [`DSH2200`](DSH2xxx.md#dsh2200) |
| `v[0]` | 丢掉下标和它后面的一切 | [`DSH2202`](DSH2xxx.md#dsh2202) |
| `if (a > 0 && b > 0)` | 只测 `a > 0` | [`DSH2210`](DSH2xxx.md#dsh2210) |
| `(float3)x` | 没有强制转换 | [`DSH2203`](DSH2xxx.md#dsh2203) |
| `a += b` | 读成一个以运算符命名的变量的声明 | [`DSH2205`](DSH2xxx.md#dsh2205) |
| `x++` | 没有这个运算符 | [`DSH2207`](DSH2xxx.md#dsh2207) |
| 作为语句的 `a + b;` | 建出没人读的节点 | [`DSH2211`](DSH2xxx.md#dsh2211) |
| `Graph` 表达式里的 `0x10` | 读成了另一个数 | [`DSH2222`](DSH2xxx.md#dsh2222) |
| 当作数字的 `1abc` 或 `0.5.5` | 取前面的数字 | [`DSH2105`](DSH2xxx.md#dsh2105) |
| 没闭合的 `/* …` | 吃掉文件剩下的部分 | [`DSH2102`](DSH2xxx.md#dsh2102) |
| 属性写了两遍：`Shader(Name="A", Name="B")` | 保留后一个值 | warning [`DSH2244`](DSH2xxx.md#dsh2244)，仍保留后一个值 |
| `Settings` 键写了两遍 | 保留后一个值 | warning [`DSH3262`](DSH3xxx.md#dsh3262)，仍保留后一个值 |
| 材质函数 `Settings` 里的未知键 | 忽略 | warning [`DSH3263`](DSH3xxx.md#dsh3263)，仍然忽略 |
| 不收位置参数的 `UE.*` 调用上的位置参数 | 忽略 | warning [`DSH5254`](DSH5xxx.md#dsh5254)，仍然丢掉 |
| 函数 `Outputs` 条目上的默认值 | 忽略 | warning [`DSH3273`](DSH3xxx.md#dsh3273)，仍然丢掉 |
| 第二个 `Graph` 或 `Layout` section | 保留后一个 | warning [`DSH2258`](DSH2xxx.md#dsh2258)，仍保留后一个 |
| 与内置同名、从 `Graph` 调用的 `ShaderFunction` | 调到了内置 | 在声明处报 [`DSH6206`](DSH6xxx.md#dsh6206) |
| `dot(vec3Value, vec2Value)` | 接受；后来 Unreal 自己的材质翻译失败 | [`DSH4226`](DSH4xxx.md#dsh4226) |

1.x 写进文档、1.x 源文件仍在依赖的写法 —— 换了大小写的名字、GLSL 拼写、写进 `float3` 位置的 `float4` —— 照样构建，
作为一条把它说出来的 [legacy 规则](#reading-a-code)。

<a id="virtualfunction-sync"></a>

## VirtualFunction 同步

启动服务会重新读取每一个 `VirtualFunction` 声明，并从它的
`UMaterialFunction` 资产刷新它，以及 Material Function 工具栏和上下文菜单上的编辑器操作。
这些消息没有 `DSHnnnn` 码，`DSH9001` 除外 —— 用于使用 `#if` 的源文件。

同步诊断通过 `stage = virtualFunctionSync`、`code = virtual-function-sync`
和 `source = DreamShader VirtualFunction` 进入存储。编辑器操作消息是 toast 通知和
日志行；它们不会被存储。

| 消息 | 原因 | 修复 | 参见 |
| :-- | :-- | :-- | :-- |
| `Created VirtualFunction file but could not open it: {File}` | `.dsh` 已写入，但 VSCode 无法启动 | 手动打开该文件 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader could not find a VirtualFunction definition for {Asset}.` | 对没有声明的资产执行 *Open VirtualFunction* | 改用 *CreateVirtualFunction* | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader could not find the selected Material Function.` | 所选资产在菜单构建与执行操作之间消失了 | 重新选择该资产 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader could not open VirtualFunction file: {File}` | 编辑器启动失败 | 手动打开该文件 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader could not read VirtualFunction source file '{File}'.` | 源文件不可读；报在第 1 行第 1 列 | 检查文件锁和权限 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction call: {Detail}` | 无法生成调用片段 | 看内层消息 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction reference: {Detail}` | 无法生成引用片段 | 看内层消息 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction: {Detail}` | 无法生成声明文本 | 看内层消息 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to create directory: {Dir}` | 无法创建 `DShader/VirtualFunctions` | 检查权限 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to update VirtualFunction source file '{File}'.` | 刷新后的声明写不回去；报在第 1 行第 1 列 | 检查文件锁和版本控制 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `DreamShader failed to write VirtualFunction file: {File}` | 无法写入新的 `.dsh` | 检查权限 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `Expected exactly one VirtualFunction block.` | 重新解析的声明区域得到零个或多个块 | 每个声明区域只保留一个 `VirtualFunction` | [VirtualFunction](../language/virtual-function.md) |
| `MaterialFunction '{Name}' does not have a valid package path.` | 资产最外层的包名为空，或不以 `/` 开头 | 把资产重新保存到已挂载内容根目录下的某处 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `MaterialFunction '{Name}' does not expose any outputs.` | 资产没有 `FunctionOutput` 节点 | 给材质函数加一个输出 | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction '{Name}' asset reference is invalid: {Detail}` | `Options.Asset` 解析不了；原始字面量作为诊断的 `assetPath` 报出 | 改正 `Path( … )` | [Path](../parameters/path.md) |
| `VirtualFunction '{Name}' could not be refreshed from MaterialFunction '{Path}': {Detail}` | 声明构建器失败 | 看内层消息 | [VirtualFunction 工具](../../tools/virtual-function-tools.md) |
| `VirtualFunction '{Name}' references missing MaterialFunction '{Path}'.` | `LoadObject` 返回 null | 恢复资产，或者更新 `Options.Asset` | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction attributes are missing a closing ')'.` | 头部中 `(` 不配平 | 配平圆括号 | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction body is missing a closing '}'.` | 主体中 `{` 不配平 | 配平花括号 | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction declaration is invalid: {Detail}` | 重新解析失败；解析错误携带在 `detail` 中 | 改正声明 | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction name cannot be empty.` | 声明构建器收到了空名字 | 提供一个名字 | [VirtualFunction](../language/virtual-function.md) |

> [!NOTE]
> 同步服务按裸关键字 `VirtualFunction` 找声明，**区分大小写**，
> 两边要有标识符边界 —— 和 legacy 前端对块关键字的要求一样。
> 小写的 `virtualfunction` 两边都看不见。

<a id="other-log-lines"></a>

## 其他日志行

不属于任何编译、永远不入库的行。

| 消息 | 含义 |
| :-- | :-- |
| `Failed to open DreamShader bridge database for diagnostics: {Path}` | `bridge.db` 打不开；JSON sink 仍然会写 |
| `'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` | 编译落到了一个背后有文件的 ThinCustom 产物上；由存储决定，所以它被重建并存盘 |
| `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.` | 记在 `Display` 级别；1.x 的 `Classic` 布局跳过了大图 |

commandlet 自己的运行行、用法横幅和退出码在
[命令行](../tools/commandlet.md)；反编译器的在[反编译导出](../tools/decompiler.md)。

<a id="example"></a>

## 示例

``Graph`` 块里的一个拼写错误，到达各个界面时的样子。

```c
// DShader/Materials/M_Sample.dsm
Shader(Name="Materials/M_Sample")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Outputs    = { vec3 Color; Base.EmissiveColor = Color; }
    Graph      = {
        vec2 UV = UE.TexCoord(Index = 0);
        Color   = Tin * UV.x;          // typo: Tin, not Tint
    }
}
```

Output Log：

```text
LogDreamShader: Error: I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.
```

`Saved/DreamShader/Bridge/diagnostics.json`：

```json
{
  "version": 1,
  "updatedAtUtc": "2026-10-04T11:04:22Z",
  "files": [
    {
      "path": "I:/Project/DShader/Materials/M_Sample.dsm",
      "diagnostics": [
        {
          "message": "'Tin' is not declared in this scope.",
          "detail": "I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.",
          "stage": "bind",
          "code": "DSH4200",
          "line": 8,
          "column": 19,
          "severity": "error",
          "source": "DreamShader Lang2"
        }
      ]
    }
  ]
}
```

同一条记录还会写进 `Bridge/diagnostics/<md5>.json` 和
`Bridge/bridge.db` 的 `diagnostics` 表，并出现在 Material Content Browser 里这个源文件的那一行。`stage` 由
码的范围决定：`preprocess`、`parse`、`bind`、`ir`、`generate` 或 `tools`。

<a id="see-also"></a>

## 另请参阅

- [README.md](README.md) — 每个码，及其严重级别和消息
- [桥接](../../tools/bridge.md) — 请求文件、WebSocket 协议和诊断产物
- [Material Content Browser](../../tools/material-browser.md) — 显示每个源文件记录的那个标签页
- [命令行](../tools/commandlet.md) — `-run=DreamShader`、`-DiagnosticsOut`、它的开关和退出码
- [DreamShaderLang 2.0](../language-v2/index.md#diagnostics) — 完整的码范围
- [`dsc migrate`](../tools/migrate.md) — 以 2.0 文本写出的 legacy 规则
- [源文件](../language/source-files.md) — 多个码所强制执行的文​件类型规则
- [import](../language/import.md) — 头部如何被读取，以及 import 的码
- [测试](../contributing/testing.md) — 固定了许多这些码的语料库夹具
